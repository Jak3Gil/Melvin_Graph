/*
 * MELVIN - Spreading Activation Network
 * 
 * 1 word = 1 node, but MILLIONS of edges for massive propagation.
 * 
 * The Key Insight:
 * When you hear "cat", ~1 million neurons fire in your brain.
 * NOT because "cat" is encoded in 1 million neurons,
 * BUT because ONE activation CASCADE to millions through associations!
 * 
 * Architecture:
 * - 1 word → 1 node (simple, clean)
 * - Each node → thousands/millions of EDGES
 * - Edges carry activation to related concepts
 * - Activation spreads like wildfire through the network
 * 
 * Example:
 *   "cat" → [animal, pet, furry, meow, whiskers, small, mammal, 
 *            cute, sits, purrs, hunts, mouse, claws, tail, ...]
 *   Each of those → more activations → eventually 1M+ nodes fire
 * 
 * This is SPREADING ACTIVATION - how associative memory works.
 * 
 * Intelligence = rich edge connectivity (not complex nodes)
 * Learning = creating more edges (not more complex representations)
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
 * CORE STRUCTURES
 * ======================================================================== */

// 24-byte node (compact!)
typedef struct __attribute__((packed)) {
    uint8_t token[16];     // 16 bytes - token data (truncated if needed)
    float activation;      // 4 bytes - current activation
    uint16_t token_len;    // 2 bytes - actual token length
    uint16_t frequency;    // 2 bytes - usage count (0-65K)
} Node;

// 9-byte edge (compact!)
typedef struct __attribute__((packed)) {
    uint32_t from;         // 4 bytes - source node
    uint32_t to;           // 4 bytes - target node
    uint8_t weight;        // 1 byte - connection strength (0-255)
} Edge;

// Edge hash table for O(1) lookups (transient, not persisted)
typedef struct {
    uint64_t key;          // (from << 32) | to
    uint32_t edge_idx;     // Index into edges array
} EdgeHashEntry;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    
    Edge *edges;           // Separate edge storage
    uint32_t edge_count;
    uint32_t edge_cap;
    
    EdgeHashEntry *edge_hash;  // Hash table (allocated separately, not in mmap)
    uint32_t edge_hash_size;   // Dynamic hash table size
    
    uint64_t tick;
} Graph;

Graph g;
void *mmap_base = NULL;
int mmap_fd = -1;
size_t mmap_size = 0;
int debug = 0;

/* ========================================================================
 * UTILITIES
 * ======================================================================== */

// Hash function (from melvin_core.c)
static inline uint64_t edge_hash_key(uint32_t from, uint32_t to) {
    return ((uint64_t)from << 32) | (uint64_t)to;
}

static inline uint32_t edge_hash_func(uint64_t key, uint32_t size) {
    // Ensure size is power of 2 for fast modulo
    return (uint32_t)((key * 2654435761ULL) & (size - 1));
}

// Insert edge into hash table
void edge_hash_insert(uint32_t edge_idx) {
    Edge *e = &g.edges[edge_idx];
    uint64_t key = edge_hash_key(e->from, e->to);
    uint32_t hash = edge_hash_func(key, g.edge_hash_size);
    
    // Linear probing
    for (uint32_t i = 0; i < g.edge_hash_size; i++) {
        uint32_t idx = (hash + i) & (g.edge_hash_size - 1);
        if (g.edge_hash[idx].key == 0 || g.edge_hash[idx].key == key) {
            g.edge_hash[idx].key = key;
            g.edge_hash[idx].edge_idx = edge_idx;
            return;
        }
    }
}

