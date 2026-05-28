#ifndef DMUART_STM32F4_REGS_H
#define DMUART_STM32F4_REGS_H

#include <stdint.h>

/* ======================================================================
 *               STM32F4 Specific UART Definitions
 * ====================================================================== */

/* Base addresses for STM32F4 */
#define STM32F4_RCC_BASE        0x40023800U
#define STM32F4_USART1_BASE     0x40011000U
#define STM32F4_USART2_BASE     0x40004400U
#define STM32F4_USART3_BASE     0x40004800U
#define STM32F4_UART4_BASE      0x40004C00U
#define STM32F4_UART5_BASE      0x40005000U
#define STM32F4_USART6_BASE     0x40011400U

/* RCC APB1ENR UART clock enable bits */
#define STM32F4_RCC_APB1ENR_USART2EN    (1U << 17)
#define STM32F4_RCC_APB1ENR_USART3EN    (1U << 18)
#define STM32F4_RCC_APB1ENR_UART4EN     (1U << 19)
#define STM32F4_RCC_APB1ENR_UART5EN     (1U << 20)

/* RCC APB2ENR UART clock enable bits */
#define STM32F4_RCC_APB2ENR_USART1EN    (1U << 4)
#define STM32F4_RCC_APB2ENR_USART6EN    (1U << 5)

/* Maximum number of UART instances on STM32F4 */
#define STM32F4_UART_MAX_INSTANCES  6

/* STM32F4 APB clock frequencies (default with HSI) */
#define STM32F4_APB1_FREQ       16000000U   /**< APB1 clock (default HSI) */
#define STM32F4_APB2_FREQ       16000000U   /**< APB2 clock (default HSI) */

/* ======================================================================
 *               STM32F4 USART Register Structure (legacy layout)
 *
 * STM32F4 uses SR/DR instead of ISR/ICR/RDR/TDR
 * ====================================================================== */

typedef struct {
    volatile uint32_t SR;       /**< Status register */
    volatile uint32_t DR;       /**< Data register */
    volatile uint32_t BRR;      /**< Baud rate register */
    volatile uint32_t CR1;      /**< Control register 1 */
    volatile uint32_t CR2;      /**< Control register 2 */
    volatile uint32_t CR3;      /**< Control register 3 */
    volatile uint32_t GTPR;     /**< Guard time and prescaler register */
} STM32F4_USART_TypeDef;

/* USART SR register bits (STM32F4) */
#define STM32F4_USART_SR_TXE        (1U << 7)   /**< Transmit data register empty */
#define STM32F4_USART_SR_RXNE       (1U << 5)   /**< Read data register not empty */
#define STM32F4_USART_SR_TC         (1U << 6)   /**< Transmission complete */
#define STM32F4_USART_SR_ORE        (1U << 3)   /**< Overrun error */
#define STM32F4_USART_SR_FE         (1U << 1)   /**< Framing error */
#define STM32F4_USART_SR_PE         (1U << 0)   /**< Parity error */
#define STM32F4_USART_SR_IDLE       (1U << 4)   /**< Idle line detected */

/* USART CR1 register bits (STM32F4) */
#define STM32F4_USART_CR1_UE        (1U << 13)  /**< USART enable */
#define STM32F4_USART_CR1_RE        (1U << 2)   /**< Receiver enable */
#define STM32F4_USART_CR1_TE        (1U << 3)   /**< Transmitter enable */
#define STM32F4_USART_CR1_PCE       (1U << 10)  /**< Parity control enable */
#define STM32F4_USART_CR1_PS        (1U << 9)   /**< Parity selection */
#define STM32F4_USART_CR1_M         (1U << 12)  /**< Word length (0=8, 1=9) */
#define STM32F4_USART_CR1_RXNEIE    (1U << 5)   /**< RXNE interrupt enable */
#define STM32F4_USART_CR1_TXEIE     (1U << 7)   /**< TXE interrupt enable */
#define STM32F4_USART_CR1_TCIE      (1U << 6)   /**< TC interrupt enable */
#define STM32F4_USART_CR1_IDLEIE    (1U << 4)   /**< IDLE interrupt enable */

/* USART CR2 register bits (STM32F4) */
#define STM32F4_USART_CR2_STOP_Pos  12
#define STM32F4_USART_CR2_STOP_Msk  (0x3U << STM32F4_USART_CR2_STOP_Pos)
#define STM32F4_USART_CR2_STOP_1BIT (0x0U << STM32F4_USART_CR2_STOP_Pos)
#define STM32F4_USART_CR2_STOP_2BIT (0x2U << STM32F4_USART_CR2_STOP_Pos)

/* USART CR3 register bits (STM32F4) */
#define STM32F4_USART_CR3_RTSE      (1U << 8)   /**< RTS enable */
#define STM32F4_USART_CR3_CTSE      (1U << 9)   /**< CTS enable */
#define STM32F4_USART_CR3_EIE       (1U << 0)   /**< Error interrupt enable */

#endif // DMUART_STM32F4_REGS_H
