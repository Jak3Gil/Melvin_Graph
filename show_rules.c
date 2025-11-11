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
    uint8_t is_required;
    uint8_t times_satisfied;
    uint8_t times_violated;
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
    
    GraphHeader *header = (GraphHeader *)base;
    Node *nodes = (Node *)((char *)base + sizeof(GraphHeader));
    Connection *connections = (Connection *)((char *)nodes + header->node_cap * sizeof(Node));
    
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║  LEARNED RULES (Required Connections) ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    
    uint32_t rule_count = 0;
    
    for (uint32_t i = 0; i < header->connection_count; i++) {
        Connection *c = &connections[i];
        
        if (!c->is_required) continue;
        
        rule_count++;
        
        if (c->src < header->node_count && c->dst < header->node_count) {
            char src_b = (c->src >= 21) ? nodes[c->src].learned_output_byte : '?';
            char dst_b = (c->dst >= 21) ? nodes[c->dst].learned_output_byte : '?';
            
            if (src_b >= 32 && src_b < 127 && dst_b >= 32 && dst_b < 127) {
                printf("  RULE: '%c' MUST→ '%c'  (weight=%.1f, satisfied=%u, violated=%u)\n",
                       src_b, dst_b, c->weight, c->times_satisfied, c->times_violated);
            }
        }
    }
    
    printf("\n  Total rules learned: %u\n", rule_count);
    printf("  Total connections: %u\n", header->connection_count);
    printf("  Rules / Connections: %.1f%%\n\n", 
           (float)rule_count * 100.0f / header->connection_count);
    
    printf("═══════════════════════════════════════\n");
    printf("These are CONSTRAINTS, not suggestions.\n");
    printf("Output must satisfy these rules to be\n");
    printf("considered complete and get rewarded.\n");
    printf("═══════════════════════════════════════\n\n");
    
    munmap(base, st.st_size);
    close(fd);
    return 0;
}

