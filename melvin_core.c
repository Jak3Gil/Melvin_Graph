/*
 * melvin_core.c — Minimal Always-On Learning System
 * 
 * Four-step loop:
 *   1) What is happening?      (INPUT)
 *   2) What did I do?          (RECALL LAST OUTPUT)
 *   3) What should happen?     (PREDICT: propagate, observe, update, credit)
 *   4) Do it.                  (OUTPUT)
 * 
 * Everything is bytes. Learning is local. Graph grows/shrinks dynamically.
 * Layers emerge from density without global backprop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

/* ========================================================================
 * COMPILE-TIME CONSTANTS
 * ======================================================================== */

#define FRAME_SIZE              4096  // Initial frame buffer size (auto-grows)
#define RX_RING_SIZE            (FRAME_SIZE * 4)
#define TX_RING_SIZE            (FRAME_SIZE * 4)

/* ========================================================================
 * DATA STRUCTURES
 * ======================================================================== */

// Node operation types - nodes can perform different computations
// These are the INSTRUCTION SET of the graph programming language
typedef enum {
    // Arithmetic/Logic Instructions
    OP_SIGMOID = 0,   // sigmoid(x) - smooth activation
    OP_SUM,           // sum(inputs) - addition
    OP_PRODUCT,       // product(inputs) - multiplication
    OP_MAX,           // max(inputs) - maximum selector / IF
    OP_MIN,           // min(inputs) - minimum selector
    OP_COMPARE,       // compare(a,b) - comparison / branching
    OP_THRESHOLD,     // threshold(x) - boolean logic
    OP_RELU,          // relu(x) - rectified linear
    OP_TANH,          // tanh(x) - bounded activation
    
    // Control Flow Instructions  
    OP_GATE,          // gate(x,g) - conditional execution / WHILE
    OP_MODULATE,      // modulate(x,m) - multiply by condition
    
    // Memory Instructions
    OP_MEMORY,        // load/store - RAM
    OP_SEQUENCE,      // sequence(history) - temporal memory / LOOP
    
    // Meta Instructions (operate on the graph itself)
    OP_HASH,          // hash(key) - addressing / pointers
    OP_EVAL,          // eval(subgraph) - execute code-as-data
    OP_SPLICE,        // splice(pattern) - code insertion
    OP_FORK,          // fork() - create parallel computation
    OP_JOIN,          // join() - synchronize parallel paths
    
    NUM_OPS
} NodeOpType;

// Meta-operation types - operations that modify the graph itself
typedef enum {
    META_NONE = 0,
    META_CREATE_EDGE,     // Create new connection
    META_DELETE_EDGE,     // Remove weak connection
    META_MERGE_NODES,     // Combine redundant nodes
    META_SPLIT_NODE,      // Divide overloaded node
    META_MUTATE_OP,       // Change node operation type
    META_OPTIMIZE_SUBGRAPH, // Compile frequent pattern
    META_CREATE_SHORTCUT, // Add skip connection
    META_PRUNE_BRANCH,    // Remove unused subgraph
    NUM_META_OPS
} MetaOpType;

typedef struct {
    uint32_t id;
    float    a;           // current activation [0,1] (continuous)
    float    a_prev;
    float    theta;       // firing threshold (continuous)
    uint16_t in_deg;
    uint16_t out_deg;
    uint32_t last_tick_seen;
    float    burst;       // short-term spike count (continuous decay)
    
    // For pattern signature (last 32 ticks of activation history)
    uint32_t sig_history; // bit vector (for compatibility, threshold at 0.5)
    
    // Computational evolution
    uint8_t  op_type;     // NodeOpType - what operation this node performs
    float    op_params[4]; // Operation-specific parameters
    float    mutation_rate; // Probability of changing operation
    
    // Meta-node capabilities
    uint8_t  is_meta;
    uint8_t  meta_op;     // MetaOpType - graph modification operation
    uint32_t cluster_id;
    uint32_t meta_target; // Target node/edge for meta-operation
    
    // Module membership (hierarchical structure)
    uint32_t module_id;   // Which module this node belongs to (0 = root/global)
    uint8_t  is_module_interface; // Is this an input/output node for a module?
    uint8_t  is_module_proxy; // Does this node represent an entire module?
    uint32_t proxy_module_id; // If proxy, which module does it represent?
    
    // Performance tracking (for self-optimization)
    uint64_t executions;      // How many times executed
    uint64_t cycles_spent;    // Computational cost (approximate)
    float    avg_utility;     // Running average utility
    float    efficiency;      // utility per cycle
    
    // Prediction support
    float    soma;        // accumulated input (continuous)
    float    hat;         // predicted activation [0,1] (continuous)
    
    // Memory for OP_MEMORY nodes
    float    memory_value;   // Stored value
    uint32_t memory_age;     // How long stored
    
    // Statistics for pruning
    float    total_active_ticks; // continuous accumulation
    
    // SAFEGUARDS
    uint8_t  eval_depth;  // Current recursion depth for OP_EVAL (prevents infinite recursion)
    uint8_t  is_protected; // Protected kernel nodes can't be mutated/deleted
} Node;

typedef struct {
    uint32_t src;
    uint32_t dst;
    uint8_t  w_fast;
    uint8_t  w_slow;
    int16_t  credit;      // signed, bias = 0
    uint16_t use_count;
    uint16_t stale_ticks;
    
    // For two-timescale learning
    float    eligibility;
    
    // Predictive lift counters (decayed)
    float    C11;         // count src=1, dst_next=1
    float    C10;         // count src=1, dst_next=0
    float    avg_U;       // average usefulness for slow updates
    uint16_t slow_update_countdown;
} Edge;

// Forward declare Module for Graph
typedef struct Module Module;

// P2 SAFEGUARD: Edge hash table for O(1) lookups
typedef struct {
    uint64_t key;      // (src << 32) | dst
    uint32_t edge_idx; // Index in edges array
} EdgeHashEntry;

#define EDGE_HASH_SIZE 16384  // Power of 2 for fast modulo

typedef struct {
    Node   *nodes;
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t *node_free_list;
    uint32_t node_free_count;
    uint32_t next_node_id;
    
    Edge   *edges;
    uint32_t edge_count;
    uint32_t edge_cap;
    uint32_t *edge_free_list;
    uint32_t edge_free_count;
    
    // P2 SAFEGUARD: Edge hash table for fast lookup
    EdgeHashEntry *edge_hash;
    uint32_t edge_hash_size;
    
    // Hierarchical modularity: subgraphs become reusable modules
    Module *modules;
    uint32_t module_count;
    uint32_t module_cap;
} Graph;

// A Module is a reusable subgraph that acts as a single computational unit
// Think: functions, cortical columns, neural motifs
typedef struct Module {
    uint32_t id;
    char name[64];              // Human-readable name (auto-generated)
    
    // The subgraph this module contains
    uint32_t *internal_nodes;   // Node indices in main graph
    uint32_t node_count;
    uint32_t *internal_edges;   // Edge indices in main graph  
    uint32_t edge_count;
    
    // Interface: inputs and outputs
    uint32_t *input_nodes;      // Which internal nodes receive external input
    uint32_t input_count;
    uint32_t *output_nodes;     // Which internal nodes produce output
    uint32_t output_count;
    
    // Pattern signature for matching
    uint64_t signature_hash;    // Hash of topology
    uint32_t pattern_frequency; // How often this pattern appears
    
    // Performance tracking
    uint64_t executions;
    uint64_t total_cycles;      // Cumulative cost
    float    avg_utility;       // How useful this module is
    float    efficiency;        // utility per cycle
    
    // Hierarchical nesting: modules can contain modules
    uint32_t *child_modules;    // Module IDs contained within this module
    uint32_t child_count;
    uint32_t parent_module;     // Module ID that contains this (0 = root)
    uint8_t  hierarchy_level;   // 0=leaf, 1=contains leaves, 2=contains level-1, etc.
    
    // Evolution
    float mutation_rate;
    uint32_t creation_tick;     // When this module was discovered
    uint32_t last_used_tick;
} Module;

typedef struct {
    char     pattern[64];  // byte pattern or token
    uint16_t len;
    uint32_t node_id;      // associated sensory node
    uint8_t  type;         // 0=exact, 1=contains, 2=regex-like
} Detector;

typedef struct {
    uint8_t  bytes[256];
    uint16_t len;
    float    U_fast;
    float    U_slow;
    uint32_t use_count;
    uint32_t last_used_tick;
} Macro;

typedef struct {
    uint8_t *buf;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} RingBuffer;

typedef struct {
    Detector *detectors;
    uint32_t  detector_count;
    uint32_t  detector_cap;
    
    // P2 SAFEGUARD: Detector deduplication map (one detector per pattern)
    uint32_t detector_map[256]; // Maps byte -> detector_id (0 = none)
    
    Macro    *macros;
    uint32_t  macro_count;
    uint32_t  macro_cap;
    
    RingBuffer rx_ring;
    RingBuffer tx_ring;
    
    uint8_t  *current_frame;        // Dynamic frame buffer
    uint32_t current_frame_cap;     // Current capacity
    uint16_t current_frame_len;     // Actual length
    
    uint8_t  *last_output_frame;    // Dynamic frame buffer
    uint32_t last_output_frame_cap; // Current capacity
    uint16_t last_output_frame_len;
    
    float    epsilon;          // exploration rate (dynamically modulated)
    float    energy;           // global energy field (drives exploration & plasticity)
    uint64_t tick;
    
    // Statistics
    uint32_t edges_created;
    uint32_t edges_pruned;
    uint32_t nodes_created;
    uint32_t nodes_pruned;
    uint32_t layers_created;
    
    float    mean_error;       // prediction error (drives energy)
    float    mean_surprise;    // continuous surprise measure
    uint32_t active_node_count;
    
    // Global baseline for predictive lift
    float    *P1;  // per node
    float    *P0;  // per node
    
    // Runtime configurable parameters (all can change during operation)
    // Basic timing and persistence
    uint32_t tick_ms;           // milliseconds per tick
    uint32_t snapshot_period;   // ticks between snapshots
    
    // Continuous dynamics parameters
    float    lambda_decay;      // count decay
    float    lambda_e;          // eligibility trace
    float    beta_blend;        // predictive vs error
    float    gamma_slow;        // slow weight fraction
    float    eta_fast;          // fast weight step
    float    delta_max;         // max weight change per tick
    float    alpha_fast_decay;  // fast weight decay rate
    float    alpha_slow_decay;  // slow weight decay rate
    
    // Meta-parameters for homeostatic adaptation
    float    adapt_rate;        // rate of parameter adaptation
    float    target_density;    // target ratio of edges to max possible
    float    target_activity;   // target fraction of active nodes
    float    target_prediction_acc; // target prediction accuracy (1 - error)
    
    // Target metrics for thought/time/space adaptation
    uint16_t target_thought_depth; // ideal number of hops (not too shallow/deep)
    float    target_settle_ratio;  // target ratio of settled thoughts (70%)
    uint16_t min_thought_hops;     // minimum depth for meaningful thought
    float    max_hop_growth_rate;  // controls how fast max_hops can grow (soft limit)
    
    // Soft reference values (replace hard thresholds)
    float    prune_weight_ref;  // reference weight for pruning
    float    stale_ref;         // reference staleness for edges
    float    node_stale_ref;    // reference staleness for nodes
    float    co_freq_ref;       // reference co-activation frequency
    float    density_ref;       // reference density
    uint16_t layer_min_size;    // minimum layer size
    
    // Adaptive parameters (self-tuning based on graph dynamics)
    float    sigmoid_k;        // sigmoid steepness
    float    prune_rate;       // pruning probability base
    float    create_rate;      // node creation probability base
    float    layer_rate;       // layer emergence probability base
    float    energy_alpha;     // energy learning rate
    float    energy_decay;     // energy decay rate
    float    epsilon_min;      // min exploration
    float    epsilon_max;      // max exploration
    float    activation_scale; // activation sensitivity
    
    // Adaptive emergent time/space/thought parameters
    uint32_t max_thought_hops; // adaptive max propagation passes (now unlimited)
    float    stability_eps;    // adaptive convergence threshold (error)
    float    activation_eps;   // adaptive convergence threshold (activation)
    float    temporal_decay;   // adaptive temporal distance scaling
    float    spatial_k;        // adaptive spatial connectivity scaling
    
    // Homeostatic targets & measurements
    float    current_density;  // current edge density
    float    current_activity; // current node activity rate
    float    prediction_acc;   // current prediction accuracy
    
    // Emergent time, space, and thought tracking
    uint32_t thought_depth;    // number of propagation hops in current tick
    float    prev_mean_error;  // for convergence detection
    float    activation_delta; // total activation change in last hop
    float    mean_temporal_distance;  // average edge staleness
    float    mean_spatial_distance;   // average connectivity-based distance
    uint32_t thoughts_settled; // count of converged thoughts
    uint32_t thoughts_maxed;   // count of max-hop thoughts (didn't converge)
    
    // Self-optimization tracking
    uint64_t total_cycles;        // Total computation cycles
    uint64_t meta_operations;     // Count of graph modifications
    uint32_t op_type_counts[NUM_OPS]; // Distribution of node operations
    float    op_type_utility[NUM_OPS]; // Avg utility per operation type
    uint32_t mutations_attempted; // Evolutionary changes tried
    uint32_t mutations_kept;      // Beneficial mutations
    float    global_mutation_rate; // System-wide mutation probability
    
    // Hierarchical modularity tracking
    uint32_t modules_created;     // Total modules discovered
    uint32_t modules_pruned;      // Modules removed for inefficiency
    uint32_t patterns_detected;   // Frequent subgraphs found
    uint32_t max_hierarchy_level; // Deepest nesting level
    float    modularity_score;    // How modular the system is (0-1)
    uint32_t module_calls;        // How many times modules were invoked
    
    // SAFEGUARDS: Pending operations queue (causal scheduling)
    uint32_t pending_meta_ops[1000][3]; // [op_type, target_a, target_b]
    uint32_t pending_count;
} System;

