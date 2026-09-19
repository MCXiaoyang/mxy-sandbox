#include "machine.h"

#include <algorithm>
#include <cstdio>

const char* stateName(ProcState s) {
    switch (s) {
        case ProcState::READY:   return "READY";
        case ProcState::RUNNING: return "RUN";
        case ProcState::BLOCKED: return "BLOCKED";
        case ProcState::DEAD:    return "DEAD";
    }
    return "?";
}

Machine::Machine() {
    fb.clear(0xFF101018);
}

uint32_t Machine::spawn(const std::string& name,
                        const std::vector<uint8_t>& code,
                        uint32_t loadAddr) {
    auto p = std::make_unique<Process>();
    p->pid = nextPid_++;
    p->name = name;
    p->cpu.reset();
    p->cpu.pc = loadAddr;
    p->state = ProcState::READY;

    if (!p->mem.load(loadAddr, code.data(), code.size())) {
        log_.push_back("spawn failed: program too large (" + name + ")");
        return 0;
    }

    const uint32_t pid = p->pid;
    procs_.push_back(std::move(p));
    log_.push_back("spawned pid " + std::to_string(pid) + " '" + name + "'");
    return pid;
}

Process* Machine::find(uint32_t pid) {
    for (auto& p : procs_) if (p->pid == pid) return p.get();
    return nullptr;
}

Process* Machine::current() {
    if (procs_.empty()) return nullptr;
    if (currentIdx_ >= procs_.size()) currentIdx_ = 0;
    return procs_[currentIdx_].get();
}

bool Machine::kill(uint32_t pid) {
    Process* p = find(pid);
    if (!p || p->state == ProcState::DEAD) return false;
    p->state = ProcState::DEAD;
    p->exitCode = 137;
    log_.push_back("killed pid " + std::to_string(pid));
    return true;
}

void Machine::reset() {
    procs_.clear();
    nextPid_ = 1;
    currentIdx_ = 0;
    sliceUsed_ = 0;
    tick = 0;
    fb.clear(0xFF101018);
    log_.push_back("machine reset");
}

void Machine::drawPixel(uint32_t index, uint32_t rgba) {
    fb.setPixelIndex(index, rgba);
}

void Machine::stepOne() {
    const size_t n = procs_.size();
    if (n == 0) { tick++; return; }

    Process* p = nullptr;
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (currentIdx_ + i) % n;
        Process* cand = procs_[idx].get();
        if (cand->state == ProcState::READY || cand->state == ProcState::RUNNING) {
            p = cand;
            currentIdx_ = idx;
            break;
        }
    }

    if (!p) {
        // Nothing runnable. Fast-forward to the earliest wake-up, if any.
        uint64_t earliest = UINT64_MAX;
        for (auto& up : procs_) {
            if (up->state == ProcState::BLOCKED && up->wakeTick < earliest)
                earliest = up->wakeTick;
        }
        if (earliest == UINT64_MAX) { tick++; return; }
        tick = std::max(tick, earliest);
        for (auto& up : procs_) {
            if (up->state == ProcState::BLOCKED && up->wakeTick <= tick)
                up->state = ProcState::READY;
        }
        return;
    }

    p->state = ProcState::RUNNING;
    int cost = p->cpu.step(*this, p->mem);
    if (cost <= 0) cost = 1;

    tick += static_cast<uint64_t>(cost);
    sliceUsed_ += static_cast<uint32_t>(cost);

    if (p->cpu.halted || p->cpu.faulted) {
        p->state = ProcState::DEAD;
        if (p->cpu.faulted)
            log_.push_back("pid " + std::to_string(p->pid) + " faulted: " + p->cpu.faultReason);
        else
            log_.push_back("pid " + std::to_string(p->pid) + " exited ("
                           + std::to_string(p->exitCode) + ")");
        sliceUsed_ = sliceBudget;   // force a reschedule
    }

    if (sliceUsed_ >= sliceBudget) {
        if (p->state == ProcState::RUNNING) p->state = ProcState::READY;
        currentIdx_ = (currentIdx_ + 1) % n;
        sliceUsed_ = 0;
    }
}

void Machine::runSlice(int maxCycles) {
    for (int i = 0; i < maxCycles; ++i) {
        bool alive = false;
        for (auto& p : procs_) {
            if (p->state != ProcState::DEAD) { alive = true; break; }
        }
        if (!alive) break;
        stepOne();
    }
}

