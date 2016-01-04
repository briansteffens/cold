#include "cold.h"

/*
 *  Parse contents of [filename] into an array of functions [out_functions]
 *
 *  [out_functions] is created with malloc so must be freed by the caller
 *
 *  returns: the number of functions parsed or a negative number on error
 */
int parse_file(char* filename, struct Function** out_functions);
