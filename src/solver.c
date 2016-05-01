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
#include "compiler.h"
#include "permute.h"

typedef struct SolveThreadArgs
{
    // Input args (write by solve, read by solve_thread)
    const char* solver_file;
    char* output_dir;
    int combination;
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

// Find a single permutation of source code patterns identified by [target]
// where [target] is the index of the permutation between 0 and
// [patterns] ^ [maxdepth].
void permute(int result[], int max_depth, int patterns, int target)
{
    int total_permutations = exponent(patterns, max_depth);

    int inverse_depth = 0;
    for (int d = max_depth - 1; d >= 0; d--)
    {
        int repeat = 1;
        if (inverse_depth > 0)
        {
            repeat = exponent(patterns, inverse_depth);
        }

        int permutation = 0;
        while (permutation < total_permutations)
        {
            for (int p = 0; p < patterns; p++)
            {
                for (int r = 0; r < repeat; r++)
                {
                    if (permutation == target)
                    {
                        result[d] = p;
                        goto level_done;
                    }

                    permutation++;
                }
            }
        }

        level_done:
        inverse_depth++;
    }
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

        free_state(states[0]);

        if (!success)
            break;
    }

    instruction_free(ret_inst);
    free(ret_inst);
    free(states);
}

// Remove an instruction from a state's program at the given index
void remove_inst(State* state, int inst_index)
{
    // Free the instruction memory if this state owns it
    if (state->instructions_owned[inst_index])
    {
        instruction_free(state->instructions[inst_index]);
        free(state->instructions[inst_index]);
    }

    // Shuffle instruction data after the deleted instruction down one
    for (int i = inst_index; i < state->instruction_count - 1; i++)
    {
        state->instructions[i] = state->instructions[i + 1];
        state->instructions_owned[i] = state->instructions_owned[i + 1];
    }

    // Shrink the instruction set
    state->instruction_count--;
    state->instructions = realloc(state->instructions,
        state->instruction_count * sizeof(Instruction*));
    state->instructions_owned = realloc(state->instructions_owned,
        state->instruction_count * sizeof(bool));
}

// Bounds-check a state's instruction pointer
bool is_execution_finished(State* state)
{
    return state->inst_ptr >= state->instruction_count;
}

// Insert the instructions from a code pattern into a state's instruction list
// at the state's current instruction pointer
void insert_pattern(Pattern* pattern, State* state, int depth)
{
    int orig_inst_count = state->instruction_count;

    // Allocate space in the state's instruction set for the new instructions
    state->instruction_count += pattern->inst_count;
    state->instructions = realloc(state->instructions,
        state->instruction_count * sizeof(Instruction*));
    state->instructions_owned = realloc(state->instructions_owned,
        state->instruction_count * sizeof(bool));

    // Move instructions after the insertion point to the end of the array
    for (int i = orig_inst_count - 1; i > state->inst_ptr; i--)
    {
        int n = pattern->inst_count + 1;

        state->instructions[n] = state->instructions[i];
        state->instructions_owned[n] = state->instructions_owned[i];
    }

    // Copy the pattern instructions into the state
    for (int i = 0; i < pattern->inst_count; i++)
    {
        int n = state->inst_ptr + i;

        state->instructions[n] = instruction_clone(pattern->insts[i]);
        state->instructions[n]->pattern_depth = depth;
        state->instructions_owned[n] = true;
    }
}

// Replace a "NEXT" instruction with all code patterns in the pattern mask
// at the current depth level
State** vary_patterns(Context* ctx, State* state, int* patterned_count)
{
    // Get the next pattern depth level
    int depth = state->instructions[state->inst_ptr]->pattern_depth + 1;

    // Setup output values
    *patterned_count = 0;
    State** ret = malloc(0);

    // Remove the "NEXT" instruction being replaced
    remove_inst(state, state->inst_ptr);

    // Max depth passed, no more patterns. Return just the current state.
    if (depth >= ctx->depth)
    {
        *patterned_count = 1;
        ret = realloc(ret, *patterned_count * sizeof(State*));
        ret[0] = state;
        return ret;
    }

    // Set the number of state forks to the number of 'true' values in the
    // pattern mask at this depth.
    for (int i = 0; i < ctx->pattern_count; i++)
    {
        *patterned_count += ctx->pattern_mask[depth][i];
    }

    ret = realloc(ret, *patterned_count * sizeof(State*));

    // Fork the state for each pattern in the pattern mask at this depth and
    // insert that pattern in place of the removed "NEXT" instruction
    int i = 0;
    for (int p = 0; p < ctx->pattern_count; p++)
    {
        if (!ctx->pattern_mask[depth][p])
            continue;

        ret[i] = state_fork(state);

        insert_pattern(ctx->patterns[p], ret[i], depth);

        i++;
    }

    return ret;
}

