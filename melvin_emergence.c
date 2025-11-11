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
    uint32_t activation_sequence;  // ORDER within tick (for repeated activations!)
    
    // Hierarchical organization
    uint8_t is_hub;           // High-degree integrator node
    uint8_t hub_level;        // 0=byte, 1=word, 2=phrase, 3=concept
    uint16_t in_degree;       // Count of incoming connections
    uint16_t out_degree;      // Count of outgoing connections
    
    // Pattern node (represents group of nodes)
    uint8_t is_pattern;       // This node represents a pattern
    uint32_t pattern_members[16];  // IDs of nodes in this pattern
    uint8_t pattern_member_count;
    
    // Output learning: tracks correlation with each byte
    float byte_correlation[256];
    uint8_t learned_output_byte;  // Byte with highest correlation
} Node;

typedef struct {
    uint32_t src;
    uint32_t dst;
    float weight;  // THE MEMORY! (0.0-10.0)
    
    // EDGES ARE RULES!
    uint8_t is_rule;         // This edge is a RULE (IF src THEN dst)
    uint8_t rule_strength;   // How strictly to enforce (0-255)
    uint8_t times_satisfied; // How many times rule held true
    uint8_t times_violated;  // How many times rule was broken
    
    // Execution mode
    uint8_t is_implication;  // src → dst (IF-THEN)
    uint8_t is_inhibitory;   // src ⊣ dst (IF-THEN-NOT)
} Connection;

typedef struct {
    uint32_t node_id;
    uint32_t sequence;
    uint8_t byte;
} ActivationEvent;

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
    
    // Sequence tracking (for same node activating multiple times in one tick)
    uint32_t activation_sequence_counter;
    
    // Input tracking for output learning
    uint8_t current_input[256];
    uint32_t current_input_len;
    uint8_t prev_input[256];
    uint32_t prev_input_len;
    
    // Activation event log (for recording multiple activations of same node)
    ActivationEvent activation_log[1024];
    uint32_t activation_log_len;
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
    n->is_hub = 0;
    n->hub_level = 0;  // Start as byte-level
    n->in_degree = 0;
    n->out_degree = 0;
    n->is_pattern = 0;
    n->pattern_member_count = 0;
    
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
    c->is_rule = 0;
    c->rule_strength = 0;
    c->times_satisfied = 0;
    c->times_violated = 0;
    c->is_implication = 1;  // Default: src → dst (IF-THEN)
    c->is_inhibitory = 0;
    
    // Update node degrees
    g_graph.nodes[src].out_degree++;
    g_graph.nodes[dst].in_degree++;
    
    return idx;
}

/* ========================================================================
 * GUARD-RAILED CONNECTION CREATION
 * ======================================================================== */

uint32_t connection_create_guarded(uint32_t src, uint32_t dst, float initial_weight) {
    if (src >= g_graph.node_count || dst >= g_graph.node_count) {
        return UINT32_MAX;
    }
    
    if (src == dst) return UINT32_MAX;  // No self-loops for now
    
    Node *src_node = &g_graph.nodes[src];
    Node *dst_node = &g_graph.nodes[dst];
    
    // GUARD RAIL 1: Temporal proximity
    // Only connect nodes that activated within 10 ticks of each other
    if (src_node->last_active_tick > 10 && dst_node->last_active_tick > 10) {
        uint32_t tick_diff = (src_node->last_active_tick > dst_node->last_active_tick) ?
                             src_node->last_active_tick - dst_node->last_active_tick :
                             dst_node->last_active_tick - src_node->last_active_tick;
        
        if (tick_diff > 10 && !src_node->is_hub && !dst_node->is_hub) {
            return UINT32_MAX;  // Too far apart in time
        }
    }
    
    // GUARD RAIL 2: Spatial locality (unless hub)
    // Only connect nearby nodes (within 100 IDs) unless at least one is a hub
    if (!src_node->is_hub && !dst_node->is_hub) {
        uint32_t spatial_diff = (src > dst) ? src - dst : dst - src;
        if (spatial_diff > 100) {
            return UINT32_MAX;  // Too far apart spatially
        }
    }
    
    // GUARD RAIL 3: Energy threshold
    // Don't waste connections on dying nodes
    if (src_node->energy < 20.0f && dst_node->energy < 20.0f) {
        return UINT32_MAX;  // Both nodes struggling
    }
    
    // All guard rails passed - create connection
    return connection_create(src, dst, initial_weight);
}

