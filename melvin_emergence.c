/*
 * ═══════════════════════════════════════════════════════════════════════════
 * MELVIN EMERGENCE - Pure Brain-like Learning System
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Philosophy: Information lives in CONNECTIONS, not in nodes
 * 
 * Core Principles:
 *   • Nodes store NO data - only state/energy/threshold
 *   • Memory IS the connection weights
 *   • Nodes learn what to output through reinforcement
 *   • Hebbian learning: "Fire together, wire together"
 *   • Energy economy: Input grants energy, output costs energy
 *   • Survival: Nodes die if energy depleted
 * 
 * How it works:
 *   1. Input byte arrives → activate or create node
 *   2. Active nodes propagate through connections
 *   3. Hebbian learning strengthens co-active connections
 *   4. Nodes output bytes based on learned correlations
 *   5. Energy flows: input grants, transmission/output costs
 * 
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* ========================================================================
 * CORE STRUCTURES
 * ======================================================================== */

typedef struct {
    float state;              // Current activation (0.0-1.0)
    float energy;             // Survival currency
    float threshold;          // Firing threshold
    uint32_t last_active_tick;
    
    // Output learning: tracks correlation with each byte
    float byte_correlation[256];
    uint8_t learned_output_byte;  // Byte with highest correlation
} Node;

typedef struct {
    uint32_t src;
    uint32_t dst;
    float weight;  // THE MEMORY! (0.0-10.0)
} Connection;

typedef struct {
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t connection_count;
    uint32_t connection_cap;
    uint64_t tick;
    uint32_t magic;  // 0xEMERGE1
} GraphHeader;

/* ========================================================================
 * GLOBAL STATE
 * ======================================================================== */

typedef struct {
    Node *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    
    Connection *connections;
    uint32_t connection_count;
    uint32_t connection_cap;
    
    uint64_t tick;
    
    // Temporal tracking for wiring
    uint32_t recent_active[10];
    uint32_t recent_active_count;
    
    // Input tracking for output learning
    uint8_t current_input[256];
    uint32_t current_input_len;
    uint8_t prev_input[256];
    uint32_t prev_input_len;
} Graph;

Graph g_graph;

// Mmap globals
static void *g_mmap_base = NULL;
static int g_mmap_fd = -1;
static size_t g_mmap_size = 0;

// Debug flag
static int g_debug = 0;

/* ========================================================================
 * MMAP PERSISTENCE
 * ======================================================================== */

void mmap_init(const char *filename) {
    uint32_t initial_node_cap = 10000;
    uint32_t initial_conn_cap = 100000;
    
    size_t header_size = sizeof(GraphHeader);
    size_t nodes_size = initial_node_cap * sizeof(Node);
    size_t connections_size = initial_conn_cap * sizeof(Connection);
    
    g_mmap_size = header_size + nodes_size + connections_size;
    
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
        if (g_debug) {
            fprintf(stderr, "[RESTORE] Loading %.2f MB from disk\n", 
                    (double)g_mmap_size / (1024.0*1024.0));
        }
    }
    
    g_mmap_base = mmap(NULL, g_mmap_size, PROT_READ | PROT_WRITE, 
                       MAP_SHARED, g_mmap_fd, 0);
    if (g_mmap_base == MAP_FAILED) {
        fprintf(stderr, "ERROR: Failed to mmap file\n");
        exit(1);
    }
    
    GraphHeader *header = (GraphHeader *)g_mmap_base;
    
    if (file_exists && header->node_count > 0 && header->magic == 0xE3E6E01) {
        g_graph.node_count = header->node_count;
        g_graph.node_cap = header->node_cap;
        g_graph.connection_count = header->connection_count;
        g_graph.connection_cap = header->connection_cap;
        g_graph.tick = header->tick;
        
        if (g_debug) {
            fprintf(stderr, "[RESTORE] Loaded: %u nodes, %u connections, tick=%llu\n",
                    g_graph.node_count, g_graph.connection_count, 
                    (unsigned long long)g_graph.tick);
        }
    } else {
        header->magic = 0xE3E6E01;
        header->node_count = 0;
        header->node_cap = initial_node_cap;
        header->connection_count = 0;
        header->connection_cap = initial_conn_cap;
        header->tick = 0;
        
        g_graph.node_count = 0;
        g_graph.node_cap = initial_node_cap;
        g_graph.connection_count = 0;
        g_graph.connection_cap = initial_conn_cap;
        g_graph.tick = 0;
        
        if (g_debug) {
            fprintf(stderr, "[MMAP INIT] Created graph.mmap: %zu bytes\n", g_mmap_size);
        }
    }
    
    // Point to mapped memory
    g_graph.nodes = (Node *)((char *)g_mmap_base + header_size);
    g_graph.connections = (Connection *)((char *)g_graph.nodes + 
                                         header->node_cap * sizeof(Node));
}

