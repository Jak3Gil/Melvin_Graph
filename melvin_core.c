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
 * COMPILE-TIME PARAMETERS (can be overridden via CLI)
 * ======================================================================== */

#define DEFAULT_NODE_CAP        8192
#define DEFAULT_EDGE_CAP        65536
#define DEFAULT_MACRO_CAP       512
#define DEFAULT_DETECTOR_CAP    128

#define TICK_MS                 50
#define FRAME_SIZE              4096
#define RX_RING_SIZE            (FRAME_SIZE * 4)
#define TX_RING_SIZE            (FRAME_SIZE * 4)

#define SNAPSHOT_PERIOD         2000

/* Continuous dynamics parameters */
#define LAMBDA_DECAY            0.99f   // count decay
#define LAMBDA_E                0.9f    // eligibility trace
#define BETA_BLEND              0.7f    // predictive vs error
#define GAMMA_SLOW              0.8f    // slow weight fraction
#define ETA_FAST                3.0f    // fast weight step
#define DELTA_MAX               4.0f    // max weight change per tick
#define ALPHA_FAST_DECAY        0.95f
#define ALPHA_SLOW_DECAY        0.999f

/* Meta-parameters for homeostatic adaptation (these are more stable) */
#define ADAPT_RATE              0.001f   // rate of parameter adaptation
#define TARGET_DENSITY          0.15f    // target ratio of edges to max possible
#define TARGET_ACTIVITY         0.1f     // target fraction of active nodes
#define TARGET_PREDICTION_ACC   0.85f    // target prediction accuracy (1 - error)
#define CAPACITY_THRESH         0.8f     // threshold for "near capacity"

/* Emergent time, space, and thought parameters (initial values - will adapt) */
#define INIT_MAX_THOUGHT_HOPS   10       // initial max propagation passes per tick
#define INIT_STABILITY_EPS      0.005f   // initial convergence threshold for error
#define INIT_ACTIVATION_EPS     0.01f    // initial convergence threshold for activation
#define INIT_TEMPORAL_DECAY     0.1f     // initial temporal decay rate
#define INIT_SPATIAL_K          0.5f     // initial spatial connectivity scaling

/* Target metrics for thought/time/space adaptation */
#define TARGET_THOUGHT_DEPTH    5        // ideal number of hops (not too shallow/deep)
#define TARGET_SETTLE_RATIO     0.7f     // target ratio of settled thoughts (70%)
#define MIN_THOUGHT_HOPS        3        // minimum depth for meaningful thought
#define MAX_THOUGHT_HOPS_LIMIT  20       // absolute maximum to prevent runaway

/* Initial values for adaptive parameters (will self-tune) */
#define INIT_SIGMOID_K          0.5f
#define INIT_PRUNE_RATE         0.0005f
#define INIT_CREATE_RATE        0.01f
#define INIT_LAYER_RATE         0.001f
#define INIT_ENERGY_ALPHA       0.1f
#define INIT_ENERGY_DECAY       0.995f
#define INIT_EPSILON_MIN        0.05f
#define INIT_EPSILON_MAX        0.3f
#define INIT_ACTIVATION_SCALE   64.0f

/* Soft reference values (replace hard thresholds) */
#define PRUNE_WEIGHT_REF        2.0f
#define STALE_REF               200.0f
#define NODE_STALE_REF          1000.0f
#define CO_FREQ_REF             10.0f
#define SIM_REF                 0.6f
#define DENSITY_REF             0.6f
#define LAYER_MIN_SIZE          10

