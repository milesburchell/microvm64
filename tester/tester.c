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

U8 buffer[1024];

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printf("Tester: No arg");
        return -1;
    }

    char* binary = argv[1];

    MVM64_REGISTERS* context = create_context();

    assert(context);

    INT64 retnval;
    U64 code_executed = execute(testcode, context, &retnval);

    printf("Test hardcode: Executed 0x%llx bytes, return value 0x%llx\n", code_executed, retnval.u);

    free_context(context);

    FILE* bin;
    fopen_s(&bin, binary, "r");

    if (!bin)
    {
        printf("Couldn't open %s.", binary);
        return -1;
    }

    size_t read = fread(buffer, sizeof(U8), 1024, bin);

    if (!read)
    {
        printf("Couldn't read data from %s.", binary);
        return -1;
    }

    printf("Read %llu bytes from %s\n", read, binary);

    fclose(bin);

    context = create_context();

    assert(context);

    INT64 twenty;
    twenty.u = 20;
    push(twenty, context);

    code_executed = execute(buffer, context, &retnval);

    printf("Test binary: Executed 0x%llx bytes, return value 0x%llx\n", code_executed, retnval.u);

    free_context(context);
}
