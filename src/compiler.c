#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cold.h"
#include "general.h"
#include "compiler.h"

void parse_param(Param* param, char* src)
{
    const char prefix = src[0];

    if (prefix == '$')
    {
        param->type = PARAM_LABEL;
        value_set_string(param->value, src + 1);
    }
    else if (prefix == '!')
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
        param->type = PARAM_LITERAL;
        value_set_from_string(param->value, src);
    }
}

void parse_instruction(Instruction* inst, char** parts, int part_count)
{
    inst->type = instruction_type_fromstring(parts[0]);

    params_allocate(inst, part_count - 1);

    for (int i = 0; i < part_count - 1; i++)
    {
        parse_param(inst->params[i], parts[i + 1]);
    }
}

Function** parse(char** lines, int line_count, int* out_count)
{
    *out_count = 0;
    Function** ret = malloc(0);
    Function* cur = NULL;

    for (int i = 0; i < line_count; i++)
    {
        int part_count;
        char** parts = split(lines[i], ' ', &part_count);

        if (strcmp(parts[0], "def") == 0)
        {
            (*out_count)++;
            ret = realloc(ret, *out_count * sizeof(Function*));
            cur = ret[*out_count - 1] = malloc(sizeof(Function));

            cur->name = strdup(parts[1]);

            cur->arg_count = part_count - 2;
            cur->args = malloc(cur->arg_count * sizeof(char*));

            for (int j = 0; j < cur->arg_count; j++)
            {
                cur->args[j] = strdup(parts[j + 2]);
            }

            cur->inst_count = 0;
            cur->insts = malloc(cur->inst_count * sizeof(Instruction));
        }
        else
        {
            cur->inst_count++;
            cur->insts = realloc(cur->insts,
                cur->inst_count * sizeof(Instruction));

            parse_instruction(&cur->insts[cur->inst_count - 1], parts,
                              part_count);
        }

        for (int p = 0; p < part_count; p++)
        {
            free(parts[p]);
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
Function** parse_file(const char* filename, int* function_count)
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

    Function** ret = parse(lines, line_count, function_count);

    for (int i = 0; i < line_count; i++)
    {
        free(lines[i]);
    }

    free(lines);

    return ret;
}
