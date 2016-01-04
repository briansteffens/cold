#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("usage: coldc source_file\n");
        return 0;
    }

    struct Function* functions;
    int function_count = parse_file(argv[1], &functions);

    if (function_count < 0)
    {
        printf("Failed to parse source file [%s]\n", argv[1]);
        return 1;
    }

    for (int i = 0; i < function_count; i++)
    {
        printf("FUNC %d %s\n", &functions[i], functions[i].name);
        free_function(&functions[i]);
    }

    free(functions);

    return 0;
}

/*
void write_code(struct State* state)
{
    state->instruction_count = 3;
    state->instructions = malloc(state->instruction_count *
                                 sizeof(struct Instruction*));
    state->instructions_owned = malloc(state->instruction_count * sizeof(bool));

    struct Instruction* inst = NULL;

    // float x = 7.0f;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_LET;
    params_allocate(inst, 2);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "x");

    inst->params[1]->type = PARAM_LITERAL;
    value_set_float(inst->params[1]->value, 7.0f);

    state->instructions[0] = inst;

    // x = [l] * 2.0f;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_MUL;

    params_allocate(inst, 3);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "x");

    inst->params[1]->type = PARAM_PATTERN;
    value_set_int(inst->params[1]->value, PTRN_LOCALS);

    inst->params[2]->type = PARAM_LITERAL;
    value_set_float(inst->params[2]->value, 1.5f);

    state->instructions[1] = inst;

    // x = [l] + 1.0f;
    inst = malloc(sizeof(struct Instruction));
    inst->type = INST_ADD;

    params_allocate(inst, 3);

    inst->params[0]->type = PARAM_LABEL;
    value_set_string(inst->params[0]->value, "x");

    inst->params[1]->type = PARAM_PATTERN;
    value_set_int(inst->params[1]->value, PTRN_LOCALS);

    inst->params[2]->type = PARAM_LITERAL;
    value_set_float(inst->params[2]->value, 1.0f);

    state->instructions[2] = inst;

    for (int i = 0; i < state->instruction_count; i++)
        state->instructions_owned[i] = true;
}

int main(int argc, char* argv[])
{
    struct Context ctx;
    value_set_float(&ctx.precision, 0.000005f);

    ctx.solution_inst = NULL;
    ctx.solution_inst_count = 0;

    ctx.input_count = 1;
    ctx.input_names = malloc(ctx.input_count * sizeof(char*));
    ctx.input_names[0] = "z";
    
    ctx.case_count = 2;
    ctx.cases = malloc(ctx.case_count * sizeof(struct Case));

    ctx.cases[0].input_values = malloc(ctx.input_count * sizeof(struct Value));
    
    value_set_float(&ctx.cases[0].input_values[0], 4.000001f);
    value_set_float(&ctx.cases[0].expected, 7.0f);
    
    ctx.cases[1].input_values = malloc(ctx.input_count * sizeof(struct Value));

    value_set_float(&ctx.cases[1].input_values[0], 5.000001f);
    value_set_float(&ctx.cases[1].expected, 8.5f);

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
*/
