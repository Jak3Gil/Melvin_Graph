#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    uint32_t node_count, node_cap, connection_count, connection_cap;
    uint64_t tick;
    uint32_t magic;
} GraphHeader;

typedef struct {
    uint32_t src, dst;
    float weight;
} Connection;

int main() {
    int fd = open("graph_minimal.mmap", O_RDONLY);
    struct stat st;
    fstat(fd, &st);
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    
    GraphHeader *h = (GraphHeader *)base;
    printf("Nodes: %u, Connections: %u\n", h->node_count, h->connection_count);
    
    close(fd);
    return 0;
}
