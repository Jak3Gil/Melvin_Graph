/*
 * ═══════════════════════════════════════════════════════════════════════════
 * MELVIN - Universal Neuron Learning System (Cleaned & Condensed)
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Core Features:
 *   ✅ 24-byte nodes, 10-byte edges
 *   ✅ Universal neuron (sigmoid activation)
 *   ✅ Hebbian learning
 *   ✅ Module system (pattern detection)
 *   ✅ Mmap persistence (graph.mmap file)
 *   ✅ Edge hash table (O(1) lookups)
 *   ✅ Ring buffer (input I/O)
 *   ✅ Meta-operations (graph self-modification)
 * 
 * Removed Dead Code:
 *   ❌ Hot/cold storage (never actually used)
 *   ❌ TX ring buffer (output goes directly to stdout)
 *   ❌ Execution stack (abandoned feature)
 *   ❌ OP_SPLICE/OP_FORK (useless opcodes)
 *   ❌ Module hierarchy fields (never populated)
 *   ❌ Conversation history (wasteful)
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* ========================================================================
 * COMPILE-TIME CONSTANTS
 * ======================================================================== */

#define MAX_FRAME_SIZE 4096
#define RX_RING_SIZE (MAX_FRAME_SIZE * 4)
#define TX_RING_SIZE (MAX_FRAME_SIZE * 4)

/* ========================================================================
 * NODE (24 bytes)
 * ======================================================================== */
typedef struct __attribute__((packed)) {
    uint64_t id;                // 8 bytes
    float    a;                 // 4 bytes - activation
    float    data;              // 4 bytes - theta/memory/byte value
    uint16_t in_deg;            // 2 bytes
    uint16_t out_deg;           // 2 bytes
    uint32_t last_tick_seen;    // 4 bytes
} Node;

/* ========================================================================
 * EDGE (10 bytes)
 * ======================================================================== */
typedef struct __attribute__((packed)) {
    uint32_t src;         // 4 bytes
    uint32_t dst;         // 4 bytes
    uint8_t  w_fast;      // 1 byte
    uint8_t  w_slow;      // 1 byte
} Edge;

/* Hot/cold storage removed - was never actually used */

/* ========================================================================
 * MODULE SYSTEM - Hierarchical reusable subgraphs
 * ======================================================================== */
typedef struct Module {
    uint32_t id;
    char name[64];
    
    // Subgraph structure
    uint32_t *internal_nodes;
    uint32_t node_count;
    uint32_t *internal_edges;
    uint32_t edge_count;
    
    // Interface
    uint32_t *input_nodes;
    uint32_t input_count;
    uint32_t *output_nodes;
    uint32_t output_count;
    
    // Pattern signature
    uint64_t signature_hash;
    uint32_t pattern_frequency;
    
    // Metadata
    uint32_t creation_tick;
    uint32_t last_used_tick;
} Module;

/* ========================================================================
 * EDGE HASH TABLE - O(1) edge lookups
 * ======================================================================== */
typedef struct {
    uint64_t key;      // (src << 32) | dst
    uint32_t edge_idx;
} EdgeHashEntry;

/* ========================================================================
 * RING BUFFER - Efficient I/O
 * ======================================================================== */
typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} RingBuffer;

/* ========================================================================
 * GRAPH
 * ======================================================================== */
typedef struct {
    Node   *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t next_node_id;
    
    Edge   *edges;
    uint32_t edge_count;
    uint32_t edge_cap;
    
    // Edge hash table
    EdgeHashEntry *edge_hash;
    uint32_t edge_hash_size;
    
    // Module system
    Module *modules;
    uint32_t module_count;
    uint32_t module_cap;
} Graph;

/* ========================================================================
 * SYSTEM
 * ======================================================================== */
typedef struct {
    uint64_t tick;
    
    // Ring buffer I/O
    RingBuffer rx_ring;
    uint8_t *current_frame;
    uint32_t current_frame_cap;
    uint16_t current_frame_len;
    uint8_t *last_output_frame;
    uint32_t last_output_frame_cap;
    uint16_t last_output_frame_len;
    
    // Temp arrays for propagation
    float *node_soma;
    float *node_a_prev;
    uint32_t arrays_cap;
    
    // Stats
    uint32_t active_node_count;
    uint32_t edges_created;
    uint32_t edges_pruned;
    uint32_t modules_created;
    uint32_t patterns_detected;
    
    // Self-supervised learning
    float current_reward;
    float avg_reward;
    uint32_t correct_predictions;
    uint32_t total_predictions;
    
    // Removed: conversation_history - wasteful, rarely helped
} System;

/* ========================================================================
 * GLOBALS
 * ======================================================================== */
Graph   g_graph;
System  g_sys;

// Mmap globals
static void *g_mmap_base = NULL;
static int g_mmap_fd = -1;
static size_t g_mmap_size = 0;

// Byte to node mapping
uint32_t byte_to_node[256];
uint8_t byte_node_exists[256];

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */
void cleanup();

/* ========================================================================
 * HELPERS
 * ======================================================================== */

static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

// Removed unused randf() function

// Bit-packed accessors
#define NODE_IDX(n) ((uint32_t)((n) - g_graph.nodes))
#define node_op_type(n) ((uint8_t)((n)->id >> 62))
#define node_set_op_type(n,v) ((n)->id = ((n)->id & 0x3FFFFFFFFFFFFFFFULL) | ((uint64_t)((v) & 0x3) << 62))
#define node_is_output(n) (((n)->in_deg >> 8) & 1)
#define node_set_output(n,v) ((n)->in_deg = ((n)->in_deg & 0xFEFF) | (((v) & 1) << 8))
#define node_is_protected(n) (((n)->in_deg >> 9) & 1)
#define node_set_protected(n,v) ((n)->in_deg = ((n)->in_deg & 0xFDFF) | (((v) & 1) << 9))
#define node_in_count(n) ((n)->in_deg & 0xFF)
#define node_out_count(n) ((n)->out_deg & 0xFF)
#define node_theta(n) ((n)->data)
#define node_memory_value(n) ((n)->data)
#define node_soma(n) (g_sys.node_soma[NODE_IDX(n)])
#define node_a_prev(n) (g_sys.node_a_prev[NODE_IDX(n)])

// Op types (only 2 used now - removed useless OP_SPLICE/OP_FORK)
#define OP_SUM     0
#define OP_MEMORY  1

// Extended meta-operations (encoded in node.data field)
#define META_COUNT_ACTIVE     1000.0f  // Counts active neighbors
#define META_CORRELATE        1001.0f  // Tracks co-activation with neighbors
#define META_THRESHOLD_CREATE 1002.0f  // Creates module when count > threshold
#define META_WIRE_PATTERN     1003.0f  // Creates edges from pattern
#define META_GROUP_MODULE     1004.0f  // Groups active nodes into module

// Self-improvement meta-operations
#define META_MEASURE_PERFORMANCE 1005.0f  // Tracks learning metrics (correlation rate, module rate)
#define META_ADJUST_THRESHOLD    1006.0f  // Modifies detector threshold based on performance
#define META_TUNE_LEARNING       1007.0f  // Adjusts learning rate based on weight changes
#define META_COMPUTE_REWARD      1008.0f  // Computes reward signal from prediction accuracy
#define META_DISCOVER_OBJECTIVE  1009.0f  // Discovers what the graph SHOULD be doing from data structure

/* ========================================================================
 * RING BUFFER OPERATIONS
 * ======================================================================== */

void ring_init(RingBuffer *rb, uint32_t size) {
    rb->buf = calloc(size, 1);
    if (!rb->buf) {
        fprintf(stderr, "[FATAL] Out of memory allocating ring buffer\n");
        exit(1);
    }
    rb->size = size;
    rb->head = rb->tail = rb->count = 0;
}

void ring_free(RingBuffer *rb) {
    free(rb->buf);
}