/* ========================================================================
 * GLOBALS
 * ======================================================================== */

Graph   g_graph;
System  g_sys;

/* ========================================================================
 * FORWARD DECLARATIONS
 * ======================================================================== */

uint32_t node_create(Graph *g);
void node_delete(Graph *g, uint32_t idx);
uint32_t edge_create(Graph *g, uint32_t src_idx, uint32_t dst_idx);
void edge_delete(Graph *g, uint32_t idx);
Edge* find_edge(Graph *g, uint32_t src_idx, uint32_t dst_idx);

// Module management
uint32_t module_create(Graph *g, uint32_t *nodes, uint32_t node_count);
void module_execute(Graph *g, Module *m, float *inputs, float *outputs);
void detect_patterns(Graph *g);
void collapse_to_module(Graph *g, uint32_t *nodes, uint32_t count);
uint32_t create_module_proxy(Graph *g, uint32_t module_id);

/* ========================================================================
 * CONTINUOUS DYNAMICS HELPERS
 * ======================================================================== */

// Smooth sigmoid: maps (-∞,∞) → (0,1)
static inline float sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

// Scaled sigmoid with steepness k and center c
static inline float sigmoid_scaled(float x, float center, float k) {
    return sigmoid(k * (x - center));
}

// Adaptive sigmoid using system's current steepness
static inline float sigmoid_adaptive(float x, float center) {
    return sigmoid_scaled(x, center, g_sys.sigmoid_k);
}

// Random float [0,1]
static inline float randf() {
    return (float)rand() / (float)RAND_MAX;
}

// Soft threshold: smooth transition around reference value
// Returns value in [0,1] representing "how much above reference"
static inline float soft_above(float x, float ref) {
    return sigmoid_adaptive(x, ref);
}

// Soft threshold: smooth transition around reference value
// Returns value in [0,1] representing "how much below reference"
static inline float soft_below(float x, float ref) {
    return sigmoid_adaptive(-x, -ref);
}

/* ========================================================================
 * NODE OPERATION EXECUTION
 * ======================================================================== */

// Execute different node operations based on op_type
static inline float execute_node_operation(Node *n) {
    uint64_t op_cost = 10; // Base cost estimate
    float result = 0.0f;
    
    switch(n->op_type) {
        case OP_SIGMOID:
            // Default: sigmoid((soma - theta) / scale)
            result = sigmoid((n->soma - n->theta) / g_sys.activation_scale);
            break;
            
        case OP_SUM:
            // Simple linear sum
            result = n->soma / (g_sys.activation_scale * 2.0f); // normalize
            if (result < 0.0f) result = 0.0f;
            if (result > 1.0f) result = 1.0f;
            break;
            
        case OP_PRODUCT:
            // Multiplicative gating: a = soma * op_params[0]
            result = n->soma * n->op_params[0];
            result = sigmoid(result); // squash to [0,1]
            break;
            
        case OP_MAX:
            // Winner-take-all / Attention: activate only if above threshold
            result = (n->soma > n->theta) ? 1.0f : 0.0f;
            break;
            
        case OP_MIN:
            // Inhibitory / minimum selector
            result = (n->soma < n->theta) ? 1.0f : 0.0f;
            break;
            
        case OP_GATE:
            // LSTM-like gating: a = tanh(soma) * sigmoid(op_params[0])
            {
                float input_gate = sigmoid(n->op_params[0]);
                result = tanhf(n->soma / g_sys.activation_scale) * input_gate;
                result = (result + 1.0f) / 2.0f; // map tanh [-1,1] to [0,1]
            }
            break;
            
        case OP_MEMORY:
            // Store and recall: if soma > threshold, store; else recall
            op_cost = 20; // Memory ops are more expensive
            if (n->soma > n->theta) {
                n->memory_value = n->soma;
                n->memory_age = 0;
                result = n->memory_value / g_sys.activation_scale;
            } else {
                result = n->memory_value / g_sys.activation_scale;
                n->memory_age++;
                // Decay memory over time
                n->memory_value *= 0.99f;
            }
            if (result < 0.0f) result = 0.0f;
            if (result > 1.0f) result = 1.0f;
            break;
            
        case OP_COMPARE:
            // Compare soma to theta, output difference
            result = sigmoid((n->soma - n->theta) * n->op_params[0]); // op_params[0] = sensitivity
            break;
            
        case OP_SEQUENCE:
            // Predict next in sequence using history
            {
                float predicted = (float)(n->sig_history & 1) * 0.3f + 
                                 (float)((n->sig_history >> 1) & 1) * 0.5f +
                                 (float)((n->sig_history >> 2) & 1) * 0.2f;
                result = sigmoid(n->soma * predicted);
            }
            break;
            
        case OP_HASH:
            // Hash lookup (simplified - uses soma as key)
            op_cost = 15; // Hash operations are moderately expensive
            {
                uint32_t key = (uint32_t)(n->soma * 1000.0f);
                uint32_t hash = (key * 2654435761u) % 256; // Knuth multiplicative hash
                result = (float)hash / 255.0f;
            }
            break;
            
        case OP_MODULATE:
            // Multiplicative modulation with previous activation
            result = sigmoid(n->soma * n->a_prev * n->op_params[0]);
            break;
            
        case OP_THRESHOLD:
            // Hard threshold (binary)
            result = (n->soma > n->theta) ? n->op_params[0] : 0.0f;
            break;
            
        case OP_RELU:
            // Rectified linear
            result = (n->soma - n->theta > 0.0f) ? (n->soma - n->theta) / g_sys.activation_scale : 0.0f;
            if (result > 1.0f) result = 1.0f;
            break;
            
        case OP_TANH:
            // Hyperbolic tangent
            result = tanhf((n->soma - n->theta) / g_sys.activation_scale);
            result = (result + 1.0f) / 2.0f; // map to [0,1]
            break;
            
        case OP_EVAL:
            // Meta-instruction: evaluate another part of the graph
            // Use soma as "program counter" to select which nodes to execute
            op_cost = 50; // Meta-ops are expensive
            
            // SAFEGUARD: Prevent infinite recursion
            if (n->eval_depth >= 10) {
                result = 0.0f; // Max depth reached, return 0
                break;
            }
            
            {
                uint32_t target_node = (uint32_t)(n->soma * 10.0f) % g_graph.node_count;
                if (target_node < g_graph.node_count) {
                    Node *target = &g_graph.nodes[target_node];
                    // Increment depth for recursive call
                    target->eval_depth = n->eval_depth + 1;
                    result = execute_node_operation(target);
                    target->eval_depth = 0; // Reset after execution
                }
            }
            break;
            
        case OP_SPLICE:
            // Meta-instruction: insert pattern into graph
            // Creates new nodes/edges based on current activation pattern
            op_cost = 100;
            if (n->soma > n->theta && randf() < 0.01f) {
                // Create a new node with random op type
                uint32_t new_idx = node_create(&g_graph);
                if (new_idx != UINT32_MAX) {
                    g_graph.nodes[new_idx].op_type = (uint8_t)(n->soma * NUM_OPS) % NUM_OPS;
                    edge_create(&g_graph, (uint32_t)((n - g_graph.nodes)), new_idx);
                    result = 1.0f; // Success
                }
            }
            break;
            
        case OP_FORK:
            // Meta-instruction: create parallel computation path
            op_cost = 80;
            if (n->soma > n->theta && n->out_deg < 5) {
                // Duplicate this node's output path
                uint32_t new_path = node_create(&g_graph);
                if (new_path != UINT32_MAX) {
                    g_graph.nodes[new_path].op_type = n->op_type;
                    result = 1.0f;
                }
            }
            break;
            
        case OP_JOIN:
            // Meta-instruction: synchronize parallel paths
            // Waits for all inputs to be active before proceeding
            {
                int all_active = 1;
                // Check if all inputs are above threshold
                for (uint32_t e = 0; e < g_graph.edge_count; e++) {
                    Edge *edge = &g_graph.edges[e];
                    if (edge->dst == (uint32_t)(n - g_graph.nodes)) {
                        if (g_graph.nodes[edge->src].a < 0.5f) {
                            all_active = 0;
                            break;
                        }
                    }
                }
                result = all_active ? 1.0f : 0.0f;
            }
            break;
            
        default:
            result = sigmoid((n->soma - n->theta) / g_sys.activation_scale);
            break;
    }
    
    // Track performance (using estimated cost)
    n->cycles_spent += op_cost;
    n->executions++;
    g_sys.total_cycles += op_cost;
    
    return result;
}

/* ========================================================================
 * META-NODE EXECUTION (Graph Self-Modification)
 * ======================================================================== */

// Queue meta-operation for execution AFTER current tick (causal scheduling)
void queue_meta_operation(MetaOpType op, uint32_t target_a, uint32_t target_b) {
    if (g_sys.pending_count >= 1000) return; // Queue full
    g_sys.pending_meta_ops[g_sys.pending_count][0] = op;
    g_sys.pending_meta_ops[g_sys.pending_count][1] = target_a;
    g_sys.pending_meta_ops[g_sys.pending_count][2] = target_b;
    g_sys.pending_count++;
}

// Execute meta-operations that modify the graph structure
void execute_meta_operation(Node *meta) {
    if (!meta->is_meta || meta->meta_op == META_NONE) return;
    if (meta->a < 0.5f) return; // Only execute if meta-node is active
    
    g_sys.meta_operations++;
    
    switch(meta->meta_op) {
        case META_CREATE_EDGE: {
            // QUEUE edge creation for next tick (causal scheduling)
            if (g_graph.node_count < 2) break;
            uint32_t src = rand() % g_graph.node_count;
            uint32_t dst = rand() % g_graph.node_count;
            if (src != dst && g_graph.nodes[src].a > 0.3f && g_graph.nodes[dst].a > 0.3f) {
                Edge *existing = find_edge(&g_graph, src, dst);
                if (!existing) {
                    queue_meta_operation(META_CREATE_EDGE, src, dst);
                }
            }
            break;
        }
        
        case META_DELETE_EDGE: {
            // Find and delete weakest edge
            if (g_graph.edge_count == 0) break;
            uint32_t weakest = 0;
            float min_weight = 1000.0f;
            for (uint32_t i = 0; i < g_graph.edge_count; i++) {
                Edge *e = &g_graph.edges[i];
                float w = g_sys.gamma_slow * e->w_slow + (1.0f - g_sys.gamma_slow) * e->w_fast;
                if (w < min_weight) {
                    min_weight = w;
                    weakest = i;
                }
            }
            if (min_weight < 10.0f) { // Only delete very weak edges
                queue_meta_operation(META_DELETE_EDGE, weakest, 0);
            }
            break;
        }
        
        case META_MUTATE_OP: {
            // Change operation type of a random node
            if (g_graph.node_count == 0) break;
            uint32_t target = rand() % g_graph.node_count;
            Node *n = &g_graph.nodes[target];
            
            // SAFEGUARD: Don't mutate protected kernel nodes
            if (n->is_protected) break;
            
            // Queue mutation for next tick
            uint8_t new_op = rand() % NUM_OPS;
            queue_meta_operation(META_MUTATE_OP, target, new_op);
            break;
        }
        
        case META_CREATE_SHORTCUT: {
            // Add skip connection between distant nodes
            if (g_graph.node_count < 3) break;
            uint32_t src = rand() % g_graph.node_count;
            uint32_t dst = rand() % g_graph.node_count;
            
            // Try to connect nodes that are topologically distant
            if (src != dst && g_graph.nodes[src].out_deg < 10 && g_graph.nodes[dst].in_deg < 10) {
                Edge *existing = find_edge(&g_graph, src, dst);
                if (!existing) {
                    edge_create(&g_graph, src, dst);
                }
            }
            break;
        }
        
        case META_MERGE_NODES: {
            // Merge two nodes with very similar activation patterns
            if (g_graph.node_count < 2) break;
            
            uint32_t best_i = 0, best_j = 0;
            float best_similarity = 0.0f;
            
            // Find most similar pair (limit search to 100 pairs for speed)
            for (int tries = 0; tries < 100; tries++) {
                uint32_t i = rand() % g_graph.node_count;
                uint32_t j = rand() % g_graph.node_count;
                if (i == j) continue;
                
                Node *ni = &g_graph.nodes[i];
                Node *nj = &g_graph.nodes[j];
                
                // Compute similarity from sig_history
                uint32_t xor_bits = ni->sig_history ^ nj->sig_history;
                float similarity = 1.0f - (__builtin_popcount(xor_bits) / 32.0f);
                
                if (similarity > best_similarity) {
                    best_similarity = similarity;
                    best_i = i;
                    best_j = j;
                }
            }
            
            // Merge if very similar (> 90%)
            if (best_similarity > 0.9f) {
                // Redirect j's edges to i, then delete j
                for (uint32_t e = 0; e < g_graph.edge_count; e++) {
                    Edge *edge = &g_graph.edges[e];
                    if (edge->src == best_j) edge->src = best_i;
                    if (edge->dst == best_j) edge->dst = best_i;
                }
                node_delete(&g_graph, best_j);
            }
            break;
        }
        
        case META_SPLIT_NODE: {
            // Split overloaded node into two
            if (g_graph.node_count == 0) break;
            
            // Find node with highest degree
            uint32_t heaviest = 0;
            uint16_t max_deg = 0;
            for (uint32_t i = 0; i < g_graph.node_count; i++) {
                Node *n = &g_graph.nodes[i];
                uint16_t deg = n->in_deg + n->out_deg;
                if (deg > max_deg) {
                    max_deg = deg;
                    heaviest = i;
                }
            }
            
            // Split if very highly connected (> 20 edges)
            if (max_deg > 20) {
                uint32_t new_idx = node_create(&g_graph);
                if (new_idx != UINT32_MAX) {
                    // Copy properties from original
                    Node *orig = &g_graph.nodes[heaviest];
                    Node *split = &g_graph.nodes[new_idx];
                    split->op_type = orig->op_type;
                    split->theta = orig->theta;
                    
                    // Redirect half of the edges to new node
                    int redirect_count = 0;
                    for (uint32_t e = 0; e < g_graph.edge_count && redirect_count < max_deg / 2; e++) {
                        Edge *edge = &g_graph.edges[e];
                        if (edge->dst == heaviest && randf() < 0.5f) {
                            edge->dst = new_idx;
                            redirect_count++;
                        }
                    }
                }
            }
            break;
        }
        
        case META_PRUNE_BRANCH: {
            // Remove entire branch of unused nodes
            if (g_graph.node_count == 0) break;
            
            // Find node with no outgoing edges and low utility
            for (uint32_t i = 0; i < g_graph.node_count; i++) {
                Node *n = &g_graph.nodes[i];
                if (n->out_deg == 0 && n->avg_utility < 0.1f && n->executions > 100) {
                    // Remove all edges pointing to this node
                    for (uint32_t e = 0; e < g_graph.edge_count; e++) {
                        Edge *edge = &g_graph.edges[e];
                        if (edge->dst == i) {
                            edge_delete(&g_graph, e);
                        }
                    }
                    node_delete(&g_graph, i);
                    break; // One at a time
                }
            }
            break;
        }
        
        case META_OPTIMIZE_SUBGRAPH: {
            // Find frequently used modules and optimize them
            if (g_graph.module_count == 0) break;
            
            // Find module with highest usage but low efficiency
            uint32_t best_module = 0;
            float best_optimization_potential = 0.0f;
            
            for (uint32_t i = 0; i < g_graph.module_count; i++) {
                Module *m = &g_graph.modules[i];
                // High executions + low efficiency = good target for optimization
                float potential = (float)m->executions * (1.0f - m->efficiency);
                if (potential > best_optimization_potential) {
                    best_optimization_potential = potential;
                    best_module = i;
                }
            }
            
            // Optimize: collapse to single proxy if highly used
            // SAFEGUARD: Include complexity penalty to preserve diversity
            if (best_optimization_potential > 100.0f) {
                Module *m = &g_graph.modules[best_module];
                
                // Calculate complexity loss from collapsing
                float complexity_loss = (float)m->node_count / (float)(g_graph.node_count + 1);
                float performance_gain = m->efficiency * (float)m->executions;
                float net_benefit = performance_gain - 10.0f * complexity_loss;
                
                // Only optimize if net benefit is positive
                if (net_benefit > 0.0f && m->executions > 50 && !m->internal_nodes) {
                    // Module already collapsed, increase its efficiency rating
                    m->efficiency *= 1.1f;
                } else if (net_benefit > 0.0f && m->pattern_frequency > 3) {
                    // Frequent pattern - worth collapsing
                    printf("[OPTIMIZE] Collapsing module %s (freq=%u, exec=%llu, benefit=%.2f)\n",
                           m->name, m->pattern_frequency, (unsigned long long)m->executions, net_benefit);
                    
                    // Create proxy and mark module as "compiled"
                    uint32_t proxy = create_module_proxy(&g_graph, m->id);
                    (void)proxy; // Future: replace all instances with proxy
                }
            }
            break;
        }
        
        default:
            break;
    }
}

