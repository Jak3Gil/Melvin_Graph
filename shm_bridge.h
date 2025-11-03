/*
 * shm_bridge.h — Shared memory interface for MelvinOS Daemon
 */

#ifndef SHM_BRIDGE_H
#define SHM_BRIDGE_H

#include "melvin_protocol.h"

/* Initialize shared memory regions */
int shm_init(void);

/* Close and cleanup shared memory */
void shm_close(void);

/* Write a frame to RX buffer (daemon -> melvin_core) */
int shm_write_rx(const MelvinFrame *frame);

/* Read a frame from TX buffer (melvin_core -> daemon) */
int shm_read_tx(MelvinFrame *frame);

/* Get statistics */
void shm_get_stats(uint32_t *rx_count, uint32_t *tx_count);

#endif /* SHM_BRIDGE_H */

