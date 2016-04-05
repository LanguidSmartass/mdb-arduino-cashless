#include <MDB.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

// SoftwareSerial for debugging
//#define SOFTWARESERIAL_DEBUG_ENABLED 1

#ifdef SOFTWARESERIAL_DEBUG_ENABLED
#define RX_DEBUG_PIN    2
#define TX_DEBUG_PIN    3
#endif

// SIMCOM GPRS Module Settings
#define SIMCOM_RX_PIN   7
#define SIMCOM_TX_PIN   8
#define SIMCOM_PW_PIN   9
// SPI Slaves Settings
#define RC522_SLS_PIN   4     // MFRC522 SlaveSelect Pin
#define RC522_RST_PIN   5     // SPI RESET Pin


typedef enum {GET, POST, HEAD} HTTP_Method;    // 'keys' for HTTP functions

#ifdef SOFTWARESERIAL_DEBUG_ENABLED
SoftwareSerial Debug(RX_DEBUG_PIN, TX_DEBUG_PIN);
#endif

// SIMCOM module instance
SoftwareSerial SIM900(SIMCOM_RX_PIN, SIMCOM_TX_PIN);
// Create MFRC522 instance
MFRC522 mfrc522(RC522_SLS_PIN, RC522_RST_PIN);
// String object for containing HEX represented UID
String uid_str_obj = "";
// Store the time of CSH_S_ENABLED start
uint32_t global_time = 0;
// Static Server port and address
String server_url_str = "http://xxx.xxx.xxx.xxx:xxx/api/Vending/";
//uint8_t in_progress = 0;

void setup() {
    // For debug LEDs
    DDRC |= 0b00111111; // PORTC LEDs for 6 commands in MDB_CommandHandler() switch-case

#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.begin(9600);
    Debug.println(F("Debug mode enabled"));
#endif
    // == Init card reader START ==
    SPI.begin();        // Init SPI bus, MANDATORY
    mfrc522.PCD_Init();
    // == Init card reader END ==
    SIM900_Init();
    MDB_Init();
    sei();
}

void loop() {
    MDB_CommandHandler();
    sessionHandler();
    // without this delay READER_ENABLE command won't be in RX Buffer
    // Establish the reason of this later (maybe something is wrong with RX buffer reading)
    if (CSH_GetDeviceState() == CSH_S_DISABLED)
        delay(10);
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
        case CSH_S_ENABLED      : RFID_readerHandler(); break;
        // check timeout
        case CSH_S_SESSION_IDLE : timeout(global_time, FUNDS_TIMEOUT * 1000); break;
        case CSH_S_VEND         : transactionHandler(); break;
        default : break;
    }
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
    String old_uid_str_obj;
    String new_uid_str_obj;
    
    String http_request;
    uint16_t status_code = 0;
    uint16_t user_funds  = 0;

    // Look for new cards
    if ( ! mfrc522.PICC_IsNewCardPresent())
        return;

    // Select one of the cards
    if ( ! mfrc522.PICC_ReadCardSerial())
        return;
//    in_progress = 0;
    // Convert UID into string represented HEX number
    // in reversed order (for human recognition)
    getUIDStrHex(&mfrc522, &new_uid_str_obj);
    
    // Check if there is no UID -- then just return
    if (new_uid_str_obj.length() == 0)
        return;
    // Check if previous UID == UID that was just read
    // to prevent an infinite HTTP GET requests
    if (uid_str_obj == new_uid_str_obj)
        return;
    // Now update the global UID
    uid_str_obj = new_uid_str_obj;
    
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.println(uid_str_obj);
#endif

    // HTTP handling
    http_request = server_url_str + uid_str_obj;
    submitHTTPRequest(GET, http_request, &status_code, &user_funds);

#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.print(F("HTTP Status Code: "));
    Debug.println(status_code);
#endif
    
    if (status_code == 200)
    {
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
        Debug.print(F("Funds: "));
        Debug.println(user_funds);
        Debug.println(F("Poll State: Begin Session"));
#endif
        CSH_SetUserFunds(user_funds); // Set current user funds
        CSH_SetPollState(CSH_BEGIN_SESSION); // Set Poll Reply to BEGIN SESSION
        CSH_SetDeviceState(CSH_S_PROCESSING);
        global_time = millis();
        return;
    }
    // If we got here that means we didn't receive permission for transaction
    // so clear global UID string and global_time
    uid_str_obj = "";
    global_time = 0;
    /*
     * SET FUNDS AND CONNECTION TIMEOUT SOMEWHERE, TO CANCEL SESSION AFTER 10 SECONDS !!!
     */
}
/*
 * Transaction routine
 * At this point item_cost and item_number (or vend_amount)
 * are should be set by VendRequest() (in MDB.c)
 */
