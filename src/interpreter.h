Value* value_clone(const Value* src);

State* setup_state(Context* ctx, int case_index);
void free_state(State* state);

void print_program(Instruction** inst, int count, bool line_nums);

bool compare(Context* ctx, Value* left, Value* right);
void interpret(State* state);
