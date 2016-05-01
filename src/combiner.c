#include "cold.h"
#include "general.h"

// Find a single permutation of source code patterns identified by [target]
// where [target] is the index of the permutation between 0 and
// [patterns] ^ [maxdepth].
void permute_patterns(int result[], int max_depth, int patterns, int target)
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

int count_combinations(Context* ctx)
{
    return exponent(ctx->pattern_count, ctx->depth);
}

void combine_patterns(Context* ctx, int patterns[], Combination* combination)
{
    combination->instruction_count = 0;
    combination->instructions = malloc(0);

    for (int p = 0; p < ctx->depth; p++)
    {
        Pattern* pattern = ctx->patterns[patterns[p]];

        for (int i = 0; i < pattern->inst_count; i++)
        {
            if (pattern->insts[i]->type == INST_NEXT)
            {
                continue;
            }

            combination->instructions = realloc(combination->instructions,
                    (combination->instruction_count + 1) *
                    sizeof(Instruction*));

            combination->instructions[combination->instruction_count] =
                    instruction_clone(pattern->insts[i]);

            combination->instruction_count++;
        }
    }
}

void combine(Context* ctx, int index, Combination* combination)
{
    int patterns[ctx->depth];
    permute_patterns(patterns, ctx->depth, ctx->pattern_count, index);

    combine_patterns(ctx, patterns, combination);
}

void load_combination(State* state, Combination* combination)
{
    state->instruction_count = combination->instruction_count;
    state->instructions = malloc(
            state->instruction_count * sizeof(Instruction*));
    state->instructions_owned = malloc(
            state->instruction_count * sizeof(bool));

    for (int i = 0; i < combination->instruction_count; i++)
    {
        state->instructions[i] =
            instruction_clone(combination->instructions[i]);

        state->instructions_owned[i] = true;
    }

    state->inst_ptr = 0;
}