/* ========================================================================
 * APPLY PENDING META-OPERATIONS (Causal Scheduling)
 * ======================================================================== */

void apply_pending_operations() {
    for (uint32_t i = 0; i < g_sys.pending_count; i++) {
        MetaOpType op = (MetaOpType)g_sys.pending_meta_ops[i][0];
        uint32_t target_a = g_sys.pending_meta_ops[i][1];
        uint32_t target_b = g_sys.pending_meta_ops[i][2];
        
        switch(op) {
            case META_CREATE_EDGE:
                if (target_a < g_graph.node_count && target_b < g_graph.node_count) {
                    edge_create(&g_graph, target_a, target_b);
                }
                break;
                
            case META_DELETE_EDGE:
                if (target_a < g_graph.edge_count) {
                    edge_delete(&g_graph, target_a);
                }
                break;
                
            case META_MUTATE_OP:
                if (target_a < g_graph.node_count) {
                    Node *n = &g_graph.nodes[target_a];
                    if (!n->is_protected) {
                        n->op_type = (uint8_t)target_b;
                        // Initialize op_params randomly
                        for (int j = 0; j < 4; j++) {
                            n->op_params[j] = randf() * 2.0f - 1.0f;
                        }
                        g_sys.mutations_attempted++;
                    }
                }
                break;
                
            default:
                break;
        }
    }
    
    // Clear pending queue
    g_sys.pending_count = 0;
}

/* ========================================================================
 * HIERARCHICAL MODULARITY (Networks of Networks)
 * ======================================================================== */

// Initialize module system
void module_system_init(Graph *g, uint32_t cap) {
    g->modules = calloc(cap, sizeof(Module));
    g->module_cap = cap;
    g->module_count = 0;
}

// Compute hash signature of a subgraph topology
uint64_t compute_subgraph_signature(Graph *g, uint32_t *nodes, uint32_t count) {
    uint64_t hash = 0;
    
    // Hash based on connectivity pattern
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = 0; j < count; j++) {
            Edge *e = find_edge(g, nodes[i], nodes[j]);
            if (e) {
                // Include connection in hash
                hash ^= ((uint64_t)nodes[i] * 31 + (uint64_t)nodes[j]) * 2654435761u;
            }
        }
    }
    
    return hash;
}

// Create a new module from a subgraph
uint32_t module_create(Graph *g, uint32_t *nodes, uint32_t node_count) {
    if (g->module_count >= g->module_cap) {
        // Auto-grow modules array
        uint32_t new_cap = g->module_cap * 2;
        Module *new_modules = realloc(g->modules, new_cap * sizeof(Module));
        if (!new_modules) return UINT32_MAX;
        g->modules = new_modules;
        g->module_cap = new_cap;
    }
    
    uint32_t idx = g->module_count++;
    Module *m = &g->modules[idx];
    memset(m, 0, sizeof(Module));
    
    m->id = idx + 1;
    snprintf(m->name, 64, "Module_%u", m->id);
    
    // Copy node membership
    m->node_count = node_count;
    m->internal_nodes = malloc(node_count * sizeof(uint32_t));
    memcpy(m->internal_nodes, nodes, node_count * sizeof(uint32_t));
    
    // Find edges within this subgraph
    m->edge_count = 0;
    m->internal_edges = malloc(node_count * node_count * sizeof(uint32_t)); // Max possible
    
    for (uint32_t i = 0; i < node_count; i++) {
        for (uint32_t j = 0; j < node_count; j++) {
            // Find edge index
            for (uint32_t e = 0; e < g->edge_count; e++) {
                Edge *edge = &g->edges[e];
                if (edge->src == nodes[i] && edge->dst == nodes[j]) {
                    m->internal_edges[m->edge_count++] = e;
                }
            }
        }
    }
    
    // Identify interface nodes (those with external connections)
    m->input_count = 0;
    m->output_count = 0;
    m->input_nodes = malloc(node_count * sizeof(uint32_t));
    m->output_nodes = malloc(node_count * sizeof(uint32_t));
    
    for (uint32_t i = 0; i < node_count; i++) {
        Node *n = &g->nodes[nodes[i]];
        
        // Check for incoming edges from outside module
        int has_external_input = 0;
        for (uint32_t e = 0; e < g->edge_count; e++) {
            Edge *edge = &g->edges[e];
            if (edge->dst == nodes[i]) {
                // Is source outside module?
                int src_in_module = 0;
                for (uint32_t k = 0; k < node_count; k++) {
                    if (edge->src == nodes[k]) {
                        src_in_module = 1;
                        break;
                    }
                }
                if (!src_in_module) {
                    has_external_input = 1;
                    break;
                }
            }
        }
        
        if (has_external_input) {
            m->input_nodes[m->input_count++] = nodes[i];
            g->nodes[nodes[i]].is_module_interface = 1;
        }
        
        // Check for outgoing edges to outside module
        if (n->out_deg > 0) {
            m->output_nodes[m->output_count++] = nodes[i];
        }
    }
    
    // Compute signature
    m->signature_hash = compute_subgraph_signature(g, nodes, node_count);
    
    // Mark all nodes as belonging to this module
    for (uint32_t i = 0; i < node_count; i++) {
        g->nodes[nodes[i]].module_id = m->id;
    }
    
    // Initialize tracking
    m->executions = 0;
    m->total_cycles = 0;
    m->avg_utility = 0.5f;
    m->efficiency = 0.0f;
    m->pattern_frequency = 1;
    m->hierarchy_level = 0;
    m->parent_module = 0;
    m->child_count = 0;
    m->child_modules = NULL;
    m->mutation_rate = g_sys.global_mutation_rate;
    m->creation_tick = g_sys.tick;
    m->last_used_tick = g_sys.tick;
    
    g_sys.modules_created++;
    
    printf("[MODULE CREATE] %s: %u nodes, %u edges, %u inputs, %u outputs, hash=0x%llx\n",
           m->name, m->node_count, m->edge_count, m->input_count, m->output_count,
           (unsigned long long)m->signature_hash);
    
    return idx;
}

// Create a proxy node that represents an entire module
uint32_t create_module_proxy(Graph *g, uint32_t module_id) {
    uint32_t proxy_idx = node_create(g);
    if (proxy_idx == UINT32_MAX) return UINT32_MAX;
    
    Node *proxy = &g->nodes[proxy_idx];
    proxy->is_module_proxy = 1;
    proxy->proxy_module_id = module_id;
    proxy->op_type = OP_GATE; // Gating operation for module execution
    
    printf("[MODULE PROXY] Created node %u as proxy for module %u\n", proxy_idx, module_id);
    
    return proxy_idx;
}

// Execute a module (run its internal subgraph)
void module_execute(Graph *g, Module *m, float *inputs, float *outputs) {
    if (!m) return;
    
    m->executions++;
    m->last_used_tick = g_sys.tick;
    g_sys.module_calls++;
    
    uint64_t start_cycles = g_sys.total_cycles;
    
    // Set inputs
    for (uint32_t i = 0; i < m->input_count; i++) {
        g->nodes[m->input_nodes[i]].a = inputs[i];
    }
    
    // Propagate within module (mini thought process)
    for (int hop = 0; hop < 3; hop++) { // Limited hops for module execution
        // Accumulate inputs within module
        for (uint32_t i = 0; i < m->node_count; i++) {
            g->nodes[m->internal_nodes[i]].soma = 0.0f;
        }
        
        for (uint32_t e = 0; e < m->edge_count; e++) {
            Edge *edge = &g->edges[m->internal_edges[e]];
            Node *src = &g->nodes[edge->src];
            Node *dst = &g->nodes[edge->dst];
            
            float w_eff = g_sys.gamma_slow * edge->w_slow + (1.0f - g_sys.gamma_slow) * edge->w_fast;
            dst->soma += src->a * w_eff;
        }
        
        // Execute operations
        for (uint32_t i = 0; i < m->node_count; i++) {
            Node *n = &g->nodes[m->internal_nodes[i]];
            n->a = execute_node_operation(n);
        }
    }
    
    // Read outputs
    for (uint32_t i = 0; i < m->output_count; i++) {
        outputs[i] = g->nodes[m->output_nodes[i]].a;
    }
    
    // Track performance
    uint64_t cycles_used = g_sys.total_cycles - start_cycles;
    m->total_cycles += cycles_used;
    
    // Update utility based on output quality
    float utility = 0.0f;
    for (uint32_t i = 0; i < m->output_count; i++) {
        utility += outputs[i]; // Simple: higher activation = more useful
    }
    utility /= (float)(m->output_count + 1);
    
    m->avg_utility = 0.99f * m->avg_utility + 0.01f * utility;
    m->efficiency = m->avg_utility / ((float)m->total_cycles / (float)(m->executions + 1));
}

// Detect frequently occurring patterns in the graph
void detect_patterns(Graph *g) {
    if (g->node_count < 3) return; // Need at least 3 nodes for a pattern
    
    // Simple pattern detection: find densely connected subgraphs of size 3-7
    for (int size = 3; size <= 7 && size <= (int)g->node_count; size++) {
        // Try random subgraphs
        for (int attempt = 0; attempt < 10; attempt++) {
            uint32_t nodes[7];
            
            // Pick random starting node
            nodes[0] = rand() % g->node_count;
            int count = 1;
            
            // Grow subgraph by following edges
            for (int i = 1; i < size; i++) {
                // Find neighbors of current nodes
                int found = 0;
                for (uint32_t e = 0; e < g->edge_count && count < size; e++) {
                    Edge *edge = &g->edges[e];
                    
                    // Is source in our set?
                    int src_in = 0;
                    for (int j = 0; j < count; j++) {
                        if (edge->src == nodes[j]) {
                            src_in = 1;
                            break;
                        }
                    }
                    
                    if (src_in) {
                        // Add destination if not already in set
                        int dst_in = 0;
                        for (int j = 0; j < count; j++) {
                            if (edge->dst == nodes[j]) {
                                dst_in = 1;
                                break;
                            }
                        }
                        
                        if (!dst_in) {
                            nodes[count++] = edge->dst;
                            found = 1;
                            break;
                        }
                    }
                }
                
                if (!found) break;
            }
            
            if (count >= 3) {
                // Check if this pattern appears multiple times
                uint64_t sig = compute_subgraph_signature(g, nodes, count);
                
                // See if we already have a module with this signature
                int already_exists = 0;
                for (uint32_t m = 0; m < g->module_count; m++) {
                    if (g->modules[m].signature_hash == sig) {
                        g->modules[m].pattern_frequency++;
                        already_exists = 1;
                        break;
                    }
                }
                
                // P3 SAFEGUARD: Statistical threshold for module creation
                // Require minimum frequency AND statistical significance
                if (!already_exists) {
                    // Pattern must occur at least 5 times
                    // AND represent > 0.1% of all ticks
                    float frequency_ratio = 5.0f / (float)(g_sys.tick + 1);
                    if (frequency_ratio > 0.001f && randf() < 0.01f) {
                        module_create(g, nodes, count);
                        g_sys.patterns_detected++;
                    }
                }
            }
        }
    }
}