// Find edge in hash table (O(1) average case)
Edge* edge_hash_find(uint32_t from, uint32_t to) {
    uint64_t key = edge_hash_key(from, to);
    uint32_t hash = edge_hash_func(key, g.edge_hash_size);
    
    for (uint32_t i = 0; i < g.edge_hash_size; i++) {
        uint32_t idx = (hash + i) & (g.edge_hash_size - 1);
        if (g.edge_hash[idx].key == key) {
            uint32_t edge_idx = g.edge_hash[idx].edge_idx;
            if (edge_idx < g.edge_count) {
                return &g.edges[edge_idx];
            }
        }
        if (g.edge_hash[idx].key == 0) return NULL;
    }
    return NULL;
}

float token_similarity(Node *a, Node *b) {
    if (a->token_len == 0 || b->token_len == 0) return 0.0f;
    
    // Simple character overlap similarity
    uint32_t shared = 0;
    uint32_t total = (a->token_len > b->token_len) ? a->token_len : b->token_len;
    
    // Compare only up to 16 bytes (node limit)
    uint32_t cmp_len_a = (a->token_len < 16) ? a->token_len : 16;
    uint32_t cmp_len_b = (b->token_len < 16) ? b->token_len : 16;
    
    for (uint32_t i = 0; i < cmp_len_a && i < cmp_len_b; i++) {
        if (a->token[i] == b->token[i]) shared++;
    }
    
    return (total > 0) ? (float)shared / (float)total : 0.0f;
}

/* ========================================================================
 * MMAP AUTO-GROW - Unlimited capacity!
 * ======================================================================== */

void graph_mmap_grow(uint32_t new_node_cap, uint32_t new_edge_cap) {
    if (mmap_base == NULL || mmap_fd < 0) return;
    
    if (debug) {
        fprintf(stderr, "[GROW] %u→%u nodes, %u→%u edges (%.1f MB)\n",
               g.node_cap, new_node_cap, g.edge_cap, new_edge_cap,
               (new_node_cap * sizeof(Node) + new_edge_cap * sizeof(Edge)) / 1024.0 / 1024.0);
    }
    
    // Save current counts before unmapping
    uint32_t saved_node_count = g.node_count;
    uint32_t saved_edge_count = g.edge_count;
    
    size_t node_section = sizeof(uint32_t) * 4 + (size_t)new_node_cap * sizeof(Node);
    size_t edge_section = (size_t)new_edge_cap * sizeof(Edge);
    size_t new_size = node_section + edge_section;
    
    munmap(mmap_base, mmap_size);
    
    if (ftruncate(mmap_fd, new_size) < 0) {
        fprintf(stderr, "[ERROR] Failed to grow file\n");
        exit(1);
    }
    
    mmap_base = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
    if (mmap_base == MAP_FAILED) {
        fprintf(stderr, "[ERROR] Failed to remap file\n");
        exit(1);
    }
    
    mmap_size = new_size;
    
    // Update header with new capacities AND restore counts
    uint32_t *header = (uint32_t *)mmap_base;
    header[0] = saved_node_count;
    header[1] = new_node_cap;
    header[2] = saved_edge_count;
    header[3] = new_edge_cap;
    
    g.node_cap = new_node_cap;
    g.edge_cap = new_edge_cap;
    
    // Update pointers after remap
    g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t) * 4);
    g.edges = (Edge *)((char *)mmap_base + node_section);
}

/* ========================================================================
 * NODE CREATION - Simple: 1 word = 1 node
 * ======================================================================== */

uint32_t create_node(uint8_t *token, uint32_t len) {
    // REUSE if exact token exists
    uint32_t cmp_len = (len < 16) ? len : 16;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len != len) continue;
        
        int match = 1;
        for (uint32_t b = 0; b < cmp_len; b++) {
            if (g.nodes[i].token[b] != token[b]) {
                match = 0;
                break;
            }
        }
        
        if (match) {
            g.nodes[i].frequency++;
            return i;
        }
    }
    
    // Auto-grow if needed (silent, like melvin_core.c)
    if (g.node_count >= g.node_cap) {
        uint32_t new_cap = g.node_cap * 2;
        graph_mmap_grow(new_cap, g.edge_cap);
    }
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    g.nodes[id].token_len = len;
    // Copy up to 16 bytes
    for (uint32_t b = 0; b < len && b < 16; b++) {
        g.nodes[id].token[b] = token[b];
    }
    g.nodes[id].frequency = 1;
    
    if (debug) {
        uint32_t print_len = (len < 16) ? len : 16;
        fprintf(stderr, "[NODE] Created #%u: '%.*s'%s\n", id, print_len, token, 
               len > 16 ? "..." : "");
    }
    
    return id;
}

