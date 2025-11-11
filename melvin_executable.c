/*
 * EXECUTABLE NODES - Data Writes Code
 * 
 * Nodes aren't just data - they're EXECUTABLE RULES
 * Input data creates PROGRAMS that run
 * Programs create more programs → EXPONENTIAL
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* NODE TYPES */
enum NodeType {
    NODE_DATA,        // Simple data (cat, sat)
    NODE_RULE,        // Executable rule (if X then Y)
    NODE_PATTERN,     // Meta-pattern (cluster of nodes)
    NODE_GENERATOR    // Creates new nodes!
};

/* EXECUTABLE NODE */
typedef struct {
    enum NodeType type;
    
    // For DATA nodes
    uint8_t token[64];
    uint8_t token_len;
    
    // For RULE nodes (executable!)
    uint32_t rule_inputs[16];      // If these nodes active
    uint8_t rule_input_count;
    uint32_t rule_outputs[16];     // Activate these nodes
    uint8_t rule_output_count;
    float rule_strength;           // How strong is this rule
    
    // For PATTERN nodes (clusters)
    uint32_t pattern_members[32];  // Nodes in this cluster
    uint8_t pattern_member_count;
    float coherence_score;         // How tightly connected
    
    // For GENERATOR nodes (create new nodes!)
    uint32_t template_pattern;     // Based on this pattern
    uint8_t can_instantiate;       // Can create instances
    
    // Runtime state
    float state;
    float energy;
    uint32_t times_executed;       // For rule nodes
} Node;

/* No separate Connection struct! Connections ARE encoded in rule nodes! */

typedef struct {
    Node *nodes;
    uint32_t node_count, node_cap;
    uint64_t tick;
} Graph;

Graph g;
void *mmap_base = NULL;
int debug = 0;

/* Create data node */
uint32_t create_data_node(uint8_t *token, uint32_t len) {
    // Try to reuse
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type == NODE_DATA && g.nodes[i].token_len == len) {
            int match = 1;
            for (uint32_t b = 0; b < len; b++) {
                if (g.nodes[i].token[b] != token[b]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                if (debug) printf("[REUSE] %u\n", i);
                return i;
            }
        }
    }
    
    // Create new
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    g.nodes[id].type = NODE_DATA;
    g.nodes[id].token_len = len;
    for (uint32_t b = 0; b < len && b < 64; b++) {
        g.nodes[id].token[b] = token[b];
    }
    g.nodes[id].energy = 100.0f;
    
    if (debug) {
        printf("[CREATE DATA] %u: ", id);
        for (uint32_t b = 0; b < len; b++) printf("%c", token[b]);
        printf("\n");
    }
    
    return id;
}

/* Create RULE node (executable!) */
uint32_t create_rule_node(uint32_t *inputs, uint8_t input_count, 
                          uint32_t *outputs, uint8_t output_count) {
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    g.nodes[id].type = NODE_RULE;
    g.nodes[id].rule_input_count = input_count;
    g.nodes[id].rule_output_count = output_count;
    
    for (uint8_t i = 0; i < input_count && i < 16; i++) {
        g.nodes[id].rule_inputs[i] = inputs[i];
    }
    for (uint8_t i = 0; i < output_count && i < 16; i++) {
        g.nodes[id].rule_outputs[i] = outputs[i];
    }
    
    g.nodes[id].rule_strength = 1.0f;
    g.nodes[id].energy = 100.0f;
    
    if (debug) {
        printf("[CREATE RULE] %u: IF [", id);
        for (uint8_t i = 0; i < input_count; i++) printf("%u,", inputs[i]);
        printf("] THEN [");
        for (uint8_t i = 0; i < output_count; i++) printf("%u,", outputs[i]);
        printf("]\n");
    }
    
    return id;
}

/* EXECUTE all rule nodes! */
void execute_rules() {
    for (uint32_t i = 0; i < g.node_count; i++) {
        Node *rule = &g.nodes[i];
        
        if (rule->type != NODE_RULE) continue;
        if (rule->energy <= 0.0f) continue;
        
        // Check if ALL inputs are active
        int all_inputs_active = 1;
        for (uint8_t j = 0; j < rule->rule_input_count; j++) {
            uint32_t input_id = rule->rule_inputs[j];
            if (input_id >= g.node_count || g.nodes[input_id].state < 0.5f) {
                all_inputs_active = 0;
                break;
            }
        }
        
        // EXECUTE RULE!
        if (all_inputs_active) {
            for (uint8_t j = 0; j < rule->rule_output_count; j++) {
                uint32_t output_id = rule->rule_outputs[j];
                if (output_id < g.node_count) {
                    g.nodes[output_id].state = 1.0f;  // ACTIVATE!
                }
            }
            
            rule->times_executed++;
            rule->rule_strength += 0.1f;  // Rule gets stronger with use
            
            if (debug && rule->times_executed < 3) {
                printf("[EXECUTE RULE] %u fired! (executed %u times)\n", 
                       i, rule->times_executed);
            }
        }
    }
}

