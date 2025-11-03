/*
 * can_monitor.h — CAN bus monitor
 */

#ifndef CAN_MONITOR_H
#define CAN_MONITOR_H

#include "melvin_protocol.h"

/* Initialize CAN monitor */
int can_monitor_init(void);

/* Poll CAN bus and emit frames */
void can_monitor_poll(void);

/* Cleanup */
void can_monitor_cleanup(void);

#endif /* CAN_MONITOR_H */