// Write a program to a file
void fprint_program(FILE* file, Instruction** instructions,
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

    fprint_program(file, state->instructions, state->instruction_count,
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

            fprint_program(solution_file, ctx->solution_inst,
                    ctx->solution_inst_count, ctx->input_names,
                    ctx->input_count);

            fprintf(solution_file, "---\n");

            fclose(solution_file);
        }

        // Output solution to the console
        if (ctx->print_solutions)
        {
            printf("\n");
            print_program(ctx->solution_inst, ctx->solution_inst_count, true);
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
        free_state(varied[j]);
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
        // If the instruction to be interpreted is a "NEXT", it needs to be
        // replaced with patterns.
        if (states[i]->instructions[states[i]->inst_ptr]->type == INST_NEXT)
        {
            // Fork the state for each pattern in the mask
            int patterned_count = 0;
            State** patterned = vary_patterns(ctx, states[i],
                    &patterned_count);

            // Run the first line in the newly-inserted pattern in each fork
            for (int j = 0; j < patterned_count; j++)
            {
                step_vary(ctx, patterned[j]);

                // End execution if a solution was found
                if (!ctx->find_all_solutions && ctx->solution_inst)
                {
                    break;
                }
            }

            // Free pattern forked states
            for (int j = 0; j < patterned_count; j++)
            {
                // Only free the forked state if it's actually a new fork.
                // vary_patterns will return the passed-in state if no variance
                // was performed, in which case this function didn't "create"
                // the fork, so we shouldn't free it here.
                if (patterned[j] != states[i])
                {
                    free_state(patterned[j]);
                }
            }

            free(patterned);
        }

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

            // Split parameters
            int part_count = 0;
            char** parts = split(line + 5, ' ', &part_count);

            if (part_count - 1 != ctx->input_count)
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

    // Restrict pattern mask to specified combination
    if (args->combination >= 0)
    {
        int combination_patterns[ctx.depth];

        permute(combination_patterns, ctx.depth, ctx.pattern_count,
                args->combination);

        for (int d = 0; d < ctx.depth; d++)
        {
            for (int p = 0; p < ctx.pattern_count; p++)
            {
                ctx.pattern_mask[d][p] = combination_patterns[d] == p;
            }
        }
    }

    // Setup root state
    State** root = malloc(1 * sizeof(State*));

    root[0] = setup_state(&ctx, 0);

    root[0]->instruction_count = 1;
    root[0]->instructions = malloc(
        root[0]->instruction_count * sizeof(Instruction*));
    root[0]->instructions_owned = malloc(
        root[0]->instruction_count * sizeof(bool));

    // One "NEXT" instruction - a placeholder for code patterns
    root[0]->instructions[0] = malloc(sizeof(Instruction));
    root[0]->instructions[0]->type = INST_NEXT;
    root[0]->instructions[0]->pattern_depth = -1;
    root[0]->instructions_owned[0] = true;
    params_allocate(root[0]->instructions[0], 0);

    root[0]->inst_ptr = 0;

    // gogogo
    step(&ctx, root, 1);

    args->ret_solved = (ctx.solution_inst != NULL);

    // Free the root state
    free_state(root[0]);
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
        free_pattern(ctx.patterns[i]);
    }
    free(ctx.patterns);

    for (int i = 0; i < ctx.depth; i++)
    {
        free(ctx.pattern_mask[i]);
    }
    free(ctx.pattern_mask);

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
void solve(const char* solver_file, const char* output_dir, int threads,
        int combination_start, int combination_count, bool interactive,
        bool print_solutions, bool find_all_solutions, bool output_generated)
{
    SolveThreadInfo info[threads];

    for (int i = 0; i < threads; i++)
    {
        info[i].started = false;
        info[i].args.ret_done = false;
        info[i].args.ret_programs_completed = 0;
    }

    // All combinations
    if (combination_start == -1)
    {
        Context ctx;
        parse_solver_file(&ctx, solver_file);

        combination_start = 0;
        combination_count = exponent(ctx.pattern_count, ctx.depth);
    }

    int combination = combination_start;

    mkdir(output_dir, 0777);

    if (output_generated)
    {
        remove("output/generated_programs");
    }

    unsigned long completed_by_old_threads = 0;
    static time_t last = 0;
    time_t program_start = time(NULL);

    while (true)
    {
        for (int i = 0; i < threads; i++)
        {
            if (info[i].started)
            {
                if (!info[i].args.ret_done)
                {
                    continue;
                }

                pthread_join(info[i].thread, NULL);
                free(info[i].args.output_dir);
                info[i].started = false;
                info[i].args.ret_done = false;
                completed_by_old_threads +=
                    info[i].args.ret_programs_completed;
                info[i].args.ret_programs_completed = 0;
                if (info[i].args.ret_solved && !find_all_solutions)
                {
                    goto all_done;
                }
            }

            if (combination >= combination_start + combination_count)
            {
                bool any_still_running = false;

                for (int j = 0; j < threads; j++)
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

            info[i].args.solver_file = solver_file;
            info[i].args.combination = combination;
            info[i].args.output_generated = output_generated;
            info[i].args.ret_done = false;
            info[i].args.ret_solved = false;
            info[i].args.print_solutions = print_solutions;
            info[i].args.find_all_solutions = find_all_solutions;

            info[i].args.output_dir = malloc(255 * sizeof(char));
            sprintf(info[i].args.output_dir, "%s/%d/", output_dir,
                    combination);

            combination++;

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
            print_total_status(info, threads, completed_by_old_threads,
                    program_start, interactive);
            last = now;
        }

        sleep(0);
    }

    all_done:
    print_total_status(info, threads, completed_by_old_threads, program_start,
            interactive);
    printf("\n");
    exit(EXIT_SUCCESS);
}
