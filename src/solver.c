#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>

#include "cold.h"
#include "general.h"
#include "interpreter.h"
#include "solver.h"
#include "compiler.h"
#include "permute.h"
#include "combiner.h"

typedef struct SolveThreadArgs
{
    // Input args (write by solve, read by solve_thread)
    const char* solver_file;
    Combination combination;
    char* output_dir;
    bool output_generated;
    bool print_solutions;
    bool find_all_solutions;

    // Return values (write by solve_thread, read by solve)
    bool ret_done;
    unsigned long ret_programs_completed;
    bool ret_solved;
} SolveThreadArgs;

// Load a pattern file into a context
bool add_pattern(Context* ctx, const char* filename)
{
    FILE* file = fopen(filename, "r");

    if (file == 0)
    {
        printf("Failed to open pattern file [%s]\n", filename);
        return false;
    }

    int line_count;
    char** lines = read_lines(file, &line_count);

    fclose(file);

    ctx->pattern_count++;
    ctx->patterns = realloc(ctx->patterns,
        ctx->pattern_count * sizeof(Pattern*));

    Pattern* ret = malloc(sizeof(Pattern));
    ret->inst_count = 0;
    ret->insts = malloc(ret->inst_count * sizeof(Instruction));

    for (int i = 0; i < line_count; i++)
    {
        int part_count;
        char** parts = split(lines[i], ' ', &part_count);

        ret->inst_count++;
        ret->insts = realloc(ret->insts,
            ret->inst_count * sizeof(Instruction*));
        ret->insts[ret->inst_count - 1] = malloc(sizeof(Instruction));

        parse_instruction(ret->insts[ret->inst_count - 1], parts, part_count);

        for (int j = 0; j < part_count; j++)
        {
            free(parts[j]);
        }
        free(parts);
        free(lines[i]);
    }

    free(lines);

    ctx->patterns[ctx->pattern_count - 1] = ret;

    return true;
}

// Given a [Context] and a test case, construct a State representing it
State* setup_state(Context* ctx, int case_index)
{
    State* ret = malloc(1 * sizeof(State));

    ret->local_count = ctx->input_count;
    ret->locals = malloc(ret->local_count * sizeof(Local*));
    ret->locals_owned = malloc(ret->local_count * sizeof(bool));

    for (int i = 0; i < ctx->input_count; i++)
    {
        ret->locals[i] = malloc(sizeof(Local));
        ret->locals[i]->name = strdup(ctx->input_names[i]);
        ret->locals[i]->value =
            value_clone(&ctx->cases[case_index].input_values[i]);

        ret->locals_owned[i] = true;
    }

    ret->ret = NULL;

    return ret;
}

// Clone an interpreter state, leaving pointers to the [orig] state's locals
// and instructions.
//
// The return value must be freed by the caller.
State* state_fork(State* orig)
{
    State* ret = malloc(sizeof(State));

    // Shallow copy locals to the new state
    ret->local_count = orig->local_count;
    ret->locals_owned = malloc(ret->local_count * sizeof(bool));
    ret->locals = malloc(ret->local_count * sizeof(Local*));

    for (int i = 0; i < ret->local_count; i++)
    {
        ret->locals[i] = orig->locals[i];
        ret->locals_owned[i] = false;
    }

    // Shallow copy instructions to the new state
    ret->instruction_count = orig->instruction_count;
    ret->instructions_owned = malloc(ret->instruction_count * sizeof(bool));
    ret->instructions = malloc(ret->instruction_count * sizeof(Instruction*));

    for (int i = 0; i < ret->instruction_count; i++)
    {
        ret->instructions[i] = orig->instructions[i];
        ret->instructions_owned[i] = false;
    }

    ret->inst_ptr = orig->inst_ptr;

    return ret;
}

