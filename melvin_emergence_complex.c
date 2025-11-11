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
    // MINIMAL NODE: Just data + transient state
    uint8_t token[64];          // Larger token size (was 16)
    uint8_t token_len;
    
    float state;                // Current activation
    float energy;               // Survival currency
    uint32_t frequency;         // Reuse count (intelligence metric!)
    uint32_t last_active_tick;
} Node;

typedef struct {
    // MINIMAL CONNECTION: Just the rule
    uint32_t src;
    uint32_t dst;
    float weight;            // Strength (grows with use)
} Connection;

// Removed: ActivationEvent, TestCase (unnecessary complexity)

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
    
    // INTELLIGENCE RULE 7: GENERALIZATION TEST
    TestCase test_cases[10];
    uint32_t test_case_count;
    
    // THE FUNDAMENTAL INTELLIGENCE METRIC
    uint32_t patterns_reused;   // Used existing pattern for new data
    uint32_t patterns_created;  // Had to create new pattern
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
    // DYNAMIC CAPACITY: Start small, grow as needed
    uint32_t initial_node_cap = 1000;      // Start with 1K (was 10K)
    uint32_t initial_conn_cap = 10000;     // Start with 10K (was 100K)
    
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
    
    if (g_debug) {
        fprintf(stderr, "[MMAP] Node size: %zu, Node offset: %zu, Conn offset: %zu\n",
                sizeof(Node), header_size, 
                (size_t)((char*)g_graph.connections - (char*)g_mmap_base));
    }
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
    n->token[0] = 0;
    n->is_hub = 0;
    n->hub_level = 0;  // Start as byte-level
    n->in_degree = 0;
    n->out_degree = 0;
    n->is_pattern = 0;
    n->pattern_member_count = 0;
    memset(n->token, 0, sizeof(n->token));
    n->token_len = 0;
    n->frequency = 0;
    n->predicted_next_node = UINT32_MAX;
    n->prediction_confidence = 0.0f;
    n->predictions_correct = 0;
    n->predictions_wrong = 0;
    
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
    if (src >= g_graph.node_count || dst >= g_graph.node_count) return UINT32_MAX;
    if (src == dst) return UINT32_MAX;  // No self-loops
    
    // Check if connection already exists
    Connection *existing = find_connection(src, dst);
    if (existing) {
        existing->weight += initial_weight * 0.1f;  // Strengthen existing
        if (existing->weight > 10.0f) existing->weight = 10.0f;
        return UINT32_MAX;
    }
    
    // DYNAMIC GROWTH: Grow if needed
    if (g_graph.connection_count >= g_graph.connection_cap) {
        // TODO: Implement dynamic growth
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
 * FORWARD DECLARATIONS
 * ======================================================================== */

void sense_input(const uint8_t *bytes, uint32_t len);
void propagate();
void mutate();
void reproduce();

/* ========================================================================
 * INTELLIGENCE RULE 7: GENERALIZATION TEST
 * ======================================================================== */

void test_generalization() {
    // Test on held-out data every 100 ticks
    if (g_graph.test_case_count == 0) return;
    
    for (uint32_t t = 0; t < g_graph.test_case_count; t++) {
        // Save current state
        uint32_t saved_log_len = g_graph.activation_log_len;
        
        // Run test input
        sense_input(g_graph.test_cases[t].input, g_graph.test_cases[t].input_len);
        
        // Propagate
        for (int hop = 0; hop < 5; hop++) {
            propagate();
        }
        
        // Check if output matches expected
        uint32_t matches = 0;
        for (uint32_t i = 0; i < g_graph.activation_log_len && 
             i < g_graph.test_cases[t].expected_len; i++) {
            if (g_graph.activation_log[i].byte == g_graph.test_cases[t].expected_output[i]) {
                matches++;
            }
        }
        
        float accuracy = (g_graph.test_cases[t].expected_len > 0) ?
                        (float)matches / (float)g_graph.test_cases[t].expected_len : 0.0f;
        
        if (accuracy > 0.8f) {
            // GENERALIZATION SUCCESS! Massive reward!
            for (uint32_t i = 21; i < g_graph.node_count; i++) {
                if (g_graph.nodes[i].state > 0.5f) {
                    g_graph.nodes[i].energy += 50.0f;
                }
            }
            
            if (g_debug) {
                fprintf(stderr, "[GENERALIZE ✓] Test case %u passed (%.0f%% accurate) - HUGE REWARD!\n",
                        t, accuracy * 100.0f);
            }
        }
        
        // Restore state
        g_graph.activation_log_len = saved_log_len;
    }
}

/* ========================================================================
 * INTELLIGENCE RULE 5: PREDICTION
 * ======================================================================== */

void predict_next() {
    // Each active node predicts what byte will come next
    // by looking at its strongest outgoing edge
    
    float activation_threshold = (g_graph.node_count > 2) ?
                                 g_graph.nodes[2].state : 0.5f;
    
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        Node *node = &g_graph.nodes[i];
        
        if (node->state <= activation_threshold) continue;
        if (node->energy <= 0.0f) continue;
        
        // Find strongest outgoing edge
        float best_weight = 0.0f;
        uint32_t best_dst = UINT32_MAX;
        
        for (uint32_t c = 0; c < g_graph.connection_count; c++) {
            Connection *conn = &g_graph.connections[c];
            if (conn->src == i && conn->weight > best_weight) {
                best_weight = conn->weight;
                best_dst = conn->dst;
            }
        }
        
        // Make prediction
        if (best_dst != UINT32_MAX && best_dst < g_graph.node_count) {
            node->predicted_next_node = g_graph.nodes[best_dst].token[0];
            node->prediction_confidence = best_weight / 10.0f;
        }
    }
}

