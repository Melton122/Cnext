#ifndef CNEXT_NET_H
#define CNEXT_NET_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Windows networking
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

// --- HTTP ---

static inline CnextString http_get(CnextString url) {
    URL_COMPONENTS uc = {0};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0};
    wchar_t path[1024] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;

    wchar_t wurl[1024];
    MultiByteToWideChar(CP_UTF8, 0, url.data, -1, wurl, 1024);
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return (CnextString){NULL, 0};

    HINTERNET hSession = WinHttpOpen(L"Cnext/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return (CnextString){NULL, 0};

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0};
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0};
    }

    size_t capacity = 4096;
    size_t length = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }

    DWORD dwSize = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        if (length + dwSize + 1 > capacity) {
            capacity = (length + dwSize + 1) * 2;
            char* newbuf = (char*)realloc(buffer, capacity);
            if (!newbuf) { free(buffer); WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }
            buffer = newbuf;
        }
        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, buffer + length, dwSize, &dwRead)) break;
        length += dwRead;
    } while (dwSize > 0);

    buffer[length] = '\0';
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    _cnext_track(buffer);
    return (CnextString){buffer, length};
}

static inline CnextString http_post(CnextString url, CnextString body) {
    URL_COMPONENTS uc = {0};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0};
    wchar_t path[1024] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;

    wchar_t wurl[1024];
    MultiByteToWideChar(CP_UTF8, 0, url.data, -1, wurl, 1024);
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return (CnextString){NULL, 0};

    HINTERNET hSession = WinHttpOpen(L"Cnext/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return (CnextString){NULL, 0};

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }

    wchar_t headers[] = L"Content-Type: application/x-www-form-urlencoded\r\n";
    if (!WinHttpSendRequest(hRequest, headers, -1L, (LPVOID)body.data, (DWORD)body.length, (DWORD)body.length, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0};
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0};
    }

    size_t capacity = 4096;
    size_t length = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }

    DWORD dwSize = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        if (length + dwSize + 1 > capacity) {
            capacity = (length + dwSize + 1) * 2;
            char* newbuf = (char*)realloc(buffer, capacity);
            if (!newbuf) { free(buffer); WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return (CnextString){NULL, 0}; }
            buffer = newbuf;
        }
        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, buffer + length, dwSize, &dwRead)) break;
        length += dwRead;
    } while (dwSize > 0);

    buffer[length] = '\0';
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    _cnext_track(buffer);
    return (CnextString){buffer, length};
}

static inline bool download_file(CnextString url, CnextString dest_path) {
    CnextString data = http_get(url);
    if (!data.data) return false;
    FILE* fp = fopen(dest_path.data, "wb");
    if (!fp) { free(data.data); return false; }
    fwrite(data.data, 1, data.length, fp);
    fclose(fp);
    free(data.data);
    return true;
}

// --- TCP ---

static inline int tcp_connect(CnextString host, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return -1;

    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host.data, port_str, &hints, &result) != 0) {
        WSACleanup();
        return -1;
    }

    int sock = -1;
    for (struct addrinfo* ptr = result; ptr != NULL; ptr = ptr->ai_next) {
        sock = (int)socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (sock == -1) continue;
        if (connect(sock, ptr->ai_addr, (int)ptr->ai_addrlen) == 0) break;
        closesocket(sock);
        sock = -1;
    }
    freeaddrinfo(result);
    if (sock == -1) { WSACleanup(); return -1; }
    return sock;
}

static inline bool tcp_send(int sock, CnextString data) {
    if (sock == -1) return false;
    int sent = send(sock, data.data, (int)data.length, 0);
    return sent == (int)data.length;
}

static inline CnextString tcp_receive(int sock, int max_bytes) {
    if (sock == -1) return (CnextString){NULL, 0};
    char* recvbuf = (char*)malloc(max_bytes + 1);
    if (!recvbuf) return (CnextString){NULL, 0};

    int iResult = recv(sock, recvbuf, max_bytes, 0);
    if (iResult > 0) {
        recvbuf[iResult] = '\0';
        _cnext_track(recvbuf);
        return (CnextString){recvbuf, (size_t)iResult};
    } else {
        free(recvbuf);
        return (CnextString){NULL, 0};
    }
}

static inline void tcp_close(int sock) {
    if (sock != -1) {
        closesocket((SOCKET)sock);
    }
    WSACleanup();
}

#endif
