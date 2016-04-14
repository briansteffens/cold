#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "interpreter.h"
#include "compiler.h"
#include "solver.h"
#include "generator.h"

bool starts_with(const char* haystack, const char* needle)
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

int usage()
{
    printf("Usage: bin/cold solve solvers/emc2.solve\n");
    printf("       bin/cold run examples/basic.cold i3\n");
    return 0;
}

void handle_solver(const char* solver_file)
{
    struct Context ctx;

    ctx.programs_completed = 0;

    // Output all generated programs to this file
    ctx.generated_programs_filename = "output/generated_programs";

    // If outputing generated programs, delete the last output first
    if (ctx.generated_programs_filename)
    {
        remove(ctx.generated_programs_filename);
    }

    // Default float precision
    value_set_float(&ctx.precision, 0.0f);

    // Default pattern depth
    ctx.depth = 3;

    ctx.pattern_count = 0;
    ctx.patterns = malloc(0);

    ctx.constant_count = 0;
    ctx.constants = malloc(0);

    ctx.input_count = 0;
    ctx.input_names = malloc(0);

    ctx.case_count = 0;
    ctx.cases = malloc(0);

    ctx.solution_inst = NULL;
    ctx.solution_inst_count = 0;

    // Read solve file
    FILE* file = fopen(solver_file, "r");

    if (file == 0)
    {
        printf("Failed to open source file [%s]\n", solver_file);
        return;
    }

    int line_count;
    char** lines = read_lines(file, &line_count);

    fclose(file);

    // Parse solve file
    for (int i = 0; i < line_count; i++)
    {
        char* raw = NULL;

        // Strip out comments
        char* comment_start = strchr(lines[i], '#');
        if (comment_start != NULL)
        {
            raw = strndup(lines[i], comment_start - lines[i]);
        }
        else
        {
            raw = strdup(lines[i]);
        }

        char* line = trim(raw);
        free(raw);

        if (starts_with(line, "precision "))
        {
            if ((line + 10)[0] != 'f')
            {
                printf("Expected a float for precision declaration\n");
                exit(0);
            }

            value_set_float(&ctx.precision, atof(line + 11));
        }
        else if (starts_with(line, "depth "))
        {
            ctx.depth = atoi(line + 6);
        }
        else if (starts_with(line, "pattern "))
        {
            char fn[80];

            strcpy(fn, "patterns/");
            strcat(fn, line + 8);
            strcat(fn, ".pattern");

            add_pattern(&ctx, fn);
        }
        else if (starts_with(line, "constant "))
        {
            ctx.constant_count++;
            ctx.constants = realloc(ctx.constants,
                    ctx.constant_count * sizeof(struct Value));

            value_set_from_string(&ctx.constants[ctx.constant_count - 1],
                    line + 9);
        }
        else if (starts_with(line, "input "))
        {
            ctx.input_count++;
            ctx.input_names = realloc(ctx.input_names,
                    ctx.input_count * sizeof(char*));

            ctx.input_names[ctx.input_count - 1] = strdup(line + 6);
        }
        else if (starts_with(line, "case "))
        {
            // Allocate a new case
            ctx.case_count++;
            ctx.cases = realloc(ctx.cases,
                    ctx.case_count * sizeof(struct Case));

            struct Case* new_case = &ctx.cases[ctx.case_count - 1];

            new_case->input_values = malloc(
                    ctx.input_count * sizeof(struct Value));

            // Split parameters
            int part_count = 0;
            char** parts = split(line + 5, ' ', &part_count);

            if (part_count - 1 != ctx.input_count)
            {
                printf("ERROR: case parameter count must match input count\n");
                exit(0);
            }

            // Set inputs
            for (int i = 0; i < part_count - 1; i++)
            {
                value_set_from_string(&new_case->input_values[i], parts[i]);
            }

            // Set expected output
            value_set_from_string(&new_case->expected, parts[part_count - 1]);

            // Free string split values
            for (int i = 0; i < part_count; i++)
                free(parts[i]);

            free(parts);
        }
        else if (strcmp(line, "") == 0)
        {
            // Blank line, ignore
        }
        else
        {
            printf("ERROR: Unrecognized solve file line [%s]\n", lines[i]);
        }

        free(line);
    }

    free(lines);

    // Setup pattern mask
    ctx.pattern_mask = malloc(ctx.depth * sizeof(bool*));

    for (int i = 0; i < ctx.depth; i++)
    {
        ctx.pattern_mask[i] = malloc(ctx.pattern_count * sizeof(bool));
        for (int j = 0; j < ctx.pattern_count; j++)
        {
            ctx.pattern_mask[i][j] = true;
        }
    }

    // Setup root state
    struct State** root = malloc(1 * sizeof(struct State*));

    root[0] = setup_state(&ctx, 0);

    root[0]->instruction_count = 1;
    root[0]->instructions = malloc(
        root[0]->instruction_count * sizeof(struct Instruction*));
    root[0]->instructions_owned = malloc(
        root[0]->instruction_count * sizeof(bool));

    // One "NEXT" instruction - a placeholder for code patterns
    root[0]->instructions[0] = malloc(sizeof(struct Instruction));
    root[0]->instructions[0]->type = INST_NEXT;
    root[0]->instructions[0]->pattern_depth = -1;
    params_allocate(root[0]->instructions[0], 0);

    root[0]->inst_ptr = 0;

    // gogogo
    step(&ctx, root, 1);

    print_status(&ctx, true);
    printf("\n");

    // Free the root state
    free_state(root[0]);
    free(root);

    // Free the context
    for (int i = 0; i < ctx.constant_count; i++)
        value_free(&ctx.constants[i]);
    free(ctx.constants);

    for (int i = 0; i < ctx.input_count; i++)
        free(ctx.input_names[i]);
    free(ctx.input_names);

    for (int i = 0; i < ctx.case_count; i++)
        free(ctx.cases[i].input_values);
    free(ctx.cases);

    for (int i = 0; i < ctx.pattern_count; i++)
        free_pattern(ctx.patterns[i]);
    free(ctx.patterns);

    for (int i = 0; i < ctx.depth; i++)
        free(ctx.pattern_mask[i]);
    free(ctx.pattern_mask);
}

void handle_run(const char* filename, char** inputs, int inputs_count)
{
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
        state->instructions_owned[i] = false;
    }

    state->ret = NULL;

    // Run!
    while (state->inst_ptr < state->instruction_count)
        interpret(state);

    // Output return value
    char buf[255];
    value_tostring(state->ret, buf, 255);
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
        handle_solver(argv[2]);
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
