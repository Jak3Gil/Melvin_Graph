#include <stdio.h>
#include <stdint.h>

int main() {
    // Nodes store raw bytes
    uint8_t token[64];
    
    // Can be ANYTHING:
    token[0] = 255;  // Binary
    token[1] = 0x42; // Hex
    token[2] = '\n'; // Control char
    token[3] = 200;  // High value
    
    printf("Nodes store RAW BYTES:\n");
    printf("  [0] = %u (0x%02X)\n", token[0], token[0]);
    printf("  [1] = %u (0x%02X)\n", token[1], token[1]);
    printf("  [2] = %u (0x%02X) '\\n'\n", token[2], token[2]);
    printf("  [3] = %u (0x%02X)\n", token[3], token[3]);
    printf("\nCan store:\n");
    printf("  - Text (ASCII/UTF-8)\n");
    printf("  - Binary data\n");
    printf("  - Images (pixel values)\n");
    printf("  - Audio (samples)\n");
    printf("  - Sensor data\n");
    printf("  - Network packets\n");
    printf("  - DNA sequences\n");
    printf("  - ANYTHING!\n");
    
    return 0;
}
