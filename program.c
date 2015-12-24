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
};

enum InstructionType
{
    INST_LET
,   INST_ADD
,   INST_SUBTRACT
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
    struct Value value;
};

struct Param
{
    enum ParamType type;
    struct Value value;
};

struct Instruction
{
    enum InstructionType type;
    struct Param* params;
    int param_count;
};

struct Function
{
    struct Instruction** instructions;
    int instruction_count;
};

struct Local** locals = NULL;
int local_count = 0;

struct Local* get_local(const char* name)
{
    for (int i = 0; i < local_count; i++)
        if (strcmp(locals[i]->name, name) == 0)
            return locals[i];

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

struct Value value_clone(const struct Value* src)
{
    struct Value ret;

    ret.type = src->type;
    ret.size = src->size;
    ret.data = malloc(ret.size);
    memcpy(ret.data, src->data, ret.size);

    return ret;
}

void value_free(const struct Value* value)
{
    free(value->data);
}

void local_free(const struct Local* local)
{
    free(local->name);
    value_free(&local->value);
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
    inst->params = malloc(param_count * sizeof(struct Param));
    inst->param_count = param_count;
}

void instruction_free(struct Instruction* inst)
{
    for (int i = 0; i < inst->param_count; i++)
        value_free(&inst->params[i].value);

    free(inst->params);
}

void function_free(struct Function* func)
{
    for (int i = 0; i < func->instruction_count; i++)
        instruction_free(func->instructions[i]);
}

struct Value* resolve(struct Param* param)
{
    switch (param->type)
    {
    case PARAM_LABEL:
        return &get_local(param->value.data)->value;
    case PARAM_LITERAL:
        return &param->value;
    default:
        printf("Unrecognized param type\n");
        exit(0);
    }
}

void interpret(struct Instruction* inst)
{
    switch (inst->type)
    {
    case INST_LET:
        local_count++;
        locals = realloc(locals, local_count * sizeof(struct Local*));

        struct Local* local = malloc(sizeof(struct Local));

        int len = strlen(inst->params[0].value.data);
        local->name = malloc(len);
        strncpy(local->name, inst->params[0].value.data, len);

        local->value = value_clone(&inst->params[1].value);

        locals[local_count - 1] = local;

        break;
    case INST_ADD: ; // ew, seriously c?
        struct Local* target = get_local(inst->params[0].value.data);

        struct Value* left = resolve(&inst->params[1]);
        struct Value* right = resolve(&inst->params[2]);

        if (left->type != TYPE_INT || right->type != TYPE_INT ||
            target->value.type != TYPE_INT)
        {
            printf("Type system can't support this\n");
            exit(0);
        }

        *((int*)target->value.data) = *((int*)left->data)+*((int*)right->data);

        break;
    default:
        printf("Unrecognized instruction type\n");
        exit(0);
    }
}

struct Function* make_function()
{
    struct Function* ret = malloc(sizeof(struct Function*));

    ret->instructions = malloc(3 * sizeof(struct Instruction*));

    struct Instruction* inst = NULL;

    // int x = 7;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_LET;
    params_allocate(inst, 2);

    inst->params[0].type = PARAM_LABEL;
    value_set_string(&inst->params[0].value, "x");

    inst->params[1].type = PARAM_LITERAL;
    value_set_int(&inst->params[1].value, 7);

    ret->instructions[0] = inst;

    // int y = 3;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_LET;
    params_allocate(inst, 2);

    inst->params[0].type = PARAM_LABEL;
    value_set_string(&inst->params[0].value, "y");

    inst->params[1].type = PARAM_LITERAL;
    value_set_int(&inst->params[1].value, 3);

    ret->instructions[1] = inst;

    // y = x + 5;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_ADD;

    params_allocate(inst, 3);

    inst->params[0].type = PARAM_LABEL;
    value_set_string(&inst->params[0].value, "y");

    inst->params[1].type = PARAM_LABEL;
    value_set_string(&inst->params[1].value, "x");

    inst->params[2].type = PARAM_LITERAL;
    value_set_int(&inst->params[2].value, 5);

    ret->instructions[2] = inst;

    return ret;
}

void dump_locals()
{
    for (int i = 0; i < local_count; i++)
    {
        printf("LOCAL %s = %d\n", locals[i]->name,
               *((int*)locals[i]->value.data));
    }
}

int main(int argc, char* argv[])
{
    struct Function* function = make_function();

    interpret(function->instructions[0]);
    dump_locals();
    printf("\n");

    interpret(function->instructions[1]);
    dump_locals();
    printf("\n");
    
    interpret(function->instructions[2]); 
    dump_locals();
    printf("\n");

    function_free(function);
    
    for (int i = 0; i < local_count; i++)
        local_free(locals[i]);

    free(locals);
}
