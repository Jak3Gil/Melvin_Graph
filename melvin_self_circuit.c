/*
 * MELVIN SELF-CIRCUIT - Learns to Build Circuits from Data
 * 
 * Key Insight:
 * "Data codes the system. We want Melvin to make a XOR circuit out of
 *  nodes and connections using simple general rules."
 * 
 * Approach:
 * - Same organic learning rules (co-occurrence, similarity)
 * - Teach by example: "0 XOR 0 = 0", "1 XOR 1 = 0", etc.
 * - Melvin discovers the pattern and builds the circuit
 * - NO hardcoded circuit construction!
 * 
 * The circuit emerges from pattern learning!
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
 * SIMPLE STRUCTURES - Same as organic learning
 * ======================================================================== */

typedef struct __attribute__((packed)) {
    uint8_t token[16];
    float activation;
    uint16_t token_len;
    uint8_t padding[2];
} Node;

typedef struct __attribute__((packed)) {
    uint32_t from;
    uint32_t to;
    uint8_t weight;
} Edge;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    
    Edge *edges;
    uint32_t edge_count;
    uint32_t edge_cap;
} Graph;

Graph g;
void *mmap_base = NULL;
int mmap_fd = -1;
size_t mmap_size = 0;
int debug = 0;

/* ========================================================================
 * SAME SIMPLE RULES - From organic learning
 * ======================================================================== */

float token_similarity(Node *a, Node *b) {
    if (a->token_len == 0 || b->token_len == 0) return 0.0f;
    
    uint32_t shared = 0;
    uint32_t max_len = (a->token_len > b->token_len) ? a->token_len : b->token_len;
    uint32_t min_len = (a->token_len < b->token_len) ? a->token_len : b->token_len;
    
    for (uint32_t i = 0; i < min_len && i < 16; i++) {
        if (a->token[i] == b->token[i]) shared++;
    }
    
    return (float)shared / (float)max_len;
}

uint32_t find_or_create_node(uint8_t *token, uint32_t len) {
    // Search existing
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int match = 1;
        uint32_t cmp_len = (len < 16) ? len : 16;
        for (uint32_t b = 0; b < cmp_len; b++) {
            if (g.nodes[i].token[b] != token[b]) {
                match = 0;
                break;
            }
        }
        
        if (match) return i;
    }
    
    // Create new
    if (g.node_count >= g.node_cap) return UINT32_MAX;
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    memcpy(g.nodes[id].token, token, (len < 16) ? len : 16);
    g.nodes[id].token_len = len;
    
    if (debug) {
        fprintf(stderr, "[NODE] #%u: '%.*s'\n", id, (len < 16) ? len : 16, token);
    }
    
    return id;
}

void create_or_strengthen_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count) return;
    if (from == to) return;
    
    // Search existing
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            if (g.edges[i].weight < 255 - weight) {
                g.edges[i].weight += weight;
            } else {
                g.edges[i].weight = 255;
            }
            
            if (debug) {
                fprintf(stderr, "[EDGE+] %u→%u (weight: %u)\n", 
                        from, to, g.edges[i].weight);
            }
            return;
        }
    }
    
    // Create new
    if (g.edge_count >= g.edge_cap) return;
    
    uint32_t id = g.edge_count++;
    g.edges[id].from = from;
    g.edges[id].to = to;
    g.edges[id].weight = weight;
    
    if (debug) {
        fprintf(stderr, "[EDGE] %u→%u (weight: %u)\n", from, to, weight);
    }
}

/* ========================================================================
 * LEARNING - Same organic approach!
 * ======================================================================== */

