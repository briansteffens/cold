#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "cold.h"

// Find the ordinal of a local by [name] in state->locals
int find_local(State* state, const char* name)
{
    for (int i = 0; i < state->local_count; i++)
        if (strcmp(state->locals[i]->name, name) == 0)
            return i;

    printf("Local %s not found\n", name);
    exit(0);
}

// Find a local struct by [name] in [state]
Local* get_local(State* state, const char* name)
{
    return state->locals[find_local(state, name)];
}

// Resolve an instruction parameter (label/literal) to its actual value
Value* resolve(State* state, Param* param)
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

void set_local(State* state, Instruction* inst, int param_index,
        ValueType type, void* val)
{
    int local_index = find_local(state,
            inst->params[param_index]->value->data);

    Local* new_local = malloc(sizeof(Local));
    new_local->name = strdup(state->locals[local_index]->name);
    new_local->value = malloc(sizeof(Value));

    value_set(new_local->value, type, val);

    if (state->locals_owned[local_index])
    {
        local_free(state->locals[local_index]);
    }

    state->locals[local_index] = new_local;
    state->locals_owned[local_index] = true;
}

typedef struct TrigRatioFunctions
{
    double (*handle_float)(double);
    long double (*handle_long_double)(long double);
} TrigRatioFunctions;

void single_param_float_function(State* state, Instruction* inst,
        TrigRatioFunctions* functions)
{
    Value* input = resolve(state, inst->params[1]);

    if (input->type == TYPE_INT || input->type == TYPE_FLOAT)
    {
        float input_f;

        if (input->type == TYPE_INT)
        {
            input_f = (float)*((int*)input->data);
        }
        else if (input->type == TYPE_FLOAT)
        {
            input_f = *((float*)input->data);
        }
        else
        {
            printf("Unable to deal with type %d\n", input->type);
            exit(EXIT_FAILURE);
        }

        float output_f = (float)(*functions->handle_float)(input_f);
        set_local(state, inst, 0, TYPE_FLOAT, &output_f);
    }
    else if (input->type == TYPE_LONG_DOUBLE)
    {
        long double output_ld =
            (*functions->handle_long_double)(*((long double*)input->data));
        set_local(state, inst, 0, TYPE_LONG_DOUBLE, &output_ld);
    }
    else
    {
        printf("Unable to sin type %d\n", input->type);
        exit(EXIT_FAILURE);
    }
}

void interpret_sin(State* state, Instruction* inst)
{
    TrigRatioFunctions functions;

    functions.handle_float = &sin;
    functions.handle_long_double = &sinl;

    single_param_float_function(state, inst, &functions);
}

void interpret_asin(State* state, Instruction* inst)
{
    TrigRatioFunctions functions;

    functions.handle_float = &asin;
    functions.handle_long_double = &asinl;

    single_param_float_function(state, inst, &functions);
}

void interpret_let(State* state, Instruction* inst)
{
    state->local_count++;
    state->locals = realloc(state->locals,
        state->local_count * sizeof(Local*));
    state->locals_owned = realloc(state->locals_owned,
        state->local_count * sizeof(bool));

    Local* local = malloc(sizeof(Local));

    local->name = strdup(inst->params[0]->value->data);

    Value* new_value = resolve(state, inst->params[1]);
    local->value = value_clone(new_value);

    state->locals[state->local_count - 1] = local;
    state->locals_owned[state->local_count - 1] = true;
}

void interpret_basic_math(State* state, Instruction* inst)
{
    Value* left = resolve(state, inst->params[1]);
    Value* right = resolve(state, inst->params[2]);

    if (left->type == TYPE_INT && right->type == TYPE_INT)
    {
        int left_i = *((int*)left->data);
        int right_i = *((int*)right->data);
        int output_i;

        switch (inst->type)
        {
        case INST_ADD:
            output_i = left_i + right_i;
            break;

        case INST_MUL:
            output_i = left_i * right_i;
            break;

        case INST_DIV:
            output_i = left_i / right_i;
            break;

        default:
            printf("No handling for this instruction type: %d\n", inst->type);
            exit(EXIT_FAILURE);
        }

        set_local(state, inst, 0, TYPE_INT, &output_i);
    }
    else if (left->type == TYPE_FLOAT && right->type == TYPE_FLOAT)
    {
        float left_f = *((float*)left->data);
        float right_f = *((float*)right->data);
        float output_f;

        switch (inst->type)
        {
        case INST_ADD:
            output_f = left_f + right_f;
            break;

        case INST_MUL:
            output_f = left_f * right_f;
            break;

        case INST_DIV:
            output_f = left_f / right_f;
            break;

        case INST_EXP:
            output_f = powf(left_f, right_f);
            break;

        default:
            printf("No handling for this instruction type: %d\n", inst->type);
            exit(EXIT_FAILURE);
        }

        set_local(state, inst, 0, TYPE_FLOAT, &output_f);
    }
    else if (left->type == TYPE_LONG_DOUBLE && right->type == TYPE_LONG_DOUBLE)
    {
        long double left_ld = *((long double*)left->data);
        long double right_ld = *((long double*)right->data);
        long double output_ld;

        switch (inst->type)
        {
        case INST_ADD:
            output_ld = left_ld + right_ld;
            break;

        case INST_MUL:
            output_ld = left_ld * right_ld;
            break;

        case INST_DIV:
            output_ld = left_ld / right_ld;
            break;

        case INST_EXP:
            output_ld = powf(left_ld, right_ld);
            break;

        default:
            printf("No handling for this instruction type: %d\n", inst->type);
            exit(EXIT_FAILURE);
        }

        set_local(state, inst, 0, TYPE_LONG_DOUBLE, &output_ld);
    }
    else
    {
        printf("interpret() can't handle these types: %d, %d\n",
                left->type, right->type);
        exit(EXIT_FAILURE);
    }
}

// Interpret the current line in a given [state] and advance the instruction
// pointer if necessary.
void interpret(State* state)
{
    Instruction* inst = state->instructions[state->inst_ptr];

    switch (inst->type)
    {
    case INST_LET:
        interpret_let(state, inst);
        break;

    case INST_ADD:
    case INST_MUL:
    case INST_DIV:
    case INST_EXP:
        interpret_basic_math(state, inst);
        break;

    case INST_SIN:;
        interpret_sin(state, inst);
        break;

    case INST_ASIN:
        interpret_asin(state, inst);
        break;

    case INST_RET:
        state->ret = resolve(state, inst->params[0]);
        state->inst_ptr = state->instruction_count;
        break;

    default:
        printf("Unrecognized instruction type: %d\n", inst->type);
        exit(0);
    }

    state->inst_ptr++;
}
