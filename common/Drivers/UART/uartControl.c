/*
 * uartControl.c
 *
 *  Created on: 2023/09/25
 *      Author: ch.wang
 */
#include <FreeRTOS.h>
#include "FreeRTOS_CLI.h"
#include <icall.h>
/* POSIX Header files */
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
/* Driver Header files */
#include <ti/drivers/UART2.h>
#include <ti/bleapp/profiles/data_stream/data_stream_profile.h>
#include <ti/bleapp/services/data_stream/data_stream_server.h>
/* Driver configuration */
#include "ti_drivers_config.h"
#include "icall_ble_api.h"
#include "uart_api.h"


/* Stack size in bytes */
#define THREADSTACKSIZE 1024

#define MAX_INPUT_LENGTH    50
#define MAX_OUTPUT_LENGTH   100
/* UART IO MODE for @VAR uUartIOMode */
#define CLI_MODE            0x01    /* UART to commad line interface mode */
#define BLE_STREAM_MODE     0x02    /* UART to BLE characteristic mode */

/* === Global Variables ===*/
uint8_t uUartIOMode = CLI_MODE;
uint8_t rxBuffer[MAX_INPUT_LENGTH];

/* === Local Variables ===*/
static sem_t sem;
static volatile size_t numBytesRead;
UART2_Handle handle;
ICall_EntityID UARTICallEntityID;
static uint8 uart_echo_onoff = UART_ECHO;

/* === Functions === */
void uartConsoleStart(void);
void *uartConsoleThread(void *arg0);
void uartRx_cb(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
void uartCliIOFxn(UART2_Handle handle);
void uarttoBleStreamFxn(UART2_Handle handle);
uint8_t uart_processMsgCB(uint8_t event, uint8_t *pMessage);

static int_fast16_t uartTx_echo(UART2_Handle handle, const void* pValue, size_t len , size_t *bytesWritten);


/*
 *  ======== callbackFxn ========
 */
void uartRx_cb(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status != UART2_STATUS_SUCCESS)
    { /* RX error occured in UART2_read() */ while (1) {} }

    numBytesRead = count;
    sem_post(&sem);
}


void *uartConsoleThread(void *arg0)
{
    // IMPORTANT: Task should register to ICall app to access BLE function
    ICall_Errno icall_staus;
    icall_staus = ICall_registerAppCback(&UARTICallEntityID, uart_processMsgCB); // handle maybe cannot be NULL

    //UART2_Handle handle;
    UART2_Params uartParams;
    int32_t semStatus;
    uint32_t status = UART2_STATUS_SUCCESS;

    semStatus = sem_init(&sem, 0, 0); /* Create semaphore */

    if (semStatus != 0)
    { while (1) {} /* Error creating semaphore */ }

    /* Create a UART in CALLBACK read mode */
    UART2_Params_init(&uartParams);
    uartParams.readMode     = UART2_Mode_CALLBACK;
    uartParams.readCallback = uartRx_cb;
    uartParams.baudRate     = 115200;

    handle = UART2_open(CONFIG_DISPLAY_UART, &uartParams);
    if (handle == NULL)
    { while (1) {} /* UART2_open() failed */ }

    switch(uUartIOMode)
    {
        case BLE_STREAM_MODE:
            uarttoBleStreamFxn(handle);
            break;
        case CLI_MODE:
        default:
            uartCliIOFxn(handle);
            break;
    }
}

uint8_t uart_processMsgCB(uint8_t event, uint8_t *pMessage)
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

void uartConsoleStart(void)
{
    pthread_t thread;
    pthread_attr_t attrs;
    struct sched_param priParam;
    int retc;
    /* Initialize the attributes structure with default values */
    pthread_attr_init(&attrs);

    /* Set priority, detach state, and stack size attributes */
    priParam.sched_priority = 1;
    retc                    = pthread_attr_setschedparam(&attrs, &priParam);
    retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
    retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
    if (retc != 0)
    { while (1) {} /* failed to set attributes */ }

    retc = pthread_create(&thread, &attrs, uartConsoleThread, NULL);
    if (retc != 0)
    { while (1) {} /* pthread_create() failed */ }
}

