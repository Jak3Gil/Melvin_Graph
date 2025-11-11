#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    uint8_t token[64];
    uint8_t token_len;
    float state, energy;
} Node;

typedef struct {
    uint32_t src, dst;
    float weight;
    int type;
    int8_t position_offset;
    float similarity_score;
    uint8_t shared_position;
    uint8_t level_difference;
} Connection;

int main() {
    int fd = open("graph_edges.mmap", O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    uint32_t node_cap = 1000;
    Node *nodes = (Node *)base;
    Connection *conns = (Connection *)((char *)base + node_cap * sizeof(Node));
    
    printf("\n=== NODES ===\n");
    for (uint32_t i = 0; i < 10; i++) {
        if (nodes[i].token_len == 0) break;
        printf("Node %u: \"", i);
        for (uint32_t b = 0; b < nodes[i].token_len; b++) {
            printf("%c", nodes[i].token[b]);
        }
        printf("\" energy=%.1f\n", nodes[i].energy);
    }
    
    printf("\n=== EDGES BY TYPE ===\n");
    const char *types[] = {"Sequential", "Similar", "Positional", "Abstraction"};
    
    for (int t = 0; t < 3; t++) {
        printf("\n%s edges:\n", types[t]);
        int count = 0;
        for (uint32_t i = 0; i < 100; i++) {
            if (conns[i].type == t && conns[i].src < 10 && conns[i].dst < 10) {
                printf("  %u → %u ", conns[i].src, conns[i].dst);
                if (t == 0) printf("(offset=%d)", conns[i].position_offset);
                if (t == 1) printf("(sim=%.2f)", conns[i].similarity_score);
                if (t == 2) printf("(pos=%u)", conns[i].shared_position);
                printf("\n");
                count++;
                if (count >= 10) break;
            }
        }
    }
    
    close(fd);
    return 0;
}
