#include <MDB.h>
#include <SPI.h>
#include <Ethernet.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>
// SoftwareSerial pins for debugging
#define RX_DEBUG_PIN    6
#define TX_DEBUG_PIN    7
// Debug object
SoftwareSerial Debug(RX_DEBUG_PIN, TX_DEBUG_PIN);

// SPI Slaves Settings
#define RESET_PIN       9     // SPI RESET Pin
#define SS_PIN_RC522    8     // MFRC522 SS Pin
#define SS_PIN_SDCRD    4     // SD Card SS Pin (ethernet shield SD card)
#define SS_PIN_W5100    10    // W5100   SS Pin (ethernet shield Ethernet)

typedef enum {RC522, SDCARD, W5100} SPISlave;  // For function 'enableSPISlave(SPISlave slave)'

// Create MFRC522 instance
MFRC522 mfrc522(SS_PIN_RC522, RESET_PIN);
// String object for containing HEX represented UID
String uid_str_obj = "";
/*
 * Ethernet Settings
 * client_mac -- mandatory
 * client_ip  -- in case of failed DHCP mode, use static IP
 * A server we connect to is a 'client' to us
 */
uint8_t client_mac[] = {
    0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02
};
IPAddress client_ip(192, 168, 137, 232); // Static IP Mode in case of failed DHCP Mode
IPAddress server_address(192, 168, 1, 139);
uint16_t server_port = 8888;
EthernetClient server;

void setup() {
    // For debug LEDs
    DDRC |= 0b00111111; // PORTC LEDs for 6 commands in MDB_CommandHandler() switch-case
    Debug.begin(9600);
    MDB_Init();
    sei();    
    // Init Ethernet Shield    
    enableSPISlave(W5100);
    if (Ethernet.begin(client_mac) == 0)  // Start in DHCP Mode
        Ethernet.begin(client_mac, client_ip); // If DHCP Mode failed, start in Static Mode
    
    // Give the Ethernet shield a second to initialize:
    delay(1000);
    unsigned long start_time = millis();
    
    Debug.println(F("Ethernet ON"));
    // Init MFRC522 RFID reader
    enableSPISlave(RC522);
    mfrc522.PCD_Init();
}

void loop() {
//    Debug.println(MDB_DataCount());
    MDB_CommandHandler();
    sessionHandler();
    // without this delay READER_ENABLE command won't be in RX Buffer
    // Establish the reason of this later (maybe something is wrong with RX buffer reading)
    if (CSH_GetDeviceState() == CSH_S_DISABLED)
        delay(10);
//    switch (c)
//    {
//        case 0x30 : CSH_SetPollState(CSH_SESSION_CANCEL_REQUEST); break;
//        case 0x31 : CSH_SetPollState(CSH_END_SESSION);    break;
//        default : break;
//    }        
}

/*
 * Handler for two Cashless Device active states:
 * ENABLED -- device is waiting for a new card
 * VEND    -- device is busy making transactions with the server
 */
void sessionHandler(void)
{
    switch(CSH_GetDeviceState())
    {
        case CSH_S_ENABLED : RFID_readerHandler(); break;
        case CSH_S_VEND    : transactionHandler(); break;
        default : break;
    }
//    char c = Debug.read();
//    if (c == 0x30)
//        CSH_SetPollState(CSH_END_SESSION);
}
/*
 * Waiting for RFID tag routine
 * I a new card is detected and it is present in the server's database,
 * server replies with available funds
 * then set them via functions from MDB.h,
 * and turn Cashless Device Poll Reply as BEGIN_SESSION
 */
void RFID_readerHandler(void)
{
    String new_uid_str_obj;
    // String objects and indexes for parsing HTTP Response
    String http_response;
    String http_code;
    uint8_t start_index = 0;
    uint8_t end_index = 0;
    uint16_t user_funds = 0;
    // Look for new cards
    enableSPISlave(RC522);
    if ( ! mfrc522.PICC_IsNewCardPresent())
        return;
    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())
        return;
    
    new_uid_str_obj = "";
    getUIDStrHex(&mfrc522, &new_uid_str_obj);
    if (uid_str_obj == new_uid_str_obj)
        return;
    
    uid_str_obj = new_uid_str_obj;
    // Check if there is no UID -- then just return
    if (uid_str_obj.length() == 0)
        return;
    
    Debug.println(uid_str_obj);
    enableSPISlave(W5100);
    // wait a second
    delay(2000);
    if (server.connect(server_address, server_port))
    {
        // Make HTTP request
        server.print(F("GET /api/getrecord/"));
        server.print(uid_str_obj);
        server.print(F("/Funds"));
        server.println(F(" HTTP/1.1"));
        server.println();
    }
    
    while (true)
    {
        if (server.available())
            http_response += (char)server.read();
        else if (!server.connected())
        {
            server.stop();
            break;
        }
    }
    
    end_index = http_response.indexOf('\n');
    // Get header string, then get HTTP Code
    http_code = http_response.substring(0, end_index).substring(9, 12);
    if (http_code != F("200"))
    {   
        // Maybe tell VMC to display message 'Card is not registered in the database'
        uid_str_obj = ""; // Clear global UID holder
        return;
    }        
        
    // Now parse for funds available
    start_index = http_response.indexOf('\"') + 1; // +1, or we going to start with \" symbol
    end_index   = http_response.indexOf('\"', start_index);
    user_funds  = http_response.substring(start_index, end_index).toInt(); 
    Debug.print(F("Funds: "));
    Debug.println(user_funds);
    
    Debug.println(F("Poll State: Begin Session"));
    CSH_SetUserFunds(user_funds); // Set current user funds
    CSH_SetPollState(CSH_BEGIN_SESSION); // Set Poll Reply to BEGIN SESSION
    /*
     * SET FUNDS AND CONNECTION TIMEOUT SOMEWHERE, TO CANCEL SESSION AFTER 5 SECONDS !!!
     */
}
/*
 * Transaction routine
 * At this point item_cost and item_number (or vend_amount)
 * are should be set by VendRequest() (in MDB.c)
 */