/* ========================================================================
 * EDGE CREATION - The key: CREATE MANY EDGES!
 * ======================================================================== */

uint32_t create_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count) return UINT32_MAX;
    
    // Check if edge already exists using hash table (O(1)!)
    Edge *existing = edge_hash_find(from, to);
    if (existing != NULL) {
        // Strengthen existing edge
        if (existing->weight < 255) {
            existing->weight++;
        }
        return (uint32_t)(existing - g.edges);  // Return index
    }
    
    // Auto-grow if needed (silent, like melvin_core.c)
    if (g.edge_count >= g.edge_cap) {
        uint32_t old_count = g.edge_count;
        uint32_t new_cap = g.edge_cap * 2;
        graph_mmap_grow(g.node_cap, new_cap);
        
        // Rebuild hash table after grow
        memset(g.edge_hash, 0, g.edge_hash_size * sizeof(EdgeHashEntry));
        for (uint32_t i = 0; i < old_count; i++) {
            edge_hash_insert(i);
        }
    }
    
    uint32_t id = g.edge_count++;
    g.edges[id].from = from;
    g.edges[id].to = to;
    g.edges[id].weight = weight;
    
    // Insert into hash table
    edge_hash_insert(id);
    
    if (debug && g.edge_count % 10000 == 0) {
        fprintf(stderr, "[EDGES] Created %u edges\n", g.edge_count);
    }
    
    return id;
}

/* ========================================================================
 * INPUT: Fit new patterns into old patterns + create edges
 * ======================================================================== */

void sense_input(uint8_t *bytes, uint32_t len) {
    // Clear previous activation (but preserve control nodes!)
    for (uint32_t i = 0; i < g.node_count; i++) {
        // Don't reset control nodes (they store persistent parameters)
        if (g.nodes[i].token_len > 0 && g.nodes[i].token[0] == '_') {
            continue;  // Preserve control node values
        }
        g.nodes[i].activation = 0.0f;
    }
    
    // Parse words into nodes (1 word = 1 node)
    uint32_t word_nodes[1000];
    uint32_t word_count = 0;
    uint32_t word_start = 0;
    
    for (uint32_t i = 0; i <= len && word_count < 1000; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '+' || ch == '=' || i == len) {
            if (i > word_start) {
                uint32_t node_id = create_node(&bytes[word_start], i - word_start);
                if (node_id != UINT32_MAX) {
                    word_nodes[word_count++] = node_id;
                    g.nodes[node_id].activation = 1.0f;  // Direct input activation
                }
            }
            
            // Operators as separate nodes
            if (ch == '+' || ch == '=') {
                uint32_t node_id = create_node(&ch, 1);
                if (node_id != UINT32_MAX) {
                    word_nodes[word_count++] = node_id;
                    g.nodes[node_id].activation = 1.0f;
                }
            }
            
            word_start = i + 1;
        }
    }
    
    if (debug) {
        fprintf(stderr, "[INPUT] Parsed %u words\n", word_count);
    }
    
    // PATTERN FITTING: Connect new words to existing patterns
    // This is where millions of edges get created!
    for (uint32_t i = 0; i < word_count; i++) {
        uint32_t current_id = word_nodes[i];
        uint32_t edges_created = 0;
        uint32_t snapshot_node_count = g.node_count;  // Snapshot before edge creation
        
        // 1. SEQUENCE EDGES: word[i] → word[i+1]
        if (i < word_count - 1) {
            create_edge(current_id, word_nodes[i+1], 255);
            edges_created++;
        }
        
        // 2. SIMILARITY EDGES: Connect to similar existing words
        // Pre-calculate similarities to avoid accessing g.nodes during create_edge
        for (uint32_t j = 0; j < snapshot_node_count && edges_created < 100; j++) {
            if (j == current_id) continue;
            
            // Reload pointers (may have changed due to auto-grow)
            Node *current = &g.nodes[current_id];
            Node *other = &g.nodes[j];
            
            float sim = token_similarity(current, other);
            
            if (sim > 0.3f) {
                uint8_t weight = (uint8_t)(sim * 255.0f);
                create_edge(current_id, j, weight);
                edges_created++;
            }
        }
        
        if (debug && edges_created > 0) {
            fprintf(stderr, "[PATTERN] node %u connected to %u patterns\n",
                   current_id, edges_created);
        }
    }
}

