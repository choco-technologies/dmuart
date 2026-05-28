#include "stm32_common.h"
#include "port/stm32_common_regs.h"

/**
 * @brief Wait for a UART status flag with timeout
 * 
 * @param usart_base USART base address
 * @param flag ISR flag to wait for
 * @param timeout Timeout counter
 * 
 * @return int 0 on success, -1 on timeout
 */
int stm32_uart_wait_flag(uint32_t usart_base, uint32_t flag, uint32_t timeout)
{
    volatile USART_TypeDef *USART = (USART_TypeDef *)usart_base;
    
    while (!(USART->ISR & flag))
    {
        if (--timeout == 0)
        {
            return -1;
        }
    }
    return 0;
}
