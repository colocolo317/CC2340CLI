/*
 * Copyright (c) 2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== uart2callback.c ========
 */
#include <FreeRTOS.h>
#include <FreeRTOS_CLI.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* POSIX Header files */
#include <semaphore.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define MAX_INPUT_LENGTH    50
#define MAX_OUTPUT_LENGTH   100

static sem_t sem;
static volatile size_t numBytesRead;

static const char * const pcWelcomeMessage = "FreeRTOS command server.\r\nType Help to view a list of registered commands.\r\n";

/*
 *  ======== callbackFxn ========
 */
void callbackFxn(UART2_Handle handle, void *buffer, size_t count, void *userArg, int_fast16_t status)
{
    if (status != UART2_STATUS_SUCCESS)
    {
        /* RX error occured in UART2_read() */
        while (1) {}
    }

    numBytesRead = count;
    sem_post(&sem);
}

/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    char input;
    UART2_Handle uart;
    UART2_Params uartParams;
    int32_t semStatus;
    uint32_t status = UART2_STATUS_SUCCESS;
    static char pcOutputString[ MAX_OUTPUT_LENGTH ], pcInputString[ MAX_INPUT_LENGTH ];
    BaseType_t xMoreDataToFollow;
    int8_t cInputIndex = 0;

    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_RED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Create semaphore */
    semStatus = sem_init(&sem, 0, 0);

    if (semStatus != 0)
    {
        /* Error creating semaphore */
        while (1) {}
    }

    /* Create a UART in CALLBACK read mode */
    UART2_Params_init(&uartParams);
    uartParams.readMode     = UART2_Mode_CALLBACK;
    uartParams.readCallback = callbackFxn;
    uartParams.baudRate     = 115200;

    uart = UART2_open(CONFIG_DISPLAY_UART, &uartParams);

    if (uart == NULL) { /* UART2_open() failed */ while (1) {} }

    /* Turn on user LED to indicate successful initialization */
    GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_ON);

    /* Pass NULL for bytesWritten since it's not used in this example */
    status = UART2_write(uart, pcWelcomeMessage, strlen( pcWelcomeMessage ), NULL);

    memcpy(pcInputString,"help\0", 5);
    do{
        // Send the command string to the command interpreter.
        xMoreDataToFollow = FreeRTOS_CLIProcessCommand(
                          pcInputString,    //The command string.
                          pcOutputString,   //The output buffer.
                          MAX_OUTPUT_LENGTH //The size of the output buffer.
                      );
        // Write the output generated by the command interpreter to theconsole.
        status = UART2_write( uart, pcOutputString, strlen( pcOutputString ), NULL);

    } while( xMoreDataToFollow != pdFALSE );

    //Clear the input string ready to receive the next command.
    cInputIndex = 0;
    memset( pcInputString, 0x00, MAX_INPUT_LENGTH );
}

void *vCommandConsoleTask( void *pvParameters )
{
    UART2_Handle xConsole;
    int8_t cRxedChar, cInputIndex = 0;
    BaseType_t xMoreDataToFollow;
    /* The input and output buffers are declared static to keep them off the stack. */
    static char pcOutputString[ MAX_OUTPUT_LENGTH ], pcInputString[ MAX_INPUT_LENGTH ];

    /* This code assumes the peripheral being used as the console has already
    been opened and configured, and is passed into the task as the task
    parameter.  Cast the task parameter to the correct type. */
    ///xConsole = ( UART2_Handle ) pvParameters;
    UART2_Params uartParams;
    int32_t semStatus;
    uint32_t status = UART2_STATUS_SUCCESS;
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED pin */
    GPIO_setConfig(CONFIG_GPIO_LED_RED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Create a UART in CALLBACK read mode */
    UART2_Params_init(&uartParams);
    //uartParams.readMode     = UART2_Mode_CALLBACK;
    //uartParams.readCallback = callbackFxn;
    uartParams.baudRate     = 115200;

    xConsole = UART2_open(CONFIG_DISPLAY_UART, &uartParams);

    if (xConsole == NULL)
    {
        /* UART2_open() failed */
        while (1) {}
    }

    /* Turn on user LED to indicate successful initialization */
    GPIO_write(CONFIG_GPIO_LED_RED, CONFIG_GPIO_LED_ON);

    /* Send a welcome message to the user knows they are connected. */
    UART2_write( xConsole, pcWelcomeMessage, strlen( pcWelcomeMessage ), NULL );

    for( ;; )
    {
        /* This implementation reads a single character at a time.  Wait in the
        Blocked state until a character is received. */
        UART2_read( xConsole, &cRxedChar, sizeof( cRxedChar ) ,NULL);

        if( cRxedChar == '\n' )
        {
            /* A newline character was received, so the input command string is
            complete and can be processed.  Transmit a line separator, just to
            make the output easier to read. */
            UART2_write( xConsole, "\r\n", strlen( "\r\n" ), NULL );

            /* The command interpreter is called repeatedly until it returns
            pdFALSE.  See the "Implementing a command" documentation for an
            exaplanation of why this is. */
            //do
            //{
                /* Send the command string to the command interpreter.  Any
                output generated by the command interpreter will be placed in the
                pcOutputString buffer. */
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand
                              (
                                  pcInputString,   /* The command string.*/
                                  pcOutputString,  /* The output buffer. */
                                  MAX_OUTPUT_LENGTH/* The size of the output buffer. */
                              );

                /* Write the output generated by the command interpreter to the
                console. */
             //   UART2_write( xConsole, pcOutputString, strlen( pcOutputString ),NULL );

            //} while( xMoreDataToFollow != pdFALSE );

            /* All the strings generated by the input command have been sent.
            Processing of the command is complete.  Clear the input string ready
            to receive the next command. */
            cInputIndex = 0;
            memset( pcInputString, 0x00, MAX_INPUT_LENGTH );
        }
        else
        {
            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */

            if( cRxedChar == '\r' )
            {
                /* Ignore carriage returns. */
            }
            else if( cRxedChar == '\b' )
            {
                /* Backspace was pressed.  Erase the last character in the input
                buffer - if there are any. */
                if( cInputIndex > 0 )
                {
                    cInputIndex--;
                    pcInputString[ cInputIndex ] = ' ';
                }
            }
            else
            {
                /* A character was entered.  It was not a new line, backspace
                or carriage return, so it is accepted as part of the input and
                placed into the input buffer.  When a n is entered the complete
                string will be passed to the command interpreter. */
                if( cInputIndex < MAX_INPUT_LENGTH )
                {
                    pcInputString[ cInputIndex ] = cRxedChar;
                    cInputIndex++;
                }
            }
        }
    }
}
