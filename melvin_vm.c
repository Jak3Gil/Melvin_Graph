/*
 * ═══════════════════════════════════════════════════════════════════════════
 * MELVIN VM - Ultra-Minimal Graph Execution Engine
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * PHILOSOPHY: C is JUST the virtual machine. ALL logic lives in graph.mmap!
 * 
 * This file is < 200 lines:
 *   • Load graph from mmap
 *   • Spread activation through edges  
 *   • Execute node operations (simple lookup)
 *   • Read/write bytes
 *   • That's it!
 * 
 * Everything else (learning, pruning, meta-ops, patterns) = GRAPH NODES
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>

// ═══════════════════════════════════════════════════════════════
// MINIMAL DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════

typedef struct __attribute__((packed)) {
    uint64_t id;
    float a;
    float data;
    uint16_t in_deg;
    uint16_t out_deg;
    uint32_t last_tick_seen;
} Node;

typedef struct __attribute__((packed)) {
    uint32_t src;
    uint32_t dst;
    uint8_t w_fast;
    uint8_t w_slow;
} Edge;

typedef struct {
    uint32_t node_count, node_cap, next_node_id;
    uint32_t edge_count, edge_cap;
    uint32_t module_count, module_cap;
    uint64_t tick;
    uint32_t magic;
    uint32_t hot_node_cap, cold_enabled;
    uint64_t total_cold_hits, total_hot_hits;
} Header;

// Globals
Node *nodes = NULL;
Edge *edges = NULL;
float *theta = NULL;
float *memory_value = NULL;
uint32_t *flags = NULL;
float *soma = NULL;  // Accumulator
uint32_t node_count = 0;
uint32_t edge_count = 0;

// ═══════════════════════════════════════════════════════════════
// LOAD GRAPH FROM MMAP
// ═══════════════════════════════════════════════════════════════

void load_graph(const char *filename) {
    int fd = open(filename, O_RDWR);
    if (fd < 0) {
        write(STDERR_FILENO, "ERROR: graph.mmap not found\n", 28);
        exit(1);
    }
    
    struct stat st;
    fstat(fd, &st);
    
    void *base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        write(STDERR_FILENO, "ERROR: mmap failed\n", 19);
        exit(1);
    }
    
    Header *h = (Header*)base;
    if (h->magic != 0xBEEF2024) {
        write(STDERR_FILENO, "ERROR: invalid graph file\n", 26);
        exit(1);
    }
    
    // Map structures
    nodes = (Node*)((char*)base + sizeof(Header));
    edges = (Edge*)((char*)nodes + h->node_cap * sizeof(Node));
    theta = (float*)((char*)edges + h->edge_cap * sizeof(Edge) + 64*512);
    memory_value = theta + h->node_cap;
    flags = (uint32_t*)(memory_value + h->node_cap + h->node_cap);
    
    node_count = h->node_count;
    edge_count = h->edge_count;
    
    // Allocate temp soma array
    soma = calloc(h->node_cap, sizeof(float));
}

// ═══════════════════════════════════════════════════════════════
// PRIMITIVE OPERATIONS (Graph nodes call these)
// ═══════════════════════════════════════════════════════════════

static inline float op_sum(Node *n) { return soma[(n - nodes)] / 128.0f; }
static inline float op_product(Node *n) { return tanhf(soma[(n - nodes)] / 64.0f) * 0.5f + 0.5f; }
static inline float op_max(Node *n) { return (soma[(n - nodes)] > theta[(n - nodes)]) ? 1.0f : 0.0f; }
static inline float op_min(Node *n) { return (soma[(n - nodes)] < theta[(n - nodes)]) ? 1.0f : 0.0f; }
static inline float op_threshold(Node *n) { return (soma[(n - nodes)] > theta[(n - nodes)]) ? 1.0f : 0.0f; }
static inline float op_compare(Node *n) { return 1.0f / (1.0f + expf(-(soma[(n - nodes)] - theta[(n - nodes)]))); }
static inline float op_memory(Node *n) { 
    uint32_t idx = n - nodes;
    if (soma[idx] > theta[idx]) {
        memory_value[idx] = soma[idx]; // Write
        return memory_value[idx] / 64.0f;
    }
    return memory_value[idx] / 64.0f; // Read
}
static inline float op_gate(Node *n) { return n->a * (soma[(n - nodes)] > theta[(n - nodes)] ? 1.0f : 0.0f); }

// OP_SPLICE and OP_FORK are GRAPH-CODED (see OP_SPLICE logic in melvin_core.c)
// They reference other graph structures to determine WHAT to create
// C just provides the mechanism (node_create, edge_create)
static inline float op_splice(Node *n) { return n->a; }  // Actual logic in propagate
static inline float op_fork(Node *n) { return n->a; }    // Actual logic in propagate

typedef float (*OpFunc)(Node*);
static OpFunc ops[16] = {
    op_compare, op_sum, op_product, op_max, op_min, op_threshold,
    op_memory, op_sum, op_sum, op_sum, op_gate, op_sum,
    op_splice, op_fork, op_sum, op_sum
};

// ═══════════════════════════════════════════════════════════════
// PROPAGATE - Pure activation spreading (NO logic!)
// ═══════════════════════════════════════════════════════════════

void propagate() {
    // Clear soma
    memset(soma, 0, node_count * sizeof(float));
    
    // Accumulate inputs (pure linear algebra!)
    for (uint32_t i = 0; i < edge_count; i++) {
        Edge *e = &edges[i];
        soma[e->dst] += nodes[e->src].a * e->w_fast / 255.0f;
    }
    
    // Execute operations (pure function calls!)
    for (uint32_t i = 0; i < node_count; i++) {
        uint8_t op = flags[i] & 0xFF;
        nodes[i].a = ops[op](&nodes[i]);
    }
}

// ═══════════════════════════════════════════════════════════════
// I/O - Read bytes, activate nodes
// ═══════════════════════════════════════════════════════════════

static uint32_t byte_to_node[256] = {0};

void process_input() {
    uint8_t buf[1024];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    
    if (n > 0) {
        // Activate byte nodes
        for (ssize_t i = 0; i < n; i++) {
            uint8_t byte = buf[i];
            if (byte_to_node[byte] != 0) {
                nodes[byte_to_node[byte]].a = 1.0f;
            }
        }
    }
}

void process_output() {
    uint8_t buf[256];
    uint32_t len = 0;
    
    // Collect active output nodes
    for (uint32_t i = 0; i < node_count && len < 256; i++) {
        if ((flags[i] & (1<<8)) && nodes[i].a > 0.5f) {  // is_output flag
            buf[len++] = (uint8_t)memory_value[i];
        }
    }
    
    if (len > 0) {
        write(STDOUT_FILENO, buf, len);
        write(STDOUT_FILENO, "\n", 1);
    }
}

// ═══════════════════════════════════════════════════════════════
// MAIN - Pure execution loop
// ═══════════════════════════════════════════════════════════════

int main() {
    // Load pre-compiled intelligence
    load_graph("graph.mmap");
    
    // Ready
    write(STDERR_FILENO, "melvin ready for input\n", 23);
    
    // Map byte values to nodes (first 256 nodes after circuits)
    for (uint32_t i = 0; i < 256 && i + 100 < node_count; i++) {
        byte_to_node[i] = i + 100;  // Offset past circuit nodes
    }
    
    // Set stdin non-blocking
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
    
    // Execute forever
    while (1) {
        process_input();   // Read bytes → activate nodes
        propagate();       // Spread activation
        process_output();  // Write active outputs
        usleep(50000);     // 50ms tick
    }
    
    return 0;
}

