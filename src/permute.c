#include <stdlib.h>

#include "cold.h"
#include "permute.h"

// Information about a permutation of pattern parameters
struct PermuteParamsInfo
{
    int param_count;
    struct Param** pattern_params;
    int* permutation_counts;
    int all_count;
};

// Count all possible substitutions for a param pattern
int count_param_substitutions(struct Context* ctx, struct State* state,
    struct Param* pattern)
{
    int flags = *((int*)pattern->value->data);

    int count = 0;

    if (flags & PTRN_LOCALS)
    {
        count += state->local_count;
    }

    if (flags & PTRN_CONSTANTS)
    {
        count += ctx->constant_count;
    }

    return count;
}

// Find and clone the permutation indicated by the index out of the possible
// variations of [param]. [permutation_index] should be in a range between 0
// and the result of count_param_substitutions() for the same [param].
//
// The return value must be freed by the caller.
struct Param* permute_param(struct Context* ctx, struct State* state,
    struct Param* param, int permutation_index)
{
    struct Param* ret = malloc(sizeof(struct Param));
    int flags = *((int*)param->value->data);

    if (flags & PTRN_LOCALS)
    {
        if (permutation_index < state->local_count)
        {
            ret->value = malloc(sizeof(struct Value));
            ret->type = PARAM_LABEL;
            value_set_string(ret->value,
                    state->locals[permutation_index]->name);
            return ret;
        }

        permutation_index -= state->local_count;
    }

    if (flags & PTRN_CONSTANTS)
    {
        if (permutation_index < ctx->constant_count)
        {
            ret->value = value_clone(&ctx->constants[permutation_index]);
            ret->type = PARAM_LITERAL;
            return ret;
        }

        permutation_index -= ctx->constant_count;
    }

    printf("Permutation index %d invalid\n", permutation_index);
    exit(EXIT_FAILURE);
}

// Collect information about a param permutation operation.
//
// info->pattern_params and info->permutation_counts must be freed by the
// caller.
void permute_params_info(struct Context* ctx, struct State* state,
        struct Instruction* instruction, struct PermuteParamsInfo* info)
{
    info->param_count = 0;
    info->pattern_params = malloc(0);
    info->permutation_counts = malloc(0);
    info->all_count = 1;

    for (int i = 0; i < instruction->param_count; i++)
    {
        if (instruction->params[i]->type != PARAM_PATTERN)
        {
            continue;
        }

        info->param_count++;

        // Add to the list of pattern params being permuted
        info->pattern_params = realloc(info->pattern_params,
                info->param_count * sizeof(struct Param*));
        info->pattern_params[info->param_count - 1] = instruction->params[i];

        // Store the number of permutations possible for this param
        info->permutation_counts = realloc(info->permutation_counts,
                info->param_count * sizeof(int));
        info->permutation_counts[info->param_count - 1] =
            count_param_substitutions(ctx, state,
                info->pattern_params[info->param_count - 1]);

        // Compute the total number of permutations for the instruction
        info->all_count *= info->permutation_counts[info->param_count - 1];
    }
}

// Generate all possible permutations of the params described in [info].
//
// [all_ptr] will store the output and should be a 2-dimensional array of size
// [info->all_count][info->param_count].
void permute_params_all(struct PermuteParamsInfo* info, int* all_ptr)
{
    // 2d array from pointer
    int(* all)[info->param_count] = (int(*)[info->param_count])all_ptr;

    int repeat = 1;

    // The return value is a 2-dimensional array. Fill it vertical-first.
    for (int depth = 0; depth < info->param_count; depth++)
    {
        int p = 0;

        // Fill the whole return value vertically before moving on to the next
        // depth level.
        while (p < info->all_count)
        {
            for (int i = 0; i < info->permutation_counts[depth]; i++)
            {
                // Repeat each permutation at this depth based on permutations
                // from previous depths
                for (int r = 0; r < repeat; r++)
                {
                    // Set the param index for this permutation at this depth
                    all[p][depth] = i;
                    p++;
                }
            }
        }

        // The number of times each value should be repeated is the number of
        // permutations possible for the previous depth levels.
        repeat *= info->permutation_counts[depth];
    }
}

