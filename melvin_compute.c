/*
 * MELVIN COMPUTE - Self-Arranging Computational Network
 * 
 * Key Innovation: DATA AS CODE, CODE AS DATA
 * 
 * Instead of implementing arithmetic, Melvin LEARNS computational patterns
 * through spreading activation. The network arranges itself to compute!
 * 
 * Teaching: "2 + 3 = 5" creates edges
 * Querying: "2 + 3 = ?" activates through those edges
 * Result: "5" emerges from spreading activation!
 * 
 * This is ANALOGY-BASED COMPUTATION - like how humans do mental math
 * by recalling similar problems, not by running algorithms.
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

// Node types - everything is a node!
#define NODE_DATA      0  // Regular word/data
#define NODE_NUMBER    1  // Numeric value
#define NODE_OPERATOR  2  // +, -, *, /, =
#define NODE_PATTERN   3  // Compound pattern like "2+3"

// 26-byte node (added node_type)
typedef struct __attribute__((packed)) {
    uint8_t token[16];
    float activation;
    uint16_t token_len;
    uint16_t frequency;
    uint8_t node_type;
    uint8_t reserved;  // Padding
} Node;

// 10-byte edge (unchanged)
typedef struct __attribute__((packed)) {
    uint32_t from;
    uint32_t to;
    uint8_t weight;
    uint8_t times_fired;
} Edge;

typedef struct {
    uint64_t key;
    uint32_t edge_idx;
} EdgeHashEntry;

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    
    Edge *edges;
    uint32_t edge_count;
    uint32_t edge_cap;
    
    EdgeHashEntry *edge_hash;
    uint32_t edge_hash_size;
    
    uint64_t tick;
    
    float decay_factor;
    uint32_t max_hops;
    float min_activation;
} Graph;

Graph g;
void *mmap_base = NULL;
int mmap_fd = -1;
size_t mmap_size = 0;
int debug = 0;

/* ========================================================================
 * UTILITIES
 * ======================================================================== */

static inline uint64_t edge_hash_key(uint32_t from, uint32_t to) {
    return ((uint64_t)from << 32) | (uint64_t)to;
}

static inline uint32_t edge_hash_func(uint64_t key, uint32_t size) {
    return (uint32_t)((key * 2654435761ULL) & (size - 1));
}

void edge_hash_insert(uint32_t edge_idx) {
    Edge *e = &g.edges[edge_idx];
    uint64_t key = edge_hash_key(e->from, e->to);
    uint32_t hash = edge_hash_func(key, g.edge_hash_size);
    
    for (uint32_t i = 0; i < g.edge_hash_size; i++) {
        uint32_t idx = (hash + i) & (g.edge_hash_size - 1);
        if (g.edge_hash[idx].key == 0 || g.edge_hash[idx].key == key) {
            g.edge_hash[idx].key = key;
            g.edge_hash[idx].edge_idx = edge_idx;
            return;
        }
    }
}

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

/* ========================================================================
 * NODE TYPE INFERENCE - The key to computational understanding!
 * ======================================================================== */

uint8_t infer_node_type(uint8_t *token, uint32_t len) {
    if (len == 0) return NODE_DATA;
    
    // Check if number (all digits)
    int is_number = 1;
    for (uint32_t i = 0; i < len && i < 16; i++) {
        if (token[i] < '0' || token[i] > '9') {
            is_number = 0;
            break;
        }
    }
    if (is_number) return NODE_NUMBER;
    
    // Check if operator
    if (len == 1) {
        char ch = token[0];
        if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '=') {
            return NODE_OPERATOR;
        }
    }
    
    // Check if compound pattern (contains operator)
    for (uint32_t i = 0; i < len && i < 16; i++) {
        char ch = token[i];
        if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
            return NODE_PATTERN;
        }
    }
    
    return NODE_DATA;
}

/* ========================================================================
 * MMAP AUTO-GROW
 * ======================================================================== */