uint32_t ring_write(RingBuffer *rb, const uint8_t *data, uint32_t len) {
    uint32_t written = 0;
    for (uint32_t i = 0; i < len && rb->count < rb->size; i++) {
        rb->buf[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->size;
        rb->count++;
        written++;
    }
    return written;
}

uint32_t ring_read(RingBuffer *rb, uint8_t *data, uint32_t len) {
    uint32_t read = 0;
    for (uint32_t i = 0; i < len && rb->count > 0; i++) {
        data[i] = rb->buf[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
        rb->count--;
        read++;
    }
    return read;
}

/* ========================================================================
 * EDGE HASH TABLE - O(1) lookups
 * ======================================================================== */

static inline uint64_t edge_hash_key(uint32_t src, uint32_t dst) {
    return ((uint64_t)src << 32) | (uint64_t)dst;
}

static inline uint32_t edge_hash_func(uint64_t key, uint32_t size) {
    return (uint32_t)((key * 2654435761ULL) & (size - 1));
}

void edge_hash_insert(Graph *g, uint32_t edge_idx) {
    // Auto-grow if needed
    uint32_t used_slots = 0;
    for (uint32_t i = 0; i < g->edge_hash_size; i++) {
        if (g->edge_hash[i].key != 0) used_slots++;
    }
    float load_factor = (float)used_slots / (float)g->edge_hash_size;
    
    if (load_factor > 0.7f) {
        uint32_t old_size = g->edge_hash_size;
        uint32_t new_size = old_size * 2;
        
        printf("[HASH GROW] %u → %u (load %.1f%%)\n", old_size, new_size, load_factor * 100.0f);
        
        EdgeHashEntry *new_hash = calloc(new_size, sizeof(EdgeHashEntry));
        if (!new_hash) {
            fprintf(stderr, "[FATAL] Out of memory growing edge hash table\n");
            cleanup();
            exit(1);
        }
        
        // Rehash existing entries
        for (uint32_t i = 0; i < old_size; i++) {
            if (g->edge_hash[i].key != 0) {
                uint32_t new_idx = edge_hash_func(g->edge_hash[i].key, new_size);
                for (uint32_t j = 0; j < new_size; j++) {
                    uint32_t slot = (new_idx + j) & (new_size - 1);
                    if (new_hash[slot].key == 0) {
                        new_hash[slot] = g->edge_hash[i];
                        break;
                    }
                }
            }
        }
        
        free(g->edge_hash);
        g->edge_hash = new_hash;
        g->edge_hash_size = new_size;
    }
    
    Edge *e = &g->edges[edge_idx];
    uint64_t key = edge_hash_key(e->src, e->dst);
    uint32_t hash = edge_hash_func(key, g->edge_hash_size);
    
    // Linear probing
    for (uint32_t i = 0; i < g->edge_hash_size; i++) {
        uint32_t idx = (hash + i) & (g->edge_hash_size - 1);
        if (g->edge_hash[idx].key == 0 || g->edge_hash[idx].key == key) {
            g->edge_hash[idx].key = key;
            g->edge_hash[idx].edge_idx = edge_idx;
            return;
        }
    }
}

void edge_hash_remove(Graph *g, uint32_t src, uint32_t dst) {
    uint64_t key = edge_hash_key(src, dst);
    uint32_t hash = edge_hash_func(key, g->edge_hash_size);
    
    for (uint32_t i = 0; i < g->edge_hash_size; i++) {
        uint32_t idx = (hash + i) & (g->edge_hash_size - 1);
        if (g->edge_hash[idx].key == key) {
            g->edge_hash[idx].key = 0;
            return;
        }
        if (g->edge_hash[idx].key == 0) return;
    }
}

Edge* edge_hash_find(Graph *g, uint32_t src, uint32_t dst) {
    uint64_t key = edge_hash_key(src, dst);
    uint32_t hash = edge_hash_func(key, g->edge_hash_size);
    
    for (uint32_t i = 0; i < g->edge_hash_size; i++) {
        uint32_t idx = (hash + i) & (g->edge_hash_size - 1);
        if (g->edge_hash[idx].key == key) {
            uint32_t edge_idx = g->edge_hash[idx].edge_idx;
            if (edge_idx < g->edge_count) {
                return &g->edges[edge_idx];
            }
        }
        if (g->edge_hash[idx].key == 0) return NULL;
    }
    return NULL;
}

/* ========================================================================
 * MMAP PERSISTENCE
 * ======================================================================== */

typedef struct {
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t next_node_id;
    uint32_t edge_count;
    uint32_t edge_cap;
    uint32_t module_count;
    uint32_t module_cap;
    uint64_t tick;
    uint32_t magic;  // 0xBEEF2024
} GraphFileHeader;

void graph_mmap_init(const char *filename, uint32_t initial_node_cap, uint32_t initial_edge_cap) {
    uint32_t initial_module_cap = 1024;  // Start at 1024, will auto-grow!
    
    size_t header_size = sizeof(GraphFileHeader);
    size_t nodes_size = initial_node_cap * sizeof(Node);
    size_t edges_size = initial_edge_cap * sizeof(Edge);
    size_t modules_size = initial_module_cap * sizeof(Module);  // DYNAMIC!
    
    g_mmap_size = header_size + nodes_size + edges_size + modules_size;
    
    g_mmap_fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (g_mmap_fd < 0) {
        fprintf(stderr, "ERROR: Failed to open %s\n", filename);
        exit(1);
    }
    
    struct stat st;
    fstat(g_mmap_fd, &st);
    int file_exists = (st.st_size > 0);
    
    if (!file_exists) {
        if (ftruncate(g_mmap_fd, g_mmap_size) < 0) {
            fprintf(stderr, "ERROR: Failed to size file\n");
            exit(1);
        }
    } else {
        g_mmap_size = st.st_size;
        printf("[RESTORE] Loading %.2f MB from disk...\n", (double)g_mmap_size / (1024.0*1024.0));
    }
    
    g_mmap_base = mmap(NULL, g_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_mmap_fd, 0);
    if (g_mmap_base == MAP_FAILED) {
        fprintf(stderr, "ERROR: Failed to mmap file\n");
        exit(1);
    }
    
    GraphFileHeader *header = (GraphFileHeader *)g_mmap_base;
    
    if (file_exists && header->node_count > 0 && header->magic == 0xBEEF2024) {
        g_graph.node_count = header->node_count;
        g_graph.node_cap = header->node_cap;
        g_graph.next_node_id = header->next_node_id;
        g_graph.edge_count = header->edge_count;
        g_graph.edge_cap = header->edge_cap;
        g_graph.module_count = header->module_count;
        g_graph.module_cap = header->module_cap;
        g_sys.tick = header->tick;
        
        printf("[RESTORE] Loaded: %u nodes, %u edges, %u modules, tick=%llu\n", 
               g_graph.node_count, g_graph.edge_count, g_graph.module_count,
               (unsigned long long)g_sys.tick);
    } else {
        header->magic = 0xBEEF2024;
        header->node_count = 0;
        header->node_cap = initial_node_cap;
        header->next_node_id = 1;
        header->edge_count = 0;
        header->edge_cap = initial_edge_cap;
        header->module_count = 0;
        header->module_cap = initial_module_cap;  // DYNAMIC!
        header->tick = 0;
        
        g_graph.node_count = 0;
        g_graph.node_cap = initial_node_cap;
        g_graph.next_node_id = 1;
        g_graph.edge_count = 0;
        g_graph.edge_cap = initial_edge_cap;
        g_graph.module_count = 0;
        g_graph.module_cap = initial_module_cap;  // DYNAMIC!
        
        printf("[MMAP INIT] Created graph.mmap: %zu bytes (modules start at %u, will grow!)\n", 
               g_mmap_size, initial_module_cap);
    }
    
    // Point to mapped memory
    g_graph.nodes = (Node *)((char *)g_mmap_base + header_size);
    g_graph.edges = (Edge *)((char *)g_graph.nodes + header->node_cap * sizeof(Node));
    g_graph.modules = (Module *)((char *)g_graph.edges + header->edge_cap * sizeof(Edge));
    
    g_sys.arrays_cap = header->node_cap;
    
    // Temp arrays
    g_sys.node_a_prev = calloc(g_sys.arrays_cap, sizeof(float));
    g_sys.node_soma = calloc(g_sys.arrays_cap, sizeof(float));
    if (!g_sys.node_a_prev || !g_sys.node_soma) {
        fprintf(stderr, "[FATAL] Out of memory allocating temp arrays\n");
        exit(1);
    }
    
    // Initialize edge hash table
    g_graph.edge_hash_size = 16384;
    g_graph.edge_hash = calloc(g_graph.edge_hash_size, sizeof(EdgeHashEntry));
    if (!g_graph.edge_hash) {
        fprintf(stderr, "[FATAL] Out of memory allocating edge hash table\n");
        exit(1);
    }
    
    if (file_exists && g_graph.edge_count > 0) {
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            edge_hash_insert(&g_graph, i);
        }
        printf("[RESTORE] Rebuilt edge hash table: %u edges\n", g_graph.edge_count);
    }
}

void graph_mmap_grow(uint32_t new_node_cap, uint32_t new_edge_cap) {
    if (g_mmap_base == NULL || g_mmap_fd < 0) return;
    
    // Auto-grow module capacity too!
    uint32_t new_module_cap = g_graph.module_cap;
    if (g_graph.module_count >= g_graph.module_cap * 0.8) {
        new_module_cap = g_graph.module_cap * 2;  // Double when 80% full
    }
    
    printf("[MMAP GROW] %u→%u nodes, %u→%u edges, %u→%u modules\n",
           g_graph.node_cap, new_node_cap, g_graph.edge_cap, new_edge_cap,
           g_graph.module_cap, new_module_cap);
    
    size_t header_size = sizeof(GraphFileHeader);
    size_t nodes_size = new_node_cap * sizeof(Node);
    size_t edges_size = new_edge_cap * sizeof(Edge);
    size_t modules_size = new_module_cap * sizeof(Module);  // DYNAMIC!
    
    size_t new_size = header_size + nodes_size + edges_size + modules_size;
    
    munmap(g_mmap_base, g_mmap_size);
    
    if (ftruncate(g_mmap_fd, new_size) < 0) {
        fprintf(stderr, "[ERROR] Failed to grow file\n");
        exit(1);
    }
    
    g_mmap_base = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_mmap_fd, 0);
    if (g_mmap_base == MAP_FAILED) {
        fprintf(stderr, "[ERROR] Failed to remap file\n");
        exit(1);
    }
    
    g_mmap_size = new_size;
    
    GraphFileHeader *header = (GraphFileHeader *)g_mmap_base;
    header->node_cap = new_node_cap;
    header->edge_cap = new_edge_cap;
    header->module_cap = new_module_cap;  // UPDATE MODULE CAP!
    g_graph.node_cap = new_node_cap;
    g_graph.edge_cap = new_edge_cap;
    g_graph.module_cap = new_module_cap;  // GRAPH CONTROLS IT!
    
    char *ptr = (char *)g_mmap_base + sizeof(GraphFileHeader);
    g_graph.nodes = (Node *)ptr; ptr += new_node_cap * sizeof(Node);
    g_graph.edges = (Edge *)ptr; ptr += new_edge_cap * sizeof(Edge);
    g_graph.modules = (Module *)ptr; ptr += new_module_cap * sizeof(Module);  // DYNAMIC!
    
    printf("[MMAP GROW] ✅ Resized to %.1f MB (modules unlimited!)\n", 
           new_size / (1024.0 * 1024.0));
}

