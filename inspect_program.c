/*
 * MELVIN PROGRAM INSPECTOR
 * ========================
 * Views Melvin graph as a compiled program:
 * - Subroutines = Cell clusters that fire together
 * - Functions = Strong synaptic pathways
 * - Variables = Cell memory values
 * - Control flow = Energy gradients
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

/* Same structures as melvin_core.c */
typedef struct {
    float state;
    float energy;
    float threshold;
    float memory;
} Node;

typedef struct {
    uint32_t src;
    uint32_t dst;
    float weight;
} Connection;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t node_count;
    uint32_t node_cap;
    uint32_t connection_count;
    uint32_t connection_cap;
    uint64_t tick;
    uint32_t byte_to_node[256];
} GraphHeader;

void print_meta_nodes(Node *nodes) {
    printf("\n📋 SYSTEM PARAMETERS (Meta-nodes 0-20 - Graph Controlled!)\n");
    printf("═════════════════════════════════════════════════════════════\n");
    printf("Core Parameters:\n");
    printf("  [0]  Learning rate:         %.4f\n", nodes[0].value);
    printf("  [1]  Decay rate:            %.4f\n", nodes[1].value);
    printf("  [2]  Memory decay:          %.4f\n", nodes[2].value);
    printf("  [3]  Mitosis threshold:     %.1f\n", nodes[3].value);
    printf("  [4]  Transmission cost:     %.4f\n", nodes[4].value);
    printf("  [5]  Activation cost:       %.4f\n", nodes[5].value);
    printf("  [6]  Metabolism cost:       %.4f\n", nodes[6].value);
    printf("  [7]  Initial population:    %.0f\n", nodes[7].value);
    printf("  [8]  Initial connections:   %.0f\n", nodes[8].value);
    printf("  [9]  Propagation depth:     %.0f\n", nodes[9].value);
    printf("  [10] Sprout rate:           %.4f\n", nodes[10].value);
    printf("\nEmergent Thresholds (Discovered by Graph!):\n");
    printf("  [11] Activation threshold:  %.4f\n", nodes[11].value);
    printf("  [12] Wiring threshold:      %.4f\n", nodes[12].value);
    printf("  [13] Pattern threshold:     %.4f\n", nodes[13].value);
    printf("  [14] Hub degree threshold:  %.1f\n", nodes[14].value);
    printf("  [15] Hub weight threshold:  %.1f\n", nodes[15].value);
    printf("  [16] Module threshold:      %.1f\n", nodes[16].value);
    printf("  [17] Saturation threshold:  %.4f\n", nodes[17].value);
    printf("  [18] Association window:    %.1f ticks\n", nodes[18].value);
    printf("  [19] Quality threshold:     %.4f\n", nodes[19].value);
    printf("  [20] Output threshold:      %.4f\n", nodes[20].value);
}

void print_program_stats(Node *nodes, uint32_t node_count, Connection *connections, uint32_t connection_count) {
    uint32_t alive = 0, active = 0, thriving = 0;
    float total_energy = 0.0f;
    
    for (uint32_t i = 21; i < node_count; i++) {
        if (nodes[i].energy > 0.0f) {
            alive++;
            total_energy += nodes[i].energy;
            if (nodes[i].state > 0.3f) active++;
            if (nodes[i].energy > 200.0f) thriving++;
        }
    }
    
    printf("\n🧠 PROGRAM STATE\n");
    printf("═════════════════════════════════════════\n");
    printf("  Total nodes:      %u\n", node_count - 21);
    printf("  Live nodes:       %u (%.1f%%)\n", alive, 100.0f * alive / (node_count - 21));
    printf("  Active nodes:     %u\n", active);
    printf("  Thriving nodes:   %u (ready to divide)\n", thriving);
    printf("  Total connections:   %u\n", connection_count);
    printf("  Total energy:     %.1f\n", total_energy);
    printf("  Avg energy/node:  %.1f\n", alive > 0 ? total_energy / alive : 0.0f);
}