void graph_mmap_grow(uint32_t new_node_cap, uint32_t new_edge_cap) {
    if (mmap_base == NULL || mmap_fd < 0) return;
    
    if (debug) {
        fprintf(stderr, "[GROW] %u→%u nodes, %u→%u edges\n",
               g.node_cap, new_node_cap, g.edge_cap, new_edge_cap);
    }
    
    uint32_t saved_node_count = g.node_count;
    uint32_t saved_edge_count = g.edge_count;
    
    size_t node_section = sizeof(uint32_t) * 4 + (size_t)new_node_cap * sizeof(Node);
    size_t edge_section = (size_t)new_edge_cap * sizeof(Edge);
    size_t new_size = node_section + edge_section;
    
    munmap(mmap_base, mmap_size);
    ftruncate(mmap_fd, new_size);
    
    mmap_base = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
    if (mmap_base == MAP_FAILED) {
        fprintf(stderr, "[ERROR] Failed to remap\n");
        exit(1);
    }
    
    mmap_size = new_size;
    
    uint32_t *header = (uint32_t *)mmap_base;
    header[0] = saved_node_count;
    header[1] = new_node_cap;
    header[2] = saved_edge_count;
    header[3] = new_edge_cap;
    
    g.node_cap = new_node_cap;
    g.edge_cap = new_edge_cap;
    
    g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t) * 4);
    g.edges = (Edge *)((char *)mmap_base + node_section);
}

/* ========================================================================
 * NODE & EDGE CREATION
 * ======================================================================== */

uint32_t create_node(uint8_t *token, uint32_t len, uint8_t node_type) {
    // Reuse if exact match exists
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
    
    if (g.node_count >= g.node_cap) {
        graph_mmap_grow(g.node_cap * 2, g.edge_cap);
    }
    
    uint32_t id = g.node_count++;
    memset(&g.nodes[id], 0, sizeof(Node));
    g.nodes[id].token_len = len;
    for (uint32_t b = 0; b < len && b < 16; b++) {
        g.nodes[id].token[b] = token[b];
    }
    g.nodes[id].frequency = 1;
    g.nodes[id].node_type = node_type;
    
    if (debug) {
        const char *type_str = (node_type == NODE_NUMBER) ? "NUM" :
                               (node_type == NODE_OPERATOR) ? "OP" :
                               (node_type == NODE_PATTERN) ? "PAT" : "DATA";
        fprintf(stderr, "[NODE] %s #%u: '%.*s'\n", type_str, id, 
               (len < 16) ? len : 16, token);
    }
    
    return id;
}

uint32_t create_edge(uint32_t from, uint32_t to, uint8_t weight) {
    if (from >= g.node_count || to >= g.node_count) return UINT32_MAX;
    
    Edge *existing = edge_hash_find(from, to);
    if (existing != NULL) {
        if (existing->weight < 255) existing->weight++;
        return (uint32_t)(existing - g.edges);
    }
    
    if (g.edge_count >= g.edge_cap) {
        uint32_t old_count = g.edge_count;
        graph_mmap_grow(g.node_cap, g.edge_cap * 2);
        
        memset(g.edge_hash, 0, g.edge_hash_size * sizeof(EdgeHashEntry));
        for (uint32_t i = 0; i < old_count; i++) {
            edge_hash_insert(i);
        }
    }
    
    uint32_t id = g.edge_count++;
    g.edges[id].from = from;
    g.edges[id].to = to;
    g.edges[id].weight = weight;
    g.edges[id].times_fired = 0;
    
    edge_hash_insert(id);
    
    return id;
}

/* ========================================================================
 * COMPUTATIONAL PATTERN DETECTION - The magic happens here!
 * ======================================================================== */

typedef struct {
    uint32_t left_operand;   // "2"
    uint32_t operator;        // "+"
    uint32_t right_operand;  // "3"
    uint32_t equals;         // "="
    uint32_t result;         // "5"
    uint8_t is_equation;     // Has result (teaching)
    uint8_t is_query;        // Missing result (asking)
} ComputePattern;

ComputePattern detect_compute_pattern(uint32_t *nodes, uint32_t count) {
    ComputePattern p = {0};
    p.left_operand = UINT32_MAX;
    p.operator = UINT32_MAX;
    p.right_operand = UINT32_MAX;
    p.equals = UINT32_MAX;
    p.result = UINT32_MAX;
    
    // Look for: NUMBER OPERATOR NUMBER [= NUMBER]
    for (uint32_t i = 0; i < count; i++) {
        Node *n = &g.nodes[nodes[i]];
        
        if (n->node_type == NODE_NUMBER) {
            if (p.left_operand == UINT32_MAX) {
                p.left_operand = nodes[i];
            } else if (p.right_operand == UINT32_MAX && p.operator != UINT32_MAX) {
                p.right_operand = nodes[i];
            } else if (p.equals != UINT32_MAX && p.result == UINT32_MAX) {
                p.result = nodes[i];
            }
        } else if (n->node_type == NODE_OPERATOR) {
            if (n->token[0] == '=') {
                p.equals = nodes[i];
            } else if (p.operator == UINT32_MAX) {
                p.operator = nodes[i];
            }
        }
    }
    
    // Valid equation: X OP Y = Z
    if (p.left_operand != UINT32_MAX && p.operator != UINT32_MAX &&
        p.right_operand != UINT32_MAX && p.equals != UINT32_MAX &&
        p.result != UINT32_MAX) {
        p.is_equation = 1;
    }
    
    // Valid query: X OP Y (or X OP Y = ?)
    if (p.left_operand != UINT32_MAX && p.operator != UINT32_MAX &&
        p.right_operand != UINT32_MAX && !p.is_equation) {
        p.is_query = 1;
    }
    
    return p;
}

