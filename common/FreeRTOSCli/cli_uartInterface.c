/*
 * cli_uartInterface.c
 *
 *  Created on: 2023/10/13
 *      Author: ch.wang
 */

#include <FreeRTOS.h>
#include "FreeRTOS_CLI.h"
#include <icall.h>
/* POSIX Header files */
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
/* Driver configuration */
#include "ti_drivers_config.h"
#include "icall_ble_api.h"
#include <common/FreeRTOSCli/cli_api.h>
#include <common/Drivers/UART/trans_uartApi.h>

/* Stack size in bytes */
#define THREADSTACKSIZE 1024

#define MAX_INPUT_LENGTH    50
#define MAX_OUTPUT_LENGTH   100

static uint8_t rxBuffer[MAX_INPUT_LENGTH];

static const char * const pcCliMessage = "\r\nAmpak WL71340 AT-Command interface.\r\nType \"HELP\" to view a list of registered commands.\r\n";
static const char * const pcTransModeMessage = "\r\nBLE streaming start. Your input into UART is output to BLE.\r\n";
const char breakLine[] = "\r\n";
const char backspace[] = "\b \b";
/* === Local Variables ===*/
static sem_t sem;
static volatile size_t numBytesRead;
static UART2_Handle cli_uartHandle = NULL;
static UART2_Params cli_uartParams;
static uint8 uart_echo_onoff = CLI_UART_ECHO;
static uint8 cli_uartGiveTransMode = CLI_SWITCH_TRANS_OFF;

ICall_EntityID cli_uartICallEntityID;

/* === Functions === */
void cli_uartConsoleStart(void);
void *cli_uartConsoleThread(void *arg0);
void cli_uartRxCB(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
uint8_t cli_uartProcessMsgCB(uint8_t event, uint8_t *pMessage);

static int_fast16_t cli_uartTxEcho(UART2_Handle handle, const void* pValue, size_t len , size_t *bytesWritten);
bStatus_t cli_switchToTransMode(void);

/*
 *  ======== callbackFxn ========
 */
void cli_uartRxCB(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status != UART2_STATUS_SUCCESS)
    { /* RX error occured in UART2_read() */ while (1) {} }

    numBytesRead = count;
    sem_post(&sem);
}

bStatus_t cli_uartEnable(void)
{
    bStatus_t status = SUCCESS;

    /* Create a UART in CALLBACK read mode */
    UART2_Params_init(&cli_uartParams);
    cli_uartParams.readMode     = UART2_Mode_CALLBACK;
    cli_uartParams.readCallback = cli_uartRxCB;
    cli_uartParams.baudRate     = 115200;

    cli_uartHandle = UART2_open(CONFIG_DISPLAY_UART, &cli_uartParams);
    if (cli_uartHandle == NULL)
    { while (1) {} /* UART2_open() failed */ }

    return status;
}

bStatus_t cli_uartDisable(void)
{
    bStatus_t status = SUCCESS;
    if(cli_uartHandle != NULL)
    {
        UART2_close(cli_uartHandle);
        cli_uartHandle = NULL;
    }
    return status;
}

static bStatus_t cli_uartCmdReceiver(void)
{
    bStatus_t status = SUCCESS;
    char cRxedChar;
    static char pcOutputString[ MAX_OUTPUT_LENGTH ], pcInputString[ MAX_INPUT_LENGTH ];
    static int8_t cInputIndex = 0;
    BaseType_t xMoreDataToFollow;

    numBytesRead = 0;
    /* Pass NULL for bytesRead since it's not used in this example */
    status = UART2_read(cli_uartHandle, &cRxedChar, 1, NULL);

    if (status != UART2_STATUS_SUCCESS)
    { /* UART2_read() failed */ while (1) {} }

    sem_wait(&sem); /* Do not write until read callback executes */

    if (numBytesRead > 0)
    {
        if(cRxedChar == '\r' || cRxedChar == '\n')
        {
            if (strlen( pcInputString ) == 0)
            { return status; } // in case null string input

            status = cli_uartTxEcho(cli_uartHandle, breakLine, 2, NULL);
            do{
                // Send the command string to the command interpreter.
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                                  pcInputString,    //The command string.
                                  pcOutputString,   //The output buffer.
                                  MAX_OUTPUT_LENGTH //The size of the output buffer.
                              );

                if (strlen( pcOutputString ) == 0)
                { continue; } // in case null string output

                if (cli_uartHandle == NULL)
                { break; } // in case transparent mode start

                // Write the output generated by the command interpreter to the console.
                status = UART2_write(cli_uartHandle, pcOutputString, strlen( pcOutputString ), NULL);
            } while( xMoreDataToFollow != pdFALSE );

            //status = UART2_write(cli_uartHandle, breakLine, 2, NULL);
            cInputIndex = 0;
            memset( pcInputString, 0x00, MAX_INPUT_LENGTH );
            memset( pcOutputString, 0x00, MAX_OUTPUT_LENGTH );
        }
        else
        {
            if(cRxedChar == '\b')
            {
                if( cInputIndex > 0 )
                {
                    cInputIndex--;
                    pcInputString[ cInputIndex ] = '\0';
                    status = cli_uartTxEcho(cli_uartHandle, backspace, 3, NULL);
                }
            }
            else
            {
                if( cInputIndex < MAX_INPUT_LENGTH )
                {
                    pcInputString[ cInputIndex ] = cRxedChar;
                    cInputIndex++;
                }
                status = cli_uartTxEcho(cli_uartHandle, &cRxedChar, 1, NULL);
            }
        }

        if (status != UART2_STATUS_SUCCESS)
        { while (1) {} /* UART2_write() failed */ }
    }
    return status;
}

