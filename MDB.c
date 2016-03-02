#include "MDB.h"
#include "USART.h"
#include <Arduino.h> // For LED debug purposes

#define DEBUG_LED_PIN 7

VMC_Config_t vmc_config = {0, 0, 0, 0};
VMC_Prices_t vmc_prices = {0, 0};

uint16_t user_funds = 0;
uint8_t csh_error_code = 0;

uint16_t     csh_poll_state = CSH_ACK;
CSH_State_t  csh_state = INACTIVE; // Cashless Device State at power-up
CSH_Config_t csh_config = {
    0x01, // featureLevel
// Russia's country code is 810 or 643, which
// translates into 0x18, 0x10 or 0x16, 0x43
    0x18, // country code H
    0x10, // country code L
    0x01, // Scale Factor
    0x02, // Decimal Places
    0x05, // Max Response Time
    0x00  // Misc Options
};
/*
 *
 */
void MDB_CommandHandler(void)
{
    uint16_t command = 0;
    MDB_Read(&command);
    switch(command)
    {
        case VMC_RESET  : MDB_ResetHandler();  break;
        case VMC_SETUP  : MDB_SetupHandler();  break;
        case VMC_POLL   : MDB_PollHandler();   break;
        case VMC_VEND   : MDB_VendHandler();   break;
        case VMC_READER : MDB_ReaderHandler(); break;
        // case VMC_EXPANSION : ; break;
        default : break;
    }

}
/*
 * Handles Just Reset sequence
 */
void MDB_ResetHandler(void)
{
    // uint16_t retVal;
    // // Read command ftom buffer and move tail,
    // // otherwise MDB_CommandHandler will go in an infinite loop
    // MDB_Read(&retVal);
    READER_Reset();
}
/*
 * Handles Setup sequence
 */
void MDB_SetupHandler(void)
{
    uint8_t i; // counter
    uint8_t checksum = VMC_SETUP;
    uint16_t vmc_temp;
    uint8_t vmc_data[6];
    /*
     * wait for the whole frame
     * frame structure:
     * 1 subcommand + 4 vmc_config fields + 1 Checksum byte
     * 6 elements total
     */
    while (1)
        if (MDB_DataCount() > 5)
            break;
    // Store frame in array
    for (i = 0; i < 6; ++i)
    {
        MDB_Read(&vmc_temp);
        vmc_data[i] = (uint8_t)(vmc_temp & 0x00FF); // get rid of Mode bit if present
    }
    // calculate checksum excluding last read element, which is a received checksum 
    for (i = 0; i < 5; ++i)
        checksum += vmc_data[i];
    // compare calculated and received checksums
    if (checksum != vmc_data[5])
        return; // checksum mismatch, error
    
    // vmc_data[0] is a SETUP Config Data or Max/Min Prices identifier
    switch(vmc_data[0])
    {
        case 0x00 : {            
            // Store VMC data
            vmc_config.featureLevel   = vmc_data[1];
            vmc_config.displayColumns = vmc_data[2];
            vmc_config.displayRows    = vmc_data[3];
            vmc_config.displayInfo    = vmc_data[4];
            READER_ConfigInfo();
        }; break;

        case 0x01 : {
            // Store VMC Prices
            vmc_prices.maxPrice = ((uint16_t)vmc_data[1] << 8) | vmc_data[2];
            vmc_prices.minPrice = ((uint16_t)vmc_data[3] << 8) | vmc_data[4];
            // Send ACK
            MDB_Send(CSH_ACK);
            // Change state to DISABLED
            // csh_state = DISABLED;
        }; break;

        default : break;
    }
}
/*
 * Handles Poll replies
 */
