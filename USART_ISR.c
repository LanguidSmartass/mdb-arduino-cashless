#include "Arduino.h"
#include "USART.h"

ISR(USART_RX_vect)
{
    USART_Receive();
}

ISR(USART_UDRE_vect)
{   
    if ( ! (USART_TXBuf_IsEmpty()) )
    {   
        USART_Transmit();
        return;
    }
    USART_UDRI_Disable();
}