/* ========================================================================
 * SELF-INTERPRETING GRAPH: Graph controls its own behavior
 * ======================================================================== */

// Find or create a control node (parameters stored as nodes)
uint32_t get_control_node(const char *name, float default_value) {
    // Search for existing control node
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].token_len == strlen(name)) {
            int match = 1;
            for (uint32_t j = 0; j < strlen(name); j++) {
                if (g.nodes[i].token[j] != name[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) return i;
        }
    }
    
    // Create new control node with default value
    uint32_t id = create_node((uint8_t *)name, strlen(name));
    g.nodes[id].activation = default_value;
    
    if (debug) {
        fprintf(stderr, "[CONTROL] Created parameter '%s' = %.2f\n", name, default_value);
    }
    
    return id;
}

// Read parameter value from graph (NO LIMITS!)
float read_param(const char *name, float default_value) {
    uint32_t node = get_control_node(name, default_value);
    return g.nodes[node].activation;
}

// Adjust parameter based on outcome (NO LIMITS!)
void adjust_param(const char *name, float delta) {
    uint32_t node = get_control_node(name, 1.0f);
    g.nodes[node].activation += delta;
    
    if (debug) {
        fprintf(stderr, "[ADJUST] %s → %.3f\n", name, g.nodes[node].activation);
    }
}

/* ========================================================================
 * SPREADING ACTIVATION: Graph determines its own dynamics!
 * ======================================================================== */

