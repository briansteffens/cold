typedef struct SolveArgs
{
    const char* solver_file;
    const char* output_dir;
    int threads;
    int combination_start;
    int combination_count;
    bool interactive;
    bool print_solutions;
    bool find_all_solutions;
    bool output_generated;
} SolveArgs;

void solve(SolveArgs* args);

void parse_solver_file(Context* ctx, const char* solver_file);
