/*
 * shm_reader.c — Bridge: reads shared memory and writes to stdout
 * 
 * This allows melvin_core to read from shared memory without modification.
 * Usage: ./shm_reader | ./melvin_core
 */

#include "melvin_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>

static volatile int running = 1;
static void *shm_rx_ptr = NULL;
static ShmRingHeader *rx_header = NULL;
static MelvinFrame *rx_frames = NULL;
static uint32_t rx_capacity = 0;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

int shm_open_rx(void) {
    int fd = shm_open("melvin_rx", O_RDONLY, 0666);
    if (fd < 0) {
        perror("shm_open melvin_rx");
        fprintf(stderr, "Make sure melvind is running first!\n");
        return -1;
    }
    
    shm_rx_ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, fd, 0);
    if (shm_rx_ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return -1;
    }
    
    close(fd);
    
    rx_header = (ShmRingHeader *)shm_rx_ptr;
    rx_frames = (MelvinFrame *)((uint8_t *)shm_rx_ptr + sizeof(ShmRingHeader));
    rx_capacity = rx_header->capacity;
    
    fprintf(stderr, "[SHM_READER] Connected to %s (%u frames)\n", 
            SHM_RX_PATH, rx_capacity);
    
    return 0;
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, signal_handler);
    
    if (shm_open_rx() < 0) {
        return 1;
    }
    
    uint32_t last_read_pos = 0;
    
    fprintf(stderr, "[SHM_READER] Streaming frames to stdout...\n");
    
    while (running) {
        /* Check if new data available */
        if (rx_header->count > 0 && last_read_pos != rx_header->write_pos) {
            MelvinFrame frame;
            
            /* Read frame */
            memcpy(&frame, &rx_frames[last_read_pos], sizeof(MelvinFrame));
            last_read_pos = (last_read_pos + 1) % rx_capacity;
            
            /* Write payload to stdout */
            if (frame.length > 0 && frame.length <= FRAME_PAYLOAD_SIZE) {
                ssize_t written = write(STDOUT_FILENO, frame.payload, frame.length);
                if (written < 0) {
                    perror("write");
                    break;
                }
                
                /* Add newline for readability */
                write(STDOUT_FILENO, "\n", 1);
            }
        } else {
            /* No data, sleep briefly */
            usleep(10000);  /* 10ms */
        }
    }
    
    if (shm_rx_ptr) {
        munmap(shm_rx_ptr, SHM_SIZE);
    }
    
    fprintf(stderr, "\n[SHM_READER] Shutdown complete\n");
    
    return 0;
}

