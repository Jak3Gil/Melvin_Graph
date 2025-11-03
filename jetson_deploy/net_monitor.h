/*
 * net_monitor.h — Network monitor
 */

#ifndef NET_MONITOR_H
#define NET_MONITOR_H

#include "melvin_protocol.h"

/* Initialize network monitor */
int net_monitor_init(void);

/* Poll network and emit frames */
void net_monitor_poll(void);

/* Cleanup */
void net_monitor_cleanup(void);

#endif /* NET_MONITOR_H */

