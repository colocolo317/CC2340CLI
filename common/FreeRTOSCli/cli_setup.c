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

#define MAX_COMMAND_COUNT 4

extern BLEAppUtil_TheardEntity_t BLEAppUtil_theardEntity;

//extern void appMain(void);
extern int BLEAppUtil_createBLEAppUtilTask(void);

static BaseType_t prvATpSTARTCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvATpSTOPCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvATpADDRCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );
static BaseType_t prvATpNOTIFYCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString );

void cli_init(void){
    uint8_t i;
    static const CLI_Command_Definition_t xTasksCommand[MAX_COMMAND_COUNT] =
    {
        {
            "AT+START",
            "AT+START          : Start BLE thread.\r\n",
            prvATpSTARTCommand,
            0   // Could input role as argument
        },
        {
            "AT+STOP",
            "AT+STOP           : Stop BLE thread.\r\n",
            prvATpSTOPCommand,
            0
        },
        {
            "AT+ADDR",
            "AT+ADDR           : Get device address.\r\n",
            prvATpADDRCommand,
            0
        },
        {
            "AT+NOTIFY",
            "AT+NOTIFY         : BLE send notify.\r\n",
            prvATpNOTIFYCommand,
            0
        }
    };

    for(i = 0; i < MAX_COMMAND_COUNT; i++)
    {
        FreeRTOS_CLIRegisterCommand( &xTasksCommand[i] );
    }
}

static BaseType_t prvATpADDRCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    uint8_t getAddr[6] = {0};
    bleStk_getDevAddr(TRUE, getAddr);
    char* strAddr = BLEAppUtil_convertBdAddr2Str(getAddr);
    strcpy(pcWriteBuffer, strAddr);
    return pdFALSE;
}

static BaseType_t prvATpSTARTCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    /* output buffer is large enough
    so the xWriteBufferLen parameter is not used. */
    ( void ) xWriteBufferLen;

    int ret = BLEAppUtil_createBLEAppUtilTask();
    /* written directly into the output buffer. */
    strcpy(pcWriteBuffer, "BLE started.");

    /* no further output string, so return pdFALSE. */
    return pdFALSE;
}

static BaseType_t prvATpSTOPCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    pthread_cancel(BLEAppUtil_theardEntity.threadId);
    strcpy(pcWriteBuffer, "BLE stopped.");
    return pdFALSE;
}

static BaseType_t prvATpNOTIFYCommand( char *pcWriteBuffer,
                                          size_t xWriteBufferLen,
                                          const char *pcCommandString )
{
    bStatus_t status = SUCCESS;
    status = DSS_setParameter( DSS_DATAOUT_ID, "AT+NOTIFY", 9 );
    if(status == SUCCESS)
    {
        strcpy(pcWriteBuffer, "BLE update characteristic.");
    }
    return pdFALSE;
}
