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
#define CSH_S_INACTIVE     0x00
#define CSH_S_DISABLED     0x01
#define CSH_S_ENABLED      0x02
#define CSH_S_SESSION_IDLE 0x03
#define CSH_S_VEND         0x04
#define CSH_S_PROCESSING   0xFF // this one is to avoid 

// typedef enum {
//     INACTIVE,
//     DISABLED,
//     ENABLED,
//     SESSION_IDLE,
//     VEND  
// } CSH_State_t;
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
 * MDB VMC Subcommands
 */
// VMC_SETUP
#define VMC_CONFIG_DATA    0x00
#define VMC_MAX_MIN_PRICES 0x01
// VMC_VEND
#define VMC_VEND_REQUEST          0x00
#define VMC_VEND_CANCEL           0x01
#define VMC_VEND_SUCCESS          0x02
#define VMC_VEND_FAILURE          0x03
#define VMC_VEND_SESSION_COMPLETE 0x04
#define VMC_VEND_CASH_SALE        0x05
// VMC_READER
#define VMC_READER_DISABLE 0x00
#define VMC_READER_ENABLE  0x01
#define VMC_READER_CANCEL  0x02
// VMC_EXPANSION
#define VMC_EXPANSION_REQUEST_ID  0x00
#define VMC_EXPANSION_DIAGNOSTICS 0xFF
/*
 * MDB Level 01 Cashless device Replies
 * These are to be written in USART TX Buffer
 * Store them with MDB_Send
 * Don't change, written as in standard
 */
#define CSH_ACK                     0x0100 // Acknowledgement, Mode-bit is set
#define CSH_NAK                     0x01FF // Negative Acknowledgement
#define CSH_SILENCE                 0xFFFF // This one is not from standard, it's an impossible value for the VMC
#define CSH_JUST_RESET              0x00
#define CSH_READER_CONFIG_INFO      0x01
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

/* These one goes to main.c or .ino sketch */
void MDB_Init (void);
void MDB_CommandHandler (void);
/* Functions to work with MDB Bus */
void MDB_Send (uint16_t  data);
void MDB_Read (uint16_t *data);
void MDB_Peek (uint16_t *data);
uint8_t MDB_DataCount (void);
/* Functions to work with Cashless device states and data */
uint8_t  CSH_GetPollState   (void);
uint8_t  CSH_GetDeviceState (void);
uint16_t CSH_GetUserFunds   (void);
uint16_t CSH_GetItemCost    (void);
uint16_t CSH_GetVendAmount  (void);
void CSH_SetPollState   (uint8_t  new_poll_state);
void CSH_SetDeviceState (uint8_t  new_device_state);
void CSH_SetUserFunds   (uint16_t new_funds);
void CSH_SetItemCost    (uint16_t new_item_cost);
void CSH_SetVendAmount  (uint16_t new_vend_amount);
/* Internal functions for MDB_CommandHandler */
static void MDB_ResetHandler     (void);
static void MDB_SetupHandler     (void);
static void MDB_PollHandler      (void);
static void MDB_VendHandler      (void);
static void MDB_ReaderHandler    (void);
static void MDB_ExpansionHandler (void);

/* Internal functions for upper handlers */
static void Reset (void);
static void ConfigInfo (void);
static void DisplayRequest (void);
static void BeginSession (void);
static void EndSession (void);
static void Cancelled (void);
static void PeripheralID (void);
static void MalfunctionError (void);
static void CmdOutOfSequence (void);
static void DiagnosticResponse (void);
static void VendRequest (void); // <<<=== Important function
static void VendApproved (void);
static void VendDenied (void);
static void VendSuccessResponse (void);
static void VendFailureHandler (void);
static void Disable (void);
static void Enable (void);
static void ExpansionRequestID (void);
static void ExpansionDiagnostics (void);
/* Internal helper functions */
static uint8_t calc_checksum (uint8_t *array, uint8_t arr_size);

#ifdef __cplusplus
    }
#endif

#endif /* MDB_LIB_H */