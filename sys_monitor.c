/*
 * sys_monitor.c — Monitor /sys for temperature, power, GPU, fan
 */

#include "sys_monitor.h"
#include "shm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define MAX_THERMAL_ZONES 16
static char thermal_paths[MAX_THERMAL_ZONES][256];
static int thermal_count = 0;

int sys_monitor_init(void) {
    /* Find thermal zones */
    for (int i = 0; i < MAX_THERMAL_ZONES; i++) {
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", i);
        
        if (access(path, R_OK) == 0) {
            strncpy(thermal_paths[thermal_count], path, sizeof(thermal_paths[0]));
            thermal_count++;
        }
    }
    
    printf("[SYS] Found %d thermal zones\n", thermal_count);
    return 0;
}

void sys_monitor_cleanup(void) {
    /* Nothing to cleanup */
}

static void emit_temperature(void) {
    char data[1024] = {0};
    int offset = 0;
    
    for (int i = 0; i < thermal_count; i++) {
        FILE *fp = fopen(thermal_paths[i], "r");
        if (fp) {
            int temp;
            if (fscanf(fp, "%d", &temp) == 1) {
                int written = snprintf(data + offset, sizeof(data) - offset,
                                       "temp%d=%d.%d ", i, temp / 1000, (temp % 1000) / 100);
                if (written > 0) offset += written;
            }
            fclose(fp);
        }
    }
    
    if (offset > 0) {
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_SYS;
        frame.subtype = SYS_TEMP;
        frame.timestamp = get_timestamp_us();
        
        snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE, 
                 "sys:temp %s", data);
        frame.length = strlen((char *)frame.payload);
        
        shm_write_rx(&frame);
    }
}

static void emit_power(void) {
    /* Try to read Jetson power stats */
    FILE *fp = fopen("/sys/devices/50000000.host1x/546c0000.i2c/i2c-6/6-0040/iio_device/in_power0_input", "r");
    if (!fp) {
        /* Try alternative path */
        fp = fopen("/sys/bus/i2c/drivers/ina3221x/6-0040/iio:device0/in_power0_input", "r");
    }
    
    if (fp) {
        int power;
        if (fscanf(fp, "%d", &power) == 1) {
            MelvinFrame frame;
            memset(&frame, 0, sizeof(frame));
            
            frame.source = SRC_SYS;
            frame.subtype = SYS_POWER;
            frame.timestamp = get_timestamp_us();
            
            snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE,
                     "sys:power %d mW", power);
            frame.length = strlen((char *)frame.payload);
            
            shm_write_rx(&frame);
        }
        fclose(fp);
    }
}

static void emit_gpu(void) {
    /* Read GPU stats if available */
    FILE *fp = popen("cat /sys/devices/gpu.0/load 2>/dev/null || echo '0'", "r");
    if (fp) {
        int gpu_load;
        if (fscanf(fp, "%d", &gpu_load) == 1) {
            MelvinFrame frame;
            memset(&frame, 0, sizeof(frame));
            
            frame.source = SRC_SYS;
            frame.subtype = SYS_GPU;
            frame.timestamp = get_timestamp_us();
            
            snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE,
                     "sys:gpu load=%d%%", gpu_load);
            frame.length = strlen((char *)frame.payload);
            
            shm_write_rx(&frame);
        }
        pclose(fp);
    }
}

void sys_monitor_poll(void) {
    static uint32_t tick = 0;
    
    /* Emit temperature every tick */
    emit_temperature();
    
    /* Emit power every 5 ticks */
    if (tick % 5 == 0) {
        emit_power();
    }
    
    /* Emit GPU every 5 ticks */
    if (tick % 5 == 0) {
        emit_gpu();
    }
    
    tick++;
}

