#pragma once

#include <cstdint>
#include <vector>

#include "cpu.h"

// Tiny assembler helper for building sandbox bytecode.
struct Asm {
    std::vector<uint8_t> code;

    void emit(uint8_t op, uint8_t rd = 0, uint8_t rs1 = 0, int32_t imm = 0) {
        code.push_back(op);
        code.push_back(rd);
        code.push_back(rs1);
        code.push_back(0);
        const uint32_t u = static_cast<uint32_t>(imm);
        code.push_back(static_cast<uint8_t>(u & 0xFF));
        code.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
        code.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
        code.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
    }

    size_t here() const { return code.size(); }
};

// Fills a `rows` x `cols` rectangle starting at pixel index `startIndex`,
// advancing by WIDTH pixels per row.
std::vector<uint8_t> makeRectProgram(uint32_t startIndex, int rows, int cols, uint32_t color);

// Draws a horizontal line of `length` pixels starting at `startIndex`.
std::vector<uint8_t> makeLineProgram(uint32_t startIndex, int length, uint32_t color);

// Demo: writes a string to /tmp/log.txt and then exits.
std::vector<uint8_t> makeFileProgram();
