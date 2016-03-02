/*
 * MDB Vending Machine Controller emulation environment
 * for Level 01 Cashless Devices
 * Author: Novoselov I.E.
 * Email: jedi.orden@gmail.com
 */
#include <SoftwareSerial.h>
#include <USART.h>
#include <MDB.h>

// SoftwareSerial pins for debugging
#define RX_DEBUG_PIN    2
#define TX_DEBUG_PIN    3
// Debug object
SoftwareSerial Debug(RX_DEBUG_PIN, TX_DEBUG_PIN);

void setup() {
    Debug.begin(9600);
    USART_Init(9600);
    sei();
}

void loop() {
    uint8_t recComm = 0;
    uint8_t bytesAvailable = 0;
    char new_session[] = "=====NEW SESSION=====";
    
    if (bytesAvailable = Debug.available())
    {
        recComm = Debug.read();
        Debug.println();
        Debug.println(new_session);
        Debug.print(F("Bytes available : 0d"));
        Debug.println(bytesAvailable, DEC);
        Debug.print(F("Incoming command : 0x"));
        Debug.println(recComm, HEX);
    }
    switch(recComm)
    {
        case 0x30 : vmcReset();     break;
        case 0x31 : vmcSetup();     break;        
        case 0x32 : vmcPoll();      break;        
        case 0x33 : vmcVend();      break;
        case 0x34 : vmcReader();    break;
        case 0x37 : vmcExpansion(); break;
        default : break;
    }
}
/* 
 * Emulates Reset and Initialize command 
 */
void vmcReset(void)
{
    uint16_t reply_reset = 0;
    Debug.println(F("VMC_RESET Call test"));
    MDB_Send(VMC_RESET);
    while (true)
        if (MDB_DataCount() > 0)
        {
            Debug.print(F("MDB_DataCount : "));
            Debug.println(MDB_DataCount());
            MDB_Read(&reply_reset);
            Debug.print(F("Cashless reply : "));
            Debug.print(F("0b"));
            Debug.println(reply_reset, HEX);
            break;
        }
}
/*
 * Emulates 2 SETUP commands in a row: 
 * SETUP - Reader Config Data, then
 * SETUP - Max / Min Prices
 */
void vmcSetup(void)
{
    uint8_t i; // counter
    uint8_t checksum;
    uint16_t commd_setup_H[6] = {VMC_SETUP, 0x00, 0x01, 0x0A, 0x04, 0x01};
    uint16_t commd_setup_L[6] = {VMC_SETUP, 0x01, 0xF1, 0xF2, 0xF3, 0xF4};
    uint16_t reply_setup_H[9];
    uint16_t reply_setup_L;
    
    Debug.println(F("VMC_SETUP Call test"));
    // Stage 1 -- Reader Config Data
    checksum = calcChecksum(commd_setup_H, 6);
    for (i = 0; i < 6; ++i)
        MDB_Send(commd_setup_H[i]);
    MDB_Send(checksum);
    Debug.print(F("Reader Config Checksum : 0x"));
    Debug.println(checksum, HEX);
    
    while (true)
        if (MDB_DataCount() > 8)
        {
            for (i = 0; i < 9; ++i)
                MDB_Read(reply_setup_H + i);
            break;          
        }
    
    Debug.print(F("Incoming Reader Config Data :"));
    for (i = 0; i < 9; ++i)
    {
        Debug.print(F(" 0x"));
         // write() cannot hold 16-bit ints, but print() can
        Debug.print(reply_setup_H[i], HEX);
//        Debug.print(reply_setup_H[i] >> 8, HEX);
//        Debug.print(F("_0x"));
//        Debug.print(reply_setup_H[i] & 0xFF, HEX);
    }
    Debug.println();
    
    // Stage 2 -- Max / Min Prices
    checksum = calcChecksum(commd_setup_L, 6);
    for (i = 0; i < 6; ++i)
        MDB_Send(commd_setup_L[i]);
    MDB_Send(checksum);
    while (true)
    {
        if (MDB_DataCount() == 0)
            continue;
        else
        {
            MDB_Read(&reply_setup_L);
            break;
        }                
    }
    
    Debug.print(F("Incoming Max / Min Prices reply : "));
    Debug.print(F("0x"));
    Debug.print(reply_setup_L, HEX);
    Debug.println();
}
/* 
 * Emulates POLL command
 */
void vmcPoll()
{
    uint8_t i; // counter
    uint16_t reply;
    uint16_t comm_poll = VMC_POLL;
    Debug.println(F("VMC_SETUP Call test"));
    MDB_Send(comm_poll);
    Debug.print(F("Incoming data :"));
    while (true)
    {
        if (MDB_DataCount() > 0)
        {
            MDB_Read(&reply);
            Debug.print(F(" 0x"));
            Debug.print(reply, HEX);
        }
    }
} 
/* 
 * Emulates VEND command 
 */
void vmcVend()
{
    
}
/* 
 * Emulates READER command 
 */
void vmcReader()
{
    uint8_t i; // counter
    uint8_t checksum = 0;    
    uint16_t reply[2];
    uint16_t comm_reader_0[2] = {VMC_READER, 0x00};
    uint16_t comm_reader_1[2] = {VMC_READER, 0x01};
    uint16_t comm_reader_2[2] = {VMC_READER, 0x02};
    
    Debug.println(F("VMC_READER Call test"));
    
    // Test 1 -- READER Disable
    checksum = calcChecksum(comm_reader_0, 2);
    for (i = 0; i < 2; ++i)
        MDB_Send(comm_reader_0[i]);
    MDB_Send(checksum);
    // Wait for reply
    while (true)
        if (MDB_DataCount() > 0)
            break;
    Debug.print(F("READER Disable reply :"));
    for (i = 0; i < 1; ++i)
    {
        MDB_Read(&reply[i]);
        Debug.print(F(" 0x"));
        Debug.print(reply[i], HEX);
    }        
    Debug.println();
    
    // Test 2 -- READER Enable
    checksum = calcChecksum(comm_reader_1, 2);
    for (i = 0; i < 2; ++i)
        MDB_Send(comm_reader_1[i]);
    MDB_Send(checksum);
    // Wait for reply
    while (true)
        if (MDB_DataCount() > 0)
            break;
    Debug.print(F("READER Disable reply :"));
    for (i = 0; i < 1; ++i)
    {
        MDB_Read(&reply[i]);
        Debug.print(F(" 0x"));
        Debug.print(reply[i], HEX);
    }        
    Debug.println();
    
    // Test 3 -- READER Cancelled
    checksum = calcChecksum(comm_reader_2, 2);
    for (i = 0; i < 2; ++i)
        MDB_Send(comm_reader_2[i]);
    MDB_Send(checksum);
    // Wait for reply
    while (true)
        if (MDB_DataCount() > 1)
            break;
    Debug.print(F("READER Disable reply :"));
    for (i = 0; i < 2; ++i)
    {
        MDB_Read(&reply[i]);
        Debug.print(F(" 0x"));
        Debug.print(reply[i], HEX);
    }        
    Debug.println();
}
/* 
 * Emulates EXPANSION command 
 */
void vmcExpansion()
{
    
}

uint8_t calcChecksum(uint16_t *array, uint8_t arr_size)
{
    uint8_t retVal = 0x00;
    uint8_t i;
    for (i = 0; i < arr_size; ++i)
        retVal += *(array + i);
    return retVal; 
}