/* ========================================================================
 * COMPUTATIONAL EXECUTION - Self-arranging computation!
 * ======================================================================== */

uint32_t create_pattern_node(ComputePattern *p) {
    // Create compound pattern: "2+3"
    uint8_t pattern[16] = {0};
    uint32_t len = 0;
    
    // Concatenate: left + op + right
    Node *left = &g.nodes[p->left_operand];
    Node *op = &g.nodes[p->operator];
    Node *right = &g.nodes[p->right_operand];
    
    for (uint32_t i = 0; i < left->token_len && len < 16; i++) {
        pattern[len++] = left->token[i];
    }
    for (uint32_t i = 0; i < op->token_len && len < 16; i++) {
        pattern[len++] = op->token[i];
    }
    for (uint32_t i = 0; i < right->token_len && len < 16; i++) {
        pattern[len++] = right->token[i];
    }
    
    uint8_t type = NODE_PATTERN;
    return create_node(pattern, len, type);
}

void learn_computation(ComputePattern *p) {
    // Create pattern node "2+3" and link to result "5"
    uint32_t pattern_node = create_pattern_node(p);
    
    // Create STRONG edge: "2+3" → "5"
    create_edge(pattern_node, p->result, 255);
    
    // Also create edges from components (helps with spreading)
    create_edge(p->left_operand, p->result, 128);
    create_edge(p->right_operand, p->result, 128);
    
    if (debug) {
        Node *pat = &g.nodes[pattern_node];
        Node *res = &g.nodes[p->result];
        fprintf(stderr, "[LEARN] '%.*s' → '%.*s'\n",
               pat->token_len, pat->token, res->token_len, res->token);
    }
}

void query_computation(ComputePattern *p) {
    // Create pattern node for query
    uint32_t pattern_node = create_pattern_node(p);
    
    // Activate it strongly - spreading activation will find the answer!
    g.nodes[pattern_node].activation = 1.0f;
    
    if (debug) {
        Node *pat = &g.nodes[pattern_node];
        fprintf(stderr, "[QUERY] Looking for answer to '%.*s'\n",
               pat->token_len, pat->token);
    }
}

void execute_computational_patterns(uint32_t *input_nodes, uint32_t count) {
    ComputePattern p = detect_compute_pattern(input_nodes, count);
    
    if (p.is_equation) {
        if (debug) fprintf(stderr, "[DETECT] Equation detected - learning!\n");
        learn_computation(&p);
    } else if (p.is_query) {
        if (debug) fprintf(stderr, "[DETECT] Query detected - searching!\n");
        query_computation(&p);
    }
}

/* ========================================================================
 * INPUT PROCESSING
 * ======================================================================== */

void sense_input(uint8_t *bytes, uint32_t len) {
    for (uint32_t i = 0; i < g.node_count; i++) {
        g.nodes[i].activation = 0.0f;
    }
    
    uint32_t input_nodes[1000];
    uint32_t input_count = 0;
    uint32_t word_start = 0;
    
    for (uint32_t i = 0; i <= len && input_count < 1000; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';
        
        if (ch == ' ' || ch == '\n' || ch == '\t' || ch == '?' || i == len) {
            if (i > word_start) {
                uint8_t node_type = infer_node_type(&bytes[word_start], i - word_start);
                uint32_t node_id = create_node(&bytes[word_start], i - word_start, node_type);
                
                if (node_id != UINT32_MAX) {
                    input_nodes[input_count++] = node_id;
                    g.nodes[node_id].activation = 1.0f;
                }
            }
            word_start = i + 1;
        }
    }
    
    // COMPUTATIONAL MAGIC: Detect and execute patterns!
    execute_computational_patterns(input_nodes, input_count);
}

/* ========================================================================
 * SPREADING ACTIVATION
 * ======================================================================== */

