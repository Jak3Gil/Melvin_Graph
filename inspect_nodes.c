#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

typedef enum { NODE_DATA, NODE_RULE, NODE_PATTERN, NODE_GENERATOR } NodeType;

typedef struct {
    NodeType type;
    uint8_t token[64];
    uint8_t token_len;
    uint32_t rule_inputs[16];
    uint8_t rule_input_count;
    uint32_t rule_outputs[16];
    uint8_t rule_output_count;
    float rule_strength;
    uint32_t times_executed;
    uint32_t pattern_members[16];
    uint8_t pattern_member_count;
    float coherence_score;
    uint32_t generator_template_inputs[8];
    uint32_t generator_template_outputs[8];
    uint8_t generator_template_input_count;
    uint8_t generator_template_output_count;
    float state;
    float energy;
    uint32_t frequency;
    uint32_t last_active_tick;
} Node;

int main() {
    int fd = open("graph.mmap", O_RDONLY);
    if (fd < 0) { printf("No graph\n"); return 1; }
    
    uint32_t *header = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    uint32_t node_count = header[0];
    
    Node *nodes = (Node*)((char*)header + 4096);
    
    printf("Nodes in graph:\n");
    for (uint32_t i = 0; i < node_count; i++) {
        if (nodes[i].type == NODE_DATA) {
            printf("  [%u] DATA: '%.*s'\n", i, nodes[i].token_len, nodes[i].token);
        } else if (nodes[i].type == NODE_RULE) {
            printf("  [%u] RULE: %u → %u\n", i, 
                   nodes[i].rule_inputs[0], nodes[i].rule_outputs[0]);
        }
    }
    
    munmap(header, 4096);
    close(fd);
    return 0;
}
