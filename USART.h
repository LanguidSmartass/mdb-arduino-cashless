/*
 *
 *
 *
*/

#ifndef USART_H
#define USART_H

#include <inttypes.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#ifdef __cplusplus
extern "C" {
#endif

void USART_Init (uint16_t  baud_rate);

void USART_TXBuf_Write (uint16_t  data);
void USART_RXBuf_Read  (uint16_t *data);

void USART_Transmit (void);
void USART_Receive  (void);

void USART_UDRI_Enable  (void);
void USART_UDRI_Disable (void);

#endif /* USART_H */

#ifdef __cplusplus
    }
#endif