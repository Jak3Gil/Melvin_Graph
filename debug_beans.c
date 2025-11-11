// Quick test to see what's happening
#include <stdio.h>
#include <stdint.h>
#include <string.h>

float token_similarity(char *a, int alen, char *b, int blen) {
    int shared = 0;
    int total = (alen > blen) ? alen : blen;
    for (int i = 0; i < alen && i < blen; i++) {
        if (a[i] == b[i]) shared++;
    }
    return (float)shared / (float)total;
}

int main() {
    printf("Checking all combinations:\n\n");
    
    char *words[] = {"beans", "cat", "bat", "mat", "sat"};
    int lens[] = {5, 3, 3, 3, 3};
    
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (i == j) continue;
            float sim = token_similarity(words[i], lens[i], words[j], lens[j]);
            printf("'%s' vs '%s': %.2f ", words[i], words[j], sim);
            if (sim > 0.5f) printf(" ← MATCH!");
            printf("\n");
        }
        printf("\n");
    }
    
    return 0;
}