void learn_pattern(uint8_t *input, uint32_t len) {
    // Parse tokens (same as organic)
    uint32_t nodes[100];
    uint32_t node_count = 0;
    uint32_t word_start = 0;
    
    for (uint32_t i = 0; i <= len && node_count < 100; i++) {
        uint8_t ch = (i < len) ? input[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || i == len) {
            if (i > word_start) {
                uint32_t word_len = i - word_start;
                uint32_t node_id = find_or_create_node(&input[word_start], word_len);
                if (node_id != UINT32_MAX) {
                    nodes[node_count++] = node_id;
                }
            }
            word_start = i + 1;
        }
    }
    
    if (node_count == 0) return;
    
    if (debug) {
        fprintf(stderr, "[PATTERN] Learned %u tokens\n", node_count);
    }
    
    // SIMPLE RULE 1: Connect sequential (A→B→C)
    for (uint32_t i = 0; i + 1 < node_count; i++) {
        create_or_strengthen_edge(nodes[i], nodes[i+1], 50);
    }
    
    // SIMPLE RULE 2: Connect similar to similar
    for (uint32_t i = 0; i < node_count; i++) {
        for (uint32_t j = i + 1; j < node_count; j++) {
            float sim = token_similarity(&g.nodes[nodes[i]], &g.nodes[nodes[j]]);
            if (sim > 0.3f) {
                uint8_t w = (uint8_t)(sim * 30.0f);
                create_or_strengthen_edge(nodes[i], nodes[j], w);
                create_or_strengthen_edge(nodes[j], nodes[i], w);
            }
        }
    }
    
    // SIMPLE RULE 3: If A→B pattern exists, and C similar to A, then C→B
    for (uint32_t i = 0; i < node_count; i++) {
        for (uint32_t j = i + 1; j < node_count; j++) {
            // For all existing nodes
            for (uint32_t k = 0; k < g.node_count; k++) {
                if (k == nodes[i] || k == nodes[j]) continue;
                
                float sim = token_similarity(&g.nodes[k], &g.nodes[nodes[i]]);
                if (sim > 0.5f) {
                    // Similar to nodes[i], so connect to nodes[j] too
                    uint8_t w = (uint8_t)(25.0f * sim);
                    create_or_strengthen_edge(k, nodes[j], w);
                }
            }
        }
    }
}

/* ========================================================================
 * QUERY - Follow edges through the learned circuit
 * ======================================================================== */

void query_pattern(uint8_t *input, uint32_t len) {
    // Clear activation
    for (uint32_t i = 0; i < g.node_count; i++) {
        g.nodes[i].activation = 0.0f;
    }
    
    // Parse input
    uint32_t input_nodes[100];
    uint32_t input_count = 0;
    uint32_t word_start = 0;
    
    for (uint32_t i = 0; i <= len && input_count < 100; i++) {
        uint8_t ch = (i < len) ? input[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || i == len) {
            if (i > word_start) {
                uint32_t word_len = i - word_start;
                
                // Find matching node
                for (uint32_t n = 0; n < g.node_count; n++) {
                    if (g.nodes[n].token_len != word_len) continue;
                    
                    int match = 1;
                    for (uint32_t b = 0; b < word_len && b < 16; b++) {
                        if (g.nodes[n].token[b] != input[word_start + b]) {
                            match = 0;
                            break;
                        }
                    }
                    
                    if (match) {
                        input_nodes[input_count++] = n;
                        g.nodes[n].activation = 1.0f;
                        break;
                    }
                }
            }
            word_start = i + 1;
        }
    }
    
    if (debug) {
        fprintf(stderr, "[QUERY] %u input nodes activated\n", input_count);
    }
    
    // Spread activation (EXECUTION through the learned circuit!)
    for (int hop = 0; hop < 5; hop++) {
        for (uint32_t e = 0; e < g.edge_count; e++) {
            float source_act = g.nodes[g.edges[e].from].activation;
            if (source_act > 0.01f) {
                float spread = source_act * (g.edges[e].weight / 255.0f) * 0.85f;
                g.nodes[g.edges[e].to].activation += spread;
            }
        }
    }
    
    // Output highly activated nodes (excluding inputs)
    printf("Result: ");
    int found = 0;
    for (uint32_t i = 0; i < g.node_count; i++) {
        // Skip if it was an input
        int is_input = 0;
        for (uint32_t j = 0; j < input_count; j++) {
            if (i == input_nodes[j]) {
                is_input = 1;
                break;
            }
        }
        
        if (!is_input && g.nodes[i].activation > 0.3f) {
            printf("%.*s ", g.nodes[i].token_len, g.nodes[i].token);
            found = 1;
        }
    }
    
    if (!found) printf("(none)");
    printf("\n");
}

