#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"

void print_trace()
{
    void* array[10];
    size_t size;
    char** strings;
    size_t i;

    size = backtrace(array, 10);
    strings = backtrace_symbols(array, size);

    for (i = 0; i < size; i++)
    {
        printf("  %s\n", strings[i]);
    }

    free(strings);
}

bool starts_with(const char* haystack, const char* needle)
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

int exponent(int value, int power)
{
    int ret = value;

    for (int i = 1; i < power; i++)
    {
        ret *= value;
    }

    return ret;
}

char* var_type_tostring(enum VarType input)
{
    switch (input)
    {
    case TYPE_INT:
        return "int";
    case TYPE_FLOAT:
        return "float";
    case TYPE_LONG_DOUBLE:
        return "long double";
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
    else if (strcmp(input, "long double") == 0)
        return TYPE_LONG_DOUBLE;
    else if (strcmp(input, "string") == 0)
        return TYPE_STRING;

    printf("String [%s] cannot be parsed into a VarType\n", input);
    exit(EXIT_FAILURE);
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
    case INST_DIV:
        return "div";
    case INST_EXP:
        return "exp";
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
    else if (strcmp(input, "div") == 0)
        return INST_DIV;
    else if (strcmp(input, "exp") == 0)
        return INST_EXP;
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

void value_set_long_double(struct Value* value, long double val)
{
    value->type = TYPE_LONG_DOUBLE;
    value->size = sizeof(long double);
    value->data = malloc(value->size);
    *((long double*)value->data) = val;
}

void value_set_string(struct Value* value, char* val)
{
    value->type = TYPE_STRING;
    value->size = strlen(val) + 1;
    value->data = malloc(value->size);
    strncpy(value->data, val, value->size);
}

// Set a value based on a string representation with preceding type hinting
//  Example: i123    -> int 123
//  Example: f123.45 -> float 123.45
//  Example: f123e2  -> float 12300.0
void value_set_from_string(struct Value* value, char* input)
{
    const char type = input[0];
    const char* val = input + 1;

    if (type == 'i')
    {
        value_set_int(value, atoi(val));
    }
    else if (type == 'f')
    {
        float f;

        if (strchr(val, 'E'))
        {
            if (!sscanf(val, "%f", &f))
            {
                printf("Unable to parse [%s] as scientific notation\n", val);
                exit(0);
            }
        }
        else
        {
            f = atof(val);
        }

        value_set_float(value, f);
    }
    else if (type == 'D')
    {
        long double ld;

        if (strchr(val, 'E'))
        {
            if (!sscanf(val, "%Le", &ld))
            {
                printf("Unable to parse [%s] as scientific notation\n", val);
                exit(0);
            }
        }
        else
        {
            printf("Unable to parse [%s] as long double\n", val);
            exit(0);
        }

        value_set_long_double(value, ld);
    }
    else
    {
        printf("ERROR: unrecognized constant type\n");
        exit(0);
    }
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
    case TYPE_LONG_DOUBLE:
        snprintf(buf, n, "%Le", *((long double*)val->data));
        break;
    case TYPE_STRING:
        snprintf(buf, n, "%s", val->data);
        break;
    default:
        printf("Unsupported ValueType %d\n", val->type);
        exit(0);
    }
}

// Free a Value and its associated data
void value_free(struct Value* value)
{
    free(value->data);
    free(value);
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
        case TYPE_LONG_DOUBLE:
            prefix = 'D';
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
            strncat(buf, " ", n - strlen(buf) - 2);

        param_tostring(input->params[i], param, 100);
        strncat(buf, param, n - strlen(buf) - strlen(param));
    }
}

// Free an instruction and its associated paramaters and their values
void instruction_free(struct Instruction* inst)
{
    // TODO: free the actual inst here?
    for (int i = 0; i < inst->param_count; i++)
    {
        value_free(inst->params[i]->value);
        free(inst->params[i]);
    }

    free(inst->params);
}

void free_function(struct Function* func)
{
    free(func->name);

    for (int i = 0; i < func->arg_count; i++)
    {
        free(func->args[i]);
    }

    free(func->args);

    for (int i = 0; i < func->inst_count; i++)
    {
        instruction_free(&func->insts[i]);
    }

    free(func->insts);
}

void free_assembly(struct Assembly* assembly)
{
    for (int i = 0; i < assembly->instruction_count; i++)
    {
        instruction_free(&assembly->instructions[i]);
    }

    free(assembly->instructions);
}
