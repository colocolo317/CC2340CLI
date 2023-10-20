/*
 * cli_setup.c
 *
 *  Created on: 2023/09/19
 *      Author: ch.wang
 */

#include <stdio.h>
#include <FreeRTOS.h>
#include "FreeRTOS_CLI.h"
#include <string.h>
#include <pthread.h>
#include "ble_stack_api.h"
#include <common/BLEAppUtil/inc/bleapputil_api.h>
#include <ti/bleapp/ble_app_util/inc/bleapputil_internal.h>
#include <common/Services/data_stream/data_stream_server.h>
#include <common/Services/dev_info/dev_info_service.h>
#include <gapgattserver.h>
#include <driverlib/pmctl.h>
#include <app_main.h>
#include <common/FreeRTOSCli/cli_api.h>
#include <common/Drivers/UART/trans_uartApi.h>
#include "icall_ble_api.h"

//#define MAX_COMMAND_COUNT 4

extern BLEAppUtil_TheardEntity_t BLEAppUtil_theardEntity;

/* External function */
extern int BLEAppUtil_createBLEAppUtilTask(void);

/* Local function */
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
static BaseType_t prvAT_BLEPERINTFYfxn( char *pcWriteBuffer,
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
            "AT+ECHO <enable>  : <enable>=1, turn on echo. <enable>=0 turn off echo.\r\n",
            prvAT_ECHOfxn,
            1
         },
         {
            "AT+BLESTART",
            "AT+BLESTART       : Start BLE default service with advertisement going on.\r\n",
            prvAT_BLESTARTfxn,
            0   // Could input role as argument
        },
        {
            "AT+BLEADDR",
            "AT+BLEADDR        : Show BLE address.\r\n",
            prvAT_BLEADDRfxn,
            0
        },
        {
            "AT+BLENAME",
            "AT+BLENAME        : Show BLE name.\r\n",
            prvAT_BLENAMEfxn,
            0
        },
        {
            "AT+VERSION",
            "AT+VERSION        : Show firmware version\r\n",
            prvAT_VERSIONfxn,
            0
        },
        {
            "AT+BLETRANMODE",
            "AT+BLETRANMODE    : Start transparent mode between UART and characteristic 0xFFF1 & 0xFFF2. UART key in \"\\r\\n+++\\r\\n\" to stop.\r\n",
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
            "AT+BLEPERINTFY",
            "AT+BLEPERINTFY <stringNoQuote>: Peripheral role send notify to default UUID.\r\n",
            prvAT_BLEPERINTFYfxn,
            1
        },
        {
            "AT+BLEDISCONN",
            "AT+BLEDISCONN     : BLE disconnect all links. \r\n",
            prvAT_BLEDISCONNfxn,
            0
        },
        {
            "AT+BLESTAT",
            "AT+BLESTAT        : Show BLE status. \r\n",
            prvAT_BLESTATfxn,
            0
        },
        {
            "AT+RST",
            "AT+RST            : Reset device immediately.\r\n",
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
        ret = cli_uartSetEchoOnOff(CLI_UART_NO_ECHO);
        break;
    case '1':
        ret = cli_uartSetEchoOnOff(CLI_UART_ECHO);
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
    bStatus_t status = SUCCESS;
    // Check BLE task is running
    // TODO: use BLEAppUtil_checkBLEstat() function get BLE status
    if (BLEAppUtil_theardEntity.threadId != NULL)
    {
        cli_setTransModeSwitchFlag(CLI_SWITCH_TRANS_ON);
        cli_writeOK(pcWriteBuffer);
        return pdFALSE;
    }

    cli_writeError(pcWriteBuffer);
    return pdFALSE;
}
static BaseType_t prvSTOPTRANMODEfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    // Will never come here.
    // TODO: in no echo mode may need "OK" rsp here?
    // trans_uartClose();  // cli_uartOpen();
    return pdFALSE;
}
static BaseType_t prvAT_BLEPERINTFYfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    bStatus_t status = SUCCESS;
    gattAttribute_t* pAttr = DSS_getDefaultNotifyGatt();

    if (pAttr == NULL)
    { cli_writeError(pcWriteBuffer); return pdFALSE; }
    if (pAttr->handle == 0)
    { cli_writeError(pcWriteBuffer); return pdFALSE; }

    const char *pcParameter1;
    BaseType_t xParameter1StringLength;
    pcParameter1 = FreeRTOS_CLIGetParameter(pcCommandString, 1, &xParameter1StringLength);
    status = DSS_setParameter( DSS_DATAOUT_ID, (void*)pcParameter1, xParameter1StringLength );

    if(status != SUCCESS)
    { cli_writeError(pcWriteBuffer); return pdFALSE; }

    cli_writeOK(pcWriteBuffer);
    sprintf(pcWriteBuffer + strlen(pcWriteBuffer),"UUID: %02X%02X , handle: %04X\r\n", *(pAttr->type.uuid+1), *pAttr->type.uuid, pAttr->handle);

    return pdFALSE;
}
static BaseType_t prvAT_BLEDISCONNfxn( char *pcWriteBuffer,
                                        size_t xWriteBufferLen,
                                        const char *pcCommandString )
{
    App_connInfo* connList = Connection_getConnList();

    bStatus_t status = SUCCESS;
    int i;
    for(i = (MAX_NUM_BLE_CONNS-1); i >= 0; i--)
    {
        if(connList[i].connHandle != LINKDB_CONNHANDLE_INVALID){
            status |= BLEAppUtil_disconnect(connList[i].connHandle);
        }
    }

    if(status == SUCCESS){
        cli_writeOK(pcWriteBuffer);
    }else{
        cli_writeError(pcWriteBuffer);
    }

    return pdFALSE;
}
static BaseType_t prvAT_BLESTATfxn( char *pcWriteBuffer,
                                    size_t xWriteBufferLen,
                                    const char *pcCommandString )
{
    cli_writeOK(pcWriteBuffer);
    sprintf(pcWriteBuffer+strlen(pcWriteBuffer), "BLE Role: , State:\r\nInit:  , Advertising:  , Connected: \r\n");

    return pdFALSE;
}
static BaseType_t prvAT_RSTfxn( char *pcWriteBuffer,
                                size_t xWriteBufferLen,
                                const char *pcCommandString )
{
    // buffer string may not meet display time.
    cli_writeOK(pcWriteBuffer);
    PMCTLResetSystem();
    return pdFALSE;
}
/*
static BaseType_t prvAT_BLESTOPfxn( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    pthread_cancel(BLEAppUtil_theardEntity.threadId);
    strcpy(pcWriteBuffer, "BLE stopped.");
    return pdFALSE;
}
*/
