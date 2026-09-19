#include "bridge.h"

#include <cctype>
#include <cstring>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Minimal flat-JSON parser.  Only handles objects whose values are strings,
// numbers, booleans, or nested containers (which are skipped).
// ---------------------------------------------------------------------------

namespace {

struct FlatJson {
    std::unordered_map<std::string, std::string> strs;
    std::unordered_map<std::string, double> nums;
    std::unordered_map<std::string, bool> bools;
};

void skipWs(const std::string& s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        ++i;
}

bool parseString(const std::string& s, size_t& i, std::string& out) {
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            switch (s[i]) {
                case 'n':  out.push_back('\n'); break;
                case 't':  out.push_back('\t'); break;
                case 'r':  out.push_back('\r'); break;
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                default:   out.push_back(s[i]); break;
            }
        } else {
            out.push_back(s[i]);
        }
        ++i;
    }
    if (i >= s.size()) return false;
    ++i;   // closing quote
    return true;
}

bool parseNumber(const std::string& s, size_t& i, double& out) {
    const size_t start = i;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
    }
    if (i == start) return false;
    out = std::stod(s.substr(start, i - start));
    return true;
}

void skipContainer(const std::string& s, size_t& i) {
    const char open  = s[i];
    const char close = (open == '{') ? '}' : ']';
    int depth = 0;
    bool inStr = false;

    while (i < s.size()) {
        const char c = s[i];
        if (inStr) {
            if (c == '\\') { i += 2; continue; }
            if (c == '"') inStr = false;
        } else {
            if (c == '"') inStr = true;
            else if (c == open) ++depth;
            else if (c == close) {
                --depth;
                if (depth == 0) { ++i; return; }
            }
        }
        ++i;
    }
}