// Filter equivalent instructions (x + y vs y + x) by masking out instructions
// detected to be functionally equivalent to others within the permutation set.
//
// [all_ptr] should be a 2d array of size [info->all_count][info->param_count].
// [out_mask] should be an array of size [info->all_count].
void unique_mask(struct PermuteParamsInfo* info,
        enum InstructionType instruction_type, int* all_ptr, bool* out_mask)
{
    // 2d array from pointer
    int(* all)[info->param_count] = (int(*)[info->param_count])all_ptr;

    // Check if the order of the instruction's parameters can be swapped and
    // have an equivalent meaning.
    bool commutative = info->param_count == 3 &&
            (instruction_type == INST_ADD ||
             instruction_type == INST_MUL);

    // Store accepted permutations for deduplication purposes
    int* found[info->all_count];
    int found_count = 0;

    for (int p = 0; p < info->all_count; p++)
    {
        if (!commutative)
        {
            out_mask[p] = true;
            continue;
        }

        bool unique = true;

        for (int f = 0; f < found_count; f++)
        {
            // Commutative check
            if (found[f][0] == all[p][0] &&
                found[f][1] == all[p][2] &&
                found[f][2] == all[p][1])
            {
                unique = false;
                break;
            }
        }

        out_mask[p] = unique;

        if (!unique)
        {
            continue;
        }

        found[found_count] = all[p];
        found_count++;
    }
}

// Compute all permutations of pattern params in a given [instruction].
//
// The return value must be freed by the caller.
struct Param*** permute_params(struct Context* ctx, struct State* state,
    struct Instruction* instruction, int* out_permutation_count)
{
    // Get some general information about the permutation operation
    struct PermuteParamsInfo info;
    permute_params_info(ctx, state, instruction, &info);

    // Calculate all possible permutations
    int all[info.all_count][info.param_count];
    permute_params_all(&info, (int*)all);

    // Filter out useless permutations
    bool mask[info.all_count];
    unique_mask(&info, instruction->type, (int*)all, mask);

    // Count permutations that passed the unique mask filtering
    int ret_count = 0;

    for (int p = 0; p < info.all_count; p++)
    {
        if (mask[p])
        {
            ret_count++;
        }
    }

    // Generate output
    struct Param*** ret = malloc(ret_count * sizeof(struct Param**));

    int ret_i = 0;

    for (int all_i = 0; all_i < info.all_count; all_i++)
    {
        // Skip if not in the unique mask
        if (!mask[all_i])
        {
            continue;
        }

        ret[ret_i] = malloc(info.param_count * sizeof(struct Param*));

        for (int p = 0; p < info.param_count; p++)
        {
            ret[ret_i][p] = permute_param(ctx, state, info.pattern_params[p],
                    all[all_i][p]);
        }

        ret_i++;
    }

    // Cleanup
    free(info.pattern_params);
    free(info.permutation_counts);

    *out_permutation_count = ret_count;

    return ret;
}

// Calculate all possible permutations of the given [instruction].
//
// The return value must be freed by the caller.
struct Instruction** permute_instruction(struct Context* ctx,
    struct State* state, struct Instruction* instruction,
    int* instruction_count)
{
    // Get all param permutations
    int permutation_count = 0;
    struct Param*** permutations = permute_params(ctx, state, instruction,
            &permutation_count);

    struct Instruction** ret = malloc(
            permutation_count * sizeof(struct Instruction*));

    // For each param permutation, clone the instruction and replace the
    // pattern params with the permutation values
    for (int i = 0; i < permutation_count; i++)
    {
        ret[i] = instruction_clone(instruction);

        int r = 0;

        for (int p = 0; p < ret[i]->param_count; p++)
        {
            if (ret[i]->params[p]->type != PARAM_PATTERN)
            {
                continue;
            }

            // Free the pattern being replaced
            value_free(ret[i]->params[p]->value);
            free(ret[i]->params[p]);

            // Replace the pattern param
            ret[i]->params[p] = permutations[i][r];
            r++;
        }
    }

    for (int i = 0; i < permutation_count; i++)
    {
        free(permutations[i]);
    }

    free(permutations);

    *instruction_count = permutation_count;
    return ret;
}
