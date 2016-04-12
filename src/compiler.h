/*
 *  Parse contents of [filename] into an array of functions [out_functions]
 *
 *  [out_functions] is created with malloc so must be freed by the caller
 *
 *  returns: the number of functions parsed or a negative number on error
 */
int parse_file(char* filename, struct Function** out_functions);

void parse_instruction(struct Instruction* inst, char** parts, int part_count);

char** read_lines(FILE* file, int* out_line_count);

char** split(char* input, char separator, int* out_count);

char* trim(const char* input);
