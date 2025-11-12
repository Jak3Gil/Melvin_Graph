// Quick tool to show all node activations after a query
// Compile: gcc -o show_activations show_activations.c
// Use after melvin runs to see what actually activated

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct __attribute__((packed)) {
    uint8_t token[16];
    float activation;
    uint16_t token_len;
    uint16_t frequency;
} Node;

int main() {
    int fd = open("graph.mmap", O_RDONLY);
    if (fd < 0) {
        printf("No graph.mmap found\n");
        return 1;
    }
    
    struct stat st;
    fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    uint32_t *header = (uint32_t *)base;
    uint32_t node_count = header[0];
    
    Node *nodes = (Node *)((char *)base + sizeof(uint32_t) * 4);
    
    printf("All node activations (non-zero):\n\n");
    
    for (uint32_t i = 0; i < node_count; i++) {
        if (nodes[i].activation > 0.001f) {
            printf("  %.*s: %.6f\n", 
                   nodes[i].token_len < 16 ? nodes[i].token_len : 16,
                   nodes[i].token,
                   nodes[i].activation);
        }
    }
    
    munmap(base, st.st_size);
    close(fd);
    return 0;
}

