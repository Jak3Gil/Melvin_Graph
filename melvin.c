/*
 * MELVIN - A Coding Language in Nodes and Edges
 * 
 * Data writes code. Rules create rules. Intelligence emerges.
 * 
 * Core Concepts:
 * - Variable-length tokens (no cat/car confusion)
 * - Executable rule nodes (data creates programs)
 * - Similarity-based generalization (transfer learning)
 * - Position-aware structure (abstract patterns)
 * - Coherence = generalization (unified metric)
 * - Network of networks (recursive emergence)
 * 
 * Intelligence = pattern reuse (unsupervised)
 * Learning = exponential (rules breed rules)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* ========================================================================
 * CORE STRUCTURES
 * ======================================================================== */

typedef enum {
    NODE_DATA,        // Token/data (cat, sat, 1, +)
    NODE_RULE,        // Executable rule (if cat then sat)
    NODE_PATTERN,     // Meta-pattern (cluster)
    NODE_GENERATOR    // Creates new rules
} NodeType;

typedef struct {
    NodeType type;
    
    // DATA nodes
    uint8_t token[64];
    uint8_t token_len;
    
    // RULE nodes (executable!)
    uint32_t rule_inputs[16];
    uint8_t rule_input_count;
    uint32_t rule_outputs[16];
    uint8_t rule_output_count;
    float rule_strength;         // COMPETITION: strength = usage
    uint32_t times_executed;
    uint32_t inhibit_outputs[8]; // INHIBITION: nodes to suppress
    uint8_t inhibit_count;
    
    // PATTERN nodes (clusters)
    uint32_t pattern_members[32];
    uint8_t pattern_member_count;
    float coherence_score;
    
    // Runtime state
    float state;
    float energy;
    uint32_t frequency;
} Node;

typedef struct {
    Node *nodes;
    uint32_t node_count, node_cap;
    uint64_t tick;
    
    // Intelligence metrics
    uint32_t patterns_reused;
    uint32_t patterns_created;
    
    // Dynamic parameters (adapt over time!)
    float learning_rate;
    float similarity_threshold;
} Graph;

Graph g;
void *mmap_base = NULL;
int debug = 0;

/* ========================================================================
 * UTILITIES
 * ======================================================================== */

float token_similarity(Node *a, Node *b) {
    if (a->type != NODE_DATA || b->type != NODE_DATA) return 0.0f;
    if (a->token_len == 0 || b->token_len == 0) return 0.0f;
    
    uint32_t shared = 0;
    uint32_t total = (a->token_len > b->token_len) ? a->token_len : b->token_len;
    
    for (uint32_t i = 0; i < a->token_len && i < b->token_len; i++) {
        if (a->token[i] == b->token[i]) shared++;
    }
    
    return (total > 0) ? (float)shared / (float)total : 0.0f;
}

/* ========================================================================
 * NODE CREATION
 * ======================================================================== */

uint32_t create_data_node(uint8_t *token, uint32_t len) {
    // RULE 1: REUSE before CREATE (generalization!)
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type != NODE_DATA) continue;
        if (g.nodes[i].token_len != len) continue;
        
        int match = 1;
        for (uint32_t b = 0; b < len; b++) {
            if (g.nodes[i].token[b] != token[b]) {
                match = 0;
                break;
            }
        }
        
        if (match) {
            g.nodes[i].frequency++;
            g.patterns_reused++;
            return i;
        }
    }
    
    // CREATE new
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    g.nodes[id].type = NODE_DATA;
    g.nodes[id].token_len = len;
    for (uint32_t b = 0; b < len && b < 64; b++) {
        g.nodes[id].token[b] = token[b];
    }
    g.nodes[id].energy = 100.0f;
    g.nodes[id].frequency = 1;
    g.patterns_created++;
    
    return id;
}

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
    
    return id;
}

/* ========================================================================
 * INPUT: Data creates structure
 * ======================================================================== */

