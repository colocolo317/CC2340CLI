/*
 * cli_setup.c
 *
 *  Created on: 2023/09/19
 *      Author: ch.wang
 */

#include <common/FreeRTOSCli/cli_api.h>
#include <FreeRTOS.h>
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <pthread.h>
#include "ble_stack_api.h"
#include <ti/bleapp/ble_app_util/inc/bleapputil_api.h>
#include <ti/bleapp/ble_app_util/inc/bleapputil_internal.h>
#include <ti/bleapp/services/data_stream/data_stream_server.h>
#include <common/Drivers/UART/uart_api.h>
#include <common/Services/dev_info/dev_info_service.h>
#include <gapgattserver.h>

//#define MAX_COMMAND_COUNT 4

extern BLEAppUtil_TheardEntity_t BLEAppUtil_theardEntity;

//extern void appMain(void);
extern int BLEAppUtil_createBLEAppUtilTask(void);

static inline void cli_writeError(char *pcWriteBuffer){ strcpy(pcWriteBuffer, "\r\nERROR\r\n"); }

static inline void cli_writeOK(char *pcWriteBuffer){ strcpy(pcWriteBuffer, "\r\nOK\r\n"); }

static inline void cli_writeRsp(char *pcWriteBuffer, const char* val, size_t len)
{
    strcpy(pcWriteBuffer, "\r\n");
    strncat(pcWriteBuffer, val, len);
    strncat(pcWriteBuffer, "\r\n", strlen("\r\n"));
}

static BaseType_t prvAT_ECHOfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvAT_BLESTARTfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvAT_BLEADDRfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvAT_BLENAMEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString );
static BaseType_t prvAT_VERSIONfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString );
static BaseType_t prvAT_BLETRANMODEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString );
static BaseType_t prvSTOPTRANMODEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString ); // "+++" command
static BaseType_t prvAT_BLEGATTSNTFYfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvAT_BLEDISCONNfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString );
static BaseType_t prvAT_BLESTATfxn( char *pcWriteBuffer,
                                    size_t xWriteBufferLen,
                                    const char *pcCommandString );
static BaseType_t prvAT_RSTfxn( char *pcWriteBuffer,
                                size_t xWriteBufferLen,
                                const char *pcCommandString );
static BaseType_t prvAT_BLESTOPfxn( char *pcWriteBuffer,
                                      size_t xWriteBufferLen,
                                      const char *pcCommandString ); // none-use

void cli_init(void){
    int i;
    static const CLI_Command_Definition_t xTasksCommand[] =
    {
         {
            "AT+ECHO",
            "AT+ECHO           : \r\n",
            prvAT_ECHOfxn,
            1
         },
         {
            "AT+BLESTART",
            "AT+BLESTART       : Start BLE thread.\r\n",
            prvAT_BLESTARTfxn,
            0   // Could input role as argument
        },
        {
            "AT+BLEADDR",
            "AT+BLEADDR        : Get device address.\r\n",
            prvAT_BLEADDRfxn,
            0
        },
        {
            "AT+BLENAME",
            "AT+BLENAME        : Get BLE name.\r\n",
            prvAT_BLENAMEfxn,
            0
        },
        {
            "AT+VERSION",
            "AT+VERSION        : Get firmware version\r\n",
            prvAT_VERSIONfxn,
            0
        },
        {
            "AT+BLETRANMODE",
            "AT+BLETRANMODE    : \r\n",
            prvAT_BLETRANMODEfxn,
            0
        },
        {
            "+++",
            "",         // hide from command list.
            prvSTOPTRANMODEfxn,
            0
        },
        {
            "AT+BLEGATTSNTFY",
            "AT+BLEGATTSNTFY   : \r\n",
            prvAT_BLEGATTSNTFYfxn,
            0
        },
        {
            "AT+BLEDISCONN",
            "AT+BLEDISCONN     : \r\n",
            prvAT_BLEDISCONNfxn,
            0
        },
        {
            "AT+BLESTAT",
            "AT+BLESTAT        : \r\n",
            prvAT_BLESTATfxn,
            0
        },
        {
            "AT+RST",
            "AT+RST            : \r\n",
            prvAT_RSTfxn,
            0
        }
    };

    int cmd_list_len = sizeof(xTasksCommand) / sizeof(CLI_Command_Definition_t);

    for(i = 0; i < cmd_list_len; i++)
    {
        FreeRTOS_CLIRegisterCommand( &xTasksCommand[i] );
    }
}


