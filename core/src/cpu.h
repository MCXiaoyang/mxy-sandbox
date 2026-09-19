#pragma once

#include <array>
#include <cstdint>
#include <string>

class Machine;
class Memory;

enum : uint8_t {
    OP_MOV  = 0x01,   // rd = imm
    OP_MOVR = 0x02,   // rd = rs1
    OP_ADD  = 0x03,   // rd += rs1
    OP_SUB  = 0x04,   // rd -= rs1
    OP_ADDI = 0x05,   // rd += imm
    OP_MUL  = 0x06,   // rd *= rs1
    OP_SHL  = 0x07,   // rd <<= imm
    OP_JMP  = 0x08,   // pc = imm
    OP_JZ   = 0x09,   // if rd == 0: pc = imm
    OP_JNZ  = 0x0A,   // if rd != 0: pc = imm
    OP_LD   = 0x0B,   // rd = mem32[rs1]
    OP_ST   = 0x0C,   // mem32[rd] = rs1
    OP_STB  = 0x0D,   // mem8[rd] = rs1 & 0xFF
    OP_LDB  = 0x0E,   // rd = mem8[rs1]
    OP_DRAW = 0x0F,   // framebuffer[rd] = rs1
    OP_SYS  = 0x10,   // syscall #imm, args in r1..r3, result in r0
    OP_HALT = 0xFF,
};

struct CPU {
    static constexpr int NREG = 32;

    std::array<uint32_t, NREG> reg{};
    uint32_t pc = 0;
    uint64_t cycles = 0;
    bool halted = false;
    bool faulted = false;
    std::string faultReason;

    CPU() { reset(); }

    void reset() {
        reg.fill(0);
        pc = 0;
        cycles = 0;
        halted = false;
        faulted = false;
        faultReason.clear();
    }

    void fault(const std::string& reason) {
        faulted = true;
        faultReason = reason;
    }

    // Returns the number of cycles consumed. 0 means halted or faulted.
    int step(Machine& m, Memory& mem);
};
