#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

class Memory {
public:
    static constexpr uint32_t SIZE = 1024u * 1024u;   // 1 MB

    Memory() : mem_(SIZE, 0) {}

    void reset() { std::fill(mem_.begin(), mem_.end(), 0); }

    bool valid(uint32_t addr, uint32_t len = 1) const {
        return static_cast<uint64_t>(addr) + len <= SIZE;
    }

    uint8_t read8(uint32_t a) const {
        return valid(a) ? mem_[a] : 0;
    }

    uint32_t read32(uint32_t a) const {
        if (!valid(a, 4)) return 0;
        return  static_cast<uint32_t>(mem_[a])
             | (static_cast<uint32_t>(mem_[a + 1]) << 8)
             | (static_cast<uint32_t>(mem_[a + 2]) << 16)
             | (static_cast<uint32_t>(mem_[a + 3]) << 24);
    }

    void write8(uint32_t a, uint8_t v) {
        if (valid(a)) mem_[a] = v;
    }

    void write32(uint32_t a, uint32_t v) {
        if (!valid(a, 4)) return;
        mem_[a]     = static_cast<uint8_t>(v & 0xFF);
        mem_[a + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        mem_[a + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        mem_[a + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
    }

    bool load(uint32_t addr, const uint8_t* data, size_t n) {
        if (!valid(addr, static_cast<uint32_t>(n))) return false;
        std::memcpy(mem_.data() + addr, data, n);
        return true;
    }

    std::string readCString(uint32_t addr, size_t maxLen = 256) const {
        std::string s;
        for (size_t i = 0; i < maxLen; ++i) {
            if (!valid(addr + static_cast<uint32_t>(i))) break;
            char c = static_cast<char>(mem_[addr + i]);
            if (c == 0) break;
            s.push_back(c);
        }
        return s;
    }

    const std::vector<uint8_t>& raw() const { return mem_; }

private:
    std::vector<uint8_t> mem_;
};
