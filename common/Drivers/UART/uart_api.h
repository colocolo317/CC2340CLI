/*
 * uart_api.h
 *
 *  Created on: 2023¦~9¤ë25¤é
 *      Author: ch.wang
 */

#ifndef COMMON_DRIVERS_UART_UART_API_H_
#define COMMON_DRIVERS_UART_UART_API_H_


#define UART_NO_ECHO        (0)
#define UART_ECHO           (1)


int_fast16_t uartTx_send(uint8 *pValue, uint16 len);
bStatus_t uartSetEchoOnOff(uint8 onOff);


#endif /* COMMON_DRIVERS_UART_UART_API_H_ */
