/*
 * uart_api.h
 *
 *  Created on: 2023/09/25
 *      Author: ch.wang
 */

#ifndef COMMON_DRIVERS_UART_TRANS_UARTAPI_H_
#define COMMON_DRIVERS_UART_TRANS_UARTAPI_H_

#include <ti/drivers/UART2.h>

int_fast16_t trans_uartTxSend(uint8_t *pValue, uint16_t len);
UART2_Handle trans_getUartHandle(void);
bStatus_t trans_uartEnable(void);
bStatus_t trans_uartDisable(void);

#endif /* COMMON_DRIVERS_UART_TRANS_UARTAPI_H_ */
