#pragma once

#ifndef NULL
#define NULL 0
#endif

#define U64_MAX 0xFFFFFFFFFFFFFFFF
#define U8_MAX  0xFF

typedef unsigned long long U64;
typedef signed long long   I64;
typedef unsigned char       U8;
typedef signed char         I8;

typedef union
{
	U64 u;
	I64 i;
} INT64;

typedef union
{
	U8 u;
	I8 i;
} INT8;

#define NUM_REGISTERS 13
#define STACK_SIZE 128 // in INT64

typedef struct
{
	INT64 A; // General purpose int 1
	INT64 B; // General purpose int 2
	INT64 C; // General purpose int 3
	INT64 D; // General purpose int 4
	INT64 E; // General purpose int 5
	INT64 F; // General purpose int 6
	INT64 G; // General purpose int 7
	INT64 H; // General purpose int 8
	INT64 R; // General purpose int 9 / int return value
	INT64 S; // Stack pointer
	INT64 Z; // Stack base pointer
	INT64 I; // Instruction pointer
	INT64 L; // Flags
} MVM64_REGISTERS_STRUCT;

typedef union
{
	MVM64_REGISTERS_STRUCT s;
	INT64 a[NUM_REGISTERS];
} MVM64_REGISTERS;

typedef INT64 MVM64_THREAD;

typedef enum
{
	ADD = 0,
	SUB,
	MUL,
	DIV,
	AND,
	OR,
	XOR,
	JMP,
	JZR,
	MOV,
	DREF,
	LADR,
	COMP,
	PUSH,
	POP,
	RET,
	NUM_INSTRUCTIONS
} INSTRUCTION;

#define VALA_FLAG (1<<5)
#define VALB_FLAG (1<<6)
#define SMALL_FLAG (1<<7)
#define SIGN_FLAG_8 (1<<7)
#define SIGN_FLAG_64 (1<<63)

#define INSTRUCTION_BASE(i) i&0xF
#define INSTRUCTION_VALA(i) i&(1<<5)
#define INSTRUCTION_VALB(i) i&(1<<6)
#define INSTRUCTION_SMALL(i) i&(1<<7)

size_t operand_count(U8 command);

MVM64_REGISTERS* create_context();

void free_context(MVM64_REGISTERS* context);

U64 execute(const void* code, MVM64_REGISTERS* context, INT64* return_value);

void push(INT64 value, MVM64_REGISTERS* context);