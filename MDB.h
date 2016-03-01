#ifndef MDB_H
#define MDB_H

#include <inttypes.h>

typedef struct
{
    uint8_t featureLevel;
    uint8_t displayColumns;
    uint8_t displayRows;
    uint8_t displayInfo;
} VMC_Config_t;

typedef struct
{
    uint8_t readerCongigData;
    uint8_t featureLevel;
    uint8_t countryCodeH;
    uint8_t countryCodeL;
    uint8_t scaleFactor;
    uint8_t decimalPlaces;
    uint8_t maxResponseTime; // seconds, overrides default NON-RESPONSE time
    uint8_t miscOptions;
} CSH_Config_t;

typedef struct
{
    uint16_t maxPrice;
    uint16_t minPrice;
} VMC_Prices_t;
/*
 * Level 01 Cashless (CSH) device states
 */
typedef enum {
    INACTIVE,
    DISABLED,
    ENABLED,
    SESSION_IDLE,
    VEND  
} CSH_State_t;
/*
 * Level 01 Cashless device address
 */
#define CSH_ADDRESS     0x110
/*
 * MDB Vending Machine Controller (VMC) Commands, 7 total
 * These are stored in USART RX Buffer
 * Read the command with MDB_Read and
 * compare to these definitions
 * Don't change, written as in standard
 */
#define VMC_RESET       0x110
#define VMC_SETUP       0x111
#define VMC_POLL        0x112
#define VMC_VEND        0x113
#define VMC_READER      0x114
#define VMC_EXPANSION   0x117
/*
 * MDB Level 01 Cashless device Replies
 * These are to be written in USART TX Buffer
 * Store them with MDB_Send
 * Don't change, written as in standard
 */
#define CSH_ACK                     0x100 // Acknowledgement, Mode-bit is set
#define CSH_JUST_RESET              0x00
#define CSH_READER_CONFIG_DATA      0x01
#define CSH_DISPLAY_REQUEST         0x02
#define CSH_BEGIN_SESSION           0x03
#define CSH_SESSION_CANCEL_REQUEST  0x04
#define CSH_VEND_APPROVED           0x05
#define CSH_VEND_DENIED             0x06
#define CSH_END_SESSION             0x07
#define CSH_CANCELLED               0x08
#define CSH_PERIPHERAL_ID           0x09
#define CSH_MALFUNCTION_ERROR       0x0A
#define CSH_CMD_OUT_OF_SEQUENCE     0x0B
#define CSH_DIAGNOSTIC_RESPONSE     0xFF

#ifdef __cplusplus
extern "C" {
#endif

/* This one goes to main.c or .ino sketch */
void MDB_CommandHandler (void);
/* Incoming VMC Commands handlers */
void MDB_ResetHandler  (void);
void MDB_SetupHandler  (void);
void MDB_PollHandler   (void);
void MDB_VendHandler   (void);
void MDB_ReaderHandler (void);
/* Functions for USART Buffers handling */
void MDB_Send (uint16_t  data);
void MDB_Read (uint16_t *data);
void MDB_Peek (uint16_t *data);
uint8_t MDB_DataCount (void);

#ifdef __cplusplus
    }
#endif

#endif /* MDB_LIB_H */