/* ========================================================================
 * PATTERN CLUSTERING - Create meta-nodes for frequent patterns
 * ======================================================================== */

void detect_patterns() {
    // Find groups of nodes that frequently activate together
    // Create pattern nodes to represent them
    
    float pattern_threshold = (g_graph.node_count > 13) ?
                              g_graph.nodes[13].state : 0.7f;  // Meta-node 13
    
    uint32_t min_pattern_size = 3;
    uint32_t max_pattern_size = 10;
    
    // Simple pattern detection: Find sequences with strong connections
    for (uint32_t start = 21; start < g_graph.node_count; start++) {
        Node *start_node = &g_graph.nodes[start];
        
        // Skip if already part of a pattern or low energy
        if (start_node->is_pattern || start_node->energy < 50.0f) continue;
        
        // Try to build a chain from this node
        uint32_t chain[16];
        float chain_weights[16];
        uint32_t chain_len = 0;
        
        chain[chain_len++] = start;
        uint32_t current = start;
        
        // Follow strongest outgoing connections
        for (uint32_t step = 0; step < max_pattern_size - 1; step++) {
            float best_weight = 0.0f;
            uint32_t best_dst = UINT32_MAX;
            
            // Find strongest outgoing connection from current
            for (uint32_t c = 0; c < g_graph.connection_count; c++) {
                Connection *conn = &g_graph.connections[c];
                if (conn->src == current && conn->weight > best_weight) {
                    // Check if dst not already in chain
                    int already_in_chain = 0;
                    for (uint32_t ch = 0; ch < chain_len; ch++) {
                        if (chain[ch] == conn->dst) {
                            already_in_chain = 1;
                            break;
                        }
                    }
                    
                    if (!already_in_chain) {
                        best_weight = conn->weight;
                        best_dst = conn->dst;
                    }
                }
            }
            
            // If no strong connection, stop building chain
            if (best_weight < pattern_threshold * 10.0f || best_dst == UINT32_MAX) {
                break;
            }
            
            chain[chain_len] = best_dst;
            chain_weights[chain_len - 1] = best_weight;
            chain_len++;
            current = best_dst;
        }
        
        // If chain is long enough and strong enough, create pattern node
        if (chain_len >= min_pattern_size) {
            // Check average weight
            float avg_weight = 0.0f;
            for (uint32_t i = 0; i < chain_len - 1; i++) {
                avg_weight += chain_weights[i];
            }
            avg_weight /= (chain_len - 1);
            
            if (avg_weight >= 8.0f) {  // Strong pattern!
                // Create pattern meta-node
                uint32_t pattern_node = node_create();
                if (pattern_node != UINT32_MAX) {
                    Node *pnode = &g_graph.nodes[pattern_node];
                    pnode->is_pattern = 1;
                    pnode->hub_level = 1;  // Word-level
                    pnode->energy = 200.0f;  // High energy
                    pnode->threshold = 0.3f;  // Easy to activate
                    
                    // Store pattern members
                    pnode->pattern_member_count = chain_len > 16 ? 16 : chain_len;
                    for (uint32_t i = 0; i < pnode->pattern_member_count; i++) {
                        pnode->pattern_members[i] = chain[i];
                    }
                    
                    // Wire: pattern members → pattern node
                    for (uint32_t i = 0; i < chain_len; i++) {
                        connection_create(chain[i], pattern_node, 2.0f);
                    }
                    
                    // Pattern node can also output its sequence
                    pnode->learned_output_byte = g_graph.nodes[chain[0]].learned_output_byte;
                    
                    if (g_debug) {
                        fprintf(stderr, "[PATTERN] Created node %u for %u-node sequence (avg_weight=%.1f)\n",
                                pattern_node, chain_len, avg_weight);
                        fprintf(stderr, "  Members: ");
                        for (uint32_t i = 0; i < chain_len && i < 5; i++) {
                            fprintf(stderr, "%u('%c') ", chain[i], 
                                    g_graph.nodes[chain[i]].learned_output_byte);
                        }
                        if (chain_len > 5) fprintf(stderr, "...");
                        fprintf(stderr, "\n");
                    }
                }
            }
        }
    }
}

