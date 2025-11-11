#include <stdio.h>
#include <stdint.h>

int main() {
    const uint8_t *bytes = (uint8_t*)"cat";
    uint32_t len = 3;
    
    printf("Testing n-gram generation for 'cat':\n");
    
    for (uint32_t start = 0; start < len; start++) {
        printf("\nstart=%u:\n", start);
        for (uint32_t ngram_len = 1; ngram_len <= 10 && start + ngram_len <= len; ngram_len++) {
            printf("  ngram_len=%u: \"", ngram_len);
            for (uint32_t b = 0; b < ngram_len; b++) {
                printf("%c", bytes[start + b]);
            }
            printf("\"\n");
        }
    }
    
    return 0;
}
