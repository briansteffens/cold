#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum VarType
{
    TYPE_INT
,   TYPE_FLOAT
,   TYPE_STRING
};

enum ParamType
{
    PARAM_TYPE
,   PARAM_LABEL
,   PARAM_VARIABLE
,   PARAM_LITERAL
,   PARAM_PATTERN
};

enum InstructionType
{
    INST_LET
,   INST_ADD
,   INST_JUMP
,   INST_CMP
,   INST_RET
,   INST_PRINT
};

enum Patterns
{
    PTRN_INPUTS = 1
,   PTRN_LOCALS = 2
,   PTRN_CONSTANTS = 4
};

struct Value
{
    enum VarType type;
    void* data;
    int size;
};

struct Local
{
    char* name;
    struct Value* value;
};

struct Param
{
    enum ParamType type;
    struct Value* value;
};

struct Instruction
{
    enum InstructionType type;
    struct Param** params;
    int param_count;
};

struct State
{
    struct Local** locals;
    int local_count;

    struct Instruction** instructions;
    int instruction_count;

    int inst_ptr;
};

struct StateSet
{
    struct State** states;
    int state_count;
};

struct Local* get_local(struct State* state, const char* name)
{
    for (int i = 0; i < state->local_count; i++)
        if (strcmp(state->locals[i]->name, name) == 0)
            return state->locals[i];

    printf("Local %s not found\n", name);
    exit(0);
}

void value_set_int(struct Value* value, int val)
{
    value->type = TYPE_INT;
    value->size = sizeof(int);
    value->data = malloc(value->size);
    *((int*)value->data) = val;
}

void value_set_string(struct Value* value, char* val)
{
    value->type = TYPE_STRING;
    value->size = strlen(val);
    value->data = malloc(value->size);
    strncpy(value->data, val, value->size);
}

struct Value* value_clone(const struct Value* src)
{
    struct Value* ret = malloc(sizeof(struct Value));

    ret->type = src->type;
    ret->size = src->size;
    ret->data = malloc(ret->size);
    memcpy(ret->data, src->data, ret->size);

    return ret;
}

void value_free(const struct Value* value)
{
    free(value->data);
}

void local_free(const struct Local* local)
{
    free(local->name);
    value_free(local->value);
}

void value_print(const struct Value* value)
{
    switch (value->type)
    {
    case TYPE_STRING:
        printf("STRING: \"%s\"\n", value->data);
        break;
    case TYPE_INT:
        printf("INT: \"%d\"\n", *((int*)value->data));
        break;
    default:
        printf("UNRECOGNIZED TYPE");
        break;
    }
}

void params_allocate(struct Instruction* inst, int param_count)
{
    inst->params = malloc(param_count * sizeof(struct Param*));
    inst->param_count = param_count;

    for (int i = 0; i < param_count; i++)
    {
        inst->params[i] = malloc(sizeof(struct Param));
        inst->params[i]->value = malloc(sizeof(struct Value));
    }
}

void instruction_free(struct Instruction* inst)
{
    for (int i = 0; i < inst->param_count; i++)
        value_free(inst->params[i]->value);

    free(inst->params);
}

void state_free(struct State* state)
{
    for (int i = 0; i < state->instruction_count; i++)
        instruction_free(state->instructions[i]);
}

struct Value* resolve(struct State* state, struct Param* param)
{
    switch (param->type)
    {
    case PARAM_LABEL:
        return get_local(state, param->value->data)->value;
    case PARAM_LITERAL:
        return param->value;
    default:
        printf("Unrecognized param type\n");
        exit(0);
    }
}

