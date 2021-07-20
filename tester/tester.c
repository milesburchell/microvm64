#include <stdio.h>
#include <assert.h>
#include "vm.h"
#pragma comment(lib,"mvm64.lib")

U8 testcode[] = {
    MOV | VALB_FLAG | SMALL_FLAG, // move 8-bit value to register
    0, // register A
    0x14, // 8-bit value
    ADD | VALB_FLAG, // add 64-bit value to register
    0, // register A
    0x10,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // 64bit 0x10
    MOV, // move register to register
    8, // dest register R
    0, // source register A
    RET // return
};

int main()
{
    MVM64_REGISTERS* context = create_context();

    assert(context);

    INT64 retnval;
    U64 code_executed = execute(testcode, context, &retnval);

    printf("Executed 0x%llx bytes, return value 0x%llx\n", code_executed, retnval.u);

    free_context(context);
}
