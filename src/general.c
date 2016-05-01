#include <string.h>

#include "cold.h"
#include "general.h"

bool starts_with(const char* haystack, const char* needle)
{
    return strncmp(haystack, needle, strlen(needle)) == 0;
}

int exponent(int value, int power)
{
    int ret = value;

    for (int i = 1; i < power; i++)
    {
        ret *= value;
    }

    return ret;
}

bool is_whitespace(char c)
{
    return c == 9 || c == 32;
}

char* trim(const char* input)
{
    int content_start = 0;
    int content_end = 0;

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