/* ========================================================================
 * PERSISTENCE
 * ======================================================================== */

void save_graph() {
    size_t header_size = sizeof(uint32_t) * 4;
    size_t node_size = g.node_count * sizeof(Node);
    size_t edge_size = g.edge_count * sizeof(Edge);
    size_t total = header_size + node_size + edge_size;
    
    int fd = open("circuit.mmap", O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    
    ftruncate(fd, total);
    void *mem = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    uint32_t *header = (uint32_t *)mem;
    header[0] = g.node_count;
    header[1] = g.node_cap;
    header[2] = g.edge_count;
    header[3] = g.edge_cap;
    
    memcpy((char *)mem + header_size, g.nodes, node_size);
    memcpy((char *)mem + header_size + node_size, g.edges, edge_size);
    
    munmap(mem, total);
    close(fd);
    
    if (debug) {
        fprintf(stderr, "[SAVE] %u nodes, %u edges (learned circuit)\n", 
                g.node_count, g.edge_count);
    }
}

void load_graph() {
    int fd = open("circuit.mmap", O_RDWR);
    if (fd < 0) return;
    
    struct stat st;
    fstat(fd, &st);
    
    void *mem = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    uint32_t *header = (uint32_t *)mem;
    g.node_count = header[0];
    g.node_cap = header[1];
    g.edge_count = header[2];
    g.edge_cap = header[3];
    
    size_t header_size = sizeof(uint32_t) * 4;
    
    memcpy(g.nodes, (char *)mem + header_size, g.node_count * sizeof(Node));
    memcpy(g.edges, (char *)mem + header_size + g.node_count * sizeof(Node), 
           g.edge_count * sizeof(Edge));
    
    munmap(mem, st.st_size);
    close(fd);
    
    if (debug) {
        fprintf(stderr, "[LOAD] %u nodes, %u edges (learned circuit)\n", 
                g.node_count, g.edge_count);
    }
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    debug = getenv("MELVIN_DEBUG") != NULL;
    
    // Initialize
    g.node_cap = 10000;
    g.edge_cap = 100000;
    g.node_count = 0;
    g.edge_count = 0;
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    memset(g.nodes, 0, g.node_cap * sizeof(Node));
    memset(g.edges, 0, g.edge_cap * sizeof(Edge));
    
    // Load existing learned circuit
    load_graph();
    
    // Read input
    uint8_t buffer[4096];
    ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    
    if (bytes > 0) {
        buffer[bytes] = 0;
        
        // Strip newline
        while (bytes > 0 && (buffer[bytes-1] == '\n' || buffer[bytes-1] == '\r')) {
            buffer[--bytes] = 0;
        }
        
        // Detect if teaching (has '=') or querying
        int has_equals = 0;
        for (ssize_t i = 0; i < bytes; i++) {
            if (buffer[i] == '=') {
                has_equals = 1;
                break;
            }
        }
        
        if (has_equals) {
            // Teaching mode
            if (debug) fprintf(stderr, "[LEARN] Teaching from example\n");
            learn_pattern(buffer, bytes);
        } else {
            // Query mode
            if (debug) fprintf(stderr, "[QUERY] Querying learned circuit\n");
            query_pattern(buffer, bytes);
        }
    }
    
    // Save learned circuit
    save_graph();
    
    // Cleanup
    free(g.nodes);
    free(g.edges);
    
    return 0;
}

