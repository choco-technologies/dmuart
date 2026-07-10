#ifndef DMUART_STM32_COMMON_REGS_H
#define DMUART_STM32_COMMON_REGS_H

#include <stdint.h>

/* ======================================================================
 *               Common STM32 USART Register Definitions
 * ====================================================================== */

/**
 * @brief USART register structure (common across STM32 families)
 */
typedef struct {
    volatile uint32_t CR1;      /**< Control register 1 */
    volatile uint32_t CR2;      /**< Control register 2 */
    volatile uint32_t CR3;      /**< Control register 3 */
    volatile uint32_t BRR;      /**< Baud rate register */
    volatile uint32_t GTPR;     /**< Guard time and prescaler register */
    volatile uint32_t RTOR;     /**< Receiver timeout register */
    volatile uint32_t RQR;      /**< Request register */
    volatile uint32_t ISR;      /**< Interrupt and status register */
    volatile uint32_t ICR;      /**< Interrupt flag clear register */
    volatile uint32_t RDR;      /**< Receive data register */
    volatile uint32_t TDR;      /**< Transmit data register */
} USART_TypeDef;

/* USART CR1 register bits */
#define USART_CR1_UE            (1U << 0)   /**< USART enable */
#define USART_CR1_RE            (1U << 2)   /**< Receiver enable */
#define USART_CR1_TE            (1U << 3)   /**< Transmitter enable */
#define USART_CR1_PCE           (1U << 10)  /**< Parity control enable */
#define USART_CR1_PS            (1U << 9)   /**< Parity selection (0=even, 1=odd) */
#define USART_CR1_M0            (1U << 12)  /**< Word length bit 0 */
#define USART_CR1_M1            (1U << 28)  /**< Word length bit 1 */
#define USART_CR1_OVER8         (1U << 15)  /**< Oversampling mode */

/* USART CR2 register bits */
#define USART_CR2_STOP_Pos      12
#define USART_CR2_STOP_Msk      (0x3U << USART_CR2_STOP_Pos)
#define USART_CR2_STOP_1BIT     (0x0U << USART_CR2_STOP_Pos)
#define USART_CR2_STOP_2BIT     (0x2U << USART_CR2_STOP_Pos)

/* USART CR3 register bits */
#define USART_CR3_RTSE          (1U << 8)   /**< RTS enable */
#define USART_CR3_CTSE          (1U << 9)   /**< CTS enable */

/* USART ISR register bits */
#define USART_ISR_TXE           (1U << 7)   /**< Transmit data register empty */
#define USART_ISR_RXNE          (1U << 5)   /**< Read data register not empty */
#define USART_ISR_TC            (1U << 6)   /**< Transmission complete */
#define USART_ISR_ORE           (1U << 3)   /**< Overrun error */
#define USART_ISR_FE            (1U << 1)   /**< Framing error */
#define USART_ISR_PE            (1U << 0)   /**< Parity error */
#define USART_ISR_TEACK         (1U << 21)  /**< Transmit enable acknowledge */
#define USART_ISR_REACK         (1U << 22)  /**< Receive enable acknowledge */

/* USART ICR register bits */
#define USART_ICR_ORECF         (1U << 3)   /**< Overrun error clear flag */
#define USART_ICR_FECF          (1U << 1)   /**< Framing error clear flag */
#define USART_ICR_PECF          (1U << 0)   /**< Parity error clear flag */

/* RCC register structure (subset for UART clock enable) */
typedef struct {
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t AHB3RSTR;
    volatile uint32_t RESERVED0;
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t AHB3ENR;
    volatile uint32_t RESERVED2;
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
} UART_RCC_TypeDef;

/* RCC_CFGR APB prescaler fields (same layout on STM32F4/F7) */
#define RCC_CFGR_PPRE1_Pos      10U
#define RCC_CFGR_PPRE1_Msk      (0x7U << RCC_CFGR_PPRE1_Pos)
#define RCC_CFGR_PPRE2_Pos      13U
#define RCC_CFGR_PPRE2_Msk      (0x7U << RCC_CFGR_PPRE2_Pos)

/* Common clock values */
#define HSI_VALUE               16000000U   /**< HSI clock value in Hz */

/* Timeout values */
#define UART_TIMEOUT_VALUE      10000U      /**< Default timeout for UART operations */

/* NVIC Interrupt Set/Clear-Enable registers (ARMv7-M, common to Cortex-M4/M7) */
#define NVIC_ISER                ((volatile uint32_t *)0xE000E100UL)
#define NVIC_ICER                ((volatile uint32_t *)0xE000E180UL)

/* NVIC Interrupt Priority Registers (ARMv7-M, common to Cortex-M4/M7).
 * Byte-addressable, one 8-bit entry per external interrupt line. */
#define NVIC_IP                   ((volatile uint8_t *)0xE000E400UL)

#endif // DMUART_STM32_COMMON_REGS_H