void graph_mmap_sync() {
    if (g_mmap_base == NULL) return;
    
    GraphFileHeader *header = (GraphFileHeader *)g_mmap_base;
    header->node_count = g_graph.node_count;
    header->node_cap = g_graph.node_cap;
    header->next_node_id = g_graph.next_node_id;
    header->edge_count = g_graph.edge_count;
    header->edge_cap = g_graph.edge_cap;
    header->module_count = g_graph.module_count;
    header->module_cap = g_graph.module_cap;
    header->tick = g_sys.tick;
    
    msync(g_mmap_base, g_mmap_size, MS_SYNC);  // Use SYNC for reliability
    
    printf("[MMAP SYNC] tick=%llu nodes=%u edges=%u modules=%u\n", 
           (unsigned long long)g_sys.tick, g_graph.node_count, 
           g_graph.edge_count, g_graph.module_count);
}

void graph_mmap_close() {
    if (g_mmap_base != NULL) {
        graph_mmap_sync();
        munmap(g_mmap_base, g_mmap_size);
        g_mmap_base = NULL;
    }
    if (g_mmap_fd >= 0) {
        close(g_mmap_fd);
        g_mmap_fd = -1;
    }
}

/* ========================================================================
 * GRAPH OPERATIONS
 * ======================================================================== */

uint32_t node_create(Graph *g) {
    if (g->node_count >= g->node_cap) {
        uint32_t new_cap = g->node_cap * 2;
        printf("[GROW] Nodes: %u → %u\n", g->node_cap, new_cap);
        
        if (g_mmap_base != NULL) {
            graph_mmap_grow(new_cap, g->edge_cap);
        }
        
        float *new_soma = realloc(g_sys.node_soma, new_cap * sizeof(float));
        float *new_a_prev = realloc(g_sys.node_a_prev, new_cap * sizeof(float));
        if (!new_soma || !new_a_prev) {
            fprintf(stderr, "[FATAL] Realloc failed during node growth\n");
            cleanup();
            exit(1);
        }
        g_sys.node_soma = new_soma;
        g_sys.node_a_prev = new_a_prev;
        
        for (uint32_t i = g->node_cap; i < new_cap; i++) {
            g_sys.node_soma[i] = 0.0f;
            g_sys.node_a_prev[i] = 0.0f;
        }
        
        g_sys.arrays_cap = new_cap;
    }
    
    uint32_t idx = g->node_count++;
    Node *n = &g->nodes[idx];
    memset(n, 0, sizeof(Node));
    n->id = g->next_node_id++;
    n->data = 0.5f;
    n->last_tick_seen = g_sys.tick;
    
    if (n->id <= 100) {
        node_set_protected(n, 1);
    }
    
    return idx;
}

uint32_t edge_create(Graph *g, uint32_t src, uint32_t dst) {
    if (src >= g->node_count || dst >= g->node_count) return UINT32_MAX;
    
    if (g->edge_count >= g->edge_cap) {
        uint32_t new_cap = g->edge_cap * 2;
        printf("[GROW] Edges: %u → %u\n", g->edge_cap, new_cap);
        
        if (g_mmap_base != NULL) {
            graph_mmap_grow(g->node_cap, new_cap);
        }
    }
    
    uint32_t idx = g->edge_count++;
    Edge *e = &g->edges[idx];
    e->src = src;
    e->dst = dst;
    e->w_fast = 200;
    e->w_slow = 200;
    
    g->nodes[src].out_deg++;
    g->nodes[dst].in_deg++;
    
    g_sys.edges_created++;
    
    edge_hash_insert(g, idx);
    
    return idx;
}

void edge_delete(Graph *g, uint32_t idx) {
    if (idx >= g->edge_count) return;
    Edge *e = &g->edges[idx];
    
    // Update node degrees
    if (e->src < g->node_count) g->nodes[e->src].out_deg--;
    if (e->dst < g->node_count) g->nodes[e->dst].in_deg--;
    
    // Remove from hash
    edge_hash_remove(g, e->src, e->dst);
    
    // Mark as deleted (w_fast = 0 means deleted)
    e->w_fast = 0;
    e->w_slow = 0;
    
    g_sys.edges_pruned++;
}

void edge_compact(Graph *g) {
    uint32_t write_idx = 0;
    
    // Compact: move non-deleted edges forward
    for (uint32_t read_idx = 0; read_idx < g->edge_count; read_idx++) {
        if (g->edges[read_idx].w_fast > 0) {
            if (write_idx != read_idx) {
                g->edges[write_idx] = g->edges[read_idx];
            }
            write_idx++;
        }
    }
    
    uint32_t deleted = g->edge_count - write_idx;
    g->edge_count = write_idx;
    
    // Rebuild hash table
    memset(g->edge_hash, 0, g->edge_hash_size * sizeof(EdgeHashEntry));
    for (uint32_t i = 0; i < g->edge_count; i++) {
        edge_hash_insert(g, i);
    }
    
    printf("[COMPACT] Removed %u deleted edges, %u remain\n", deleted, g->edge_count);
}

Edge* find_edge(Graph *g, uint32_t src, uint32_t dst) {
    return edge_hash_find(g, src, dst);
}

/* ========================================================================
 * MODULE OPERATIONS
 * ======================================================================== */

void module_free(Module *m) {
    if (m->internal_nodes) {
        free(m->internal_nodes);
        m->internal_nodes = NULL;
    }
    if (m->internal_edges) {
        free(m->internal_edges);
        m->internal_edges = NULL;
    }
    if (m->input_nodes) {
        free(m->input_nodes);
        m->input_nodes = NULL;
    }
    if (m->output_nodes) {
        free(m->output_nodes);
        m->output_nodes = NULL;
    }
}

uint32_t module_create(Graph *g, uint32_t *nodes, uint32_t node_count) {
    // AUTO-GROW: No artificial limit!
    if (g->module_count >= g->module_cap) {
        uint32_t new_cap = g->module_cap * 2;
        printf("[GROW] Modules: %u → %u (unlimited!)\n", g->module_cap, new_cap);
        
        if (g_mmap_base != NULL) {
            // Need to remap with larger module space
            graph_mmap_grow(g->node_cap, g->edge_cap);
            // Pointer is now INVALID - will re-get after incrementing count
        }
        
        // Note: Modules are in mmap, pointer updated by graph_mmap_grow
        g->module_cap = new_cap;
    }
    
    uint32_t module_idx = g->module_count++;
    
    // RE-GET pointer after potential remap
    Module *m = &g->modules[module_idx];
    memset(m, 0, sizeof(Module));
    
    m->id = module_idx;
    snprintf(m->name, 64, "module_%u", module_idx);
    m->node_count = node_count;
    m->creation_tick = g_sys.tick;
    m->last_used_tick = g_sys.tick;
    
    // Store internal nodes (allocate in regular heap, not mmap)
    m->internal_nodes = malloc(node_count * sizeof(uint32_t));
    if (!m->internal_nodes) {
        fprintf(stderr, "[FATAL] Out of memory allocating module nodes\n");
        cleanup();
        exit(1);
    }
    memcpy(m->internal_nodes, nodes, node_count * sizeof(uint32_t));
    
    // ═══════════════════════════════════════════════════════════════════
    // HIERARCHICAL: Create meta-node representing this module
    // ═══════════════════════════════════════════════════════════════════
    uint32_t meta_node = node_create(g);
    if (meta_node != UINT32_MAX) {
        // Meta-node represents the module at a higher level
        node_set_op_type(&g->nodes[meta_node], OP_MEMORY);
        g->nodes[meta_node].data = 10000.0f + (float)module_idx;  // Encode module ID
        node_set_protected(&g->nodes[meta_node], 1);  // Protect from pruning
        
        // Wire: internal nodes → meta-node (module activation)
        for (uint32_t i = 0; i < node_count && i < 32; i++) {
            if (nodes[i] < g->node_count) {
                edge_create(g, nodes[i], meta_node);
            }
        }
        
        printf("[MODULE] Created module %u with %u nodes → meta-node[%u]\n", 
               module_idx, node_count, meta_node);
        printf("  Internal nodes: ");
        for (uint32_t i = 0; i < node_count && i < 5; i++) {
            printf("%u ", nodes[i]);
        }
        if (node_count > 5) printf("...");
        printf("\n");
    }
    
    g_sys.modules_created++;
    
    return module_idx;
}

/* ========================================================================
 * META-OPERATION EXECUTOR - Graph modifies itself
 * ======================================================================== */

