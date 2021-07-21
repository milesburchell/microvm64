#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <climits>
#include "vm.h"

#define VER_MAJ 0
#define VER_MIN "01a"
#define MAX_TOKENS_LINE 5 // meximum number of tokens in a single line
#define MAX_LINES 4096 // maximum lines of code
#define MAX_BYTES MAX_LINES // maximum bytes of binary output (most is 1 byte per line for now)
#define MAX_SYMBOLS 128 // maximum number of defined symbols
#define MAX_SYMBOL_REFERENCES 32 // maximum number of times a symbol may be referred to
#define SYMBOL_NAME_SIZE 32
#define STR_BUF_SIZE 128
#define SPACE ' '
#define SYM_PREFIX '@' // prefix for referencing a token
#define SYM_SUFFIX ':' // suffix for a label/symbol token
#define BYTE unsigned char
#define NUM_INSTRUCTIONS 16
#define NUM_REGISTERS 13
#define I8_MAX 127
#define I8_MIN -128
#define DATA "DATA" // data emplacement command

typedef struct
{
    U64 offset; // code location of empty reference (always 64-bit)
    int is_jump; // indicates that a signed relative offset should be emplaced
} SYMBOL_REFERENCE;

typedef struct
{
    U64 offset;
    SYMBOL_REFERENCE references[MAX_SYMBOL_REFERENCES];
    size_t reference_count;
    int is_defined;
    size_t line_defined;
    size_t line_first_referenced;
    char name[SYMBOL_NAME_SIZE];
} SYMBOL;

typedef enum
{
    OP_NONE = 0,
    OP_REGISTER,
    OP_SMALL_VAL_U,
    OP_SMALL_VAL_S,
    OP_LARGE_VAL_U,
    OP_LARGE_VAL_S,
    OP_SYMBOL,
    OP_INVALID,
    OP_TYPE_SIZE
} OP_TYPE;

const char* OP_TYPES[OP_TYPE_SIZE] =
{
    "None",
    "Register",
    "Unsigned 8-bit Integer",
    "Signed 8-bit Integer",
    "Unsigned 64-bit Integer",
    "Signed 64-bit Integer",
    "Symbol",
    "Invalid"
};

const char* INSTRUCTIONS[NUM_INSTRUCTIONS] = {
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "AND",
    "OR",
    "XOR",
    "JMP",
    "JZR",
    "MOV",
    "DREF",
    "LADR",
    "COMP",
    "PUSH",
    "POP",
    "RET"
};

const char* REGISTERS[NUM_REGISTERS] = {
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "R",
    "S",
    "Z",
    "I",
    "L"
};

char* lines[MAX_LINES] = { 0 };
size_t num_lines = 0;
char* lines_trimmed[MAX_LINES] = { 0 };
size_t num_lines_trimmed = 0;

BYTE code[MAX_BYTES] = { 0 };
size_t code_size = 0;

SYMBOL symbols[MAX_SYMBOLS];
size_t num_symbols = 0;

void print_file_line(const char* file, size_t line)
{
    // converts internal line numbering (0-index) to normal line numbering (1-index)
    printf("  In file: '%s', line %llu\n", file, line + 1);
}

// finds a matching symbol by name
SYMBOL* get_symbol(const char* name)
{
    if (name == NULL)
        return NULL;

    for (size_t s = 0; s < num_symbols; s++)
    {
        if (!strcmp(symbols[s].name, name))
            return &(symbols[s]);
    }

    return NULL;
}

void write_code_u8(U8 u8)
{
    code[code_size] = u8;
    code_size += sizeof(U8);
}

void write_code_i8(I8 i8)
{
    code[code_size] = i8;
    code_size += sizeof(I8);
}

void write_code_u64(U64 u64)
{
    *(U64*)(&(code[code_size])) = u64;
    code_size += sizeof(U64);
}

void write_code_i64(I64 i64)
{
    *(I64*)(&(code[code_size])) = i64;
    code_size += sizeof(I64);
}

