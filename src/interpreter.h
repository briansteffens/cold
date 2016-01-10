struct Value* value_clone(const struct Value* src);
void value_free(const struct Value* value);

struct State* setup_state(struct Context* ctx, int case_index);
void free_state(struct State* state);

void print_program(struct Instruction** inst, int count, bool line_nums);

bool compare(struct Context* ctx, struct Value* left, struct Value* right);
void interpret(struct State* state);

void instruction_free(struct Instruction* inst);
