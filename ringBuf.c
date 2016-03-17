#include "ringBuf.h"

/*
 * Initialize ringBuf_t struct
 * ringBuf_t instance must be declared
 * before the call of this function
 */
void rBufInit(ringBuf_t *_this)
{   
    // memset(_this->buf, 0, RING_BUF_SIZE);
    // _this->head = 0;
    // _this->tail = 0;
    // or
    memset(_this, 0, sizeof(*_this));
}

/*
 * Set all data to zero (technically same as rBufInit)
 */
void rBufFlush(ringBuf_t *_this)
{
    memset(_this, 0, sizeof(*_this)); // or //rBufInit(_this);
}

/*
 * Push data to buf[head] and increment head
 * if head + 1 exceeds BUF_SIZE, null it
 * if head + 1, just return because buffer is full
 */
void rBufPushFront(ringBuf_t *_this, uint16_t data)
{
    // uint8_t next = _this->head + 1;
    // if (next >= RING_BUF_SIZE)
    //     next = 0;
    uint8_t next = (_this->head + 1) & RING_BUF_MASK;
    // Ring buffer is full
    if (rBufIsFull(_this)) // (next == _this->tail)
        return; // -1;

    _this->buf[_this->head] = data;
    _this->head = next;
}
/*
 * Pop buf[tail] element and increment tail
 * if tail == head, just return because buffer is empty
 * if tail exceeds BUF_SIZE, null it
 */
void rBufPopBack(ringBuf_t *_this, uint16_t *data)
{
    if (rBufIsEmpty(_this)) // (_this->head == _this->tail)
        return; // -1;

    *data = _this->buf[_this->tail];    // Store value by pointer
    _this->buf[_this->tail] = 0;        // Clear the data, optional
    // Update index with masking
    _this->tail = (_this->tail + 1) & RING_BUF_MASK;
}
/*
 * Read buf[tail] element, but do not move tail or overwrite the element
 */
void rBufPeekBack(ringBuf_t *_this, uint16_t *data)
{
    if (rBufIsEmpty(_this)) // (_this->head == _this->tail)
        return;
    *data = _this->buf[_this->tail];
}

uint8_t rBufIsEmpty(ringBuf_t *_this)
{
    return ( _this->head == _this->tail );
}

uint8_t rBufIsFull(ringBuf_t *_this)
{
    return ( _this->head + 1 == _this->tail );
}

uint8_t rBufElemCount(ringBuf_t *_this)
{
    return (( _this->head - _this->tail ) & RING_BUF_MASK);
}