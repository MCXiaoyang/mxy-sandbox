#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
  static const socket_t INVALID_SOCK = INVALID_SOCKET;
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <cstring>
  using socket_t = int;
  static const socket_t INVALID_SOCK = -1;
#endif

#include <string>

namespace net {

struct WinsockInit {
#ifdef _WIN32
    WinsockInit()  { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); }
    ~WinsockInit() { WSACleanup(); }
#endif
};

inline int closeSocket(socket_t s) {
#ifdef _WIN32
    return ::closesocket(s);
#else
    return ::close(s);
#endif
}

inline void shutdownSocket(socket_t s) {
#ifdef _WIN32
    ::shutdown(s, SD_BOTH);
#else
    ::shutdown(s, SHUT_RDWR);
#endif
}

inline bool setNoDelay(socket_t s) {
    int one = 1;
    return ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
                        reinterpret_cast<const char*>(&one), sizeof(one)) == 0;
}

} // namespace net
