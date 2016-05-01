bool is_whitespace(char c);

char** read_lines(FILE* file, int* out_line_count);

char** split(char* input, char separator, int* out_count);

char* trim(const char* input);

bool starts_with(const char* haystack, const char* needle);

int exponent(int value, int power);