void spreading_activation() {
    if (debug) fprintf(stderr, "[SPREAD] Starting propagation...\n");
    
    // READ ALL PARAMETERS FROM GRAPH
    float decay = read_param("_decay", 0.9f);
    float saturation = read_param("_saturation", 5.0f);
    float max_iters = read_param("_max_iterations", 50.0f);
    float convergence_threshold = read_param("_convergence_threshold", 0.001f);
    float input_threshold = read_param("_input_threshold", 0.99f);
    float active_threshold = read_param("_active_threshold", 0.001f);
    
    // Count initial active (using graph threshold)
    uint32_t initial_active = 0;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].activation > input_threshold) initial_active++;
    }
    
    // Spread using ALL graph-determined parameters
    uint32_t iterations = 0;
    uint32_t saturated_count = 0;
    int converged = 0;
    
    while (iterations < (uint32_t)max_iters) {
        iterations++;
        float max_change = 0.0f;
        saturated_count = 0;
        
        float *new_act = calloc(g.node_count, sizeof(float));
        
        for (uint32_t i = 0; i < g.node_count; i++) {
            new_act[i] = g.nodes[i].activation;
        }
        
        // Spread through edges
        for (uint32_t e = 0; e < g.edge_count; e++) {
            Edge *edge = &g.edges[e];
            Node *from = &g.nodes[edge->from];
            
            if (from->activation > 0.0f) {
                float propagated = from->activation * (edge->weight / 255.0f) * decay;
                new_act[edge->to] += propagated;
            }
        }
        
        // Apply saturation
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (new_act[i] > saturation) {
                new_act[i] = saturation;
                saturated_count++;
            }
            
            float change = fabs(new_act[i] - g.nodes[i].activation);
            if (change > max_change) max_change = change;
            
            g.nodes[i].activation = new_act[i];
        }
        
        free(new_act);
        
        // Check convergence (using graph threshold)
        if (max_change < convergence_threshold) {
            converged = 1;
            break;
        }
    }
    
    // Count final active (using graph threshold)
    uint32_t final_active = 0;
    for (uint32_t i = 0; i < g.node_count; i++) {
        if (g.nodes[i].activation > active_threshold) final_active++;
    }
    
    // MINIMAL SELF-ADJUSTMENT - just detect problems, don't force solutions
    // Let the graph figure out the right adjustments through structure
    
    float saturation_ratio = (float)saturated_count / (float)g.node_count;
    
    if (saturation_ratio > 0.5f && decay < 1.0f) {
        // Too much saturation - try reducing decay slightly
        adjust_param("_decay", -0.01f);
        if (debug) fprintf(stderr, "[ADJUST] High saturation, reducing decay\n");
    }
    
    if (final_active <= initial_active && decay > 0.5f) {
        // No cascade - try reducing decay to spread more
        adjust_param("_decay", -0.02f);
        if (debug) fprintf(stderr, "[ADJUST] No cascade, reducing decay\n");
    }
    
    if (debug) {
        fprintf(stderr, "[SPREAD] %u iters, %u→%u nodes, %u sat (%.1f%%)\n",
               iterations, initial_active, final_active, 
               saturated_count, saturation_ratio * 100.0f);
        fprintf(stderr, "[PARAMS] decay=%.3f sat=%.1f\n", decay, saturation);
    }
}

/* ========================================================================
 * OUTPUT: Show activated nodes
 * ======================================================================== */

