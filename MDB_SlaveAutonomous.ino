//#include <SoftwareSerial.h>
#include <USART.h>
#include <MDB.h>

//// SoftwareSerial pins for debugging
//#define RX_DEBUG_PIN    2
//#define TX_DEBUG_PIN    3
//// Debug object
//SoftwareSerial Debug(RX_DEBUG_PIN, TX_DEBUG_PIN);

void setup() {
//    Debug.begin(9600);
    USART_Init(9600); 
    sei();
    DDRD  |= (1 << 5);
    DDRD  |= (1 << 6);
    DDRD  |= (1 << 7);
}

void loop() {
    MDB_CommandHandler();
}