void sense_input(uint8_t *bytes, uint32_t len) {
    // Clear previous state
    for (uint32_t i = 0; i < g.node_count; i++) {
        g.nodes[i].state = 0.0f;
    }
    
    // Parse words
    uint32_t word_nodes[128];
    uint32_t word_count = 0;
    uint32_t word_start = 0;
    
    for (uint32_t i = 0; i <= len && word_count < 128; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '+' || ch == '=' || i == len) {
            if (i > word_start) {
                uint32_t node_id = create_data_node(&bytes[word_start], i - word_start);
                if (node_id != UINT32_MAX) {
                    word_nodes[word_count++] = node_id;
                    g.nodes[node_id].state = 1.0f;
                }
            }
            
            // Operators as separate nodes
            if (ch == '+' || ch == '=') {
                uint32_t node_id = create_data_node(&ch, 1);
                if (node_id != UINT32_MAX) {
                    word_nodes[word_count++] = node_id;
                    g.nodes[node_id].state = 1.0f;
                }
            }
            
            word_start = i + 1;
        }
    }
    
    // RULE 2: CREATE executable rules from sequences
    if (word_count >= 2) {
        for (uint32_t i = 0; i < word_count - 1; i++) {
            uint32_t inputs[1] = {word_nodes[i]};
            uint32_t outputs[1] = {word_nodes[i+1]};
            create_rule_node(inputs, 1, outputs, 1);
        }
        
        // Create compound rules (if A and B then C)
        if (word_count >= 3) {
            for (uint32_t i = 0; i < word_count - 2; i++) {
                uint32_t inputs[2] = {word_nodes[i], word_nodes[i+1]};
                uint32_t outputs[1] = {word_nodes[i+2]};
                create_rule_node(inputs, 2, outputs, 1);
            }
        }
    }
}

/* ========================================================================
 * GENERALIZATION: Transfer rules via similarity
 * ======================================================================== */

void generalize_rules() {
    for (uint32_t inp = 0; inp < g.node_count; inp++) {
        if (g.nodes[inp].type != NODE_DATA) continue;
        if (g.nodes[inp].state < 0.99f) continue;
        
        // Check if has rule
        int has_rule = 0;
        for (uint32_t r = 0; r < g.node_count; r++) {
            if (g.nodes[r].type == NODE_RULE && 
                g.nodes[r].rule_input_count == 1 &&
                g.nodes[r].rule_inputs[0] == inp) {
                has_rule = 1;
                break;
            }
        }
        
        if (has_rule) continue;
        
        // Find MOST similar node with rule (NO THRESHOLD!)
        uint32_t best_match = UINT32_MAX;
        float best_similarity = -1.0f;
        
        for (uint32_t sim = 0; sim < g.node_count; sim++) {
            if (g.nodes[sim].type != NODE_DATA) continue;
            if (sim == inp) continue;
            
            // Check if this node has a rule
            int has_rule_sim = 0;
            for (uint32_t r = 0; r < g.node_count; r++) {
                if (g.nodes[r].type == NODE_RULE && 
                    g.nodes[r].rule_input_count == 1 &&
                    g.nodes[r].rule_inputs[0] == sim) {
                    has_rule_sim = 1;
                    break;
                }
            }
            
            if (!has_rule_sim) continue;
            
            float similarity = token_similarity(&g.nodes[inp], &g.nodes[sim]);
            
            if (similarity > best_similarity) {
                best_similarity = similarity;
                best_match = sim;
            }
        }
        
        // Use the BEST match, even if weak!
        if (best_match != UINT32_MAX) {
            // Find rule for best match
            for (uint32_t r = 0; r < g.node_count; r++) {
                Node *rule = &g.nodes[r];
                if (rule->type != NODE_RULE) continue;
                if (rule->rule_input_count != 1) continue;
                if (rule->rule_inputs[0] != best_match) continue;
                
                // TRANSFER LEARNING: Create analogous rule!
                uint32_t new_inputs[1] = {inp};
                uint32_t new_rule = create_rule_node(new_inputs, 1, 
                                                      rule->rule_outputs, 
                                                      rule->rule_output_count);
                
                if (debug) {
                    fprintf(stderr, "[GENERALIZE] %u ('%.*s') → %u (%.2f sim to '%.*s')\n", 
                           inp, g.nodes[inp].token_len, g.nodes[inp].token,
                           rule->rule_outputs[0], best_similarity,
                           g.nodes[best_match].token_len, g.nodes[best_match].token);
                }
                
                return;
            }
        }
    }
}

