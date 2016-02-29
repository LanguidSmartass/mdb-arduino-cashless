#include "MDB.h"
#include "USART.h"

VMC_Config_t vmc_config = {0, 0, 0, 0};
VMC_Prices_t vmc_prices = {0, 0};

CSH_State_t  csh_state = INACTIVE; // State at power-up
CSH_Config_t csh_config = {
    0x01; // reader config data
    0x01; // featureLevel
// Russia's country code is 810 or 643, which
// translates into 0x18, 0x10 or 0x16, 0x43
    0x18;
    0x10;
    0x01; // Scale Factor
    0x02; // Decimal Places
    0x05; // Max Response Time
    0x00; // Misc Options
}

/*
 *
 */
void MDB_CommandsHandler(void)
{
    uint16_t command;
    MDB_Peek(&command);
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
 *
 */
void MDB_ResetHandler(void)
{
    // Reset all data
    vmc_config.featureLevel   = 0;
    vmc_config.displayColumns = 0;
    vmc_config.displayRows    = 0;
    vmc_config.displayInfo    = 0;

    vmc_prices.maxPrice = 0;
    vmc_prices.minPrice = 0;
    // Send ACK, turn INACTIVE
    MDB_Send(CSH_ACK);
    csh_state = INACTIVE;
}
/*
 *
 */
void MDB_SetupHandler(void)
{    
    uint8_t i; // counter
    uint8_t checksum = 0;
    uint16_t vmc_temp;
    uint8_t vmc_data[7];
    /*
     * wait for the whole frame
     * frame structure:
     * 1 command + 1 subcommand + 4 vmc_config fields + 1 Checksum byte
     * 7 bytes total
     */
    if (MDB_DataCount() < 7)
        return;
    // Store frame in array
    for (i = 0; i < 7; ++i)
    {
        MDB_Read(&vmc_temp);
        vmc_data[i] = (uint8_t)(vmc_temp & 0x00FF); // get rid of Mode bit if present
    }
    // calculate checksum excluding last read element, which is a received checksum 
    for (i = 0; i < 6; ++i)
        checksum += vmc_data[i];
     // vmc_data[1] is a SETUP Config Data or Max/Min Prices identifier
    switch(vmc_data[1])
    {
        case 0x00 : {
            // compare calculated and received checksums
            if (checksum != vmc_data[6])
                return; // checksum mismatch, error
            // Store VMC data
            vmc_config.featureLevel   = vmc_data[2];
            vmc_config.displayColumns = vmc_data[3];
            vmc_config.displayRows    = vmc_data[4];
            vmc_config.displayInfo    = vmc_data[5];
            // calculate Response checksum (without Mode bit yet)
            checksum = ( csh_config.readerCongigData
                       + csh_config.featureLevel
                       + csh_config.countryCodeH
                       + csh_config.countryCodeL
                       + csh_config.scaleFactor
                       + csh_config.decimalPlaces
                       + csh_config.maxResponseTime
                       + csh_config.miscOptions )
            // Send Response, cast all data to uint16_t
            MDB_Send(csh_config.readerCongigData);
            MDB_Send(csh_config.featureLevel);
            MDB_Send(csh_config.countryCodeH);
            MDB_Send(csh_config.countryCodeL);
            MDB_Send(csh_config.scaleFactor);
            MDB_Send(csh_config.decimalPlaces);
            MDB_Send(csh_config.maxResponseTime);
            MDB_Send(csh_config.miscOptions);
            MDB_Send(checksum | 0x0100); // cast to uint16_t and set Mode Bit
        } break;

        case 0x01 : {
            if (checksum != vmc_data[6])
                return; // checksum mismatch, error
            // Store VMC Prices
            vmc_prices.maxPrice = ((uint16_t)vmc_data[2] << 8) | vmc_data[3];
            vmc_prices.minPrice = ((uint16_t)vmc_data[4] << 8) | vmc_data[5];
            // Send ACK
            MDB_Send(CSH_ACK);
            // Change state to DISABLED
            csh_state = DISABLED;
        } break;

        default : break;
    }
}
/*
 *
 */
void MDB_PollHandler(void)
{

    /*
        MDB_Send(CSH_JUST_RESET);
        MDB_Send(CSH_ACK);
    */
}
/*
 *
 */
void MDB_VendHandler(void)
{
    uint8_t i; // counter
    uint8_t checksum = 0;
    uint16_t vend_temp;
    uint8_t vend_data[7];
    // Wait for Command and Subcommand (2 bytes total)
    if (MDB_DataCount() < 2)
        return;
    // Store Command and Subcommand in array, but keep commands in a buffer
    for (i = 0; i < 2; ++i)
    {
        MDB_Peek(&vend_temp);
        vend_data[i] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
    }
    // Look at subcommands
    switch(vend_data[1])
    {
        case 0x00 : { // Vend Request
            // Wait for 6 bytes of data
            // 1 command + 1 subcommand + 4 data bytes + 1 checksum
            if (MDB_DataCount < 7)
                return;
            // Now Store all data in array and delete it from buffer
            for (i = 0; i < 7; ++i)
            {
                MDB_Read(&vend_temp);
                vend_data[i] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            }
            // calculate checksum excluding last read element, which is a received checksum 
            for (i = 0; i < 6; ++i)
                checksum += vend_data[i];
            // compare calculated and received checksums
            if (checksum != vend_data[6])
                return; // checksum mismatch, error
            /*
             * ===================================
             * HERE GOES THE CODE FOR RFID/HTTP Handlers
             * if enough payment media, tell server to subtract sum
             * wait for server reply, then CSH_VEND_APPROVED
             * Otherwise, CSH_VEND_DENIED
             * ===================================
            */
        } break;

        case 0x01 : { // Vend Cancel
            for (i = 0; i < 2; ++i)
            {
                MDB_Read(&vend_temp);
                vend_data[i] = (uint8_t)(vend_temp & 0x00FF); // get rid of Mode bit if present
            }
            
            // checksum

            MDB_Send(CSH_VEND_DENIED);
            // ACK, maybe?
            csh_state = SESSION_IDLE;
        } break;

        case 0x02 : { // Vend Success
            // Wait for 5 bytes of data
            // 1 command + 1 subcommand + 2 data bytes + 1 checksum
            if (MDB_DataCount < 5)
                return;
            /* maybe here goes another check-check with a server */
            MDB_Send(CSH_ACK);
        } break;

        case 0x03 : { // Vend Failure
            /* refund through server connection */
            MDB_Send(CSH_ACK);
        } break;

        case 0x04 : { // Session Complete
            MDB_Send(CSH_END_SESSION);
            csh_state = ENABLED;
        } break;

        case 0x05 : { // Cash Sale
            // last one to implement
        } break;

        default : break;
    }
}
/*
 *
 */
void MDB_ReaderHandler(void)
{
    uint8_t i; // counter
    uint8_t checksum = 0;
    uint16_t reader_temp;
    uint8_t reader_data[2];
    // Wait for Command and Subcommand (2 bytes total)
    if (MDB_DataCount() < 2)
        return;
    // Store Command and Subcommand in array, but keep commands in a buffer
    for (i = 0; i < 2; ++i)
    {
        MDB_Read(&reader_temp);
        reader_data[i] = (uint8_t)(reader_temp & 0x00FF); // get rid of Mode bit if present
    }
    switch(reader_data[1])
    {
        case 0x00 : { // Disable
            csh_state = DISABLED;
            MDB_Send(CSH_ACK);
        } break;

        case 0x01 : { // Enable
            csh_state = ENABLED;
            MDB_Send(CSH_ACK);
        } break;

        case 0x02 : { // Cancelled
            MDB_Send(CSH_CANCELLED);
        } break;

        default : break;
    }

}
/*
 * Send MDB Command/Reply/Data on the Bus
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
