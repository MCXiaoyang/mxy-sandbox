#include <atomic>
#include <chrono>
#include <cstdio>
#include <csignal>
#include <sstream>
#include <string>
#include <thread>

#include "bridge.h"
#include "machine.h"
#include "programs.h"
#include "socket_util.h"

namespace {

std::atomic<bool> g_running{true};

void onSignal(int) {
    g_running = false;
}

const uint32_t kBackground = 0xFF101018;

// 输出到 core 控制台 + TTY
void reply(Bridge& bridge, const std::string& text) {
    std::printf("[core] %s\n", text.c_str());
    bridge.sendText(text + "\n");
}

void spawnDemo(Machine& m, Bridge& bridge, const std::string& name) {
    if (name == "rect-a" || name.empty()) {
        m.spawn("rect-a", makeRectProgram(0, 240, 320, 0xFF0000FF));
    } else if (name == "rect-b") {
        m.spawn("rect-b", makeRectProgram(240u * 640u + 320u, 240, 320, 0xFFFF0000));
    } else if (name == "line") {
        m.spawn("line", makeLineProgram(240u * 640u + 160u, 320, 0xFF00FF00));
    } else if (name == "file") {
        m.spawn("file", makeFileProgram());
    } else {
        reply(bridge, "unknown program '" + name + "'");
        reply(bridge, "available: rect-a, rect-b, line, file");
    }
}

void printProcesses(const Machine& m, Bridge& bridge) {
    reply(bridge, "pid    name         state    pc       cycles");
    for (const auto& p : m.processes()) {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
                      "%-6u %-12s %-8s %06X   %llu",
                      p->pid,
                      p->name.c_str(),
                      stateName(p->state),
                      p->cpu.pc,
                      static_cast<unsigned long long>(p->cpu.cycles));
        reply(bridge, buf);
    }
}

void handleCommand(Machine& m, Bridge& bridge, const InputEvent& ev) {
    const std::string& c = ev.cmd;

    if (c == "help") {
        reply(bridge, "commands:");
        reply(bridge, "  ls [path]       list virtual files");
        reply(bridge, "  cat <path>      print a file");
        reply(bridge, "  ps              list processes");
        reply(bridge, "  spawn <name>    rect-a / rect-b / line / file");
        reply(bridge, "  kill <pid>      kill a process");
        reply(bridge, "  clear           clear framebuffer");
        reply(bridge, "  restart         reset machine");
        reply(bridge, "  quit            disconnect");
    } else if (c == "spawn") {
        spawnDemo(m, bridge, ev.arg);
    } else if (c == "kill") {
        if (!m.kill(static_cast<uint32_t>(ev.argi)))
            reply(bridge, "no such pid " + std::to_string(ev.argi));
    } else if (c == "ls") {
        const std::string path = ev.arg.empty() ? "/" : ev.arg;
        auto entries = m.vfs.list(path);
        reply(bridge, "ls " + path + ":");
        if (entries.empty()) {
            reply(bridge, "  (empty)");
        } else {
            for (const auto& e : entries) reply(bridge, "  " + e);
        }
    } else if (c == "cat") {
        auto data = m.vfs.read(ev.arg);
        if (data.empty()) {
            reply(bridge, "cat: " + ev.arg + ": no such file or empty");
        } else {
            reply(bridge, ev.arg + " (" + std::to_string(data.size()) + " bytes):");
            std::string s(data.begin(), data.end());
            std::istringstream iss(s);
            std::string line;
            while (std::getline(iss, line)) reply(bridge, line);
        }
    } else if (c == "ps") {
        printProcesses(m, bridge);
    } else if (c == "clear") {
        m.fb.clear(kBackground);
        reply(bridge, "framebuffer cleared");
    } else if (c == "restart") {
        m.reset();
        spawnDemo(m, bridge, "rect-a");
        spawnDemo(m, bridge, "rect-b");
        reply(bridge, "machine restarted");
    } else {
        reply(bridge, "unknown command '" + c + "', type 'help'");
    }
}

void handleEvent(Machine& m, Bridge& bridge, const InputEvent& ev) {
    switch (ev.kind) {
        case EventKind::KEY:
            break;

        case EventKind::MOUSE_DOWN:
        case EventKind::MOUSE_UP:
        case EventKind::MOUSE_MOVE:
            break;

        case EventKind::QUIT:
            std::printf("[core] shell requested quit\n");
            break;

        case EventKind::CMD:
            handleCommand(m, bridge, ev);
            break;

        default:
            break;
    }
}

} // namespace

int main(int argc, char** argv) {
    net::WinsockInit wsa;

    uint16_t port = 9000;
    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));

    std::signal(SIGINT, onSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, onSignal);
#endif

    Machine machine;
    Bridge bridge;

    if (!bridge.start(port)) {
        std::fprintf(stderr, "[core] failed to bind 127.0.0.1:%u\n", port);
        return 1;
    }

    std::printf("[core] mxy-sandbox core listening on 127.0.0.1:%u\n", port);
    std::printf("[core] press Ctrl+C to quit\n");

    machine.reset();
    spawnDemo(machine, bridge, "rect-a");
    spawnDemo(machine, bridge, "rect-b");

    const auto frameInterval = std::chrono::microseconds(33333);

    while (g_running.load()) {
        std::printf("[core] waiting for shell...\n");

        if (!bridge.waitForClient()) {
            if (!g_running.load()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::printf("[core] shell connected\n");
        bridge.sendText("mxy-sandbox TTY\n");
        bridge.sendText("type 'help' for commands\n\n");

        auto lastFrame = std::chrono::steady_clock::now();

        while (g_running.load() && bridge.connected()) {
            for (const auto& ev : bridge.pollEvents()) {
                handleEvent(machine, bridge, ev);
            }

            machine.runSlice(200000);

            for (const auto& line : machine.drainLog()) {
                std::printf("[core] %s\n", line.c_str());
                bridge.sendText(line + "\n");
            }

            if (!bridge.sendFrame(machine.fb.data(), machine.fb.size())) break;

            const auto now = std::chrono::steady_clock::now();
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::microseconds>(now - lastFrame);
            if (elapsed < frameInterval) {
                std::this_thread::sleep_for(frameInterval - elapsed);
            }
            lastFrame = std::chrono::steady_clock::now();
        }

        std::printf("[core] shell disconnected\n");
        bridge.disconnect();
    }

    std::printf("[core] shutting down\n");
    bridge.stop();
    return 0;
}