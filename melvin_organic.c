/*
 * MELVIN ORGANIC - Pattern-Based Learning Without Memorization
 * 
 * Core Principle:
 * "Inputs are ephemeral - extract patterns, connect similar, then forget"
 * 
 * Algorithm:
 * 1. Input comes in (bytes/tokens)
 * 2. Extract patterns (what follows what, what's similar to what)
 * 3. Find similar nodes in existing graph
 * 4. Create edges between similar nodes based on patterns observed
 * 5. Forget the input - patterns are now in the graph!
 * 
 * No frequency counting.
 * No memorizing exact inputs.
 * Just organic pattern growth through similarity and co-occurrence.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <math.h>

/* ========================================================================
 * SIMPLIFIED STRUCTURES - No frequency, no memorization
 * ======================================================================== */

// 20-byte node (removed frequency!)
typedef struct __attribute__((packed)) {
    uint8_t token[16];     // Token data
    float activation;      // Current activation (temporary)
    uint16_t token_len;    // Length
    uint8_t is_temp;       // 1 = temporary (from current input), 0 = permanent
    uint8_t padding;
} Node;

// 9-byte edge (minimal)
typedef struct __attribute__((packed)) {
    uint32_t from;         // Source node
    uint32_t to;           // Target node
    uint8_t weight;        // Strength (grows with each pattern match)
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
 * SIMILARITY - How similar are two byte sequences?
 * ======================================================================== */

float byte_similarity(uint8_t *a, uint32_t len_a, uint8_t *b, uint32_t len_b) {
    if (len_a == 0 || len_b == 0) return 0.0f;
    
    // Character overlap
    uint32_t shared = 0;
    uint32_t max_len = (len_a > len_b) ? len_a : len_b;
    uint32_t min_len = (len_a < len_b) ? len_a : len_b;
    
    for (uint32_t i = 0; i < min_len && i < 16; i++) {
        if (a[i] == b[i]) shared++;
    }
    
    // Similarity = shared chars / max length
    float similarity = (float)shared / (float)max_len;
    
    // Bonus: same length is more similar
    if (len_a == len_b) similarity *= 1.2f;
    if (similarity > 1.0f) similarity = 1.0f;
    
    return similarity;
}

/* ========================================================================
 * PATTERN EXTRACTION - What patterns exist in this input?
 * ======================================================================== */

typedef struct {
    uint32_t from;  // Index in temporary nodes
    uint32_t to;    // Index in temporary nodes
    float strength; // How strong is this pattern?
} Pattern;

void extract_patterns(Node *temp_nodes, uint32_t temp_count, Pattern *patterns, uint32_t *pattern_count) {
    *pattern_count = 0;
    
    // Pattern 1: SEQUENCE (A follows B)
    for (uint32_t i = 0; i + 1 < temp_count; i++) {
        patterns[*pattern_count].from = i;
        patterns[*pattern_count].to = i + 1;
        patterns[*pattern_count].strength = 1.0f;  // Strong: direct sequence
        (*pattern_count)++;
    }
    
    // Pattern 2: SIMILARITY (A is similar to B)
    for (uint32_t i = 0; i < temp_count; i++) {
        for (uint32_t j = i + 1; j < temp_count; j++) {
            float sim = byte_similarity(
                temp_nodes[i].token, temp_nodes[i].token_len,
                temp_nodes[j].token, temp_nodes[j].token_len
            );
            
            if (sim > 0.3f) {  // Threshold for similarity
                patterns[*pattern_count].from = i;
                patterns[*pattern_count].to = j;
                patterns[*pattern_count].strength = sim;
                (*pattern_count)++;
                
                // Bidirectional similarity
                patterns[*pattern_count].from = j;
                patterns[*pattern_count].to = i;
                patterns[*pattern_count].strength = sim;
                (*pattern_count)++;
            }
        }
    }
}

/* ========================================================================
 * ORGANIC CONNECTION - Connect patterns to existing graph
 * ======================================================================== */

uint32_t find_or_create_permanent_node(uint8_t *token, uint32_t len) {
    // Search existing permanent nodes
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].is_temp) continue;  // Skip temp nodes
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
    
    // Create new permanent node
    if (g.node_count >= g.node_cap) {
        fprintf(stderr, "[ERROR] Node capacity reached\n");
        return UINT32_MAX;
    }
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    memcpy(g.nodes[id].token, token, (len < 16) ? len : 16);
    g.nodes[id].token_len = len;
    g.nodes[id].is_temp = 0;  // Permanent!
    
    if (debug) {
        fprintf(stderr, "[NODE] Created permanent #%u: '%.*s'\n", 
                id, (len < 16) ? len : 16, token);
    }
    
    return id;
}

