void print_status(Context* ctx, bool always);
void step(Context* ctx, State** states, int state_count);
bool add_pattern(Context* ctx, const char* filename);
void free_pattern(Pattern* pattern);
Instruction* instruction_clone(Instruction* orig);
void fprint_program(FILE* file, Instruction** instructions,
        int instruction_count, char** args, int arg_count);
void solve(const char* solver_file, const char* output_dir, int threads,
        int assembly_start, int assembly_count, bool interactive,
        bool print_solutions, bool find_all_solutions, bool output_generated);
void parse_solver_file(Context* ctx, const char* solver_file);
void permute(int result[], int max_depth, int patterns, int target);
State* setup_state(Context* ctx, int case_index);
