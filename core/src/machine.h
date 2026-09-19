#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cpu.h"
#include "framebuffer.h"
#include "memory.h"
#include "vfs.h"

enum class ProcState { READY, RUNNING, BLOCKED, DEAD };

const char* stateName(ProcState s);

struct FileHandle {
    bool used = false;
    std::string path;
    size_t offset = 0;
    bool writable = false;
};

struct Process {
    uint32_t pid = 0;
    std::string name;
    CPU cpu;
    Memory mem;
    ProcState state = ProcState::READY;
    uint64_t wakeTick = 0;
    uint32_t exitCode = 0;
    std::array<FileHandle, 16> fds{};
};

class Machine {
public:
    Machine();

    Memory memory;      // kernel / shared scratch memory
    Framebuffer fb;
    VFS vfs;

    uint64_t tick = 0;
    uint32_t sliceBudget = 10000;

    uint32_t spawn(const std::string& name,
                   const std::vector<uint8_t>& code,
                   uint32_t loadAddr = 0x1000);

    bool kill(uint32_t pid);
    Process* find(uint32_t pid);
    Process* current();

    void stepOne();
    void runSlice(int maxCycles);
    void reset();

    void drawPixel(uint32_t index, uint32_t rgba);
    int  syscall(CPU* cpu, int num);

    std::vector<std::string> drainLog();
    const std::vector<std::unique_ptr<Process>>& processes() const { return procs_; }

private:
    std::vector<std::unique_ptr<Process>> procs_;
    uint32_t nextPid_ = 1;
    size_t currentIdx_ = 0;
    uint32_t sliceUsed_ = 0;
    std::vector<std::string> log_;
};