void mmap_sync() {
    if (g_mmap_base == NULL) return;
    
    GraphHeader *header = (GraphHeader *)g_mmap_base;
    header->node_count = g_graph.node_count;
    header->node_cap = g_graph.node_cap;
    header->connection_count = g_graph.connection_count;
    header->connection_cap = g_graph.connection_cap;
    header->tick = g_graph.tick;
    
    msync(g_mmap_base, g_mmap_size, MS_SYNC);
}

void mmap_close() {
    if (g_mmap_base != NULL) {
        mmap_sync();
        munmap(g_mmap_base, g_mmap_size);
        g_mmap_base = NULL;
    }
    if (g_mmap_fd >= 0) {
        close(g_mmap_fd);
        g_mmap_fd = -1;
    }
}

/* ========================================================================
 * NODE OPERATIONS
 * ======================================================================== */

uint32_t node_create() {
    if (g_graph.node_count >= g_graph.node_cap) {
        fprintf(stderr, "ERROR: Node capacity exceeded\n");
        return UINT32_MAX;
    }
    
    uint32_t idx = g_graph.node_count++;
    Node *n = &g_graph.nodes[idx];
    memset(n, 0, sizeof(Node));
    
    n->state = 0.0f;
    n->energy = 100.0f;  // Initial energy
    n->threshold = 0.5f;
    n->last_active_tick = g_graph.tick;
    n->learned_output_byte = 0;
    
    return idx;
}

/* ========================================================================
 * CONNECTION OPERATIONS
 * ======================================================================== */

Connection* find_connection(uint32_t src, uint32_t dst) {
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        if (c->src == src && c->dst == dst) {
            return c;
        }
    }
    return NULL;
}

uint32_t connection_create(uint32_t src, uint32_t dst, float initial_weight) {
    if (src >= g_graph.node_count || dst >= g_graph.node_count) {
        return UINT32_MAX;
    }
    
    // Check if connection already exists
    Connection *existing = find_connection(src, dst);
    if (existing) {
        existing->weight += initial_weight * 0.1f;  // Strengthen existing
        if (existing->weight > 10.0f) existing->weight = 10.0f;
        return UINT32_MAX;  // Return invalid to indicate already exists
    }
    
    if (g_graph.connection_count >= g_graph.connection_cap) {
        fprintf(stderr, "ERROR: Connection capacity exceeded\n");
        return UINT32_MAX;
    }
    
    uint32_t idx = g_graph.connection_count++;
    Connection *c = &g_graph.connections[idx];
    c->src = src;
    c->dst = dst;
    c->weight = initial_weight;
    
    return idx;
}

/* ========================================================================
 * INPUT SYSTEM - Dynamic node creation and temporal wiring
 * ======================================================================== */