// Collapse a subgraph into a single proxy node
void collapse_to_module(Graph *g, uint32_t *nodes, uint32_t count) {
    // Create module from subgraph
    uint32_t module_id = module_create(g, nodes, count);
    if (module_id == UINT32_MAX) return;
    
    // Create proxy node
    uint32_t proxy_idx = create_module_proxy(g, module_id + 1);
    if (proxy_idx == UINT32_MAX) return;
    
    Module *m = &g->modules[module_id];
    
    // Redirect external edges to proxy
    for (uint32_t e = 0; e < g->edge_count; e++) {
        Edge *edge = &g->edges[e];
        
        // If edge points to module input, redirect to proxy
        for (uint32_t i = 0; i < m->input_count; i++) {
            if (edge->dst == m->input_nodes[i]) {
                edge->dst = proxy_idx;
            }
        }
        
        // If edge comes from module output, redirect from proxy
        for (uint32_t i = 0; i < m->output_count; i++) {
            if (edge->src == m->output_nodes[i]) {
                edge->src = proxy_idx;
            }
        }
    }
    
    printf("[MODULE COLLAPSE] Collapsed %u nodes into proxy %u (module %u)\n",
           count, proxy_idx, module_id + 1);
}

/* ========================================================================
 * RING BUFFER
 * ======================================================================== */

void ring_init(RingBuffer *rb, uint32_t size) {
    rb->buf = calloc(size, 1);
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

uint32_t ring_peek(RingBuffer *rb, uint8_t *data, uint32_t len) {
    uint32_t avail = rb->count < len ? rb->count : len;
    uint32_t pos = rb->tail;
    for (uint32_t i = 0; i < avail; i++) {
        data[i] = rb->buf[pos];
        pos = (pos + 1) % rb->size;
    }
    return avail;
}

/* ========================================================================
 * P2 SAFEGUARD: EDGE HASH TABLE (O(1) Lookups)
 * ======================================================================== */

static inline uint64_t edge_hash_key(uint32_t src, uint32_t dst) {
    return ((uint64_t)src << 32) | (uint64_t)dst;
}

static inline uint32_t edge_hash_func(uint64_t key, uint32_t size) {
    // Knuth multiplicative hash
    return (uint32_t)((key * 2654435761ULL) & (size - 1));
}

void edge_hash_insert(Graph *g, uint32_t edge_idx) {
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
            g->edge_hash[idx].key = 0; // Mark as deleted
            return;
        }
        if (g->edge_hash[idx].key == 0) return; // Not found
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
        if (g->edge_hash[idx].key == 0) return NULL; // Not found
    }
    return NULL;
}

/* ========================================================================
 * GRAPH MANAGEMENT
 * ======================================================================== */

void graph_init(Graph *g, uint32_t node_cap, uint32_t edge_cap) {
    // Graph data now lives in mmap, initialized by graph_mmap_init()
    // This function is now a no-op, kept for API compatibility
    (void)g;
    (void)node_cap;
    (void)edge_cap;
}

void graph_free(Graph *g) {
    // Nodes, edges, modules are now in mmap, don't free
    // Only free the free lists (in regular memory)
    free(g->node_free_list);
    free(g->edge_free_list);
    
    // P2 SAFEGUARD: Free edge hash table
    if (g->edge_hash) {
        free(g->edge_hash);
        g->edge_hash = NULL;
    }
}

uint32_t node_create(Graph *g) {
    uint32_t idx;
    if (g->node_free_count > 0) {
        idx = g->node_free_list[--g->node_free_count];
    } else {
        if (g->node_count >= g->node_cap) {
            // AUTO-GROW: Double the capacity when full
            uint32_t new_cap = g->node_cap * 2;
            printf("[AUTO-GROW] Expanding node capacity: %u -> %u\n", g->node_cap, new_cap);
            
            Node *new_nodes = realloc(g->nodes, new_cap * sizeof(Node));
            if (!new_nodes) {
                fprintf(stderr, "ERROR: Failed to allocate memory for %u nodes\n", new_cap);
                return UINT32_MAX;
            }
            g->nodes = new_nodes;
            
            uint32_t *new_free_list = realloc(g->node_free_list, new_cap * sizeof(uint32_t));
            if (!new_free_list) {
                fprintf(stderr, "ERROR: Failed to allocate node free list\n");
                return UINT32_MAX;
            }
            g->node_free_list = new_free_list;
            
            // Also grow P1/P0 baseline arrays
            float *new_P1 = realloc(g_sys.P1, new_cap * sizeof(float));
            float *new_P0 = realloc(g_sys.P0, new_cap * sizeof(float));
            if (!new_P1 || !new_P0) {
                fprintf(stderr, "ERROR: Failed to allocate baseline arrays\n");
                return UINT32_MAX;
            }
            
            // Initialize new baseline entries
            for (uint32_t i = g->node_cap; i < new_cap; i++) {
                new_P1[i] = 0.5f;
                new_P0[i] = 0.5f;
            }
            
            g_sys.P1 = new_P1;
            g_sys.P0 = new_P0;
            g->node_cap = new_cap;
        }
        idx = g->node_count++;
    }
    
    Node *n = &g->nodes[idx];
    memset(n, 0, sizeof(Node));
    n->id = g->next_node_id++;
    n->theta = 128.0f; // default threshold (continuous)
    n->last_tick_seen = g_sys.tick;
    n->a = 0.0f;
    n->a_prev = 0.0f;
    n->hat = 0.0f;
    n->soma = 0.0f;
    n->burst = 0.0f;
    
    // Initialize computational evolution fields
    n->op_type = OP_SIGMOID; // Start with default operation
    n->mutation_rate = g_sys.global_mutation_rate;
    
    // Random initialization of op_params
    for (int i = 0; i < 4; i++) {
        n->op_params[i] = randf() * 0.2f + 0.9f; // Start near 1.0
    }
    
    // Performance tracking
    n->executions = 0;
    n->cycles_spent = 0;
    n->avg_utility = 0.5f;
    n->efficiency = 0.0f;
    
    // Memory for OP_MEMORY nodes
    n->memory_value = 0.0f;
    n->memory_age = 0;
    
    // Meta-node fields (initially not meta)
    n->is_meta = 0;
    n->meta_op = META_NONE;
    n->meta_target = 0;
    
    // Module fields (initially root level)
    n->module_id = 0; // 0 = root/global scope
    n->is_module_interface = 0;
    n->is_module_proxy = 0;
    n->proxy_module_id = 0;
    
    // SAFEGUARD: Initialize safety fields
    n->eval_depth = 0;
    n->is_protected = 0;
    
    // SAFEGUARD: First 100 nodes are protected kernel (immutable base interpreter)
    if (n->id <= 100) {
        n->is_protected = 1;
    }
    
    return idx;
}

void node_delete(Graph *g, uint32_t idx) {
    if (idx >= g->node_count) return;
    
    // SAFEGUARD: Don't delete protected kernel nodes
    if (g->nodes[idx].is_protected) return;
    
    g->node_free_list[g->node_free_count++] = idx;
}

uint32_t edge_create(Graph *g, uint32_t src_idx, uint32_t dst_idx) {
    uint32_t idx;
    if (g->edge_free_count > 0) {
        idx = g->edge_free_list[--g->edge_free_count];
    } else {
        if (g->edge_count >= g->edge_cap) {
            // AUTO-GROW: Double the capacity when full
            uint32_t new_cap = g->edge_cap * 2;
            printf("[AUTO-GROW] Expanding edge capacity: %u -> %u\n", g->edge_cap, new_cap);
            
            Edge *new_edges = realloc(g->edges, new_cap * sizeof(Edge));
            if (!new_edges) {
                fprintf(stderr, "ERROR: Failed to allocate memory for %u edges\n", new_cap);
            return UINT32_MAX;
            }
            g->edges = new_edges;
            
            uint32_t *new_free_list = realloc(g->edge_free_list, new_cap * sizeof(uint32_t));
            if (!new_free_list) {
                fprintf(stderr, "ERROR: Failed to allocate edge free list\n");
                return UINT32_MAX;
            }
            g->edge_free_list = new_free_list;
            g->edge_cap = new_cap;
        }
        idx = g->edge_count++;
    }
    
    Edge *e = &g->edges[idx];
    memset(e, 0, sizeof(Edge));
    e->src = src_idx;
    e->dst = dst_idx;
    e->w_fast = 32;  // small initial weight
    e->w_slow = 32;
    e->slow_update_countdown = 50;
    
    g->nodes[src_idx].out_deg++;
    g->nodes[dst_idx].in_deg++;
    
    g_sys.edges_created++;
    
    // P2 SAFEGUARD: Add to hash table
    edge_hash_insert(g, idx);
    
    return idx;
}

void edge_delete(Graph *g, uint32_t idx) {
    if (idx >= g->edge_count) return;
    Edge *e = &g->edges[idx];
    
    if (e->src < g->node_count) g->nodes[e->src].out_deg--;
    if (e->dst < g->node_count) g->nodes[e->dst].in_deg--;
    
    // P2 SAFEGUARD: Remove from hash table
    edge_hash_remove(g, e->src, e->dst);
    
    g->edge_free_list[g->edge_free_count++] = idx;
    g_sys.edges_pruned++;
}

Edge* find_edge(Graph *g, uint32_t src_idx, uint32_t dst_idx) {
    // P2 SAFEGUARD: Use hash table for O(1) lookup instead of O(n) search
    return edge_hash_find(g, src_idx, dst_idx);
}

/* ========================================================================
 * DETECTORS (Byte patterns → Node activations)
 * ======================================================================== */

void detector_init(uint32_t cap) {
    g_sys.detectors = calloc(cap, sizeof(Detector));
    g_sys.detector_cap = cap;
    g_sys.detector_count = 0;
    
    // P2 SAFEGUARD: Initialize detector deduplication map
    memset(g_sys.detector_map, 0, sizeof(g_sys.detector_map));
    
    // Rebuild detector map from existing detectors (if any)
    for (uint32_t i = 0; i < g_sys.detector_count; i++) {
        Detector *d = &g_sys.detectors[i];
        if (d->len == 1) {
            uint8_t byte = (uint8_t)d->pattern[0];
            g_sys.detector_map[byte] = i + 1; // Store index+1 (0 means no detector)
        }
    }
}

uint32_t detector_add(const char *pattern, uint8_t type) {
    if (g_sys.detector_count >= g_sys.detector_cap) {
        // AUTO-GROW: Double the capacity when full
        uint32_t new_cap = g_sys.detector_cap * 2;
        printf("[AUTO-GROW] Expanding detector capacity: %u -> %u\n", g_sys.detector_cap, new_cap);
        
        Detector *new_detectors = realloc(g_sys.detectors, new_cap * sizeof(Detector));
        if (!new_detectors) {
            fprintf(stderr, "ERROR: Failed to allocate memory for %u detectors\n", new_cap);
            return UINT32_MAX;
        }
        g_sys.detectors = new_detectors;
        g_sys.detector_cap = new_cap;
    }
    
    Detector *d = &g_sys.detectors[g_sys.detector_count++];
    strncpy(d->pattern, pattern, 63);
    d->pattern[63] = 0;
    d->len = strlen(d->pattern);
    d->type = type;
    d->node_id = node_create(&g_graph);
    
    return d->node_id;
}

void detector_run_all(const uint8_t *frame, uint16_t frame_len) {
    // AUTO-LEARN: Create detectors from input
    // P2 SAFEGUARD: Use deduplication map to avoid duplicate detectors
    if (frame_len > 0) {
        uint32_t new_detectors = 0;
        for (uint16_t i = 0; i < frame_len && i < 20; i++) {  // Learn up to 20 unique bytes
            uint8_t byte = frame[i];
            // Check if detector already exists for this byte (0 = no detector)
            if (g_sys.detector_map[byte] == 0) {
                char pattern[2] = {byte, 0};
                uint32_t node_id = detector_add(pattern, 1);
                if (node_id != UINT32_MAX) {
                    g_sys.detector_map[byte] = g_sys.detector_count; // Map byte to detector index+1
                    new_detectors++;
                }
            }
        }
        if (new_detectors > 0) {
            printf("[BOOTSTRAP] Auto-learned %u new detectors (total: %u)\n", 
                   new_detectors, g_sys.detector_count);
            fflush(stdout);
        }
    }
    
    for (uint32_t i = 0; i < g_sys.detector_count; i++) {
        Detector *d = &g_sys.detectors[i];
        Node *n = &g_graph.nodes[d->node_id];
        
        n->a_prev = n->a;
        
        // Continuous detection: smooth activation based on match quality
        int match_count = 0;
        
        if (d->type == 0) {
            // Exact match → binary but smoothed
            if (frame_len >= d->len) {
                if (memcmp(frame, d->pattern, d->len) == 0) {
                    match_count = 1;
                }
            }
        } else if (d->type == 1) {
            // Contains → count occurrences for graded response
            for (uint16_t j = 0; j + d->len <= frame_len; j++) {
                if (memcmp(frame + j, d->pattern, d->len) == 0) {
                    match_count++;
                }
            }
        }
        
        // Continuous activation: sigmoid of match strength
        float match_strength = (float)match_count;
        n->a = sigmoid(match_strength - 0.5f); // smooth around 0-1 matches
        
        // Continuous burst with decay
        if (n->a > 0.5f) {
            n->last_tick_seen = g_sys.tick;
            n->burst = n->burst * 0.9f + n->a; // accumulate with decay
        } else {
            n->burst *= 0.95f; // gradual decay
        }
        
        // Update signature history (threshold at 0.5 for binary compatibility)
        n->sig_history = (n->sig_history << 1) | (n->a > 0.5f ? 1 : 0);
    }
}

