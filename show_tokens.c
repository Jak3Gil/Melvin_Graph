#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    float state, energy, threshold;
    uint32_t last_active_tick, activation_sequence;
    uint8_t is_hub, hub_level;
    uint16_t in_degree, out_degree;
    uint8_t is_pattern;
    uint32_t pattern_members[16];
    uint8_t pattern_member_count;
    uint8_t token[16];
    uint8_t token_len;
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
    
    GraphHeader *header = (GraphHeader *)base;
    Node *nodes = (Node *)((char *)base + sizeof(GraphHeader));
    
    printf("\n═══ ALL TOKENS (sorted by frequency) ═══\n");
    for (uint32_t i = 21; i < header->node_count && i < 50; i++) {
        Node *n = &nodes[i];
        if (n->token_len == 0) continue;
        
        printf("Node %3u [freq=%4u]: \"", i, n->frequency);
        for (uint8_t b = 0; b < n->token_len; b++) {
            printf("%c", (n->token[b] >= 32 && n->token[b] < 127) ? n->token[b] : '?');
        }
        printf("\" (%u bytes, energy=%.1f)\n", n->token_len, n->energy);
    }
    
    munmap(base, st.st_size);
    close(fd);
    return 0;
}