void sense_input(const uint8_t *bytes, uint32_t len) {
    // Store current input for output learning
    g_graph.prev_input_len = g_graph.current_input_len;
    memcpy(g_graph.prev_input, g_graph.current_input, g_graph.current_input_len);
    
    g_graph.current_input_len = len > 256 ? 256 : len;
    memcpy(g_graph.current_input, bytes, g_graph.current_input_len);
    
    float input_energy = (g_graph.node_count > 3) ? 
                         g_graph.nodes[3].state : 1.0f;  // Meta-node 3
    
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = bytes[i];
        
        // Find or create node for this byte
        // Strategy: Nodes learn which bytes activate them through correlation
        uint32_t node_id = UINT32_MAX;
        float best_correlation = 0.0f;
        
        // Search for node that responds to this byte
        for (uint32_t n = 21; n < g_graph.node_count; n++) {
            Node *node = &g_graph.nodes[n];
            
            // Check if this node has learned to respond to this byte
            if (node->byte_correlation[byte] > best_correlation) {
                best_correlation = node->byte_correlation[byte];
                node_id = n;
            }
        }
        
        // If no node strongly responds, create new one
        if (best_correlation < 0.5f || node_id == UINT32_MAX) {
            node_id = node_create();
            if (node_id != UINT32_MAX) {
                // Initialize correlation for this byte
                g_graph.nodes[node_id].byte_correlation[byte] = 1.0f;
                g_graph.nodes[node_id].learned_output_byte = byte;
                
                if (g_debug && g_graph.tick % 10 == 0) {
                    fprintf(stderr, "[CREATE] Node %u for byte '%c' (%u)\n",
                            node_id, (byte >= 32 && byte < 127) ? byte : '?', byte);
                }
            }
        } else {
            // Strengthen correlation
            g_graph.nodes[node_id].byte_correlation[byte] += 0.1f;
            if (g_graph.nodes[node_id].byte_correlation[byte] > 10.0f) {
                g_graph.nodes[node_id].byte_correlation[byte] = 10.0f;
            }
        }
        
        if (node_id == UINT32_MAX) continue;
        
        Node *node = &g_graph.nodes[node_id];
        
        // Activate node
        node->state = 1.0f;
        node->energy += input_energy;
        node->last_active_tick = g_graph.tick;
        
        // TEMPORAL WIRING: Connect to recently active nodes
        for (uint32_t r = 0; r < g_graph.recent_active_count; r++) {
            uint32_t recent_id = g_graph.recent_active[r];
            if (recent_id != node_id && recent_id < g_graph.node_count) {
                connection_create(recent_id, node_id, 1.0f);
            }
        }
        
        // Add to recent active list (circular buffer)
        g_graph.recent_active[g_graph.recent_active_count % 10] = node_id;
        if (g_graph.recent_active_count < 10) {
            g_graph.recent_active_count++;
        }
    }
}

/* ========================================================================
 * PROPAGATION - Weighted signal transmission
 * ======================================================================== */

void propagate() {
    float transmission_cost = (g_graph.node_count > 4) ?
                              g_graph.nodes[4].state * 0.01f : 0.01f;  // Meta-node 4
    
    // Decay all activations first
    float decay_rate = (g_graph.node_count > 1) ? 
                       g_graph.nodes[1].state : 0.95f;  // Meta-node 1
    
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        g_graph.nodes[i].state *= decay_rate;
        if (g_graph.nodes[i].state < 0.001f) {
            g_graph.nodes[i].state = 0.0f;
        }
    }
    
    // Propagate through connections
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        
        if (c->src >= g_graph.node_count || c->dst >= g_graph.node_count) continue;
        if (c->weight <= 0.0f) continue;
        
        Node *src = &g_graph.nodes[c->src];
        Node *dst = &g_graph.nodes[c->dst];
        
        // Skip inactive nodes
        if (src->state < 0.01f) continue;
        
        // Weighted transmission
        float signal = src->state * c->weight * 0.1f;
        dst->state += signal;
        
        // Energy cost for transmission
        src->energy -= signal * transmission_cost;
        
        // Clamp destination state
        if (dst->state > 1.0f) dst->state = 1.0f;
    }
}

/* ========================================================================
 * HEBBIAN LEARNING - Fire together, wire together
 * ======================================================================== */

