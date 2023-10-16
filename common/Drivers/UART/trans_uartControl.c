/*
 * uartControl.c
 *
 *  Created on: 2023/09/25
 *      Author: ch.wang
 */
/*
 * For transparent mode. Streaming from UART
 * to BLE characteristic.
 */

#include <FreeRTOS.h>
#include <icall.h>
/* POSIX Header files */
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
/* Driver Header files */
#include <ti/bleapp/profiles/data_stream/data_stream_profile.h>
#include <ti/bleapp/services/data_stream/data_stream_server.h>
#include <trans_uartApi.h>
/* Driver configuration */
#include "ti_drivers_config.h"
#include "icall_ble_api.h"


/* Stack size in bytes */
#define THREADSTACKSIZE 1024

#define MAX_INPUT_LENGTH    50
#define MAX_OUTPUT_LENGTH   100

/* === Global Variables ===*/
uint8_t rxBuffer[MAX_INPUT_LENGTH];

/* === Local Variables ===*/
static sem_t sem;
static volatile size_t numBytesRead;
static UART2_Handle trans_uartHandle;
static UART2_Params trans_uartParams;
ICall_EntityID trans_UartICallEntityID;

/* === Functions === */
void trans_uartStart(void);
void *trans_uartThread(void *arg0);
void trans_uartRxCB(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status);
uint8_t trans_uartProcessMsgCB(uint8_t event, uint8_t *pMessage);


/*
 *  ======== callbackFxn ========
 */
void trans_uartRxCB(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status != UART2_STATUS_SUCCESS)
    { /* RX error occured in UART2_read() */ while (1) {} }

    numBytesRead = count;
    sem_post(&sem);
}


void *trans_uartThread(void *arg0)
{
    // IMPORTANT: Task should register to ICall app to access BLE function
    ICall_Errno icall_staus;
    icall_staus = ICall_registerAppCback(&trans_UartICallEntityID, trans_uartProcessMsgCB);

    int32_t semStatus;
    uint32_t status = UART2_STATUS_SUCCESS;

    semStatus = sem_init(&sem, 0, 0); /* Create semaphore */

    if (semStatus != 0)
    { while (1) {} /* Error creating semaphore */ }

    /* Create a UART in CALLBACK read mode */
    UART2_Params_init(&trans_uartParams);
    trans_uartParams.readMode     = UART2_Mode_CALLBACK;
    trans_uartParams.readCallback = trans_uartRxCB;
    trans_uartParams.baudRate     = 115200;

    trans_uartHandle = UART2_open(CONFIG_DISPLAY_UART, &trans_uartParams);
    if (trans_uartHandle == NULL)
    { while (1) {} /* UART2_open() failed */ }

    uint8 bleStatus = SUCCESS;
    static const char * const pcBleMessage = "\r\nBLE streaming start. Your input in UART is output to BLE.\r\n";
    status = UART2_write(trans_uartHandle, pcBleMessage, strlen( pcBleMessage ), NULL);

    while (1)
    {
        numBytesRead = 0;
        status = UART2_read(trans_uartHandle, rxBuffer, MAX_INPUT_LENGTH, NULL);
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

uint8_t trans_uartProcessMsgCB(uint8_t event, uint8_t *pMessage)
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

void trans_uartStart(void)
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

    retc = pthread_create(&thread, &attrs, trans_uartThread, NULL);
    if (retc != 0)
    { while (1) {} /* pthread_create() failed */ }
}

int_fast16_t trans_uartTxSend(uint8_t *pValue, uint16_t len)
{
    return UART2_write(trans_uartHandle, pValue, len, NULL);
}

UART2_Handle trans_getUartHandle(void)
{
    return trans_uartHandle;
}
