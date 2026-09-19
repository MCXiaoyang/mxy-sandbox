#include "programs.h"

#include "framebuffer.h"

static constexpr uint32_t PROG_BASE = 0x1000;

std::vector<uint8_t> makeRectProgram(uint32_t startIndex, int rows, int cols, uint32_t color) {
    Asm a;

    a.emit(OP_MOV,  1, 0, rows);
    a.emit(OP_MOV,  8, 0, static_cast<int32_t>(startIndex));
    a.emit(OP_MOV,  4, 0, static_cast<int32_t>(color));

    const size_t outer = a.here();

    a.emit(OP_MOVR, 9, 8, 0);
    a.emit(OP_MOV,  2, 0, cols);

    const size_t inner = a.here();

    a.emit(OP_DRAW, 9, 4, 0);
    a.emit(OP_ADDI, 9, 0, 1);
    a.emit(OP_ADDI, 2, 0, -1);
    a.emit(OP_JNZ,  2, 0, static_cast<int32_t>(PROG_BASE + inner));

    a.emit(OP_ADDI, 8, 0, Framebuffer::WIDTH);
    a.emit(OP_ADDI, 1, 0, -1);
    a.emit(OP_JNZ,  1, 0, static_cast<int32_t>(PROG_BASE + outer));

    a.emit(OP_HALT);

    return a.code;
}

std::vector<uint8_t> makeLineProgram(uint32_t startIndex, int length, uint32_t color) {
    Asm a;

    a.emit(OP_MOV,  1, 0, length);
    a.emit(OP_MOV,  2, 0, static_cast<int32_t>(startIndex));
    a.emit(OP_MOV,  4, 0, static_cast<int32_t>(color));

    const size_t loop = a.here();

    a.emit(OP_DRAW, 2, 4, 0);
    a.emit(OP_ADDI, 2, 0, 1);
    a.emit(OP_ADDI, 1, 0, -1);
    a.emit(OP_JNZ,  1, 0, static_cast<int32_t>(PROG_BASE + loop));

    a.emit(OP_HALT);

    return a.code;
}

std::vector<uint8_t> makeFileProgram() {
    Asm a;

    a.emit(OP_MOV, 1, 0, 0x0000);
    a.emit(OP_MOV, 2, 0, 1);
    a.emit(OP_SYS, 0, 0, 4);

    a.emit(OP_MOVR, 5, 0, 0);

    a.emit(OP_MOV, 1, 0, 0);
    a.emit(OP_MOVR, 1, 5, 0);
    a.emit(OP_MOV, 2, 0, 0x0100);
    a.emit(OP_MOV, 3, 0, 19);
    a.emit(OP_SYS, 0, 0, 6);

    a.emit(OP_MOVR, 1, 5, 0);
    a.emit(OP_SYS, 0, 0, 7);

    a.emit(OP_HALT);

    return a.code;
}