void spreading_activation() {
    if (debug) fprintf(stderr, "[SPREAD] Propagating activation...\n");
    
    for (uint32_t hop = 0; hop < g.max_hops; hop++) {
        int any_activated = 0;
        
        for (uint32_t e = 0; e < g.edge_count; e++) {
            Edge *edge = &g.edges[e];
            Node *from = &g.nodes[edge->from];
            Node *to = &g.nodes[edge->to];
            
            if (from->activation < g.min_activation) continue;
            
            float coherence_boost = 1.0f + (edge->times_fired / 100.0f);
            if (coherence_boost > 2.0f) coherence_boost = 2.0f;
            
            float new_activation = from->activation * 
                                   (edge->weight / 255.0f) *
                                   g.decay_factor *
                                   coherence_boost;
            
            if (new_activation > to->activation) {
                to->activation = new_activation;
                edge->times_fired++;
                any_activated = 1;
            }
        }
        
        if (!any_activated) break;
    }
    
    if (debug) {
        uint32_t active = 0;
        for (uint32_t i = 0; i < g.node_count; i++) {
            if (g.nodes[i].activation > g.min_activation) active++;
        }
        fprintf(stderr, "[SPREAD] %u nodes activated\n", active);
    }
}

/* ========================================================================
 * OUTPUT
 * ======================================================================== */

void emit_output() {
    // Find most activated NUMBER node (that wasn't direct input)
    float max_activation = 0.0f;
    uint32_t best_node = UINT32_MAX;
    
    for (uint32_t i = 0; i < g.node_count; i++) {
        Node *n = &g.nodes[i];
        
        // Must be: activated, not full input strength, and a NUMBER
        if (n->activation > g.min_activation && 
            n->activation < 0.99f &&
            n->node_type == NODE_NUMBER &&
            n->activation > max_activation) {
            max_activation = n->activation;
            best_node = i;
        }
    }
    
    if (best_node != UINT32_MAX) {
        Node *answer = &g.nodes[best_node];
        printf("Answer: %.*s (activation: %.3f)\n", 
               answer->token_len, answer->token, answer->activation);
    } else {
        printf("No answer found\n");
    }
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main() {
    if (getenv("MELVIN_DEBUG")) debug = 1;
    
    uint32_t initial_nodes = 256;
    uint32_t initial_edges = 2048;
    
    mmap_fd = open("compute.mmap", O_RDWR | O_CREAT, 0644);
    struct stat st;
    int exists = (fstat(mmap_fd, &st) == 0 && st.st_size > 0);
    
    if (exists) {
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
            fprintf(stderr, "[LOAD] %u nodes, %u edges\n", g.node_count, g.edge_count);
        }
    } else {
        size_t node_section = sizeof(uint32_t) * 4 + (size_t)initial_nodes * sizeof(Node);
        size_t edge_section = (size_t)initial_edges * sizeof(Edge);
        mmap_size = node_section + edge_section;
        
        ftruncate(mmap_fd, mmap_size);
        mmap_base = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, mmap_fd, 0);
        
        uint32_t *header = (uint32_t *)mmap_base;
        header[0] = 0;
        header[1] = initial_nodes;
        header[2] = 0;
        header[3] = initial_edges;
        
        g.node_count = 0;
        g.node_cap = initial_nodes;
        g.edge_count = 0;
        g.edge_cap = initial_edges;
        
        g.nodes = (Node *)((char *)mmap_base + sizeof(uint32_t) * 4);
        g.edges = (Edge *)((char *)mmap_base + node_section);
        
        if (debug) {
            fprintf(stderr, "[NEW] Created compute graph\n");
        }
    }
    
    g.edge_hash_size = 16384;
    g.edge_hash = calloc(g.edge_hash_size, sizeof(EdgeHashEntry));
    
    if (exists && g.edge_count > 0) {
        for (uint32_t i = 0; i < g.edge_count; i++) {
            edge_hash_insert(i);
        }
    }
    
    g.decay_factor = 0.9f;    // High decay for precise answers
    g.max_hops = 5;
    g.min_activation = 0.01f;
    
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
    
    uint8_t input[1048576];
    int idle = 0;
    
    while (idle < 100) {
        ssize_t n = read(STDIN_FILENO, input, sizeof(input));
        
        if (n > 0) {
            sense_input(input, n);
            spreading_activation();
            emit_output();
            g.tick++;
            idle = 0;
        } else {
            idle++;
            usleep(10000);
        }
    }
    
    // Save state
    uint32_t *header = (uint32_t *)mmap_base;
    header[0] = g.node_count;
    header[1] = g.node_cap;
    header[2] = g.edge_count;
    header[3] = g.edge_cap;
    msync(mmap_base, mmap_size, MS_SYNC);
    munmap(mmap_base, mmap_size);
    close(mmap_fd);
    free(g.edge_hash);
    
    return 0;
}

