#include <stdio.h>
#include <stdint.h>

// Each pixel is 3 bytes (RGB)
typedef struct {
    uint8_t r, g, b;
} Pixel;

int main() {
    printf("IMAGE PROCESSING CONCEPT:\n\n");
    
    printf("Pixel data as tokens:\n");
    Pixel p1 = {255, 0, 0};    // Red pixel
    Pixel p2 = {0, 255, 0};    // Green pixel
    
    uint8_t token[64];
    token[0] = p1.r;
    token[1] = p1.g;
    token[2] = p1.b;
    
    printf("  Token for red pixel: [%u, %u, %u]\n", token[0], token[1], token[2]);
    printf("  Token for green pixel: [%u, %u, %u]\n", p2.r, p2.g, p2.b);
    
    printf("\nCould learn:\n");
    printf("  - Adjacent pixel patterns\n");
    printf("  - Edge detection (high contrast)\n");
    printf("  - Color gradients\n");
    printf("  - Object boundaries\n");
    printf("  - Texture patterns\n");
    
    printf("\nTraining example:\n");
    printf("  Input:  RGB(255,0,0) RGB(255,0,0)\n");
    printf("  Output: RGB(255,0,0)  (learns red line continues)\n");
    
    printf("\nMultimodal example:\n");
    printf("  Input:  RGB(255,0,0) RGB(255,0,0) RGB(255,0,0)\n");
    printf("  Output: 'red' 'line'  (learns to caption!)\n");
    
    return 0;
}
