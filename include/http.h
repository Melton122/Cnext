#ifndef CNEXT_HTTP_H
#define CNEXT_HTTP_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <curl/curl.h>
#endif

typedef struct {
    int status_code;
    CnextString body;
    CnextString headers;
} HttpResponse;

static inline void http_free_response(HttpResponse* resp) {
    if (resp->body.data) cnext_free(resp->body);
    if (resp->headers.data) cnext_free(resp->headers);
    resp->body = (CnextString){NULL, 0};
    resp->headers = (CnextString){NULL, 0};
    resp->status_code = 0;
}

#ifdef _WIN32

static inline HttpResponse http_request(CnextString method, CnextString url, CnextString body, CnextString extra_headers) {
    HttpResponse resp = {0, {NULL, 0}, {NULL, 0}};
    URL_COMPONENTS uc = {0};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {0};
    wchar_t path[1024] = {0};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 1024;
    wchar_t wurl[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, url.data, -1, wurl, 1024) == 0) return resp;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return resp;

    HINTERNET hSession = WinHttpOpen(L"Cnext/9.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return resp;
    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return resp; }

    wchar_t wmethod[16];
    MultiByteToWideChar(CP_UTF8, 0, method.data, -1, wmethod, 16);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wmethod, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp; }

    wchar_t headers_buf[4096] = {0};
    if (extra_headers.data && extra_headers.length > 0) {
        MultiByteToWideChar(CP_UTF8, 0, extra_headers.data, -1, headers_buf, 4096);
    }
    const void* req_body = (body.data && body.length > 0) ? body.data : WINHTTP_NO_REQUEST_DATA;
    DWORD req_len = (body.data && body.length > 0) ? (DWORD)body.length : 0;

    if (!WinHttpSendRequest(hRequest, headers_buf[0] ? headers_buf : WINHTTP_NO_ADDITIONAL_HEADERS,
        headers_buf[0] ? -1L : 0, (LPVOID)req_body, req_len, req_len, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp;
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp;
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    resp.status_code = (int)status;

    size_t capacity = 4096;
    size_t length = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp; }
    DWORD dwSize = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        if (length + dwSize + 1 > capacity) {
            capacity = (length + dwSize + 1) * 2;
            char* nb = (char*)realloc(buffer, capacity);
            if (!nb) { free(buffer); WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return resp; }
            buffer = nb;
        }
        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, buffer + length, dwSize, &dwRead)) break;
        length += dwRead;
    } while (dwSize > 0);
    buffer[length] = '\0';
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    _cnext_track(buffer);
    resp.body = (CnextString){buffer, length};
    return resp;
}

#else

struct CnextHttpBuf { char* data; size_t length; size_t capacity; };

static inline size_t _http_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    struct CnextHttpBuf* buf = (struct CnextHttpBuf*)userp;
    size_t total = size * nmemb;
    if (buf->length + total + 1 > buf->capacity) {
        size_t new_cap = (buf->length + total + 1) * 2;
        char* nd = (char*)realloc(buf->data, new_cap);
        if (!nd) return 0;
        buf->data = nd;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->length, contents, total);
    buf->length += total;
    buf->data[buf->length] = '\0';
    return total;
}

static inline size_t _http_header_cb(char* buffer, size_t size, size_t nitems, void* userp) {
    struct CnextHttpBuf* buf = (struct CnextHttpBuf*)userp;
    size_t total = size * nitems;
    if (buf->length + total + 1 > buf->capacity) {
        size_t new_cap = (buf->length + total + 1) * 2;
        char* nd = (char*)realloc(buf->data, new_cap);
        if (!nd) return 0;
        buf->data = nd;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->length, buffer, total);
    buf->length += total;
    buf->data[buf->length] = '\0';
    return total;
}

