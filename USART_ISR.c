#include "Arduino.h"
#include "USART.h"

ISR(USART_RX_vect)
{
    USART_Receive();   
}

ISR(USART_UDRE_vect)
{	
	USART_Transmit();
	USART_UDRI_Disable();
}