void MDB_PollHandler(void)
{
    uint8_t i; // counter
    uint8_t checksum = VMC_POLL;
    uint16_t readCmd;

    MDB_Read(&readCmd);
    switch(csh_poll_state)
    {
        case CSH_JUST_RESET : {
            MDB_Send(CSH_JUST_RESET);
            READER_Reset();
            // csh_poll_state = CSH_JUST_RESET;
        }; break;

        case CSH_READER_CONFIG_INFO : READER_ConfigInfo();     break;
        case CSH_DISPLAY_REQUEST    : READER_DisplayRequest(); break;
        case CSH_BEGIN_SESSION      : READER_BeginSession();   break;

        case CSH_SESSION_CANCEL_REQUEST : {
            MDB_Send(CSH_SESSION_CANCEL_REQUEST);
            MDB_Send(CSH_SESSION_CANCEL_REQUEST | CSH_ACK); // Checksum with Mode bit
        }; break;

        case CSH_VEND_APPROVED : {
            MDB_Send(CSH_VEND_APPROVED);
            MDB_Send(0xFF); // Vend Amount H
            MDB_Send(0xFF); // Vend Amount L
            MDB_Send(CSH_VEND_APPROVED | 0xFF | 0xFF | CSH_ACK); // Checksum, manual calculation
        }; break;

        case CSH_VEND_DENIED : {
            MDB_Send(CSH_VEND_DENIED);
            MDB_Send(CSH_VEND_DENIED | CSH_ACK); // Checksum with Mode bit
        }; break;

        case CSH_END_SESSION : {
            csh_state = ENABLED;
            MDB_Send(CSH_END_SESSION);
            MDB_Send(CSH_END_SESSION | CSH_ACK); // Checksum with Mode bit
        }; break;
        case CSH_CANCELLED   : {
            csh_poll_state = CSH_ACK;
            // csh_state = ENABLED;
            MDB_Send(CSH_CANCELLED);
            MDB_Send(CSH_CANCELLED | CSH_ACK); // Checksum with Mode bit
        }; break;

        case CSH_PERIPHERAL_ID       : READER_PeripheralID();       break;
        case CSH_MALFUNCTION_ERROR   : READER_MalfunctionError();   break;
        case CSH_CMD_OUT_OF_SEQUENCE : READER_CmdOutOfSequence();   break;
        case CSH_DIAGNOSTIC_RESPONSE : READER_DiagnosticResponse(); break;

        default : break;
    }
}
/*
 *
 */
void MDB_VendHandler(void)
{
    uint8_t i; // counter
    uint8_t checksum = VMC_VEND;
    uint16_t vend_temp;
    uint8_t vend_data[6];
    // Wait for Subcommand (1 element total)
    while (1)
        if (MDB_DataCount() > 0)
            break;
    // Store Subcommand in array
    MDB_Read(&vend_temp);
    vend_data[0] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present    
    // Switch through subcommands
    switch(vend_data[0])
    {
        case 0x00 : { // Vend Request
            // Wait for 5 elements in buffer
            // 4 data + 1 checksum
            while (1)
                if (MDB_DataCount() > 4)
                    break;
            // Read all data and store it in an array, with a subcommand
            for (i = 1; i < 6; ++i)
            {
                MDB_Read(&vend_temp);
                vend_data[i] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            }
            // calculate checksum excluding last read element, which is a received checksum
            checksum += calc_checksum(vend_data, 5);
            // compare calculated and received checksums
            if (checksum != vend_data[5])
                return; // checksum mismatch, error
            /*
             * ===================================
             * HERE GOES THE CODE FOR RFID/HTTP Handlers
             * if enough payment media, tell server to subtract sum
             * wait for server reply, then CSH_VEND_APPROVED
             * Otherwise, CSH_VEND_DENIED
             * ===================================
            */
        }; break;

        case 0x01 : { // Vend Cancel
            // Wait for 1 element in buffer
            // 1 checksum
            while (1)
                if (MDB_DataCount() > 0)
                    break;
            MDB_Read(&vend_temp);
            vend_data[1] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            // checksum
            checksum += calc_checksum(vend_data, 1);
            if (checksum != vend_data[1])
                return; // checksum mismatch, error
            csh_poll_state = CSH_VEND_DENIED;
            csh_state = SESSION_IDLE;
            MDB_Send(CSH_ACK);
        }; break;

        case 0x02 : { // Vend Success
            // Wait for 3 elements in buffer
            // 2 data + 1 checksum
            while (1)
                if (MDB_DataCount() > 2)
                    break;
            for (i = 1; i < 4; ++i)
            {
                MDB_Read(&vend_temp);
                vend_data[i] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            }
            checksum += calc_checksum(vend_data, 3);
            if (checksum != vend_data[3])
                return; // checksum mismatch, error

            /* here goes another check-check with a server */

            MDB_Send(CSH_ACK);
        }; break;

        case 0x03 : { // Vend Failure
            // Wait for 1 element in buffer
            // 1 checksum
            while (1)
                if (MDB_DataCount() > 0)
                    break;
            MDB_Read(&vend_temp);
            vend_data[1] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            checksum += calc_checksum(vend_data, 1);
            if (checksum != vend_data[1])
                return; // checksum mismatch, error

            /* refund through server connection */

            MDB_Send(CSH_ACK); // in case of success
            // READER_MalfunctionError(); -- in case of failure, like unable to connect to server
        }; break;

        case 0x04 : { // Session Complete
            // Wait for 1 element in buffer
            // 1 checksum
            while (1)
                if (MDB_DataCount() > 0)
                    break;
            MDB_Read(&vend_temp);
            vend_data[1] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            checksum += calc_checksum(vend_data, 1);
            if (checksum != vend_data[1])
                return; // checksum mismatch, error

            MDB_Send(CSH_END_SESSION);
            csh_state = ENABLED;
        }; break;

        case 0x05 : { // Cash Sale
            // Wait for 5 elements in buffer
            // 4 data + 1 checksum
            while (1)
                if (MDB_DataCount() > 4)
                    break;
            for (i = 1; i < 6; ++i)
            {
                MDB_Read(&vend_temp);
                vend_data[i] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            }
            checksum += calc_checksum(vend_data, 5);
            if (checksum != vend_data[5])
                return; // checksum mismatch, error

            /* Cash sale implementation */

            MDB_Send(CSH_ACK);
        }; break;

        default : break;
    }
}
/*
 *
 */