void execute_meta_op(Node *n, float meta_op_code) {
    uint32_t node_idx = NODE_IDX(n);
    
    if (meta_op_code >= META_COUNT_ACTIVE && meta_op_code < META_COUNT_ACTIVE + 1.0f) {
        // COUNT_ACTIVE: Count how many neighbors are active, store in memory
        uint32_t active_count = 0;
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            Edge *e = &g_graph.edges[i];
            if (e->src == node_idx && e->dst < g_graph.node_count) {
                if (g_graph.nodes[e->dst].a > 0.5f) active_count++;
            }
        }
        node_memory_value(n) = (float)active_count;
    }
    else if (meta_op_code >= META_CORRELATE && meta_op_code < META_CORRELATE + 1.0f) {
        // CORRELATE: Track co-activation with neighbors
        // Use w_slow of outgoing edges as correlation counter
        static uint32_t correlation_debug = 0;
        uint32_t correlations_found = 0;
        
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            Edge *e = &g_graph.edges[i];
            if (e->src == node_idx && e->dst < g_graph.node_count) {
                Node *neighbor = &g_graph.nodes[e->dst];
                if (n->a > 0.5f && neighbor->a > 0.5f) {
                    // Both active - increment correlation
                    if (e->w_slow < 255) {
                        e->w_slow++;
                        correlations_found++;
                        
                        if (correlation_debug < 3 && e->w_slow > 5) {
                            printf("[META-CORRELATE] Edge %u→%u: correlation=%u\n",
                                   e->src, e->dst, e->w_slow);
                            correlation_debug++;
                        }
                    }
                }
            }
        }
        
        if (correlations_found > 0 && g_sys.tick % 10 == 0) {
            printf("[META-CORRELATE] Tick %llu: Found %u co-activations\n",
                   (unsigned long long)g_sys.tick, correlations_found);
        }
    }
    else if (meta_op_code >= META_THRESHOLD_CREATE && meta_op_code < META_THRESHOLD_CREATE + 1.0f) {
        // THRESHOLD_CREATE: Create module when correlation threshold reached
        // Threshold is stored in THIS node's soma (graph controls it!)
        float threshold = node_soma(n);  // Graph sets the threshold!
        if (threshold < 10.0f) threshold = 50.0f;  // Sensible default
        
        // Check incoming edges for high w_slow (correlation count)
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            Edge *e = &g_graph.edges[i];
            if (e->dst == node_idx && (float)e->w_slow > threshold) {
                // High correlation detected! Extract pattern
                uint32_t pattern_nodes[256];  // UNLIMITED (was 32)
                uint32_t count = 0;
                
                // Collect highly correlated neighbors
                for (uint32_t j = 0; j < g_graph.edge_count && count < 256; j++) {
                    Edge *e2 = &g_graph.edges[j];
                    if ((float)e2->w_slow > threshold && e2->src < g_graph.node_count) {
                        pattern_nodes[count++] = e2->src;
                    }
                }
                
                if (count > 2) {
                    module_create(&g_graph, pattern_nodes, count);
                    printf("[META-OP] Created module from %u correlated nodes (threshold=%.0f)\n", 
                           count, threshold);
                }
                break;
            }
        }
    }
    else if (meta_op_code >= META_WIRE_PATTERN && meta_op_code < META_WIRE_PATTERN + 1.0f) {
        // WIRE_PATTERN: Create edges between all currently active nodes
        static uint32_t wire_debug = 0;
        uint32_t active_nodes[256];
        uint32_t active_count = 0;
        
        for (uint32_t i = 0; i < g_graph.node_count && active_count < 256; i++) {
            if (g_graph.nodes[i].a > 0.5f) {
                active_nodes[active_count++] = i;
            }
        }
        
        // Create edges between active pairs
        uint32_t new_edges = 0;
        for (uint32_t i = 0; i < active_count; i++) {
            for (uint32_t j = i+1; j < active_count && j < i+10; j++) {
                if (!find_edge(&g_graph, active_nodes[i], active_nodes[j])) {
                    edge_create(&g_graph, active_nodes[i], active_nodes[j]);
                    new_edges++;
                }
            }
        }
        
        if (new_edges > 0 && wire_debug < 5) {
            printf("[META-WIRE] Created %u edges between %u active nodes\n",
                   new_edges, active_count);
            wire_debug++;
        }
    }
    else if (meta_op_code >= META_GROUP_MODULE && meta_op_code < META_GROUP_MODULE + 1.0f) {
        // GROUP_MODULE: Create module from currently active nodes
        uint32_t active_nodes[256];
        uint32_t count = 0;
        
        for (uint32_t i = 0; i < g_graph.node_count && count < 256; i++) {
            if (g_graph.nodes[i].a > 0.5f) {
                active_nodes[count++] = i;
            }
        }
        
        if (count > 2) {
            module_create(&g_graph, active_nodes, count);
            printf("[META-OP] Grouped %u active nodes into module\n", count);
        }
    }
    else if (meta_op_code >= META_MEASURE_PERFORMANCE && meta_op_code < META_MEASURE_PERFORMANCE + 1.0f) {
        // MEASURE_PERFORMANCE: Track learning metrics and store in memory
        static uint32_t last_module_count = 0;
        static uint32_t last_check_tick = 0;
        static uint32_t high_correlation_edges = 0;
        
        // Count edges with high correlation
        uint32_t current_high_corr = 0;
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            if (g_graph.edges[i].w_slow > 30) current_high_corr++;
        }
        
        // Calculate rates
        uint32_t ticks_elapsed = g_sys.tick - last_check_tick;
        if (ticks_elapsed > 10) {
            uint32_t modules_created = g_graph.module_count - last_module_count;
            uint32_t corr_increase = current_high_corr - high_correlation_edges;
            
            // Performance score: modules_per_tick + correlations_per_tick
            float performance = (float)modules_created / (float)ticks_elapsed + 
                               (float)corr_increase / (float)ticks_elapsed;
            
            // Store in this node's memory (graph reads this!)
            node_memory_value(n) = performance;
            
            if (g_sys.tick % 100 == 0) {
                printf("[SELF-MONITOR] Performance=%.3f modules=%u→%u correlations=%u→%u\n",
                       performance, last_module_count, g_graph.module_count,
                       high_correlation_edges, current_high_corr);
            }
            
            last_module_count = g_graph.module_count;
            last_check_tick = g_sys.tick;
            high_correlation_edges = current_high_corr;
        }
    }
    else if (meta_op_code >= META_ADJUST_THRESHOLD && meta_op_code < META_ADJUST_THRESHOLD + 1.0f) {
        // ADJUST_THRESHOLD: Self-correcting threshold adjustment
        // Reads performance from monitor node, adjusts detector threshold
        
        // Find performance OR reward node
        float performance = 0.0f;
        
        // Prioritize reward signal (more direct measure of correctness)
        for (uint32_t i = 0; i < g_graph.node_count; i++) {
            if (g_graph.nodes[i].data >= META_COMPUTE_REWARD && 
                g_graph.nodes[i].data < META_COMPUTE_REWARD + 1.0f) {
                performance = node_memory_value(&g_graph.nodes[i]);
                break;
            }
        }
        
        // Fallback to performance monitor
        if (performance == 0.0f) {
            for (uint32_t i = 0; i < g_graph.node_count; i++) {
                if (g_graph.nodes[i].data >= META_MEASURE_PERFORMANCE && 
                    g_graph.nodes[i].data < META_MEASURE_PERFORMANCE + 1.0f) {
                    performance = node_memory_value(&g_graph.nodes[i]);
                    break;
                }
            }
        }
        
        // Find pattern detector node (node 3 in bootstrap)
        for (uint32_t i = 0; i < g_graph.node_count && i < 10; i++) {
            if (g_graph.nodes[i].data >= META_THRESHOLD_CREATE && 
                g_graph.nodes[i].data < META_THRESHOLD_CREATE + 1.0f) {
                
                float current_threshold = node_soma(&g_graph.nodes[i]);
                if (current_threshold < 10.0f) current_threshold = 50.0f;
                
                // ADAPT based on reward/performance
                // Low accuracy → lower threshold (create more patterns to explore)
                // High accuracy → raise threshold (be selective, keep what works)
                float adjustment = 0.0f;
                
                if (g_sys.total_predictions > 5) {
                    // Use prediction accuracy as guide
                    if (g_sys.avg_reward < 0.3f) {
                        adjustment = -10.0f;  // Poor predictions → explore more
                    } else if (g_sys.avg_reward > 0.7f) {
                        adjustment = 5.0f;    // Good predictions → be selective
                    }
                } else if (performance < 0.01f && g_sys.tick > 50) {
                    adjustment = -5.0f;  // No learning happening → lower threshold
                } else if (performance > 0.5f) {
                    adjustment = 2.0f;   // Too much activity → raise threshold
                }
                
                if (adjustment != 0.0f) {
                    float new_threshold = current_threshold + adjustment;
                    if (new_threshold < 10.0f) new_threshold = 10.0f;
                    if (new_threshold > 100.0f) new_threshold = 100.0f;
                    
                    // Modify detector's soma to change threshold!
                    node_soma(&g_graph.nodes[i]) = new_threshold;
                    
                    if (g_sys.total_predictions > 0) {
                        printf("[SELF-ADJUST] Threshold %.0f→%.0f (accuracy=%.1f%% → adapt!)\n",
                               current_threshold, new_threshold, g_sys.avg_reward * 100.0f);
                    } else {
                        printf("[SELF-ADJUST] Threshold %.0f→%.0f (perf=%.3f)\n",
                               current_threshold, new_threshold, performance);
                    }
                }
                break;
            }
        }
    }
    else if (meta_op_code >= META_TUNE_LEARNING && meta_op_code < META_TUNE_LEARNING + 1.0f) {
        // TUNE_LEARNING: Adjusts learning rate based on weight stability
        static float prev_avg_weight = 0.0f;
        
        float avg_weight = 0.0f;
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            avg_weight += g_graph.edges[i].w_fast;
        }
        if (g_graph.edge_count > 0) avg_weight /= g_graph.edge_count;
        
        float weight_change = fabsf(avg_weight - prev_avg_weight);
        
        // Store suggested learning rate in memory
        // High change → reduce eta, Low change → increase eta
        float suggested_eta = 0.05f;
        if (weight_change > 10.0f) {
            suggested_eta = 0.01f;  // Too volatile, slow down
        } else if (weight_change < 0.1f && g_sys.tick > 100) {
            suggested_eta = 0.1f;   // Too slow, speed up
        }
        
        node_memory_value(n) = suggested_eta;
        prev_avg_weight = avg_weight;
        
        if (g_sys.tick % 200 == 0 && suggested_eta != 0.05f) {
            printf("[SELF-TUNE] Weight change=%.2f → eta=%.3f\n", weight_change, suggested_eta);
        }
    }
    else if (meta_op_code >= META_COMPUTE_REWARD && meta_op_code < META_COMPUTE_REWARD + 1.0f) {
        // COMPUTE_REWARD: Self-supervised reward from prediction accuracy
        // Compares: What did I predict? vs What actually happened?
        
        static uint8_t prev_input[256];
        static uint32_t prev_len = 0;
        static uint8_t prev_output[256];
        static uint32_t prev_output_len = 0;
        static uint32_t reward_exec_count = 0;
        
        reward_exec_count++;
        if (reward_exec_count % 10 == 0) {
            printf("[REWARD-EXEC] Running (call #%u, prev_len=%u)\n", reward_exec_count, prev_len);
        }
        
        // Collect current output
        uint8_t current_output[256];
        uint32_t current_len = 0;
        for (uint32_t i = 0; i < g_graph.node_count && current_len < 256; i++) {
            if (node_is_output(&g_graph.nodes[i]) && g_graph.nodes[i].a > 0.3f) {
                current_output[current_len++] = (uint8_t)node_memory_value(&g_graph.nodes[i]);
            }
        }
        
        // Compute reward: How well did we predict?
        if (prev_len > 0 && g_sys.current_frame_len > 0) {
            // Check if our previous output matches current input (prediction accuracy)
            uint32_t matches = 0;
            uint32_t max_check = prev_output_len < g_sys.current_frame_len ? prev_output_len : g_sys.current_frame_len;
            
            for (uint32_t i = 0; i < max_check; i++) {
                if (prev_output[i] == g_sys.current_frame[i]) {
                    matches++;
                }
            }
            
            // Reward = prediction accuracy
            float accuracy = (max_check > 0) ? (float)matches / (float)max_check : 0.0f;
            g_sys.current_reward = accuracy;
            
            // Running average
            g_sys.total_predictions++;
            if (accuracy > 0.5f) g_sys.correct_predictions++;
            g_sys.avg_reward = (float)g_sys.correct_predictions / (float)g_sys.total_predictions;
            
            // Store reward in this node's memory (other circuits read this!)
            node_memory_value(n) = g_sys.current_reward;
            
            // Show reward frequently to prove it's working
            if (g_sys.total_predictions > 0 && g_sys.total_predictions % 5 == 0) {
                printf("[REWARD] Prediction accuracy: %.1f%% (%u/%u correct) last=%.0f%%\n",
                       g_sys.avg_reward * 100.0f, g_sys.correct_predictions, 
                       g_sys.total_predictions, accuracy * 100.0f);
                
                if (g_sys.avg_reward < 0.3f) {
                    printf("  → Low accuracy! Graph should adapt...\n");
                } else if (g_sys.avg_reward > 0.7f) {
                    printf("  → High accuracy! Graph is learning!\n");
                }
            }
        }
        
        // Save current state for next comparison
        memcpy(prev_input, g_sys.current_frame, g_sys.current_frame_len);
        prev_len = g_sys.current_frame_len;
        memcpy(prev_output, current_output, current_len);
        prev_output_len = current_len;
    }
    else if (meta_op_code >= META_DISCOVER_OBJECTIVE && meta_op_code < META_DISCOVER_OBJECTIVE + 1.0f) {
        // DISCOVER_OBJECTIVE: Infers what it should be doing from data structure
        // This is the KEY: graph figures out its own goal!
        
        static uint32_t discovery_count = 0;
        discovery_count++;
        
        // Analyze input patterns to discover objective
        uint8_t unique_bytes = 0;
        uint32_t byte_counts[256] = {0};
        
        // Count unique bytes and repetitions in recent input
        for (uint32_t i = 0; i < g_sys.current_frame_len && i < 256; i++) {
            byte_counts[g_sys.current_frame[i]]++;
        }
        for (int i = 0; i < 256; i++) {
            if (byte_counts[i] > 0) unique_bytes++;
        }
        
        // Infer objective from structure:
        float inferred_objective = 0.0f;
        char objective_type[64] = "unknown";
        
        if (unique_bytes <= 5 && g_sys.current_frame_len > 10) {
            // Low diversity, high repetition → OBJECTIVE: Predict next in sequence
            inferred_objective = 1.0f;  // Sequence prediction
            snprintf(objective_type, 64, "sequence_prediction");
        } else if (unique_bytes > 20) {
            // High diversity → OBJECTIVE: Compress/abstract
            inferred_objective = 2.0f;  // Abstraction
            snprintf(objective_type, 64, "pattern_abstraction");
        } else {
            // Medium diversity → OBJECTIVE: Learn associations
            inferred_objective = 3.0f;  // Association
            snprintf(objective_type, 64, "association_learning");
        }
        
        // Store discovered objective in memory
        node_memory_value(n) = inferred_objective;
        
        if (discovery_count % 20 == 0 && g_sys.current_frame_len > 0) {
            printf("[SELF-DISCOVER] Data structure suggests objective: %s (%.0f)\n",
                   objective_type, inferred_objective);
            printf("  → Unique bytes: %u, Length: %u\n", unique_bytes, g_sys.current_frame_len);
            printf("  → Graph now knows what to optimize for!\n");
        }
        
        // Modify other circuits based on discovered objective
        // If sequence prediction → strengthen temporal edges
        // If abstraction → lower module threshold
        // If association → increase edge creation rate
        
        for (uint32_t i = 0; i < g_graph.node_count && i < 10; i++) {
            if (g_graph.nodes[i].data >= META_THRESHOLD_CREATE && 
                g_graph.nodes[i].data < META_THRESHOLD_CREATE + 1.0f) {
                
                if (inferred_objective == 1.0f) {
                    // Sequence prediction needs MORE temporal connections
                    node_soma(&g_graph.nodes[i]) = 30.0f;  // Lower threshold
                } else if (inferred_objective == 2.0f) {
                    // Abstraction needs selective module creation
                    node_soma(&g_graph.nodes[i]) = 70.0f;  // Higher threshold
                }
                break;
            }
        }
    }
}

