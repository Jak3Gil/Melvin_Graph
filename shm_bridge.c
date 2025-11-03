/*
 * shm_bridge.c — Shared memory ring buffer implementation
 */

#include "shm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

static int shm_rx_fd = -1;
static int shm_tx_fd = -1;
static void *shm_rx_ptr = NULL;
static void *shm_tx_ptr = NULL;
static ShmRingHeader *rx_header = NULL;
static ShmRingHeader *tx_header = NULL;
static MelvinFrame *rx_frames = NULL;
static MelvinFrame *tx_frames = NULL;
static uint32_t rx_capacity = 0;
static uint32_t tx_capacity = 0;

static int create_shm(const char *path, size_t size, int *fd, void **ptr) {
    /* Try to unlink first in case it exists */
    shm_unlink(path + 9);  /* skip /dev/shm/ */
    
    *fd = shm_open(path + 9, O_CREAT | O_RDWR, 0666);
    if (*fd < 0) {
        perror("shm_open");
        return -1;
    }
    
    if (ftruncate(*fd, size) < 0) {
        perror("ftruncate");
        close(*fd);
        return -1;
    }
    
    *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0);
    if (*ptr == MAP_FAILED) {
        perror("mmap");
        close(*fd);
        return -1;
    }
    
    memset(*ptr, 0, size);
    return 0;
}

int shm_init(void) {
    /* Create RX buffer (daemon writes, melvin_core reads) */
    if (create_shm(SHM_RX_PATH, SHM_SIZE, &shm_rx_fd, &shm_rx_ptr) < 0) {
        fprintf(stderr, "Failed to create RX shared memory\n");
        return -1;
    }
    
    /* Create TX buffer (melvin_core writes, daemon reads) */
    if (create_shm(SHM_TX_PATH, SHM_SIZE, &shm_tx_fd, &shm_tx_ptr) < 0) {
        fprintf(stderr, "Failed to create TX shared memory\n");
        munmap(shm_rx_ptr, SHM_SIZE);
        close(shm_rx_fd);
        return -1;
    }
    
    /* Setup RX ring */
    rx_header = (ShmRingHeader *)shm_rx_ptr;
    rx_frames = (MelvinFrame *)((uint8_t *)shm_rx_ptr + sizeof(ShmRingHeader));
    rx_capacity = (SHM_SIZE - sizeof(ShmRingHeader)) / sizeof(MelvinFrame);
    rx_header->capacity = rx_capacity;
    rx_header->write_pos = 0;
    rx_header->read_pos = 0;
    rx_header->count = 0;
    
    /* Setup TX ring */
    tx_header = (ShmRingHeader *)shm_tx_ptr;
    tx_frames = (MelvinFrame *)((uint8_t *)shm_tx_ptr + sizeof(ShmRingHeader));
    tx_capacity = (SHM_SIZE - sizeof(ShmRingHeader)) / sizeof(MelvinFrame);
    tx_header->capacity = tx_capacity;
    tx_header->write_pos = 0;
    tx_header->read_pos = 0;
    tx_header->count = 0;
    
    printf("[SHM] Initialized RX/TX buffers: %u frames each\n", rx_capacity);
    
    return 0;
}

void shm_close(void) {
    if (shm_rx_ptr) {
        munmap(shm_rx_ptr, SHM_SIZE);
        shm_rx_ptr = NULL;
    }
    
    if (shm_tx_ptr) {
        munmap(shm_tx_ptr, SHM_SIZE);
        shm_tx_ptr = NULL;
    }
    
    if (shm_rx_fd >= 0) {
        close(shm_rx_fd);
        shm_unlink("melvin_rx");
        shm_rx_fd = -1;
    }
    
    if (shm_tx_fd >= 0) {
        close(shm_tx_fd);
        shm_unlink("melvin_tx");
        shm_tx_fd = -1;
    }
}

int shm_write_rx(const MelvinFrame *frame) {
    if (!rx_header || !rx_frames) return -1;
    
    /* Check if buffer is full */
    if (rx_header->count >= rx_capacity) {
        return -1;  /* Buffer full, drop frame */
    }
    
    /* Write frame */
    memcpy(&rx_frames[rx_header->write_pos], frame, sizeof(MelvinFrame));
    
    /* Update write position */
    rx_header->write_pos = (rx_header->write_pos + 1) % rx_capacity;
    uint32_t new_count = rx_header->count + 1;
    rx_header->count = new_count;
    
    return 0;
}

int shm_read_tx(MelvinFrame *frame) {
    if (!tx_header || !tx_frames) return -1;
    
    /* Check if buffer is empty */
    if (tx_header->count == 0) {
        return -1;  /* No data available */
    }
    
    /* Read frame */
    memcpy(frame, &tx_frames[tx_header->read_pos], sizeof(MelvinFrame));
    
    /* Update read position */
    tx_header->read_pos = (tx_header->read_pos + 1) % tx_capacity;
    uint32_t new_count = tx_header->count - 1;
    tx_header->count = new_count;
    
    return 0;
}

void shm_get_stats(uint32_t *rx_count, uint32_t *tx_count) {
    if (rx_count && rx_header) *rx_count = rx_header->count;
    if (tx_count && tx_header) *tx_count = tx_header->count;
}

