#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "interpreter.h"
#include "compiler.h"
#include "solver.h"
#include "generator.h"
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

void solve_emc2()
{
    struct Context ctx;

    // Float precision
    value_set_float(&ctx.precision, 10000.0f);

    // Load patterns
    ctx.pattern_count = 0;
    ctx.patterns = malloc(0);

    add_pattern(&ctx, "patterns/let.pattern");
    add_pattern(&ctx, "patterns/add.pattern");
    add_pattern(&ctx, "patterns/mul.pattern");

    // Setup pattern mask
    ctx.depth = 3;
    ctx.pattern_mask = malloc(ctx.depth * sizeof(bool*));

    for (int i = 0; i < ctx.depth; i++)
    {
        ctx.pattern_mask[i] = malloc(ctx.pattern_count * sizeof(bool));
        for (int j = 0; j < ctx.pattern_count; j++)
        {
            ctx.pattern_mask[i][j] = true;
            printf("%d %d\n", i, j);
        }
    }

    // Setup context constants
    ctx.constant_count = 1;
    ctx.constants = malloc(ctx.constant_count * sizeof(struct Value));
    //value_set_int(&ctx.constants[0], 1);
    //value_set_int(&ctx.constants[1], 2);
    value_set_float(&ctx.constants[0], 300000000.0f); // speed of light
    //value_set_float(&ctx.constants[1], 1.0f);

    ctx.solution_inst = NULL;
    ctx.solution_inst_count = 0;

    // Function inputs
    ctx.input_count = 1;
    ctx.input_names = malloc(ctx.input_count * sizeof(char*));
    ctx.input_names[0] = "z";

    // Setup cases
    ctx.case_count = 2;
    ctx.cases = malloc(ctx.case_count * sizeof(struct Case));

    // Case 0
    ctx.cases[0].input_values = malloc(ctx.input_count * sizeof(struct Value));

    //value_set_int(&ctx.cases[0].input_values[0], 3);
    //value_set_int(&ctx.cases[0].expected, 7);
    value_set_float(&ctx.cases[0].input_values[0], 1.0f); // kg
    value_set_float(&ctx.cases[0].expected, 90000000000000000.0f); // joules

    // Case 1
    ctx.cases[1].input_values = malloc(ctx.input_count * sizeof(struct Value));

    //value_set_int(&ctx.cases[1].input_values[0], 4);
    //value_set_int(&ctx.cases[1].expected, 9);
    value_set_float(&ctx.cases[1].input_values[0], 86.18f); // kg
    value_set_float(&ctx.cases[1].expected, 7756200000000001024.0); // joules

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

    // Free the root state
    free_state(root[0]);
    free(root);

    // Free the context
    free(ctx.input_names);

    for (int i = 0; i < ctx.constant_count; i++)
        value_free(&ctx.constants[i]);
    free(ctx.constants);

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

bool starts_with(const char* haystack, const char* needle)
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

int main(int argc, char* argv[])
{
    struct Context ctx;

    // Default float precision
    value_set_float(&ctx.precision, 0.0f);

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
    const char* filename = "test.solve";
    FILE* file = fopen(filename, "r");

    if (file == 0)
    {
        printf("Failed to open source file [%s]\n", filename);
        return -1;
    }

    int line_count;
    char** lines = read_lines(file, &line_count);

    fclose(file);

    // Parse solve file
    for (int i = 0; i < line_count; i++)
    {
        printf("%d|%s\n", i, lines[i]);

        // Comment
        if (starts_with(lines[i], "#"))
        {
            continue;
        }

        if (starts_with(lines[i], "precision "))
        {
            if ((lines[i] + 10)[0] != 'f')
            {
                printf("Expected a float for precision declaration\n");
                exit(0);
            }

            value_set_float(&ctx.precision, atof(lines[i] + 11));

            continue;
        }

        if (starts_with(lines[i], "pattern "))
        {
            char fn[80];

            strcpy(fn, "patterns/");
            strcat(fn, lines[i] + 8);
            strcat(fn, ".pattern");

            add_pattern(&ctx, fn);

            continue;
        }

        if (starts_with(lines[i], "constant "))
        {
            ctx.constant_count++;
            ctx.constants = realloc(ctx.constants,
                    ctx.constant_count * sizeof(struct Value));

            value_set_from_string(&ctx.constants[ctx.constant_count - 1],
                    lines[i] + 9);

            continue;
        }

        if (starts_with(lines[i], "input "))
        {
            ctx.input_count++;
            ctx.input_names = realloc(ctx.input_names,
                    ctx.input_count * sizeof(char*));

            ctx.input_names[ctx.input_count - 1] = strdup(lines[i] + 6);
        }

        if (starts_with(lines[i], "case "))
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
            char** parts = split(lines[i] + 5, ' ', &part_count);

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

            continue;
        }
    }

    free(lines);

    // Setup pattern mask
    ctx.depth = 3;
    ctx.pattern_mask = malloc(ctx.depth * sizeof(bool*));

    for (int i = 0; i < ctx.depth; i++)
    {
        ctx.pattern_mask[i] = malloc(ctx.pattern_count * sizeof(bool));
        for (int j = 0; j < ctx.pattern_count; j++)
        {
            ctx.pattern_mask[i][j] = true;
            printf("%d %d\n", i, j);
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

    return 0;
}
