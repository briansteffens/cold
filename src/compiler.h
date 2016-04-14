struct Function** parse_file(const char* filename, int* function_count);

void parse_instruction(struct Instruction* inst, char** parts, int part_count);

char** read_lines(FILE* file, int* out_line_count);

char** split(char* input, char separator, int* out_count);

char* trim(const char* input);
