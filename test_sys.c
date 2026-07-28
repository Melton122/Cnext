#include <stdio.h>
#include <stdlib.h>

int main() {
    /* This mimics what cnext's test runner does */
    char cmd[2048];
    
    /* Test with exact same format as test runner */
    snprintf(cmd, sizeof(cmd), "\"C:\\Users\\melto\\cnext\\cnext.exe\" build \"tests/test_full.cn\" -o \"tests/_test_out.exe\" 2>nul");
    printf("cmd: [%s]\n", cmd);
    int r1 = system(cmd);
    printf("result: %d\n", r1);
    
    /* Now test: what does cnext itself do? Let's look at temp_out.c after */
    int r2 = system("cnext.exe build tests/test_full.cn -o tests/_test_out.exe 2>nul");
    printf("bare result: %d\n", r2);
    
    return 0;
}