void learn() {
    float learning_rate = (g_graph.node_count > 0) ?
                          g_graph.nodes[0].state : 0.1f;  // Meta-node 0
    
    float activation_threshold = (g_graph.node_count > 2) ?
                                 g_graph.nodes[2].state : 0.5f;  // Meta-node 2
    
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        
        if (c->src >= g_graph.node_count || c->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[c->src];
        Node *dst = &g_graph.nodes[c->dst];
        
        int src_active = (src->state > activation_threshold);
        int dst_active = (dst->state > activation_threshold);
        
        if (src_active && dst_active) {
            // Both firing → strengthen connection
            c->weight += learning_rate;
            if (c->weight > 10.0f) c->weight = 10.0f;
        } else {
            // Not co-active → slight decay
            c->weight -= learning_rate * 0.01f;
            if (c->weight < 0.0f) c->weight = 0.0f;
        }
    }
}

/* ========================================================================
 * OUTPUT SYSTEM - Learned byte emission
 * ======================================================================== */

void emit_output() {
    float output_threshold = (g_graph.node_count > 2) ?
                             g_graph.nodes[2].state : 0.5f;  // Meta-node 2
    
    float output_cost = (g_graph.node_count > 4) ?
                        g_graph.nodes[4].state * 0.5f : 0.5f;  // Meta-node 4
    
    uint8_t output_buffer[256];
    uint32_t output_len = 0;
    uint8_t already_output[256] = {0};
    
    // Find active nodes and emit their learned bytes
    for (uint32_t i = 21; i < g_graph.node_count && output_len < 256; i++) {
        Node *node = &g_graph.nodes[i];
        
        if (node->state <= output_threshold) continue;
        if (node->energy <= output_cost) continue;  // Need energy to output
        
        uint8_t byte = node->learned_output_byte;
        
        // Deduplicate
        if (already_output[byte]) continue;
        already_output[byte] = 1;
        
        output_buffer[output_len++] = byte;
        
        // Output costs energy
        node->energy -= output_cost;
        
        // Reinforce correct outputs
        // If this byte was in the previous input, strengthen correlation
        for (uint32_t j = 0; j < g_graph.prev_input_len; j++) {
            if (g_graph.prev_input[j] == byte) {
                node->byte_correlation[byte] += 0.05f;
                if (node->byte_correlation[byte] > 10.0f) {
                    node->byte_correlation[byte] = 10.0f;
                }
                // Reward with energy
                node->energy += 1.0f;
                break;
            }
        }
    }
    
    // Write output
    if (output_len > 0) {
        write(STDOUT_FILENO, output_buffer, output_len);
        write(STDOUT_FILENO, "\n", 1);
    }
}

/* ========================================================================
 * ENERGY ECONOMY - Metabolism and death
 * ======================================================================== */

void apply_metabolism() {
    float metabolism_cost = (g_graph.node_count > 5) ?
                            g_graph.nodes[5].state * 0.1f : 0.1f;  // Meta-node 5
    
    // Mark dead nodes
    uint32_t deaths = 0;
    
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        Node *node = &g_graph.nodes[i];
        
        // Metabolism: living costs energy
        node->energy -= metabolism_cost;
        
        // Death by starvation
        if (node->energy <= 0.0f) {
            node->energy = 0.0f;
            node->state = 0.0f;
            deaths++;
            
            // Mark connections for removal (set weight to 0)
            for (uint32_t c = 0; c < g_graph.connection_count; c++) {
                if (g_graph.connections[c].src == i || 
                    g_graph.connections[c].dst == i) {
                    g_graph.connections[c].weight = 0.0f;
                }
            }
        }
    }
    
    if (g_debug && deaths > 0 && g_graph.tick % 100 == 0) {
        fprintf(stderr, "[DEATH] %u nodes starved (energy depleted)\n", deaths);
    }
    
    // Periodically compact connections (remove dead ones)
    if (g_graph.tick % 500 == 0) {
        uint32_t write_idx = 0;
        for (uint32_t read_idx = 0; read_idx < g_graph.connection_count; read_idx++) {
            if (g_graph.connections[read_idx].weight > 0.0f) {
                if (write_idx != read_idx) {
                    g_graph.connections[write_idx] = g_graph.connections[read_idx];
                }
                write_idx++;
            }
        }
        uint32_t removed = g_graph.connection_count - write_idx;
        g_graph.connection_count = write_idx;
        
        if (g_debug && removed > 0) {
            fprintf(stderr, "[COMPACT] Removed %u dead connections\n", removed);
        }
    }
}