void interpret(struct State* state)
{
    struct Instruction* inst = state->instructions[state->inst_ptr];

    switch (inst->type)
    {
    case INST_LET:
        state->local_count++;
        state->locals = realloc(state->locals,
            state->local_count * sizeof(struct Local*));

        struct Local* local = malloc(sizeof(struct Local));

        int len = strlen(inst->params[0]->value->data);
        local->name = malloc(len);
        strncpy(local->name, inst->params[0]->value->data, len);

        local->value = value_clone(inst->params[1]->value);

        state->locals[state->local_count - 1] = local;

        break;
    case INST_ADD:; // ew, seriously c?
        struct Local* target = get_local(state, inst->params[0]->value->data);

        struct Value* left = resolve(state, inst->params[1]);
        struct Value* right = resolve(state, inst->params[2]);

        if (left->type != TYPE_INT || right->type != TYPE_INT ||
            target->value->type != TYPE_INT)
        {
            printf("Type system can't support this\n");
            exit(0);
        }

        *((int*)target->value->data) = *((int*)left->data)+*((int*)right->data);

        break;
    case INST_JUMP:
        state->inst_ptr = *((int*)inst->params[0]->value->data);
        return;
    case INST_CMP:;
        struct Value* cmp_left = resolve(state, inst->params[0]);
        struct Value* cmp_right = resolve(state, inst->params[1]);

        if (cmp_left->type != TYPE_INT || cmp_right->type != TYPE_INT)
        {
            printf("Type system can't support this\n");
            exit(0);
        }

        if (*((int*)cmp_left->data) == *((int*)cmp_right->data))
        {
            state->inst_ptr = *((int*)inst->params[2]->value->data);
            return;
        }

        break;
    case INST_PRINT:;
        struct Value* print_target = resolve(state, inst->params[0]);

        switch (print_target->type)
        {
        case TYPE_INT:
            printf("%d\n", *((int*)print_target->data));
            break;
        default:
            printf("Can't print this type\n");
            exit(0);
        }


        break;
    default:
        printf("Unrecognized instruction type\n");
        exit(0);
    }

    state->inst_ptr++;
}

struct State* state_fork(struct State* orig)
{
    struct State* ret = malloc(sizeof(struct State));

    ret->local_count = orig->local_count;
    ret->locals = malloc(ret->local_count * sizeof(struct Local*));

    for (int i = 0; i < ret->local_count; i++)
        ret->locals[i] = orig->locals[i];

    ret->instruction_count = orig->instruction_count;
    ret->instructions = malloc(ret->instruction_count *
                               sizeof(struct Instruction*));

    for (int i = 0; i < ret->instruction_count; i++)
        ret->instructions[i] = orig->instructions[i];

    ret->inst_ptr = orig->inst_ptr;

    return ret;
}

int count_patterns(struct State* state, struct Param* param)
{
    int ret = 0;

    int flags = *((int*)param->value->data);

    if (flags & PTRN_LOCALS)
        ret += state->local_count;

    printf("%d\n", ret);

    return ret;
}

struct Instruction* instruction_clone(struct Instruction* orig)
{
    struct Instruction* ret = malloc(sizeof(struct Instruction));

    ret->type = orig->type;
    ret->param_count = orig->param_count;
    ret->params = malloc(ret->param_count * sizeof(struct Param*));

    for (int i = 0; i < ret->param_count; i++)
        ret->params[i] = orig->params[i];

    return ret;
}

struct Param** vary_params(struct State* state, struct Param* pattern,
                           int* output_count)
{
    int flags = *((int*)pattern->value->data);

    int count = 0;
    if (flags & PTRN_LOCALS)
        count += state->local_count;

    struct Param** ret = malloc(count * sizeof(struct Param*));

    int current = 0;
    if (flags & PTRN_LOCALS)
        for (int i = 0; i < state->local_count; i++)
        {
            ret[current] = malloc(sizeof(struct Param));
            ret[current]->value = malloc(sizeof(struct Value));
            ret[current]->type = PARAM_LABEL;
            value_set_string(ret[current]->value, state->locals[i]->name);
            current++;
        }

    *output_count = current;
    return ret;
}

