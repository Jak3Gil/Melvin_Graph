#include <stdio.h>
#include <string.h>

float sim(char *a, char *b) {
    int shared = 0;
    int len_a = strlen(a);
    int len_b = strlen(b);
    int total = (len_a > len_b) ? len_a : len_b;
    
    for (int i = 0; i < len_a && i < len_b; i++) {
        if (a[i] == b[i]) shared++;
    }
    
    return (float)shared / (float)total;
}

int main() {
    printf("Similarity calculations:\n\n");
    printf("beans vs cat: %.2f\n", sim("beans", "cat"));
    printf("beans vs bat: %.2f\n", sim("beans", "bat"));
    printf("beans vs mat: %.2f\n", sim("beans", "mat"));
    printf("\n");
    printf("rat vs cat: %.2f\n", sim("rat", "cat"));
    printf("rat vs bat: %.2f\n", sim("rat", "bat"));
    printf("\n");
    printf("Threshold: 0.50 (need > 0.50 to generalize)\n");
    return 0;
}