// EVOLUTION FACTOR 6: FITNESS (accuracy = survival)
void validate_predictions(uint8_t actual_byte) {
    // Check all predictions against actual input
    // REWARD accurate predictions (intelligence!)
    
    for (uint32_t i = 21; i < g_graph.node_count; i++) {
        Node *node = &g_graph.nodes[i];
        
        if (node->prediction_confidence <= 0.0f) continue;
        
        if (node->predicted_next_node == actual_byte) {
            // CORRECT PREDICTION! This is intelligence!
            node->predictions_correct++;
            float reward = 10.0f * node->prediction_confidence;
            node->energy += reward;
            
            if (g_debug && g_graph.tick % 50 == 0) {
                fprintf(stderr, "[PREDICT ✓] Node %u predicted '%c' correctly! (+%.1f energy)\n",
                        i, actual_byte, reward);
            }
        } else {
            // Wrong prediction - penalize
            node->predictions_wrong++;
            float penalty = 2.0f * node->prediction_confidence;
            node->energy -= penalty;
            
            if (g_debug && g_graph.tick % 50 == 0 && node->prediction_confidence > 0.8f) {
                fprintf(stderr, "[PREDICT ✗] Node %u predicted '%c' but got '%c' (-%1f energy)\n",
                        i, node->predicted_next_node, actual_byte, penalty);
            }
        }
        
        // Reset prediction
        node->prediction_confidence = 0.0f;
    }
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
                    pnode->token[0] = g_graph.nodes[chain[0]].token[0];
                    
                    // INTELLIGENCE RULE 6: COMPRESSION REWARD!
                    float compression_ratio = (float)chain_len;
                    float compression_reward = compression_ratio * 20.0f;
                    pnode->energy += compression_reward;
                    
                    if (g_debug) {
                        fprintf(stderr, "[PATTERN] Created node %u for %u-node sequence (avg_weight=%.1f)\n",
                                pattern_node, chain_len, avg_weight);
                        fprintf(stderr, "  Members: ");
                        for (uint32_t i = 0; i < chain_len && i < 5; i++) {
                            fprintf(stderr, "%u('%c') ", chain[i], 
                                    g_graph.nodes[chain[i]].token[0]);
                        }
                        if (chain_len > 5) fprintf(stderr, "...");
                        fprintf(stderr, "\n");
                        fprintf(stderr, "  [COMPRESSION] %u→1 = %.0fx compression (+%.1f energy)\n",
                                chain_len, compression_ratio, compression_reward);
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
    // Store current input
    g_graph.prev_input_len = g_graph.current_input_len;
    memcpy(g_graph.prev_input, g_graph.current_input, g_graph.current_input_len);
    
    g_graph.current_input_len = len > 256 ? 256 : len;
    memcpy(g_graph.current_input, bytes, g_graph.current_input_len);
    
    float input_energy = (g_graph.node_count > 3) ? 
                         g_graph.nodes[3].state : 1.0f;
    
    // Reset for this input
    g_graph.activation_sequence_counter = 0;
    g_graph.activation_log_len = 0;
    
    // TOKENIZE INTO WORDS: Find word boundaries first, then create tokens
    uint32_t word_starts[128];
    uint32_t word_lens[128];
    uint32_t word_count = 0;
    
    uint32_t word_start = 0;
    int in_word = 0;
    
    for (uint32_t i = 0; i <= len && word_count < 128; i++) {
        uint8_t ch = (i < len) ? bytes[i] : ' ';  // Treat end as space
        int is_boundary = (ch == ' ' || ch == '\n' || ch == '\r');
        
        if (!in_word && !is_boundary) {
            // Start of new word
            word_start = i;
            in_word = 1;
        } else if (in_word && is_boundary) {
            // End of word
            word_starts[word_count] = word_start;
            word_lens[word_count] = i - word_start;
            word_count++;
            in_word = 0;
            
            if (ch == '\n' || ch == '\r') {
                g_graph.recent_active_count = 0;  // Reset on newlines
            }
        }
    }
    
    // Now create tokens for each word (all n-grams within each word)
    for (uint32_t w = 0; w < word_count; w++) {
        uint32_t start = word_starts[w];
        uint32_t word_len = word_lens[w];
        uint32_t longest_node = UINT32_MAX;
        
        // Create all n-grams for this word
        for (uint32_t ngram_len = 1; ngram_len <= word_len && ngram_len <= 10; ngram_len++) {
            for (uint32_t offset = 0; offset + ngram_len <= word_len; offset++) {
                uint32_t pos = start + offset;
                
                // Find if this n-gram already has a node
                uint32_t node_id = UINT32_MAX;
                
                for (uint32_t n = 21; n < g_graph.node_count; n++) {
                    Node *node = &g_graph.nodes[n];
                    
                    // Does this node's token match our n-gram?
                    if (node->token_len == ngram_len) {
                        int match = 1;
                        for (uint32_t b = 0; b < ngram_len; b++) {
                            if (node->token[b] != bytes[pos + b]) {
                                match = 0;
                                break;
                            }
                        }
                        
                        if (match) {
                            node_id = n;
                            break;
                        }
                    }
                }
                
                // N-gram doesn't exist - CREATE IT (low intelligence)
                if (node_id == UINT32_MAX) {
                    node_id = node_create();
                    if (node_id == UINT32_MAX) continue;
                    
                    Node *new_node = &g_graph.nodes[node_id];
                    new_node->token_len = ngram_len;
                    for (uint32_t b = 0; b < ngram_len; b++) {
                        new_node->token[b] = bytes[pos + b];
                    }
                    new_node->frequency = 1;
                    
                    // Had to create new pattern = not intelligent about this input
                    g_graph.patterns_created++;
                    
                    if (g_debug) {
                        fprintf(stderr, "[CREATE] ");
                        for (uint32_t b = 0; b < ngram_len; b++) {
                            fprintf(stderr, "%c", new_node->token[b]);
                        }
                        fprintf(stderr, " (%u) [NEW PATTERN]\n", node_id);
                    }
                } else {
                    // REUSED existing pattern = intelligence!
                    g_graph.nodes[node_id].frequency++;
                    g_graph.patterns_reused++;
                    
                    if (g_debug && ngram_len >= 2) {
                        fprintf(stderr, "[REUSE] ");
                        for (uint32_t b = 0; b < ngram_len; b++) {
                            fprintf(stderr, "%c", g_graph.nodes[node_id].token[b]);
                        }
                        fprintf(stderr, " (intelligence!)\n");
                    }
                }
                
                // Track longest token for this word
                if (offset == 0 && ngram_len == word_len) {
                    longest_node = node_id;
                }
                
                // Activate all n-grams
                g_graph.nodes[node_id].state = (float)ngram_len;
                g_graph.nodes[node_id].energy += input_energy * ngram_len;
                g_graph.nodes[node_id].last_active_tick = g_graph.tick;
            }
        }
        
        // Connect ONLY the longest token (full word) to previous word
        if (longest_node != UINT32_MAX && g_graph.recent_active_count > 0) {
            uint32_t prev_id = g_graph.recent_active[(g_graph.recent_active_count - 1) % 10];
            if (prev_id != longest_node && prev_id < g_graph.node_count) {
                uint32_t conn_id = connection_create(prev_id, longest_node, 1.0f);
                if (g_debug && conn_id != UINT32_MAX) {
                    fprintf(stderr, "[CONNECT] %u → %u\n", prev_id, longest_node);
                }
            }
        }
        
        // Add longest token to recent
        if (longest_node != UINT32_MAX) {
            g_graph.recent_active[g_graph.recent_active_count % 10] = longest_node;
            if (g_graph.recent_active_count < 10) {
                g_graph.recent_active_count++;
            }
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
                    g_graph.activation_log[g_graph.activation_log_len].byte = member->token[0];
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
                            src->token[0], dst->token[0], rule_force);
                }
            }
        } else {
            // STATISTICAL MODE: Weighted transmission (old behavior)
            signal = src->state * c->weight * 0.1f;
            if (src->is_hub) signal *= 1.5f;  // Hub boost!
            dst->state += signal;
            
            if (g_debug && signal > 0.1f) {
                fprintf(stderr, "[PROPAGATE] %u → %u (signal=%.2f, dst_state=%.2f)\n",
                        c->src, c->dst, signal, dst->state);
            }
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
                g_graph.activation_log[g_graph.activation_log_len].byte = dst->token[0];
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
                            src->token[0], dst->token[0], c->times_satisfied);
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
                            src->token[0], dst->token[0], c->times_violated);
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
    // INTELLIGENCE RULE 8: VERIFICATION
    // Multi-stage validation of output quality
    
    uint32_t rule_count = 0;
    uint32_t satisfied_count = 0;
    uint32_t violated_count = 0;
    
    // CHECK 1: Are sequential rules satisfied?
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
    
    // CHECK 2: Is pattern complete (not cut off mid-sequence)?
    int pattern_started = 0;
    int pattern_completed = 0;
    
    for (uint32_t i = 0; i < g_graph.activation_log_len; i++) {
        uint32_t node_id = g_graph.activation_log[i].node_id;
        if (node_id >= g_graph.node_count) continue;
        
        // Check if this node starts a pattern
        if (g_graph.nodes[node_id].out_degree > 2) {
            pattern_started = 1;
        }
        
        // Check if pattern reaches an end node
        if (g_graph.nodes[node_id].out_degree == 0 || 
            g_graph.nodes[node_id].out_degree == 1) {
            pattern_completed = 1;
        }
    }
    
    // CHECK 3: Self-consistency (no contradictory activations)
    int has_conflicts = 0;
    for (uint32_t i = 0; i < g_graph.activation_log_len; i++) {
        for (uint32_t j = i + 1; j < g_graph.activation_log_len; j++) {
            if (g_graph.activation_log[i].node_id == g_graph.activation_log[j].node_id) {
                // Same node activated twice - check if bytes match
                if (g_graph.activation_log[i].byte != g_graph.activation_log[j].byte) {
                    has_conflicts = 1;  // Contradiction!
                }
            }
        }
    }
    
    // OVERALL VALIDATION
    if (violated_count > 0) return 0;  // Rule violations
    if (pattern_started && !pattern_completed) return 0;  // Incomplete
    if (has_conflicts) return 0;  // Contradictions
    
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
    
    // Output by FOLLOWING THE RULES (graph structure)!
    // NOT prediction, NOT generalization
    // The connections ARE the rules/patterns learned from data
    // Output = execute the rules (follow strongest connections)
    
    typedef struct {
        uint32_t node_id;
        float score;  // Rule strength score
    } TokenCandidate;
    
    TokenCandidate candidates[256];
    uint32_t candidate_count = 0;
    
    // Collect active token nodes
    for (uint32_t i = 21; i < g_graph.node_count && candidate_count < 256; i++) {
        Node *node = &g_graph.nodes[i];
        
        if (node->state <= output_threshold) continue;
        if (node->energy <= output_cost) continue;
        if (node->token_len == 0) continue;
        
        // RULE-BASED SCORE
        // Score = how strongly the CURRENT ACTIVE NODES point to this one
        // (What do the rules say should happen?)
        
        float rule_score = 0.0f;
        
        // Sum up incoming connection strengths from active nodes
        for (uint32_t c = 0; c < g_graph.connection_count; c++) {
            Connection *conn = &g_graph.connections[c];
            
            // Does this connection point to this node?
            if (conn->dst != i) continue;
            
            // Is the source node active?
            if (conn->src >= g_graph.node_count) continue;
            Node *src = &g_graph.nodes[conn->src];
            if (src->state < 0.1f) continue;
            
            // Add weighted rule strength
            // = source activation × connection weight
            // (Strong rule from active source = high score)
            rule_score += src->state * conn->weight;
        }
        
        // If no incoming rules, use node's own activation
        if (rule_score < 0.1f) {
            rule_score = node->state;
        }
        
        candidates[candidate_count].node_id = i;
        candidates[candidate_count].score = rule_score;
        candidate_count++;
        
        if (g_debug && rule_score > 0.5f) {
            fprintf(stderr, "[RULE] Node %u: ", i);
            for (uint8_t b = 0; b < node->token_len && b < 10; b++) {
                fprintf(stderr, "%c", node->token[b]);
            }
            fprintf(stderr, " rule_score=%.2f\n", rule_score);
        }
    }
    
    // Sort by GENERALIZATION score (higher = more general = better)
    for (uint32_t i = 0; i < candidate_count; i++) {
        for (uint32_t j = i + 1; j < candidate_count; j++) {
            if (candidates[j].score > candidates[i].score) {
                TokenCandidate tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }
    
    if (g_debug && candidate_count > 0) {
        fprintf(stderr, "[OUTPUT] Following rules (strongest connections):\n");
        for (uint32_t i = 0; i < candidate_count && i < 3; i++) {
            Node *n = &g_graph.nodes[candidates[i].node_id];
            fprintf(stderr, "  %u. ", i+1);
            for (uint8_t b = 0; b < n->token_len && b < 10; b++) {
                fprintf(stderr, "%c", n->token[b]);
            }
            fprintf(stderr, " (rule_strength=%.2f)\n", candidates[i].score);
        }
    }
    
    // Output nodes reachable from INPUT through rules
    // Start from input nodes, follow strongest connection, output that
    // This is RULE EXECUTION not prediction!
    
    uint8_t output_sequence[128];
    uint32_t output_seq_len = 0;
    
    // Find which input nodes are active (from sense_input)
    uint32_t input_nodes[10];
    uint32_t input_count = 0;
    
    for (uint32_t i = 21; i < g_graph.node_count && input_count < 10; i++) {
        Node *node = &g_graph.nodes[i];
        
        // Was this node activated by input (not propagation)?
        // Check if its token appears in current_input
        int in_input = 0;
        for (uint32_t p = 0; p + node->token_len <= g_graph.current_input_len; p++) {
            int matches = 1;
            for (uint32_t b = 0; b < node->token_len; b++) {
                if (g_graph.current_input[p + b] != node->token[b]) {
                    matches = 0;
                    break;
                }
            }
            if (matches && node->token_len >= 2) {  // Only words
                input_nodes[input_count++] = i;
                in_input = 1;
                break;
            }
        }
    }
    
    // For each input node, follow its STRONGEST outgoing rule
    for (uint32_t inp = 0; inp < input_count && output_seq_len < 128; inp++) {
        uint32_t current_node = input_nodes[inp];
        output_sequence[output_seq_len++] = current_node;
        
        // Follow rules for N steps
        for (uint32_t step = 0; step < 3 && output_seq_len < 128; step++) {
            // Find strongest outgoing connection from current node
            float strongest_weight = 0.0f;
            uint32_t next_node = UINT32_MAX;
            
            for (uint32_t c = 0; c < g_graph.connection_count; c++) {
                Connection *conn = &g_graph.connections[c];
                if (conn->src == current_node && conn->weight > strongest_weight) {
                    strongest_weight = conn->weight;
                    next_node = conn->dst;
                }
            }
            
            // Found next node via rule? Follow it!
            if (next_node != UINT32_MAX && next_node < g_graph.node_count) {
                output_sequence[output_seq_len++] = next_node;
                current_node = next_node;
            } else {
                break;  // No more rules to follow
            }
        }
    }
    
    // Output the sequence determined by rules
    for (uint32_t i = 0; i < output_seq_len && output_len < 1020; i++) {
        Node *node = &g_graph.nodes[output_sequence[i]];
        
        // Output token
        for (uint32_t b = 0; b < node->token_len && output_len < 1024; b++) {
            output_buffer[output_len++] = node->token[b];
        }
        
        // Space between words
        if (i < output_seq_len - 1 && output_len < 1023) {
            output_buffer[output_len++] = ' ';
        }
        
        node->energy -= output_cost * 0.1f;
        node->energy += 1.0f;
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
            // INTELLIGENCE: Predict before seeing (Rule 5)
            predict_next();
            
            // Process FULL input (for n-gram tokenization!)
            sense_input(input_buffer, n);
            
            // Validate predictions byte-by-byte (Rule 5)
            for (ssize_t b = 0; b < n; b++) {
                validate_predictions(input_buffer[b]);
            }
            
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
        
        // EVOLUTION: Mutation and reproduction
        mutate();
        reproduce();
        
        // NETWORK OF NETWORKS: Detect hubs and patterns periodically
        if (g_graph.tick % 50 == 0 && g_graph.tick > 0) {
            detect_hubs();
        }
        
        if (g_graph.tick % 100 == 0 && g_graph.tick > 0) {
            detect_patterns();
            test_generalization();
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


/* ========================================================================
 * EVOLUTION FACTOR 7: MUTATION (explore new patterns)
 * ======================================================================== */

void mutate() {
    // Every N ticks, create random connections to explore new patterns
    if (g_graph.tick % 50 != 0) return;
    if (g_graph.node_count < 30) return;
    
    // Random mutation: 1% chance to create new connection
    if (rand() % 100 < 1) {
        uint32_t src = 21 + (rand() % (g_graph.node_count - 21));
        uint32_t dst = 21 + (rand() % (g_graph.node_count - 21));
        
        if (src != dst && g_graph.nodes[src].frequency > 0 && g_graph.nodes[dst].frequency > 0) {
            Connection *existing = find_connection(src, dst);
            if (!existing) {
                uint32_t conn_id = connection_create(src, dst, 0.1f);  // Weak mutation
                
                if (g_debug && conn_id != UINT32_MAX) {
                    fprintf(stderr, "[MUTATION] Random connection %u → %u created\n", src, dst);
                }
            }
        }
    }
}

/* ========================================================================
 * EVOLUTION FACTOR 8: REPRODUCTION (copy successful patterns)
 * ======================================================================== */

/* ========================================================================
 * INTELLIGENCE FITNESS: Measure IQ not just survival
 * ======================================================================== */

// THE FUNDAMENTAL INTELLIGENCE MEASURE (UNSUPERVISED!)
// Intelligence = ability to represent NEW data with OLD patterns
//
// User's insight: "Being able to put new inputs into old patterns"
//
// This is:
// - Unsupervised (no labels needed)
// - Objective (count reuse vs creation)
// - Captures generalization (reusing = generalizing)
// - Measurable (compression ratio)

float calculate_intelligence_fitness(Node *node) {
    float fitness = 0.0f;
    
    // FACTOR 1: REUSE FREQUENCY (60%)
    // How often is this pattern reused for new data?
    // High frequency = high intelligence (pattern generalizes!)
    if (node->frequency > 20) {
        fitness += 60.0f;
    } else {
        fitness += (float)node->frequency * 3.0f;  // Up to 60
    }
    
    // FACTOR 2: COMPRESSION (30%)
    // Longer tokens = more compression = intelligence
    // 1 node for "cat" is smarter than 3 nodes for c,a,t
    fitness += (float)node->token_len * 10.0f;  // Max 30 for 3+ chars
    
    // FACTOR 3: ENERGY (10%)
    // Patterns that help you survive = intelligent
    // But this is secondary to reuse/compression
    if (node->energy > 50.0f) {
        fitness += 10.0f;
    } else {
        fitness += node->energy * 0.2f;  // Up to 10
    }
    
    return fitness;
}

void reproduce() {
    // RELATIVE SELECTION: Kill bottom 20%, reward top 10%
    if (g_graph.tick % 200 != 0) return;
    if (g_graph.node_count < 30) return;
    
    // Calculate IQ for all active nodes
    typedef struct {
        uint32_t node_id;
        float iq_score;
    } NodeIQ;
    
    NodeIQ all_nodes[1000];
    uint32_t active_count = 0;
    
    for (uint32_t i = 21; i < g_graph.node_count && active_count < 1000; i++) {
        Node *node = &g_graph.nodes[i];
        if (node->frequency == 0) continue;
        
        all_nodes[active_count].node_id = i;
        all_nodes[active_count].iq_score = calculate_intelligence_fitness(node);
        active_count++;
    }
    
    if (active_count < 10) return;
    
    // Sort by IQ (bubble sort for simplicity)
    for (uint32_t i = 0; i < active_count; i++) {
        for (uint32_t j = i + 1; j < active_count; j++) {
            if (all_nodes[j].iq_score < all_nodes[i].iq_score) {
                NodeIQ tmp = all_nodes[i];
                all_nodes[i] = all_nodes[j];
                all_nodes[j] = tmp;
            }
        }
    }
    
    // Calculate cutoffs
    uint32_t bottom_20_cutoff = active_count / 5;  // Bottom 20%
    uint32_t top_10_cutoff = active_count - (active_count / 10);  // Top 10%
    
    uint32_t killed = 0;
    uint32_t rewarded = 0;
    
    // BOTTOM 20%: PUNISH (relative losers)
    for (uint32_t i = 0; i < bottom_20_cutoff && i < active_count; i++) {
        Node *loser = &g_graph.nodes[all_nodes[i].node_id];
        loser->energy -= 20.0f;  // Heavy penalty
        loser->frequency = (loser->frequency > 1) ? loser->frequency - 1 : 0;
        killed++;
        
        if (loser->energy < 0.0f) loser->energy = 0.0f;
    }
    
    // TOP 10%: REWARD (relative winners)
    for (uint32_t i = top_10_cutoff; i < active_count; i++) {
        Node *winner = &g_graph.nodes[all_nodes[i].node_id];
        winner->energy += all_nodes[i].iq_score * 2.0f;  // Proportional reward
        winner->frequency++;  // More important
        rewarded++;
        
        // Strengthen connections from winners
        for (uint32_t c = 0; c < g_graph.connection_count; c++) {
            if (g_graph.connections[c].src == all_nodes[i].node_id) {
                g_graph.connections[c].weight += 0.5f;
                if (g_graph.connections[c].weight > 10.0f) {
                    g_graph.connections[c].weight = 10.0f;
                }
            }
        }
    }
    
            if (g_debug) {
                // Calculate intelligence ratio for this period
                uint32_t total = g_graph.patterns_reused + g_graph.patterns_created;
                float intelligence_ratio = (total > 0) ? 
                    (float)g_graph.patterns_reused / (float)total * 100.0f : 0.0f;
                
                fprintf(stderr, "[RELATIVE SELECTION] Bottom 20%% (%u punished), Top 10%% (%u rewarded)\n",
                        killed, rewarded);
                fprintf(stderr, "  IQ range: %.1f (worst) to %.1f (best)\n",
                        all_nodes[0].iq_score, all_nodes[active_count-1].iq_score);
                fprintf(stderr, "  INTELLIGENCE: %.1f%% reused patterns (created %u, reused %u)\n",
                        intelligence_ratio, g_graph.patterns_created, g_graph.patterns_reused);
            }
            
            // Reset counters for next period
            g_graph.patterns_created = 0;
            g_graph.patterns_reused = 0;
}