// implementation of POSIX C getline for Windows
// credit: Will Hartung on Stack Overflow
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

// gets the code for a command by name, or returns U8_MAX if it is invalid
// note: token must be uppercase
U8 get_command(const char* token)
{
    if (token == NULL)
        return U8_MAX;

    for (U8 s = 0; s < NUM_INSTRUCTIONS; s++)
    {
        if (!strcmp(INSTRUCTIONS[s], token))
            return s;
    }

    return U8_MAX;
}

// gets the code for a register by name, or returns U8_MAX if it is invalid
// note: token must be uppercase
U8 get_register(const char* token)
{
    if (token == NULL)
        return U8_MAX;

    for (U8 s = 0; s < NUM_REGISTERS; s++)
    {
        if (!strcmp(REGISTERS[s], token))
            return s;
    }

    return U8_MAX;
}

void write_instruction(INSTRUCTION base, OP_TYPE op_a, OP_TYPE op_b)
{
    U8 ins = base;

    if (op_a == OP_SMALL_VAL_U || op_a == OP_SMALL_VAL_S ||
        op_b == OP_SMALL_VAL_U || op_b == OP_SMALL_VAL_S)
    {
        ins |= SMALL_FLAG;
    }

    if (op_a != OP_REGISTER && op_a != OP_NONE)
    {
        ins |= VALA_FLAG;
    }

    if (op_b != OP_REGISTER && op_b != OP_NONE)
    {
        ins |= VALB_FLAG;
    }

    write_code_u8(ins);
}

// op must be one of the specified types (not none or invalid)
// returns 0 on success, -1 on invalid operand, -2 on too many symbols
int write_operand(INSTRUCTION ins, OP_TYPE op, const char* token)
{
    if (token == NULL)
        return -1;

    switch (op)
    {
    case OP_REGISTER:
        write_code_u8(get_register(token));
        return 0;
    case OP_SMALL_VAL_U:
        write_code_u8((U8)strtoul(token, NULL, 0));
        return 0;
    case OP_SMALL_VAL_S:
        write_code_i8((I8)strtol(token, NULL, 0));
        return 0;
    case OP_LARGE_VAL_U:
        write_code_u64(strtoull(token, NULL, 0));
        return 0;
    case OP_LARGE_VAL_S:
        write_code_i64(strtoll(token, NULL, 0));
        return 0;
    case OP_SYMBOL:
        write_code_i64(0);
        token++;

        // add symbol reference
        SYMBOL* sym = get_symbol(token);

        if (sym)
        {
            sym->references[sym->reference_count].offset = code_size;

            if (ins == JMP || ins == JZR)
                sym->references[sym->reference_count].is_jump = 1;

            sym->reference_count++;
        }
        else
        {
            if (num_symbols >= (MAX_SYMBOLS - 1))
                return -2;

            sym = &(symbols[num_symbols]);
            strcpy_s(sym->name, SYMBOL_NAME_SIZE, token);
            sym->line_defined = 0;
            sym->offset = 0;
            sym->is_defined = 0;
            sym->references[sym->reference_count].offset
                = code_size;

            if (ins == JMP || ins == JZR)
                sym->references[sym->reference_count].is_jump = 1;

            sym->reference_count++;

            num_symbols++;
        }

        return 0;
    }

    return -1;
}

// trims delimiters from string tokens
// note: memory must be freed if return value is valid
// returns NULL if string is effectively empty or allocation fails
char* trim_token(char* token)
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

// trims leading whitespace, trailing whitespace and trailing comments prefixed by ;
// from a string. also shortens runs of space characters to a single space
// note: memory must be freed
// returns NULL if string is effectively empty or allocation fails
char* trim_line(char* string)
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

// changes all lowercase letters in a string to uppercase
void to_upper(char* string, size_t len)
{
    if (string == NULL)
        return;

    for (size_t s = 0; s < len; s++)
    {
        if (string[s] >= 'a' && string[s] <= 'z')
        {
            string[s] = toupper(string[s]);
        }
    }
}

