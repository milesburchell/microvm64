#include "vm.h"
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#include <stdio.h> // debug output

// global variables for exec_instruction
U8 ins;
INT64* OP_A;
INT64 OP_A_LOCAL;
INT64* OP_B;
INT64 OP_B_LOCAL;
U8 OPA_SIZE;
U8 OPB_SIZE;

__inline INT64* get_register(const INT8 code, MVM64_REGISTERS* context)
{
	assert(code.u < NUM_REGISTERS);

	return &(context->a[code.u]);
}

// returns number of bytes executed, or U64_MAX if execution has ended
// note - no pointer safety check
__inline U64 exec_instruction(MVM64_REGISTERS* context)
{
	U64 bytes_executed;
	ins = *(U8*)(context->s.I.u);

	printf("exec_instruction: starting from 0x%llx\n", context->s.I.u);

	if ((INSTRUCTION_BASE(ins)) == RET)
	{
		printf(" - Returning...");
		return U64_MAX;
	}

	if (INSTRUCTION_VALA(ins))
	{
		printf(" - Operand A is a value, pulling value from 0x%llx\n", context->s.I.u + sizeof(ins));
		
		if (INSTRUCTION_SMALL(ins))
		{
			OP_A_LOCAL.u = *(U8*)(context->s.I.u + sizeof(ins));
			OP_A = &OP_A_LOCAL;

			OPA_SIZE = sizeof(U8);

			printf(" - Operand A: 0x%llx (8-bit)\n", OP_A->u);
		}
		else
		{
			OP_A = (INT64*)(context->s.I.u + sizeof(ins));

			OPA_SIZE = sizeof(INT64);

			printf(" - Operand A: 0x%llx (64-bit)\n", OP_A->u);
		}
	}
	else
	{
		printf(" - Operand A: Register %u\n", *(U8*)((context->s.I.u) + sizeof(ins)));

		OP_A = get_register(*(INT8*)(context->s.I.u + sizeof(ins)), context);

		OPA_SIZE = sizeof(INT8);
	}

	if (INSTRUCTION_VALB(ins))
	{
		printf(" - Operand B is a value, pulling value from 0x%llx\n", context->s.I.u + sizeof(ins) + OPA_SIZE);
		
		if (INSTRUCTION_SMALL(ins))
		{
			OP_B_LOCAL.u = *(U8*)(context->s.I.u + sizeof(ins) + OPA_SIZE);
			OP_B = &OP_B_LOCAL;

			bytes_executed = OPA_SIZE + sizeof(INT8) + sizeof(ins);

			printf(" - Operand B: 0x%llx (8-bit)\n", OP_B->u);
		}
		else
		{
			OP_B = (INT64*)(context->s.I.u + sizeof(ins) + OPA_SIZE);

			bytes_executed = OPA_SIZE + sizeof(INT64) + sizeof(ins);

			printf(" - Operand B: 0x%llx (64-bit)\n", OP_B->u);
		}
	}
	else
	{
		printf(" - Operand B: Register %u\n", *(U8*)(context->s.I.u + sizeof(ins) + OPA_SIZE));

		OP_B = get_register(*(INT8*)(context->s.I.u + sizeof(ins) + OPA_SIZE), context);

		bytes_executed = OPA_SIZE + sizeof(INT8) + sizeof(ins);
	}

	printf(" - Instruction: %u\n", INSTRUCTION_BASE(ins));

	switch (INSTRUCTION_BASE(ins))
	{
	case ADD:
		OP_A->i += OP_B->i;
		break;

	case SUB:
		OP_A->i -= OP_B->i;
		break;

	case MUL:
		OP_A->i *= OP_B->i;
		break;

	case DIV:
		OP_A->i /= OP_B->i;
		break;

	case AND:
		OP_A->u &= OP_B->u;
		break;

	case OR:
		OP_A->u |= OP_B->u;
		break;

	case XOR:
		OP_A->u ^= OP_B->u;
		break;

	case MOV:
		OP_A->u = OP_B->u;
		break;

	case JMP:
		context->s.I = *OP_A;
		return bytes_executed;

	case JZR:
		if (!context->s.R.u)
		{
			context->s.I = *OP_A;
			return bytes_executed;
		}
		break;

	case DREF:
		*OP_A = *(INT64*)(OP_B->u);
		break;

	case LADR:
		OP_A->u = (U64)OP_B;
		break;

	case COMP:
		OP_A->u = ~(OP_B->u);

	case PUSH:
		// check for stack overflow
		assert(((context->s.S.u - context->s.Z.u) / sizeof(INT64)) < STACK_SIZE);

		context->s.S.u += sizeof(INT64);
		*(INT64*)context->s.S.u = *OP_A;
		break;

	case POP:
		// check that there's something on the stack
		assert(context->s.S.u - context->s.Z.u);

		*OP_A = *(INT64*)context->s.S.u;
		context->s.S.u -= sizeof(INT64);
		break;

	default:
		return 0;
	}

	context->s.I.u += bytes_executed;

	return bytes_executed;
}

U64 execute(const void* code, MVM64_REGISTERS* context, INT64* return_value)
{
	U64 bytes_executed = 0;

	context->s.I.u = (U64)code;

	while (1)
	{
		U64 instruction_size = exec_instruction(context);

		if (instruction_size == 0)
		{
			return_value->u = 0;
			return 0;
		}

		if (instruction_size == U64_MAX)
		{
			*return_value = context->s.R;
			return bytes_executed + sizeof(U8);
		}

		bytes_executed += instruction_size;
	}
}

MVM64_REGISTERS* create_context()
{
	MVM64_REGISTERS* reg = calloc(1, sizeof(MVM64_REGISTERS));

	if (!reg)
		return NULL;

	void* stack = malloc(STACK_SIZE * sizeof(INT64));

	if (!stack)
		return NULL;

	reg->s.S.u = (U64)stack;
	reg->s.Z.u = (U64)stack;

	return reg;
}