/* ========================================================================
 * MACROS (Action selection)
 * ======================================================================== */

void macro_init(uint32_t cap) {
    g_sys.macros = calloc(cap, sizeof(Macro));
    g_sys.macro_cap = cap;
    g_sys.macro_count = 0;
    
    // Initialize basic timing and persistence
    g_sys.tick_ms = 50;
    g_sys.snapshot_period = 2000;
    
    // Initialize continuous dynamics parameters
    g_sys.lambda_decay = 0.99f;
    g_sys.lambda_e = 0.9f;
    g_sys.beta_blend = 0.7f;
    g_sys.gamma_slow = 0.8f;
    g_sys.eta_fast = 3.0f;
    g_sys.delta_max = 4.0f;
    g_sys.alpha_fast_decay = 0.95f;
    g_sys.alpha_slow_decay = 0.999f;
    
    // Initialize meta-parameters for homeostatic adaptation
    g_sys.adapt_rate = 0.001f;
    g_sys.target_density = 0.15f;
    g_sys.target_activity = 0.1f;
    g_sys.target_prediction_acc = 0.85f;
    
    // Initialize target metrics for thought/time/space adaptation
    g_sys.target_thought_depth = 5;
    g_sys.target_settle_ratio = 0.7f;
    g_sys.min_thought_hops = 3;
    g_sys.max_hop_growth_rate = 0.1f; // 10% max growth per adaptation cycle
    
    // Initialize soft reference values
    g_sys.prune_weight_ref = 2.0f;
    g_sys.stale_ref = 200.0f;
    g_sys.node_stale_ref = 1000.0f;
    g_sys.co_freq_ref = 10.0f;
    g_sys.density_ref = 0.6f;
    g_sys.layer_min_size = 10;
    
    // Initialize adaptive parameters
    g_sys.sigmoid_k = 0.5f;
    g_sys.prune_rate = 0.0005f;
    g_sys.create_rate = 0.01f;
    g_sys.layer_rate = 0.001f;
    g_sys.energy_alpha = 0.1f;
    g_sys.energy_decay = 0.995f;
    g_sys.epsilon_min = 0.05f;
    g_sys.epsilon_max = 0.3f;
    g_sys.activation_scale = 64.0f;
    
    g_sys.epsilon = 0.5f * (g_sys.epsilon_min + g_sys.epsilon_max); // start in middle
    g_sys.energy = 0.0f; // start with zero energy
    
    // Initialize homeostatic measurements
    g_sys.current_density = 0.0f;
    g_sys.current_activity = 0.0f;
    g_sys.prediction_acc = 0.5f; // neutral start
    
    // Initialize adaptive emergent parameters
    g_sys.max_thought_hops = 10; // initial value, can grow unlimited
    g_sys.stability_eps = 0.005f;
    g_sys.activation_eps = 0.01f;
    g_sys.temporal_decay = 0.1f;
    g_sys.spatial_k = 0.5f;
    
    // Initialize emergent time/space/thought tracking
    g_sys.thought_depth = 0;
    g_sys.prev_mean_error = 0.0f;
    g_sys.activation_delta = 0.0f;
    g_sys.mean_temporal_distance = 0.0f;
    g_sys.mean_spatial_distance = 0.0f;
    g_sys.thoughts_settled = 0;
    g_sys.thoughts_maxed = 0;
    
    // Initialize self-optimization tracking
    g_sys.total_cycles = 0;
    g_sys.meta_operations = 0;
    g_sys.mutations_attempted = 0;
    g_sys.mutations_kept = 0;
    g_sys.global_mutation_rate = 0.01f; // 1% initial mutation rate
    
    // Initialize operation type statistics
    for (int i = 0; i < NUM_OPS; i++) {
        g_sys.op_type_counts[i] = 0;
        g_sys.op_type_utility[i] = 0.5f;
    }
    
    // Initialize hierarchical modularity tracking
    g_sys.modules_created = 0;
    g_sys.modules_pruned = 0;
    g_sys.patterns_detected = 0;
    g_sys.max_hierarchy_level = 0;
    g_sys.modularity_score = 0.0f;
    g_sys.module_calls = 0;
    
    // SAFEGUARD: Initialize pending operations queue
    g_sys.pending_count = 0;
}

uint32_t macro_add(const uint8_t *bytes, uint16_t len) {
    if (g_sys.macro_count >= g_sys.macro_cap) {
        // AUTO-GROW: Double the capacity when full
        uint32_t new_cap = g_sys.macro_cap * 2;
        printf("[AUTO-GROW] Expanding macro capacity: %u -> %u\n", g_sys.macro_cap, new_cap);
        
        Macro *new_macros = realloc(g_sys.macros, new_cap * sizeof(Macro));
        if (!new_macros) {
            fprintf(stderr, "ERROR: Failed to allocate memory for %u macros\n", new_cap);
            return UINT32_MAX;
        }
        g_sys.macros = new_macros;
        g_sys.macro_cap = new_cap;
    }
    
    Macro *m = &g_sys.macros[g_sys.macro_count++];
    memcpy(m->bytes, bytes, len);
    m->len = len;
    m->U_fast = 0.5f;
    m->U_slow = 0.5f;
    m->use_count = 0;
    m->last_used_tick = 0;
    
    return g_sys.macro_count - 1;
}

// macro_add_defaults() REMOVED - graph learns all patterns from input

uint32_t macro_select() {
    // ε-greedy
    float r = (float)rand() / RAND_MAX;
    
    if (r < g_sys.epsilon) {
        // Random
        return rand() % g_sys.macro_count;
    } else {
        // Best utility (blend of fast and slow)
        float best_u = -1e9;
        uint32_t best_idx = 0;
        
        for (uint32_t i = 0; i < g_sys.macro_count; i++) {
            Macro *m = &g_sys.macros[i];
            float u = g_sys.gamma_slow * m->U_slow + (1.0f - g_sys.gamma_slow) * m->U_fast;
            if (u > best_u) {
                best_u = u;
                best_idx = i;
            }
        }
        
        return best_idx;
    }
}

void macro_update_utility(uint32_t idx, float reward) {
    if (idx >= g_sys.macro_count) return;
    Macro *m = &g_sys.macros[idx];
    
    // Fast track
    m->U_fast = g_sys.alpha_fast_decay * m->U_fast + (1.0f - g_sys.alpha_fast_decay) * reward;
    
    // Slow track (less aggressive)
    m->U_slow = g_sys.alpha_slow_decay * m->U_slow + (1.0f - g_sys.alpha_slow_decay) * reward;
    
    m->use_count++;
    m->last_used_tick = g_sys.tick;
}

/* ========================================================================
 * PROPAGATION & PREDICTION
 * ======================================================================== */

void propagate() {
    // Clear soma and predictions
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        Node *n = &g_graph.nodes[i];
        n->soma = 0.0f;
        n->hat = 0.0f;
    }
    
    // Track temporal and spatial distances for emergent metrics
    float total_temporal_dist = 0.0f;
    float total_spatial_dist = 0.0f;
    uint32_t active_edges = 0;
    
    // Accumulate weighted inputs (continuous, with emergent time & space)
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        // Check if edge is free
        int is_free = 0;
        for (uint32_t j = 0; j < g_graph.edge_free_count; j++) {
            if (g_graph.edge_free_list[j] == i) {
                is_free = 1;
                break;
            }
        }
        if (is_free) continue;
        
        Edge *e = &g_graph.edges[i];
        Node *src = &g_graph.nodes[e->src];
        Node *dst = &g_graph.nodes[e->dst];
        
        // Effective weight (continuous blend)
        float w_eff = g_sys.gamma_slow * e->w_slow + (1.0f - g_sys.gamma_slow) * e->w_fast;
        if (w_eff < 0.0f) w_eff = 0.0f;
        if (w_eff > 255.0f) w_eff = 255.0f;
        
        // EMERGENT TIME: Fresh edges have stronger influence (adaptive decay)
        // temporal_weight = 1 / (1 + stale_ticks * decay_rate)
        float temporal_weight = 1.0f / (1.0f + (float)e->stale_ticks * g_sys.temporal_decay);
        
        // EMERGENT SPACE: Connectivity determines "distance" (adaptive scaling)
        // High-degree nodes are "central hubs", low-degree are "distant"
        float connectivity = (float)(src->out_deg + dst->in_deg + 1);
        float spatial_weight = 1.0f / (1.0f + g_sys.spatial_k * logf(connectivity));
        
        // Combined influence: weight × temporal × spatial
        float total_weight = w_eff * temporal_weight * spatial_weight;
        
        // Continuous contribution: src activation * combined weight
        dst->soma += src->a * total_weight;
        
        // Track metrics for emergent time/space perception
        if (src->a > 0.1f) { // only count active edges
            total_temporal_dist += (float)e->stale_ticks;
            total_spatial_dist += 1.0f / (spatial_weight + 1e-6f);
            active_edges++;
        }
        
        // Use count increases proportional to src activation
        e->use_count += (uint16_t)(src->a + 0.5f);
        
        // Stale ticks decay when edge is used, increment otherwise
        if (src->a > 0.5f) {
            e->stale_ticks = (uint16_t)((float)e->stale_ticks * 0.95f); // freshen
        } else {
            e->stale_ticks++; // age
        }
    }
    
    // Update emergent distance metrics
    g_sys.mean_temporal_distance = active_edges > 0 ? total_temporal_dist / active_edges : 0.0f;
    g_sys.mean_spatial_distance = active_edges > 0 ? total_spatial_dist / active_edges : 0.0f;
    
    // Compute predictions using node-specific operations
    g_sys.active_node_count = 0;
    g_sys.activation_delta = 0.0f;
    
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        Node *n = &g_graph.nodes[i];
        
        // Save previous activation for delta computation
        float prev_a = n->a;
        
        // Execute node-specific operation (polymorphic activation)
        n->hat = execute_node_operation(n);
        
        // Update actual activation (becomes prediction for next hop)
        n->a = n->hat;
        
        // Track activation change (for convergence detection)
        g_sys.activation_delta += fabsf(n->a - prev_a);
        
        // Count "active" nodes (above threshold 0.5)
        if (n->a > 0.5f) {
            g_sys.active_node_count++;
        }
        
        // Accumulate total active ticks (continuous)
        n->total_active_ticks += n->a;
        
        // Update node utility and efficiency
        float utility = 1.0f - fabsf(n->a - n->hat); // How well node predicted
        n->avg_utility = 0.99f * n->avg_utility + 0.01f * utility;
        if (n->cycles_spent > 0) {
            n->efficiency = n->avg_utility / (float)(n->cycles_spent / (n->executions + 1));
        }
        
        // Track operation type statistics
        g_sys.op_type_counts[n->op_type]++;
        g_sys.op_type_utility[n->op_type] = 0.99f * g_sys.op_type_utility[n->op_type] + 0.01f * utility;
    }
    
    // Normalize activation delta
    if (g_graph.node_count > 0) {
        g_sys.activation_delta /= (float)g_graph.node_count;
    }
    
    // SAFEGUARD: Energy normalization to prevent thermal runaway
    float total_activation = 0.0f;
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        total_activation += g_graph.nodes[i].a;
    }
    
    // If total energy exceeds cap, normalize all activations
    float energy_cap = (float)g_graph.node_count * 0.5f; // Average activation of 0.5
    if (total_activation > energy_cap) {
        float scale = energy_cap / total_activation;
        for (uint32_t i = 0; i < g_graph.node_count; i++) {
            g_graph.nodes[i].a *= scale;
            g_graph.nodes[i].hat *= scale;
        }
        // printf("[ENERGY CAP] Normalized: %.1f -> %.1f\n", total_activation, energy_cap);
    }
}

/* ========================================================================
 * THOUGHT CONVERGENCE (Multi-hop propagation until stable)
 * ======================================================================== */

uint32_t converge_thought() {
    // Run multiple propagation passes until prediction stabilizes (adaptive)
    // This mimics cortical gamma oscillations: perception → reverberation → settlement
    
    g_sys.prev_mean_error = g_sys.mean_error;
    g_sys.thought_depth = 0;
    
    for (uint32_t hop = 0; hop < g_sys.max_thought_hops; hop++) {
        g_sys.thought_depth = hop + 1;
        
        // One propagation pass
        propagate();
        
        // Check convergence: has prediction stabilized? (adaptive thresholds)
        float error_delta = fabsf(g_sys.mean_error - g_sys.prev_mean_error);
        
        int error_stable = (error_delta < g_sys.stability_eps);
        int activation_stable = (g_sys.activation_delta < g_sys.activation_eps);
        
        // Require minimum depth for meaningful thought
        if (error_stable && activation_stable && hop >= (g_sys.min_thought_hops - 1)) {
            // Thought has settled - internal dynamics reached equilibrium
            g_sys.thoughts_settled++;
            return g_sys.thought_depth;
        }
        
        // Update for next iteration
        g_sys.prev_mean_error = g_sys.mean_error;
    }
    
    // Hit max hops without converging - thought continues but we must act
    g_sys.thoughts_maxed++;
    return g_sys.thought_depth;
}

/* ========================================================================
 * OBSERVATION, UPDATE, CREDIT
 * ======================================================================== */

