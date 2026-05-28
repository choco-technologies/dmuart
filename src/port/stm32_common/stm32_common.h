#ifndef STM32_COMMON_H
#define STM32_COMMON_H

#include <stdint.h>

/**
 * @brief Wait for a UART status flag with timeout
 * 
 * @param usart_base USART base address
 * @param flag ISR flag to wait for
 * @param timeout Timeout counter
 * 
 * @return int 0 on success, -1 on timeout
 */
int stm32_uart_wait_flag(uint32_t usart_base, uint32_t flag, uint32_t timeout);

#endif // STM32_COMMON_H
