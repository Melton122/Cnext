#include <stdio.h>
#include <stdbool.h>
#include <string.h>

static bool is_valid_interpolation(const char* start, const char* end) {
    const char* c = start;
    bool found_close = false;
    while (c < end) {
        if (*c == '}') {
            found_close = true;
            break;
        }
        if (*c == '"' || *c == '\\') {
            printf("  -> Rejected at char: 0x%02X ('%c')\n", (unsigned char)*c, *c);
            return false;
        }
        printf("  -> Scanning char: 0x%02X ('%c')\n", (unsigned char)*c, *c);
        c++;
    }
    if (!found_close) return false;
    if (c == start) return false;
    return true;
}

int main() {
    // Raw string as it appears in source: "{\"name\":\"Melton\",...}"
    const char* raw = "\"{\\\"name\\\":\\\"Melton\\\",\\\"age\\\":19}\"";
    printf("Raw string: %s\n", raw);
    int length = strlen(raw);
    const char* start = raw;
    const char* p = start + 1;
    const char* end = start + length - 1;
    
    printf("p[0] = 0x%02X ('%c')\n", (unsigned char)*p, *p);
    printf("p[1] = 0x%02X ('%c')\n", (unsigned char)*(p+1), *(p+1));
    
    for (const char* c = p; c < end; c++) {
        if (*c == '{') {
            printf("Found { at offset %ld\n", c - raw);
            printf("Next char: 0x%02X ('%c')\n", (unsigned char)*(c+1), *(c+1));
            bool v = is_valid_interpolation(c + 1, end);
            printf("is_valid_interpolation: %s\n", v ? "TRUE" : "FALSE");
        }
    }
    return 0;
}