/* ========================================================================
 * HUB DETECTION - Identify integrator nodes
 * ======================================================================== */

void detect_hubs() {
    // Find nodes with high connectivity and promote them to hubs
    float hub_degree_threshold = (g_graph.node_count > 14) ?
                                 g_graph.nodes[14].state : 10.0f;  // Meta-node 14
    
    float hub_weight_threshold = (g_graph.node_count > 15) ?
                                 g_graph.nodes[15].state : 20.0f;  // Meta-node 15
    
    uint32_t hubs_promoted = 0;
    
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        Node *node = &g_graph.nodes[i];
        
        // Skip if already a hub
        if (node->is_hub) continue;
        
        // Calculate total incoming weight
        float total_in_weight = 0.0f;
        for (uint32_t c = 0; c < g_graph.connection_count; c++) {
            if (g_graph.connections[c].dst == i) {
                total_in_weight += g_graph.connections[c].weight;
            }
        }
        
        // Promote to hub if high degree AND high total weight
        if (node->in_degree >= (uint16_t)hub_degree_threshold && 
            total_in_weight >= hub_weight_threshold) {
            
            node->is_hub = 1;
            node->hub_level = 1;  // Word-level hub
            node->threshold *= 0.8f;  // Lower threshold (easier to activate)
            node->energy += 50.0f;  // Energy bonus for becoming hub
            hubs_promoted++;
            
            if (g_debug) {
                fprintf(stderr, "[HUB] Node %u promoted (in_deg=%u, weight=%.1f)\n",
                        i, node->in_degree, total_in_weight);
            }
        }
    }
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
    
    // Reset sequence counter and activation log for this input
    g_graph.activation_sequence_counter = 0;
    g_graph.activation_log_len = 0;
    
    for (uint32_t i = 0; i < len; i++) {
        uint8_t byte = bytes[i];
        
        // Reset sequence on newline (don't connect across lines!)
        if (byte == '\n' || byte == '\r') {
            g_graph.recent_active_count = 0;
        }
        
        // Find node that represents this byte (REUSE neurons!)
        uint32_t node_id = UINT32_MAX;
        float best_correlation = 0.0f;
        
        for (uint32_t n = 21; n < g_graph.node_count; n++) {
            Node *node = &g_graph.nodes[n];
            
            // Find node with strongest correlation to this byte
            if (node->byte_correlation[byte] > best_correlation) {
                best_correlation = node->byte_correlation[byte];
                node_id = n;
            }
        }
        
        // Create new node only if no existing node responds
        if (best_correlation < 0.5f || node_id == UINT32_MAX) {
            node_id = node_create();
            
            if (node_id != UINT32_MAX) {
                g_graph.nodes[node_id].byte_correlation[byte] = 1.0f;
                g_graph.nodes[node_id].learned_output_byte = byte;
                
                if (g_debug && node_id < 40) {  // Only show first few
                    fprintf(stderr, "[CREATE] Node %u for byte '%c' (%u)\n",
                            node_id, (byte >= 32 && byte < 127) ? byte : '?', byte);
                }
            }
        } else {
            // Strengthen existing node's correlation
            g_graph.nodes[node_id].byte_correlation[byte] += 0.1f;
            if (g_graph.nodes[node_id].byte_correlation[byte] > 10.0f) {
                g_graph.nodes[node_id].byte_correlation[byte] = 10.0f;
            }
        }
        
        if (node_id == UINT32_MAX) continue;
        
        Node *node = &g_graph.nodes[node_id];
        
        // Activate node with sequence tracking
        node->state = 1.0f;
        node->energy += input_energy;
        node->last_active_tick = g_graph.tick;
        node->activation_sequence = g_graph.activation_sequence_counter;
        
        // LOG THIS ACTIVATION EVENT (allows same node to fire multiple times!)
        if (g_graph.activation_log_len < 1024) {
            g_graph.activation_log[g_graph.activation_log_len].node_id = node_id;
            g_graph.activation_log[g_graph.activation_log_len].sequence = g_graph.activation_sequence_counter;
            g_graph.activation_log[g_graph.activation_log_len].byte = byte;
            g_graph.activation_log_len++;
        }
        
        g_graph.activation_sequence_counter++;  // Increment for next byte
        
        // SEQUENTIAL WIRING: Only connect to IMMEDIATE previous node (learn true rules!)
        // This learns "H→e" not "H→everything"
        if (g_graph.recent_active_count > 0) {
            uint32_t prev_id = g_graph.recent_active[(g_graph.recent_active_count - 1) % 10];
            if (prev_id != node_id && prev_id < g_graph.node_count) {
                connection_create_guarded(prev_id, node_id, 1.0f);
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
    
    // Decay all activations first (SKIP META-NODES 0-20!)
    float decay_rate = (g_graph.node_count > 1) ? 
                       g_graph.nodes[1].state : 0.95f;  // Meta-node 1
    
    // Only decay regular nodes, not meta-nodes!
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        g_graph.nodes[i].state *= decay_rate;
        if (g_graph.nodes[i].state < 0.001f) {
            g_graph.nodes[i].state = 0.0f;
        }
    }
    
    // HIERARCHICAL: Pattern nodes activate their members
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        Node *node = &g_graph.nodes[i];
        
        if (!node->is_pattern) continue;
        if (node->state < node->threshold) continue;
        
        // Pattern activated! Trigger all member nodes
        for (uint32_t m = 0; m < node->pattern_member_count; m++) {
            uint32_t member_id = node->pattern_members[m];
            if (member_id < g_graph.node_count) {
                Node *member = &g_graph.nodes[member_id];
                member->state += node->state * 0.5f;  // Pattern activates members
                
                // Log member activation
                if (g_graph.activation_log_len < 1024) {
                    g_graph.activation_log[g_graph.activation_log_len].node_id = member_id;
                    g_graph.activation_log[g_graph.activation_log_len].sequence = g_graph.activation_sequence_counter++;
                    g_graph.activation_log[g_graph.activation_log_len].byte = member->learned_output_byte;
                    g_graph.activation_log_len++;
                    
                    member->last_active_tick = g_graph.tick;
                }
            }
        }
    }
    
    // EXECUTE CONNECTIONS AS RULES!
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        
        if (c->src >= g_graph.node_count || c->dst >= g_graph.node_count) continue;
        if (c->weight <= 0.0f) continue;
        
        Node *src = &g_graph.nodes[c->src];
        Node *dst = &g_graph.nodes[c->dst];
        
        // Skip inactive nodes
        if (src->state < 0.01f) continue;
        
        float signal = 0.0f;
        
        if (c->is_rule) {
            // RULE MODE: IF src fires, dst MUST fire!
            if (src->state > src->threshold) {
                // Execute the rule: FORCE dst to activate
                float rule_force = (float)c->rule_strength / 255.0f;
                dst->state = rule_force;  // Direct assignment (enforcement!)
                signal = rule_force;
                
                if (g_debug && c->rule_strength > 200 && g_graph.tick % 100 == 0) {
                    fprintf(stderr, "[RULE EXEC] '%c' → '%c' (forced activation %.2f)\n",
                            src->learned_output_byte, dst->learned_output_byte, rule_force);
                }
            }
        } else {
            // STATISTICAL MODE: Weighted transmission (old behavior)
            signal = src->state * c->weight * 0.1f;
            if (src->is_hub) signal *= 1.5f;  // Hub boost!
            dst->state += signal;
        }
        
        // If dst crosses threshold, LOG IT (for output!)
        if (dst->state > dst->threshold && g_graph.activation_log_len < 1024) {
            // Check if not already in log this tick
            int already_logged = 0;
            for (uint32_t l = 0; l < g_graph.activation_log_len; l++) {
                if (g_graph.activation_log[l].node_id == c->dst) {
                    already_logged = 1;
                    break;
                }
            }
            
            if (!already_logged) {
                g_graph.activation_log[g_graph.activation_log_len].node_id = c->dst;
                g_graph.activation_log[g_graph.activation_log_len].sequence = g_graph.activation_sequence_counter++;
                g_graph.activation_log[g_graph.activation_log_len].byte = dst->learned_output_byte;
                g_graph.activation_log_len++;
                
                dst->last_active_tick = g_graph.tick;
            }
        }
        
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
            
            // Track rule discovery
            if (c->times_satisfied < 255) c->times_satisfied++;
            
            // PROMOTE TO RULE: If consistently satisfied, this becomes LAW!
            if (c->weight >= 9.5f && c->times_satisfied > 20 && c->times_violated == 0) {
                c->is_rule = 1;
                c->rule_strength = (uint8_t)(c->weight * 25.5f);  // Scale to 0-255
                
                if (g_debug && !c->is_rule) {
                    fprintf(stderr, "[RULE DISCOVERY] '%c' → '%c' promoted to RULE (satisfied %u times, never violated)\n",
                            src->learned_output_byte, dst->learned_output_byte, c->times_satisfied);
                }
            }
        } else if (src_active && !dst_active) {
            // Source fired but destination didn't → RULE VIOLATION!
            if (c->times_violated < 255) c->times_violated++;
            
            // If this is a rule, this is SERIOUS
            if (c->is_rule) {
                // Penalty for breaking a rule!
                src->energy -= 10.0f;  // Heavy penalty
                
                if (g_debug && g_graph.tick % 50 == 0) {
                    fprintf(stderr, "[RULE VIOLATION] '%c' fired but '%c' didn't! (violations=%u)\n",
                            src->learned_output_byte, dst->learned_output_byte, c->times_violated);
                }
                
                // Demote from rule if violated too many times
                if (c->times_violated > 10) {
                    c->is_rule = 0;
                    c->rule_strength = 0;
                }
            }
            
            // Slight decay
            c->weight -= learning_rate * 0.01f;
            if (c->weight < 0.0f) c->weight = 0.0f;
        } else {
            // Not co-active → slight decay
            c->weight -= learning_rate * 0.01f;
            if (c->weight < 0.0f) c->weight = 0.0f;
        }
    }
}

