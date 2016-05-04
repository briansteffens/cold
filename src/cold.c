#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cold.h"

#define MAX_VALUE_LEN 255
#define MAX_INSTRUCTION_LEN 255

char* var_type_tostring(ValueType input)
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

ValueType var_type_fromstring(char* input)
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

char* param_type_tostring(ParamType input)
{
    switch (input)
    {
    case PARAM_LABEL:
        return "label";
    case PARAM_LITERAL:
        return "literal";
    case PARAM_SUBSTITUTION:
        return "substitution";
    default:
        printf("ParamType %d not supported\n", input);
        exit(0);
    }
}

char* instruction_type_tostring(InstructionType input)
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

InstructionType instruction_type_fromstring(char* input)
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

void value_set_int(Value* value, int val)
{
    value->type = TYPE_INT;
    value->size = sizeof(int);
    value->data = malloc(value->size);
    *((int*)value->data) = val;
}

void value_set_float(Value* value, float val)
{
    value->type = TYPE_FLOAT;
    value->size = sizeof(float);
    value->data = malloc(value->size);
    *((float*)value->data) = val;
}

void value_set_long_double(Value* value, long double val)
{
    value->type = TYPE_LONG_DOUBLE;
    value->size = sizeof(long double);
    value->data = malloc(value->size);
    *((long double*)value->data) = val;
}

void value_set_string(Value* value, char* val)
{
    value->type = TYPE_STRING;
    value->size = strlen(val) + 1;
    value->data = malloc(value->size);
    strncpy(value->data, val, value->size);
}

// Set a value based on a string representation with preceding type hinting
//  Example: 123     -> int 123
//  Example: 123.45f -> float 123.45
//  Example: 123e2f  -> float 12300.0
//  Example: 123e2L  -> long double 12300.0
void value_set_from_string(Value* value, char* input)
{
    const size_t input_len = strnlen(input, MAX_VALUE_LEN);
    const char last_char = input[input_len - 1];

    // Determine type
    ValueType type = TYPE_INT;

    if (last_char == 'f')
    {
        type = TYPE_FLOAT;
    }
    else if (last_char == 'L')
    {
        type = TYPE_LONG_DOUBLE;
    }

    // Strip type hint off input
    const int val_len = type == TYPE_INT ? MAX_VALUE_LEN : MAX_VALUE_LEN - 1;

    char val[val_len];
    strncpy(val, input, val_len);

    switch (type)
    {
    case TYPE_INT:
        value_set_int(value, atoi(val));
        break;

    case TYPE_FLOAT:;
        float f;

        if (strchr(val, 'e'))
        {
            if (!sscanf(val, "%f", &f))
            {
                printf("Unable to parse scientific notation [%s]\n", val);
                exit(0);
            }
        }
        else
        {
            f = atof(val);
        }

        value_set_float(value, f);
        break;

    case TYPE_LONG_DOUBLE:;
        long double ld;

        if (strchr(val, 'e'))
        {
            if (!sscanf(val, "%Le", &ld))
            {
                printf("Unable to parse scientific notation [%s]\n", val);
                exit(0);
            }
        }
        else
        {
            printf("Unable to parse [%s] as long double\n", val);
            exit(0);
        }

        value_set_long_double(value, ld);
        break;

    default:
        printf("ERROR: unrecognized constant type\n");
        exit(0);
    }
}

void value_tostring(Value* val, char* buf, int n)
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

// Deep copy a Value (including the data itself)
Value* value_clone(const Value* src)
{
    Value* ret = malloc(sizeof(Value));

    ret->type = src->type;
    ret->size = src->size;
    ret->data = malloc(ret->size);
    memcpy(ret->data, src->data, ret->size);

    return ret;
}

// Compare two values for value-based equality
bool compare(Context* ctx, Value* left, Value* right)
{
    if (left->type == TYPE_INT && right->type == TYPE_INT)
    {
        return *((int*)left->data) == *((int*)right->data);
    }
    else if (left->type == TYPE_FLOAT && right->type == TYPE_FLOAT &&
             ctx->precision.type == TYPE_FLOAT)
    {
        float f_left = *((float*)left->data);
        float f_right = *((float*)right->data);
        float f_precision = *((float*)ctx->precision.data);
        return f_left >= f_right-f_precision && f_left <= f_right+f_precision;
    }
    else if (left->type == TYPE_LONG_DOUBLE &&
             right->type == TYPE_LONG_DOUBLE &&
             ctx->precision.type == TYPE_LONG_DOUBLE)
    {
        long double f_left = *((long double*)left->data);
        long double f_right = *((long double*)right->data);
        long double f_precision = *((long double*)ctx->precision.data);
        return fabsl(f_left - f_right) <= f_precision;
    }
    else
    {
        printf("compare() can't support these types: %d == %d\n",
            left->type, right->type);
        char buf[MAX_VALUE_LEN];
        value_tostring(left, buf, MAX_VALUE_LEN);
        printf("\tleft: %s\n", buf);
        value_tostring(right, buf, MAX_VALUE_LEN);
        printf("\tright: %s\n", buf);
        exit(0);
    }
}

