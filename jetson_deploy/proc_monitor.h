/*
 * proc_monitor.h — /proc filesystem monitor
 */

#ifndef PROC_MONITOR_H
#define PROC_MONITOR_H

#include "melvin_protocol.h"

/* Initialize proc monitor */
int proc_monitor_init(void);

/* Poll proc data and emit frames */
void proc_monitor_poll(void);

/* Cleanup */
void proc_monitor_cleanup(void);

#endif /* PROC_MONITOR_H */

