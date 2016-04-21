void print_status(struct Context* ctx, bool always);
void step(struct Context* ctx, struct State** states, int state_count);
bool add_pattern(struct Context* ctx, const char* filename);
void free_pattern(struct Pattern* pattern);
struct Instruction* instruction_clone(struct Instruction* orig);
void fprint_program(FILE* file, struct Instruction** instructions,
    int instruction_count, char** args, int arg_count);
void solve(const char* solver_file, const char* output_dir, int threads,
        int assembly_start, int assembly_count, bool interactive,
        bool print_solutions, bool find_all_solutions, bool output_generated);
void parse_solver_file(struct Context* ctx, const char* solver_file);
void permute(int result[], int max_depth, int patterns, int target);