// Fork the state [input] for each permutation of the current instruction.
//
// The return value must be freed by the caller.
State** vary(Context* ctx, State* input, int* state_count)
{
    int inst_count = 0;
    Instruction** insts = permute_instruction(ctx, input,
            input->instructions[input->inst_ptr], &inst_count);

    State** ret = malloc(inst_count * sizeof(State*));

    for (int i = 0; i < inst_count; i++)
    {
        ret[i] = state_fork(input);
        ret[i]->instructions[ret[i]->inst_ptr] = insts[i];
        ret[i]->instructions_owned[ret[i]->inst_ptr] = true;
    }

    free(insts);
    *state_count = inst_count;

    return ret;
}

// Check a state's locals for an expected value
Local* expect(Context* ctx, State* state, Value* expected)
{
    for (int k = 0; k < state->local_count; k++)
    {
        if (!compare(ctx, state->locals[k]->value, expected))
            continue;

        return state->locals[k];
    }

    return NULL;
}

// Check all (except the first, which is assumed to have already been checked
// by the caller) cases against a state program. If the same local in all
// case executions finds the expected output of its case, the program contained
// in state is considered a solution to the context being run.
void check_cases(Context* ctx, State* base, Local* found)
{
    State** states = malloc(sizeof(State*));

    // Setup return instruction
    Instruction* ret_inst = malloc(sizeof(Instruction));
    ret_inst->type = INST_RET;
    params_allocate(ret_inst, 1);
    ret_inst->params[0]->type = PARAM_LABEL;
    value_set_string(ret_inst->params[0]->value, found->name);

    for (int i = 1; i < ctx->case_count; i++)
    {
        // TODO: this state seems to have previous runs' data in it sometimes?
        states[0] = setup_state(ctx, i);

        states[0]->instruction_count = base->inst_ptr + 1;
        states[0]->instructions = malloc(states[0]->instruction_count *
            sizeof(Instruction*));
        states[0]->instructions_owned = malloc(
            states[0]->instruction_count * sizeof(bool));

        for (int j = 0; j <= base->inst_ptr; j++)
        {
            states[0]->instructions[j] = base->instructions[j];
            states[0]->instructions_owned[j] = false;
        }

        states[0]->instructions[states[0]->instruction_count - 1] = ret_inst;
        states[0]->instructions_owned[states[0]->instruction_count - 1] =
                false;

        states[0]->inst_ptr = 0;
        while (states[0]->inst_ptr < states[0]->instruction_count)
        {
            interpret(states[0]);
        }

        bool success = true;
        if (!compare(ctx, states[0]->ret, &ctx->cases[i].expected))
        {
            success = false;
        }
        else if (i == ctx->case_count - 1)
        {
            // Success: copy solution program to context
            ctx->solution_inst_count = states[0]->instruction_count;
            ctx->solution_inst = malloc(
                ctx->solution_inst_count * sizeof(Instruction*));

            for (int i = 0; i < ctx->solution_inst_count; i++)
                ctx->solution_inst[i] = instruction_clone(
                    states[0]->instructions[i]);
        }

        state_free(states[0]);

        if (!success)
            break;
    }

    instruction_free(ret_inst);
    free(ret_inst);
    free(states);
}

// Bounds-check a state's instruction pointer
bool is_execution_finished(State* state)
{
    return state->inst_ptr >= state->instruction_count;
}

// Append a program to generated_programs file
void fprint_generated_program(Context* ctx, State* state)
{
    FILE* file = fopen(ctx->generated_programs_filename, "a");

    if (file == NULL)
    {
        printf("Error opening [%s] for writing.\n",
                ctx->generated_programs_filename);
        exit(0);
    }

    print_program(file, state->instructions, state->instruction_count,
            ctx->input_names, ctx->input_count);

    fprintf(file, "\n");

    fclose(file);
}

// Forward declaration because step and step_vary are mutually-dependent
void step(Context* ctx, State** states, int state_count);