int Machine::syscall(CPU* cpu, int num) {
    Process* p = current();
    if (!p) return 1;

    switch (num) {
        case 1: {   // exit(code)
            p->exitCode = cpu->reg[1];
            p->cpu.halted = true;
            return 1;
        }

        case 2: {   // sleep(ticks)
            const uint32_t ticks = cpu->reg[1];
            p->state = ProcState::BLOCKED;
            p->wakeTick = tick + ticks;
            return 1;
        }

        case 3: {   // draw(x, y, color)
            const uint32_t x = cpu->reg[1];
            const uint32_t y = cpu->reg[2];
            const uint32_t c = cpu->reg[3];
            if (x < static_cast<uint32_t>(Framebuffer::WIDTH) &&
                y < static_cast<uint32_t>(Framebuffer::HEIGHT)) {
                drawPixel(y * Framebuffer::WIDTH + x, c);
            }
            return 2;
        }

        case 4: {   // open(pathPtr, mode) -> fd
            const uint32_t pathPtr = cpu->reg[1];
            const uint32_t mode    = cpu->reg[2];
            const std::string path = p->mem.readCString(pathPtr);

            int fd = -1;
            for (int i = 3; i < 16; ++i) {
                if (!p->fds[i].used) { fd = i; break; }
            }
            if (fd < 0 || path.empty()) { cpu->reg[0] = 0xFFFFFFFFu; return 3; }

            if (!vfs.exists(path)) {
                if (mode == 1) vfs.create(path);
                else { cpu->reg[0] = 0xFFFFFFFFu; return 3; }
            }
            p->fds[fd].used = true;
            p->fds[fd].path = path;
            p->fds[fd].offset = 0;
            p->fds[fd].writable = (mode == 1);
            cpu->reg[0] = static_cast<uint32_t>(fd);
            return 3;
        }

        case 5: {   // read(fd, buf, len) -> n
            const uint32_t fd  = cpu->reg[1];
            const uint32_t buf = cpu->reg[2];
            const uint32_t len = cpu->reg[3];
            if (fd >= 16 || !p->fds[fd].used) { cpu->reg[0] = 0xFFFFFFFFu; return 3; }

            const std::vector<uint8_t> data = vfs.read(p->fds[fd].path);
            const size_t off = p->fds[fd].offset;
            const size_t avail = (off < data.size()) ? data.size() - off : 0;
            const size_t n = std::min<size_t>(avail, len);
            for (size_t i = 0; i < n; ++i)
                p->mem.write8(buf + static_cast<uint32_t>(i), data[off + i]);
            p->fds[fd].offset += n;
            cpu->reg[0] = static_cast<uint32_t>(n);
            return 3;
        }

        case 6: {   // write(fd, buf, len) -> n
            const uint32_t fd  = cpu->reg[1];
            const uint32_t buf = cpu->reg[2];
            const uint32_t len = cpu->reg[3];
            if (fd >= 16 || !p->fds[fd].used) { cpu->reg[0] = 0xFFFFFFFFu; return 3; }

            std::vector<uint8_t> data = vfs.read(p->fds[fd].path);
            const size_t off = p->fds[fd].offset;
            if (data.size() < off + len) data.resize(off + len, 0);
            for (uint32_t i = 0; i < len; ++i)
                data[off + i] = p->mem.read8(buf + i);
            vfs.write(p->fds[fd].path, data.data(), data.size());
            p->fds[fd].offset += len;
            cpu->reg[0] = len;
            return 3;
        }

        case 7: {   // close(fd)
            const uint32_t fd = cpu->reg[1];
            if (fd < 16 && p->fds[fd].used) {
                p->fds[fd] = FileHandle{};
                cpu->reg[0] = 0;
            } else {
                cpu->reg[0] = 0xFFFFFFFFu;
            }
            return 2;
        }

        case 8: {   // print(ptr)  -- debug output to console
            const std::string s = p->mem.readCString(cpu->reg[1]);
            log_.push_back("[pid " + std::to_string(p->pid) + "] " + s);
            return 3;
        }

        default:
            log_.push_back("pid " + std::to_string(p->pid)
                           + " called unknown syscall " + std::to_string(num));
            return 1;
    }
}

std::vector<std::string> Machine::drainLog() {
    std::vector<std::string> out;
    out.swap(log_);
    return out;
}