/* ========================================================================
 * EXECUTION: Run the rules!
 * ======================================================================== */

void execute_rules() {
    static uint8_t from_input[10000];
    memset(from_input, 0, sizeof(from_input));
    
    // Mark input nodes
    for (uint32_t i = 0; i < g.node_count && i < 10000; i++) {
        if (g.nodes[i].type == NODE_DATA && g.nodes[i].state > 0.99f) {
            from_input[i] = 1;
        }
    }
    
    // MULTI-HOP with DECAY + REASONING!
    for (int hop = 0; hop < 5; hop++) {
        float activation_strength = 1.0f - (hop * 0.2f);
        if (activation_strength <= 0.0f) break;
        
        int any_fired = 0;
        
        // COMPETITION: Calculate rule strengths first
        for (uint32_t i = 0; i < g.node_count; i++) {
            Node *rule = &g.nodes[i];
            if (rule->type != NODE_RULE) continue;
            // Strength = usage (frequent patterns are stronger)
            rule->rule_strength = (float)rule->times_executed / (rule->times_executed + 10.0f);
        }
        
        for (uint32_t i = 0; i < g.node_count; i++) {
            Node *rule = &g.nodes[i];
            if (rule->type != NODE_RULE) continue;
            
            // Check if all inputs are active
            int all_active = 1;
            float min_input_state = 1.0f;
            for (uint8_t j = 0; j < rule->rule_input_count; j++) {
                uint32_t inp_id = rule->rule_inputs[j];
                if (inp_id >= g.node_count || g.nodes[inp_id].state < 0.3f) {
                    all_active = 0;
                    break;
                }
                if (g.nodes[inp_id].state < min_input_state) {
                    min_input_state = g.nodes[inp_id].state;
                }
            }
            
            if (all_active) {
                // COMPETITION: Only fire if rule is strong enough
                if (rule->rule_strength < 0.1f) continue;  // Weak rules blocked!
                
                // Fire rule with decay
                float output_strength = min_input_state * 0.7f;
                if (output_strength < 0.4f) continue;
                
                // EXCITATION: Activate outputs
                for (uint8_t j = 0; j < rule->rule_output_count; j++) {
                    uint32_t out_id = rule->rule_outputs[j];
                    if (out_id < g.node_count && g.nodes[out_id].state < output_strength) {
                        g.nodes[out_id].state = output_strength;
                        any_fired = 1;
                    }
                }
                
                // INHIBITION: Suppress competing nodes!
                for (uint8_t j = 0; j < rule->inhibit_count; j++) {
                    uint32_t inh_id = rule->inhibit_outputs[j];
                    if (inh_id < g.node_count) {
                        g.nodes[inh_id].state -= output_strength;  // Negative activation!
                        if (g.nodes[inh_id].state < 0.0f) g.nodes[inh_id].state = 0.0f;
                    }
                }
                
                rule->times_executed++;
            }
        }
        
        if (!any_fired) break;
    }
}

/* ========================================================================
 * OUTPUT: Show rule results only
 * ======================================================================== */

void emit_output() {
    static uint8_t from_input[10000];
    memset(from_input, 0, sizeof(from_input));
    
    for (uint32_t i = 0; i < g.node_count && i < 10000; i++) {
        if (g.nodes[i].type == NODE_DATA && g.nodes[i].state > 0.99f) {
            from_input[i] = 1;
        }
    }
    
    uint8_t output[1024];
    uint32_t output_len = 0;
    
    // Output only nodes activated BY rules with sufficient strength
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type != NODE_DATA) continue;
        if (g.nodes[i].state < 0.4f) continue;  // Higher threshold = cleaner output
        if (from_input[i]) continue;  // Skip input nodes
        
        
        for (uint32_t b = 0; b < g.nodes[i].token_len; b++) {
            output[output_len++] = g.nodes[i].token[b];
        }
        output[output_len++] = ' ';
    }
    
    if (output_len > 0) {
        write(STDOUT_FILENO, output, output_len);
    }
    write(STDOUT_FILENO, "\n", 1);
}

/* ========================================================================
 * PATTERN DETECTION: Find coherent clusters
 * ======================================================================== */