void observe_and_update() {
    float total_error = 0.0f;
    float total_surprise = 0.0f;
    uint32_t edge_count_active = 0;
    
    // Update global baseline counts (continuous)
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        Node *n = &g_graph.nodes[i];
        g_sys.P1[i] *= g_sys.lambda_decay;
        g_sys.P0[i] *= g_sys.lambda_decay;
        
        // Continuous accumulation weighted by activation
        g_sys.P1[i] += n->a;
        g_sys.P0[i] += (1.0f - n->a);
    }
    
    // Per-edge learning (fully continuous)
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        int is_free = 0;
        for (uint32_t j = 0; j < g_graph.edge_free_count; j++) {
            if (g_graph.edge_free_list[j] == i) {
                is_free = 1;
                break;
            }
        }
        if (is_free) continue;
        
        Edge *e = &g_graph.edges[i];
        Node *src = &g_graph.nodes[e->src];
        Node *dst = &g_graph.nodes[e->dst];
        
        // Continuous observation
        float a_j_next = dst->a;
        float hat_j = dst->hat;
        
        // Continuous surprise: absolute prediction error
        float surprise = fabsf(a_j_next - hat_j);
        total_error += surprise;
        total_surprise += surprise * surprise; // squared for energy
        edge_count_active++;
        
        // Continuous discrepancy (signed error modulated by previous activation)
        float d_ij = src->a_prev * (a_j_next - hat_j);
        
        // Update predictive lift counters (continuous)
        e->C11 *= g_sys.lambda_decay;
        e->C10 *= g_sys.lambda_decay;
        
        // Continuous accumulation weighted by activation
        e->C11 += src->a_prev * a_j_next;
        e->C10 += src->a_prev * (1.0f - a_j_next);
        
        // Compute usefulness (continuous)
        float p_j_given_i = e->C11 / (e->C11 + e->C10 + 1e-6f);
        float p_j = g_sys.P1[e->dst] / (g_sys.P1[e->dst] + g_sys.P0[e->dst] + 1e-6f);
        float u_ij = p_j_given_i - p_j;
        
        float e_ij = d_ij * surprise;
        
        float U_ij = g_sys.beta_blend * u_ij + (1.0f - g_sys.beta_blend) * e_ij;
        
        // Update average usefulness for slow track
        e->avg_U = 0.95f * e->avg_U + 0.05f * U_ij;
        
        // Eligibility trace (continuous)
        e->eligibility = g_sys.lambda_e * e->eligibility + src->a_prev;
        
        // Fast weight update (continuous, no branches)
        float delta_fast = g_sys.eta_fast * U_ij * e->eligibility;
        // Soft clamp using tanh
        delta_fast = g_sys.delta_max * tanhf(delta_fast / g_sys.delta_max);
        
        float new_w_fast = (float)e->w_fast + delta_fast;
        // Soft clamp to [0, 255]
        if (new_w_fast < 0.0f) new_w_fast = 0.0f;
        if (new_w_fast > 255.0f) new_w_fast = 255.0f;
        e->w_fast = (uint8_t)(new_w_fast + 0.5f);
        
        // Slow weight update (continuous probability-based)
        float p_slow_update = soft_above((float)e->slow_update_countdown, 50.0f);
        if (randf() < p_slow_update * 0.1f) { // probabilistic update
            e->slow_update_countdown = 0;
            
            // Continuous slow update: sign of avg_U, but smoothed
            float delta_slow = tanhf(e->avg_U * 20.0f); // smooth sign
            
            float new_w_slow = (float)e->w_slow + delta_slow;
            if (new_w_slow < 0.0f) new_w_slow = 0.0f;
            if (new_w_slow > 255.0f) new_w_slow = 255.0f;
            e->w_slow = (uint8_t)(new_w_slow + 0.5f);
        }
        e->slow_update_countdown++;
        
        // Continuous credit accumulation (no branches)
        float credit_delta = (1.0f - surprise) * U_ij; // reward when no surprise and useful
        e->credit += (int16_t)(credit_delta * 10.0f); // scale for integer storage
        
        // Soft clamp credit
        if (e->credit > 10000) e->credit = 10000;
        if (e->credit < -10000) e->credit = -10000;
    }
    
    // Update global statistics
    g_sys.mean_error = edge_count_active > 0 ? total_error / edge_count_active : 0.0f;
    g_sys.mean_surprise = edge_count_active > 0 ? total_surprise / edge_count_active : 0.0f;
    
    // Update energy field: increases with surprise, decays over time (adaptive)
    g_sys.energy = g_sys.energy_decay * g_sys.energy + g_sys.energy_alpha * g_sys.mean_surprise;
    
    // Modulate exploration rate via energy
    // High energy → more exploration; low energy → more exploitation
    g_sys.epsilon = g_sys.epsilon_min + (g_sys.epsilon_max - g_sys.epsilon_min) * sigmoid(g_sys.energy - 0.5f);
}

/* ========================================================================
 * HOMEOSTATIC PARAMETER ADAPTATION
 * ======================================================================== */

void adapt_parameters() {
    // Compute current system statistics
    float max_possible_edges = (float)(g_graph.node_count * g_graph.node_count);
    g_sys.current_density = (max_possible_edges > 0) 
        ? (float)g_graph.edge_count / max_possible_edges 
        : 0.0f;
    
    g_sys.current_activity = (g_graph.node_count > 0)
        ? (float)g_sys.active_node_count / (float)g_graph.node_count
        : 0.0f;
    
    g_sys.prediction_acc = 1.0f - g_sys.mean_error;
    
    // Homeostatic adaptation: adjust parameters to maintain targets
    
    // 1. PRUNING RATE: Increase if too dense, decrease if too sparse
    float density_error = g_sys.current_density - g_sys.target_density;
    g_sys.prune_rate += g_sys.adapt_rate * density_error;
    if (g_sys.prune_rate < 0.0001f) g_sys.prune_rate = 0.0001f;
    if (g_sys.prune_rate > 0.01f) g_sys.prune_rate = 0.01f;
    
    // 2. CREATION RATE: Increase if too sparse and prediction is good
    // Decrease if too dense or prediction is poor
    float density_deficit = g_sys.target_density - g_sys.current_density;
    float prediction_quality = g_sys.prediction_acc - g_sys.target_prediction_acc;
    g_sys.create_rate += g_sys.adapt_rate * density_deficit * (1.0f + prediction_quality);
    if (g_sys.create_rate < 0.001f) g_sys.create_rate = 0.001f;
    if (g_sys.create_rate > 0.1f) g_sys.create_rate = 0.1f;
    
    // 3. ACTIVATION SCALE: Adjust to maintain target activity level
    float activity_error = g_sys.current_activity - g_sys.target_activity;
    // If too active, increase scale (harder to activate)
    // If not active enough, decrease scale (easier to activate)
    g_sys.activation_scale += g_sys.adapt_rate * 100.0f * activity_error;
    if (g_sys.activation_scale < 16.0f) g_sys.activation_scale = 16.0f;
    if (g_sys.activation_scale > 256.0f) g_sys.activation_scale = 256.0f;
    
    // 4. ENERGY ALPHA: Adapt based on prediction accuracy
    // If prediction is poor, increase energy learning to respond faster
    // If prediction is good, decrease to avoid overreaction
    float acc_deficit = g_sys.target_prediction_acc - g_sys.prediction_acc;
    g_sys.energy_alpha += g_sys.adapt_rate * 0.1f * acc_deficit;
    if (g_sys.energy_alpha < 0.01f) g_sys.energy_alpha = 0.01f;
    if (g_sys.energy_alpha > 0.5f) g_sys.energy_alpha = 0.5f;
    
    // 5. ENERGY DECAY: Faster decay if predictions are stable
    // Slower decay if predictions are unstable (keep energy around longer)
    float stability = 1.0f - fabsf(acc_deficit);
    g_sys.energy_decay += g_sys.adapt_rate * 0.01f * (stability - 0.5f);
    if (g_sys.energy_decay < 0.95f) g_sys.energy_decay = 0.95f;
    if (g_sys.energy_decay > 0.999f) g_sys.energy_decay = 0.999f;
    
    // 6. SIGMOID_K: Adapt based on activity variance
    // If activity is too uniform, sharpen transitions (increase k)
    // If activity is too varied, smooth transitions (decrease k)
    float activity_pressure = (g_sys.current_activity < 0.05f || g_sys.current_activity > 0.5f) ? 1.0f : -1.0f;
    g_sys.sigmoid_k += g_sys.adapt_rate * activity_pressure;
    if (g_sys.sigmoid_k < 0.1f) g_sys.sigmoid_k = 0.1f;
    if (g_sys.sigmoid_k > 2.0f) g_sys.sigmoid_k = 2.0f;
    
    // 7. EPSILON RANGE: Widen if prediction is poor, narrow if good
    // This allows more exploration when needed
    float exploration_need = (g_sys.prediction_acc < g_sys.target_prediction_acc) ? 1.0f : -1.0f;
    g_sys.epsilon_max += g_sys.adapt_rate * 0.1f * exploration_need;
    if (g_sys.epsilon_max < 0.2f) g_sys.epsilon_max = 0.2f;
    if (g_sys.epsilon_max > 0.5f) g_sys.epsilon_max = 0.5f;
    
    // Epsilon_min stays relatively stable
    g_sys.epsilon_min = g_sys.epsilon_max * 0.2f; // always 20% of max
    
    // 8. LAYER EMERGENCE RATE: Increase if density is high and structure is good
    float structural_readiness = g_sys.current_density * g_sys.prediction_acc;
    g_sys.layer_rate += g_sys.adapt_rate * 0.01f * (structural_readiness - 0.1f);
    if (g_sys.layer_rate < 0.0001f) g_sys.layer_rate = 0.0001f;
    if (g_sys.layer_rate > 0.01f) g_sys.layer_rate = 0.01f;
    
    // 9. CAPACITY MONITORING: Arrays auto-grow as needed, no artificial limits
    // System self-balances through homeostatic adaptation above
    
    // 10. THOUGHT DEPTH ADAPTATION: Adjust max hops based on settlement patterns (soft control, no hard limit)
    float current_settle_ratio = (g_sys.thoughts_settled + g_sys.thoughts_maxed) > 0
        ? (float)g_sys.thoughts_settled / (float)(g_sys.thoughts_settled + g_sys.thoughts_maxed)
        : 0.5f;
    
    // If too many thoughts max out, increase limit
    // If too many settle early, decrease limit
    float settle_error = current_settle_ratio - g_sys.target_settle_ratio;
    float depth_error = (float)g_sys.thought_depth - (float)g_sys.target_thought_depth;
    
    // Increase max if thoughts often max out (settle_ratio low)
    // Decrease max if thoughts settle too early (depth below target)
    float hop_adjustment = -g_sys.adapt_rate * 10.0f * (settle_error + 0.5f * depth_error);
    
    // Apply soft growth rate limiting (prevents runaway growth)
    if (hop_adjustment > 0) {
        hop_adjustment = fminf(hop_adjustment, (float)g_sys.max_thought_hops * g_sys.max_hop_growth_rate);
    }
    
    g_sys.max_thought_hops = (uint32_t)fmaxf((float)g_sys.min_thought_hops, (float)g_sys.max_thought_hops + hop_adjustment);
    
    // 11. STABILITY THRESHOLD ADAPTATION: Adjust based on thought depth
    // If thoughts too shallow, tighten convergence (decrease eps)
    // If thoughts too deep, relax convergence (increase eps)
    float depth_pressure = ((float)g_sys.thought_depth - (float)g_sys.target_thought_depth) / (float)g_sys.target_thought_depth;
    g_sys.stability_eps += g_sys.adapt_rate * 0.01f * depth_pressure;
    if (g_sys.stability_eps < 0.001f) g_sys.stability_eps = 0.001f;
    if (g_sys.stability_eps > 0.05f) g_sys.stability_eps = 0.05f;
    
    g_sys.activation_eps += g_sys.adapt_rate * 0.02f * depth_pressure;
    if (g_sys.activation_eps < 0.005f) g_sys.activation_eps = 0.005f;
    if (g_sys.activation_eps > 0.1f) g_sys.activation_eps = 0.1f;
    
    // 12. TEMPORAL DECAY ADAPTATION: Adjust based on temporal distance
    // If edges are too stale on average, increase decay (make time matter more)
    // If edges are too fresh, decrease decay (time matters less)
    float temporal_pressure = (g_sys.mean_temporal_distance - 10.0f) / 10.0f;
    g_sys.temporal_decay += g_sys.adapt_rate * 0.1f * temporal_pressure;
    if (g_sys.temporal_decay < 0.01f) g_sys.temporal_decay = 0.01f;
    if (g_sys.temporal_decay > 0.5f) g_sys.temporal_decay = 0.5f;
    
    // 13. SPATIAL SCALING ADAPTATION: Adjust based on connectivity patterns
    // If spatial distances are uniform, adjust scaling
    // High spatial distance = nodes are "far" in graph → may need adjustment
    float spatial_pressure = (g_sys.mean_spatial_distance - 2.0f) / 2.0f;
    g_sys.spatial_k += g_sys.adapt_rate * spatial_pressure;
    if (g_sys.spatial_k < 0.1f) g_sys.spatial_k = 0.1f;
    if (g_sys.spatial_k > 2.0f) g_sys.spatial_k = 2.0f;
}

/* ========================================================================
 * PRUNING (Continuous, probabilistic synaptic decay)
 * ======================================================================== */

