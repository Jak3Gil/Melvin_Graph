/*
 * melvind_main.c — MelvinOS Daemon main loop
 * 
 * Captures all system events and feeds them to melvin_core via shared memory
 */

#include "melvin_protocol.h"
#include "shm_bridge.h"
#include "proc_monitor.h"
#include "sys_monitor.h"
#include "log_monitor.h"
#include "net_monitor.h"
#include "can_monitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    printf("\n[MELVIND] Shutting down...\n");
    running = 0;
}

void print_stats(void) {
    uint32_t rx_count, tx_count;
    shm_get_stats(&rx_count, &tx_count);
    
    printf("[MELVIND] RX buffer: %u frames | TX buffer: %u frames\n",
           rx_count, tx_count);
}

void process_actions(void) {
    MelvinFrame frame;
    int actions_processed = 0;
    
    /* Process up to 10 actions per cycle */
    while (actions_processed < 10 && shm_read_tx(&frame) == 0) {
        /* Parse action from payload */
        char *payload = (char *)frame.payload;
        
        if (strncmp(payload, "ACTION:", 7) == 0) {
            char *action = payload + 7;
            
            printf("[MELVIND] Action received: %s\n", action);
            
            /* TODO: Validate against policy and execute safely */
            /* For now, just log it */
        }
        
        actions_processed++;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    printf("=== MelvinOS Daemon Starting ===\n");
    printf("Version: 1.0.0\n");
    printf("Target: NVIDIA Jetson\n\n");
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize shared memory bridge */
    printf("[INIT] Initializing shared memory...\n");
    if (shm_init() < 0) {
        fprintf(stderr, "Failed to initialize shared memory\n");
        return 1;
    }
    
    /* Initialize all monitors */
    printf("[INIT] Starting monitors...\n");
    
    if (proc_monitor_init() < 0) {
        fprintf(stderr, "Warning: proc monitor failed to initialize\n");
    } else {
        printf("[INIT] ✓ proc monitor\n");
    }
    
    if (sys_monitor_init() < 0) {
        fprintf(stderr, "Warning: sys monitor failed to initialize\n");
    } else {
        printf("[INIT] ✓ sys monitor\n");
    }
    
    if (log_monitor_init() < 0) {
        fprintf(stderr, "Warning: log monitor failed to initialize\n");
    } else {
        printf("[INIT] ✓ log monitor\n");
    }
    
    if (net_monitor_init() < 0) {
        fprintf(stderr, "Warning: net monitor failed to initialize\n");
    } else {
        printf("[INIT] ✓ net monitor\n");
    }
    
    if (can_monitor_init() < 0) {
        /* CAN is optional */
    } else {
        printf("[INIT] ✓ can monitor\n");
    }
    
    printf("\n[MELVIND] All monitors initialized\n");
    printf("[MELVIND] Tick rate: 20 Hz (50ms)\n");
    printf("[MELVIND] Shared memory: %s (RX), %s (TX)\n\n",
           SHM_RX_PATH, SHM_TX_PATH);
    
    uint64_t tick = 0;
    struct timespec tick_start, tick_end;
    
    /* Main loop */
    while (running) {
        clock_gettime(CLOCK_MONOTONIC, &tick_start);
        
        /* Poll all monitors */
        proc_monitor_poll();
        sys_monitor_poll();
        log_monitor_poll();
        net_monitor_poll();
        can_monitor_poll();
        
        /* Process actions from melvin_core */
        process_actions();
        
        /* Print stats every 100 ticks (~5 seconds) */
        if (tick % 100 == 0) {
            print_stats();
        }
        
        tick++;
        
        /* Sleep for remainder of 50ms tick */
        clock_gettime(CLOCK_MONOTONIC, &tick_end);
        long elapsed_us = (tick_end.tv_sec - tick_start.tv_sec) * 1000000L +
                          (tick_end.tv_nsec - tick_start.tv_nsec) / 1000L;
        
        long sleep_us = 50000 - elapsed_us;
        if (sleep_us > 0) {
            usleep(sleep_us);
        } else if (tick % 100 == 0) {
            printf("[MELVIND] Warning: tick overrun by %ld us\n", -sleep_us);
        }
    }
    
    /* Cleanup */
    printf("\n[CLEANUP] Stopping monitors...\n");
    proc_monitor_cleanup();
    sys_monitor_cleanup();
    log_monitor_cleanup();
    net_monitor_cleanup();
    can_monitor_cleanup();
    
    printf("[CLEANUP] Closing shared memory...\n");
    shm_close();
    
    printf("[MELVIND] Shutdown complete\n");
    
    return 0;
}

