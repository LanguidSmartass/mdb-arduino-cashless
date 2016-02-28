#ifndef RING_BUF_H
#define RING_BUF_H

#include <inttypes.h>

#define RING_BUF_SIZE    64
#define RING_BUF_MASK    (RING_BUF_SIZE - 1)

typedef struct
{
    uint16_t buf[RING_BUF_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} ringBuf_t;

#ifdef __cplusplus
    extern "C" {
#endif

void     rBufInit        (ringBuf_t *_this);
void     rBufFlush       (ringBuf_t *_this);

void     rBufPushFront   (ringBuf_t *_this, uint16_t  data);
void     rBufPopBack     (ringBuf_t *_this, uint16_t *data);

uint8_t  rBufIsEmpty     (ringBuf_t *_this);
uint8_t  rBufIsFull      (ringBuf_t *_this);

#ifdef __cplusplus
    }
#endif

#endif /* RING_BUF_H */
