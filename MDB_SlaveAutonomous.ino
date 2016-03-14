#include <USART.h>
#include <MDB.h>
#include <SoftwareSerial.h>
// SoftwareSerial pins for debugging
#define RX_DEBUG_PIN    6
#define TX_DEBUG_PIN    7
// Debug object
SoftwareSerial Debug(RX_DEBUG_PIN, TX_DEBUG_PIN);

void setup() {
    Debug.begin(9600);
    USART_Init(9600); 
    sei();
    
    // For debug LEDs
    DDRB |= 0b00111111; // PORTC LEDs for 6 commands in MDB_CommandHandler() switch-case
    DDRD |= 0b00111100; // PORTD LEDs for more manual handling as a breakpoints
}

void loop() {
    uint8_t command = 0;
    uint8_t poll_state, csh_state;
    uint16_t retComm = 0;
    uint16_t item_cost, vend_amount;
    
    retComm = MDB_CommandHandler();
    CSH_GetPollState(&poll_state);
    CSH_GetCSHState(&csh_state);
    CSH_GetItemCost(&item_cost);
    CSH_GetVendAmount(&vend_amount);
    
    command = Debug.read();
    switch(command)
    {
        case 0x30 : CSH_SetPollState(CSH_ACK); break;
        case 0x31 : CSH_SetPollState(CSH_JUST_RESET); break;
        case 0x32 : CSH_SetPollState(CSH_BEGIN_SESSION); break;
        case 0x33 : CSH_SetPollState(CSH_VEND_APPROVED); break;            
        case 0x34 : CSH_SetPollState(CSH_VEND_DENIED); break;
        case 0x35 : CSH_SetPollState(CSH_END_SESSION); break;
        case 0x36 : CSH_SetPollState(CSH_CANCELLED); break;
        default : break;
    }

    if ((retComm >= 0x110) && (retComm <= 0x117))
    {
        Debug.print(" 0x");
        Debug.print(retComm, HEX);
//        Debug.print(" p_state: 0x");
//        Debug.print(poll_state, HEX);
//        Debug.print(" d_state: 0x");
//        Debug.print(csh_state,  HEX);
        Debug.print(" 0x");
        Debug.print(item_cost,  HEX);
        Debug.print(" 0x");
        Debug.print(vend_amount,  HEX);
        Debug.println();
    }    
}
