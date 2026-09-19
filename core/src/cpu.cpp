#include "cpu.h"
#include "machine.h"
#include "memory.h"

#include <cstdio>

static std::string hex32(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", v);
    return buf;
}

int CPU::step(Machine& m, Memory& mem) {
    if (halted || faulted) return 0;

    if (!mem.valid(pc, 8)) {
        fault("pc out of range: " + hex32(pc));
        return 0;
    }

    const uint8_t op  = mem.read8(pc);
    const uint8_t rd  = static_cast<uint8_t>(mem.read8(pc + 1) & (NREG - 1));
    const uint8_t rs1 = static_cast<uint8_t>(mem.read8(pc + 2) & (NREG - 1));
    const int32_t imm = static_cast<int32_t>(mem.read32(pc + 4));

    uint32_t next = pc + 8;
    int cost = 1;

    switch (op) {
        case OP_MOV:  reg[rd] = static_cast<uint32_t>(imm); break;
        case OP_MOVR: reg[rd] = reg[rs1]; break;
        case OP_ADD:  reg[rd] = reg[rd] + reg[rs1]; break;
        case OP_SUB:  reg[rd] = reg[rd] - reg[rs1]; break;
        case OP_ADDI: reg[rd] = reg[rd] + static_cast<uint32_t>(imm); break;
        case OP_MUL:  reg[rd] = reg[rd] * reg[rs1]; cost = 3; break;
        case OP_SHL:  reg[rd] = reg[rd] << (static_cast<uint32_t>(imm) & 31u); break;

        case OP_JMP:  next = static_cast<uint32_t>(imm); cost = 2; break;
        case OP_JZ:   if (reg[rd] == 0) { next = static_cast<uint32_t>(imm); cost = 2; } break;
        case OP_JNZ:  if (reg[rd] != 0) { next = static_cast<uint32_t>(imm); cost = 2; } break;

        case OP_LD:   reg[rd] = mem.read32(reg[rs1]); cost = 2; break;
        case OP_ST:   mem.write32(reg[rd], reg[rs1]); cost = 2; break;
        case OP_STB:  mem.write8(reg[rd], static_cast<uint8_t>(reg[rs1] & 0xFF)); cost = 2; break;
        case OP_LDB:  reg[rd] = mem.read8(reg[rs1]); cost = 2; break;

        case OP_DRAW: m.drawPixel(reg[rd], reg[rs1]); cost = 2; break;

        case OP_SYS:  cost = m.syscall(this, static_cast<int>(imm)); break;

        case OP_HALT:
            halted = true;
            return 0;

        default:
            fault("illegal opcode 0x" + hex32(op) + " at pc=" + hex32(pc));
            return 0;
    }

    pc = next;
    cycles += static_cast<uint64_t>(cost);
    return cost;
}
