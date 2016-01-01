#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { false, true } bool;

enum VarType
{
    TYPE_INT
,   TYPE_FLOAT
,   TYPE_STRING
};

char* var_type_tostring(enum VarType input)
{
    switch (input)
    {
    case TYPE_INT:
        return "int";
    case TYPE_FLOAT:
        return "float";
    case TYPE_STRING:
        return "string";
    default:
        printf("VarType %d not supported\n", input);
        exit(0);
    }
}

enum ParamType
{
    PARAM_LABEL
,   PARAM_LITERAL
,   PARAM_PATTERN
};

char* param_type_tostring(enum ParamType input)
{
    switch (input)
    {
    case PARAM_LABEL:
        return "label";
    case PARAM_LITERAL:
        return "literal";
    case PARAM_PATTERN:
        return "pattern";
    default:
        printf("ParamType %d not supported\n", input);
        exit(0);
    }
}

enum InstructionType
{
    INST_LET
,   INST_ADD
,   INST_JUMP
,   INST_CMP
,   INST_RET
,   INST_PRINT
};

char* instruction_type_tostring(enum InstructionType input)
{
    switch (input)
    {
    case INST_LET:
        return "let";
    case INST_ADD:
        return "add";
    case INST_JUMP:
        return "jmp";
    case INST_CMP:
        return "cmp";
    case INST_RET:
        return "ret";
    case INST_PRINT:
        return "prt";
    default:
        printf("InstructionType %d not supported\n", input);
        exit(0);
    }
}

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

void value_tostring(struct Value* val, char* buf, int n)
{
    switch (val->type)
    {
    case TYPE_INT:
        snprintf(buf, n, "%d", *((int*)val->data));
        break;
    case TYPE_STRING:
        snprintf(buf, n, "%s", val->data);
        break;
    default:
        printf("Unsupported ValueType %d\n", val->type);
        exit(0);
    }
}

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

void param_tostring(struct Param* p, char* buf, int n)
{
    char val[100];
    value_tostring(p->value, val, 100);

    //snprintf(buf, n, "%s %s", param_type_tostring(p->type), val);
    snprintf(buf, n, "%s", val);
}

struct Instruction
{
    enum InstructionType type;
    struct Param** params;
    int param_count;
};

void instruction_tostring(struct Instruction* input, char* buf, int n)
{
    snprintf(buf, n, "%s ", instruction_type_tostring(input->type));

    char param[100];
    for (int i = 0; i < input->param_count; i++)
    {
        if (i > 0 && i < input->param_count)
            strncat(buf, ", ", n - strlen(buf) - 2);

        param_tostring(input->params[i], param, 100);
        strncat(buf, param, n - strlen(buf) - strlen(param));
    }
}

struct State
{
    struct Local** locals;
    int local_count;

    struct Instruction** instructions;
    int instruction_count;

    int inst_ptr;

    struct Value* ret;
};

struct StateSet
{
    struct State** states;
    int state_count;
};

struct Case
{
    struct Value* input_values;
    struct Value expected;
};

struct Context
{
    char** input_names;
    int input_count;

    struct Case* cases;
    int case_count;
};

int find_local(struct State* state, const char* name)
{
    for (int i = 0; i < state->local_count; i++)
        if (strcmp(state->locals[i]->name, name) == 0)
            return i;

    printf("Local %s not found\n", name);
    exit(0);
}

struct Local* get_local(struct State* state, const char* name)
{
    return state->locals[find_local(state, name)];
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
        printf("Unrecognized param type: %d\n", param->type);
        exit(0);
    }
}

bool compare(struct Value* left, struct Value* right)
{
    if (left->type != TYPE_INT || right->type != TYPE_INT)
    {
        printf("Type system can't support this\n");
        exit(0);
    }

    return *((int*)left->data) == *((int*)right->data);
}

void dump_locals(struct State* state)
{
    for (int i = 0; i < state->local_count; i++)
        printf("LOCAL %s = %d\n", state->locals[i]->name,
               *((int*)state->locals[i]->value->data));
}

void interpret(struct State* state)
{
    const int BUF_LEN = 100;
    char buf[BUF_LEN];

    struct Instruction* inst = state->instructions[state->inst_ptr];

    instruction_tostring(inst, buf, BUF_LEN);
    printf("%d %s\n", state->inst_ptr, buf);

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
        int target = find_local(state, inst->params[0]->value->data);

        struct Value* left = resolve(state, inst->params[1]);
        struct Value* right = resolve(state, inst->params[2]);

        if (left->type != TYPE_INT || right->type != TYPE_INT ||
            state->locals[target]->value->type != TYPE_INT)
        {
            printf("Type system can't support this\n");
            exit(0);
        }

        // TODO: shallow copy?
        struct Local* new_local = malloc(sizeof(struct Local));
        new_local->name = state->locals[target]->name;
        new_local->value = malloc(sizeof(struct Value));
        value_set_int(new_local->value,
            *((int*)left->data)+*((int*)right->data));
        state->locals[target] = new_local;

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
    case INST_RET:
        state->ret = resolve(state, inst->params[0]);
        break;
    default:
        printf("Unrecognized instruction type: %d\n", inst->type);
        exit(0);
    }

    dump_locals(state);
    printf("\n");

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
    state->instruction_count = 2;
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

    state->instructions[1] = inst;
}

