#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cold.h"
#include "interpreter.h"
#include "compiler.h"

void print_status(struct Context* ctx, bool always)
{
    static time_t last = 0;
    time_t now = time(NULL);

    if (!always && now == last)
    {
        return;
    }

    printf("\rprograms completed: %d", ctx->programs_completed);
    fflush(stdout);

    last = now;
}

struct State* state_fork(struct State* orig)
{
    struct State* ret = malloc(sizeof(struct State));
    //printf("FORK %d\n", ret);
    // Shallow copy locals to the new state
    ret->local_count = orig->local_count;
    ret->locals_owned = malloc(ret->local_count * sizeof(bool));
    ret->locals = malloc(ret->local_count * sizeof(struct Local*));

    for (int i = 0; i < ret->local_count; i++)
    {
        ret->locals[i] = orig->locals[i];
        ret->locals_owned[i] = false;
    }

    // Shallow copy instructions to the new state
    ret->instruction_count = orig->instruction_count;
    ret->instructions_owned = malloc(ret->instruction_count * sizeof(bool));
    ret->instructions = malloc(ret->instruction_count *
                               sizeof(struct Instruction*));

    for (int i = 0; i < ret->instruction_count; i++)
    {
        ret->instructions[i] = orig->instructions[i];
        ret->instructions_owned[i] = false;
    }

    ret->inst_ptr = orig->inst_ptr;

    return ret;
}

struct Instruction* instruction_clone(struct Instruction* orig)
{
    struct Instruction* ret = malloc(sizeof(struct Instruction));

    ret->type = orig->type;
    ret->param_count = orig->param_count;
    ret->params = malloc(ret->param_count * sizeof(struct Param*));

    for (int i = 0; i < ret->param_count; i++)
    {
        ret->params[i] = malloc(sizeof(struct Param));
        ret->params[i]->type = orig->params[i]->type;
        ret->params[i]->value = value_clone(orig->params[i]->value);
    }

    return ret;
}

// Calculate all possible replacement params for a param pattern
struct Param** vary_params(struct Context* ctx, struct State* state,
    struct Param* pattern, int* output_count)
{
    int flags = *((int*)pattern->value->data);

    int count = 0;
    if (flags & PTRN_LOCALS)
        count += state->local_count;
    if (flags & PTRN_CONSTANTS)
        count += ctx->constant_count;

    struct Param** ret = malloc(count * sizeof(struct Param*));

    int current = 0;

    if (flags & PTRN_LOCALS)
        for (int i = 0; i < state->local_count; i++)
        {
            ret[current] = malloc(sizeof(struct Param));
            ret[current]->value = malloc(sizeof(struct Value));
            ret[current]->type = PARAM_LABEL;
            value_set_string(ret[current]->value, state->locals[i]->name);
            current++;
        }

    if (flags & PTRN_CONSTANTS)
        for (int i = 0; i < ctx->constant_count; i++)
        {
            ret[current] = malloc(sizeof(struct Param));
            ret[current]->value = value_clone(&ctx->constants[i]);
            ret[current]->type = PARAM_LITERAL;
            current++;
        }

    *output_count = current;
    return ret;
}

struct Instruction** vary_instructions(struct Context* ctx,
    struct State* input, struct Instruction* inst, int* instruction_count)
{
    // TODO: why do we allocate for 1 with a count of 0?
    struct Instruction** ret = malloc(1 * sizeof(struct Instruction*));
    *instruction_count = 0;

    // Loop through the parameters in the instruction checking for patterns
    for (int i = 0; i < inst->param_count; i++)
    {
        // Non-patterns don't need to be replaced (varied)
        if (inst->params[i]->type != PARAM_PATTERN)
            continue;

        // Get the set of possible params to replace the pattern param with
        int param_count = 0;
        struct Param** varied = vary_params(ctx, input, inst->params[i],
            &param_count);

        // For each possible param, clone the current instruction and
        // substitute the pattern param for each variance
        for (int p = 0; p < param_count; p++)
        {
            struct Instruction* new_inst = instruction_clone(inst);
            new_inst->params[i] = varied[p];

            // Since there can be multiple pattern params in an instruction,
            // recur on each new clone to generate any further variances
            int recur_count = 0;
            struct Instruction** recurs = vary_instructions(ctx, input,
                new_inst, &recur_count);

            // Combine the result of the recursion with any results from this
            // run of the function
            ret = realloc(ret, (*instruction_count + recur_count) *
                          sizeof(struct Instruction*));

            for (int j = 0; j < recur_count; j++)
            {
                ret[*instruction_count] = recurs[j];
                *instruction_count = *instruction_count + 1;
            }

            free(recurs);
        }

        free(varied);

        break;
    }