static BaseType_t prvAT_ECHOfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    const char *pcParameter1;
    BaseType_t xParameter1StringLength;
    pcParameter1 = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParameter1StringLength);

    // (pcParameter1 == NULL) will be deal with default no parameter error.
    // parameter length should stick to 1
    if(xParameter1StringLength > 1)
    {
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    bStatus_t ret = SUCCESS;
    switch(*pcParameter1)
    {
    case '0':
        ret = uartSetEchoOnOff(0);
        break;
    case '1':
        ret = uartSetEchoOnOff(1);
        break;
    default:
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    if(ret != SUCCESS)
    {
        // Fail to set echo flag
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    cli_writeOK(pcWriteBuffer);
    return pdFALSE;
}
static BaseType_t prvAT_BLESTARTfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    int ret = SUCCESS;
    // output buffer is large enough so the xWriteBufferLen parameter is not used.
    ( void ) xWriteBufferLen;

    // BLE is running. Should not call create task API.
    if (BLEAppUtil_theardEntity.threadId != NULL)
    {
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    ret = BLEAppUtil_createBLEAppUtilTask();
    if(ret != SUCCESS)
    {
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    cli_writeOK(pcWriteBuffer);
    // written directly into the output buffer.
    //strcpy(pcWriteBuffer, "BLE peripheral start.");

    // no further output string, so return pdFALSE.
    return pdFALSE;
}
static BaseType_t prvAT_BLEADDRfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    uint8_t getAddr[6] = {0};
    bleStk_getDevAddr(TRUE, getAddr);
    char* strAddr = BLEAppUtil_convertBdAddr2Str(getAddr);

    cli_writeRsp(pcWriteBuffer, strAddr, 2*B_ADDR_LEN+3);

    return pdFALSE;
}
static BaseType_t prvAT_BLENAMEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    uint32 status = SUCCESS;
    char buffer[GAP_DEVICE_NAME_LEN];
    memset(buffer, 0x00, GAP_DEVICE_NAME_LEN);

    status = GGS_GetParameter(GGS_DEVICE_NAME_ATT, buffer);

    if(status != SUCCESS)
    {
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    cli_writeRsp(pcWriteBuffer, buffer, GAP_DEVICE_NAME_LEN);
    return pdFALSE;
}
static BaseType_t prvAT_VERSIONfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    bStatus_t status = SUCCESS;
    char buffer[DEVINFO_STR_ATTR_LEN+1];
    memset(buffer, 0x00, DEVINFO_STR_ATTR_LEN+1);

    status = DevInfo_getParameter(DEVINFO_FIRMWARE_REV, buffer);

    if(status != SUCCESS)
    {
        cli_writeError(pcWriteBuffer);
        return pdFALSE;
    }

    cli_writeRsp(pcWriteBuffer, buffer, DEVINFO_STR_ATTR_LEN+1);
    return pdFALSE;
}
static BaseType_t prvAT_BLETRANMODEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    return pdFALSE;
}
static BaseType_t prvSTOPTRANMODEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    return pdFALSE;
}
static BaseType_t prvAT_BLEGATTSNTFYfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    bStatus_t status = SUCCESS;
    status = DSS_setParameter( DSS_DATAOUT_ID, "AT+NOTIFY", 9 );
    if(status == SUCCESS)
    {
        strcpy(pcWriteBuffer, "\r\nBLE update characteristic.\r\n");
    }
    return pdFALSE;
}
static BaseType_t prvAT_BLEDISCONNfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    return pdFALSE;
}
static BaseType_t prvAT_BLESTATfxn( char *pcWriteBuffer,
                                    size_t xWriteBufferLen,
                                    const char *pcCommandString )
{
    return pdFALSE;
}
static BaseType_t prvAT_RSTfxn( char *pcWriteBuffer,
                                size_t xWriteBufferLen,
                                const char *pcCommandString )
{
    return pdFALSE;
}
static BaseType_t prvAT_BLESTOPfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    pthread_cancel(BLEAppUtil_theardEntity.threadId);
    strcpy(pcWriteBuffer, "BLE stopped.");
    return pdFALSE;
}
