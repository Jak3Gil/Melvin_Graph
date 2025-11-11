#include <stdio.h>

int main() {
    char *trained[] = {"cat", "bat", "mat"};
    char *tests[] = {"rat", "beans"};
    
    printf("\nSimilarity analysis:\n\n");
    
    for (int t = 0; t < 2; t++) {
        printf("'%s' compared to training:\n", tests[t]);
        
        for (int i = 0; i < 3; i++) {
            int shared = 0;
            int len_test = strlen(tests[t]);
            int len_train = strlen(trained[i]);
            int total = (len_test > len_train) ? len_test : len_train;
            
            for (int j = 0; j < len_test && j < len_train; j++) {
                if (tests[t][j] == trained[i][j]) shared++;
            }
            
            float sim = (float)shared / (float)total;
            printf("  vs '%s': %.2f ", trained[i], sim);
            if (sim > 0.5f) printf("← WILL GENERALIZE!");
            printf("\n");
        }
        printf("\n");
    }
    
    return 0;
}