void MDB_ReaderHandler(void)
{
    uint8_t i; // counter
    uint8_t checksum = VMC_READER;
    uint16_t reader_temp;
    uint8_t reader_data[2];
    // Wait for Subcommand and checksum (2 elements total)
    while (1)
        if (MDB_DataCount() > 1)
            break;
    // Store received data in array
    for (i = 0; i < 2; ++i)
    {
        MDB_Read(&reader_temp);
        reader_data[i] = (uint8_t)(reader_temp & 0x00FF); // get rid of Mode bit if present
    }
    // Calculate checksum
    checksum += calc_checksum(reader_data, 1);
    // Second element is an incoming checksum, compare it to the calculated one
    if (checksum != reader_data[1])
        return; // checksum mismatch, error
    // Look at Subcommand
    switch(reader_data[0])
    {
        case 0x00 : { // Disable
            csh_state = DISABLED;
            // csh_poll_state = CSH_ACK;
            MDB_Send(CSH_ACK);
        }; break;

        case 0x01 : { // Enable
            if (csh_state != DISABLED)
                return;
            csh_state = ENABLED;
            MDB_Send(CSH_ACK);
        }; break;

        case 0x02 : { // Cancelled
            if (csh_state != ENABLED)
                return;
            csh_poll_state = CSH_CANCELLED;
            MDB_Send(CSH_ACK);
        }; break;

        default : break;
    }
}
/*
 * ========================================
 * Start of MDB Bus Communication functions
 * ========================================
 */
void MDB_Send(uint16_t data)
{
    USART_TXBuf_Write(data);
}
/* 
 * Receive MDB Command/Reply/Data from the Bus
 */
void MDB_Read(uint16_t *data)
{
    USART_RXBuf_Read(data);
}
/*
 * Peek at the oldest incoming data from the Bus
 */
void MDB_Peek(uint16_t *data)
{
    USART_RXBuf_Peek(data);
}

uint8_t MDB_DataCount (void)
{
    return USART_RXBuf_Count();
}
/*
 * ======================================
 * End of MDB Bus Communication functions
 * ======================================
 */
/*
 * =============================================
 * Start of MDB_PollHandler() internal functions
 * =============================================
 */
