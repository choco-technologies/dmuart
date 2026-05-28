#ifndef DMUART_STM32F7_REGS_H
#define DMUART_STM32F7_REGS_H

#include <stdint.h>

/* ======================================================================
 *               STM32F7 Specific UART Definitions
 * ====================================================================== */

/* Base addresses for STM32F7 */
#define STM32F7_RCC_BASE        0x40023800U
#define STM32F7_USART1_BASE     0x40011000U
#define STM32F7_USART2_BASE     0x40004400U
#define STM32F7_USART3_BASE     0x40004800U
#define STM32F7_UART4_BASE      0x40004C00U
#define STM32F7_UART5_BASE      0x40005000U
#define STM32F7_USART6_BASE     0x40011400U
#define STM32F7_UART7_BASE      0x40007800U
#define STM32F7_UART8_BASE      0x40007C00U

/* RCC APB1ENR UART clock enable bits */
#define RCC_APB1ENR_USART2EN    (1U << 17)
#define RCC_APB1ENR_USART3EN    (1U << 18)
#define RCC_APB1ENR_UART4EN     (1U << 19)
#define RCC_APB1ENR_UART5EN     (1U << 20)
#define RCC_APB1ENR_UART7EN     (1U << 30)
#define RCC_APB1ENR_UART8EN     (1U << 31)

/* RCC APB2ENR UART clock enable bits */
#define RCC_APB2ENR_USART1EN    (1U << 4)
#define RCC_APB2ENR_USART6EN    (1U << 5)

/* Maximum number of UART instances on STM32F7 */
#define STM32F7_UART_MAX_INSTANCES  8

/* STM32F7 APB clock frequencies (default with HSI) */
#define STM32F7_APB1_FREQ       16000000U   /**< APB1 clock (default HSI) */
#define STM32F7_APB2_FREQ       16000000U   /**< APB2 clock (default HSI) */

#endif // DMUART_STM32F7_REGS_H
