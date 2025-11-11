/*
 * Inspector for melvin_emergence.c graph structure
 * Shows: nodes, connections, weights, energy
 */

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
    uint8_t is_hub;
    uint8_t hub_level;
    uint16_t in_degree;
    uint16_t out_degree;
    uint8_t is_pattern;
    uint32_t pattern_members[16];
    uint8_t pattern_member_count;
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
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open graph_emergence.mmap\n");
        return 1;
    }
    
    struct stat st;
    fstat(fd, &st);
    
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "ERROR: Cannot mmap file\n");
        return 1;
    }
    
    GraphHeader *header = (GraphHeader *)base;
    Node *nodes = (Node *)((char *)base + sizeof(GraphHeader));
    Connection *connections = (Connection *)((char *)nodes + header->node_cap * sizeof(Node));
    
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║  MELVIN EMERGENCE INSPECTOR           ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    
    printf("📊 GRAPH STATE\n");
    printf("═════════════════════════════════════════\n");
    printf("  Nodes: %u / %u\n", header->node_count, header->node_cap);
    printf("  Connections: %u / %u\n", header->connection_count, header->connection_cap);
    printf("  Tick: %llu\n", (unsigned long long)header->tick);
    printf("\n");
    
    printf("🧠 META-NODES (System Parameters 0-20)\n");
    printf("═════════════════════════════════════════\n");
    printf("  [0]  Learning rate:     %.3f\n", nodes[0].state);
    printf("  [1]  Decay rate:        %.3f\n", nodes[1].state);
    printf("  [2]  Activation thresh: %.3f\n", nodes[2].state);
    printf("  [3]  Input energy:      %.3f\n", nodes[3].state);
    printf("  [4]  Output cost:       %.3f\n", nodes[4].state);
    printf("  [5]  Metabolism cost:   %.3f\n", nodes[5].state);
    printf("\n");
    
    printf("📝 REGULAR NODES (21+)\n");
    printf("═════════════════════════════════════════\n");
    printf("  Total regular nodes: %u\n", header->node_count - 21);
    printf("\n");
    
    printf("  Top 10 nodes by energy:\n");
    for (uint32_t i = 21; i < header->node_count && i < 31; i++) {
        Node *n = &nodes[i];
        char c = (n->learned_output_byte >= 32 && n->learned_output_byte < 127) ? 
                 n->learned_output_byte : '?';
        const char *type = n->is_pattern ? "[PATTERN]" : (n->is_hub ? "[HUB]" : "");
        printf("    Node %3u: outputs='%c' energy=%7.1f in=%u out=%u %s\n",
               i, c, n->energy, n->in_degree, n->out_degree, type);
    }
    
    // Show hubs and patterns
    uint32_t hub_count = 0, pattern_count = 0;
    for (uint32_t i = 21; i < header->node_count; i++) {
        if (nodes[i].is_hub) hub_count++;
        if (nodes[i].is_pattern) pattern_count++;
    }
    printf("\n  Hierarchical organization:\n");
    printf("    Hub nodes: %u\n", hub_count);
    printf("    Pattern nodes: %u\n", pattern_count);
    printf("\n");
    
    printf("🔗 CONNECTIONS (Weights = Memory!)\n");
    printf("═════════════════════════════════════════\n");
    printf("  Total connections: %u\n", header->connection_count);
    printf("\n");
    
    printf("  Top 20 strongest connections:\n");
    
    // Find strongest connections
    typedef struct { uint32_t idx; float weight; } ConnWeight;
    ConnWeight top[20] = {0};
    
    for (uint32_t i = 0; i < header->connection_count; i++) {
        Connection *c = &connections[i];
        for (int t = 0; t < 20; t++) {
            if (c->weight > top[t].weight) {
                // Shift down
                for (int s = 19; s > t; s--) {
                    top[s] = top[s-1];
                }
                top[t].idx = i;
                top[t].weight = c->weight;
                break;
            }
        }
    }
    
    for (int i = 0; i < 20 && top[i].weight > 0.0f; i++) {
        Connection *c = &connections[top[i].idx];
        if (c->src < header->node_count && c->dst < header->node_count) {
            char src_byte = (c->src >= 21) ? nodes[c->src].learned_output_byte : '?';
            char dst_byte = (c->dst >= 21) ? nodes[c->dst].learned_output_byte : '?';
            
            if (src_byte >= 32 && src_byte < 127 && dst_byte >= 32 && dst_byte < 127) {
                printf("    %3u → %3u: '%c' → '%c'  weight=%.2f\n",
                       c->src, c->dst, src_byte, dst_byte, c->weight);
            } else {
                printf("    %3u → %3u: weight=%.2f\n", c->src, c->dst, c->weight);
            }
        }
    }
    
    printf("\n");
    printf("═════════════════════════════════════════\n");
    printf("🧬 Information lives in CONNECTION WEIGHTS!\n");
    printf("   The stronger the weight, the stronger\n");
    printf("   the learned association.\n");
    printf("═════════════════════════════════════════\n\n");
    
    munmap(base, st.st_size);
    close(fd);
    
    return 0;
}

