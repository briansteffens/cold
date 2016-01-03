struct Value* value_clone(const struct Value* src);

void value_set_string(struct Value* value, char* val);
void value_set_int(struct Value* value, int val);
void value_set_float(struct Value* value, float val);

struct State* setup_state(struct Context* ctx, int case_index);
void free_state(struct State* state);

void params_allocate(struct Instruction* inst, int param_count);

void print_program(struct Instruction** inst, int count, bool line_nums);

bool compare(struct Context* ctx, struct Value* left, struct Value* right);
void interpret(struct State* state);
