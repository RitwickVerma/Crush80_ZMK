#ifndef VIA_H
#define VIA_H

#include <stdint.h>

#define VIA_PACKET_SIZE 32

/* Handle one VIA command in place: msg (32 bytes) holds the request and is
 * overwritten with the reply. Safe to call from any thread. Also used by the
 * OpenRGB interface for VIA's 32-byte packets (see openrgb.c). */
void via_handle_packet(uint8_t *msg);

#endif /* VIA_H */