/* ========================================================================
 * META-NODE BOOTSTRAP
 * ======================================================================== */

void bootstrap_meta_nodes() {
    // Create 21 meta-nodes (0-20) that store system parameters
    // These nodes control the behavior of the system
    
    if (g_debug) {
        fprintf(stderr, "[BOOTSTRAP] Creating 21 meta-nodes for system parameters\n");
    }
    
    // Node 0: Learning rate
    uint32_t id = node_create();
    g_graph.nodes[id].state = 0.1f;  // Store parameter in state
    g_graph.nodes[id].energy = 10000.0f;  // High energy so they never die
    g_graph.nodes[id].threshold = 999.0f;  // Never activate
    
    // Node 1: Activation decay rate
    id = node_create();
    g_graph.nodes[id].state = 0.95f;
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Node 2: Activation threshold for output/learning
    id = node_create();
    g_graph.nodes[id].state = 0.5f;
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Node 3: Input energy grant per byte
    id = node_create();
    g_graph.nodes[id].state = 1.0f;
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Node 4: Transmission/output cost multiplier
    id = node_create();
    g_graph.nodes[id].state = 1.0f;
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Node 5: Metabolism cost
    id = node_create();
    g_graph.nodes[id].state = 0.1f;
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Nodes 6-20: Reserved for future parameters
    for (int i = 6; i < 21; i++) {
        id = node_create();
        g_graph.nodes[id].state = 1.0f;  // Default value
        g_graph.nodes[id].energy = 10000.0f;
        g_graph.nodes[id].threshold = 999.0f;
    }
    
    if (g_debug) {
        fprintf(stderr, "[BOOTSTRAP] Meta-nodes 0-20 created\n");
        fprintf(stderr, "  [0] Learning rate: %.2f\n", g_graph.nodes[0].state);
        fprintf(stderr, "  [1] Decay rate: %.2f\n", g_graph.nodes[1].state);
        fprintf(stderr, "  [2] Activation threshold: %.2f\n", g_graph.nodes[2].state);
        fprintf(stderr, "  [3] Input energy: %.2f\n", g_graph.nodes[3].state);
        fprintf(stderr, "  [4] Output/transmission cost: %.2f\n", g_graph.nodes[4].state);
        fprintf(stderr, "  [5] Metabolism cost: %.2f\n", g_graph.nodes[5].state);
    }
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    // Check for debug mode
    if (getenv("MELVIN_DEBUG")) {
        g_debug = 1;
    }
    
    // Initialize
    memset(&g_graph, 0, sizeof(Graph));
    mmap_init("graph_emergence.mmap");
    
    // Bootstrap meta-nodes if starting fresh
    if (g_graph.node_count == 0) {
        bootstrap_meta_nodes();
    }
    
    if (g_debug) {
        fprintf(stderr, "[INIT] Ready: %u nodes, %u connections\n",
                g_graph.node_count, g_graph.connection_count);
    }
    
    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    // Main loop
    uint8_t input_buffer[4096];
    int idle_ticks = 0;
    
    while (idle_ticks < 10) {
        ssize_t n = read(STDIN_FILENO, input_buffer, sizeof(input_buffer));
        
        if (n > 0) {
            sense_input(input_buffer, n);
            idle_ticks = 0;
        } else {
            idle_ticks++;
            usleep(10000);  // 10ms
        }
        
        // Propagate multiple hops
        for (int hop = 0; hop < 5; hop++) {
            propagate();
        }
        
        learn();
        emit_output();
        apply_metabolism();
        
        g_graph.tick++;
        
        // Sync to disk periodically
        if (g_graph.tick % 100 == 0) {
            mmap_sync();
            
            if (g_debug) {
                fprintf(stderr, "[TICK %llu] Nodes: %u, Connections: %u, Avg energy: %.1f\n",
                        (unsigned long long)g_graph.tick, g_graph.node_count, 
                        g_graph.connection_count,
                        (g_graph.node_count > 21) ? 
                        g_graph.nodes[21].energy : 0.0f);
            }
        }
    }
    
    mmap_close();
    return 0;
}