bool parseFlatObject(const std::string& s, size_t& i, FlatJson& out) {
    skipWs(s, i);
    if (i >= s.size() || s[i] != '{') return false;
    ++i;
    skipWs(s, i);
    if (i < s.size() && s[i] == '}') { ++i; return true; }

    while (i < s.size()) {
        skipWs(s, i);

        std::string key;
        if (!parseString(s, i, key)) return false;

        skipWs(s, i);
        if (i >= s.size() || s[i] != ':') return false;
        ++i;
        skipWs(s, i);
        if (i >= s.size()) return false;

        if (s[i] == '"') {
            std::string val;
            if (!parseString(s, i, val)) return false;
            out.strs[key] = val;
        } else if (s[i] == '{' || s[i] == '[') {
            skipContainer(s, i);
        } else if (s.compare(i, 4, "true") == 0) {
            out.bools[key] = true;
            i += 4;
        } else if (s.compare(i, 5, "false") == 0) {
            out.bools[key] = false;
            i += 5;
        } else if (s.compare(i, 4, "null") == 0) {
            i += 4;
        } else {
            double d = 0;
            if (!parseNumber(s, i, d)) return false;
            out.nums[key] = d;
        }

        skipWs(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        return false;
    }
    return false;
}

std::string getStr(const FlatJson& j, const std::string& k, const std::string& def = "") {
    auto it = j.strs.find(k);
    return (it == j.strs.end()) ? def : it->second;
}

double getNum(const FlatJson& j, const std::string& k, double def = 0) {
    auto it = j.nums.find(k);
    return (it == j.nums.end()) ? def : it->second;
}

bool getBool(const FlatJson& j, const std::string& k, bool def = false) {
    auto it = j.bools.find(k);
    return (it == j.bools.end()) ? def : it->second;
}

bool parseInputEvent(const std::string& line, InputEvent& ev) {
    size_t i = 0;
    FlatJson j;
    if (!parseFlatObject(line, i, j)) return false;

    const std::string type = getStr(j, "type");

    if (type == "key") {
        ev.kind = EventKind::KEY;
        ev.code = static_cast<int>(getNum(j, "code"));
        ev.pressed = getBool(j, "pressed");
    } else if (type == "mouse") {
        ev.kind = getBool(j, "pressed") ? EventKind::MOUSE_DOWN : EventKind::MOUSE_UP;
        ev.x = static_cast<int>(getNum(j, "x"));
        ev.y = static_cast<int>(getNum(j, "y"));
        ev.button = static_cast<int>(getNum(j, "button"));
    } else if (type == "mouse_move") {
        ev.kind = EventKind::MOUSE_MOVE;
        ev.x = static_cast<int>(getNum(j, "x"));
        ev.y = static_cast<int>(getNum(j, "y"));
    } else if (type == "quit") {
        ev.kind = EventKind::QUIT;
    } else if (type == "cmd") {
        ev.kind = EventKind::CMD;
        ev.cmd  = getStr(j, "name");
        ev.arg  = getStr(j, "arg");
        ev.argi = static_cast<int>(getNum(j, "argi"));
    } else {
        return false;
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Bridge
// ---------------------------------------------------------------------------

Bridge::~Bridge() {
    stop();
}

bool Bridge::start(uint16_t port) {
    listenSock_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock_ == INVALID_SOCK) return false;

    int yes = 1;
    ::setsockopt(listenSock_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = ::htons(port);

    if (::bind(listenSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        net::closeSocket(listenSock_);
        listenSock_ = INVALID_SOCK;
        return false;
    }

    if (::listen(listenSock_, 1) != 0) {
        net::closeSocket(listenSock_);
        listenSock_ = INVALID_SOCK;
        return false;
    }

    running_ = true;
    return true;
}

bool Bridge::waitForClient() {
    if (listenSock_ == INVALID_SOCK) return false;

    sockaddr_in addr{};
#ifdef _WIN32
    int len = sizeof(addr);
#else
    socklen_t len = sizeof(addr);
#endif

    clientSock_ = ::accept(listenSock_, reinterpret_cast<sockaddr*>(&addr), &len);
    if (clientSock_ == INVALID_SOCK) return false;

    net::setNoDelay(clientSock_);
    connected_ = true;

    recvThread_ = std::thread(&Bridge::recvLoop, this);
    return true;
}

void Bridge::disconnect() {
    if (clientSock_ != INVALID_SOCK) net::shutdownSocket(clientSock_);
    connected_ = false;

    if (recvThread_.joinable()) recvThread_.join();

    if (clientSock_ != INVALID_SOCK) {
        net::closeSocket(clientSock_);
        clientSock_ = INVALID_SOCK;
    }

    std::lock_guard<std::mutex> lk(evMutex_);
    events_.clear();
}

void Bridge::stop() {
    disconnect();
    running_ = false;
    if (listenSock_ != INVALID_SOCK) {
        net::closeSocket(listenSock_);
        listenSock_ = INVALID_SOCK;
    }
}

bool Bridge::sendAll(const void* data, size_t n) {
    const char* p = static_cast<const char*>(data);
    size_t sent = 0;

    while (sent < n) {
        const size_t remain = n - sent;
        const size_t chunk = (remain > (1u << 20)) ? (1u << 20) : remain;
        const int r = ::send(clientSock_, p + sent, static_cast<int>(chunk), 0);
        if (r <= 0) {
            connected_ = false;
            return false;
        }
        sent += static_cast<size_t>(r);
    }
    return true;
}

bool Bridge::sendFrame(const uint8_t* pixels, size_t n) {
    if (!connected_.load()) return false;

    const uint32_t magic = 0x46524D31u;
    uint8_t header[4] = {
        static_cast<uint8_t>(magic & 0xFF),
        static_cast<uint8_t>((magic >> 8) & 0xFF),
        static_cast<uint8_t>((magic >> 16) & 0xFF),
        static_cast<uint8_t>((magic >> 24) & 0xFF),
    };

    if (!sendAll(header, sizeof(header))) return false;
    if (!sendAll(pixels, n)) return false;
    return true;
}

bool Bridge::sendText(const std::string& text) {
    if (!connected_.load()) return false;
    const uint32_t magic = 0x4C545854u;
    const uint32_t len   = static_cast<uint32_t>(text.size());
    uint8_t header[8] = {
        static_cast<uint8_t>(magic & 0xFF),
        static_cast<uint8_t>((magic >> 8) & 0xFF),
        static_cast<uint8_t>((magic >> 16) & 0xFF),
        static_cast<uint8_t>((magic >> 24) & 0xFF),
        static_cast<uint8_t>(len & 0xFF),
        static_cast<uint8_t>((len >> 8) & 0xFF),
        static_cast<uint8_t>((len >> 16) & 0xFF),
        static_cast<uint8_t>((len >> 24) & 0xFF),
    };
    if (!sendAll(header, sizeof(header))) return false;
    if (len && !sendAll(text.data(), len)) return false;
    return true;
}

std::vector<InputEvent> Bridge::pollEvents() {
    std::lock_guard<std::mutex> lk(evMutex_);
    std::vector<InputEvent> out;
    out.swap(events_);
    return out;
}

void Bridge::recvLoop() {
    std::string buf;
    char tmp[4096];

    while (connected_.load() && running_.load()) {
        const int n = ::recv(clientSock_, tmp, sizeof(tmp), 0);
        if (n <= 0) break;

        buf.append(tmp, static_cast<size_t>(n));

        size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);
            if (line.empty()) continue;

            InputEvent ev;
            if (parseInputEvent(line, ev)) {
                std::lock_guard<std::mutex> lk(evMutex_);
                events_.push_back(std::move(ev));
            }
        }
    }

    connected_ = false;
}