void transactionHandler(void)
{
    // String objects and indexes for parsing HTTP Response
    String http_response;
    String http_code;
    uint8_t end_index = 0;
    
    uint16_t item_cost = CSH_GetItemCost();
    uint16_t item_numb = CSH_GetVendAmount();
    
    Debug.println(F("Vend Request"));;
    enableSPISlave(W5100);
    if (server.connect(server_address, server_port))
    {
        // Make HTTP request
        // GET /api/getrecord/UID/Funds/VendRequest/item_cost/item_numb HTTP/1.1
        server.print(F("GET /api/getrecord/"));
        server.print(uid_str_obj);
        server.print(F("/Funds/VendRequest/"));
        server.print(String(item_cost));
        server.print(F("/"));
        server.print(String(item_numb));
        server.println(F(" HTTP/1.1"));
        server.println();
    }
    // Clear global UID variable
    uid_str_obj = "";
    while (true)
    {
        if (server.available())
            http_response += (char)server.read();
        else if (!server.connected())
        {
            server.stop();
            break;
        }
    }
    
    end_index = http_response.indexOf('\n');
    // Get header string, then get HTTP Code
    http_code = http_response.substring(0, end_index).substring(9, 12);
    // If response is not 200 (or 403), tell VMC that vend is denied and abort
    if (http_code != F("200"))
    {
        CSH_SetPollState(CSH_VEND_DENIED);
        CSH_SetDeviceState(CSH_S_SESSION_IDLE);
        Debug.println(F("Vend Denied"));
        return;
    }
    // If code is 200 -- tell VMC that vend is approved
    CSH_SetPollState(CSH_VEND_APPROVED);
    CSH_SetDeviceState(CSH_S_SESSION_IDLE);
    Debug.println(F("Vend Approved"));
}

/*
 * 20-02-2016
 * Convert byte array with size of (4..10) 8-bit hex numbers into a string object
 * Example: array of 4 hex numbers, which represents 32-bit unsigned long:
 * {0x36, 0x6B, 0x1B, 0xDB} represents a 32-bit (4-word) number 0xDB1B6B36, which is converted into a string "63B6B1BD"
 * This string represented hex number appears reversed for human recognition,
 * although technically the least 4-bit hex digit accords to the first letter of a string, uuid_str[0]
 *       and the most significant 4-bit hex digit placed as a last char element (except for '\0')
 *       
 * changes the String &uid_str object by pointer, considered as output
 */
void getUIDStrHex(MFRC522 *card, String *uid_str)
{
    char uuid_str[2 * card->uid.size];
    
    // This cycle makes a conversion of the following manner for each 8-bit number: {0x1B} --> {0x0B, 0x01}
    for (byte i = 0; i < card->uid.size; ++i) {
        uuid_str[2 * i] = card->uid.uidByte[i] & 0b1111;
        uuid_str[2 * i + 1] = card->uid.uidByte[i] >> 4;
    }
    //uuid_str[2 * card->uid.size + 1] = '\0';  // Add a null-terminator to make it a C-style string

    /* 
     *  This cycle adds 0x30 or (0x41 - 0x0A) to actual numbers according to ASCII table 
     *  0x00 to 0x09 digits become '0' to '9' chars (add 0x30)
     *  0x0A to 0x0F digits become 'A' to 'F' chars in UPPERCASE (add 0x41 and subtract 0x0A)
     *  Last thing is to copy that into a String object, which is easier to handle
     */
    for (byte i = 0; i < 2 * card->uid.size; ++i)
    {
        if (uuid_str[i] < 0x0A)
            uuid_str[i] = (uuid_str[i] + 0x30);
        else
            uuid_str[i] = (uuid_str[i] - 0x0A + 0x41);
        *uid_str += uuid_str[i];
    }
}

/* 
 * 20-02-2016
 * Enables selected SPI slave, disables all others
 * All slave SS pins must be #defined, slave must be of enumerated type
 */
void enableSPISlave(SPISlave slave)
{
    // Automatically disable all other SPI Slaves    
    switch(slave)
    {
        case RC522 :
        {
            pinMode(SS_PIN_RC522, OUTPUT);
            digitalWrite(SS_PIN_RC522, LOW);
            digitalWrite(SS_PIN_SDCRD, HIGH);
            digitalWrite(SS_PIN_W5100, HIGH);
            break;
        }
        case SDCARD :
        {
            pinMode(SS_PIN_SDCRD, OUTPUT);
            digitalWrite(SS_PIN_SDCRD, LOW);
            digitalWrite(SS_PIN_RC522, HIGH);
            digitalWrite(SS_PIN_W5100, HIGH);
            break;
        }
        case W5100 :        
        {
            pinMode(SS_PIN_W5100, OUTPUT);
            digitalWrite(SS_PIN_W5100, LOW);
            digitalWrite(SS_PIN_RC522, HIGH);
            digitalWrite(SS_PIN_SDCRD, HIGH);
            break;
        }
        default : break;
    }  
}