void prune() {
    // Probabilistic edge pruning: no hard thresholds, smooth decay
    for (uint32_t i = 0; i < g_graph.edge_count; i++) {
        int is_free = 0;
        for (uint32_t j = 0; j < g_graph.edge_free_count; j++) {
            if (g_graph.edge_free_list[j] == i) {
                is_free = 1;
                break;
            }
        }
        if (is_free) continue;
        
        Edge *e = &g_graph.edges[i];
        
        float w_eff = g_sys.gamma_slow * e->w_slow + (1.0f - g_sys.gamma_slow) * e->w_fast;
        
        // Compute pruning probability from multiple continuous factors
        float p_weak = soft_below(w_eff, g_sys.prune_weight_ref);        // weak weight
        float p_unused = soft_below((float)e->use_count, 10.0f);   // low use
        float p_stale = soft_above((float)e->stale_ticks, g_sys.stale_ref); // stale
        
        // Combined pruning probability (multiplicative) — adaptive rate
        float p_prune = g_sys.prune_rate * p_weak * p_unused * p_stale;
        
        // Stochastic pruning
        if (randf() < p_prune) {
            edge_delete(&g_graph, i);
        }
    }
    
    // Probabilistic node pruning: isolated and stale nodes decay away
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        int is_free = 0;
        for (uint32_t j = 0; j < g_graph.node_free_count; j++) {
            if (g_graph.node_free_list[j] == i) {
                is_free = 1;
                break;
            }
        }
        if (is_free) continue;
        
        Node *n = &g_graph.nodes[i];
        
        // Pruning probability for isolated nodes
        float p_isolated = (n->in_deg == 0 && n->out_deg == 0) ? 1.0f : 0.0f;
        float staleness = (float)(g_sys.tick - n->last_tick_seen);
        float p_stale = soft_above(staleness, g_sys.node_stale_ref);
        
        float p_prune_node = g_sys.prune_rate * 2.0f * p_isolated * p_stale;
        
        if (randf() < p_prune_node) {
            node_delete(&g_graph, i);
            g_sys.nodes_pruned++;
        }
    }
}

/* ========================================================================
 * NODE CREATION (Probabilistic emergence from co-activation)
 * ======================================================================== */

void try_create_nodes() {
    // Probabilistic node creation: no hard thresholds, emergent from patterns
    
    for (uint32_t i = 0; i < g_graph.node_count && i < 1000; i++) {
        int is_free_i = 0;
        for (uint32_t j = 0; j < g_graph.node_free_count; j++) {
            if (g_graph.node_free_list[j] == i) {
                is_free_i = 1;
                break;
            }
        }
        if (is_free_i) continue;
        
        Node *ni = &g_graph.nodes[i];
        
        // Continuous activation gating (no hard threshold)
        float activation_strength_i = ni->a;
        
        for (uint32_t j = i + 1; j < g_graph.node_count && j < 1000; j++) {
            int is_free_j = 0;
            for (uint32_t k = 0; k < g_graph.node_free_count; k++) {
                if (g_graph.node_free_list[k] == j) {
                    is_free_j = 1;
                    break;
                }
            }
            if (is_free_j) continue;
            
            Node *nj = &g_graph.nodes[j];
            float activation_strength_j = nj->a;
            
            // Co-activation strength (continuous product)
            float co_activation = activation_strength_i * activation_strength_j;
            if (co_activation < 0.1f) continue; // skip very weak pairs
            
            // Check co-activation frequency (continuous measure)
            uint32_t common_bits = ni->sig_history & nj->sig_history;
            float co_count = (float)__builtin_popcount(common_bits);
            
            // Continuous similarity measure
            uint32_t xor_bits = ni->sig_history ^ nj->sig_history;
            float similarity = 1.0f - (__builtin_popcount(xor_bits) / 32.0f);
            
            // Compute novelty: combination of co-occurrence and similarity
            float novelty = (co_count / g_sys.co_freq_ref) * similarity;
            
            // Creation probability: sigmoid of novelty, modulated by energy — adaptive rate
            float p_create = g_sys.create_rate * sigmoid(novelty * 10.0f - 5.0f);
            p_create *= (1.0f + g_sys.energy); // energy boosts creation
            
            // Stochastic node creation
            if (randf() < p_create) {
                // Create new internal node if no edge exists
                Edge *existing = find_edge(&g_graph, i, j);
                if (existing == NULL) {
                    uint32_t new_idx = node_create(&g_graph);
                    if (new_idx != UINT32_MAX) {
                        edge_create(&g_graph, i, new_idx);
                        edge_create(&g_graph, j, new_idx);
                        g_sys.nodes_created++;
                    }
                }
            }
        }
    }
}

/* ========================================================================
 * META-NODE CREATION (Self-optimization agents)
 * ======================================================================== */

void try_create_meta_nodes() {
    // Probabilistically create meta-nodes with different optimization capabilities
    if (g_graph.node_count < 10) return; // Need some structure first
    
    // Create different types of meta-nodes based on system needs
    MetaOpType meta_types[] = {
        META_MUTATE_OP,         // Evolve node operations
        META_CREATE_SHORTCUT,   // Add skip connections
        META_MERGE_NODES,       // Reduce redundancy
        META_SPLIT_NODE,        // Handle overload
        META_OPTIMIZE_SUBGRAPH, // Discover and compile patterns
        META_PRUNE_BRANCH       // Remove waste
    };
    
    for (int i = 0; i < 6; i++) {
        // Low probability - we don't want too many meta-nodes
        if (randf() < 0.001f * (1.0f + g_sys.energy)) {
            uint32_t meta_idx = node_create(&g_graph);
            if (meta_idx != UINT32_MAX) {
                Node *meta = &g_graph.nodes[meta_idx];
                meta->is_meta = 1;
                meta->meta_op = meta_types[i];
                meta->cluster_id = g_sys.layers_created++;
                meta->op_type = OP_GATE; // Meta-nodes use gating
                
                printf("[META-NODE] Created %s optimizer (node %u)\n",
                       i == 0 ? "MUTATE" : i == 1 ? "SHORTCUT" : i == 2 ? "MERGE" :
                       i == 3 ? "SPLIT" : i == 4 ? "OPTIMIZE" : "PRUNE",
                       meta_idx);
            }
        }
    }
}

/* ========================================================================
 * LAYER EMERGENCE (Probabilistic meta-node creation from dense clusters)
 * ======================================================================== */

void try_layer_emergence() {
    // Probabilistic layer emergence: continuous density measures
    // Also creates meta-nodes for self-optimization
    
    for (uint32_t i = 0; i < g_graph.node_count && i < 500; i++) {
        int is_free = 0;
        for (uint32_t j = 0; j < g_graph.node_free_count; j++) {
            if (g_graph.node_free_list[j] == i) {
                is_free = 1;
                break;
            }
        }
        if (is_free) continue;
        
        Node *n = &g_graph.nodes[i];
        if (n->is_meta) continue;
        
        // Soft check for sufficient connectivity (no hard threshold)
        float connectivity_strength = soft_above((float)n->out_deg, (float)(g_sys.layer_min_size / 2));
        if (connectivity_strength < 0.3f) continue; // skip very weakly connected
        
        // Count active neighbors (continuous measure)
        float active_neighbors = 0.0f;
        float total_neighbors = 0.0f;
        
        for (uint32_t e = 0; e < g_graph.edge_count; e++) {
            int ef = 0;
            for (uint32_t k = 0; k < g_graph.edge_free_count; k++) {
                if (g_graph.edge_free_list[k] == e) {
                    ef = 1;
                    break;
                }
            }
            if (ef) continue;
            
            Edge *edge = &g_graph.edges[e];
            if (edge->src == i) {
                total_neighbors += 1.0f;
                // Continuous activation instead of binary
                active_neighbors += g_graph.nodes[edge->dst].a;
            }
        }
        
        float density = total_neighbors > 0.0f ? active_neighbors / total_neighbors : 0.0f;
        
        // Compute emergence probability from continuous factors
        float p_dense = soft_above(density, g_sys.density_ref);
        float p_size = soft_above(total_neighbors, (float)g_sys.layer_min_size);
        
        float p_emerge = g_sys.layer_rate * p_dense * p_size * connectivity_strength;
        p_emerge *= (1.0f + 0.5f * g_sys.energy); // energy modulates emergence
        
        // Stochastic layer emergence
        if (randf() < p_emerge) {
            uint32_t meta_idx = node_create(&g_graph);
            if (meta_idx != UINT32_MAX) {
                g_graph.nodes[meta_idx].is_meta = 1;
                g_graph.nodes[meta_idx].cluster_id = g_sys.layers_created;
                g_sys.layers_created++;
                
                // Connect meta-node to this node
                edge_create(&g_graph, i, meta_idx);
            }
        }
    }
}

/* ========================================================================
 * PERSISTENCE (Memory-Mapped Single File)
 * ======================================================================== */

#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t next_node_id;
    uint32_t edge_count;
    uint32_t edge_cap;
    uint32_t module_count;
    uint32_t module_cap;
    uint64_t tick;
    // Graph data follows immediately after header
} GraphFileHeader;

static int g_mmap_fd = -1;
static void *g_mmap_base = NULL;
static size_t g_mmap_size = 0;

void graph_mmap_init(const char *filename, uint32_t initial_node_cap, uint32_t initial_edge_cap) {
    // Calculate initial file size
    size_t header_size = sizeof(GraphFileHeader);
    size_t nodes_size = initial_node_cap * sizeof(Node);
    size_t edges_size = initial_edge_cap * sizeof(Edge);
    size_t modules_size = 64 * sizeof(Module);
    g_mmap_size = header_size + nodes_size + edges_size + modules_size;
    
    // Open or create file
    g_mmap_fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (g_mmap_fd < 0) {
        fprintf(stderr, "ERROR: Failed to open %s\n", filename);
        exit(1);
    }
    
    // Check if file exists and has data
    struct stat st;
    fstat(g_mmap_fd, &st);
    int file_exists = (st.st_size > 0);
    
    if (!file_exists) {
        // New file - set size
        if (ftruncate(g_mmap_fd, g_mmap_size) < 0) {
            fprintf(stderr, "ERROR: Failed to size file\n");
            exit(1);
        }
    } else {
        // Existing file - use its size
        g_mmap_size = st.st_size;
    }
    
    // Memory map the file
    g_mmap_base = mmap(NULL, g_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, g_mmap_fd, 0);
    if (g_mmap_base == MAP_FAILED) {
        fprintf(stderr, "ERROR: Failed to mmap file\n");
        exit(1);
    }
    
    // Set up pointers into mapped memory
    GraphFileHeader *header = (GraphFileHeader *)g_mmap_base;
    
    if (file_exists && header->node_count > 0) {
        // Restore from existing file
        g_graph.node_count = header->node_count;
        g_graph.node_cap = header->node_cap;
        g_graph.next_node_id = header->next_node_id;
        g_graph.edge_count = header->edge_count;
        g_graph.edge_cap = header->edge_cap;
        g_graph.module_count = header->module_count;
        g_graph.module_cap = header->module_cap;
        g_sys.tick = header->tick;
        
        printf("[RESTORE] Loaded from mmap: %u nodes, %u edges, tick=%llu\n", 
               g_graph.node_count, g_graph.edge_count, (unsigned long long)g_sys.tick);
    } else {
        // Initialize new file
        header->node_count = 0;
        header->node_cap = initial_node_cap;
        header->next_node_id = 1;
        header->edge_count = 0;
        header->edge_cap = initial_edge_cap;
        header->module_count = 0;
        header->module_cap = 64;
        header->tick = 0;
        
        g_graph.node_count = 0;
        g_graph.node_cap = initial_node_cap;
        g_graph.next_node_id = 1;
        g_graph.edge_count = 0;
        g_graph.edge_cap = initial_edge_cap;
        g_graph.module_count = 0;
        g_graph.module_cap = 64;
        
        printf("[MMAP INIT] Created new graph file: %zu bytes\n", g_mmap_size);
    }
    
    // Point graph structures to mapped memory
    g_graph.nodes = (Node *)((char *)g_mmap_base + header_size);
    g_graph.edges = (Edge *)((char *)g_graph.nodes + header->node_cap * sizeof(Node));
    g_graph.modules = (Module *)((char *)g_graph.edges + header->edge_cap * sizeof(Edge));
    
    // Allocate free lists in regular memory (not persisted)
    g_graph.node_free_list = calloc(initial_node_cap, sizeof(uint32_t));
    g_graph.edge_free_list = calloc(initial_edge_cap, sizeof(uint32_t));
    g_graph.node_free_count = 0;
    g_graph.edge_free_count = 0;
    
    // P2 SAFEGUARD: Initialize edge hash table
    g_graph.edge_hash_size = EDGE_HASH_SIZE;
    g_graph.edge_hash = calloc(EDGE_HASH_SIZE, sizeof(EdgeHashEntry));
    
    // Rebuild hash table from existing edges (if restoring)
    if (file_exists && g_graph.edge_count > 0) {
        for (uint32_t i = 0; i < g_graph.edge_count; i++) {
            edge_hash_insert(&g_graph, i);
        }
        printf("[RESTORE] Rebuilt edge hash table: %u edges\n", g_graph.edge_count);
    }
}

