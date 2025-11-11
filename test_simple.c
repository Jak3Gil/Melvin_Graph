#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t token[64];
    uint8_t token_len;
} TestNode;

int main() {
    TestNode n;
    memset(&n, 0, sizeof(TestNode));
    
    char *test = "cat";
    uint32_t len = strlen(test);
    
    for (uint8_t i = 0; i < len && i < 64; i++) {
        n.token[i] = test[i];
    }
    n.token_len = len;
    
    printf("Token: '%.*s' (len=%u)\n", n.token_len, n.token, n.token_len);
    
    return 0;
}
