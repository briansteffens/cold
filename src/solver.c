#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "cold.h"
#include "interpreter.h"

struct State* state_fork(struct State* orig)
{
    struct State* ret = malloc(sizeof(struct State));

    ret->local_count = orig->local_count;
    ret->locals_owned = malloc(ret->local_count * sizeof(bool));
    ret->locals = malloc(ret->local_count * sizeof(struct Local*));

    for (int i = 0; i < ret->local_count; i++)
    {
        ret->locals[i] = orig->locals[i];
        ret->locals_owned[i] = false;
    }

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

int count_patterns(struct State* state, struct Param* param)
{
    int ret = 0;

    int flags = *((int*)param->value->data);

    if (flags & PTRN_LOCALS)
        ret += state->local_count;

    printf("%d\n", ret);

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

struct Param** vary_params(struct State* state, struct Param* pattern,
                           int* output_count)
{
    int flags = *((int*)pattern->value->data);

    int count = 0;
    if (flags & PTRN_LOCALS)
        count += state->local_count;

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

    *output_count = current;
    return ret;
}

struct Instruction** vary_instructions(
    struct State* input, struct Instruction* inst, int* instruction_count)
{
    struct Instruction** ret = malloc(1 * sizeof(struct Instruction*));
    *instruction_count = 0;

    for (int i = 0; i < inst->param_count; i++)
    {
        if (inst->params[i]->type != PARAM_PATTERN)
            continue;

        int param_count = 0;
        struct Param** varied = vary_params(input, inst->params[i],
                                            &param_count);
        for (int p = 0; p < param_count; p++)
        {
            struct Instruction* new_inst = instruction_clone(inst);
            new_inst->params[i] = varied[p];

            int recur_count = 0;
            struct Instruction** recurs = vary_instructions(input, new_inst,
                                                            &recur_count);

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

    // TODO: remove? this should never happen but just in case..
    assert(*instruction_count != 1);

    if (*instruction_count == 0)
    {
        ret = realloc(ret, 1 * sizeof(struct Instruction*));
        ret[0] = inst;
        *instruction_count = 1;
    }

    return ret;
}

struct State** vary(struct State* input, int* state_count)
{
    struct Instruction** insts = vary_instructions(
        input, input->instructions[input->inst_ptr], state_count);

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
        states[0] = setup_state(ctx, i);

        states[0]->instruction_count = base->inst_ptr + 1;
        states[0]->instructions = malloc(states[0]->instruction_count *
            sizeof(struct Instruction*));
        states[0]->instructions_owned = malloc(
            states[0]->instruction_count * sizeof(bool));

        for (int j = 0; j < states[0]->instruction_count - 1; j++)
        {
            states[0]->instructions[j] = base->instructions[j];
            states[0]->instructions_owned[j] = false;
        }

        states[0]->instructions[states[0]->instruction_count - 1] = ret_inst;
        states[0]->instructions_owned[states[0]->instruction_count - 1] = false;

        printf("CASE %d\n", i);
        while (states[0]->inst_ptr < states[0]->instruction_count)
            interpret(states[0]);

        bool success = true;
        if (!compare(ctx, states[0]->ret, &ctx->cases[i].expected))
        {
            printf("FAIL: CASE %d\n", i);
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

void step(struct Context* ctx, struct State** states, int state_count)
{
    for (int i = 0; i < state_count; i++)
    {
        if (states[i]->inst_ptr >= states[i]->instruction_count)
        {
            printf("---\n");
            print_program(states[i]->instructions,states[i]->instruction_count,
                          true);
            printf("---\n");

            continue;
        }

        int varied_count = 0;
        struct State** varied = vary(states[i], &varied_count);

        for (int j = 0; j < varied_count; j++)
        {
            interpret(varied[j]);

            struct Local* found = expect(
                ctx, varied[j], &ctx->cases[0].expected);

            if (found)
            {
                check_cases(ctx, varied[j], found);
                if (ctx->solution_inst)
                {
                    printf("*** SOLUTION ***\n");
                    print_program(ctx->solution_inst, ctx->solution_inst_count,
                                  true);
                    break;
                }
            }
        }

        if (!ctx->solution_inst)
            step(ctx, varied, varied_count);

        for (int j = 0; j < varied_count; j++)
            free_state(varied[j]);

        free(varied);

        if (ctx->solution_inst)
            break;
    }
}
