/*
 * can_monitor.c — Monitor CAN bus (SocketCAN)
 */

#include "can_monitor.h"
#include "shm_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef __linux__
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#endif

static int can_sock = -1;
static int can_available = 0;

int can_monitor_init(void) {
#ifndef __linux__
    printf("[CAN] Not available on non-Linux systems\n");
    can_available = 0;
    return 0;
#else
    struct sockaddr_can addr;
    struct ifreq ifr;
    
    /* Try to open CAN socket */
    can_sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (can_sock < 0) {
        printf("[CAN] SocketCAN not available (not an error)\n");
        can_available = 0;
        return 0;  /* Not a fatal error */
    }
    
    /* Try can0 interface */
    strcpy(ifr.ifr_name, "can0");
    if (ioctl(can_sock, SIOCGIFINDEX, &ifr) < 0) {
        close(can_sock);
        can_sock = -1;
        can_available = 0;
        printf("[CAN] No can0 interface found (not an error)\n");
        return 0;
    }
    
    /* Bind to can0 */
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    if (bind(can_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(can_sock);
        can_sock = -1;
        can_available = 0;
        printf("[CAN] Failed to bind to can0 (not an error)\n");
        return 0;
    }
    
    /* Set non-blocking */
    int flags = fcntl(can_sock, F_GETFL, 0);
    fcntl(can_sock, F_SETFL, flags | O_NONBLOCK);
    
    can_available = 1;
    printf("[CAN] Monitoring can0\n");
    
    return 0;
#endif
}

void can_monitor_cleanup(void) {
    if (can_sock >= 0) {
        close(can_sock);
        can_sock = -1;
    }
}

void can_monitor_poll(void) {
    if (!can_available || can_sock < 0) return;
    
#ifdef __linux__
    struct can_frame frame_can;
    int nbytes;
    int frames_read = 0;
    
    /* Read up to 10 CAN frames per poll */
    while (frames_read < 10) {
        nbytes = read(can_sock, &frame_can, sizeof(struct can_frame));
        
        if (nbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  /* No more data */
            }
            break;
        }
        
        if (nbytes < sizeof(struct can_frame)) {
            break;
        }
        
        /* Create Melvin frame */
        MelvinFrame frame;
        memset(&frame, 0, sizeof(frame));
        
        frame.source = SRC_CAN;
        frame.subtype = (frame_can.can_id & CAN_EFF_FLAG) ? CAN_EXTENDED : CAN_STANDARD;
        frame.timestamp = get_timestamp_us();
        
        /* Format: "can:id=0x123 len=8 data=0102030405060708" */
        int offset = snprintf((char *)frame.payload, FRAME_PAYLOAD_SIZE,
                              "can:id=0x%03X len=%d data=",
                              frame_can.can_id & CAN_EFF_MASK,
                              frame_can.can_dlc);
        
        for (int i = 0; i < frame_can.can_dlc && i < 8; i++) {
            offset += snprintf((char *)frame.payload + offset,
                               FRAME_PAYLOAD_SIZE - offset,
                               "%02X", frame_can.data[i]);
        }
        
        frame.length = offset;
        
        shm_write_rx(&frame);
        frames_read++;
    }
#endif
}