// Fork the state for each possible variance of the next instruction and
// execute those new instructions.
void step_vary(Context* ctx, State* state)
{
    if (is_execution_finished(state))
    {
        (*ctx->programs_completed)++;

        if (ctx->generated_programs_filename)
        {
            fprint_generated_program(ctx, state);
        }

        return;
    }

    int varied_count = 0;
    State** varied = vary(ctx, state, &varied_count);

    for (int j = 0; j < varied_count; j++)
    {
        // Interpret the varied line
        interpret(varied[j]);

        // Check for the expected case output
        Local* found = expect(ctx, varied[j], &ctx->cases[0].expected);

        if (!found)
            continue;

        // We found a program that solves the first case. Try the other cases
        // to see if the program solves those cases too.
        check_cases(ctx, varied[j], found);

        if (!ctx->solution_inst)
            continue;

        // Write solution to solution file
        if (ctx->solution_inst != NULL)
        {
            FILE* solution_file = fopen(ctx->solution_fn, "a");

            if (solution_file == 0)
            {
                printf("Failed to open solution file [%s]\n",
                        ctx->solution_fn);
                exit(EXIT_FAILURE);
            }

            print_program(solution_file, ctx->solution_inst,
                    ctx->solution_inst_count, ctx->input_names,
                    ctx->input_count);

            fprintf(solution_file, "---\n");

            fclose(solution_file);
        }

        // Output solution to the console
        if (ctx->print_solutions)
        {
            printf("\n");
            print_program(stdout, ctx->solution_inst, ctx->solution_inst_count,
                    ctx->input_names, ctx->input_count);
        }

        if (!ctx->find_all_solutions)
        {
            break;
        }

        // Clear solution and keep looking for more
        free(ctx->solution_inst);
        ctx->solution_inst = NULL;
        ctx->solution_inst_count = 0;
    }

    // If no solution has been found yet, continue recursion
    if (!ctx->solution_inst)
    {
        step(ctx, varied, varied_count);
    }

    // Free forked states
    for (int j = 0; j < varied_count; j++)
    {
        state_free(varied[j]);
    }

    free(varied);
}

// Interpret the current line in each of the given [states], forking the
// interpeter state as necessary for any possible permutations of the
// current instruction.
void step(Context* ctx, State** states, int state_count)
{
    for (int i = 0; i < state_count; i++)
    {
        step_vary(ctx, states[i]);

        // End execution if a solution was found
        if (!ctx->find_all_solutions && ctx->solution_inst)
        {
            break;
        }
    }
}

