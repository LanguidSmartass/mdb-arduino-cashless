/*          mm.dd.yyyy
 * Created: 02.26.2016 by Novoselov Ivan
 * ATmega328P (Arduino Uno)
 * For 9-bit, no parity, 1 stop-bit config only
 *
 */
// #include <stdlib.h>
// #include <stdio.h>
// #include <string.h>
// #include <inttypes.h>
#include "Arduino.h"

#include "USART.h"
#include "ringBuf.h"

ringBuf_t usartReceiveBuf;
ringBuf_t usartTransmitBuf;
/*
 * Initialize UART
 *
 */
void USART_Init(uint16_t baud_rate)
{
    uint16_t baud_setting = 0;
    // baud_setting = (F_CPU / 8 / baud_rate - 1) / 2;   // full integer equialent of (F_CPU / (16 * baud_rate) - 1)
    UCSR0A |= (1 << U2X0); // try double speed
    baud_setting = (F_CPU / 4 / baud_rate - 1) / 2;

    /* Set Baud Rate */
    UBRR0H = (uint8_t)(baud_setting >> 8);
    UBRR0L = (uint8_t)(baud_setting &  0x00FF);
    /* Enable Receiver and Transmitter */
    UCSR0B |= (1 << RXEN0) | (1 << TXEN0);
    /* Enable Interrupt on Recieve Complete */
    UCSR0B |= (1 << RXCIE0);
    /* Set Frame Format, 9-bit word, no parity, 1 stop-bit */
    UCSR0B |= (1 << UCSZ02);
    UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00);

    rBufInit(&usartReceiveBuf);
    rBufInit(&usartTransmitBuf);
}
/*
 * Write data to usartTransmitBuf and update head.
 * Then enable Data Register Empty Interrupt.
 * USART_Transmit will handle the transmission itself (look into USART_ISR.c)
 */
void USART_TXBuf_Write(uint16_t  data)
{
    rBufPushFront(&usartTransmitBuf, data);
    USART_UDRI_Enable();
}
/*
 * Read data to usartTransmitBuf
 * USART_Receive handles actual reception of the data (look into USART_ISR.c)
 */
void USART_RXBuf_Read(uint16_t *data)
{
    rBufPopBack(&usartReceiveBuf, data);
}
/*
 *
 */
void USART_RXBuf_Peek(uint16_t *data)
{
    rBufPeekBack(&usartReceiveBuf, data);
}

uint8_t USART_TXBuf_IsEmpty (void)
{
    return rBufIsEmpty(&usartTransmitBuf);
}

uint8_t USART_RXBuf_Count (void)
{
    return rBufElemCount(&usartReceiveBuf);
}

void USART_Transmit(void)
{
    uint16_t transmitVal;
    /* 
     * Wait for empty transmit buffer
     * When UDREn flag is 1 -- buffer is empty and ready to be written
    */
    // while ( !(UCSR0A & (1 << UDRE0)) )
    //     ;
    rBufPopBack(&usartTransmitBuf, &transmitVal);
    /* 
     *  Copy 9-th bit to TXB8n
     *  Clear this bit manually to prevent
     *  If data does not exceed 0x0100, don't do anything
    */
    UCSR0B &= ~(1 << TXB80);
    if (transmitVal & 0x0100)
        UCSR0B |= (1 << TXB80);
    /* Write the rest to UDRn register */
    UDR0 = (uint8_t)transmitVal;
}
/*
 * Read UDRn RX register with 9-th bit from UCSRnB
 */
void USART_Receive(void)
{
    uint8_t state, recH, recL;
    uint16_t data;

    // while ( !(UCSR0A & (1 << RXC0)) )
    //     ;
    state = UCSR0A;
    recH = UCSR0B;
    recL = UDR0;
    
    if ( state & (1 << UPE0) )
        return;
    
    recH = 0x01 & (recH >> 1);
    data = (recH << 8) | recL;
    
    rBufPushFront(&usartReceiveBuf, data);
}
/*
 * Enable Data Register Empty Interrupt * 
 */
void USART_UDRI_Enable(void)
{
    UCSR0B |= (1 << UDRIE0);
}
/*
 * Disable Data Register Empty Interrupt
 */
void USART_UDRI_Disable(void)
{
    UCSR0B &= ~(1 << UDRIE0);
}
