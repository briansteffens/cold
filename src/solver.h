void print_status(struct Context* ctx, bool always);
void step(struct Context* ctx, struct State** states, int state_count);
bool add_pattern(struct Context* ctx, const char* filename);
void free_pattern(struct Pattern* pattern);