void analyze_subroutines(Node *nodes, uint32_t node_count, Connection *connections, uint32_t connection_count, uint32_t *byte_to_node) {
    printf("\n🔗 STRONGEST PATHWAYS (Functions)\n");
    printf("═════════════════════════════════════════\n");
    
    // Find strongest connections
    typedef struct {
        uint32_t src, dst;
        float weight;
    } WeightedEdge;
    
    WeightedEdge *edges = malloc(connection_count * sizeof(WeightedEdge));
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < connection_count; i++) {
        if (connections[i].weight > 1.0f) {  // Only strong connections
            edges[count].src = connections[i].src;
            edges[count].dst = connections[i].dst;
            edges[count].weight = connections[i].weight;
            count++;
        }
    }
    
    // Sort by weight
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (edges[j].weight > edges[i].weight) {
                WeightedEdge temp = edges[i];
                edges[i] = edges[j];
                edges[j] = temp;
            }
        }
    }
    
    // Show top 20
    uint32_t show = count < 20 ? count : 20;
    for (uint32_t i = 0; i < show; i++) {
        uint32_t src = edges[i].src;
        uint32_t dst = edges[i].dst;
        
        // Try to find what byte this represents
        char src_char = '?', dst_char = '?';
        for (uint32_t b = 0; b < 256; b++) {
            if (byte_to_node[b] == src) {
                if (b >= 32 && b < 127) src_char = b;
                else if (b == 10) src_char = 'n';
                break;
            }
        }
        for (uint32_t b = 0; b < 256; b++) {
            if (byte_to_node[b] == dst) {
                if (b >= 32 && b < 127) dst_char = b;
                else if (b == 10) dst_char = 'n';
                break;
            }
        }
        
        printf("  '%c' → '%c': weight=%.2f (node %u → %u)\n", 
               src_char, dst_char, edges[i].weight, src, dst);
    }
    
    free(edges);
}

void show_memory_values(Node *nodes, uint32_t node_count, uint32_t *byte_to_node) {
    printf("\n💾 LEARNED MEMORY VALUES (Top 20 by energy)\n");
    printf("═════════════════════════════════════════\n");
    
    typedef struct {
        uint32_t id;
        float energy;
        float memory;
    } CellInfo;
    
    CellInfo *sorted = malloc((node_count - 21) * sizeof(CellInfo));
    uint32_t count = 0;
    
    for (uint32_t i = 21; i < node_count; i++) {
        if (nodes[i].energy > 0.0f) {
            sorted[count].id = i;
            sorted[count].energy = nodes[i].energy;
            sorted[count].value = nodes[i].value;
            count++;
        }
    }
    
    // Sort by energy
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (sorted[j].energy > sorted[i].energy) {
                CellInfo temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
        }
    }
    
    // Show top 20
    uint32_t show = count < 20 ? count : 20;
    for (uint32_t i = 0; i < show; i++) {
        uint8_t byte = (uint8_t)sorted[i].value;
        char ch = (byte >= 32 && byte < 127) ? byte : '.';
        printf("  Cell %5u: energy=%6.1f memory=%5.1f ('%c' / 0x%02X)\n",
               sorted[i].id, sorted[i].energy, sorted[i].value, ch, byte);
    }
    
    free(sorted);
}

int main(int argc, char **argv) {
    const char *filename = "graph.mmap";
    if (argc > 1) filename = argv[1];
    
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Cannot open %s\n", filename);
        fprintf(stderr, "Run melvin_core first to create a graph!\n");
        return 1;
    }
    
    struct stat st;
    fstat(fd, &st);
    
    void *base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) {
        fprintf(stderr, "ERROR: mmap failed\n");
        return 1;
    }
    
    GraphHeader *header = (GraphHeader *)base;
    
    if (header->magic != 0xCE112024) {
        fprintf(stderr, "ERROR: Invalid graph file\n");
        return 1;
    }
    
    Node *nodes = (Node *)((char *)base + sizeof(GraphHeader));
    Connection *connections = (Connection *)((char *)nodes + header->node_cap * sizeof(Node));
    
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║  MELVIN PROGRAM INSPECTOR             ║\n");
    printf("║  Graph as Compiled Code               ║\n");
    printf("╚═══════════════════════════════════════╝\n");
    printf("\n📊 RUNTIME INFO\n");
    printf("═════════════════════════════════════════\n");
    printf("  File: %s\n", filename);
    printf("  Tick: %lu\n", header->tick);
    printf("  Node capacity: %u\n", header->node_cap);
    printf("  Connection capacity: %u\n", header->connection_cap);
    
    print_meta_nodes(nodes);
    print_program_stats(nodes, header->node_count, connections, header->connection_count);
    analyze_subroutines(nodes, header->node_count, connections, header->connection_count, header->byte_to_node);
    show_memory_values(nodes, header->node_count, header->byte_to_node);
    
    printf("\n═════════════════════════════════════════\n");
    printf("🧬 Graph is the program!\n");
    printf("   Data flows through = execution\n");
    printf("   Weights = compiled functions\n");
    printf("   Energy = computational priority\n");
    printf("═════════════════════════════════════════\n\n");
    
    munmap(base, st.st_size);
    close(fd);
    
    return 0;
}