// Free a Value and its associated data
void value_free(Value* value)
{
    free(value->data);
    free(value);
}

void params_allocate(Instruction* inst, int param_count)
{
    inst->params = malloc(param_count * sizeof(Param*));
    inst->param_count = param_count;

    for (int i = 0; i < param_count; i++)
    {
        inst->params[i] = malloc(sizeof(Param));
        inst->params[i]->value = malloc(sizeof(Value));
    }
}

void param_tostring(Param* p, char* buf, int n)
{
    char* prefix = "";
    char* suffix = "";

    switch (p->type)
    {
    case PARAM_LITERAL:
        switch (p->value->type)
        {
        case TYPE_INT:
            break;
        case TYPE_FLOAT:
            suffix = "f";
            break;
        case TYPE_LONG_DOUBLE:
            suffix = "L";
            break;
        default:
            printf("Unrecognized value type %d\n", p->value->type);
            exit(1);
        }
        break;
    case PARAM_LABEL:
        prefix = "$";
        break;
    case PARAM_SUBSTITUTION:
        prefix = "!";
        break;
    default:
        printf("Unrecognized param type %d\n", p->type);
        exit(1);
    }

    char val[MAX_VALUE_LEN];
    value_tostring(p->value, val, MAX_VALUE_LEN);

    snprintf(buf, n, "%s%s%s", prefix, val, suffix);
}

void instruction_tostring(Instruction* input, char* buf, int n)
{
    snprintf(buf, n, "%s ", instruction_type_tostring(input->type));

    char param[MAX_VALUE_LEN];
    for (int i = 0; i < input->param_count; i++)
    {
        if (i > 0 && i < input->param_count)
            strncat(buf, " ", n - strlen(buf) - 2);

        param_tostring(input->params[i], param, MAX_VALUE_LEN);
        strncat(buf, param, n - strlen(buf) - strlen(param));
    }
}

// Free an instruction and its associated paramaters and their values
void instruction_free(Instruction* inst)
{
    // TODO: free the actual inst here?
    for (int i = 0; i < inst->param_count; i++)
    {
        value_free(inst->params[i]->value);
        free(inst->params[i]);
    }

    free(inst->params);
}

// Clone an instruction.
//
// The return value must be freed by the caller.
Instruction* instruction_clone(Instruction* orig)
{
    Instruction* ret = malloc(sizeof(Instruction));

    ret->type = orig->type;
    ret->param_count = orig->param_count;
    ret->params = malloc(ret->param_count * sizeof(Param*));

    for (int i = 0; i < ret->param_count; i++)
    {
        ret->params[i] = malloc(sizeof(Param));

        ret->params[i]->type = orig->params[i]->type;
        ret->params[i]->value = value_clone(orig->params[i]->value);

        char buf[MAX_INSTRUCTION_LEN];
        instruction_tostring(orig, buf, MAX_INSTRUCTION_LEN);
    }

    return ret;
}

void function_free(Function* func)
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

void combination_free(Combination* combination)
{
    for (int i = 0; i < combination->instruction_count; i++)
    {
        instruction_free(combination->instructions[i]);
        free(combination->instructions[i]);
    }

    free(combination->instructions);
}

void pattern_free(Pattern* pattern)
{
    for (int i = 0; i < pattern->inst_count; i++)
    {
        instruction_free(pattern->insts[i]);
        free(pattern->insts[i]);
    }

    free(pattern->insts);
    free(pattern);
}

// Free a local and its associated value
void local_free(Local* local)
{
    value_free(local->value);
    free(local->name);
    free(local);
}

void state_free(State* state)
{
    for (int i = 0; i < state->instruction_count; i++)
    {
        if (state->instructions_owned[i])
        {
            instruction_free(state->instructions[i]);
            free(state->instructions[i]);
        }
    }

    free(state->instructions);
    free(state->instructions_owned);

    for (int i = 0; i < state->local_count; i++)
        if (state->locals_owned[i])
            local_free(state->locals[i]);

    free(state->locals);
    free(state->locals_owned);

    free(state);
}

void print_program(FILE* file, Instruction** instructions,
        int instruction_count, char** args, int arg_count)
{
    const int BUF_LEN = 255;
    char buf[BUF_LEN];

    strncpy(buf, "def main", BUF_LEN);

    for (int i = 0; i < arg_count; i++)
    {
        strncat(buf, " $", BUF_LEN);
        strncat(buf, args[i], BUF_LEN);
    }

    fprintf(file, "%s\n", buf);

    for (int i = 0; i < instruction_count; i++)
    {
        instruction_tostring(instructions[i], buf, BUF_LEN);
        fprintf(file, "    %s\n", buf);
    }
}
