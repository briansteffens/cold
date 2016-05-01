// Return the total number of possible combinations of patterns in the context.
int count_combinations(Context* ctx);

// Produce a combination of patterns specified by [index] where [index] ranges
// from 0 to the result of count_combinations(ctx).
void combine(Context* ctx, int index, Combination* combination);

// Load a combination into a state. Clones the instructions in the combination
// so the state will own its copies.
void load_combination(State* state, Combination* combination);
