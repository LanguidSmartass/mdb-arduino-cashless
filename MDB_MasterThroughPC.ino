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
    uint8_t i; // counter
    uint8_t recComm = 0;
    uint8_t bytesAvailable = 0;
    uint8_t checksum;
    char new_session[] = "=====NEW SESSION=====";
    uint16_t reply_reset = 0;
    uint16_t commd_setup_H[6] = {VMC_SETUP, 0x00, 0x01, 0x0A, 0x04, 0x01};
    uint16_t commd_setup_L[6] = {VMC_SETUP, 0x01, 0xF1, 0xF2, 0xF3, 0xF4};
    uint16_t reply_setup_H[9];
    uint16_t reply_setup_L;
    
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
        case 0x30 : {
            MDB_Send(VMC_RESET);
            while (true)
            {
                if (MDB_DataCount() == 0)
                    continue;
                Debug.print(F("MDB_DataCount : "));
                Debug.println(MDB_DataCount());
                MDB_Read(&reply_reset);
                Debug.print(F("Cashless reply :"));
                Debug.print(F("0b"));
                Debug.print((uint8_t)(reply_reset >> 8), BIN);
                Debug.print(F("_0b"));
                Debug.print((uint8_t)(reply_reset & 0xFF), BIN);
                Debug.println();
            }
        }; break;

        case 0x31 : {
            // Stage 1 -- Reader Config Data
            checksum = calcChecksum(commd_setup_H, 6);
            for (i = 0; i < 6; ++i)
                MDB_Send(commd_setup_H[i]);
            MDB_Send(checksum);
            Debug.print(F("Reader Config Checksum :"));
            Debug.print(checksum, HEX);
            while (true)
            {
                if (MDB_DataCount() < 9)
                    continue;
                    
                for (i = 0; i < 9; ++i)
                    MDB_Read(reply_setup_H + i);
                Debug.println(F("_2_Checkpoint_2_"));
                break;
            }
            
            Debug.print(F("Incoming Reader Config Data :"));
            for (i = 0; i < 9; ++i)
            {
                Debug.print(F("_0x"));
                Debug.print(reply_setup_H[i] >> 8, HEX);
                Debug.print(F("_0x"));
                Debug.print(reply_setup_H[i] & 0xFF, HEX);
                Debug.print(F("  "));
            }
            Debug.println();
            
            // Stage 2 -- Max / Min Prices
            checksum = calcChecksum(commd_setup_L, 6);
            for (i = 0; i < 6; ++i)
                MDB_Send(commd_setup_L[i]);
            MDB_Send(checksum);
            Debug.println(F("_3_Checkpoint_3_"));
            while (true)
            {
                if (MDB_DataCount() == 0)
                    continue;
                else
                {
                    MDB_Read(&reply_setup_L);
                    Debug.println(F("_4_Checkpoint_4_"));
                    break;
                }                
            }
            
            Debug.print(F("Incoming Max / Min Prices reply :"));
            Debug.print(F("_0x"));
            Debug.print(reply_setup_L, HEX);
            Debug.println();
            
        }; break;
        
        case 0x32 : {
            
        }; break;
        
        case 0x33 : {
            
        }; break;
        
        case 0x34 : {
            
        }; break;
        
        case 0x37 : {
            
        }; break;

        default : break;
    }
}

uint8_t calcChecksum(uint16_t *array, uint8_t arr_size)
{
    uint8_t retVal = 0x00;
    uint8_t i;
    for (i = 0; i < arr_size; ++i)
        retVal += *(array + i);
    return retVal;    
}

