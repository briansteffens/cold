#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "interpreter.h"
#include "compiler.h"
#include "solver.h"
#include "generator.h"

int usage()
{
    printf("Usage: bin/cold solve solvers/emc2.solve\n");
    printf("       bin/cold run examples/basic.cold i3\n");
    return 0;
}

void handle_solver(int argc, char* argv[])
{
    char* solver_file = NULL;
    int assembly_index = -1;
    bool output_generated = false;

    struct Context ctx;

    for (int i = 0; i < argc; i++)
    {
        if (starts_with(argv[i], "--assembly="))
        {
            assembly_index = atoi(argv[i] + 11);
        }
        else if (strcmp(argv[i], "--output-all") == 0)
        {
            output_generated = true;
        }
        else
        {
            solver_file = argv[i];
        }
    }

    if (solver_file == NULL)
    {
        usage();
        return;
    }

    solve(solver_file, assembly_index, output_generated);
}

void handle_run(const char* filename, char** inputs, int inputs_count)
{
    const char* DEBUG_OUTPUT = "output/debug";
    remove(DEBUG_OUTPUT);

    int function_count;
    struct Function** functions = parse_file(filename, &function_count);

    // Find the main function
    struct Function* main_function = NULL;

    for (int i = 0; i < function_count; i++)
    {
        if (strcmp(functions[i]->name, "main") == 0)
        {
            main_function = functions[i];
            break;
        }
    }

    if (main_function == NULL)
    {
        printf("No main function found.\n");
        exit(0);
    }

    if (main_function->arg_count != inputs_count)
    {
        printf("Main function requires %d inputs but only %d given.\n",
                main_function->arg_count, inputs_count);
        exit(0);
    }

    struct State* state = malloc(sizeof(struct State));

    // Use CLI inputs as function arguments
    state->local_count = inputs_count;
    state->locals = malloc(inputs_count * sizeof(struct Local*));
    state->locals_owned = malloc(inputs_count * sizeof(bool));

    for (int i = 0; i < inputs_count; i++)
    {
        state->locals[i] = malloc(sizeof(struct Local));

        // Strip preceding $ symbol from argument name if present
        char* arg_name = main_function->args[i];
        if (arg_name[0] == '$')
        {
            arg_name++;
        }
        state->locals[i]->name = strdup(arg_name);

        state->locals[i]->value = malloc(sizeof(struct Value));
        value_set_from_string(state->locals[i]->value, inputs[i]);

        state->locals_owned[i] = true;
    }

    // Load function instructions into the state
    state->instruction_count = main_function->inst_count;
    state->instructions = malloc(
            state->instruction_count * sizeof(struct Instruction*));
    state->instructions_owned = malloc(
            state->instruction_count * sizeof(bool));
    for (int i = 0; i < state->instruction_count; i++)
    {
        state->instructions[i] = instruction_clone(&main_function->insts[i]);
        state->instructions_owned[i] = true;
    }

    state->ret = NULL;
    state->inst_ptr = 0;

    // Open file for debug output
    FILE* file = fopen(DEBUG_OUTPUT, "a");
    if (file == NULL)
    {
        printf("Error opening [%s] for writing.\n", DEBUG_OUTPUT);
        exit(0);
    }

    const int BUF_LEN = 255;
    char buf[BUF_LEN];

    // Run!
    while (state->inst_ptr < state->instruction_count)
    {
        // Output locals
        for (int i = 0; i < state->local_count; i++)
        {
            value_tostring(state->locals[i]->value, buf, BUF_LEN);
            fprintf(file, "  %s = %s [%s]\n", state->locals[i]->name, buf,
                var_type_tostring(state->locals[i]->value->type));
        }

        // Output instruction to be run
        instruction_tostring(state->instructions[state->inst_ptr],
            buf, BUF_LEN);
        fprintf(file, "%s\n", buf);

        interpret(state);
    }

    fclose(file);

    // Output return value
    value_tostring(state->ret, buf, BUF_LEN);
    printf("Return: %s [%s]\n", buf, var_type_tostring(state->ret->type));

    // Free stuff
    free_state(state);

    for (int i = 0; i < function_count; i++)
    {
        free_function(functions[i]);
        free(functions[i]);
    }

    free(functions);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        return usage();
    }

    if (strcmp(argv[1], "solve") == 0)
    {
        handle_solver(argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "run") == 0)
    {
        handle_run(argv[2], argv + 3, argc - 3);
    }
    else
    {
        return usage();
    }

    return 0;
}

/*
int handle_run(int argc, char* argv[])
{
    if (argc != 1)
    {
        printf("usage: coldc run SOURCE_FILE\n");
        return 0;
    }

    struct Function* functions;
    int function_count = parse_file(argv[0], &functions);

    if (function_count < 0)
    {
        printf("Failed to parse source file [%s]\n", argv[0]);
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

int handle_solve(int argc, char* argv[])
{
    return 0;
}

int handle_generate(int argc, char* argv[])
{
    generate();
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "run") == 0)
    {
        return handle_run(argc - 2, argv + 2);
    }

    if (argc > 1 && strcmp(argv[1], "solve") == 0)
    {
        return handle_solve(argc - 2, argv + 2);
    }

    if (argc > 1 && strcmp(argv[1], "generate") == 0)
    {
        return handle_generate(argc - 2, argv + 2);
    }

    printf("usage: cold [run|solve]\n");
    return 1;
}
*/
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
*/
