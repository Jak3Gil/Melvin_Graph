#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    uint32_t src, dst;
    float weight;
    uint8_t is_rule, rule_strength, times_satisfied, times_violated;
    uint8_t is_implication, is_inhibitory;
} Connection;

typedef struct {
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    uint32_t magic;
} GraphHeader;

int main() {
    int fd = open("graph_emergence.mmap", O_RDONLY);
    if (fd < 0) { printf("No graph file\n"); return 1; }
    
    struct stat st;
    fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    GraphHeader *h = (GraphHeader *)base;
    Connection *conns = (Connection *)((char *)base + sizeof(GraphHeader) + h->node_cap * 256);
    
    printf("Total connections: %u\n", h->connection_count);
    for (uint32_t i = 0; i < h->connection_count && i < 20; i++) {
        printf("  %u → %u (weight=%.2f)\n", conns[i].src, conns[i].dst, conns[i].weight);
    }
    
    close(fd);
    return 0;
}