void emit_output() {
    uint8_t output[1048576];
    uint32_t output_len = 0;
    
    // Read output thresholds from graph
    float output_min = read_param("_output_min", 0.001f);
    float output_max = read_param("_output_max", 0.99f);
    
    // Output nodes activated above threshold (but not direct input or control nodes)
    for (uint32_t i = 0; i < g.node_count; i++) {
        // Skip control nodes (start with '_')
        if (g.nodes[i].token_len > 0 && g.nodes[i].token[0] == '_') continue;
        
        // Must be activated, but not at full strength (those are direct input)
        if (g.nodes[i].activation > output_min && 
            g.nodes[i].activation < output_max) {
            
            for (uint32_t b = 0; b < g.nodes[i].token_len; b++) {
                if (output_len < sizeof(output)) {
                    output[output_len++] = g.nodes[i].token[b];
                }
            }
            if (output_len < sizeof(output)) {
                output[output_len++] = ' ';
            }
        }
    }
    
    if (output_len > 0) {
        write(STDOUT_FILENO, output, output_len);
    }
    write(STDOUT_FILENO, "\n", 1);
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    // UNLIMITED: Start tiny, auto-grow as needed (like melvin_core.c)
    uint32_t initial_nodes = 256;       // Start: 256 nodes (6 KB)
    uint32_t initial_edges = 1024;      // Start: 1K edges (10 KB)
    
    mmap_fd = open("graph.mmap", O_RDWR | O_CREAT, 0644);
    struct stat st;
    int exists = (fstat(mmap_fd, &st) == 0 && st.st_size > 0);
    
    if (exists) {
        // Load existing graph
        mmap_size = st.st_size;
        mmap_base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
        
        uint32_t *header = (uint32_t *)mmap_base;
        g.node_count = header[0];
        g.node_cap = header[1];
        g.edge_count = header[2];
        g.edge_cap = header[3];
        
        size_t node_section = sizeof(uint32_t) * 4 + (size_t)g.node_cap * sizeof(Node);
        g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t) * 4);
        g.edges = (Edge *)((char *)mmap_base + node_section);
        
        if (debug) {
            fprintf(stderr, "[LOAD] %u/%u nodes, %u/%u edges (%.1f MB)\n", 
                   g.node_count, g.node_cap, g.edge_count, g.edge_cap,
                   mmap_size / 1024.0 / 1024.0);
        }
    } else {
        // Create new graph - start small!
        size_t node_section = sizeof(uint32_t) * 4 + (size_t)initial_nodes * sizeof(Node);
        size_t edge_section = (size_t)initial_edges * sizeof(Edge);
        mmap_size = node_section + edge_section;
        
        ftruncate(mmap_fd, mmap_size);
        mmap_base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
        
        uint32_t *header = (uint32_t *)mmap_base;
        header[0] = 0;  // node_count
        header[1] = initial_nodes;  // node_cap
        header[2] = 0;  // edge_count
        header[3] = initial_edges;  // edge_cap
        
        g.node_count = 0;
        g.node_cap = initial_nodes;
        g.edge_count = 0;
        g.edge_cap = initial_edges;
        
        g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t) * 4);
        g.edges = (Edge *)((char *)mmap_base + node_section);
        
        if (debug) {
            fprintf(stderr, "[NEW] Start: %u nodes, %u edges (%.1f KB) - will auto-grow!\n",
                   initial_nodes, initial_edges, mmap_size / 1024.0);
        }
    }
    
    // Allocate hash table separately (not in mmap - transient structure)
    // Start small, will auto-grow (like melvin_core.c)
    g.edge_hash_size = 16384;  // Must be power of 2
    g.edge_hash = calloc(g.edge_hash_size, sizeof(EdgeHashEntry));
    if (!g.edge_hash) {
        fprintf(stderr, "[FATAL] Out of memory allocating edge hash table\n");
        exit(1);
    }
    
    if (debug) {
        fprintf(stderr, "[HASH] Allocated %u buckets (%.1f MB)\n", 
               g.edge_hash_size, 
               (g.edge_hash_size * sizeof(EdgeHashEntry)) / 1024.0 / 1024.0);
    }
    
    // Rebuild hash table from existing edges (if loading)
    if (exists && g.edge_count > 0) {
        if (debug) fprintf(stderr, "[HASH] Rebuilding from %u edges...\n", g.edge_count);
        for (uint32_t i = 0; i < g.edge_count; i++) {
            edge_hash_insert(i);
        }
        if (debug) fprintf(stderr, "[HASH] Complete\n");
    }
    
    // NO PARAMETERS - Pure emergence
    // Graph structure determines all dynamics
    
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    
    uint8_t input[1048576];
    int idle = 0;
    
    while (idle < 100) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        
        if (n > 0) {
            sense_input(input, n);       // Parse input + fit to patterns + create edges
            spreading_activation();      // CASCADE through the network!
            emit_output();               // Show activated nodes
            g.tick++;
            idle = 0;
        } else {
            idle++;
            usleep(10000);
        }
    }
    
    // Save state
    if (debug) {
        printf("\n[FINAL] %u nodes, %u edges\n", g.node_count, g.edge_count);
        printf("[FINAL] Avg out-degree: %.1f edges/node\n", 
               g.node_count > 0 ? (float)g.edge_count / g.node_count : 0.0f);
    }
    
    uint32_t *header = (uint32_t *)mmap_base;
    header[0] = g.node_count;
    header[1] = g.node_cap;
    header[2] = g.edge_count;
    header[3] = g.edge_cap;
    msync(mmap_base, mmap_size, MS_SYNC);
    munmap(mmap_base, mmap_size);
    close(mmap_fd);
    
    // Free hash table
    free(g.edge_hash);
    
    return 0;
}