struct Instruction** vary_instructions(
    struct State* input, struct Instruction* inst, int* instruction_count)
{
    struct Instruction** ret = malloc(1 * sizeof(struct Instruction*));
    *instruction_count = 0;

    for (int i = 0; i < inst->param_count; i++)
    {
        if (inst->params[i]->type != PARAM_PATTERN)
            continue;

        int param_count = 0;
        struct Param** varied = vary_params(input, inst->params[i],
                                            &param_count);
        for (int p = 0; p < param_count; p++)
        {
            struct Instruction* new_inst = instruction_clone(inst);
            new_inst->params[i] = varied[p];

            int recur_count = 0;
            struct Instruction** recurs = vary_instructions(input, new_inst,
                                                            &recur_count);

            ret = realloc(ret, (*instruction_count + recur_count) *
                          sizeof(struct Instruction*));

            for (int j = 0; j < recur_count; j++)
            {
                ret[*instruction_count] = recurs[j];
                *instruction_count = *instruction_count + 1;
            }

            free(recurs);
        }
        free(varied);

        break;
    }

    if (*instruction_count == 0)
    {
        ret = realloc(ret, 1 * sizeof(struct Instruction*));
        ret[0] = inst;
        *instruction_count = 1;
    }

    return ret;
}

struct State** vary(struct State* input, int* state_count)
{
    struct Instruction** insts = vary_instructions(
        input, input->instructions[input->inst_ptr], state_count);

    struct State** ret = malloc(*state_count * sizeof(struct State*));

    for (int i = 0; i < *state_count; i++)
    {
        ret[i] = state_fork(input);
        ret[i]->instructions[ret[i]->inst_ptr] = insts[i];
    }

    free(insts);

    return ret;
}

void write_code(struct State* state)
{
    state->instruction_count = 4;
    state->instructions = malloc(state->instruction_count *
                                 sizeof(struct Instruction*));

    struct Instruction* inst = NULL;

    // int x = 7;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_LET;
    params_allocate(inst, 2);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "x");

    inst->params[1]->type = PARAM_LITERAL;
    value_set_int(inst->params[1]->value, 7);

    state->instructions[0] = inst;

    // int y = 3;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_LET;
    params_allocate(inst, 2);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "y");

    inst->params[1]->type = PARAM_LITERAL;
    value_set_int(inst->params[1]->value, 3);

    state->instructions[1] = inst;

    // x = [l] + 1;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_ADD;

    params_allocate(inst, 3);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "x");

    inst->params[1]->type = PARAM_PATTERN;
    value_set_int(inst->params[1]->value, PTRN_LOCALS);

    inst->params[2]->type = PARAM_LITERAL;
    value_set_int(inst->params[2]->value, 1);

    state->instructions[2] = inst;

    // print(x);
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_PRINT;

    params_allocate(inst, 1);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "x");

    state->instructions[3] = inst;
}

void dump_locals(struct State* state)
{
    for (int i = 0; i < state->local_count; i++)
        printf("LOCAL %s = %d\n", state->locals[i]->name,
               *((int*)state->locals[i]->value->data));
}

int main(int argc, char* argv[])
{
    struct State* state = malloc(sizeof(struct State));
    state->locals = NULL;
    state->local_count = 0;

    write_code(state);

    state->inst_ptr = 0;

    while (state->inst_ptr < state->instruction_count)
    {
        printf("LINE %d\n", state->inst_ptr);

        if (state->inst_ptr == 2)
        {
            int varied_count = 0;
            struct State** varied = vary(state, &varied_count);

            state = varied[1];

            free(varied);
        }

        interpret(state);

        dump_locals(state);
        printf("\n");
    }

    for (int i = 0; i < state->instruction_count; i++)
        instruction_free(state->instructions[i]);

    free(state->instructions);
    
    for (int i = 0; i < state->local_count; i++)
        local_free(state->locals[i]);

    free(state->locals);
}
