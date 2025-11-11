#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct __attribute__((packed)) {
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    uint32_t magic;
} GraphHeader;

typedef struct __attribute__((packed)) {
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
    printf("Header: nodes=%u/%u conns=%u/%u\n", 
           h->node_count, h->node_cap, h->connection_count, h->connection_cap);
    
    // Calculate offset (nodes come after header)
    size_t node_size = 256; // Node struct size
    Connection *conns = (Connection *)((char *)base + sizeof(GraphHeader) + h->node_cap * node_size);
    
    printf("\nFirst 5 connections:\n");
    for (uint32_t i = 0; i < h->connection_count && i < 5; i++) {
        printf("  [%u] %u → %u (weight=%.2f)\n", i, conns[i].src, conns[i].dst, conns[i].weight);
    }
    
    close(fd);
    return 0;
}