/* ========================================================================
 * UNIVERSAL NEURON EXECUTION
 * ======================================================================== */

static inline float execute_node(Node *n) {
    float soma = node_soma(n);
    float theta = n->data;
    uint8_t op = node_op_type(n);
    float result;
    
    // Check for extended meta-operations (encoded in theta/data field)
    if (theta >= 1000.0f && soma > 0.1f) {  // Lowered from 0.5 - meta-ops need less activation
        execute_meta_op(n, theta);
        return 1.0f;  // Meta-op executed
    }
    
    switch(op) {
        case OP_MEMORY:
            // Memory node: stores values when activated
            if (soma > theta) {
                node_memory_value(n) = soma;
                result = soma;
            } else {
                result = node_is_output(n) ? node_memory_value(n) : node_memory_value(n) / 64.0f;
            }
            break;
            
        default:
            // Universal neuron: pure sigmoid(soma - theta)
            result = sigmoid(soma - theta);
            break;
    }
    
    if (result < 0.0f) result = 0.0f;
    if (result > 1.0f) result = 1.0f;
    
    return result;
}

/* ========================================================================
 * PROPAGATION
 * ======================================================================== */

void propagate() {
    if (g_graph.node_count == 0) return;
    
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        g_sys.node_a_prev[i] = g_graph.nodes[i].a;
        g_sys.node_soma[i] = 0.0f;
    }
    
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        Edge *e = &g_graph.edges[i];
        
        // SKIP deleted edges
        if (e->w_fast == 0) continue;
        
        if (e->src >= g_graph.node_count || e->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[e->src];
        Node *dst = &g_graph.nodes[e->dst];
        
        if (src->last_tick_seen < g_sys.tick - 10) continue;
        
        float w_eff = 0.8f * e->w_slow + 0.2f * e->w_fast;
        node_soma(dst) += src->a * w_eff;
    }
    
    g_sys.active_node_count = 0;
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        Node *n = &g_graph.nodes[i];
        n->a = execute_node(n);
        
        if (n->a > 0.5f) g_sys.active_node_count++;
    }
    
    // Energy normalization
    float total_a = 0.0f;
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        total_a += g_graph.nodes[i].a;
    }
    float cap = g_graph.node_count * 0.3f;
    if (total_a > cap) {
        float scale = cap / total_a;
        for (uint32_t i = 0; i < g_graph.node_count; i++) {
            g_graph.nodes[i].a *= scale;
        }
    }
}

