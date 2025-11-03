/*
 * log_monitor.c — Monitor system logs (journalctl, syslog, dmesg)
 */

#include "log_monitor.h"
#include "shm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static FILE *journal_fp = NULL;
static FILE *dmesg_fp = NULL;

int log_monitor_init(void) {
    /* Tail journalctl (last 5 lines, follow mode) */
    journal_fp = popen("journalctl -n 5 -f 2>/dev/null", "r");
    if (journal_fp) {
        /* Set non-blocking */
        int fd = fileno(journal_fp);
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    
    return 0;
}

void log_monitor_cleanup(void) {
    if (journal_fp) pclose(journal_fp);
    if (dmesg_fp) pclose(dmesg_fp);
}

static void emit_journal_logs(void) {
    if (!journal_fp) return;
    
    char line[1024];
    int lines_read = 0;
    
    /* Read up to 10 lines */
    while (lines_read < 10 && fgets(line, sizeof(line), journal_fp)) {
        size_t len = strlen(line);
        if (len > 0) {
            MelvinFrame frame;
            memset(&frame, 0, sizeof(frame));
            
            frame.source = SRC_LOG;
            frame.subtype = LOG_SYSLOG;
            frame.timestamp = get_timestamp_us();
            
            snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE,
                     "log:journal %s", line);
            frame.length = strlen((char *)frame.payload);
            
            shm_write_rx(&frame);
            lines_read++;
        }
    }
}

static void emit_dmesg(void) {
    /* Get recent kernel messages */
    FILE *fp = popen("dmesg | tail -5", "r");
    if (!fp) return;
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        if (len > 0) {
            MelvinFrame frame;
            memset(&frame, 0, sizeof(frame));
            
            frame.source = SRC_LOG;
            frame.subtype = LOG_KERN;
            frame.timestamp = get_timestamp_us();
            
            snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE,
                     "log:kernel %s", line);
            frame.length = strlen((char *)frame.payload);
            
            shm_write_rx(&frame);
        }
    }
    
    pclose(fp);
}

void log_monitor_poll(void) {
    static uint32_t tick = 0;
    
    /* Check journal logs every tick (non-blocking) */
    emit_journal_logs();
    
    /* Emit dmesg every 20 ticks (~1 second) */
    if (tick % 20 == 0) {
        emit_dmesg();
    }
    
    tick++;
}

