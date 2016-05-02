#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "general.h"
#include "interpreter.h"
#include "compiler.h"
#include "solver.h"
#include "combiner.h"

int usage()
{
    printf("Usage: bin/cold solve solvers/emc2.solve\n");
    printf("       bin/cold run examples/basic.cold i3\n");
    return 0;
}

void handle_solver(int argc, char* argv[])
{
    SolveArgs args;

    args.solver_file = NULL;
    args.combination_start = -1;
    args.combination_count = 1;
    args.output_generated = false;
    args.threads = 1;
    args.interactive = true;
    args.output_dir = "output/";
    args.print_solutions = true;
    args.find_all_solutions = false;

    for (int i = 0; i < argc; i++)
    {
        if (starts_with(argv[i], "--combination="))
        {
            args.combination_start = atoi(argv[i] + 14);
        }
        else if (starts_with(argv[i], "--combination-count="))
        {
            args.combination_count = atoi(argv[i] + 20);
        }
        else if (strcmp(argv[i], "--output-all") == 0)
        {
            args.output_generated = true;
        }
        else if (starts_with(argv[i], "--threads="))
        {
            args.threads = atoi(argv[i] + 10);
        }
        else if (strcmp(argv[i], "--non-interactive") == 0)
        {
            args.interactive = false;
        }
        else if (starts_with(argv[i], "--output-dir="))
        {
            args.output_dir = argv[i] + 13;
        }
        else if (strcmp(argv[i], "--hide-solutions") == 0)
        {
            args.print_solutions = false;
        }
        else if (strcmp(argv[i], "--all") == 0)
        {
            args.find_all_solutions = true;
        }
        else
        {
            args.solver_file = argv[i];
        }
    }

    if (args.solver_file == NULL)
    {
        usage();
        return;
    }

    solve(&args);
}

void handle_run(const char* filename, char** inputs, int inputs_count)
{
    const char* DEBUG_OUTPUT = "output/debug";
    remove(DEBUG_OUTPUT);

    int function_count;
    Function** functions = parse_file(filename, &function_count);

    // Find the main function
    Function* main_function = NULL;

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

    State* state = malloc(sizeof(State));

    // Use CLI inputs as function arguments
    state->local_count = inputs_count;
    state->locals = malloc(inputs_count * sizeof(Local*));
    state->locals_owned = malloc(inputs_count * sizeof(bool));

    for (int i = 0; i < inputs_count; i++)
    {
        state->locals[i] = malloc(sizeof(Local));

        // Strip preceding $ symbol from argument name if present
        char* arg_name = main_function->args[i];
        if (arg_name[0] == '$')
        {
            arg_name++;
        }
        state->locals[i]->name = strdup(arg_name);

        state->locals[i]->value = malloc(sizeof(Value));
        value_set_from_string(state->locals[i]->value, inputs[i]);

        state->locals_owned[i] = true;
    }

    // Load function instructions into the state
    state->instruction_count = main_function->inst_count;
    state->instructions = malloc(
            state->instruction_count * sizeof(Instruction*));
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
    state_free(state);

    for (int i = 0; i < function_count; i++)
    {
        function_free(functions[i]);
        free(functions[i]);
    }

    free(functions);
}

void handle_combinations(int argc, char* argv[])
{
    if (argc != 1)
    {
        usage();
        return;
    }

    Context ctx;
    parse_solver_file(&ctx, argv[0]);

    int combination_count = count_combinations(&ctx);
    char buf[255];

    for (int i = 0; i < combination_count; i++)
    {
        Combination combination;
        combine(&ctx, i, &combination);

        printf("combination %d:\n", i);

        for (int i = 0; i < combination.instruction_count; i++)
        {
            instruction_tostring(combination.instructions[i], buf, 255);
            printf("  %s\n", buf);
        }

        combination_free(&combination);

        printf("\n");
    }
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
    else if (strcmp(argv[1], "combinations") == 0)
    {
        handle_combinations(argc - 2, argv + 2);
    }
    else
    {
        return usage();
    }

    return 0;
}
