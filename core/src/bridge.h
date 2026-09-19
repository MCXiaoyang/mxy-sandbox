#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "socket_util.h"

enum class EventKind {
    UNKNOWN,
    KEY,
    MOUSE_DOWN,
    MOUSE_UP,
    MOUSE_MOVE,
    QUIT,
    CMD,
};

struct InputEvent {
    EventKind kind = EventKind::UNKNOWN;
    int code = 0;
    bool pressed = false;
    int x = 0;
    int y = 0;
    int button = 0;
    std::string cmd;
    std::string arg;
    int argi = 0;
};

class Bridge {
public:
    Bridge() = default;
    ~Bridge();

    Bridge(const Bridge&) = delete;
    Bridge& operator=(const Bridge&) = delete;

    bool start(uint16_t port);

    // Blocks until a shell connects. Returns false on fatal error.
    bool waitForClient();

    void disconnect();
    void stop();

    bool sendFrame(const uint8_t* pixels, size_t n);
    bool sendText(const std::string& text);

    std::vector<InputEvent> pollEvents();

    bool connected() const { return connected_.load(); }

private:
    socket_t listenSock_ = INVALID_SOCK;
    socket_t clientSock_ = INVALID_SOCK;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread recvThread_;
    std::mutex evMutex_;
    std::vector<InputEvent> events_;

    bool sendAll(const void* data, size_t n);
    void recvLoop();
};