    // If there were no variances found, return the original input
    if (*instruction_count == 0)
    {
        ret = realloc(ret, 1 * sizeof(struct Instruction*));
        ret[0] = inst;
        *instruction_count = 1;
    }

    return ret;
}

struct State** vary(struct Context* ctx, struct State* input, int* state_count)
{
    struct Instruction** insts = vary_instructions(ctx, input,
        input->instructions[input->inst_ptr], state_count);

    // If the instruction was varied (forked into multiple instructions),
    // then they are new copies and the new forked states "own" them.
    // Otherwise they're pointing to the input state's instruction.
    bool did_vary = (*state_count > 1);

    struct State** ret = malloc(*state_count * sizeof(struct State*));

    for (int i = 0; i < *state_count; i++)
    {
        ret[i] = state_fork(input);
        ret[i]->instructions[ret[i]->inst_ptr] = insts[i];
        ret[i]->instructions_owned[ret[i]->inst_ptr] = did_vary;
    }

    free(insts);

    return ret;
}

// Check a state's locals for an expected value
struct Local* expect(struct Context* ctx, struct State* state,
    struct Value* expected)
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
void check_cases(struct Context* ctx, struct State* base, struct Local* found)
{
    struct State** states = malloc(sizeof(struct State*));

    // Setup return instruction
    struct Instruction* ret_inst = malloc(sizeof(struct Instruction));
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
            sizeof(struct Instruction*));
        states[0]->instructions_owned = malloc(
            states[0]->instruction_count * sizeof(bool));

        for (int j = 0; j <= base->inst_ptr; j++)
        {
            states[0]->instructions[j] = base->instructions[j];
            states[0]->instructions_owned[j] = false;
        }

        states[0]->instructions[states[0]->instruction_count - 1] = ret_inst;
        states[0]->instructions_owned[states[0]->instruction_count - 1] = false;

        states[0]->inst_ptr = 0;
        while (states[0]->inst_ptr < states[0]->instruction_count)
            interpret(states[0]);

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
                ctx->solution_inst_count * sizeof(struct Instruction*));

            for (int i = 0; i < ctx->solution_inst_count; i++)
                ctx->solution_inst[i] = instruction_clone(
                    states[0]->instructions[i]);
        }

        free_state(states[0]);

        if (!success)
            break;
    }

    free(ret_inst);
    free(states);
}

// Remove an instruction from a state's program at the given index
void remove_inst(struct State* state, int inst_index)
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
        state->instruction_count * sizeof(struct Instruction*));
    state->instructions_owned = realloc(state->instructions_owned,
        state->instruction_count * sizeof(bool));
}

// Bounds-check a state's instruction pointer
bool is_execution_finished(struct State* state)
{
    return state->inst_ptr >= state->instruction_count;
}