void free_state(struct State* state)
{
    // TODO: figure this out
    return;

    for (int i = 0; i < state->instruction_count; i++)
        instruction_free(state->instructions[i]);

    free(state->instructions);

    for (int i = 0; i < state->local_count; i++)
        local_free(state->locals[i]);

    free(state->locals);

    free(state);
}

struct State* setup_state(struct Context* ctx, int case_index)
{
    struct State* ret = malloc(1 * sizeof(struct State));

    ret->local_count = ctx->input_count;
    ret->locals = malloc(ret->local_count * sizeof(struct Local*));

    for (int i = 0; i < ctx->input_count; i++)
    {
        ret->locals[i] = malloc(sizeof(struct Local));
        ret->locals[i]->name = ctx->input_names[i];
        ret->locals[i]->value = value_clone(&ctx->cases[case_index].input_values[i]);
    }

    ret->ret = NULL;

    return ret;
}

struct Local* expect(struct State* state, struct Value* expected)
{
    for (int k = 0; k < state->local_count; k++)
    {
        if (!compare(state->locals[k]->value, expected))
            continue;

        printf("*** FOUND ***\n");

        return state->locals[k];
    }

    return NULL;
}

bool check_cases(struct Context* ctx, struct State* base, struct Local* found)
{
    struct State** states = malloc(sizeof(struct State*));

    // Setup return instruction
    struct Instruction* ret_inst = malloc(sizeof(struct Instruction));
    ret_inst->type = INST_RET;
    params_allocate(ret_inst, 1);
    ret_inst->params[0]->type = PARAM_LABEL;
    value_set_string(ret_inst->params[0]->value, found->name);

    bool success = true;

    for (int i = 1; i < ctx->case_count; i++)
    {
        states[0] = setup_state(ctx, i);

        printf("inst_ptr: %d\n", base->inst_ptr);
        states[0]->instruction_count = base->inst_ptr + 1;
        states[0]->instructions = malloc(states[0]->instruction_count *
            sizeof(struct Instruction*));

        for (int j = 0; j < states[0]->instruction_count - 1; j++)
            states[0]->instructions[j] = base->instructions[j];

        states[0]->instructions[states[0]->instruction_count - 1] = ret_inst;

        printf("CASE %d\n", i);
        printf("lines: %d\n", states[0]->instruction_count);
        while (states[0]->inst_ptr < states[0]->instruction_count)
            interpret(states[0]);

        if (!compare(states[0]->ret, &ctx->cases[i].expected))
        {
            printf("FAIL: CASE %d\n", i);
            success = false;
        }

        // Remove instruction pointers so they don't get freed
        states[0]->instructions = NULL;
        states[0]->instruction_count = 0;

        free_state(states[0]);

        if (!success)
            break;
    }

    free(ret_inst);
    free(states);
    printf("*** LEAVING\n");

    return success;
}

void step(struct Context* ctx, struct State** states, int state_count)
{
    for (int i = 0; i < state_count; i++)
    {
        if (states[i]->inst_ptr >= states[i]->instruction_count)
        {
            const int BUF_LEN = 100;
            char buf[BUF_LEN];

            printf("---\n");
            for (int j = 0; j < states[i]->instruction_count; j++)
            {
                instruction_tostring(states[i]->instructions[j], buf, BUF_LEN);
                printf("%d %s\n", j, buf);
            }
            printf("---\n");

            continue;
        }

        int varied_count = 0;
        struct State** varied = vary(states[i], &varied_count);

        for (int j = 0; j < varied_count; j++)
        {
            interpret(varied[j]);

            struct Local* found = expect(varied[j], &ctx->cases[0].expected);

            if (found && check_cases(ctx, varied[j], found))
            {
                printf("Found it\n");
                exit(0);
            }
        }

        step(ctx, varied, varied_count);

        for (int j = 0; j < varied_count; j++)
            free_state(varied[j]);

        free(varied);
    }
}

int main(int argc, char* argv[])
{
    struct Context ctx;
    ctx.input_count = 1;
    ctx.input_names = malloc(ctx.input_count * sizeof(char*));
    ctx.input_names[0] = "z";
    
    ctx.case_count = 2;
    ctx.cases = malloc(ctx.case_count * sizeof(struct Case));

    ctx.cases[0].input_values = malloc(ctx.input_count * sizeof(struct Value));
    
    value_set_int(&ctx.cases[0].input_values[0], 2);
    value_set_int(&ctx.cases[0].expected, 3);
    
    ctx.cases[1].input_values = malloc(ctx.input_count * sizeof(struct Value));

    value_set_int(&ctx.cases[1].input_values[0], 6);
    value_set_int(&ctx.cases[1].expected, 7);

    struct State** root = malloc(1 * sizeof(struct State*));

    root[0] = setup_state(&ctx, 0);

    write_code(root[0]);
    root[0]->inst_ptr = 0;

    step(&ctx, root, 1);

    free_state(root[0]);
    free(root);

    free(ctx.input_names);
    for (int i = 0; i < ctx.case_count; i++)
        free(ctx.cases[i].input_values);
    free(ctx.cases);
}
