#ifndef ESP32S3_UART_H
#define ESP32S3_UART_H

#include <stdint.h>

/*
 * ESP32-S3: three UARTs, with a 128-byte FIFO in each direction, clocked from the XTAL (40 MHz) here, so the baud rate does
 * not depend on the CPU / APB frequency that dmclk sets. The signals are routed to the pads by the GPIO matrix.
 */

#define ESP32S3_UART_COUNT              3U

static inline uint32_t esp32s3_uart_base(uint32_t n)
{
    return n == 0U ? 0x60000000UL : n == 1U ? 0x60010000UL : 0x6002E000UL;
}

/* UART registers (offsets) */
#define UART_FIFO               0x00UL
#define UART_INT_RAW            0x04UL
#define UART_INT_ST             0x08UL
#define UART_INT_ENA            0x0CUL
#define UART_INT_CLR            0x10UL
#define UART_CLKDIV             0x14UL
#define UART_STATUS             0x1CUL
#define UART_CONF0              0x20UL
#define UART_CONF1              0x24UL
#define UART_FLOW_CONF          0x34UL
#define UART_MEM_CONF           0x60UL
#define UART_FSM_STATUS         0x6CUL
#define UART_CLK_CONF           0x78UL
#define UART_ID                 0x80UL

#define BIT(n)                  (1UL << (n))

/* INT_RAW / INT_ST / INT_ENA / INT_CLR */
#define UART_INT_RXFIFO_FULL    BIT(0)
#define UART_INT_TXFIFO_EMPTY   BIT(1)
#define UART_INT_PARITY_ERR     BIT(2)
#define UART_INT_FRM_ERR        BIT(3)
#define UART_INT_RXFIFO_OVF     BIT(4)
#define UART_INT_RXFIFO_TOUT    BIT(8)
#define UART_INT_TX_DONE        BIT(14)
#define UART_INT_ALL            0x000FFFFFUL

/* CLKDIV: [11:0] the integer part, [23:20] the fraction in 1/16 */
#define UART_CLKDIV_INT_M       0xFFFUL
#define UART_CLKDIV_FRAG_S      20U
/* STATUS */
#define UART_STATUS_RXFIFO_CNT_M    0x3FFUL
#define UART_STATUS_TXFIFO_CNT_S    16U
#define UART_STATUS_TXFIFO_CNT_M    (0x3FFUL << UART_STATUS_TXFIFO_CNT_S)
/* CONF0 */
#define UART_CONF0_PARITY_ODD   BIT(0)
#define UART_CONF0_PARITY_EN    BIT(1)
#define UART_CONF0_BIT_NUM_S    2U
#define UART_CONF0_BIT_NUM_M    (3UL << UART_CONF0_BIT_NUM_S)
#define UART_CONF0_STOP_S       4U
#define UART_CONF0_STOP_M       (3UL << UART_CONF0_STOP_S)
#define UART_CONF0_LOOPBACK     BIT(14)
#define UART_CONF0_TX_FLOW_EN   BIT(15)
#define UART_CONF0_RXFIFO_RST   BIT(17)
#define UART_CONF0_TXFIFO_RST   BIT(18)
#define UART_CONF0_RXD_INV      BIT(19)
#define UART_CONF0_TXD_INV      BIT(22)
#define UART_CONF0_CLK_EN       BIT(25)
/* CONF1 */
#define UART_CONF1_RXFIFO_FULL_THRHD_M  0x3FFUL
#define UART_CONF1_TXFIFO_EMPTY_THRHD_S 10U
#define UART_CONF1_RX_FLOW_EN   BIT(22)
#define UART_CONF1_RX_TOUT_EN   BIT(23)
/* MEM_CONF */
#define UART_MEM_CONF_RX_FLOW_THRHD_S   7U
#define UART_MEM_CONF_RX_TOUT_THRHD_S   17U
/* FSM_STATUS: [3:0] the receiver, [7:4] the transmitter (0: idle) */
#define UART_FSM_ST_UTX_OUT_M   0xF0UL
/* CLK_CONF */
#define UART_CLK_CONF_SCLK_DIV_NUM_S    12U
#define UART_CLK_CONF_SCLK_DIV_NUM_M    (0xFFUL << UART_CLK_CONF_SCLK_DIV_NUM_S)
#define UART_CLK_CONF_SCLK_SEL_S        20U
#define UART_CLK_CONF_SCLK_SEL_M        (3UL << UART_CLK_CONF_SCLK_SEL_S)
#define UART_CLK_CONF_SCLK_SEL_XTAL     3UL
#define UART_CLK_CONF_SCLK_EN           BIT(22)
#define UART_CLK_CONF_RST_CORE          BIT(23)
#define UART_CLK_CONF_TX_SCLK_EN        BIT(24)
#define UART_CLK_CONF_RX_SCLK_EN        BIT(25)
/* ID: write 1 to REG_UPDATE to copy the configuration to the clock domain of the core (self-clearing) */
#define UART_ID_REG_UPDATE      BIT(31)

