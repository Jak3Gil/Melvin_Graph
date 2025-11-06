#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    uint32_t node_count, node_cap, next_node_id;
    uint32_t edge_count, edge_cap;
    uint32_t module_count, module_cap;
    uint64_t tick;
    uint32_t magic;
} Header;

int main() {
    int fd = open("graph.mmap", O_RDONLY);
    if (fd < 0) { printf("Can't open graph.mmap\n"); return 1; }
    
    struct stat st;
    fstat(fd, &st);
    
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    Header *h = (Header*)base;
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  GRAPH INSPECTION                                            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    printf("📊 STATS:\n");
    printf("   Nodes: %u / %u (%.1f%% used)\n", h->node_count, h->node_cap, 
           100.0f * h->node_count / h->node_cap);
    printf("   Edges: %u / %u (%.1f%% used)\n", h->edge_count, h->edge_cap,
           100.0f * h->edge_count / h->edge_cap);
    printf("   Tick: %llu\n", (unsigned long long)h->tick);
    printf("   File size: %lld bytes (%.1f KB)\n", (long long)st.st_size, st.st_size/1024.0);
    
    munmap(base, st.st_size);
    close(fd);
    return 0;
}