void READER_Reset(void)
{
    // Reset all data
    vmc_config.featureLevel   = 0;
    vmc_config.displayColumns = 0;
    vmc_config.displayRows    = 0;
    vmc_config.displayInfo    = 0;

    vmc_prices.maxPrice = 0;
    vmc_prices.minPrice = 0;
    // Send ACK, turn INACTIVE
    csh_state = INACTIVE;
    MDB_Send(CSH_ACK);
}

void READER_ConfigInfo(void)
{
    uint8_t checksum = 0;
    // calculate Response checksum (without Mode bit yet)
    checksum = ( CSH_READER_CONFIG_INFO
               + csh_config.featureLevel
               + csh_config.countryCodeH
               + csh_config.countryCodeL
               + csh_config.scaleFactor
               + csh_config.decimalPlaces
               + csh_config.maxResponseTime
               + csh_config.miscOptions );
    // Send Response, cast all data to uint16_t
    MDB_Send(CSH_READER_CONFIG_INFO);
    MDB_Send(csh_config.featureLevel);
    MDB_Send(csh_config.countryCodeH);
    MDB_Send(csh_config.countryCodeL);
    MDB_Send(csh_config.scaleFactor);
    MDB_Send(csh_config.decimalPlaces);
    MDB_Send(csh_config.maxResponseTime);
    MDB_Send(csh_config.miscOptions);
    MDB_Send(checksum | CSH_ACK); // cast to uint16_t and set Mode Bit
}

void READER_DisplayRequest(void)
{
    char *message;
    uint8_t i;
    uint8_t message_length;
    // As in standard, the number of bytes must equal the product of Y3 and Y4
    // up to a maximum of 32 bytes in the setup/configuration command
    message_length = (vmc_config.displayColumns * vmc_config.displayRows);
    message = (char *)malloc(message_length * sizeof(char));
    /*
     * Here you need to copy the message into allocated memory
     */
    // Send al this on the Bus
    MDB_Send(CSH_DISPLAY_REQUEST);
    MDB_Send(0xAA); // Display time in arg * 0.1 second units
    for(i = 0; i < message_length; ++i)
        MDB_Send(*(message + i));
    free(message);
}

void READER_BeginSession(void)
{
    uint8_t checksum = 0;
    MDB_Send(CSH_BEGIN_SESSION);
    MDB_Send(user_funds >> 8);      // upper byte of funds available
    MDB_Send(user_funds &  0xFF);   // lower byte of funds available
    checksum = CSH_BEGIN_SESSION + (user_funds >> 8) + (user_funds &  0xFF);
    MDB_Send(checksum | CSH_ACK); // send checksum with Mode bit set
}

void READER_PeripheralID(void)
{
    // Manufacturer Data, 30 bytes + checksum with mode bit
    // not yet implemented
    // uint8_t periph_id[30];
    // periph_id[0] = CSH_PERIPHERAL_ID;
    // periph_id[1] = ''; periph_id[2] = ''; periph_id[3] = '';
}

void READER_MalfunctionError(void)
{
    uint8_t i;
    uint8_t malf_err[2];
    malf_err[0] = CSH_MALFUNCTION_ERROR;
    malf_err[1] = csh_error_code;
    // calculate checksum, set Mode bit and store it
    malf_err[2] = calc_checksum(malf_err, 2) | CSH_ACK;
    for (i = 0; i < 3; ++i)
        MDB_Send(malf_err[i]);
}

void READER_CmdOutOfSequence(void)
{
    MDB_Send(CSH_CMD_OUT_OF_SEQUENCE);
    MDB_Send(CSH_CMD_OUT_OF_SEQUENCE | CSH_ACK);
}

void READER_DiagnosticResponse(void)
{

}
/*
 * ===========================================
 * End of MDB_PollHandler() internal functions
 * ===========================================
 */

/*
 * Misc helper functions
 */

 /*
  * calc_checksum()
  * Calculates checksum of *array from 0 to arr_size
  * Use with caution (because of pointer arithmetics)
  */
uint8_t calc_checksum(uint8_t *array, uint8_t arr_size)
{
    uint8_t ret_val = 0x00;
    uint8_t i;
    for (i = 0; i < arr_size; ++i)
        ret_val += *(array + i);
    return ret_val;
}
