#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    float state;
    float energy;
    float threshold;
    uint32_t last_active_tick;
    uint32_t activation_sequence;
    float byte_correlation[256];
    uint8_t learned_output_byte;
} Node;

typedef struct {
    uint32_t src;
    uint32_t dst;
    float weight;
} Connection;

typedef struct {
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t connection_count;
    uint32_t connection_cap;
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
    Connection *connections = (Connection *)((char *)nodes + header->node_cap * sizeof(Node));
    
    printf("All %u connections:\n", header->connection_count);
    for (uint32_t i = 0; i < header->connection_count; i++) {
        Connection *c = &connections[i];
        char src_b = (c->src >= 21 && c->src < header->node_count) ? nodes[c->src].learned_output_byte : '?';
        char dst_b = (c->dst >= 21 && c->dst < header->node_count) ? nodes[c->dst].learned_output_byte : '?';
        printf("  %3u → %3u: '%c' → '%c'  w=%.2f\n", c->src, c->dst, src_b, dst_b, c->weight);
    }
    
    munmap(base, st.st_size);
    close(fd);
    return 0;
}