/* ========================================================================
 * OUTPUT VALIDATION - Check if pattern is complete
 * ======================================================================== */

int validate_output_completeness() {
    // Check if all RULES were satisfied
    uint32_t rule_count = 0;
    uint32_t satisfied_count = 0;
    uint32_t violated_count = 0;
    
    for (uint32_t i = 0; i < g_graph.connection_count; i++) {
        Connection *c = &g_graph.connections[i];
        
        if (!c->is_rule) continue;
        
        rule_count++;
        
        if (c->src >= g_graph.node_count || c->dst >= g_graph.node_count) continue;
        
        Node *src = &g_graph.nodes[c->src];
        Node *dst = &g_graph.nodes[c->dst];
        
        // RULE CHECK: IF src fired, did dst fire?
        if (src->state > 0.5f) {
            if (dst->state > 0.5f) {
                satisfied_count++;  // Rule held!
            } else {
                violated_count++;   // Rule broken!
            }
        }
    }
    
    // Pattern is complete if NO rules violated
    if (violated_count > 0) {
        return 0;  // FAIL - rules broken
    }
    
    // And most rules satisfied
    float completeness = (rule_count > 0) ? 
                         (float)satisfied_count / (float)rule_count : 1.0f;
    
    return (completeness >= 0.8f);
}

