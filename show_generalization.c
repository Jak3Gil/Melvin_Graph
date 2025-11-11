#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    uint8_t token[16];
    uint8_t token_len;
    float state;
    uint32_t last_active_tick;
    uint32_t activation_sequence;
    float energy, threshold;
    uint8_t is_hub, hub_level;
    uint16_t in_degree, out_degree;
    uint8_t is_pattern;
    uint32_t pattern_members[16];
    uint8_t pattern_member_count;
    uint32_t frequency;
    uint32_t predicted_next_node;
    float prediction_confidence;
    uint32_t predictions_correct, predictions_wrong;
} Node;

typedef struct {
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    uint32_t magic;
} GraphHeader;

int main() {
    int fd = open("graph_emergence.mmap", O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    GraphHeader *h = (GraphHeader *)base;
    Node *nodes = (Node *)((char *)base + sizeof(GraphHeader));
    
    printf("\n=== GENERALIZED PATTERNS (in_degree > 1) ===\n");
    for (uint32_t i = 21; i < h->node_count && i < 100; i++) {
        Node *n = &nodes[i];
        if (n->in_degree > 1 && n->token_len >= 2) {
            printf("Node %u: \"", i);
            for (uint8_t b = 0; b < n->token_len; b++) {
                printf("%c", n->token[b]);
            }
            printf("\" in_degree=%u (used in %u contexts!)\n", n->in_degree, n->in_degree);
        }
    }
    
    close(fd);
    return 0;
}
