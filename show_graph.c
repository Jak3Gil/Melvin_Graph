#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct __attribute__((packed)) {
    uint8_t token[16];
    uint16_t token_len;
    int32_t value;
} Node;

typedef struct __attribute__((packed)) {
    uint32_t from;
    uint32_t to;
    uint8_t weight;
} Edge;

int main() {
    int fd = open("melvin.mmap", O_RDONLY);
    if (fd < 0) {
        printf("No graph\n");
        return 1;
    }
    
    struct stat st;
    fstat(fd, &st);
    
    void *mem = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mem == MAP_FAILED) { close(fd); return 1; }
    
    uint32_t *header = (uint32_t *)mem;
    uint32_t node_count = header[0];
    uint32_t edge_count = header[2];
    
    size_t hsize = sizeof(uint32_t) * 4;
    Node *nodes = (Node *)((char *)mem + hsize);
    Edge *edges = (Edge *)((char *)mem + hsize + node_count * sizeof(Node));
    
    printf("GRAPH: %u nodes, %u edges\n\n", node_count, edge_count);
    
    printf("NODES:\n");
    for (uint32_t i = 0; i < node_count && i < 20; i++) {
        printf("%2u: \"%.*s\"", i, nodes[i].token_len, nodes[i].token);
        if (nodes[i].value != 0) printf(" (val=%d)", nodes[i].value);
        printf("\n");
    }
    if (node_count > 20) printf("... (%u more)\n", node_count - 20);
    
    printf("\nEDGES:\n");
    for (uint32_t i = 0; i < edge_count && i < 20; i++) {
        printf("%2u→%2u", edges[i].from, edges[i].to);
        if (edges[i].from < node_count && edges[i].to < node_count) {
            printf("  \"%.*s\" → \"%.*s\"",
                   nodes[edges[i].from].token_len, nodes[edges[i].from].token,
                   nodes[edges[i].to].token_len, nodes[edges[i].to].token);
        }
        printf("\n");
    }
    if (edge_count > 20) printf("... (%u more)\n", edge_count - 20);
    
    munmap(mem, st.st_size);
    close(fd);
    return 0;
}
