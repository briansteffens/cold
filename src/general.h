bool starts_with(const char* haystack, const char* needle);

int exponent(int base, int power);

// Trim whitespace from <input>.
//
// The return value must be freed by the caller.
char* trim(const char* input);

// Read the contents of a file separated by newlines.
//
// Returns a 2-dimensional array of strings and both dimensions must be freed
// by the caller.
//
// <out_line_count> will be the size of the first dimension of the return
// value.
char** read_lines(FILE* file, int* out_line_count);

// Split <input> by <separator>.
//
// Returns a 2-dimensional array of strings and both dimensions must be freed
// by the caller.
//
// <out_count> will be the size of the first dimension of the return
// value.
char** split(char* input, char separator, int* out_count);
