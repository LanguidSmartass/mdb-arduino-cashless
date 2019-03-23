# mdb-arduino-cashless
Arduino "Cashless Device" project for MDB protocol

Target configuration:
1) Arduino Uno (ATmega 328P)
2) MFRC522 13.56MHz RFID Reader
3) Ethernet Shield W5100
4) Vending Machine Controller, MultiDropBus Protocol (https://www.ccv.eu/wp-content/uploads/2018/05/mdb_interface_specification.pdf)

Main features:
1) Target is a device of "Cashless Device" class.
2) Hardware USART of Arduino Uno configured for 9-bit word communication (9600 baud, 9 data bits, no parity, 1 stop-bit).
3) Payment is based on 13.56MHz RFID cards.
4) Balance of the card is stored on the HTTP Server.
4) Device makes HTTP request to Server to make a payment and then vend item in case of success.