// gets number of operands for a command, or return U64_MAX if command is invalid
size_t operand_count(U8 command)
{
    size_t needed;

    switch (command)
    {
    case ADD:
    case SUB:
    case MUL:
    case DIV:
    case AND:
    case OR:
    case XOR:
    case MOV:
    case DREF:
    case LADR:
    case COMP:
        needed = 2;
        break;

    case JMP:
    case JZR:
    case PUSH:
    case POP:
        needed = 1;
        break;

    case RET:
        needed = 0;
        break;

    default:
        return U64_MAX;
    }

    return needed;
}

// gets the type of an operand token
// OP_REGISTER for a register
// OP_SMALL_VAL_U for an 8-bit unsigned value
// OP_SMALL_VAL_S for an 8-bit value
// OP_LARGE_VAL_U for a 64-bit unsigned value
// OP_LARGE_VAL_S for a 64-bit signed value
// OP_SYMBOL for a symbol reference
// OP_INVALID if the operand is invalid
OP_TYPE operand_type(const char* operand)
{
    if (operand == NULL)
        return OP_INVALID;

    if (operand[0] == SYM_PREFIX)
        return OP_SYMBOL;

    if (get_register(operand) != U8_MAX)
        return OP_REGISTER;

    if (operand[0] == '0')
    {
        if (strlen(operand) > 2 && operand[1] == 'x' && isxdigit(operand[2]))
        {
            U64 ull = strtoull(operand, NULL, 0);

                if (ull > U8_MAX)
                    return OP_LARGE_VAL_U;
                else
                    return OP_SMALL_VAL_U;
        }
    }

    if (operand[0] == '-')
    {
        if (strlen(operand) > 1 && isdigit(operand[1]))
        {
            I64 ill = strtoll(operand, NULL, 0);

            if (ill > I8_MAX || ill < I8_MIN)
                return OP_LARGE_VAL_S;
            else
                return OP_SMALL_VAL_S;
        }
        else
        {
            return OP_INVALID;
        }
    }

    if (isdigit(operand[0]))
    {
        I64 ill = strtoll(operand, NULL, 0);

        if (ill == LONG_MAX)
        {
            U64 ull = strtoull(operand, NULL, 0);

            if (ull == ULONG_MAX)
                return OP_INVALID;
            else
                return OP_LARGE_VAL_U;
        }
        else if (ill == LONG_MIN)
            return OP_INVALID;
        else if (ill > I8_MAX)
            return OP_LARGE_VAL_S;
        else
            return OP_SMALL_VAL_S;
    }

    return OP_INVALID;
}

