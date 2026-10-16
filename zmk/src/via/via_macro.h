#ifndef VIA_MACRO_H
#define VIA_MACRO_H

#include <stdint.h>

#define VIA_MACRO_COUNT 16
#define VIA_MACRO_SIZE 1024

/* Read len bytes of the macro buffer at offset (zero-filled past the end). */
void via_macro_read(uint16_t offset, uint8_t len, uint8_t *out);

/* Write len bytes at offset; the buffer is saved to flash shortly after the
 * last write and any macro that is playing is stopped. */
void via_macro_write(uint16_t offset, uint8_t len, const uint8_t *in);

/* Erase every macro. */
void via_macro_reset(void);

#endif /* VIA_MACRO_H */