/* ========================================================================
 * HEBBIAN LEARNING
 * ======================================================================== */

void learn() {
    float eta = 0.05f;
    
    // Use self-supervised reward signal (defaults to 1.0 if no reward circuit)
    float reward = (g_sys.total_predictions > 0) ? 
                   (g_sys.current_reward * 2.0f) :  // Scale reward: 0.0-2.0
                   1.0f;  // Default: neutral
    
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        Edge *e = &g_graph.edges[i];
        if (e->src >= g_graph.node_count || e->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[e->src];
        Node *dst = &g_graph.nodes[e->dst];
        
        float baseline = 0.3f;
        float hebbian = (node_a_prev(src) - baseline) * (dst->a - baseline);
        float delta = eta * hebbian * reward;  // REWARD modulates learning!
        
        float new_w = (float)e->w_fast + delta * 100.0f;
        if (new_w < 1.0f) new_w = 1.0f;
        if (new_w > 255.0f) new_w = 255.0f;
        
        e->w_fast = (uint8_t)new_w;
        // DON'T overwrite w_slow - that's the correlation counter for META_CORRELATE!
        // w_slow tracks co-activation count, w_fast tracks learned weight
    }
}

/* ========================================================================
 * I/O
 * ======================================================================== */

static int g_stdin_closed = 0;

void read_input() {
    uint8_t buf[1024];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    
    if (n > 0) {
        ring_write(&g_sys.rx_ring, buf, n);
    } else if (n == 0) {
        g_stdin_closed = 1;
    }
}

void slice_frame() {
    g_sys.current_frame_len = ring_read(&g_sys.rx_ring, g_sys.current_frame, g_sys.current_frame_cap);
}

// Byte to output mapping (each byte gets both input and output node)
static uint32_t byte_to_output[256];
static uint8_t byte_output_exists[256];

void activate_input_bytes(const uint8_t *bytes, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = bytes[i];
        
        // Get or create INPUT node for this byte
        uint32_t input_idx;
        if (byte_node_exists[byte]) {
            input_idx = byte_to_node[byte];
        } else {
            input_idx = node_create(&g_graph);
            if (input_idx != UINT32_MAX) {
                byte_to_node[byte] = input_idx;
                byte_node_exists[byte] = 1;
                node_set_op_type(&g_graph.nodes[input_idx], OP_MEMORY);  // Persist activation
                g_graph.nodes[input_idx].data = 0.1f;  // Low threshold
                node_memory_value(&g_graph.nodes[input_idx]) = (float)byte;
            }
        }
        
        // Get or create OUTPUT node for this byte
        uint32_t output_idx;
        if (byte_output_exists[byte]) {
            output_idx = byte_to_output[byte];
        } else {
            output_idx = node_create(&g_graph);
            if (output_idx != UINT32_MAX) {
                byte_to_output[byte] = output_idx;
                byte_output_exists[byte] = 1;
                node_set_op_type(&g_graph.nodes[output_idx], OP_MEMORY);
                node_set_output(&g_graph.nodes[output_idx], 1);  // Mark as output!
                g_graph.nodes[output_idx].data = 0.5f;  // Activation threshold
                node_memory_value(&g_graph.nodes[output_idx]) = (float)byte;  // Store actual byte
                
                // Wire input → output (direct echo path)
                edge_create(&g_graph, input_idx, output_idx);
                
                printf("[I/O] Byte '%c'(%u) → input[%u] → output[%u]\n",
                       (byte >= 32 && byte < 127) ? byte : '?', byte, input_idx, output_idx);
            }
        }
        
        if (input_idx < g_graph.node_count) {
            g_graph.nodes[input_idx].a = 1.0f;
            node_memory_value(&g_graph.nodes[input_idx]) = 1.0f;
            g_graph.nodes[input_idx].last_tick_seen = g_sys.tick;
            
            // Create PREDICTIVE edges at multiple strides within current input
            uint32_t strides[] = {1, 2, 4, 8, 16, 32};
            for (int s = 0; s < 6; s++) {
                if (i >= strides[s]) {
                    uint32_t prev_idx = i - strides[s];
                    uint8_t prev_byte = bytes[prev_idx];
                    
                    if (byte_node_exists[prev_byte] && byte_output_exists[byte]) {
                        uint32_t prev_input_idx = byte_to_node[prev_byte];
                        uint32_t curr_output_idx = byte_to_output[byte];
                        
                        // Wire: seeing prev_byte PREDICTS current byte
                        if (!find_edge(&g_graph, prev_input_idx, curr_output_idx)) {
                            edge_create(&g_graph, prev_input_idx, curr_output_idx);
                        }
                    }
                }
            }
        }
    }
}

void emit_output() {
    static uint32_t last_output_tick = UINT32_MAX;
    static uint8_t already_output[256];
    
    // Only output once per unique tick (prevents spam)
    if (g_sys.tick == last_output_tick) return;
    last_output_tick = g_sys.tick;
    
    // Clear the already-outputted tracking
    memset(already_output, 0, sizeof(already_output));
    
    // Output all activated output nodes (deduplicated)
    uint8_t output_buffer[256];
    uint32_t output_len = 0;
    
    for (uint32_t i = 0; i < g_graph.node_count && output_len < 256; i++) {
        Node *n = &g_graph.nodes[i];
        
        if (!node_is_output(n)) continue;
        if (n->a < 0.3f) continue;
        
        uint8_t byte = (uint8_t)node_memory_value(n);
        
        // Skip if we already output this byte this tick
        if (already_output[byte]) continue;
        already_output[byte] = 1;
        
        // Check if this was in input (echo) or prediction
        int in_input = 0;
        for (uint32_t j = 0; j < g_sys.current_frame_len; j++) {
            if (g_sys.current_frame[j] == byte) {
                in_input = 1;
                break;
            }
        }
        
        output_buffer[output_len++] = byte;
        
        if (!in_input) {
            printf("[PREDICT] '%c'(%u) a=%.3f\n",
                   (byte >= 32 && byte < 127) ? byte : '?', byte, n->a);
        }
    }
    
    // Write output
    if (output_len > 0) {
        write(STDOUT_FILENO, output_buffer, output_len);
        write(STDOUT_FILENO, "\n", 1);
        
        memcpy(g_sys.last_output_frame, output_buffer, output_len);
        g_sys.last_output_frame_len = output_len;
    }
}

/* ========================================================================
 * VALIDATION
 * ======================================================================== */

void validate_graph_integrity() {
    printf("[VALIDATION] Checking %u edges...\n", g_graph.edge_count);
    
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        Edge *e = &g_graph.edges[i];
        if (e->w_fast == 0) continue;  // Deleted
        
        if (e->src >= g_graph.node_count || e->dst >= g_graph.node_count) {
            printf("[VALIDATION] ERROR: Edge %u has invalid nodes: %u→%u\n", 
                   i, e->src, e->dst);
        }
    }
    
    printf("[VALIDATION] ✓ All edges valid\n");
}

/* ========================================================================
 * MAIN LOOP
 * ======================================================================== */

