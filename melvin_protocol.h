/*
 * melvin_protocol.h — Frame format and type definitions for MelvinOS Daemon
 */

#ifndef MELVIN_PROTOCOL_H
#define MELVIN_PROTOCOL_H

#include <stdint.h>
#include <time.h>

#define FRAME_PAYLOAD_SIZE 4096
#define SHM_RX_PATH "/dev/shm/melvin_rx"
#define SHM_TX_PATH "/dev/shm/melvin_tx"
#define SHM_SIZE (16 * 1024 * 1024)  // 16 MB

/* Source IDs */
enum {
    SRC_PROC = 1,
    SRC_SYS  = 2,
    SRC_LOG  = 3,
    SRC_NET  = 4,
    SRC_CAN  = 5,
    SRC_USER = 6,
    SRC_CTRL = 7
};

/* Subtypes for PROC */
enum {
    PROC_CPU = 1,
    PROC_MEM = 2,
    PROC_LIST = 3,
    PROC_IO = 4
};

/* Subtypes for SYS */
enum {
    SYS_TEMP = 1,
    SYS_POWER = 2,
    SYS_GPU = 3,
    SYS_FAN = 4
};

/* Subtypes for LOG */
enum {
    LOG_KERN = 1,
    LOG_SYSLOG = 2,
    LOG_AUTH = 3,
    LOG_USER = 4
};

/* Subtypes for NET */
enum {
    NET_TCP = 1,
    NET_UDP = 2,
    NET_ICMP = 3,
    NET_RAW = 4
};

/* Subtypes for CAN */
enum {
    CAN_STANDARD = 1,
    CAN_EXTENDED = 2,
    CAN_ERROR = 3
};

/* Frame structure */
typedef struct {
    uint8_t  source;
    uint8_t  subtype;
    uint16_t length;
    uint64_t timestamp;
    uint8_t  payload[FRAME_PAYLOAD_SIZE];
} __attribute__((packed)) MelvinFrame;

/* Shared memory ring buffer header */
typedef struct {
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    volatile uint32_t count;
    uint32_t capacity;
    uint8_t  padding[48];  // cache line alignment
} __attribute__((packed)) ShmRingHeader;

/* Get current timestamp in microseconds */
static inline uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

#endif /* MELVIN_PROTOCOL_H */

