#include <stdio.h>
#include <sys/mman.h>
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
    uint32_t pattern_members[32];
    uint8_t pattern_member_count;
    float coherence_score;
    float state;
    float energy;
    uint32_t frequency;
} Node;

int main() {
    int fd = open("graph.mmap", O_RDONLY);
    if (fd < 0) return 1;
    
    uint32_t *header = mmap(NULL, 4096 + sizeof(Node) * 10000, PROT_READ, MAP_SHARED, fd, 0);
    uint32_t count = header[0];
    Node *nodes = (Node*)((char*)header + sizeof(uint32_t));
    
    printf("GRAPH STRUCTURE:\n\n");
    
    printf("DATA NODES (vertices):\n");
    for (uint32_t i = 0; i < count; i++) {
        if (nodes[i].type == NODE_DATA && nodes[i].token_len > 0) {
            printf("  [%u] '%.*s'\n", i, nodes[i].token_len, nodes[i].token);
        }
    }
    
    printf("\nRULE NODES (edges = executable connections):\n");
    for (uint32_t i = 0; i < count; i++) {
        if (nodes[i].type == NODE_RULE) {
            printf("  [%u] RULE: ", i);
            for (int j = 0; j < nodes[i].rule_input_count; j++) {
                uint32_t inp = nodes[i].rule_inputs[j];
                printf("'%.*s'", nodes[inp].token_len, nodes[inp].token);
                if (j < nodes[i].rule_input_count - 1) printf(" + ");
            }
            printf(" → ");
            for (int j = 0; j < nodes[i].rule_output_count; j++) {
                uint32_t out = nodes[i].rule_outputs[j];
                printf("'%.*s'", nodes[out].token_len, nodes[out].token);
                if (j < nodes[i].rule_output_count - 1) printf(" + ");
            }
            printf("\n");
        }
    }
    
    printf("\nVISUAL:\n");
    printf("  'cat' ──[RULE]──> 'sat'\n");
    printf("  'dog' ──[RULE]──> 'ran'\n");
    printf("\nRules ARE the edges!\n");
    
    munmap(header, 4096 + sizeof(Node) * 10000);
    close(fd);
    return 0;
}