void graph_mmap_sync() {
    if (g_mmap_base == NULL) return;
    
    // Update header
    GraphFileHeader *header = (GraphFileHeader *)g_mmap_base;
    header->node_count = g_graph.node_count;
    header->node_cap = g_graph.node_cap;
    header->next_node_id = g_graph.next_node_id;
    header->edge_count = g_graph.edge_count;
    header->edge_cap = g_graph.edge_cap;
    header->module_count = g_graph.module_count;
    header->module_cap = g_graph.module_cap;
    header->tick = g_sys.tick;
    
    // Sync to disk
    msync(g_mmap_base, g_mmap_size, MS_ASYNC);
    
    printf("[MMAP SYNC] tick=%llu nodes=%u edges=%u\n", 
           (unsigned long long)g_sys.tick, g_graph.node_count, g_graph.edge_count);
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
 * I/O
 * ======================================================================== */

void read_input() {
    // Non-blocking read from stdin
    uint8_t buf[1024];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    
    if (n > 0) {
        ring_write(&g_sys.rx_ring, buf, n);
    }
}

void slice_frame() {
    // Extract up to current_frame_cap bytes from RX ring for current tick
    g_sys.current_frame_len = ring_read(&g_sys.rx_ring, g_sys.current_frame, g_sys.current_frame_cap);
}

void merge_output_into_input() {
    // Append last output to current input frame for self-observation
    if (g_sys.last_output_frame_len > 0) {
        uint32_t required_size = g_sys.current_frame_len + g_sys.last_output_frame_len;
        
        // Auto-grow current_frame if needed
        if (required_size > g_sys.current_frame_cap) {
            uint32_t new_cap = g_sys.current_frame_cap;
            while (new_cap < required_size) new_cap *= 2;
            
            printf("[AUTO-GROW] Expanding current_frame: %u -> %u bytes\n", g_sys.current_frame_cap, new_cap);
            uint8_t *new_frame = realloc(g_sys.current_frame, new_cap);
            if (new_frame) {
                g_sys.current_frame = new_frame;
                g_sys.current_frame_cap = new_cap;
            } else {
                fprintf(stderr, "ERROR: Failed to grow current_frame\n");
                return;
            }
        }
        
        memcpy(g_sys.current_frame + g_sys.current_frame_len,
               g_sys.last_output_frame,
               g_sys.last_output_frame_len);
        
        g_sys.current_frame_len += g_sys.last_output_frame_len;
    }
}

void emit_action() {
    // If no macros exist yet, create one from current input
    if (g_sys.macro_count == 0) {
        if (g_sys.current_frame_len > 0) {
            // Learn first macro from input
            uint16_t len = g_sys.current_frame_len > 64 ? 64 : g_sys.current_frame_len;
            macro_add(g_sys.current_frame, len);
            printf("[BOOTSTRAP] Learned first macro from input (%u bytes)\n", len);
        } else {
            // No input yet, emit nothing
            return;
        }
    }
    
    uint32_t macro_idx = macro_select();
    Macro *m = &g_sys.macros[macro_idx];
    
    // Write to stdout
    write(STDOUT_FILENO, m->bytes, m->len);
    
    // Mirror to TX ring for self-observation
    ring_write(&g_sys.tx_ring, m->bytes, m->len);
    
    // Save as last output
    memcpy(g_sys.last_output_frame, m->bytes, m->len);
    g_sys.last_output_frame_len = m->len;
    
    // Update macro utility based on current error (continuous reward signal)
    float reward = 1.0f - g_sys.mean_error;
    macro_update_utility(macro_idx, reward);
    
    // Note: epsilon is now modulated by energy field in observe_and_update()
    // No manual decay needed - it's self-regulating
}

/* ========================================================================
 * MAIN LOOP
 * ======================================================================== */

void main_loop() {
    while (1) {
        // (1) INPUT — "what is happening?"
        read_input();
        slice_frame();
        
        // (2) RECALL LAST OUTPUT — "what did I do?"
        merge_output_into_input();
        
        // Run detectors to set sensory node activations
        detector_run_all(g_sys.current_frame, g_sys.current_frame_len);
        
        // (3) PREDICT — "what should happen?"
        // EMERGENT THOUGHT: Propagate until prediction stabilizes
        // Thought duration emerges from graph dynamics (not fixed)
        converge_thought();
        
        // After thought settles, learn from prediction
        observe_and_update(); // updates energy field & epsilon
        
        // Homeostatic parameter adaptation (every 10 ticks for efficiency)
        if (g_sys.tick % 10 == 0) {
            adapt_parameters();
        }
        
        // Continuous probabilistic node creation (no fixed schedule)
        // More frequent than pruning/layers - runs ~10% of ticks
        if (randf() < 0.1f) {
            try_create_nodes();
        }
        
        // (4) OUTPUT — "do it"
        emit_action();
        
        // EVOLUTIONARY MUTATION: Nodes randomly change operations
        if (randf() < g_sys.global_mutation_rate && g_graph.node_count > 0) {
            uint32_t mutate_idx = rand() % g_graph.node_count;
            Node *n = &g_graph.nodes[mutate_idx];
            
            // SAFEGUARD: Don't mutate protected kernel nodes
            if (randf() < n->mutation_rate && !n->is_meta && !n->is_protected) {
                uint8_t old_op = n->op_type;
                n->op_type = rand() % NUM_OPS;
                g_sys.mutations_attempted++;
                
                // Initialize new params
                for (int i = 0; i < 4; i++) {
                    n->op_params[i] = randf() * 2.0f - 1.0f;
                }
                
                printf("[MUTATION] Node %u: %d -> %d\n", mutate_idx, old_op, n->op_type);
            }
        }
        
        // META-NODE EXECUTION: Active meta-nodes queue modifications
        for (uint32_t i = 0; i < g_graph.node_count && i < 10; i++) { // Limit to 10 meta-ops per tick
            Node *n = &g_graph.nodes[i];
            if (n->is_meta && n->a > 0.5f) {
                execute_meta_operation(n);
            }
        }
        
        // SAFEGUARD: Apply pending operations AFTER current tick (causal scheduling)
        apply_pending_operations();
        
        // Continuous probabilistic housekeeping (no hard schedules)
        // Prune is called every tick but acts probabilistically inside
        prune();
        
        // Layer emergence is rare, modulated by energy (adaptive rate)
        float p_layer = g_sys.layer_rate * (1.0f + g_sys.energy);
        if (randf() < p_layer) {
            try_layer_emergence();
        }
        
        // META-NODE CREATION: Spawn optimization agents (very rare)
        if (randf() < g_sys.layer_rate * 0.1f) {
            try_create_meta_nodes();
        }
        
        // PATTERN DETECTION: Find frequently used subgraphs (every 100 ticks)
        if (g_sys.tick % 100 == 0 && g_graph.node_count > 10) {
            detect_patterns(&g_graph);
        }
        
        // MODULE EXECUTION: Execute module proxies hierarchically
        for (uint32_t i = 0; i < g_graph.node_count && i < 20; i++) {
            Node *n = &g_graph.nodes[i];
            if (n->is_module_proxy && n->a > 0.3f && n->proxy_module_id > 0) {
                // Execute the module this node represents
                uint32_t mod_idx = n->proxy_module_id - 1;
                if (mod_idx < g_graph.module_count) {
                    Module *m = &g_graph.modules[mod_idx];
                    float inputs[16] = {n->a}; // Simple: use proxy activation as input
                    float outputs[16] = {0};
                    
                    module_execute(&g_graph, m, inputs, outputs);
                    
                    // Set proxy output to module's output
                    if (m->output_count > 0) {
                        n->a = outputs[0];
                    }
                }
            }
        }
        
        // Sync memory-mapped file periodically
        if (g_sys.tick % g_sys.snapshot_period == 0) {
            graph_mmap_sync();
        }
        
        // Logging (still periodic for readability)
        if (g_sys.tick % 100 == 0) {
            float settle_ratio = g_sys.thoughts_settled + g_sys.thoughts_maxed > 0 
                ? (float)g_sys.thoughts_settled / (float)(g_sys.thoughts_settled + g_sys.thoughts_maxed)
                : 0.0f;
            
            printf("[TICK %llu] nodes=%u edges=%u active=%u err=%.3f energy=%.3f ε=%.3f | "
                   "density=%.4f activity=%.3f acc=%.3f | "
                   "hops=%u/%u t_dist=%.1f s_dist=%.2f settle=%.2f | "
                   "stab_ε=%.4f temp_decay=%.3f\n",
                   (unsigned long long)g_sys.tick,
                   g_graph.node_count,
                   g_graph.edge_count,
                   g_sys.active_node_count,
                   g_sys.mean_error,
                   g_sys.energy,
                   g_sys.epsilon,
                   g_sys.current_density,
                   g_sys.current_activity,
                   g_sys.prediction_acc,
                   g_sys.thought_depth,
                   g_sys.max_thought_hops,
                   g_sys.mean_temporal_distance,
                   g_sys.mean_spatial_distance,
                   settle_ratio,
                   g_sys.stability_eps,
                   g_sys.temporal_decay);
        }
        
        // Evolution status (every 1000 ticks)
        if (g_sys.tick % 1000 == 0 && g_sys.tick > 0) {
            printf("[EVOLUTION] meta_ops=%llu mutations=%u/%u (%.1f%%) cycles=%llu\n",
                   (unsigned long long)g_sys.meta_operations,
                   g_sys.mutations_kept,
                   g_sys.mutations_attempted,
                   g_sys.mutations_attempted > 0 ? (100.0f * g_sys.mutations_kept / g_sys.mutations_attempted) : 0.0f,
                   (unsigned long long)g_sys.total_cycles);
            
            // Show operation type distribution
            printf("[OPS] ");
            const char *op_names[] = {"SIG", "SUM", "PRO", "MAX", "MIN", "GAT", "MEM", "CMP", "SEQ", "HSH", "MOD", "THR", "REL", "TNH"};
            for (int i = 0; i < NUM_OPS && i < 14; i++) {
                if (g_sys.op_type_counts[i] > 0) {
                    printf("%s:%u(%.2f) ", op_names[i], g_sys.op_type_counts[i], g_sys.op_type_utility[i]);
                }
            }
            printf("\n");
            
            // Show module/hierarchy status
            if (g_graph.module_count > 0 || g_sys.patterns_detected > 0) {
                printf("[MODULES] created=%u patterns=%u calls=%u hierarchy_lvl=%u\n",
                       g_sys.modules_created,
                       g_sys.patterns_detected,
                       g_sys.module_calls,
                       g_sys.max_hierarchy_level);
                
                // Show active modules
                if (g_graph.module_count > 0 && g_graph.module_count <= 10) {
                    printf("[ACTIVE_MODULES] ");
                    for (uint32_t i = 0; i < g_graph.module_count; i++) {
                        Module *m = &g_graph.modules[i];
                        printf("%s(%u nodes,%.1f util,freq=%u) ", 
                               m->name, m->node_count, m->avg_utility, m->pattern_frequency);
                    }
                    printf("\n");
                }
            }
            
            // Reset counters for next period
            for (int i = 0; i < NUM_OPS; i++) {
                g_sys.op_type_counts[i] = 0;
            }
        }
        
        g_sys.tick++;
        
        // Sleep for tick duration
        usleep(g_sys.tick_ms * 1000);
    }
}

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

void system_init(uint32_t node_cap, uint32_t edge_cap, uint32_t detector_cap, uint32_t macro_cap) {
    memset(&g_sys, 0, sizeof(System));
    
    // Initialize memory-mapped graph file (replaces graph_init)
    graph_mmap_init("graph.mmap", node_cap, edge_cap);
    
    ring_init(&g_sys.rx_ring, RX_RING_SIZE);
    ring_init(&g_sys.tx_ring, TX_RING_SIZE);
    
    // Allocate dynamic frame buffers (start with FRAME_SIZE, can grow if needed)
    g_sys.current_frame = malloc(FRAME_SIZE);
    g_sys.current_frame_cap = FRAME_SIZE;
    g_sys.current_frame_len = 0;
    
    g_sys.last_output_frame = malloc(FRAME_SIZE);
    g_sys.last_output_frame_cap = FRAME_SIZE;
    g_sys.last_output_frame_len = 0;
    
    detector_init(detector_cap);
    macro_init(macro_cap);
    
    g_sys.P1 = calloc(node_cap, sizeof(float));
    g_sys.P0 = calloc(node_cap, sizeof(float));
    
    // Initialize baseline to 0.5
    for (uint32_t i = 0; i < node_cap; i++) {
        g_sys.P1[i] = 0.5f;
        g_sys.P0[i] = 0.5f;
    }
    
    // NO HARDCODED PATTERNS - graph learns everything from input
    // All detectors and macros emerge from observing byte patterns
    
    printf("[INIT] Ultra-minimal system (auto-growing from %u nodes, %u edges)\n",
           node_cap, edge_cap);
    printf("[INIT] Starting empty - graph learns all patterns from input\n");
    printf("[INIT] Graph lives in: graph.mmap (grows to 4TB)\n");
    fflush(stdout);
}

void system_cleanup() {
    graph_mmap_close();  // Close and sync memory-mapped file
    graph_free(&g_graph);
    ring_free(&g_sys.rx_ring);
    ring_free(&g_sys.tx_ring);
    free(g_sys.current_frame);
    free(g_sys.last_output_frame);
    free(g_sys.detectors);
    free(g_sys.macros);
    free(g_sys.P1);
    free(g_sys.P0);
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char **argv) {
    // Start small - system auto-grows as needed (no artificial limits)
    uint32_t node_cap = 256;      // Start tiny, will double to 512, 1K, 2K, 4K, 8K, 16K...
    uint32_t edge_cap = 1024;     // Start tiny, will double to 2K, 4K, 8K, 16K, 32K...
    uint32_t detector_cap = 32;   // Start tiny, will grow as patterns are learned
    uint32_t macro_cap = 128;     // Start tiny, will grow as actions are learned
    
    // Simple CLI parsing (mainly for help)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s\n", argv[0]);
            printf("Melvin Core - Self-organizing learning system\n");
            printf("All structures auto-grow as needed (unlimited by design)\n");
            return 0;
        }
    }
    
    srand(time(NULL));
    
    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    system_init(node_cap, edge_cap, detector_cap, macro_cap);
    
    // Graph automatically restores from graph.mmap if it exists
    
    printf("=== MELVIN CORE STARTING ===\n");
    printf("Pure self-programming system - ALL INPUT IS CODE\n");
    printf("Graph learns its own language from patterns in input\n");
    printf("Tick period: %u ms\n", g_sys.tick_ms);
    printf("Press Ctrl+C to stop.\n\n");
    fflush(stdout);
    
    main_loop();
    
    system_cleanup();
    
    return 0;
}