/* ========================================================================
 * DATA STRUCTURES
 * ======================================================================== */

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
    
    // For layer meta-nodes
    uint8_t  is_meta;
    uint32_t cluster_id;
    
    // Prediction support
    float    soma;        // accumulated input (continuous)
    float    hat;         // predicted activation [0,1] (continuous)
    
    // Statistics for pruning
    float    total_active_ticks; // continuous accumulation
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
    
    // Adjacency for fast lookup (could be improved with hash)
    // For simplicity, we'll do linear search in this minimal version
} Graph;

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
    
    Macro    *macros;
    uint32_t  macro_count;
    uint32_t  macro_cap;
    
    RingBuffer rx_ring;
    RingBuffer tx_ring;
    
    uint8_t  current_frame[FRAME_SIZE];
    uint16_t current_frame_len;
    
    uint8_t  last_output_frame[FRAME_SIZE];
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
    uint16_t max_thought_hops; // adaptive max propagation passes
    float    stability_eps;    // adaptive convergence threshold (error)
    float    activation_eps;   // adaptive convergence threshold (activation)
    float    temporal_decay;   // adaptive temporal distance scaling
    float    spatial_k;        // adaptive spatial connectivity scaling
    
    // Homeostatic targets & measurements
    float    current_density;  // current edge density
    float    current_activity; // current node activity rate
    float    prediction_acc;   // current prediction accuracy
    
    // Emergent time, space, and thought tracking
    uint16_t thought_depth;    // number of propagation hops in current tick
    float    prev_mean_error;  // for convergence detection
    float    activation_delta; // total activation change in last hop
    float    mean_temporal_distance;  // average edge staleness
    float    mean_spatial_distance;   // average connectivity-based distance
    uint32_t thoughts_settled; // count of converged thoughts
    uint32_t thoughts_maxed;   // count of max-hop thoughts (didn't converge)
} System;

/* ========================================================================
 * GLOBALS
 * ======================================================================== */

Graph   g_graph;
System  g_sys;

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

