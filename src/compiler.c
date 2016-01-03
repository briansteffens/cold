#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "interpreter.h"
#include "compiler.h"

struct Argument
{
    char* name;
    enum VarType type;
};

struct Function
{
    char* name;

    struct Argument* args;
    int arg_count;

    struct Instruction* insts;
    int inst_count;
};

void free_function(struct Function* func)
{
    free(func->args);
    free(func->insts);
}

bool is_whitespace(char c)
{
    return c == 9 || c == 32;
}

char* trim(const char* input)
{
    int content_start;
    int content_end;

    int input_len = strlen(input);
    
    for (int i = 0; i < input_len; i++)
        if (!is_whitespace(input[i]))
        {
            content_start = i;
            break;
        }

    for (int i = input_len - 1; i > content_start; i--)
        if (!is_whitespace(input[i]))
        {
            content_end = i;
            break;
        }

    int ret_len = content_end - content_start + 1;
    char* ret = malloc((ret_len + 1) * sizeof(char));

    strncpy(ret, input + content_start, ret_len);
    ret[ret_len] = 0;

    return ret;
}

char** read_lines(FILE* file, int* out_line_count)
{
    *out_line_count = 0;
    char** ret = malloc(*out_line_count * sizeof(char*));

    const int buf_len = 255;
    char buf[buf_len];
    int buf_n = 0;

    int c;

    while ((c = fgetc(file)) != EOF)
    {
        if (c == 10 || c == 13)
        {
            bool all_whitespace = true;

            for (int i = 0; i < buf_n; i++)
                if (!is_whitespace(buf[i]))
                {
                    all_whitespace = false;
                    break;
                }

            if (!all_whitespace)
            {
                buf[buf_n] = 0;

                ret = realloc(ret, (*out_line_count + 1) * sizeof(char*));
                ret[*out_line_count] = trim(buf);

                (*out_line_count)++;
            }

            buf_n = 0;
            continue;
        }

        buf[buf_n] = c;
        buf_n++;
    }

    return ret;
}

char** split(char* input, char separator, int* out_count)
{
    *out_count = 0;
    char** ret = malloc(*out_count * sizeof(char*));

    char* current = input;
    char* next;
    char* last = input + strlen(input);

    while (current < last)
    {
        next = strchr(current, separator);

        if (next == NULL)
            next = last;

        int len = next - current;

        if (len == 0)
            continue;

        ret = realloc(ret, (*out_count + 1) * sizeof(char*));
        ret[*out_count] = malloc((len + 1) * sizeof(char));
        strncpy(ret[*out_count], current, len);
        ret[*out_count][len] = 0;

        (*out_count)++;
        current = next + 1;
    }

    return ret;
}

void parse_instruction(struct Instruction* inst, char** parts, int part_count)
{
    inst->type = instruction_type_fromstring(parts[0]);

    switch (inst->type)
    {
    case INST_LET:
        params_allocate(inst, 2);

        inst->params[0]->type = PARAM_LITERAL;

        printf("hi\n");
        break;
    default:
        printf("Instruction type [%d/%s] unrecognized\n", inst->type, parts[0]);
        exit(0);
    }
}

struct Function* parse(char** lines, int line_count, int* out_count)
{
    *out_count = 0;
    struct Function* ret = malloc(*out_count * sizeof(struct Function));
    struct Function* cur = NULL;

    for (int i = 0; i < line_count; i++)
    {
        int part_count;
        char** parts = split(lines[i], ' ', &part_count);

        if (strcmp(parts[0], "def") == 0)
        {
            (*out_count)++;
            ret = realloc(ret, *out_count * sizeof(struct Function));

            cur = &ret[*out_count - 1];

            cur->name = strdup(parts[1]);

            cur->arg_count = 0;
            cur->args = malloc(cur->arg_count * sizeof(struct Argument));

            cur->inst_count = 0;
            cur->insts = malloc(cur->inst_count * sizeof(struct Instruction));
        }
        else if (strcmp(parts[0], "arg") == 0)
        {
            cur->arg_count++;
            cur->args = realloc(cur->args,
                cur->arg_count * sizeof(struct Argument));

            cur->args[cur->arg_count - 1].name = strdup(parts[1]);
            cur->args[cur->arg_count - 1].type = var_type_fromstring(parts[2]);
        }
        else
        {
            cur->inst_count++;
            cur->insts = realloc(cur->insts,
                cur->inst_count * sizeof(struct Instruction));

            parse_instruction(&cur->insts[cur->inst_count - 1], parts,
                              part_count);
        }

        free(parts);
    }

    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("usage: coldc source_file\n");
        return 0;
    }

    FILE* file = fopen(argv[1], "r");

    if (file == 0)
    {
        printf("Failed to open source file: %s\n", argv[1]);
        return 0;
    }

    int line_count;
    char** lines = read_lines(file, &line_count);

    int function_count;
    struct Function* functions = parse(lines, line_count, &function_count);

    for (int i = 0; i < function_count; i++)
    {
        printf("FUNC %d %s\n", &functions[i], functions[i].name);
        free_function(&functions[i]);
    }

    free(functions);

    fclose(file);
    return 0;
}
