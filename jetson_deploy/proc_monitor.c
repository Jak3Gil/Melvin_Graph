/*
 * proc_monitor.c — Monitor /proc for CPU, memory, process info
 */

#include "proc_monitor.h"
#include "shm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *stat_fp = NULL;
static FILE *meminfo_fp = NULL;

int proc_monitor_init(void) {
    stat_fp = fopen("/proc/stat", "r");
    if (!stat_fp) {
        perror("Failed to open /proc/stat");
        return -1;
    }
    
    meminfo_fp = fopen("/proc/meminfo", "r");
    if (!meminfo_fp) {
        perror("Failed to open /proc/meminfo");
        fclose(stat_fp);
        return -1;
    }
    
    return 0;
}

void proc_monitor_cleanup(void) {
    if (stat_fp) fclose(stat_fp);
    if (meminfo_fp) fclose(meminfo_fp);
}

static void emit_cpu_stats(void) {
    char line[512];
    
    rewind(stat_fp);
    if (fgets(line, sizeof(line), stat_fp)) {
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_PROC;
        frame.subtype = PROC_CPU;
        frame.timestamp = get_timestamp_us();
        
        snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE, 
                 "proc:cpu %s", line);
        frame.length = strlen((char *)frame.payload);
        
        shm_write_rx(&frame);
    }
}

static void emit_mem_stats(void) {
    char line[512];
    char mem_data[1024] = {0};
    int offset = 0;
    
    rewind(meminfo_fp);
    
    /* Read first few lines of meminfo */
    for (int i = 0; i < 5 && fgets(line, sizeof(line), meminfo_fp); i++) {
        int written = snprintf(mem_data + offset, sizeof(mem_data) - offset, 
                               "%s", line);
        if (written > 0) offset += written;
    }
    
    if (offset > 0) {
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_PROC;
        frame.subtype = PROC_MEM;
        frame.timestamp = get_timestamp_us();
        
        snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE, 
                 "proc:mem %s", mem_data);
        frame.length = strlen((char *)frame.payload);
        
        shm_write_rx(&frame);
    }
}

static void emit_process_list(void) {
    FILE *fp = popen("ps aux | head -10", "r");
    if (!fp) return;
    
    char output[FRAME_PAYLOAD_SIZE];
    size_t bytes = fread(output, 1, FRAME_PAYLOAD_SIZE - 100, fp);
    pclose(fp);
    
    if (bytes > 0) {
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_PROC;
        frame.subtype = PROC_LIST;
        frame.timestamp = get_timestamp_us();
        
        memcpy(frame.payload, output, bytes);
        frame.length = bytes;
        
        shm_write_rx(&frame);
    }
}

void proc_monitor_poll(void) {
    static uint32_t tick = 0;
    
    /* Emit CPU stats every tick */
    emit_cpu_stats();
    
    /* Emit memory stats every tick */
    emit_mem_stats();
    
    /* Emit process list every 10 ticks (~500ms) */
    if (tick % 10 == 0) {
        emit_process_list();
    }
    
    tick++;
}