void main_loop() {
    static int idle_ticks = 0;
    
    // Validate at startup
    validate_graph_integrity();
    
    while (1) {
        read_input();
        
        if (g_stdin_closed && idle_ticks++ > 5) break;
        
        slice_frame();
        
        if (g_sys.current_frame_len > 0) {
            // Check for inspection commands
            if (g_sys.current_frame_len > 4 && 
                memcmp(g_sys.current_frame, "show ", 5) == 0) {
                // Inspection command - handle and skip normal processing
                
                if (g_sys.current_frame_len >= 10 && 
                    memcmp(g_sys.current_frame + 5, "graph", 5) == 0) {
                    // "show graph" - display graph structure
                    printf("\n╔═══ GRAPH STATE ═══╗\n");
                    printf("  Nodes: %u/%u\n", g_graph.node_count, g_graph.node_cap);
                    printf("  Edges: %u/%u\n", g_graph.edge_count, g_graph.edge_cap);
                    printf("  Modules: %u/%u\n", g_graph.module_count, g_graph.module_cap);
                    printf("  Tick: %llu\n", (unsigned long long)g_sys.tick);
                    
                    // Show first 10 nodes
                    printf("\n  First 10 nodes:\n");
                    for (uint32_t i = 0; i < g_graph.node_count && i < 10; i++) {
                        Node *n = &g_graph.nodes[i];
                        printf("    [%u] op=%u theta=%.2f a=%.3f %s%s\n",
                               i, node_op_type(n), n->data, n->a,
                               node_is_output(n) ? "OUT " : "",
                               node_is_protected(n) ? "PROT" : "");
                    }
                    printf("╚═══════════════════╝\n\n");
                }
                else if (g_sys.current_frame_len >= 12 && 
                         memcmp(g_sys.current_frame + 5, "modules", 7) == 0) {
                    // "show modules" - display module information
                    printf("\n╔═══ MODULES ═══╗\n");
                    printf("  Total: %u modules\n\n", g_graph.module_count);
                    
                    for (uint32_t i = 0; i < g_graph.module_count && i < 10; i++) {
                        Module *m = &g_graph.modules[i];
                        printf("  [%u] %s\n", i, m->name);
                        printf("      Nodes: %u, Created: tick %u\n",
                               m->node_count, m->creation_tick);
                        if (m->internal_nodes) {
                            printf("      Internal: ");
                            for (uint32_t j = 0; j < m->node_count && j < 5; j++) {
                                printf("%u ", m->internal_nodes[j]);
                            }
                            if (m->node_count > 5) printf("...");
            printf("\n");
                        }
                    }
                    printf("╚═══════════════╝\n\n");
                }
                else if (g_sys.current_frame_len >= 13 && 
                         memcmp(g_sys.current_frame + 5, "circuit", 7) == 0) {
                    // "show circuit N" - trace from node N
                    uint32_t node_id = 0;
                    if (g_sys.current_frame_len > 13) {
                        node_id = g_sys.current_frame[13] - '0';
                    }
                    
                    if (node_id < g_graph.node_count) {
                        printf("\n╔═══ CIRCUIT from node %u ═══╗\n", node_id);
                        Node *n = &g_graph.nodes[node_id];
                        printf("  Node[%u]: op=%u theta=%.2f a=%.3f\n",
                               node_id, node_op_type(n), n->data, n->a);
                        
                        // Show outgoing edges
                        printf("  Outgoing edges:\n");
                        uint32_t out_count = 0;
                        for (uint32_t i = 0; i < g_graph.edge_count && out_count < 10; i++) {
        Edge *e = &g_graph.edges[i];
                            if (e->src == node_id) {
                                printf("    → [%u] w=%u/%u\n", 
                                       e->dst, e->w_fast, e->w_slow);
                                out_count++;
                            }
                        }
                        printf("╚═══════════════════════╝\n\n");
                    }
                }
                
                idle_ticks = 0;
                g_sys.tick++;
                continue;  // Skip normal processing for commands
            }
            
            activate_input_bytes(g_sys.current_frame, g_sys.current_frame_len);
            idle_ticks = 0;
        }
        
        for (int hop = 0; hop < 5; hop++) {
            propagate();
        }
        
        emit_output();
        learn();
    
    // ═══════════════════════════════════════════════════════════════════
        // INSTRUMENTATION: Track learning metrics
    // ═══════════════════════════════════════════════════════════════════
        if (g_sys.tick % 100 == 0) {
            // Weight statistics
            static float prev_avg_weight = 0.0f;
            float avg_weight = 0.0f;
            float max_weight = 0.0f;
            uint32_t strong_edges = 0;  // w > 200
            
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
                avg_weight += g_graph.edges[i].w_fast;
                if (g_graph.edges[i].w_fast > max_weight) {
                    max_weight = g_graph.edges[i].w_fast;
                }
                if (g_graph.edges[i].w_fast > 200) strong_edges++;
            }
            if (g_graph.edge_count > 0) {
                avg_weight /= g_graph.edge_count;
            }
            
            float weight_delta = fabsf(avg_weight - prev_avg_weight);
            prev_avg_weight = avg_weight;
            
            // Co-activation statistics (high w_slow = high correlation)
            uint32_t correlated_edges = 0;
            for (uint32_t i = 0; i < g_graph.edge_count; i++) {
                if (g_graph.edges[i].w_slow > 50) correlated_edges++;
            }
            
            printf("[TICK %llu] nodes=%u edges=%u modules=%u active=%u\n",
                   (unsigned long long)g_sys.tick,
                   g_graph.node_count,
                   g_graph.edge_count,
                   g_graph.module_count,
                   g_sys.active_node_count);
            
            printf("  [LEARNING] avg_w=%.1f (Δ%.2f) strong=%u/%u correlated=%u\n",
                   avg_weight, weight_delta, strong_edges, g_graph.edge_count, correlated_edges);
        }
        
        if (g_sys.tick % 1000 == 0) {
            // Compact edges if many deleted
            if (g_sys.edges_pruned > 100) {
                edge_compact(&g_graph);
                g_sys.edges_pruned = 0;
            }
            
            graph_mmap_sync();
        }
        
        g_sys.tick++;
    }
}

/* ========================================================================
 * MINIMAL BOOTSTRAP - Seed the self-programming system
 * ======================================================================== */

void bootstrap_minimal_seed() {
    // Only create if graph is empty
    if (g_graph.node_count > 0) {
        printf("[BOOTSTRAP] Graph already initialized (%u nodes), skipping seed\n", 
               g_graph.node_count);
        return;
    }
    
    printf("\n[BOOTSTRAP] Creating minimal seed circuits...\n");
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 1: THINKER - Always-active node drives everything
    // ═══════════════════════════════════════════════════════════════════
    uint32_t thinker = node_create(&g_graph);
    node_set_op_type(&g_graph.nodes[thinker], OP_SUM);  // Use SUM (sigmoid)
    g_graph.nodes[thinker].data = -5.0f;  // Negative threshold → sigmoid always ~1.0
    g_graph.nodes[thinker].a = 1.0f;
    node_set_protected(&g_graph.nodes[thinker], 1);
    
    // Self-loop with strong weight keeps it active
    uint32_t self_edge = edge_create(&g_graph, thinker, thinker);
    if (self_edge != UINT32_MAX) {
        g_graph.edges[self_edge].w_fast = 255;  // Max weight
        g_graph.edges[self_edge].w_slow = 255;
    }
    
    printf("  ✓ Thinker (node %u): self-loop maintains activation\n", thinker);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 2: EDGE CREATOR - Wires co-active nodes (Hebbian)
    // ═══════════════════════════════════════════════════════════════════
    uint32_t edge_creator = node_create(&g_graph);
    g_graph.nodes[edge_creator].data = META_WIRE_PATTERN;  // Extended meta-op!
    node_set_op_type(&g_graph.nodes[edge_creator], OP_MEMORY);  // Use memory to persist
    node_set_protected(&g_graph.nodes[edge_creator], 1);
    
    // Thinker drives edge creator every tick
    edge_create(&g_graph, thinker, edge_creator);
    
    printf("  ✓ Edge creator (node %u): META_WIRE_PATTERN wires active nodes\n", edge_creator);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 3: CORRELATION TRACKER - Counts co-activations
    // ═══════════════════════════════════════════════════════════════════
    uint32_t correlator = node_create(&g_graph);
    g_graph.nodes[correlator].data = META_CORRELATE;  // Extended meta-op!
    node_set_op_type(&g_graph.nodes[correlator], OP_MEMORY);
    node_set_protected(&g_graph.nodes[correlator], 1);
    
    // Thinker drives correlator
    edge_create(&g_graph, thinker, correlator);
    
    printf("  ✓ Correlator (node %u): META_CORRELATE tracks patterns\n", correlator);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 4: PATTERN DETECTOR - Creates modules when correlation high
    // ═══════════════════════════════════════════════════════════════════
    uint32_t pattern_detector = node_create(&g_graph);
    g_graph.nodes[pattern_detector].data = META_THRESHOLD_CREATE;  // Extended meta-op!
    node_set_op_type(&g_graph.nodes[pattern_detector], OP_MEMORY);
    node_set_protected(&g_graph.nodes[pattern_detector], 1);
    
    // Correlator feeds detector
    edge_create(&g_graph, correlator, pattern_detector);
    
    printf("  ✓ Pattern detector (node %u): META_THRESHOLD_CREATE spawns modules\n", pattern_detector);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 5: PERFORMANCE MONITOR - Measures learning effectiveness
    // ═══════════════════════════════════════════════════════════════════
    uint32_t monitor = node_create(&g_graph);
    g_graph.nodes[monitor].data = META_MEASURE_PERFORMANCE;
    node_set_op_type(&g_graph.nodes[monitor], OP_MEMORY);
    node_set_protected(&g_graph.nodes[monitor], 1);
    
    // Thinker drives monitor
    edge_create(&g_graph, thinker, monitor);
    
    printf("  ✓ Performance monitor (node %u): META_MEASURE_PERFORMANCE tracks metrics\n", monitor);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 6: THRESHOLD ADJUSTER - Self-correcting optimization
    // ═══════════════════════════════════════════════════════════════════
    uint32_t adjuster = node_create(&g_graph);
    g_graph.nodes[adjuster].data = META_ADJUST_THRESHOLD;
    node_set_op_type(&g_graph.nodes[adjuster], OP_MEMORY);
    node_set_protected(&g_graph.nodes[adjuster], 1);
    
    // Monitor → Adjuster (feedback loop!)
    edge_create(&g_graph, monitor, adjuster);
    
    printf("  ✓ Threshold adjuster (node %u): META_ADJUST_THRESHOLD self-optimizes\n", adjuster);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 7: LEARNING TUNER - Optimizes learning rate
    // ═══════════════════════════════════════════════════════════════════
    uint32_t tuner = node_create(&g_graph);
    g_graph.nodes[tuner].data = META_TUNE_LEARNING;
    node_set_op_type(&g_graph.nodes[tuner], OP_MEMORY);
    node_set_protected(&g_graph.nodes[tuner], 1);
    
    // Thinker drives tuner
    edge_create(&g_graph, thinker, tuner);
    
    printf("  ✓ Learning tuner (node %u): META_TUNE_LEARNING adjusts eta\n", tuner);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 8: REWARD COMPUTER - Measures prediction accuracy
    // ═══════════════════════════════════════════════════════════════════
    uint32_t reward_circuit = node_create(&g_graph);
    g_graph.nodes[reward_circuit].data = META_COMPUTE_REWARD;
    node_set_op_type(&g_graph.nodes[reward_circuit], OP_MEMORY);
    node_set_protected(&g_graph.nodes[reward_circuit], 1);
    
    // Thinker drives reward computer
    edge_create(&g_graph, thinker, reward_circuit);
    
    // Reward → Adjuster (close the loop!)
    edge_create(&g_graph, reward_circuit, adjuster);
    
    printf("  ✓ Reward computer (node %u): META_COMPUTE_REWARD measures accuracy\n", reward_circuit);
    
    // ═══════════════════════════════════════════════════════════════════
    // CIRCUIT 9: OBJECTIVE DISCOVERER - Figures out what it should be doing!
    // ═══════════════════════════════════════════════════════════════════
    uint32_t discoverer = node_create(&g_graph);
    g_graph.nodes[discoverer].data = META_DISCOVER_OBJECTIVE;
    node_set_op_type(&g_graph.nodes[discoverer], OP_MEMORY);
    node_set_protected(&g_graph.nodes[discoverer], 1);
    
    // Thinker drives discoverer
    edge_create(&g_graph, thinker, discoverer);
    
    // Discoverer → Adjuster (tells it WHAT to optimize for!)
    edge_create(&g_graph, discoverer, adjuster);
    
    printf("  ✓ Objective discoverer (node %u): META_DISCOVER_OBJECTIVE infers goals\n", discoverer);
    
    printf("\n[BOOTSTRAP] Seed complete: %u nodes, %u edges\n", 
           g_graph.node_count, g_graph.edge_count);
    printf("[BOOTSTRAP] System is now SELF-DISCOVERING!\n");
    printf("  Layer 1: Basic operations\n");
    printf("    - Thinker, Correlator, Detector, Wiring\n");
    printf("  Layer 2: Self-optimization\n");
    printf("    - Monitor, Reward, Adjuster, Tuner\n");
    printf("  Layer 3: META-META (Self-awareness!)\n");
    printf("    - Objective Discoverer: Infers goals from data\n");
    printf("    - Adjusts behavior based on inferred objective\n");
    printf("  → Graph discovers what it SHOULD do, then does it!\n\n");
    
    // Save seed to mmap
    graph_mmap_sync();
}

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

