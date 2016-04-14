#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "compiler.h"

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

void parse_param(struct Param* param, char* src)
{
    if (src[0] == 'i')
    {
        param->type = PARAM_LITERAL;
        value_set_int(param->value, atoi(src + 1));
    }
    else if (src[0] == 'f')
    {
        param->type = PARAM_LITERAL;
        // TODO: known issue that locale setting can mess this up
        value_set_float(param->value, strtod(src + 1, NULL));
    }
    else if (src[0] == '$')
    {
        param->type = PARAM_LABEL;
        value_set_string(param->value, src + 1);
    }
    else if (src[0] == '!')
    {
        param->type = PARAM_PATTERN;

        int pattern_flags = 0;
        int src_len = strlen(src);

        for (int i = 1; i < src_len; i++)
        {
            switch (src[i])
            {
            case 'l':
                pattern_flags |= PTRN_LOCALS;
                break;
            case 'c':
                pattern_flags |= PTRN_CONSTANTS;
                break;
            default:
                printf("Pattern [%s] unrecognized\n", src);
                exit(0);
            }
        }

        value_set_int(param->value, pattern_flags);
    }
    else
    {
        printf("Literal [%s] unrecognized\n", src);
        exit(0);
    }
}

void parse_instruction(struct Instruction* inst, char** parts, int part_count)
{
    inst->type = instruction_type_fromstring(parts[0]);

    params_allocate(inst, part_count - 1);

    for (int i = 0; i < part_count - 1; i++)
    {
        parse_param(inst->params[i], parts[i + 1]);
    }
}

struct Function** parse(char** lines, int line_count, int* out_count)
{
    *out_count = 0;
    struct Function** ret = malloc(0);
    struct Function* cur = NULL;

    for (int i = 0; i < line_count; i++)
    {
        int part_count;
        char** parts = split(lines[i], ' ', &part_count);

        if (strcmp(parts[0], "def") == 0)
        {
            (*out_count)++;
            ret = realloc(ret, *out_count * sizeof(struct Function*));
            cur = ret[*out_count - 1] = malloc(sizeof(struct Function));

            cur->name = strdup(parts[1]);

            cur->arg_count = part_count - 2;
            cur->args = malloc(cur->arg_count * sizeof(char*));

            for (int j = 0; j < cur->arg_count; j++)
            {
                cur->args[j] = strdup(parts[j + 2]);
            }

            cur->inst_count = 0;
            cur->insts = malloc(cur->inst_count * sizeof(struct Instruction));
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

/*
 *  Parse contents of [filename] into an array of functions.
 *
 *  The array is created with malloc so must be freed by the caller.
 */
struct Function** parse_file(const char* filename, int* function_count)
{
    FILE* file = fopen(filename, "r");

    if (file == 0)
    {
        printf("Failed to open source file [%s]\n", filename);
        exit(0);
    }

    int line_count;
    char** lines = read_lines(file, &line_count);

    fclose(file);

    struct Function** ret = parse(lines, line_count, function_count);

    for (int i = 0; i < line_count; i++)
    {
        free(lines[i]);
    }

    free(lines);

    return ret;
}