void uarttoBleStreamFxn(UART2_Handle handle)
{
    uint32_t status = UART2_STATUS_SUCCESS;
    uint8 bleStatus = SUCCESS;
    static const char * const pcBleMessage = "BLE streaming start. Your input in UART is output to BLE.\r\n";
    status = UART2_write(handle, pcBleMessage, strlen( pcBleMessage ), NULL);

    while (1)
    {
        numBytesRead = 0;
        status = UART2_read(handle, rxBuffer, MAX_INPUT_LENGTH, NULL);
        if (status != UART2_STATUS_SUCCESS)
        { /* UART2_read() failed */ while (1) {} }

        sem_wait(&sem); /* Do not write until read callback executes */

        if (numBytesRead > 0)
        {
            // send to BLE characteristic
            bleStatus = DSS_setParameter( DSS_DATAOUT_ID, rxBuffer, numBytesRead );

            if(bleStatus != SUCCESS)
            { /* DSS_setParameter() failed */ while (1) {} }
        }
    }
}

void uartCliIOFxn(UART2_Handle handle)
{
    const char breakLine[] = "\r\n";
    const char backspace[] = " \b";
    char cRxedChar;
    static char pcOutputString[ MAX_OUTPUT_LENGTH ], pcInputString[ MAX_INPUT_LENGTH ];
    int8_t cInputIndex = 0;
    BaseType_t xMoreDataToFollow;
    uint32_t status = UART2_STATUS_SUCCESS;
    static const char * const pcCliMessage = "FreeRTOS command line interface.\r\nType HELP to view a list of registered commands.\r\n";
    /* Pass NULL for bytesWritten since it's not used in this example */
    status = UART2_write(handle, pcCliMessage, strlen( pcCliMessage ), NULL);

    /* Loop forever echoing */
    while (1)
    {
        numBytesRead = 0;
        /* Pass NULL for bytesRead since it's not used in this example */
        status = UART2_read(handle, &cRxedChar, 1, NULL);

        if (status != UART2_STATUS_SUCCESS)
        { /* UART2_read() failed */ while (1) {} }

        sem_wait(&sem); /* Do not write until read callback executes */

        if (numBytesRead > 0)
        {
            status = uartTx_echo(handle, &cRxedChar, 1, NULL);
            if(cRxedChar == '\r' || cRxedChar == '\n')
            {
                status = uartTx_echo(handle, breakLine, 2, NULL);
                do{
                    // Send the command string to the command interpreter.
                    xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                                      pcInputString,    //The command string.
                                      pcOutputString,   //The output buffer.
                                      MAX_OUTPUT_LENGTH //The size of the output buffer.
                                  );
                    // Write the output generated by the command interpreter to the console.
                    status = UART2_write( handle, pcOutputString, strlen( pcOutputString ), NULL);
                } while( xMoreDataToFollow != pdFALSE );

                status = UART2_write(handle, breakLine, 2, NULL);
                cInputIndex = 0;
                memset( pcInputString, 0x00, MAX_INPUT_LENGTH );
            }
            else
            {
                if(cRxedChar == '\b')
                {
                    if( cInputIndex > 0 )
                    {
                        cInputIndex--;
                        pcInputString[ cInputIndex ] = ' ';
                    }
                    status = uartTx_echo(handle, backspace, 2, NULL);
                }
                else
                {
                    if( cInputIndex < MAX_INPUT_LENGTH )
                    {
                        pcInputString[ cInputIndex ] = cRxedChar;
                        cInputIndex++;
                    }
                }
            }

            if (status != UART2_STATUS_SUCCESS)
            { while (1) {} /* UART2_write() failed */ }
        }
    }
}

int_fast16_t uartTx_send(uint8 *pValue, uint16 len)
{
    return UART2_write(handle, pValue, len, NULL);
}

static int_fast16_t uartTx_echo(UART2_Handle handle, const void* pValue, size_t len , size_t *bytesWritten)
{
    if(uart_echo_onoff)
    {
        return UART2_write(handle, pValue, len, NULL);
    }
    return 0;
}

bStatus_t uartSetEchoOnOff(uint8 onOff)
{
    bStatus_t status = SUCCESS;

    switch(onOff)
    {
    case UART_NO_ECHO:
    case UART_ECHO:
        uart_echo_onoff = onOff;
        break;
    default:
        status = FAILURE;
    }

    return status;
}
