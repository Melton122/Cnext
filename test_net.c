#include <stdio.h>
#include "include/net.h"

int main() {
    printf("Fetching...");
    char* content = http_get("http://example.com");
    if (content) printf("Success\n");
    return 0;
}