/* Process input - creates DATA nodes AND RULE nodes! */
void sense_input(uint8_t *bytes, uint32_t len) {
    uint32_t word_nodes[128];
    uint32_t word_count = 0;
    uint32_t word_start = 0;
    
    // Parse words and create DATA nodes
    for (uint32_t i = 0; i <= len && word_count < 128; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || i == len) {
            if (i > word_start) {
                uint32_t node_id = create_data_node(&bytes[word_start], i - word_start);
                if (node_id != UINT32_MAX) {
                    word_nodes[word_count++] = node_id;
                    g.nodes[node_id].state = 1.0f;
                }
            }
            word_start = i + 1;
        }
    }
    
    // DATA WRITES CODE: Create RULE nodes from sequence!
    if (word_count >= 2) {
        // Create "if A then B" rules for each pair
        for (uint32_t i = 0; i < word_count - 1; i++) {
            uint32_t inputs[1] = {word_nodes[i]};
            uint32_t outputs[1] = {word_nodes[i+1]};
            create_rule_node(inputs, 1, outputs, 1);
        }
        
        // Create "if A and B then C" rules for triplets
        if (word_count >= 3) {
            for (uint32_t i = 0; i < word_count - 2; i++) {
                uint32_t inputs[2] = {word_nodes[i], word_nodes[i+1]};
                uint32_t outputs[1] = {word_nodes[i+2]};
                create_rule_node(inputs, 2, outputs, 1);
            }
        }
    }
}

/* Find coherent clusters and create PATTERN nodes */
void detect_patterns() {
    if (g.tick % 10 != 0) return;  // Every 10 ticks
    
    // Find tightly connected groups of data nodes
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type != NODE_DATA) continue;
        
        // Find all nodes this one frequently co-activates with
        uint32_t cluster[32];
        uint8_t cluster_size = 0;
        cluster[cluster_size++] = i;
        
        // Check which rules involve this node
        for (uint32_t r = 0; r < g.node_count; r++) {
            Node *rule = &g.nodes[r];
            if (rule->type != NODE_RULE) continue;
            if (rule->times_executed < 5) continue;  // Only strong rules
            
            // Does this rule involve node i?
            for (uint8_t j = 0; j < rule->rule_input_count; j++) {
                if (rule->rule_inputs[j] == i) {
                    // Add outputs to cluster
                    for (uint8_t k = 0; k < rule->rule_output_count; k++) {
                        uint32_t out = rule->rule_outputs[k];
                        if (cluster_size < 32) {
                            cluster[cluster_size++] = out;
                        }
                    }
                }
            }
        }
        
        // If cluster has 3+ nodes, create PATTERN node!
        if (cluster_size >= 3 && g.node_count < g.node_cap) {
            uint32_t pattern_id = g.node_count++;
            memset(&g.nodes[pattern_id], 0, sizeof(Node));
            g.nodes[pattern_id].type = NODE_PATTERN;
            g.nodes[pattern_id].pattern_member_count = cluster_size;
            
            for (uint8_t j = 0; j < cluster_size && j < 32; j++) {
                g.nodes[pattern_id].pattern_members[j] = cluster[j];
            }
            
            g.nodes[pattern_id].energy = 200.0f;  // Patterns get more energy
            
            if (debug) {
                printf("[CREATE PATTERN] %u contains %u nodes\n", 
                       pattern_id, cluster_size);
            }
            
            break;  // One pattern per cycle
        }
    }
}

/* Output by executing rules */
void emit_output() {
    // Find all active data nodes
    uint8_t output[1024];
    uint32_t output_len = 0;
    
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type == NODE_DATA && g.nodes[i].state > 0.5f) {
            for (uint32_t b = 0; b < g.nodes[i].token_len; b++) {
                output[output_len++] = g.nodes[i].token[b];
            }
            output[output_len++] = ' ';
        }
    }
    
    if (output_len > 0) {
        write(STDOUT_FILENO, output, output_len);
        write(STDOUT_FILENO, "\n", 1);
    }
}

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    g.node_cap = 10000;  // Larger for rules + patterns + generators
    
    size_t size = sizeof(uint32_t) + g.node_cap * sizeof(Node);
    int fd = open("graph_exec.mmap", O_RDWR | O_CREAT, 0644);
    struct stat st;
    int exists = (fstat(fd, &st) == 0 && st.st_size > 0);
    
    if (!exists) ftruncate(fd, size);
    
    mmap_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint32_t *header = (uint32_t *)mmap_base;
    g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t));
    
    if (exists && header[0] > 0) {
        g.node_count = header[0];
        if (debug) printf("[RESTORE] %u nodes (data + rules + patterns)\n", g.node_count);
    }
    
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    
    uint8_t input[4096];
    int idle = 0;
    
    while (idle < 10) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        
        if (n > 0) {
            sense_input(input, n);  // Creates data + rule nodes!
            idle = 0;
        } else {
            idle++;
            usleep(10000);
        }
        
        // EXECUTE all rule nodes (they run as programs!)
        for (int hop = 0; hop < 5; hop++) {
            execute_rules();
        }
        
        // Detect patterns (creates pattern nodes)
        detect_patterns();
        
        emit_output();
        
        g.tick++;
    }
    
    header[0] = g.node_count;
    msync(mmap_base, size, MS_SYNC);
    munmap(mmap_base, size);
    close(fd);
    
    if (debug) {
        uint32_t data_count = 0, rule_count = 0, pattern_count = 0;
        for (uint32_t i = 0; i < g.node_count; i++) {
            switch(g.nodes[i].type) {
                case NODE_DATA: data_count++; break;
                case NODE_RULE: rule_count++; break;
                case NODE_PATTERN: pattern_count++; break;
                default: break;
            }
        }
        printf("\n[SUMMARY] %u data, %u rules, %u patterns = %u total nodes\n",
               data_count, rule_count, pattern_count, g.node_count);
    }
    
    return 0;
}

