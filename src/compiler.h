struct Function** parse_file(const char* filename, int* function_count);

void parse_instruction(struct Instruction* inst, char** parts, int part_count);
