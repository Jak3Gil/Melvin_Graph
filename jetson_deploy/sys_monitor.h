/*
 * sys_monitor.h — /sys filesystem monitor
 */

#ifndef SYS_MONITOR_H
#define SYS_MONITOR_H

#include "melvin_protocol.h"

/* Initialize sys monitor */
int sys_monitor_init(void);

/* Poll sys data and emit frames */
void sys_monitor_poll(void);

/* Cleanup */
void sys_monitor_cleanup(void);

#endif /* SYS_MONITOR_H */