void create_or_strengthen_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count) return;
    if (from == to) return;  // No self-edges
    
    // Search for existing edge
    for (uint32_t i = 0; i < g.edge_count; i++) {
        if (g.edges[i].from == from && g.edges[i].to == to) {
            // Strengthen existing edge
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
    
    // Create new edge
    if (g.edge_count >= g.edge_cap) {
        fprintf(stderr, "[ERROR] Edge capacity reached\n");
        return;
    }
    
    uint32_t id = g.edge_count++;
    g.edges[id].from = from;
    g.edges[id].to = to;
    g.edges[id].weight = weight;
    
    if (debug) {
        fprintf(stderr, "[EDGE] %u→%u (weight: %u)\n", from, to, weight);
    }
}

void organic_connect(Node *temp_nodes, uint32_t temp_count __attribute__((unused)), Pattern *patterns, uint32_t pattern_count) {
    /*
     * For each pattern observed in temporary input:
     * 1. Find similar nodes in permanent graph
     * 2. Create edges between similar nodes based on pattern
     * 3. This grows the graph organically!
     */
    
    for (uint32_t p = 0; p < pattern_count; p++) {
        Pattern pat = patterns[p];
        Node *from_temp = &temp_nodes[pat.from];
        Node *to_temp = &temp_nodes[pat.to];
        
        // Find or create permanent nodes matching these temporary ones
        uint32_t from_perm = find_or_create_permanent_node(from_temp->token, from_temp->token_len);
        uint32_t to_perm = find_or_create_permanent_node(to_temp->token, to_temp->token_len);
        
        if (from_perm == UINT32_MAX || to_perm == UINT32_MAX) continue;
        
        // Create edge with strength based on pattern strength
        uint8_t edge_weight = (uint8_t)(pat.strength * 50.0f);  // Scale 0.0-1.0 → 0-50
        if (edge_weight < 10) edge_weight = 10;  // Minimum weight
        
        create_or_strengthen_edge(from_perm, to_perm, edge_weight);
        
        // GENERALIZATION: Connect similar nodes too!
        // If "cat"→"sat" pattern exists, also connect similar words
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].is_temp) continue;
            if (i == from_perm || i == to_perm) continue;
            
            // Is this node similar to from_perm?
            float sim_from = byte_similarity(
                g.nodes[i].token, g.nodes[i].token_len,
                g.nodes[from_perm].token, g.nodes[from_perm].token_len
            );
            
            if (sim_from > 0.5f) {  // High similarity threshold
                // Connect similar node to same target!
                // If "cat"→"sat", then "mat"→"sat" (because "cat"≈"mat")
                uint8_t gen_weight = (uint8_t)(edge_weight * sim_from);
                create_or_strengthen_edge(i, to_perm, gen_weight);
                
                if (debug) {
                    fprintf(stderr, "[GEN] Similar node %u→%u (sim: %.2f)\n", 
                            i, to_perm, sim_from);
                }
            }
        }
    }
}

/* ========================================================================
 * LEARNING - Process input, extract patterns, connect, forget
 * ======================================================================== */

void learn(uint8_t *input, uint32_t len) {
    if (debug) fprintf(stderr, "\n[LEARN] Processing %u bytes\n", len);
    
    // Step 1: Parse input into temporary nodes
    Node temp_nodes[1000];
    uint32_t temp_count = 0;
    
    uint32_t word_start = 0;
    for (uint32_t i = 0; i <= len && temp_count < 1000; i++) {
        uint8_t ch = (i < len) ? input[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || i == len) {
            if (i > word_start) {
                uint32_t word_len = i - word_start;
                memset(&temp_nodes[temp_count], 0, sizeof(Node));
                memcpy(temp_nodes[temp_count].token, &input[word_start], 
                       (word_len < 16) ? word_len : 16);
                temp_nodes[temp_count].token_len = word_len;
                temp_nodes[temp_count].is_temp = 1;
                
                if (debug) {
                    fprintf(stderr, "[TEMP] #%u: '%.*s'\n", 
                            temp_count, (word_len < 16) ? word_len : 16, &input[word_start]);
                }
                
                temp_count++;
            }
            word_start = i + 1;
        }
    }
    
    if (temp_count == 0) return;
    
    // Step 2: Extract patterns from temporary nodes
    Pattern patterns[10000];
    uint32_t pattern_count = 0;
    extract_patterns(temp_nodes, temp_count, patterns, &pattern_count);
    
    if (debug) fprintf(stderr, "[PATTERNS] Extracted %u patterns\n", pattern_count);
    
    // Step 3: Connect patterns to permanent graph
    organic_connect(temp_nodes, temp_count, patterns, pattern_count);
    
    // Step 4: FORGET temporary nodes!
    // They're gone. Only the patterns remain in the graph.
    
    if (debug) fprintf(stderr, "[FORGET] Temporary nodes discarded\n");
}

/* ========================================================================
 * QUERY - Activate and spread through graph
 * ======================================================================== */

