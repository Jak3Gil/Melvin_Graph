/*
 * log_monitor.h — System log monitor
 */

#ifndef LOG_MONITOR_H
#define LOG_MONITOR_H

#include "melvin_protocol.h"

/* Initialize log monitor */
int log_monitor_init(void);

/* Poll logs and emit frames */
void log_monitor_poll(void);

/* Cleanup */
void log_monitor_cleanup(void);

#endif /* LOG_MONITOR_H */

