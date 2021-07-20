#include <stdio.h>
#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include "vm.h"

#define VER_MAJ 0
#define VER_MIN "01a"
#define MAX_LINES 4096 // maximum lines of code
#define MAX_BYTES MAX_LINES // maximum bytes of binary output (most is 1 byte per line for now)
#define STR_BUF_SIZE 128
#define SPACE ' '
#define BYTE unsigned char

char* lines[MAX_LINES] = { 0 };
size_t num_lines = 0;
char* lines_trimmed[MAX_LINES] = { 0 };
size_t num_lines_trimmed = 0;

BYTE code[MAX_BYTES] = { 0 };
size_t code_size = 0;

// Implementation of POSIX C getline for Windows
// Credit: Will Hartung on Stack Overflow
size_t getline(char** lineptr, size_t* n, FILE* stream)
{
    char* bufptr = NULL;
    char* p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL || stream == NULL || n == NULL)
        return -1;

    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);

    if (c == EOF)
    {
        return -1;
    }

    if (bufptr == NULL)
    {
        bufptr = malloc(STR_BUF_SIZE);

        if (bufptr == NULL)
        {
            return -1;
        }
        size = 128;
    }

    p = bufptr;

    while (c != EOF)
    {
        if ((p - bufptr) > (size - 1))
        {
            size = size + STR_BUF_SIZE;
            bufptr = realloc(bufptr, size);

            if (bufptr == NULL)
            {
                return -1;
            }
        }

        *p++ = c;

        if (c == '\n')
        {
            break;
        }

        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

// trims delimiters from string tokens
// note: memory must be freed
// returns NULL if string is effectively empty or allocation fails
char* trimtoken(char* token)
{
    size_t len = strlen(token);

    // copy string to new buffer
    char* newstring = calloc(len + 1, sizeof(char));

    if (newstring == NULL)
        return NULL;

    strcpy_s(newstring, len + 1, token);

    for (size_t s = 0; s < len; s++)
    {
        // trim
        if (newstring[s] == ',' || isspace(newstring[s]))
        {
            newstring[s] = 0;
        }
    }

    // if token was only space, free it and return null
    if (!strlen(newstring))
    {
        free(newstring);
        return NULL;
    }

    return newstring;
}

// trims leading whitespace, trailing whitespace and trailing comments
// prefixed by ; from a string. also shortens runs of space characters to a
// single space
// note: memory must be freed
// returns NULL if string is effectively empty or allocation fails
char* trimline(char* string)
{
    // first trim leading whitespace by modifying pointer to original string
    while (isspace(*string))
    {
        string++;
    }

    size_t len = strlen(string);

    // copy string to new buffer
    char* newstring = calloc(len + 1, sizeof(char));

    if (newstring == NULL)
        return NULL;

    strcpy_s(newstring, len + 1, string);

    // trim trailing whitespace
    while (isspace(newstring[len - 1]))
    {
        newstring[len - 1] = 0;
        len--;
    }

    // truncate runs of spaces to a single space
    for (size_t s = 0; s < len; s++)
    {
        // trim comments
        if (newstring[s] == ';')
        {
            newstring[s] = 0;
            len = s;

            break;
        }

        if (newstring[s] == SPACE)
        {
            size_t run = 1;

            // count run of spaces
            while (s + run < len && newstring[s + run] == SPACE)
            {
                run++;
            }

            // move content after spaces forwards
            if (run > 1)
            {
                memmove(&(newstring[s + 1]), &(newstring[s + run]), len - s - run + 1);
                // null terminate
                newstring[len - run + 2] = 0;
                len -= run;
            }
        }
    }

    if (len)
    {
        // trim subsequent whitespace
        size_t end = len - 1;

        while (isspace(newstring[end]))
        {
            newstring[end] = 0;
            end--;
            len--;
        }
    }

    // if line was devoid of useful tokens, free it and return null
    if (!strlen(newstring))
    {
        free(newstring);
        return NULL;
    }

    return newstring;
}

void assemble(FILE* input, FILE* output)
{
    char* line = NULL;
    size_t linesize = 0, n = 0;

    linesize = getline(&line, &n, input);

    // code cleaning logic
    while (linesize != -1 && num_lines < (MAX_LINES - 1) && line != NULL)
    {
        lines[num_lines] = line;

        char* trimmedline = trimline(line);

        lines_trimmed[num_lines] = trimmedline; // may be NULL

        num_lines++;

        n = 0;
        line = NULL;
        linesize = getline(&line, &n, input);
    }

    // assembler logic
    for (size_t s = 0; s < num_lines; s++)
    {
        if (lines_trimmed[s])
        {
            char* token, * tok_context = NULL;
            token = strtok_s(lines_trimmed[s], " ", &tok_context);

            while (token)
            {
                char* token_trimmed = trimtoken(token);

                if (token_trimmed)
                {
                    // TODO: actually compile stuff
                    printf("Token %s\n", token_trimmed);

                    free(token_trimmed);
                }
                
                token = strtok_s(NULL, " ", &tok_context);
            }
        }
    }

#ifdef _DEBUG
    printf("Raw source code:\n");

    for (size_t s = 0; s < num_lines; s++)
    {
        if (lines[s])
            printf("%4llu: %s", s, lines[s]);
    }

    printf("\nProcessed and cleaned source code:\n");

    for (size_t s = 0; s < num_lines; s++)
    {
        if (lines_trimmed[s])
            printf("%4llu: %s (length %llu)\n", s, lines_trimmed[s], strlen(lines_trimmed[s]));
    }
#endif

    for (size_t s = 0; s < num_lines; s++)
    {
        free(lines[s]);

        if (lines_trimmed[s])
            free(lines_trimmed[s]);
    }
}

int main(int argc, char* argv[])
{
    printf("======================================\n");
    printf("=       MVM64 Assembler v%d.%s       =\n", VER_MAJ, VER_MIN);
    printf("======================================\n");

#ifdef _DEBUG
    printf(" :: %s\n", argv[0]);
#endif

    if (argc < 3)
    {
        printf("Error: Insufficient arguments (%d): expected 2.\n", argc - 1);
        goto INVALID_ARGS;
    }

    FILE *source, *bin;

    if (fopen_s(&source, argv[1], "r"))
    {
        printf("Error: Couldn't open source file %s\n", argv[1]);
        goto INVALID_ARGS;
    }

    if (fopen_s(&bin, argv[2], "w+"))
    {
        printf("Error: Couldn't open output file %s\n", argv[2]);
        fclose(source);
        goto INVALID_ARGS;
    }

    assemble(source, bin);

INVALID_ARGS:
    printf("Usage: mvm64asm [input file name] [output file name]\n");
    return 0;
}
