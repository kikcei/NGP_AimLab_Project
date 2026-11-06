#pragma once
#include <winsock2.h>

// 블로킹 TCP 소켓에 패킷 전체를 보낸다.
inline bool SendPacket(SOCKET sock, const char* data, int size) {
    if (sock == INVALID_SOCKET || data == nullptr || size <= 0) return false;

    int sent = 0;
    while (sent < size) {
        const int bytes = ::send(sock, data + sent, size - sent, 0);
        if (bytes <= 0) return false;
        sent += bytes;
    }
    return true;
}