// Soft clamp: tanh-like, maps (-∞,∞) → (-1,1)
static inline float soft_clamp(float x) {
    return tanhf(x);
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
 * GRAPH MANAGEMENT
 * ======================================================================== */

void graph_init(Graph *g, uint32_t node_cap, uint32_t edge_cap) {
    g->nodes = calloc(node_cap, sizeof(Node));
    g->node_cap = node_cap;
    g->node_count = 0;
    g->node_free_list = calloc(node_cap, sizeof(uint32_t));
    g->node_free_count = 0;
    g->next_node_id = 1;
    
    g->edges = calloc(edge_cap, sizeof(Edge));
    g->edge_cap = edge_cap;
    g->edge_count = 0;
    g->edge_free_list = calloc(edge_cap, sizeof(uint32_t));
    g->edge_free_count = 0;
}

void graph_free(Graph *g) {
    free(g->nodes);
    free(g->node_free_list);
    free(g->edges);
    free(g->edge_free_list);
}

uint32_t node_create(Graph *g) {
    uint32_t idx;
    if (g->node_free_count > 0) {
        idx = g->node_free_list[--g->node_free_count];
    } else {
        if (g->node_count >= g->node_cap) {
            fprintf(stderr, "ERROR: Node capacity exceeded\n");
            return UINT32_MAX;
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
    
    return idx;
}

void node_delete(Graph *g, uint32_t idx) {
    if (idx >= g->node_count) return;
    g->node_free_list[g->node_free_count++] = idx;
}

uint32_t edge_create(Graph *g, uint32_t src_idx, uint32_t dst_idx) {
    uint32_t idx;
    if (g->edge_free_count > 0) {
        idx = g->edge_free_list[--g->edge_free_count];
    } else {
        if (g->edge_count >= g->edge_cap) {
            fprintf(stderr, "ERROR: Edge capacity exceeded\n");
            return UINT32_MAX;
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
    
    return idx;
}

void edge_delete(Graph *g, uint32_t idx) {
    if (idx >= g->edge_count) return;
    Edge *e = &g->edges[idx];
    
    if (e->src < g->node_count) g->nodes[e->src].out_deg--;
    if (e->dst < g->node_count) g->nodes[e->dst].in_deg--;
    
    g->edge_free_list[g->edge_free_count++] = idx;
    g_sys.edges_pruned++;
}

Edge* find_edge(Graph *g, uint32_t src_idx, uint32_t dst_idx) {
    for (uint32_t i = 0; i < g->edge_count; i++) {
        if (g->edge_free_count > 0) {
            // Check if this index is in free list
            int is_free = 0;
            for (uint32_t j = 0; j < g->edge_free_count; j++) {
                if (g->edge_free_list[j] == i) {
                    is_free = 1;
                    break;
                }
            }
            if (is_free) continue;
        }
        
        Edge *e = &g->edges[i];
        if (e->src == src_idx && e->dst == dst_idx) {
            return e;
        }
    }
    return NULL;
}

/* ========================================================================
 * DETECTORS (Byte patterns → Node activations)
 * ======================================================================== */

void detector_init(uint32_t cap) {
    g_sys.detectors = calloc(cap, sizeof(Detector));
    g_sys.detector_cap = cap;
    g_sys.detector_count = 0;
}

uint32_t detector_add(const char *pattern, uint8_t type) {
    if (g_sys.detector_count >= g_sys.detector_cap) return UINT32_MAX;
    
    Detector *d = &g_sys.detectors[g_sys.detector_count++];
    strncpy(d->pattern, pattern, 63);
    d->pattern[63] = 0;
    d->len = strlen(d->pattern);
    d->type = type;
    d->node_id = node_create(&g_graph);
    
    return d->node_id;
}

void detector_run_all(const uint8_t *frame, uint16_t frame_len) {
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
    
    // Initialize adaptive parameters
    g_sys.sigmoid_k = INIT_SIGMOID_K;
    g_sys.prune_rate = INIT_PRUNE_RATE;
    g_sys.create_rate = INIT_CREATE_RATE;
    g_sys.layer_rate = INIT_LAYER_RATE;
    g_sys.energy_alpha = INIT_ENERGY_ALPHA;
    g_sys.energy_decay = INIT_ENERGY_DECAY;
    g_sys.epsilon_min = INIT_EPSILON_MIN;
    g_sys.epsilon_max = INIT_EPSILON_MAX;
    g_sys.activation_scale = INIT_ACTIVATION_SCALE;
    
    g_sys.epsilon = 0.5f * (g_sys.epsilon_min + g_sys.epsilon_max); // start in middle
    g_sys.energy = 0.0f; // start with zero energy
    
    // Initialize homeostatic measurements
    g_sys.current_density = 0.0f;
    g_sys.current_activity = 0.0f;
    g_sys.prediction_acc = 0.5f; // neutral start
    
    // Initialize adaptive emergent parameters
    g_sys.max_thought_hops = INIT_MAX_THOUGHT_HOPS;
    g_sys.stability_eps = INIT_STABILITY_EPS;
    g_sys.activation_eps = INIT_ACTIVATION_EPS;
    g_sys.temporal_decay = INIT_TEMPORAL_DECAY;
    g_sys.spatial_k = INIT_SPATIAL_K;
    
    // Initialize emergent time/space/thought tracking
    g_sys.thought_depth = 0;
    g_sys.prev_mean_error = 0.0f;
    g_sys.activation_delta = 0.0f;
    g_sys.mean_temporal_distance = 0.0f;
    g_sys.mean_spatial_distance = 0.0f;
    g_sys.thoughts_settled = 0;
    g_sys.thoughts_maxed = 0;
}

uint32_t macro_add(const uint8_t *bytes, uint16_t len) {
    if (g_sys.macro_count >= g_sys.macro_cap) return UINT32_MAX;
    
    Macro *m = &g_sys.macros[g_sys.macro_count++];
    memcpy(m->bytes, bytes, len);
    m->len = len;
    m->U_fast = 0.5f;
    m->U_slow = 0.5f;
    m->use_count = 0;
    m->last_used_tick = 0;
    
    return g_sys.macro_count - 1;
}

void macro_add_defaults() {
    // Safe alphabet
    for (char c = 'a'; c <= 'z'; c++) {
        uint8_t b = (uint8_t)c;
        macro_add(&b, 1);
    }
    for (char c = 'A'; c <= 'Z'; c++) {
        uint8_t b = (uint8_t)c;
        macro_add(&b, 1);
    }
    for (char c = '0'; c <= '9'; c++) {
        uint8_t b = (uint8_t)c;
        macro_add(&b, 1);
    }
    
    // Special chars
    uint8_t newline = '\n';
    macro_add(&newline, 1);
    uint8_t space = ' ';
    macro_add(&space, 1);
    uint8_t dot = '.';
    macro_add(&dot, 1);
    uint8_t slash = '/';
    macro_add(&slash, 1);
}

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
            float u = GAMMA_SLOW * m->U_slow + (1.0f - GAMMA_SLOW) * m->U_fast;
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
    m->U_fast = ALPHA_FAST_DECAY * m->U_fast + (1.0f - ALPHA_FAST_DECAY) * reward;
    
    // Slow track (less aggressive)
    m->U_slow = ALPHA_SLOW_DECAY * m->U_slow + (1.0f - ALPHA_SLOW_DECAY) * reward;
    
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
        float w_eff = GAMMA_SLOW * e->w_slow + (1.0f - GAMMA_SLOW) * e->w_fast;
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
    
    // Compute predictions (continuous sigmoid activation)
    g_sys.active_node_count = 0;
    g_sys.activation_delta = 0.0f;
    
    for (uint32_t i = 0; i < g_graph.node_count; i++) {
        Node *n = &g_graph.nodes[i];
        
        // Save previous activation for delta computation
        float prev_a = n->a;
        
        // Continuous activation: sigmoid((soma - theta) / scale) — adaptive scale
        float x = (n->soma - n->theta) / g_sys.activation_scale;
        n->hat = sigmoid(x);
        
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
    }
    
    // Normalize activation delta
    if (g_graph.node_count > 0) {
        g_sys.activation_delta /= (float)g_graph.node_count;
    }
}

/* ========================================================================
 * THOUGHT CONVERGENCE (Multi-hop propagation until stable)
 * ======================================================================== */

uint16_t converge_thought() {
    // Run multiple propagation passes until prediction stabilizes (adaptive)
    // This mimics cortical gamma oscillations: perception → reverberation → settlement
    
    g_sys.prev_mean_error = g_sys.mean_error;
    g_sys.thought_depth = 0;
    
    for (uint16_t hop = 0; hop < g_sys.max_thought_hops; hop++) {
        g_sys.thought_depth = hop + 1;
        
        // One propagation pass
        propagate();
        
        // Check convergence: has prediction stabilized? (adaptive thresholds)
        float error_delta = fabsf(g_sys.mean_error - g_sys.prev_mean_error);
        
        int error_stable = (error_delta < g_sys.stability_eps);
        int activation_stable = (g_sys.activation_delta < g_sys.activation_eps);
        
        // Require minimum depth for meaningful thought
        if (error_stable && activation_stable && hop >= (MIN_THOUGHT_HOPS - 1)) {
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
        g_sys.P1[i] *= LAMBDA_DECAY;
        g_sys.P0[i] *= LAMBDA_DECAY;
        
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
        e->C11 *= LAMBDA_DECAY;
        e->C10 *= LAMBDA_DECAY;
        
        // Continuous accumulation weighted by activation
        e->C11 += src->a_prev * a_j_next;
        e->C10 += src->a_prev * (1.0f - a_j_next);
        
        // Compute usefulness (continuous)
        float p_j_given_i = e->C11 / (e->C11 + e->C10 + 1e-6f);
        float p_j = g_sys.P1[e->dst] / (g_sys.P1[e->dst] + g_sys.P0[e->dst] + 1e-6f);
        float u_ij = p_j_given_i - p_j;
        
        float e_ij = d_ij * surprise;
        
        float U_ij = BETA_BLEND * u_ij + (1.0f - BETA_BLEND) * e_ij;
        
        // Update average usefulness for slow track
        e->avg_U = 0.95f * e->avg_U + 0.05f * U_ij;
        
        // Eligibility trace (continuous)
        e->eligibility = LAMBDA_E * e->eligibility + src->a_prev;
        
        // Fast weight update (continuous, no branches)
        float delta_fast = ETA_FAST * U_ij * e->eligibility;
        // Soft clamp using tanh
        delta_fast = DELTA_MAX * tanhf(delta_fast / DELTA_MAX);
        
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
    float density_error = g_sys.current_density - TARGET_DENSITY;
    g_sys.prune_rate += ADAPT_RATE * density_error;
    if (g_sys.prune_rate < 0.0001f) g_sys.prune_rate = 0.0001f;
    if (g_sys.prune_rate > 0.01f) g_sys.prune_rate = 0.01f;
    
    // 2. CREATION RATE: Increase if too sparse and prediction is good
    // Decrease if too dense or prediction is poor
    float density_deficit = TARGET_DENSITY - g_sys.current_density;
    float prediction_quality = g_sys.prediction_acc - TARGET_PREDICTION_ACC;
    g_sys.create_rate += ADAPT_RATE * density_deficit * (1.0f + prediction_quality);
    if (g_sys.create_rate < 0.001f) g_sys.create_rate = 0.001f;
    if (g_sys.create_rate > 0.1f) g_sys.create_rate = 0.1f;
    
    // 3. ACTIVATION SCALE: Adjust to maintain target activity level
    float activity_error = g_sys.current_activity - TARGET_ACTIVITY;
    // If too active, increase scale (harder to activate)
    // If not active enough, decrease scale (easier to activate)
    g_sys.activation_scale += ADAPT_RATE * 100.0f * activity_error;
    if (g_sys.activation_scale < 16.0f) g_sys.activation_scale = 16.0f;
    if (g_sys.activation_scale > 256.0f) g_sys.activation_scale = 256.0f;
    
    // 4. ENERGY ALPHA: Adapt based on prediction accuracy
    // If prediction is poor, increase energy learning to respond faster
    // If prediction is good, decrease to avoid overreaction
    float acc_deficit = TARGET_PREDICTION_ACC - g_sys.prediction_acc;
    g_sys.energy_alpha += ADAPT_RATE * 0.1f * acc_deficit;
    if (g_sys.energy_alpha < 0.01f) g_sys.energy_alpha = 0.01f;
    if (g_sys.energy_alpha > 0.5f) g_sys.energy_alpha = 0.5f;
    
    // 5. ENERGY DECAY: Faster decay if predictions are stable
    // Slower decay if predictions are unstable (keep energy around longer)
    float stability = 1.0f - fabsf(acc_deficit);
    g_sys.energy_decay += ADAPT_RATE * 0.01f * (stability - 0.5f);
    if (g_sys.energy_decay < 0.95f) g_sys.energy_decay = 0.95f;
    if (g_sys.energy_decay > 0.999f) g_sys.energy_decay = 0.999f;
    
    // 6. SIGMOID_K: Adapt based on activity variance
    // If activity is too uniform, sharpen transitions (increase k)
    // If activity is too varied, smooth transitions (decrease k)
    float activity_pressure = (g_sys.current_activity < 0.05f || g_sys.current_activity > 0.5f) ? 1.0f : -1.0f;
    g_sys.sigmoid_k += ADAPT_RATE * activity_pressure;
    if (g_sys.sigmoid_k < 0.1f) g_sys.sigmoid_k = 0.1f;
    if (g_sys.sigmoid_k > 2.0f) g_sys.sigmoid_k = 2.0f;
    
    // 7. EPSILON RANGE: Widen if prediction is poor, narrow if good
    // This allows more exploration when needed
    float exploration_need = (g_sys.prediction_acc < TARGET_PREDICTION_ACC) ? 1.0f : -1.0f;
    g_sys.epsilon_max += ADAPT_RATE * 0.1f * exploration_need;
    if (g_sys.epsilon_max < 0.2f) g_sys.epsilon_max = 0.2f;
    if (g_sys.epsilon_max > 0.5f) g_sys.epsilon_max = 0.5f;
    
    // Epsilon_min stays relatively stable
    g_sys.epsilon_min = g_sys.epsilon_max * 0.2f; // always 20% of max
    
    // 8. LAYER EMERGENCE RATE: Increase if density is high and structure is good
    float structural_readiness = g_sys.current_density * g_sys.prediction_acc;
    g_sys.layer_rate += ADAPT_RATE * 0.01f * (structural_readiness - 0.1f);
    if (g_sys.layer_rate < 0.0001f) g_sys.layer_rate = 0.0001f;
    if (g_sys.layer_rate > 0.01f) g_sys.layer_rate = 0.01f;
    
    // 9. CAPACITY MANAGEMENT: If near capacity, increase pruning, decrease creation
    float node_capacity_ratio = (float)g_graph.node_count / (float)g_graph.node_cap;
    float edge_capacity_ratio = (float)g_graph.edge_count / (float)g_graph.edge_cap;
    
    if (node_capacity_ratio > CAPACITY_THRESH) {
        g_sys.prune_rate *= 1.01f;
        g_sys.create_rate *= 0.99f;
    }
    if (edge_capacity_ratio > CAPACITY_THRESH) {
        g_sys.prune_rate *= 1.02f;
    }
    
    // 10. THOUGHT DEPTH ADAPTATION: Adjust max hops based on settlement patterns
    float current_settle_ratio = (g_sys.thoughts_settled + g_sys.thoughts_maxed) > 0
        ? (float)g_sys.thoughts_settled / (float)(g_sys.thoughts_settled + g_sys.thoughts_maxed)
        : 0.5f;
    
    // If too many thoughts max out, increase limit
    // If too many settle early, decrease limit
    float settle_error = current_settle_ratio - TARGET_SETTLE_RATIO;
    float depth_error = (float)g_sys.thought_depth - (float)TARGET_THOUGHT_DEPTH;
    
    // Increase max if thoughts often max out (settle_ratio low)
    // Decrease max if thoughts settle too early (depth below target)
    float hop_adjustment = -ADAPT_RATE * 10.0f * (settle_error + 0.5f * depth_error);
    g_sys.max_thought_hops = (uint16_t)((float)g_sys.max_thought_hops + hop_adjustment);
    if (g_sys.max_thought_hops < MIN_THOUGHT_HOPS) g_sys.max_thought_hops = MIN_THOUGHT_HOPS;
    if (g_sys.max_thought_hops > MAX_THOUGHT_HOPS_LIMIT) g_sys.max_thought_hops = MAX_THOUGHT_HOPS_LIMIT;
    
    // 11. STABILITY THRESHOLD ADAPTATION: Adjust based on thought depth
    // If thoughts too shallow, tighten convergence (decrease eps)
    // If thoughts too deep, relax convergence (increase eps)
    float depth_pressure = ((float)g_sys.thought_depth - (float)TARGET_THOUGHT_DEPTH) / (float)TARGET_THOUGHT_DEPTH;
    g_sys.stability_eps += ADAPT_RATE * 0.01f * depth_pressure;
    if (g_sys.stability_eps < 0.001f) g_sys.stability_eps = 0.001f;
    if (g_sys.stability_eps > 0.05f) g_sys.stability_eps = 0.05f;
    
    g_sys.activation_eps += ADAPT_RATE * 0.02f * depth_pressure;
    if (g_sys.activation_eps < 0.005f) g_sys.activation_eps = 0.005f;
    if (g_sys.activation_eps > 0.1f) g_sys.activation_eps = 0.1f;
    
    // 12. TEMPORAL DECAY ADAPTATION: Adjust based on temporal distance
    // If edges are too stale on average, increase decay (make time matter more)
    // If edges are too fresh, decrease decay (time matters less)
    float temporal_pressure = (g_sys.mean_temporal_distance - 10.0f) / 10.0f;
    g_sys.temporal_decay += ADAPT_RATE * 0.1f * temporal_pressure;
    if (g_sys.temporal_decay < 0.01f) g_sys.temporal_decay = 0.01f;
    if (g_sys.temporal_decay > 0.5f) g_sys.temporal_decay = 0.5f;
    
    // 13. SPATIAL SCALING ADAPTATION: Adjust based on connectivity patterns
    // If spatial distances are uniform, adjust scaling
    // High spatial distance = nodes are "far" in graph → may need adjustment
    float spatial_pressure = (g_sys.mean_spatial_distance - 2.0f) / 2.0f;
    g_sys.spatial_k += ADAPT_RATE * spatial_pressure;
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
        
        float w_eff = GAMMA_SLOW * e->w_slow + (1.0f - GAMMA_SLOW) * e->w_fast;
        
        // Compute pruning probability from multiple continuous factors
        float p_weak = soft_below(w_eff, PRUNE_WEIGHT_REF);        // weak weight
        float p_unused = soft_below((float)e->use_count, 10.0f);   // low use
        float p_stale = soft_above((float)e->stale_ticks, STALE_REF); // stale
        
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
        float p_stale = soft_above(staleness, NODE_STALE_REF);
        
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
            float novelty = (co_count / CO_FREQ_REF) * similarity;
            
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
 * LAYER EMERGENCE (Probabilistic meta-node creation from dense clusters)
 * ======================================================================== */

void try_layer_emergence() {
    // Probabilistic layer emergence: continuous density measures
    
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
        float connectivity_strength = soft_above((float)n->out_deg, (float)(LAYER_MIN_SIZE / 2));
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
        float p_dense = soft_above(density, DENSITY_REF);
        float p_size = soft_above(total_neighbors, (float)LAYER_MIN_SIZE);
        
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
 * PERSISTENCE
 * ======================================================================== */

void persist_graph() {
    FILE *fn = fopen("nodes.bin", "wb");
    if (fn) {
        fwrite(&g_graph.node_count, sizeof(uint32_t), 1, fn);
        fwrite(&g_graph.next_node_id, sizeof(uint32_t), 1, fn);
        fwrite(g_graph.nodes, sizeof(Node), g_graph.node_count, fn);
        fclose(fn);
    }
    
    FILE *fe = fopen("edges.bin", "wb");
    if (fe) {
        fwrite(&g_graph.edge_count, sizeof(uint32_t), 1, fe);
        fwrite(g_graph.edges, sizeof(Edge), g_graph.edge_count, fe);
        fclose(fe);
    }
    
    printf("[PERSIST] tick=%llu nodes=%u edges=%u\n", 
           (unsigned long long)g_sys.tick, g_graph.node_count, g_graph.edge_count);
}

void restore_graph() {
    FILE *fn = fopen("nodes.bin", "rb");
    if (fn) {
        uint32_t count, next_id;
        fread(&count, sizeof(uint32_t), 1, fn);
        fread(&next_id, sizeof(uint32_t), 1, fn);
        
        if (count <= g_graph.node_cap) {
            fread(g_graph.nodes, sizeof(Node), count, fn);
            g_graph.node_count = count;
            g_graph.next_node_id = next_id;
            printf("[RESTORE] Loaded %u nodes\n", count);
        }
        fclose(fn);
    }
    
    FILE *fe = fopen("edges.bin", "rb");
    if (fe) {
        uint32_t count;
        fread(&count, sizeof(uint32_t), 1, fe);
        
        if (count <= g_graph.edge_cap) {
            fread(g_graph.edges, sizeof(Edge), count, fe);
            g_graph.edge_count = count;
            printf("[RESTORE] Loaded %u edges\n", count);
        }
        fclose(fe);
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
    // Extract up to FRAME_SIZE bytes from RX ring for current tick
    g_sys.current_frame_len = ring_read(&g_sys.rx_ring, g_sys.current_frame, FRAME_SIZE);
}

void merge_output_into_input() {
    // Append last output to current input frame for self-observation
    if (g_sys.last_output_frame_len > 0 && 
        g_sys.current_frame_len + g_sys.last_output_frame_len < FRAME_SIZE) {
        
        memcpy(g_sys.current_frame + g_sys.current_frame_len,
               g_sys.last_output_frame,
               g_sys.last_output_frame_len);
        
        g_sys.current_frame_len += g_sys.last_output_frame_len;
    }
}

void emit_action() {
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
        
        // Continuous probabilistic housekeeping (no hard schedules)
        // Prune is called every tick but acts probabilistically inside
        prune();
        
        // Layer emergence is rare, modulated by energy (adaptive rate)
        float p_layer = g_sys.layer_rate * (1.0f + g_sys.energy);
        if (randf() < p_layer) {
            try_layer_emergence();
        }
        
        // Persistence still on fixed schedule (practical consideration)
        if (g_sys.tick % SNAPSHOT_PERIOD == 0) {
            persist_graph();
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
        
        g_sys.tick++;
        
        // Sleep for tick duration
        usleep(TICK_MS * 1000);
    }
}

/* ========================================================================
 * INITIALIZATION
 * ======================================================================== */

void system_init(uint32_t node_cap, uint32_t edge_cap, uint32_t detector_cap, uint32_t macro_cap) {
    memset(&g_sys, 0, sizeof(System));
    
    graph_init(&g_graph, node_cap, edge_cap);
    
    ring_init(&g_sys.rx_ring, RX_RING_SIZE);
    ring_init(&g_sys.tx_ring, TX_RING_SIZE);
    
    detector_init(detector_cap);
    macro_init(macro_cap);
    
    g_sys.P1 = calloc(node_cap, sizeof(float));
    g_sys.P0 = calloc(node_cap, sizeof(float));
    
    // Initialize baseline to 0.5
    for (uint32_t i = 0; i < node_cap; i++) {
        g_sys.P1[i] = 0.5f;
        g_sys.P0[i] = 0.5f;
    }
    
    // Add default detectors
    detector_add("\n", 1);           // newline
    detector_add("/dev/video", 1);   // video device
    detector_add("error", 1);        // error string
    detector_add("Error", 1);
    detector_add("\xFF\xD8\xFF", 1); // JPEG header
    detector_add("created", 1);
    detector_add("success", 1);
    detector_add("failed", 1);
    detector_add("$", 1);            // shell prompt
    
    // Add default macros
    macro_add_defaults();
    
    printf("[INIT] System initialized: %u nodes, %u edges, %u detectors, %u macros\n",
           node_cap, edge_cap, g_sys.detector_count, g_sys.macro_count);
}

void system_cleanup() {
    graph_free(&g_graph);
    ring_free(&g_sys.rx_ring);
    ring_free(&g_sys.tx_ring);
    free(g_sys.detectors);
    free(g_sys.macros);
    free(g_sys.P1);
    free(g_sys.P0);
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char **argv) {
    uint32_t node_cap = DEFAULT_NODE_CAP;
    uint32_t edge_cap = DEFAULT_EDGE_CAP;
    uint32_t detector_cap = DEFAULT_DETECTOR_CAP;
    uint32_t macro_cap = DEFAULT_MACRO_CAP;
    
    // Simple CLI parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--nodes") == 0 && i + 1 < argc) {
            node_cap = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--edges") == 0 && i + 1 < argc) {
            edge_cap = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--nodes N] [--edges M]\n", argv[0]);
            printf("  --nodes N   Node capacity (default %u)\n", DEFAULT_NODE_CAP);
            printf("  --edges M   Edge capacity (default %u)\n", DEFAULT_EDGE_CAP);
            return 0;
        }
    }
    
    srand(time(NULL));
    
    // Set stdin to non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    
    system_init(node_cap, edge_cap, detector_cap, macro_cap);
    
    // Try to restore previous state
    restore_graph();
    
    printf("=== MELVIN CORE STARTING ===\n");
    printf("Tick period: %d ms\n", TICK_MS);
    printf("Always-on loop active. Press Ctrl+C to stop.\n\n");
    
    main_loop();
    
    system_cleanup();
    
    return 0;
}

