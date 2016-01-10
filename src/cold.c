#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"

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

enum VarType var_type_fromstring(char* input)
{
    if (strcmp(input, "int") == 0)
        return TYPE_INT;
    else if (strcmp(input, "float") == 0)
        return TYPE_FLOAT;
    else if (strcmp(input, "string") == 0)
        return TYPE_STRING; 

    printf("String [%s] cannot be parsed into a VarType\n", input);
}

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

char* instruction_type_tostring(enum InstructionType input)
{
    switch (input)
    {
    case INST_LET:
        return "let";
    case INST_ADD:
        return "add";
    case INST_MUL:
        return "mul";
    case INST_JUMP:
        return "jmp";
    case INST_CMP:
        return "cmp";
    case INST_RET:
        return "ret";
    case INST_PRINT:
        return "prt";
    case INST_NEXT:
        return "nxt";
    default:
        printf("InstructionType %d not supported\n", input);
        exit(0);
    }
}

enum InstructionType instruction_type_fromstring(char* input)
{
    if (strcmp(input, "let") == 0)
        return INST_LET;
    else if (strcmp(input, "add") == 0)
        return INST_ADD;
    else if (strcmp(input, "mul") == 0)
        return INST_MUL;
    else if (strcmp(input, "jmp") == 0)
        return INST_JUMP;
    else if (strcmp(input, "cmp") == 0)
        return INST_CMP;
    else if (strcmp(input, "ret") == 0)
        return INST_RET;
    else if (strcmp(input, "prt") == 0)
        return INST_PRINT;
    else if (strcmp(input, "nxt") == 0)
        return INST_NEXT;
    else
    {
        printf("Instruction type [%s] unrecognized\n", input);
        exit(0);
    }
}

void value_set_int(struct Value* value, int val)
{
    value->type = TYPE_INT;
    value->size = sizeof(int);
    value->data = malloc(value->size);
    *((int*)value->data) = val;
}

void value_set_float(struct Value* value, float val)
{
    value->type = TYPE_FLOAT;
    value->size = sizeof(float);
    value->data = malloc(value->size);
    *((float*)value->data) = val;
}

void value_set_string(struct Value* value, char* val)
{
    value->type = TYPE_STRING;
    value->size = strlen(val) + 1;
    value->data = malloc(value->size);
    strncpy(value->data, val, value->size);
}

void value_tostring(struct Value* val, char* buf, int n)
{
    switch (val->type)
    {
    case TYPE_INT:
        snprintf(buf, n, "%d", *((int*)val->data));
        break;
    case TYPE_FLOAT:
        snprintf(buf, n, "%f", *((float*)val->data));
        break;
    case TYPE_STRING:
        snprintf(buf, n, "%s", val->data);
        break;
    default:
        printf("Unsupported ValueType %d\n", val->type);
        exit(0);
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

void param_tostring(struct Param* p, char* buf, int n)
{
    char prefix;

    switch (p->type)
    {
    case PARAM_LITERAL:
        switch (p->value->type)
        {
        case TYPE_INT:
            prefix = 'i';
            break;
        case TYPE_FLOAT:
            prefix = 'f';
            break;
        default:
            printf("Unrecognized value type %d\n", p->value->type);
            exit(1);
        }
        break;
    case PARAM_LABEL:
        prefix = '$';
        break;
    case PARAM_PATTERN:
        prefix = '!';
        break;
    default:
        printf("Unrecognized param type %d\n", p->type);
        exit(1);
    }

    char val[100];
    value_tostring(p->value, val, 100);

    snprintf(buf, n, "%c%s", prefix, val);
}

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

void free_function(struct Function* func)
{
    free(func->args);
    free(func->insts);
}