// returns 0 on success, nonzero on failure
int parse_line(char** tokens, size_t num_tokens, const char* input_filename, 
    size_t line_num)
{
    if (tokens == NULL || num_tokens == 0)
        return -1;

    // first token must be a command or a symbol
    if (tokens[0] == NULL)
        return -1;

    size_t len = strlen(tokens[0]);

    // force uppercase
    to_upper(tokens[0], len);

    // check if token deines data
    if (!strcmp(tokens[0], DATA))
    {
        if (num_tokens != 2)
        {
            printf("Error: Too many tokens for %s, expecting a single value\n",
                tokens[0]);
            print_file_line(input_filename, line_num);
            return -7;
        }

        OP_TYPE op_type = operand_type(tokens[1]);

        if (op_type == OP_NONE || op_type == OP_INVALID || op_type == OP_SYMBOL)
        {
            printf("Error: Invalid operand type for %s (%s)\n",
                tokens[0], OP_TYPES[op_type]);
            print_file_line(input_filename, line_num);
            return -7;
        }

        write_operand(0, op_type, tokens[1]);
    }
    // check if token defines a symbol (label)
    else if (tokens[0][len - 1] == SYM_SUFFIX)
    {
        // null-terminate symbol name, removing suffix
        tokens[0][len - 1] = 0;

        // check if symbol has already been defined
        SYMBOL* sym = get_symbol(tokens[0]);

        if (sym)
        {
            if (sym->is_defined)
            {
                printf("Error: Symbol %s was already defined at line %llu\n", 
                    tokens[0], sym->line_defined);
                print_file_line(input_filename, line_num);
                return -1;
            }
            else
            {
                // define symbol
                symbols[num_symbols].line_defined = line_num;
                symbols[num_symbols].offset = code_size;
            }
        }
        else // create symbol
        {
            if (num_symbols >= (MAX_SYMBOLS - 1))
            {
                printf("Error: Exceeded maximum number of symbols (%u)\n", MAX_SYMBOLS);
                print_file_line(input_filename, line_num);
                return -2;
            }

#ifdef _DEBUG
            printf("    DEBUG: Created symbol %s at code offset %llu\n", tokens[0], code_size);
#endif

            strcpy_s(symbols[num_symbols].name, SYMBOL_NAME_SIZE, tokens[0]);
            symbols[num_symbols].line_defined = line_num;
            symbols[num_symbols].offset = code_size;
            symbols[num_symbols].is_defined = 1;

            num_symbols++;
        }
    }
    else // otherwise try to find a matching command
    {
        U8 command = get_command(tokens[0]);

        if (command == U8_MAX)
        {
            printf("Error: %s is not a valid command or symbol\n", tokens[0]);
            print_file_line(input_filename, line_num);
            return -2;
        }

#ifdef _DEBUG
        printf("    DEBUG: Interpreted command %s as %u\n", tokens[0], command);
#endif

        size_t ops = operand_count(command);

        if (ops != (num_tokens - 1))
        {
            printf("Error: Expected %llu operands for command %s, got %llu\n",
                ops, tokens[0], (num_tokens - 1));
            print_file_line(input_filename, line_num);
            return -4;
        }

        OP_TYPE op_a_type = OP_NONE, op_b_type = OP_NONE;

        if (num_tokens > 1)
        {
            op_a_type = operand_type(tokens[1]);

            if (num_tokens > 2)
            {
                op_b_type = operand_type(tokens[2]);
            }
        }

        if (ops)
        {
            if (op_a_type == OP_NONE || op_a_type == OP_INVALID)
            {
                printf("Error: %s expects %llu operands but A is invalid (%s)\n",
                    tokens[0], ops, tokens[1]);
                print_file_line(input_filename, line_num);
                return -5;
            }
        }

        if (ops == 2)
        {
            if (op_b_type == OP_NONE || op_b_type == OP_INVALID)
            {
                printf("Error: %s expects 2 operands but B is invalid (%s)\n",
                    tokens[0], tokens[1]);
                print_file_line(input_filename, line_num);
                return -5;
            }
        }

#ifdef _DEBUG
        printf("    DEBUG: Operand A: %s, operand B: %s\n", 
            OP_TYPES[op_a_type], OP_TYPES[op_b_type]);
#endif

        switch (command)
        {
        // class: arithmetic commands with op A register, op B register or value
        case ADD:
        case SUB:
        case MUL:
        case DIV:
        case AND:
        case OR:
        case XOR:
        case MOV:
            if (op_a_type != OP_REGISTER)
            {
                printf("Error: %s expects Register as operand A, not %s\n", 
                    tokens[0], OP_TYPES[op_a_type]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            if (op_b_type == OP_SYMBOL)
            {
                printf("Error: Symbol operand for %s is invalid\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            write_instruction(command, op_a_type, op_b_type);

            if (write_operand(command, op_a_type, tokens[1]) ||
                write_operand(command, op_b_type, tokens[2]))
            {
                printf("Error: Invalid operands for %s (generic)\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }
            
            return 0;

        // class: jump commands with op A register, symbol or value
        case JMP:
        case JZR:
            write_instruction(command, op_a_type, op_b_type);

            if (write_operand(command, op_a_type, tokens[1]))
            {
                printf("Error: Invalid operand/s for %s (generic)\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }
            return 0;

        // dref/ladr take register as A, and either register or LARGE VALUE(!) as B
        case DREF:
        case LADR:
            if (op_a_type != OP_REGISTER)
            {
                printf("Error: %s expects Register as operand A, not %s\n",
                    tokens[0], OP_TYPES[op_a_type]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            if (op_b_type == OP_SYMBOL || op_b_type == OP_SMALL_VAL_S 
                || op_b_type == OP_SMALL_VAL_U)
            {
                printf("Error: Symbol operand for %s is invalid\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            write_instruction(command, op_a_type, op_b_type);

            if (write_operand(command, op_a_type, tokens[1]) ||
                write_operand(command, op_b_type, tokens[2]))
            {
                printf("Error: Invalid operands for %s (generic)\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }
            
            return 0;

        // comp will only take 2 registers
        case COMP:
            if (op_a_type != OP_REGISTER)
            {
                printf("Error: %s expects Register as operand A, not %s\n",
                    tokens[0], OP_TYPES[op_a_type]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            if (op_b_type != OP_REGISTER)
            {
                printf("Error: %s expects Register as operand B, not %s\n",
                    tokens[0], OP_TYPES[op_b_type]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            write_instruction(command, op_a_type, op_b_type);

            if (write_operand(command, op_a_type, tokens[1]) ||
                write_operand(command, op_b_type, tokens[2]))
            {
                printf("Error: Invalid operands for %s (generic)\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            return 0;

        // push will take a register or value as A
        case PUSH:
            if (op_a_type == OP_SYMBOL)
            {
                printf("Error: %s expects Register or Value as operand A, not %s\n",
                    tokens[0], OP_TYPES[op_a_type]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            write_instruction(command, op_a_type, op_b_type);

            if (write_operand(command, op_a_type, tokens[1]))
            {
                printf("Error: Invalid operands for %s (generic)\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            return 0;

        // pop will only take a single register
        case POP:
            if (op_a_type != OP_REGISTER)
            {
                printf("Error: %s expects Register as operand A, not %s\n",
                    tokens[0], OP_TYPES[op_a_type]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            write_instruction(command, op_a_type, op_b_type);

            if (write_operand(command, op_a_type, tokens[1]))
            {
                printf("Error: Invalid operands for %s (generic)\n", tokens[0]);
                print_file_line(input_filename, line_num);
                return -6;
            }

            return 0;

        // ret has no args
        case RET:
            write_code_u8((U8)RET);
            return 0;

        default:
            printf("Error: %s is an unknown command\n", tokens[0]);
            print_file_line(input_filename, line_num);
            return -3;
        }
    }

    return 0;
}

int resolve_symbols(const char* input_filename)
{
    for (size_t s = 0; s < num_symbols && s < MAX_SYMBOLS; s++)
    {
        if (!symbols[s].is_defined)
        {
            printf("Error: Unresolved symbol %s\n", symbols[s].name);
            return -1;
        }

        if (symbols[s].reference_count == 0)
        {
            printf("Warning: Unreferenced symbol %s\n", symbols[s].name);
            print_file_line(input_filename, symbols[s].line_defined);
        }

        for (size_t t = 0; t < symbols[s].reference_count
            && t < MAX_SYMBOL_REFERENCES; t++)
        {
            if (symbols[s].references[t].is_jump)
            {
                // get relative offset of symbol location
                I64 offset = (I64)symbols[s].offset - 
                    (I64)symbols[s].references[t].offset;

                *(I64*)(&(code[symbols[s].references[t].offset])) = offset;
            }
            else
            {
                // copy U64 from symbol location to reference location
                *(U64*)(&(code[symbols[s].references[t].offset])) =
                    *(U64*)(&(code[symbols[s].offset]));
            }
        }
    }

    return 0;
}

void assemble(FILE* input, FILE* output, const char* input_filename)
{
    char* line = NULL;
    size_t linesize = 0, n = 0;

    // initialise symbol list
    for (size_t s = 0; s < MAX_SYMBOLS; s++)
    {
        symbols[s].name[0] = 0;
        symbols[s].offset = 0;
        symbols[s].reference_count = 0;
        symbols[s].line_defined = 0;
        symbols[s].is_defined = 0;

        for (size_t t = 0; t < MAX_SYMBOL_REFERENCES; t++)
            symbols[s].references[t].is_jump = 0;
    }

    linesize = getline(&line, &n, input);

    // code cleaning logic
    while (linesize != -1 && num_lines < (MAX_LINES - 1) && line != NULL)
    {
        lines[num_lines] = line;

        char* trimmedline = trim_line(line);

        lines_trimmed[num_lines] = trimmedline; // may be NULL

        num_lines++;

        n = 0;
        line = NULL;
        linesize = getline(&line, &n, input);
    }

#ifdef _DEBUG
    printf("    DEBUG: Raw source code:\n");

    for (size_t s = 0; s < num_lines; s++)
    {
        if (lines[s])
            printf("    %4llu: %s", s, lines[s]);
    }

    printf("\n    DEBUG: Processed and cleaned source code:\n");

    for (size_t s = 0; s < num_lines; s++)
    {
        if (lines_trimmed[s])
            printf("    %4llu: %s (length %llu)\n", s, lines_trimmed[s], 
                strlen(lines_trimmed[s]));
    }
#endif

    // assembler logic
    for (size_t s = 0; s < num_lines; s++)
    {
        if (lines_trimmed[s])
        {
            char* token, * tok_context = NULL;
            token = strtok_s(lines_trimmed[s], " ", &tok_context);

            char* tokens[MAX_TOKENS_LINE];
            size_t num_tokens = 0;

            while (token)
            {
                char* token_trimmed = trim_token(token);

                if (token_trimmed)
                {
                    // add to array
                    tokens[num_tokens] = token_trimmed;
                    num_tokens++;

#ifdef _DEBUG
                    printf("    DEBUG: Token %s\n", token_trimmed);
#endif
                }

                if (num_tokens == MAX_TOKENS_LINE)
                {
                    printf("Error: Too many tokens in a single line (max %d)\n", MAX_TOKENS_LINE);
                    print_file_line(input_filename, s);
                    goto CLEANUP;
                }
                
                token = strtok_s(NULL, " ", &tok_context);
            }

            // parse line
            if (num_tokens)
            {
                if (parse_line(tokens, num_tokens, input_filename, s))
                {
                    for (size_t s = 0; s < num_tokens; s++)
                    {
                        if (tokens[s])
                            free(tokens[s]);
                    }

                    goto CLEANUP;
                }
            }

            for (size_t s = 0; s < num_tokens; s++)
            {
                if (tokens[s])
                    free(tokens[s]);
            }
        }
    }

    if (resolve_symbols(input_filename))
        goto CLEANUP;

    printf("\nAssembly complete, writing binary...\n");

    size_t bytes_written = fwrite(code, sizeof(BYTE), code_size, output);

    if (bytes_written != code_size)
        printf("Error: Couldn't write to output file\n");
    else
        printf("%llu bytes written.\n", bytes_written);

CLEANUP:
    fclose(input);
    fclose(output);

    for (size_t s = 0; s < num_lines; s++)
    {
        free(lines[s]);

        if (lines_trimmed[s])
            free(lines_trimmed[s]);
    }

    printf("Exiting...\n");
}

int main(int argc, char* argv[])
{
    printf("======================================\n");
    printf("======= MVM64 Assembler v%d.%s =======\n", VER_MAJ, VER_MIN);
    printf("======================================\n");
    printf("         Miles Burchell, 2021\n\n");

#ifdef _DEBUG
    printf("    DEBUG: image %s\n", argv[0]);
#endif

    if (argc < 3)
    {
        printf("Error: Insufficient arguments (%d): expected 2\n", argc - 1);
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

    printf("Assembling source file %s, output to binary %s\n\n", argv[1], argv[2]);

    assemble(source, bin, argv[1]);

    return 0;

INVALID_ARGS:
    printf("Usage: mvm64asm [input file name] [output file name]\n");
    return -1;
}
