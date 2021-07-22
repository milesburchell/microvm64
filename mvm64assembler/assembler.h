#pragma once

#define VER_MAJ 0
#define VER_MIN "01c"
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
#define NUM_REGISTERS 13
#define I8_MAX 127
#define I8_MIN -128
#define DATA "DATA" // data emplacement command
#define MAX_INSTRUCTION_SIZE 10 // in bytes, 8 bit instruction + 8bit op a + 64bit op b

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