void query(uint8_t *input, uint32_t len) {
    if (debug) fprintf(stderr, "\n[QUERY] '%.*s'\n", len, input);
    
    // Clear activation
    for (uint32_t i = 0; i < g.node_count; i++) {
        g.nodes[i].activation = 0.0f;
    }
    
    // Find matching node
    uint32_t match = UINT32_MAX;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].is_temp) continue;
        if (g.nodes[i].token_len != len) continue;
        
        int exact = 1;
        uint32_t cmp_len = (len < 16) ? len : 16;
        for (uint32_t b = 0; b < cmp_len; b++) {
            if (g.nodes[i].token[b] != input[b]) {
                exact = 0;
                break;
            }
        }
        
        if (exact) {
            match = i;
            break;
        }
    }
    
    if (match == UINT32_MAX) {
        fprintf(stderr, "[QUERY] No match found\n");
        return;
    }
    
    // Activate matched node
    g.nodes[match].activation = 1.0f;
    
    // Spread activation through edges
    for (uint32_t hop = 0; hop < 3; hop++) {  // 3 hops
        for (uint32_t e = 0; e < g.edge_count; e++) {
            Edge *edge = &g.edges[e];
            float source_act = g.nodes[edge->from].activation;
            
            if (source_act > 0.01f) {
                float spread = source_act * (edge->weight / 255.0f) * 0.8f;  // Decay
                g.nodes[edge->to].activation += spread;
            }
        }
    }
    
    // Output activated nodes (excluding input)
    printf("Output: ");
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (i == match) continue;  // Don't echo input
        if (g.nodes[i].activation > 0.1f) {
            printf("%.*s (%.2f) ", 
                   (g.nodes[i].token_len < 16) ? g.nodes[i].token_len : 16,
                   g.nodes[i].token,
                   g.nodes[i].activation);
        }
    }
    printf("\n");
}

/* ========================================================================
 * PERSISTENCE
 * ======================================================================== */

void save_graph(const char *path) {
    size_t header_size = sizeof(uint32_t) * 4;
    size_t node_size = g.node_count * sizeof(Node);
    size_t edge_size = g.edge_count * sizeof(Edge);
    size_t total_size = header_size + node_size + edge_size;
    
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return;
    
    ftruncate(fd, total_size);
    void *mem = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    uint32_t *header = (uint32_t *)mem;
    header[0] = g.node_count;
    header[1] = g.node_cap;
    header[2] = g.edge_count;
    header[3] = g.edge_cap;
    
    memcpy((char *)mem + header_size, g.nodes, node_size);
    memcpy((char *)mem + header_size + node_size, g.edges, edge_size);
    
    munmap(mem, total_size);
    close(fd);
    
    if (debug) fprintf(stderr, "[SAVE] %u nodes, %u edges\n", g.node_count, g.edge_count);
}

void load_graph(const char *path) {
    int fd = open(path, O_RDWR);
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
    size_t node_size = g.node_count * sizeof(Node);
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    memcpy(g.nodes, (char *)mem + header_size, node_size);
    memcpy(g.edges, (char *)mem + header_size + node_size, g.edge_count * sizeof(Edge));
    
    munmap(mem, st.st_size);
    close(fd);
    
    if (debug) fprintf(stderr, "[LOAD] %u nodes, %u edges\n", g.node_count, g.edge_count);
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    debug = getenv("MELVIN_DEBUG") != NULL;
    
    // Initialize graph
    g.node_cap = 10000;
    g.edge_cap = 100000;
    g.node_count = 0;
    g.edge_count = 0;
    
    g.nodes = malloc(g.node_cap * sizeof(Node));
    g.edges = malloc(g.edge_cap * sizeof(Edge));
    
    memset(g.nodes, 0, g.node_cap * sizeof(Node));
    memset(g.edges, 0, g.edge_cap * sizeof(Edge));
    
    // Try to load existing graph
    load_graph("organic.mmap");
    
    // Read input
    uint8_t buffer[65536];
    ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = 0;
        
        // Strip trailing newline/whitespace
        while (bytes_read > 0 && (buffer[bytes_read - 1] == '\n' || 
                                   buffer[bytes_read - 1] == '\r' ||
                                   buffer[bytes_read - 1] == ' ')) {
            bytes_read--;
            buffer[bytes_read] = 0;
        }
        
        // Is this a query (single word) or learning (multiple words)?
        int is_query = 1;
        for (ssize_t i = 0; i < bytes_read; i++) {
            if (buffer[i] == ' ') {
                is_query = 0;
                break;
            }
        }
        
        if (is_query) {
            query(buffer, bytes_read);
        } else {
            learn(buffer, bytes_read);
        }
    }
    
    // Save graph
    save_graph("organic.mmap");
    
    // Cleanup
    free(g.nodes);
    free(g.edges);
    
    return 0;
}