void init(uint32_t node_cap, uint32_t edge_cap) {
    memset(&g_sys, 0, sizeof(System));
    
    // Initialize byte mappings
    memset(byte_to_node, 0, sizeof(byte_to_node));
    memset(byte_node_exists, 0, sizeof(byte_node_exists));
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("  MELVIN - Universal Neuron System (Condensed)\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  Node: %zu bytes | Edge: %zu bytes | Module: %zu bytes\n", 
           sizeof(Node), sizeof(Edge), sizeof(Module));
    printf("  ✅ Mmap persistence\n");
    printf("  ✅ Edge hash table (O(1) lookups)\n");
    printf("  ✅ Module auto-growth\n");
    printf("  ✅ Meta-operations (self-modification)\n");
    printf("  ✅ Hebbian learning\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    graph_mmap_init("graph.mmap", node_cap, edge_cap);
    
    // Bootstrap minimal seed circuits (only if graph is empty)
    bootstrap_minimal_seed();
    
    ring_init(&g_sys.rx_ring, RX_RING_SIZE);
    
    g_sys.current_frame = malloc(MAX_FRAME_SIZE);
    g_sys.last_output_frame = malloc(MAX_FRAME_SIZE);
    if (!g_sys.current_frame || !g_sys.last_output_frame) {
        fprintf(stderr, "[FATAL] Out of memory allocating I/O buffers\n");
        cleanup();
        exit(1);
    }
    g_sys.current_frame_cap = MAX_FRAME_SIZE;
    g_sys.last_output_frame_cap = MAX_FRAME_SIZE;
    
    printf("[INIT] Ready: %u nodes, %u edges, %u modules\n", 
           g_graph.node_count, g_graph.edge_count, g_graph.module_count);
}

void cleanup() {
    // Free all module memory before exit
    for (uint32_t i = 0; i < g_graph.module_count; i++) {
        module_free(&g_graph.modules[i]);
    }
    
    graph_mmap_close();
    ring_free(&g_sys.rx_ring);
    free(g_sys.current_frame);
    free(g_sys.last_output_frame);
    free(g_sys.node_soma);
    free(g_sys.node_a_prev);
    free(g_graph.edge_hash);
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char **argv) {
    (void)argc;  // Mark as unused
    (void)argv;  // Mark as unused
    
    srand(time(NULL));
    
    // Ignore SIGPIPE (broken pipe)
    signal(SIGPIPE, SIG_IGN);
    
    // Check if stdin is a terminal (interactive mode)
    int is_interactive = isatty(STDIN_FILENO);
    
    if (!is_interactive) {
        // Pipe mode: non-blocking stdin
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
    
    init(256, 1024);
    
    if (is_interactive) {
        printf("\n╔══════════════════════════════════════════════════════╗\n");
        printf("║          MELVIN - Interactive Mode                   ║\n");
        printf("║          Graph computes, learns, generalizes         ║\n");
        printf("╚══════════════════════════════════════════════════════╝\n\n");
        printf("Commands:\n");
        printf("  Type text and press Enter to feed the graph\n");
        printf("  'quit' or 'exit' to stop\n");
        printf("  'stats' to show graph statistics\n");
        printf("  Graph will learn patterns and create modules!\n\n");
        
        validate_graph_integrity();
        
        // Interactive loop - feeds into ring buffer like pipe mode
        char line[4096];
        while (1) {
            printf("→ ");
            fflush(stdout);
            
            if (fgets(line, sizeof(line), stdin) == NULL) break;
            
            // Remove newline
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') {
                line[len-1] = '\0';
                len--;
            }
            
            // Check commands
            if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
                printf("\nShutting down...\n");
                break;
            } else if (strcmp(line, "stats") == 0) {
                printf("\n╔═══ GRAPH STATS ═══╗\n");
                printf("  Nodes: %u/%u\n", g_graph.node_count, g_graph.node_cap);
                printf("  Edges: %u/%u\n", g_graph.edge_count, g_graph.edge_cap);
                printf("  Modules: %u/%u\n", g_graph.module_count, g_graph.module_cap);
                printf("  Tick: %llu\n", (unsigned long long)g_sys.tick);
                printf("  Active: %u nodes\n", g_sys.active_node_count);
                printf("╚═══════════════════╝\n\n");
                continue;
            } else if (strcmp(line, "help") == 0) {
                printf("\nCommands:\n");
                printf("  stats         - Show graph statistics\n");
                printf("  quit          - Exit\n\n");
                printf("Just type text - the graph will:\n");
                printf("  - Learn patterns\n");
                printf("  - Discover its own objective\n");
                printf("  - Self-optimize toward that goal\n\n");
                continue;
            }
            
            if (len == 0) continue;
            
            // Write to ring buffer (same as pipe mode)
            ring_write(&g_sys.rx_ring, (uint8_t*)line, len);
            ring_write(&g_sys.rx_ring, (uint8_t*)"\n", 1);  // Add newline
            
            // Process through full graph computation (like pipe mode)
            slice_frame();
            
            if (g_sys.current_frame_len > 0) {
                activate_input_bytes(g_sys.current_frame, g_sys.current_frame_len);
                
                // Full propagation with ALL meta-circuits
                for (int hop = 0; hop < 5; hop++) {
                    propagate();  // This executes meta-ops!
                }
                
                // Emit PREDICTIONS (same logic as emit_output)
                uint8_t output_buffer[256];
                uint32_t output_len = 0;
                
                for (uint32_t i = 0; i < g_graph.node_count && output_len < 256; i++) {
                    Node *n = &g_graph.nodes[i];
                    if (!node_is_output(n)) continue;
                    
                    uint8_t byte = (uint8_t)node_memory_value(n);
                    
                    // Skip if in current input
                    int in_input = 0;
                    for (uint32_t j = 0; j < g_sys.current_frame_len; j++) {
                        if (g_sys.current_frame[j] == byte) {
                            in_input = 1;
                            break;
                        }
                    }
                    if (in_input) continue;
                    
                    // Check for strong predictive edges
                    for (uint32_t e_idx = 0; e_idx < g_graph.edge_count; e_idx++) {
                        Edge *e = &g_graph.edges[e_idx];
                        if (e->dst == i && e->w_slow > 220) {
                            if (e->src < g_graph.node_count && g_graph.nodes[e->src].a > 0.5f) {
                                output_buffer[output_len++] = byte;
                                printf("[PREDICT] '%c' ", (byte >= 32 && byte < 127) ? byte : '?');
                                break;
                            }
                        }
                    }
                }
                
                if (output_len > 0) {
                    printf("\n← ");
                    write(STDOUT_FILENO, output_buffer, output_len);
                    printf("\n");
                } else {
                    printf("← (learning pattern...)\n");
                }
                
                learn();  // Hebbian learning!
            }
            
            g_sys.tick++;
            printf("\n");
        }
        
        cleanup();
    } else {
        // Pipe mode: original behavior
        printf("melvin ready\n");
        main_loop();
        cleanup();
    }
    
    return 0;
}