void transactionHandler(void)
{
    String http_request;
    uint16_t status_code = 0;
    uint16_t user_funds  = 0;
        
    uint16_t item_cost = CSH_GetItemCost();
    uint16_t item_numb = CSH_GetVendAmount();
    
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.println(F("Vend Request"));
#endif

    http_request = server_url_str + uid_str_obj + "/" + String(item_cost);
    uid_str_obj = "";
    global_time = 0;
    submitHTTPRequest(POST, http_request, &status_code, &user_funds);
    
    if (status_code != 202)
    {
        CSH_SetPollState(CSH_VEND_DENIED);
        CSH_SetDeviceState(CSH_S_PROCESSING);
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
        Debug.println(F("Vend Denied"));
#endif
        return;
    }
    // If code is 202 -- tell VMC that vend is approved
    CSH_SetPollState(CSH_VEND_APPROVED);
    CSH_SetDeviceState(CSH_S_PROCESSING);
//    in_progress = 1;
    
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.println(F("Vend Approved"));
#endif
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

void setUID(String uid_str)
{
    
}
String getUID(void)
{
    
}

/*
 * Send HTTP request
 * input arguments: enum HTTPMethod -- GET, POST or HEAD
 *                  String request -- request string of structure http://server_address:server_port/path
 *                  uint8_t  *http_status_code -- returned http code by pointer
 *                  uint16_t *user_funds -- returned user funds by pointer
 * returned value:  none
 */
void submitHTTPRequest(HTTP_Method method, String http_link, uint16_t *status_code, uint16_t *user_funds)
{
    String response;
    uint8_t start_index;
    uint8_t end_index;
//    uint8_t data_len;
    // Commands for maitainance
//    SIM900_SendCommand(F("AT"));
//    SIM900_SendCommand(F("CPIN?"));
//    SIM900_SendCommand(F("CSQ"));
//    SIM900_SendCommand(F("CREG?"));
//    SIM900_SendCommand(F("CGATT?"));
    
    // TCP config
//    SIM900_SendCommand(F("CIPSHUT"));
//    SIM900_SendCommand(F("CSTT=\"internet.beeline.ru\",\"beeline\",\"beeline\""));
//    SIM900_SendCommand(F("CIICR"));
//    SIM900_SendCommand(F("CIFSR"));
//    SIM900_SendCommand(F("CIPSTATUS"));

    // Make connection and HTTP request
    SIM900_SendCommand(F("SAPBR=1,1")); // Enable bearer
    SIM900_SendCommand(F("HTTPINIT"));
    SIM900_SendCommand("HTTPPARA=\"URL\",\"" + http_link + "\""); // cant put these two strings into flash memory
    SIM900_SendCommand(F("HTTPPARA=\"CID\",1"));
    
    switch (method)
    {   //submit the request
        case GET  : response = SIM900_SendCommand(F("HTTPACTION=0")); break; // GET
        case POST : response = SIM900_SendCommand(F("HTTPACTION=1")); break; // POST
//        case HEAD : response = SIM900_SendCommand(F("HTTPACTION=2")); break; // HEAD // not needed for now, disabled to free Flash memory
        default : return;
    } // optimize for flash/RAM space, HTTPACTION repeats 3 times
    /*
     * Read the HTTP code
     * Successful response is of the following format (without angular brackets):
     * OK\r\n+HTTPACTION:<method>,<status_code>,<datalen>\r\n
     * so search for the first comma
     */
    start_index = response.indexOf(',') + 1;
    end_index = response.indexOf(',', start_index);
    *status_code = (uint16_t)response.substring(start_index, end_index).toInt();
    
    if (*status_code == 200) // read the response
    {
        /*
         * Successful response is of the following format:
         * AT+HTTPREAD\r\n+HTTPREAD:<datalen>,<data>\r\nOK\r\n
         * so search for the first ':', read data_len until '\r'
         * then read the data itself
         */
        response = SIM900_SendCommand(F("HTTPREAD"));
        start_index = response.indexOf(':') + 1;
        end_index = response.indexOf('\r', start_index);
//        data_len = (uint8_t)response.substring(start_index, end_index).toInt();
        // now parse string from a new start_index to start_index + data_len
        start_index = response.indexOf('\n', end_index) + 1;
//        *user_funds = (uint16_t)response.substring(start_index, start_index + data_len).toInt();
//      // or until '\r', which takes less Flash memory
        end_index = response.indexOf('\r', start_index);
        *user_funds = (uint16_t)response.substring(start_index, end_index).toInt();
    }
    // These two are must be present in the end
    SIM900_SendCommand(F("HTTPTERM")); // Terminate session, mandatory
    SIM900_SendCommand(F("SAPBR=0,1")); // Disable bearer, mandatory
}
/*
 * Initiate SIMCOM SIM900 Module
 */
void SIM900_Init(void)
{
    SIM900_PowerON();
    SIM900.begin(19200);
    // HTTP Config
    SIM900_SendCommand(F("SAPBR=3,1,\"CONTYPE\",\"GPRS\"")); //setting the SAPBR, the connection type is using gprs
    SIM900_SendCommand(F("SAPBR=3,1,\"APN\",\"internet.beeline.ru\"")); //setting the APN, the second need you fill in your local apn server
}
/*
 * Turn module ON (not global power, only functionality)
 * Arguments:
 * uint8_t fun : 0 -- Minimum functionality
 *               1 -- Full functionality
 *               4 -- Disable phone transmit and receive RF circuits
 * uint8_t rst : 0 -- Do not reset the ME before setting it to <fun> power level
 *               1 -- Reset the ME before setting it to <fun> power level
 * RetVal: none
 */
void SIM900_TurnOn(uint8_t fun, uint8_t rst)
{
    String command = F("CFUN=");
    String send_comm;
    switch (fun)
    {
        case 0 : send_comm = command + "0"; break;
        case 1 : send_comm = command + "1"; break;
        case 4 : send_comm = command + "4"; break;
        default : return;
    }
    send_comm += ",";
    switch (rst)
    {
        case 0 : send_comm += "0"; break;
        case 1 : send_comm += "1"; break;
        default : return;
    }
    SIM900_SendCommand(send_comm); // Turn module ON: 1 - full functionality, do not reset
}
/*
 * Send command to GSM/GPRS modem
 * accepts AT command (SIMCOM modules)
 * returns response String object
 */
String SIM900_SendCommand(String command)
{
    String at = F("AT+");
    SIM900.println(at + command);
    return verifyResponse(command);
}
/*
 * Wait for reply for the command from SIMCOM module and return it
 * returns the reply (String object)
 */
String verifyResponse(String command)
{
    String buff_str;
    String word_chk;
    while ( !(SIM900.available()) )
        ; // wait for data on the bus
//    delay(300); // this delay may be needed, but for now everything works without it
    
    if (command.indexOf(F("CIFSR")) != -1)
        word_chk = F("."); // because there is no OK response to this command, only IP address xxx.xxx.xxx.xxx, which always contains dots
    else if (command.indexOf(F("HTTPACTION")) != -1)
        word_chk = F("+HTTPACTION:"); // because after OK there is still a message to wait
    else
        word_chk = F("OK");
        
    while (1)
    {
        if (SIM900.available())
        {
            buff_str += char(SIM900.read());
            if ( (buff_str.indexOf(word_chk) != -1) && (buff_str.endsWith(F("\r\n"))) )
                break; // wait for "+HTTPACTION:num,num,num\r\n" reply
            else if ( buff_str.indexOf(F("ERROR")) > 0 )
                break;
        }
    }
    // debug
//    Serial.println(buff_str);
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.println(buff_str);
#endif
    return buff_str;
}

/*
 * Turn SIM900 Power ON
 */
void SIM900_PowerON(void)
{
    pinMode(SIMCOM_PW_PIN, OUTPUT);
    digitalWrite(SIMCOM_PW_PIN, LOW);
    delay(1000);
    digitalWrite(SIMCOM_PW_PIN, HIGH);
    delay(2000);
    digitalWrite(SIMCOM_PW_PIN, LOW);
    delay(3000);
}

/*
 * Turn SIM900 Power OFF
 * Argument:
 * uint8_t mode: 0 -- turn off Urgently (won't send anything back)
 *               1 -- turn off Normally (will send Normal Power Down)
 * RetVal: none
 */
void SIM900_PowerOff(uint8_t mode)
{
    String command = F("AT+CPOWD=");
    switch (mode)
    {
        case 0 : SIM900.println    (command + "0"); break; // println because we don't wait for any response
        case 1 : SIM900_SendCommand(command + "1"); break;
        default : return;
    }
}
/*
 * Checks if timeout has ran off
 * Simply compares start_time with timeout_value
 * Calls terminateSession() if time ran out
 * Can only be called in CSH_S_SESSION_IDLE state
 */
void timeout(uint32_t start_time, uint32_t timeout_value)
{
    if (CSH_GetDeviceState() != CSH_S_SESSION_IDLE)
        return;
//    if (in_progress == 1)
//        return;
    if (millis() - start_time >= timeout_value)
        terminateSession();
}
/*
 * Terminates current session and sets device back to ENABLED state
 * This one called when timeout ran out or return button was pressed
 */
void terminateSession(void)
{
    CSH_SetPollState(CSH_SESSION_CANCEL_REQUEST);
    CSH_SetDeviceState(CSH_S_ENABLED);
    uid_str_obj = "";
    global_time = 0;
#ifdef SOFTWARESERIAL_DEBUG_ENABLED
    Debug.println(F("Session terminated"));
#endif
}