void detect_patterns() {
    if (g.tick % 20 != 0) return;
    
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].type != NODE_DATA) continue;
        
        uint32_t cluster[32];
        uint8_t cluster_size = 0;
        cluster[cluster_size++] = i;
        
        // Find nodes activated by rules from this node (no frequency threshold!)
        for (uint32_t r = 0; r < g.node_count; r++) {
            Node *rule = &g.nodes[r];
            if (rule->type != NODE_RULE) continue;
            if (rule->times_executed < 1) continue;  // Just needs to have fired once
            
            for (uint8_t j = 0; j < rule->rule_input_count; j++) {
                if (rule->rule_inputs[j] == i && cluster_size < 32) {
                    for (uint8_t k = 0; k < rule->rule_output_count; k++) {
                        cluster[cluster_size++] = rule->rule_outputs[k];
                    }
                }
            }
        }
        
        if (cluster_size >= 3 && g.node_count < g.node_cap) {
            uint32_t pattern_id = g.node_count++;
            memset(&g.nodes[pattern_id], 0, sizeof(Node));
            g.nodes[pattern_id].type = NODE_PATTERN;
            g.nodes[pattern_id].pattern_member_count = cluster_size;
            
            for (uint8_t j = 0; j < cluster_size; j++) {
                g.nodes[pattern_id].pattern_members[j] = cluster[j];
            }
            
            g.nodes[pattern_id].energy = 200.0f;
            return;
        }
    }
}

/* ========================================================================
 * ADAPTATION: Dynamic parameters
 * ======================================================================== */

void adapt_parameters() {
    if (g.tick % 100 != 0) return;
    
    // Adapt learning rate based on reuse ratio
    uint32_t total = g.patterns_created + g.patterns_reused;
    if (total > 50) {
        float reuse_ratio = (float)g.patterns_reused / (float)total;
        
        if (reuse_ratio < 0.5f) {
            g.learning_rate += 0.01f;
            if (g.learning_rate > 0.5f) g.learning_rate = 0.5f;
        } else {
            g.learning_rate -= 0.01f;
            if (g.learning_rate < 0.05f) g.learning_rate = 0.05f;
        }
        
        g.patterns_created = 0;
        g.patterns_reused = 0;
    }
    
    // Similarity threshold stays FIXED at 0.5
    // (Removed adaptive decay - it caused over-generalization)
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    g.node_cap = 10000;
    size_t size = sizeof(uint32_t) + g.node_cap * sizeof(Node);
    
    int fd = open("graph.mmap", O_RDWR | O_CREAT, 0644);
    struct stat st;
    int exists = (fstat(fd, &st) == 0 && st.st_size > 0);
    
    if (!exists) ftruncate(fd, size);
    
    mmap_base = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint32_t *header = (uint32_t *)mmap_base;
    g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t));
    
    if (exists && header[0] > 0) {
        g.node_count = header[0];
        if (debug) printf("[LOAD] %u nodes\n", g.node_count);
    } else {
        g.node_count = 0;
    }
    
    // ALWAYS initialize these (they're not persisted in mmap)
    g.learning_rate = 0.1f;
    g.similarity_threshold = 0.5f;  // FIXED: was 0.0 on reload!
    
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    
    uint8_t input[4096];
    int idle = 0;
    
    while (idle < 10) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        
        if (n > 0) {
            sense_input(input, n);
            generalize_rules();  // Transfer learning!
            execute_rules();     // Run programs!
            emit_output();       // Show results (once per input!)
            detect_patterns();   // Find clusters
            adapt_parameters();  // Evolve
            g.tick++;
            idle = 0;
        } else {
            idle++;
            usleep(10000);
        }
    }
    
    header[0] = g.node_count;
    msync(mmap_base, size, MS_SYNC);
    munmap(mmap_base, size);
    close(fd);
    
    if (debug) {
        uint32_t data=0, rules=0, patterns=0;
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].type == NODE_DATA) data++;
            else if (g.nodes[i].type == NODE_RULE) rules++;
            else if (g.nodes[i].type == NODE_PATTERN) patterns++;
        }
        printf("\n[FINAL] %u data, %u rules, %u patterns\n", data, rules, patterns);
    }
    
    return 0;
}

