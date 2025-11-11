#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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

typedef struct {
    uint32_t src, dst;
    float weight;
    uint8_t is_rule, rule_strength, times_satisfied, times_violated;
    uint8_t is_implication, is_inhibitory;
} Connection;

int main() {
    int fd = open("graph_emergence.mmap", O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    GraphHeader *h = (GraphHeader *)base;
    Node *nodes = (Node *)((char *)base + sizeof(GraphHeader));
    Connection *conns = (Connection *)((char *)nodes + h->node_cap * sizeof(Node));
    
    printf("sizeof(Node)=%zu, sizeof(Connection)=%zu\n", sizeof(Node), sizeof(Connection));
    printf("Connections: %u/%u\n", h->connection_count, h->connection_cap);
    
    for (uint32_t i = 0; i < h->connection_count && i < 10; i++) {
        printf("  [%u] %u → %u (weight=%.2f)\n", i, conns[i].src, conns[i].dst, conns[i].weight);
    }
    
    close(fd);
    return 0;
}
