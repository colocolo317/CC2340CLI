/*
 * cli_api.h
 *
 *  Created on: 2023/09/19
 *      Author: ch.wang
 *  Free RTOS command line interface
 */

#ifndef COMMON_FREERTOSCLI_CLI_API_H_
#define COMMON_FREERTOSCLI_CLI_API_H_

#include <ti/drivers/UART2.h>

#define CLI_UART_NO_ECHO        (0)
#define CLI_UART_ECHO           (1)
#define CLI_SWITCH_TRANS_ON     true
#define CLI_SWITCH_TRANS_OFF    false

/**
 * Setup command list table
 */
void cli_init(void);
bStatus_t cli_uartSetEchoOnOff(uint8 onOff);
UART2_Handle cli_getUartHandle(void);
bStatus_t cli_uartEnable(void);
bStatus_t cli_uartDisable(void);
int cli_resumeByPostSemaphore(void);
void cli_setTransModeSwitchFlag(uint8 onOff);

#endif /* COMMON_FREERTOSCLI_CLI_API_H_ */