#define UART_FIFO_SIZE          128U
#define UART_SCLK_HZ            40000000UL      /* XTAL */

/* SYSTEM: clocks and resets of the UARTs */
#define SYSTEM_BASE                 0x600C0000UL
#define SYSTEM_PERIP_CLK_EN0_REG    (SYSTEM_BASE + 0x18UL)
#define SYSTEM_PERIP_CLK_EN1_REG    (SYSTEM_BASE + 0x1CUL)
#define SYSTEM_PERIP_RST_EN0_REG    (SYSTEM_BASE + 0x20UL)
#define SYSTEM_PERIP_RST_EN1_REG    (SYSTEM_BASE + 0x24UL)
/* UART0 bit 2, UART1 bit 5 of EN0; UART2 bit 9 of EN1 (clock and reset alike) */

/* GPIO matrix and IO MUX (the same as the dmgpio port) */
#define GPIO_BASE                   0x60004000UL
#define GPIO_FUNC_IN_SEL_CFG(sig)   (GPIO_BASE + 0x154UL + 4UL * (sig))
#define GPIO_FUNC_OUT_SEL_CFG(pad)  (GPIO_BASE + 0x554UL + 4UL * (pad))
#define GPIO_IN_SEL_SIG_IN_SEL      BIT(7)      /* the signal is routed by the matrix */
#define GPIO_OUT_SEL_OEN_SEL        BIT(10)     /* 1: the output enable is the GPIO_ENABLE register, 0: the signal's own */
#define IO_MUX_REG(pad)             (0x60009004UL + 4UL * (pad))
#define IO_MUX_FUN_IE               BIT(9)
#define IO_MUX_MCU_SEL_S            12U
#define IO_MUX_MCU_SEL_M            (7UL << IO_MUX_MCU_SEL_S)
#define IO_MUX_FUNC_GPIO            1UL
#define GPIO_PAD_COUNT              49U

/* signal numbers of the matrix: UARTn TXD / RTS out, RXD / CTS in */
static inline uint32_t esp32s3_uart_sig_txd(uint32_t n) { return n == 0U ? 12U : n == 1U ? 15U : 18U; }
static inline uint32_t esp32s3_uart_sig_rts(uint32_t n) { return n == 0U ? 13U : n == 1U ? 16U : 19U; }
#define esp32s3_uart_sig_rxd    esp32s3_uart_sig_txd
#define esp32s3_uart_sig_cts    esp32s3_uart_sig_rts

/* the pads of the UART0 that the chip has by default (the ROM console) */
#define UART0_DEFAULT_TX_PAD    43
#define UART0_DEFAULT_RX_PAD    44

/* The interrupt matrix: the source of each UART goes to a level 1 interrupt of the CPU that nothing else takes (the GPIO of the
   dmgpio port has 2). Interrupts of the CPU: UART0 3, UART1 9, UART2 12. */
#define INTMTX_UART_MAP_REG(n)      (0x600C2000UL + 0x06CUL + 4UL * (n))
static inline uint32_t esp32s3_uart_cpu_intr(uint32_t n) { return n == 0U ? 3U : n == 1U ? 9U : 12U; }

#endif /* ESP32S3_UART_H */