static inline HttpResponse http_request(CnextString method, CnextString url, CnextString body, CnextString extra_headers) {
    HttpResponse resp = {0, {NULL, 0}, {NULL, 0}};
    CURL* curl = curl_easy_init();
    if (!curl) return resp;

    struct CnextHttpBuf body_buf = {0};
    body_buf.capacity = 4096;
    body_buf.data = (char*)malloc(body_buf.capacity);
    if (!body_buf.data) { curl_easy_cleanup(curl); return resp; }
    body_buf.data[0] = '\0';

    struct CnextHttpBuf hdr_buf = {0};
    hdr_buf.capacity = 4096;
    hdr_buf.data = (char*)malloc(hdr_buf.capacity);
    if (!hdr_buf.data) { free(body_buf.data); curl_easy_cleanup(curl); return resp; }
    hdr_buf.data[0] = '\0';

    char* method_str = strndup(method.data, method.length);
    curl_easy_setopt(curl, CURLOPT_URL, strndup(url.data, url.length));
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _http_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_buf);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, _http_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &hdr_buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    if (body.data && body.length > 0) {
        char* body_copy = strndup(body.data, body.length);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_copy);
    }

    struct curl_slist* headers = NULL;
    if (extra_headers.data && extra_headers.length > 0) {
        char* hdr_copy = strndup(extra_headers.data, extra_headers.length);
        headers = curl_slist_append(headers, hdr_copy);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        resp.status_code = (int)status;
    }

    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    _cnext_track(body_buf.data);
    resp.body = (CnextString){body_buf.data, body_buf.length};
    if (hdr_buf.data) {
        _cnext_track(hdr_buf.data);
        resp.headers = (CnextString){hdr_buf.data, hdr_buf.length};
    }
    return resp;
}

#endif

static inline HttpResponse http_get_full(CnextString url) {
    return http_request((CnextString){"GET", 3}, url, (CnextString){NULL, 0}, (CnextString){NULL, 0});
}

static inline HttpResponse http_post_full(CnextString url, CnextString body) {
    return http_request((CnextString){"POST", 4}, url, body, (CnextString){NULL, 0});
}

static inline HttpResponse http_put(CnextString url, CnextString body) {
    return http_request((CnextString){"PUT", 3}, url, body, (CnextString){NULL, 0});
}

static inline HttpResponse http_delete(CnextString url) {
    return http_request((CnextString){"DELETE", 6}, url, (CnextString){NULL, 0}, (CnextString){NULL, 0});
}

static inline HttpResponse http_patch(CnextString url, CnextString body) {
    return http_request((CnextString){"PATCH", 5}, url, body, (CnextString){NULL, 0});
}

static inline HttpResponse http_get_with_headers(CnextString url, CnextString headers) {
    return http_request((CnextString){"GET", 3}, url, (CnextString){NULL, 0}, headers);
}

static inline HttpResponse http_post_with_headers(CnextString url, CnextString body, CnextString headers) {
    return http_request((CnextString){"POST", 4}, url, body, headers);
}

static inline CnextString http_status_text(int code) {
    switch (code) {
        case 200: return (CnextString){"OK", 2};
        case 201: return (CnextString){"Created", 7};
        case 204: return (CnextString){"No Content", 10};
        case 301: return (CnextString){"Moved Permanently", 17};
        case 302: return (CnextString){"Found", 5};
        case 304: return (CnextString){"Not Modified", 12};
        case 400: return (CnextString){"Bad Request", 11};
        case 401: return (CnextString){"Unauthorized", 12};
        case 403: return (CnextString){"Forbidden", 9};
        case 404: return (CnextString){"Not Found", 9};
        case 405: return (CnextString){"Method Not Allowed", 18};
        case 409: return (CnextString){"Conflict", 8};
        case 422: return (CnextString){"Unprocessable Entity", 20};
        case 429: return (CnextString){"Too Many Requests", 17};
        case 500: return (CnextString){"Internal Server Error", 21};
        case 502: return (CnextString){"Bad Gateway", 11};
        case 503: return (CnextString){"Service Unavailable", 19};
        default: return (CnextString){"Unknown", 7};
    }
}

#endif