void parse_solver_file(Context* ctx, const char* solver_file)
{
    // Default float precision
    value_set_float(&ctx->precision, 0.0f);

    // Default pattern depth
    ctx->depth = 3;

    ctx->pattern_count = 0;
    ctx->patterns = malloc(0);

    ctx->constant_count = 0;
    ctx->constants = malloc(0);

    ctx->input_count = 0;
    ctx->input_names = malloc(0);

    ctx->case_count = 0;
    ctx->cases = malloc(0);

    FILE* file = fopen(solver_file, "r");

    if (file == 0)
    {
        printf("Failed to open source file [%s]\n", solver_file);
        exit(EXIT_FAILURE);
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

        free(lines[i]);
        char* line = trim(raw);
        free(raw);

        if (starts_with(line, "precision "))
        {
            free(ctx->precision.data);
            value_set_from_string(&ctx->precision, line + 10);
        }
        else if (starts_with(line, "depth "))
        {
            ctx->depth = atoi(line + 6);
        }
        else if (starts_with(line, "pattern "))
        {
            char fn[80];

            strcpy(fn, "patterns/");
            strcat(fn, line + 8);
            strcat(fn, ".pattern");

            add_pattern(ctx, fn);
        }
        else if (starts_with(line, "constant "))
        {
            ctx->constant_count++;
            ctx->constants = realloc(ctx->constants,
                    ctx->constant_count * sizeof(Value));

            value_set_from_string(&ctx->constants[ctx->constant_count - 1],
                    line + 9);
        }
        else if (starts_with(line, "input "))
        {
            ctx->input_count++;
            ctx->input_names = realloc(ctx->input_names,
                    ctx->input_count * sizeof(char*));

            ctx->input_names[ctx->input_count - 1] = strdup(line + 6);
        }
        else if (starts_with(line, "case "))
        {
            // Allocate a new case
            ctx->case_count++;
            ctx->cases = realloc(ctx->cases, ctx->case_count * sizeof(Case));

            Case* new_case = &ctx->cases[ctx->case_count - 1];

            new_case->input_values = malloc(ctx->input_count * sizeof(Value));

            // Find input parameter list
            char* inputs_start = strchr(line + 5, '(') + 1;
            char* inputs_stop = strchr(inputs_start, ')');
            int inputs_len = inputs_stop - inputs_start;

            char inputs[inputs_len + 1];
            strncpy(inputs, inputs_start, inputs_len);
            inputs[inputs_len] = 0;

            // Split inputs
            int part_count = 0;
            char** parts = split(inputs, ',', &part_count);

            if (part_count != ctx->input_count)
            {
                printf("ERROR: case parameter count must match input count\n");
                exit(0);
            }

            // Set inputs
            for (int i = 0; i < part_count; i++)
            {
                value_set_from_string(&new_case->input_values[i], parts[i]);
            }

            // Find expected output
            char* expected = strstr(inputs_stop + 1, "=>");

            if (!expected)
            {
                printf("case directive requires expected output\n");
                exit(EXIT_FAILURE);
            }

            char* expected_value = trim(expected + 2);

            // Set expected output
            value_set_from_string(&new_case->expected, expected_value);

            // Free string split values
            free(expected_value);

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
}

// Solve a single combination
void* solve_thread(void* ptr)
{
    SolveThreadArgs* args = (SolveThreadArgs*)ptr;

    Context ctx;

    ctx.print_solutions = args->print_solutions;
    ctx.find_all_solutions = args->find_all_solutions;

    // Create the output directory
    mkdir(args->output_dir, 0777);

    ctx.generated_programs_filename = NULL;

    if (args->output_generated)
    {
        // Build the generated output file path
        char output_generated[255];
        strcpy(output_generated, args->output_dir);
        strcat(output_generated, "generated");
        ctx.generated_programs_filename = output_generated;
    }

    // Build the solution file output path
    char solution_fn[255];
    strcpy(solution_fn, args->output_dir);
    strcat(solution_fn, "solution.cold");
    remove(solution_fn);

    ctx.solution_fn = solution_fn;

    ctx.programs_completed = &args->ret_programs_completed;

    ctx.solution_inst = NULL;
    ctx.solution_inst_count = 0;

    parse_solver_file(&ctx, args->solver_file);

    // Setup root state
    State** root = malloc(1 * sizeof(State*));

    root[0] = setup_state(&ctx, 0);
    load_combination(root[0], &args->combination);

    // gogogo
    step(&ctx, root, 1);

    args->ret_solved = (ctx.solution_inst != NULL);

    // Free the root state
    state_free(root[0]);
    free(root);

    // Free the context
    free(ctx.precision.data);

    for (int i = 0; i < ctx.solution_inst_count; i++)
    {
        instruction_free(ctx.solution_inst[i]);
        free(ctx.solution_inst[i]);
    }

    free(ctx.solution_inst);

    for (int i = 0; i < ctx.constant_count; i++)
    {
        free(ctx.constants[i].data);
    }
    free(ctx.constants);

    for (int i = 0; i < ctx.input_count; i++)
        free(ctx.input_names[i]);
    free(ctx.input_names);

    for (int i = 0; i < ctx.case_count; i++)
    {
        free(ctx.cases[i].expected.data);

        for (int j = 0; j < ctx.input_count; j++)
        {
            free(ctx.cases[i].input_values[j].data);
        }

        free(ctx.cases[i].input_values);
    }
    free(ctx.cases);

    for (int i = 0; i < ctx.pattern_count; i++)
    {
        pattern_free(ctx.patterns[i]);
    }
    free(ctx.patterns);

    args->ret_done = true;
    return NULL;
}

typedef struct SolveThreadInfo
{
    pthread_t thread;
    SolveThreadArgs args;
    bool started;
} SolveThreadInfo;

// Output solver status to STDOUT
void print_total_status(SolveThreadInfo info[], int threads,
        int completed_by_old_threads, time_t program_start, bool interactive)
{
    unsigned long total_completed = completed_by_old_threads;

    for (int i = 0; i < threads; i++)
    {
        if (!info[i].started)
        {
            continue;
        }

        total_completed += info[i].args.ret_programs_completed;
    }

    size_t now = time(NULL);
    float run_rate = (float)total_completed / (float)(now - program_start);

    if (interactive)
    {
        printf("\r");
    }

    printf("\rtotal: %lu, running %.0f/sec", total_completed, run_rate);

    if (!interactive)
    {
        printf("\n");
    }

    fflush(stdout);
}

// Bootstrap a multi-threaded solve operation
void solve(SolveArgs* args)
{
    // TODO: temp
    Context ctx;
    parse_solver_file(&ctx, args->solver_file);

    SolveThreadInfo info[args->threads];

    for (int i = 0; i < args->threads; i++)
    {
        info[i].started = false;
        info[i].args.ret_done = false;
        info[i].args.ret_programs_completed = 0;
    }

    // All combinations
    if (args->combination_start == -1)
    {
        Context ctx;
        parse_solver_file(&ctx, args->solver_file);

        args->combination_start = 0;
        args->combination_count = exponent(ctx.pattern_count, ctx.depth);
    }

    int combination_index = args->combination_start;

    mkdir(args->output_dir, 0777);

    if (args->output_generated)
    {
        remove("output/generated_programs");
    }

    unsigned long completed_by_old_threads = 0;
    static time_t last = 0;
    time_t program_start = time(NULL);

    while (true)
    {
        for (int i = 0; i < args->threads; i++)
        {
            if (info[i].started)
            {
                if (!info[i].args.ret_done)
                {
                    continue;
                }

                pthread_join(info[i].thread, NULL);

                // Cleanup finished thread data
                free(info[i].args.output_dir);
                combination_free(&info[i].args.combination);

                info[i].started = false;
                info[i].args.ret_done = false;
                completed_by_old_threads +=
                    info[i].args.ret_programs_completed;
                info[i].args.ret_programs_completed = 0;

                if (info[i].args.ret_solved && !args->find_all_solutions)
                {
                    goto all_done;
                }
            }

            if (combination_index >=
                args->combination_start + args->combination_count)
            {
                bool any_still_running = false;

                for (int j = 0; j < args->threads; j++)
                {
                    if (info[j].started)
                    {
                        any_still_running = true;
                        break;
                    }
                }

                if (!any_still_running)
                {
                    goto all_done;
                }

                continue;
            }

            // Launch new thread
            combine(&ctx, combination_index, &info[i].args.combination);

            info[i].args.solver_file = args->solver_file;
            info[i].args.output_generated = args->output_generated;
            info[i].args.ret_done = false;
            info[i].args.ret_solved = false;
            info[i].args.print_solutions = args->print_solutions;
            info[i].args.find_all_solutions = args->find_all_solutions;

            info[i].args.output_dir = malloc(255 * sizeof(char));
            sprintf(info[i].args.output_dir, "%s/%d/", args->output_dir,
                    combination_index);

            combination_index++;

            int res = pthread_create(&info[i].thread, NULL, solve_thread,
                    (void*)&info[i].args);

            if (res)
            {
                printf("Thread creation error: %d\n", res);
                exit(EXIT_FAILURE);
            }

            info[i].started = true;
        }

        time_t now = time(NULL);

        if (now > last)
        {
            print_total_status(info, args->threads, completed_by_old_threads,
                    program_start, args->interactive);
            last = now;
        }

        sleep(0);
    }

    all_done:
    print_total_status(info, args->threads, completed_by_old_threads,
            program_start, args->interactive);
    printf("\n");
    exit(EXIT_SUCCESS);
}