/* ========================================================================
 * OUTPUT SYSTEM - Learned byte emission with completeness checking
 * ======================================================================== */

void emit_output() {
    float output_threshold = (g_graph.node_count > 2) ?
                             g_graph.nodes[2].state : 0.5f;  // Meta-node 2
    
    float output_cost = (g_graph.node_count > 4) ?
                        g_graph.nodes[4].state * 0.5f : 0.5f;  // Meta-node 4
    
    // Use activation log (allows same node to output multiple times!)
    uint8_t output_buffer[1024];
    uint32_t output_len = 0;
    
    // Output each logged activation event in sequence order
    for (uint32_t i = 0; i < g_graph.activation_log_len && output_len < 1024; i++) {
        uint32_t node_id = g_graph.activation_log[i].node_id;
        
        if (node_id >= g_graph.node_count) continue;
        
        Node *node = &g_graph.nodes[node_id];
        
        // Don't check state - use the logged event directly!
        // This allows same node to output multiple times
        if (node->energy <= output_cost) continue;
        
        // Output the byte from the activation event
        output_buffer[output_len++] = g_graph.activation_log[i].byte;
        
        // Charge energy cost
        node->energy -= output_cost * 0.1f;  // Small cost per output
        
        // Reinforce correct outputs
        for (uint32_t j = 0; j < g_graph.prev_input_len; j++) {
            if (g_graph.prev_input[j] == g_graph.activation_log[i].byte) {
                node->byte_correlation[g_graph.activation_log[i].byte] += 0.05f;
                if (node->byte_correlation[g_graph.activation_log[i].byte] > 10.0f) {
                    node->byte_correlation[g_graph.activation_log[i].byte] = 10.0f;
                }
                node->energy += 1.0f;  // Reward
                break;
            }
        }
    }
    
    // VALIDATE OUTPUT BEFORE EMITTING
    int is_complete = validate_output_completeness();
    
    // Write output
    if (output_len > 0) {
        write(STDOUT_FILENO, output_buffer, output_len);
        write(STDOUT_FILENO, "\n", 1);
        
        // REINFORCEMENT: Only reward complete outputs!
        if (is_complete) {
            // Reward all active nodes for producing complete pattern
            for (uint32_t i = 21; i < g_graph.node_count; i++) {
                if (g_graph.nodes[i].state > 0.5f) {
                    g_graph.nodes[i].energy += 2.0f;  // Bonus for completeness!
                }
            }
            
            if (g_debug && g_graph.tick % 50 == 0) {
                fprintf(stderr, "[COMPLETE] Output passed validation, nodes rewarded\n");
            }
        } else {
            // Incomplete output - penalize
            for (uint32_t i = 21; i < g_graph.node_count; i++) {
                if (g_graph.nodes[i].state > 0.5f) {
                    g_graph.nodes[i].energy -= 1.0f;  // Penalty for incompleteness
                }
            }
            
            if (g_debug && g_graph.tick % 50 == 0) {
                fprintf(stderr, "[INCOMPLETE] Output missing required nodes, penalized\n");
            }
        }
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
    
    // Node 6-12: Reserved
    for (int i = 6; i < 13; i++) {
        id = node_create();
        g_graph.nodes[id].state = 1.0f;
        g_graph.nodes[id].energy = 10000.0f;
        g_graph.nodes[id].threshold = 999.0f;
    }
    
    // Node 13: Pattern detection threshold
    id = node_create();
    g_graph.nodes[id].state = 0.7f;  // 70% connection strength
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Node 14: Hub degree threshold
    id = node_create();
    g_graph.nodes[id].state = 5.0f;  // Need 5+ connections to become hub
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Node 15: Hub weight threshold
    id = node_create();
    g_graph.nodes[id].state = 15.0f;  // Need 15+ total weight
    g_graph.nodes[id].energy = 10000.0f;
    g_graph.nodes[id].threshold = 999.0f;
    
    // Nodes 16-20: Reserved
    for (int i = 16; i < 21; i++) {
        id = node_create();
        g_graph.nodes[id].state = 1.0f;
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
        fprintf(stderr, "  [13] Pattern threshold: %.2f\n", g_graph.nodes[13].state);
        fprintf(stderr, "  [14] Hub degree threshold: %.1f\n", g_graph.nodes[14].state);
        fprintf(stderr, "  [15] Hub weight threshold: %.1f\n", g_graph.nodes[15].state);
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
        
        // NETWORK OF NETWORKS: Detect hubs and patterns periodically
        if (g_graph.tick % 50 == 0 && g_graph.tick > 0) {
            detect_hubs();
        }
        
        if (g_graph.tick % 100 == 0 && g_graph.tick > 0) {
            detect_patterns();
        }
        
        g_graph.tick++;
        
        // Sync to disk periodically
        if (g_graph.tick % 100 == 0) {
            mmap_sync();
            
            if (g_debug) {
                uint32_t hub_count = 0;
                uint32_t pattern_count = 0;
                for (uint32_t i = 21; i < g_graph.node_count; i++) {
                    if (g_graph.nodes[i].is_hub) hub_count++;
                    if (g_graph.nodes[i].is_pattern) pattern_count++;
                }
                
                fprintf(stderr, "[TICK %llu] Nodes: %u (%u hubs, %u patterns), Connections: %u\n",
                        (unsigned long long)g_graph.tick, g_graph.node_count,
                        hub_count, pattern_count, g_graph.connection_count);
            }
        }
    }
    
    mmap_close();
    return 0;
}