void *cli_uartConsoleThread(void *arg0)
{
    // IMPORTANT: Task should register to ICall app to access BLE function
    // NOTE: ICall_registerAppCback() invoke in trans_uart task cause fail.
    //       CLI don't use ICall for BLE access so cancel the registry here.
    // FIXED: ICall max num task set to 4
    ICall_Errno icall_staus;
    icall_staus = ICall_registerAppCback(&cli_uartICallEntityID, cli_uartProcessMsgCB);

    int32_t semStatus;
    uint32_t status = UART2_STATUS_SUCCESS;

    semStatus = sem_init(&sem, 0, 0); /* Create semaphore */

    if (semStatus != 0)
    { while (1) {} /* Error creating semaphore */ }

    status = cli_uartEnable();
    /* Pass NULL for bytesWritten since it's not used in this example */
    status = UART2_write(cli_uartHandle, pcCliMessage, strlen( pcCliMessage ), NULL);

    /* Loop forever echoing */
    while (1)
    {
        if(cli_uartGiveTransMode)
        {
            status = cli_switchToTransMode();
        }

        if(cli_uartHandle != NULL)
        {
            status = cli_uartCmdReceiver();
        }
        else
        {
            sem_wait(&sem);
        }
    }
}

bStatus_t cli_switchToTransMode(void)
{
    bStatus_t status = SUCCESS;
    status = UART2_write(cli_uartHandle, pcTransModeMessage, strlen( pcTransModeMessage ), NULL);
    status |= cli_uartDisable();
    status |= trans_uartEnable();
    trans_modeSetSwitchFlag(TRANS_MODE_ON);
    status |= trans_resumeByPostSemaphore();
    return status;
}

uint8_t cli_uartProcessMsgCB(uint8_t event, uint8_t *pMessage)
{
  // ignore the event
  // Enqueue the msg in order to be excuted in the application context
  //if (BLEAppUtil_enqueueMsg(BLEAPPUTIL_EVT_STACK_CALLBACK, pMessage) != SUCCESS)
  //{
  //    BLEAppUtil_free(pMessage);
  //}

  // Not safe to dealloc, the application BleAppUtil module will free the msg
  return FALSE;
}

void cli_uartConsoleStart(void)
{
    pthread_t thread;
    pthread_attr_t attrs;
    struct sched_param priParam;
    int retc;
    /* Initialize the attributes structure with default values */
    pthread_attr_init(&attrs);

    /* Set priority, detach state, and stack size attributes */
    priParam.sched_priority = 1;
    retc  = pthread_attr_setschedparam(&attrs, &priParam);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
    if (retc != 0)
    { while (1) {} /* failed to set attributes */ }

    retc = pthread_create(&thread, &attrs, cli_uartConsoleThread, NULL);
    if (retc != 0)
    { while (1) {} /* pthread_create() failed */ }
}

static int_fast16_t cli_uartTxEcho(UART2_Handle handle, const void* pValue, size_t len , size_t *bytesWritten)
{
    if(uart_echo_onoff)
    {
        return UART2_write(handle, pValue, len, NULL);
    }
    return 0;
}

bStatus_t cli_uartSetEchoOnOff(uint8 onOff)
{
    bStatus_t status = SUCCESS;

    switch(onOff)
    {
    case CLI_UART_NO_ECHO:
    case CLI_UART_ECHO:
        uart_echo_onoff = onOff;
        break;
    default:
        status = FAILURE;
    }

    return status;
}

UART2_Handle cli_getUartHandle(void)
{
    return cli_uartHandle;
}

int cli_resumeByPostSemaphore(void)
{
    return sem_post(&sem);
}

void cli_setTransModeSwitchFlag(uint8 onOff)
{
    cli_uartGiveTransMode = onOff;
}