// Insert the instructions from a code pattern into a state's instruction list
// at the state's current instruction pointer
void insert_pattern(struct Pattern* pattern, struct State* state, int depth)
{
    int orig_inst_count = state->instruction_count;

    // Allocate space in the state's instruction set for the new instructions
    state->instruction_count += pattern->inst_count;
    state->instructions = realloc(state->instructions,
        state->instruction_count * sizeof(struct Instruction*));
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
struct State** vary_patterns(struct Context* ctx, struct State* state,
    int* patterned_count)
{
    // Get the next pattern depth level
    int depth = state->instructions[state->inst_ptr]->pattern_depth + 1;

    // Setup output values
    *patterned_count = 0;
    struct State** ret = malloc(0);

    // Remove the "NEXT" instruction being replaced
    remove_inst(state, state->inst_ptr);

    // Max depth passed, no more patterns. Return just the current state.
    if (depth >= ctx->depth)
    {
        *patterned_count = 1;
        ret = realloc(ret, *patterned_count * sizeof(struct State*));
        ret[0] = state;
        return ret;
    }

    // Set the number of state forks to the number of 'true' values in the
    // pattern mask at this depth.
    for (int i = 0; i < ctx->pattern_count; i++)
    {
        *patterned_count += ctx->pattern_mask[depth][i];
    }

    ret = realloc(ret, *patterned_count * sizeof(struct State*));

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

// Append a program to generated_programs file
void fprint_program(struct Context* ctx, struct State* state)
{
    FILE* file = fopen(ctx->generated_programs_filename, "a");

    if (file == NULL)
    {
        printf("Error opening [%s] for writing.\n",
                ctx->generated_programs_filename);
        exit(0);
    }

    const int BUF_LEN = 255;
    char buf[BUF_LEN];

    for (int i = 0; i < state->instruction_count; i++)
    {
        instruction_tostring(state->instructions[i], buf, BUF_LEN);
        fprintf(file, "%s\n", buf);
    }

    fprintf(file, "\n");

    fclose(file);
}

// Forward declaration because step and step_vary are mutually-dependent
void step(struct Context* ctx, struct State** states, int state_count);

// Fork the state for each possible variance of the next instruction and
// execute those new instructions.
void step_vary(struct Context* ctx, struct State* state)
{
    if (is_execution_finished(state))
    {
        if (ctx->generated_programs_filename)
        {
            ctx->programs_completed++;
            fprint_program(ctx, state);
        }

        return;
    }

    int varied_count = 0;
    struct State** varied = vary(ctx, state, &varied_count);

    for (int j = 0; j < varied_count; j++)
    {
        // Interpret the varied line
        interpret(varied[j]);

        // Check for the expected case output
        struct Local* found = expect(ctx, varied[j], &ctx->cases[0].expected);

        if (!found)
            continue;

        // We found a program that solves the first case. Try the other cases
        // to see if the program solves those cases too.
        check_cases(ctx, varied[j], found);

        if (!ctx->solution_inst)
            continue;

        // Found a solution to all cases.
        printf("\n");
        print_program(ctx->solution_inst, ctx->solution_inst_count, true);
        break;
    }

    // If no solution has been found yet, continue recursion
    if (!ctx->solution_inst)
        step(ctx, varied, varied_count);

    // Free forked states
    for (int j = 0; j < varied_count; j++)
        free_state(varied[j]);

    free(varied);
}

void step(struct Context* ctx, struct State** states, int state_count)
{
    print_status(ctx, false);

    for (int i = 0; i < state_count; i++)
    {
        // If the instruction to be interpreted is a "NEXT", it needs to be
        // replaced with patterns.
        if (states[i]->instructions[states[i]->inst_ptr]->type == INST_NEXT)
        {
            // Fork the state for each pattern in the mask
            int patterned_count = 0;
            struct State** patterned = vary_patterns(ctx, states[i],
                &patterned_count);

            // Run the first line in the newly-inserted pattern in each fork
            for (int j = 0; j < patterned_count; j++)
            {
                step_vary(ctx, patterned[j]);

                // End execution if a solution was found
                if (ctx->solution_inst)
                    break;
            }

            // Free pattern forked states
            for (int j = 0; j < patterned_count; j++)
            {
                // Only free the forked state if it's actually a new fork.
                // vary_patterns will return the passed-in state if no variance
                // was performed, in which case this function didn't "create"
                // the fork, so we shouldn't free it here.
                if (patterned[j] != states[i])
                    free_state(patterned[j]);
            }

            free(patterned);
        }

        // End execution if a solution was found
        if (ctx->solution_inst)
            break;
    }
}

// Load a pattern file into a context
bool add_pattern(struct Context* ctx, const char* filename)
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
        ctx->pattern_count * sizeof(struct Pattern*));

    struct Pattern* ret = malloc(sizeof(struct Pattern));
    ret->inst_count = 0;
    ret->insts = malloc(ret->inst_count * sizeof(struct Instruction));

    for (int i = 0; i < line_count; i++)
    {
        int part_count;
        char** parts = split(lines[i], ' ', &part_count);

        ret->inst_count++;
        ret->insts = realloc(ret->insts,
            ret->inst_count * sizeof(struct Instruction*));
        ret->insts[ret->inst_count - 1] = malloc(sizeof(struct Instruction));

        parse_instruction(ret->insts[ret->inst_count - 1], parts, part_count);

        free(parts);
        free(lines[i]);
    }

    free(lines);

    ctx->patterns[ctx->pattern_count - 1] = ret;

    return true;
}

void free_pattern(struct Pattern* pattern)
{
    for (int i = 0; i < pattern->inst_count; i++)
    {
        instruction_free(pattern->insts[i]);
        free(pattern->insts[i]);
    }

    free(pattern->insts);
    free(pattern);
}
