/*
 * net_monitor.c — Monitor network statistics and connections
 */

#include "net_monitor.h"
#include "shm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int net_monitor_init(void) {
    /* Check if we can read network stats */
    if (access("/proc/net/dev", R_OK) != 0) {
        fprintf(stderr, "Warning: Cannot read /proc/net/dev\n");
        return -1;
    }
    
    return 0;
}

void net_monitor_cleanup(void) {
    /* Nothing to cleanup */
}

static void emit_net_stats(void) {
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return;
    
    char line[512];
    char output[FRAME_PAYLOAD_SIZE] = {0};
    int offset = 0;
    
    /* Skip first two header lines */
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    /* Read interface stats */
    while (fgets(line, sizeof(line), fp) && offset < FRAME_PAYLOAD_SIZE - 100) {
        char iface[32];
        unsigned long rx_bytes, tx_bytes;
        
        if (sscanf(line, "%31[^:]:%lu %*u %*u %*u %*u %*u %*u %*u %lu",
                   iface, &rx_bytes, &tx_bytes) == 3) {
            
            /* Trim whitespace from interface name */
            char *p = iface;
            while (*p == ' ') p++;
            
            int written = snprintf(output + offset, FRAME_PAYLOAD_SIZE - offset,
                                   "%s:rx=%lu,tx=%lu ", p, rx_bytes, tx_bytes);
            if (written > 0) offset += written;
        }
    }
    
    fclose(fp);
    
    if (offset > 0) {
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_NET;
        frame.subtype = NET_RAW;
        frame.timestamp = get_timestamp_us();
        
        snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE,
                 "net:stats %s", output);
        frame.length = strlen((char *)frame.payload);
        
        shm_write_rx(&frame);
    }
}

static void emit_connections(void) {
    /* Get active TCP connections */
    FILE *fp = popen("ss -tan | head -10", "r");
    if (!fp) return;
    
    char output[FRAME_PAYLOAD_SIZE];
    size_t bytes = fread(output, 1, FRAME_PAYLOAD_SIZE - 100, fp);
    pclose(fp);
    
    if (bytes > 0) {
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_NET;
        frame.subtype = NET_TCP;
        frame.timestamp = get_timestamp_us();
        
        memcpy(frame.payload, output, bytes);
        frame.length = bytes;
        
        shm_write_rx(&frame);
    }
}

void net_monitor_poll(void) {
    static uint32_t tick = 0;
    
    /* Emit network stats every 10 ticks (~500ms) */
    if (tick % 10 == 0) {
        emit_net_stats();
    }
    
    /* Emit connections every 20 ticks (~1 second) */
    if (tick % 20 == 0) {
        emit_connections();
    }
    
    tick++;
}

