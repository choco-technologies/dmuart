#define DMOD_ENABLE_REGISTRATION    ON
#include "dmuart_port.h"
#include "dmod.h"
#include "dm_sw_ring.h"
#include "esp32s3_uart.h"
#include <errno.h>
#include <string.h>

/*
 * The UART port of the ESP32-S3 (UART0, UART1, UART2: instance 0, 1, 2).
 *
 * - The clock of the baud rate is the XTAL (40 MHz): it does not move when dmclk changes the CPU / APB frequency.
 * - The signals go to the pads through the GPIO matrix: dmuart_port_set_pins() (the keys tx_pin, rx_pin, rts_pin, cts_pin of the
 *   configuration); UART0 has the pads of the chip's console (GPIO43, GPIO44) when none are given.
 * - The receiver is served by the interrupt of the UART: the FIFO (128 bytes) is moved into the ring of the driver when it
 *   holds 32 bytes or when the line has been quiet for the time of ten bits.
 * - 9-bit words, MSB-first order and the DMA are not in the chip's UART (-ENOTSUP / nothing to do).
 */

#define RX_FULL_THRESHOLD       32U     /* bytes in the FIFO that call the interrupt */
#define RX_TIMEOUT_BITS         10U     /* quiet line (in bit times) that calls it for fewer bytes */
#define TX_SPIN_BAUD_MIN        460800U /* below it a wait for the FIFO sleeps, above it spins (the FIFO is quickly empty) */
#define WAIT_LOOPS_MAX          4000000U
#define WAIT_SLEEPS_MAX         2000U
#define RX_DRAIN_MAX            UART_FIFO_SIZE

typedef struct
{
    bool                            initialized;
    int8_t                          tx_pad, rx_pad, rts_pad, cts_pad;   /* -1: not given */
    dmuart_baudrate_t               baudrate;
    dmuart_port_interrupt_handler_t handler;
    void                           *user_ptr;
    dm_sw_ring_t                    ring;
    dmuart_int_trigger_t            trigger;
} uart_state_t;

static uart_state_t s_uart[ESP32S3_UART_COUNT] =
{
    { .tx_pad = -1, .rx_pad = -1, .rts_pad = -1, .cts_pad = -1 },
    { .tx_pad = -1, .rx_pad = -1, .rts_pad = -1, .cts_pad = -1 },
    { .tx_pad = -1, .rx_pad = -1, .rts_pad = -1, .cts_pad = -1 },
};

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    DMOD_LOG_INFO("DMUART port module initialized (ESP32-S3)\n");
    return 0;
}

int dmod_deinit(void)
{
    for (dmuart_instance_t i = 0; i < ESP32S3_UART_COUNT; i++)
    {
        if (s_uart[i].initialized)
            dmuart_port_deinit(i);
    }
    DMOD_LOG_INFO("DMUART port module deinitialized (ESP32-S3)\n");
    return 0;
}

/* ---- helpers ---- */

static inline volatile uint32_t *reg(uint32_t address)
{
    return (volatile uint32_t *)address;
}

static inline volatile uint32_t *ureg(dmuart_instance_t instance, uint32_t offset)
{
    return reg(esp32s3_uart_base(instance) + offset);
}

static int validate_instance(dmuart_instance_t instance)
{
    return instance < ESP32S3_UART_COUNT ? 0 : -1;
}

static inline uint32_t intenable_read(void)
{
    uint32_t value;
    __asm__ __volatile__("rsr.intenable %0" : "=a"(value));
    return value;
}

static inline void intenable_write(uint32_t value)
{
    __asm__ __volatile__("wsr.intenable %0; rsync" :: "a"(value) : "memory");
}

/** The configuration written to the registers is copied to the clock domain of the core. */
static void update_registers(dmuart_instance_t instance)
{
    *ureg(instance, UART_ID) |= UART_ID_REG_UPDATE;
    uint32_t guard = WAIT_LOOPS_MAX;
    while ((*ureg(instance, UART_ID) & UART_ID_REG_UPDATE) && --guard != 0U) { }
}

static int pad_valid(int pad)
{
    /* GPIO0-21 and GPIO26-48 exist */
    return (pad >= 0 && pad <= 21) || (pad >= 26 && pad < (int)GPIO_PAD_COUNT);
}

/** A pad as a GPIO of the matrix, input enabled (the level can be read back) */
static void pad_to_matrix(int pad)
{
    volatile uint32_t *r = reg(IO_MUX_REG((uint32_t)pad));
    *r = (*r & ~IO_MUX_MCU_SEL_M) | (IO_MUX_FUNC_GPIO << IO_MUX_MCU_SEL_S) | IO_MUX_FUN_IE;
}

static void route_output(int pad, uint32_t signal)
{
    pad_to_matrix(pad);
    *reg(GPIO_FUNC_OUT_SEL_CFG((uint32_t)pad)) = signal;        /* the output enable is the signal's own */
}

static void route_input(int pad, uint32_t signal)
{
    pad_to_matrix(pad);
    *reg(GPIO_FUNC_IN_SEL_CFG(signal)) = GPIO_IN_SEL_SIG_IN_SEL | (uint32_t)pad;
}

static void route_pins(dmuart_instance_t instance)
{
    const uart_state_t *u = &s_uart[instance];
    int tx = u->tx_pad, rx = u->rx_pad;
    if (instance == 0 && tx < 0 && rx < 0 && u->rts_pad < 0 && u->cts_pad < 0)
    {
        tx = UART0_DEFAULT_TX_PAD;
        rx = UART0_DEFAULT_RX_PAD;
    }
    if (tx >= 0)      route_output(tx, esp32s3_uart_sig_txd(instance));
    if (rx >= 0)      route_input(rx, esp32s3_uart_sig_rxd(instance));
    if (u->rts_pad >= 0) route_output(u->rts_pad, esp32s3_uart_sig_rts(instance));
    if (u->cts_pad >= 0) route_input(u->cts_pad, esp32s3_uart_sig_cts(instance));
}

static void periph_clock(dmuart_instance_t instance, bool enable)
{
    uint32_t en_reg = instance == 2 ? SYSTEM_PERIP_CLK_EN1_REG : SYSTEM_PERIP_CLK_EN0_REG;
    uint32_t bit = instance == 0 ? BIT(2) : instance == 1 ? BIT(5) : BIT(9);
    Dmod_EnterCritical();
    if (enable) *reg(en_reg) |= bit; else *reg(en_reg) &= ~bit;
    Dmod_ExitCritical();
}

/** Resets the UART: the core reset is raised before the module reset, as ESP-IDF does (else the line can output garbage). */
static void periph_reset(dmuart_instance_t instance)
{
    uint32_t rst_reg = instance == 2 ? SYSTEM_PERIP_RST_EN1_REG : SYSTEM_PERIP_RST_EN0_REG;
    uint32_t bit = instance == 0 ? BIT(2) : instance == 1 ? BIT(5) : BIT(9);
    Dmod_EnterCritical();
    *ureg(instance, UART_CLK_CONF) |= UART_CLK_CONF_RST_CORE;
    *reg(rst_reg) |= bit;
    *reg(rst_reg) &= ~bit;
    *ureg(instance, UART_CLK_CONF) &= ~UART_CLK_CONF_RST_CORE;
    Dmod_ExitCritical();
}

static void fifo_reset(dmuart_instance_t instance)
{
    *ureg(instance, UART_CONF0) |= UART_CONF0_RXFIFO_RST | UART_CONF0_TXFIFO_RST;
    *ureg(instance, UART_CONF0) &= ~(UART_CONF0_RXFIFO_RST | UART_CONF0_TXFIFO_RST);
}

static inline uint32_t rx_count(dmuart_instance_t instance)
{
    return *ureg(instance, UART_STATUS) & UART_STATUS_RXFIFO_CNT_M;
}

static inline uint32_t tx_count(dmuart_instance_t instance)
{
    return (*ureg(instance, UART_STATUS) & UART_STATUS_TXFIFO_CNT_M) >> UART_STATUS_TXFIFO_CNT_S;
}

static inline bool tx_idle(dmuart_instance_t instance)
{
    return tx_count(instance) == 0U && (*ureg(instance, UART_FSM_STATUS) & UART_FSM_ST_UTX_OUT_M) == 0U;
}

/** Turns the interrupt line of the instance on or off at the CPU, and routes the source of the UART to it. */
static void cpu_line(dmuart_instance_t instance, bool enable)
{
    uint32_t line = esp32s3_uart_cpu_intr(instance);
    Dmod_EnterCritical();
    if (enable)
    {
        *reg(INTMTX_UART_MAP_REG(instance)) = line;
        intenable_write(intenable_read() | BIT(line));
    }
    else
    {
        intenable_write(intenable_read() & ~BIT(line));
    }
    Dmod_ExitCritical();
}

/* ---- lifecycle ---- */

dmod_dmuart_port_api_declaration(1.0, int, _init, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;
    uart_state_t *u = &s_uart[instance];

    periph_reset(instance);
    periph_clock(instance, true);

    /* the clock of the baud rate: the XTAL */
    *ureg(instance, UART_CLK_CONF) = UART_CLK_CONF_SCLK_EN | UART_CLK_CONF_TX_SCLK_EN | UART_CLK_CONF_RX_SCLK_EN
                                   | (UART_CLK_CONF_SCLK_SEL_XTAL << UART_CLK_CONF_SCLK_SEL_S);

    /* 8N1, the registers clocked, FIFOs empty */
    *ureg(instance, UART_CONF0) = UART_CONF0_CLK_EN | (3UL << UART_CONF0_BIT_NUM_S) | (1UL << UART_CONF0_STOP_S);
    fifo_reset(instance);

    *ureg(instance, UART_CONF1) = RX_FULL_THRESHOLD | UART_CONF1_RX_TOUT_EN;
    *ureg(instance, UART_MEM_CONF) = (*ureg(instance, UART_MEM_CONF) & ~(0x3FFUL << UART_MEM_CONF_RX_TOUT_THRHD_S))
                                   | (RX_TIMEOUT_BITS << UART_MEM_CONF_RX_TOUT_THRHD_S);

    *ureg(instance, UART_INT_ENA) = 0U;
    *ureg(instance, UART_INT_CLR) = UART_INT_ALL;

    u->baudrate = 0U;
    u->trigger = dmuart_int_trigger_off;
    route_pins(instance);
    u->initialized = true;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _deinit, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;
    uart_state_t *u = &s_uart[instance];

    cpu_line(instance, false);
    *ureg(instance, UART_INT_ENA) = 0U;
    *ureg(instance, UART_INT_CLR) = UART_INT_ALL;
    u->trigger = dmuart_int_trigger_off;
    u->initialized = false;
    return 0;
}

/** Pads of the signals (-1: leave as it is). Before init(): the instance routes them when it starts. */
dmod_dmuart_port_api_declaration(1.0, int, _set_pins, ( dmuart_instance_t instance, int tx_pin, int rx_pin, int rts_pin, int cts_pin ))
{
    if (validate_instance(instance) != 0) return -1;
    if ((tx_pin >= 0 && !pad_valid(tx_pin)) || (rx_pin >= 0 && !pad_valid(rx_pin)) ||
        (rts_pin >= 0 && !pad_valid(rts_pin)) || (cts_pin >= 0 && !pad_valid(cts_pin)))
        return -EINVAL;

    uart_state_t *u = &s_uart[instance];
    if (tx_pin >= 0)  u->tx_pad = (int8_t)tx_pin;
    if (rx_pin >= 0)  u->rx_pad = (int8_t)rx_pin;
    if (rts_pin >= 0) u->rts_pad = (int8_t)rts_pin;
    if (cts_pin >= 0) u->cts_pad = (int8_t)cts_pin;

    if (u->initialized)
    {
        route_pins(instance);
    }
    /* hardware flow control: the RTS pad stops the sender when the FIFO is nearly full, the CTS pad lets us send */
    if (u->initialized && u->rts_pad >= 0)
    {
        *ureg(instance, UART_MEM_CONF) = (*ureg(instance, UART_MEM_CONF) & ~(0x3FFUL << UART_MEM_CONF_RX_FLOW_THRHD_S))
                                       | ((UART_FIFO_SIZE - 16U) << UART_MEM_CONF_RX_FLOW_THRHD_S);
        *ureg(instance, UART_CONF1) |= UART_CONF1_RX_FLOW_EN;
    }
    if (u->initialized && u->cts_pad >= 0)
        *ureg(instance, UART_CONF0) |= UART_CONF0_TX_FLOW_EN;
    return 0;
}

/* ---- data transfer ---- */

/** Waits for a place in the TX FIFO. Sleeps when the baud rate is low (the FIFO takes long to empty), else spins. */
static int wait_tx_room(dmuart_instance_t instance)
{
    const uart_state_t *u = &s_uart[instance];
    uint32_t loops = WAIT_LOOPS_MAX;
    uint32_t sleeps = WAIT_SLEEPS_MAX;
    while (tx_count(instance) >= UART_FIFO_SIZE)
    {
        if (u->baudrate != 0U && u->baudrate < TX_SPIN_BAUD_MIN)
        {
            if (--sleeps == 0U) return -ETIMEDOUT;
            Dmod_ThreadSleep(1);
        }
        else if (--loops == 0U)
        {
            return -ETIMEDOUT;
        }
    }
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _transmit, ( dmuart_instance_t instance, const uint8_t* data, size_t size ))
{
    if (validate_instance(instance) != 0 || (data == NULL && size != 0U)) return -1;

    for (size_t i = 0; i < size; i++)
    {
        if (wait_tx_room(instance) != 0) return -1;
        *ureg(instance, UART_FIFO) = data[i];
    }

    /* wait for the last bit to leave */
    const uart_state_t *u = &s_uart[instance];
    uint32_t loops = WAIT_LOOPS_MAX;
    uint32_t sleeps = WAIT_SLEEPS_MAX;
    while (!tx_idle(instance))
    {
        if (u->baudrate != 0U && u->baudrate < TX_SPIN_BAUD_MIN)
        {
            if (--sleeps == 0U) return -1;
            Dmod_ThreadSleep(1);
        }
        else if (--loops == 0U)
        {
            return -1;
        }
    }
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _receive, ( dmuart_instance_t instance, uint8_t* data, size_t size, size_t* received ))
{
    if (validate_instance(instance) != 0 || data == NULL || received == NULL) return -1;
    uart_state_t *u = &s_uart[instance];

    if (u->ring != NULL)
    {
        *received = (size_t)dm_sw_ring_read(u->ring, data, (dm_sw_ring_capacity_t)size);
        return 0;
    }

    /* no ring: the bytes that are in the FIFO (and those that come in the next few milliseconds) */
    *received = 0;
    uint32_t sleeps = 20U;
    while (*received < size)
    {
        uint32_t n = rx_count(instance);
        if (n == 0U)
        {
            if (*received != 0U || --sleeps == 0U) break;
            Dmod_ThreadSleep(1);
            continue;
        }
        while (n-- != 0U && *received < size)
            data[(*received)++] = (uint8_t)(*ureg(instance, UART_FIFO) & 0xFFU);
    }

    uint32_t status = *ureg(instance, UART_INT_RAW);
    if (status & (UART_INT_PARITY_ERR | UART_INT_FRM_ERR | UART_INT_RXFIFO_OVF))
    {
        *ureg(instance, UART_INT_CLR) = UART_INT_PARITY_ERR | UART_INT_FRM_ERR | UART_INT_RXFIFO_OVF;
        return -1;
    }
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _flush, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;
    const uart_state_t *u = &s_uart[instance];

    uint32_t loops = WAIT_LOOPS_MAX;
    uint32_t sleeps = WAIT_SLEEPS_MAX;
    while (!tx_idle(instance))
    {
        if (u->baudrate != 0U && u->baudrate < TX_SPIN_BAUD_MIN)
        {
            if (--sleeps == 0U) return -1;
            Dmod_ThreadSleep(1);
        }
        else if (--loops == 0U)
        {
            return -1;
        }
    }

    /* discard what has been received, and the error flags */
    *ureg(instance, UART_CONF0) |= UART_CONF0_RXFIFO_RST;
    *ureg(instance, UART_CONF0) &= ~UART_CONF0_RXFIFO_RST;
    *ureg(instance, UART_INT_CLR) = UART_INT_PARITY_ERR | UART_INT_FRM_ERR | UART_INT_RXFIFO_OVF;
    return 0;
}

/* ---- line parameters ---- */

dmod_dmuart_port_api_declaration(1.0, int, _set_baudrate, ( dmuart_instance_t instance, dmuart_baudrate_t baudrate ))
{
    if (validate_instance(instance) != 0 || baudrate == 0U) return -1;

    /* 12 bits of the integer part and 4 of the fraction; below ~9.8 kbaud the source clock is divided as well */
    uint32_t max_div = UART_CLKDIV_INT_M;
    uint32_t sclk_div = (UART_SCLK_HZ + (uint64_t)max_div * baudrate - 1U) / ((uint64_t)max_div * baudrate);
    if (sclk_div == 0U || sclk_div > 256U) return -ERANGE;
    uint32_t clk_div = (uint32_t)(((uint64_t)UART_SCLK_HZ << 4) / ((uint64_t)baudrate * sclk_div));
    if ((clk_div >> 4) == 0U) return -ERANGE;                   /* faster than the source clock allows */

    *ureg(instance, UART_CLKDIV) = (clk_div >> 4) | ((clk_div & 0xFU) << UART_CLKDIV_FRAG_S);
    *ureg(instance, UART_CLK_CONF) = (*ureg(instance, UART_CLK_CONF) & ~UART_CLK_CONF_SCLK_DIV_NUM_M)
                                   | ((sclk_div - 1U) << UART_CLK_CONF_SCLK_DIV_NUM_S);
    update_registers(instance);
    s_uart[instance].baudrate = baudrate;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_baudrate_t, _get_baudrate, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return 0;
    uint32_t div = *ureg(instance, UART_CLKDIV);
    uint32_t clk_div = ((div & UART_CLKDIV_INT_M) << 4) | ((div >> UART_CLKDIV_FRAG_S) & 0xFU);
    uint32_t sclk_div = ((*ureg(instance, UART_CLK_CONF) & UART_CLK_CONF_SCLK_DIV_NUM_M) >> UART_CLK_CONF_SCLK_DIV_NUM_S) + 1U;
    if (clk_div == 0U) return 0;
    return (dmuart_baudrate_t)(((uint64_t)UART_SCLK_HZ << 4) / ((uint64_t)clk_div * sclk_div));
}

dmod_dmuart_port_api_declaration(1.0, int, _set_word_length, ( dmuart_instance_t instance, dmuart_word_length_t word_length ))
{
    if (validate_instance(instance) != 0) return -1;
    uint32_t bits;
    switch (word_length)
    {
        case dmuart_word_length_7: bits = 2UL; break;
        case dmuart_word_length_8: bits = 3UL; break;
        default:                   return -ENOTSUP;     /* the chip has 5 to 8 bits */
    }
    *ureg(instance, UART_CONF0) = (*ureg(instance, UART_CONF0) & ~UART_CONF0_BIT_NUM_M) | (bits << UART_CONF0_BIT_NUM_S);
    update_registers(instance);
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_word_length_t, _get_word_length, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_word_length_8;
    return ((*ureg(instance, UART_CONF0) & UART_CONF0_BIT_NUM_M) >> UART_CONF0_BIT_NUM_S) == 2UL ? dmuart_word_length_7 : dmuart_word_length_8;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_parity, ( dmuart_instance_t instance, dmuart_parity_t parity ))
{
    if (validate_instance(instance) != 0) return -1;
    uint32_t conf0 = *ureg(instance, UART_CONF0) & ~(UART_CONF0_PARITY_EN | UART_CONF0_PARITY_ODD);
    if (parity == dmuart_parity_even)     conf0 |= UART_CONF0_PARITY_EN;
    else if (parity == dmuart_parity_odd) conf0 |= UART_CONF0_PARITY_EN | UART_CONF0_PARITY_ODD;
    *ureg(instance, UART_CONF0) = conf0;
    update_registers(instance);
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_parity_t, _get_parity, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_parity_none;
    uint32_t conf0 = *ureg(instance, UART_CONF0);
    if (!(conf0 & UART_CONF0_PARITY_EN)) return dmuart_parity_none;
    return (conf0 & UART_CONF0_PARITY_ODD) ? dmuart_parity_odd : dmuart_parity_even;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_stop_bit, ( dmuart_instance_t instance, dmuart_stop_bit_t stop_bit ))
{
    if (validate_instance(instance) != 0) return -1;
    uint32_t value = stop_bit == dmuart_stop_bit_2 ? 3UL : 1UL;     /* 1: one stop bit, 2: one and a half, 3: two */
    *ureg(instance, UART_CONF0) = (*ureg(instance, UART_CONF0) & ~UART_CONF0_STOP_M) | (value << UART_CONF0_STOP_S);
    update_registers(instance);
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_stop_bit_t, _get_stop_bit, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_stop_bit_1;
    return ((*ureg(instance, UART_CONF0) & UART_CONF0_STOP_M) >> UART_CONF0_STOP_S) == 3UL ? dmuart_stop_bit_2 : dmuart_stop_bit_1;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_bit_order, ( dmuart_instance_t instance, dmuart_bit_order_t bit_order ))
{
    if (validate_instance(instance) != 0) return -1;
    return bit_order == dmuart_bit_order_lsb_first ? 0 : -ENOTSUP;      /* the chip sends the LSB first only */
}

dmod_dmuart_port_api_declaration(1.0, dmuart_bit_order_t, _get_bit_order, ( dmuart_instance_t instance ))
{
    (void)instance;
    return dmuart_bit_order_lsb_first;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_invert, ( dmuart_instance_t instance, dmuart_invert_t invert ))
{
    if (validate_instance(instance) != 0) return -1;
    uint32_t conf0 = *ureg(instance, UART_CONF0) & ~(UART_CONF0_TXD_INV | UART_CONF0_RXD_INV);
    if (invert & dmuart_invert_tx) conf0 |= UART_CONF0_TXD_INV;
    if (invert & dmuart_invert_rx) conf0 |= UART_CONF0_RXD_INV;
    *ureg(instance, UART_CONF0) = conf0;
    update_registers(instance);
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_invert_t, _get_invert, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_invert_none;
    uint32_t conf0 = *ureg(instance, UART_CONF0);
    int result = dmuart_invert_none;
    if (conf0 & UART_CONF0_TXD_INV) result |= dmuart_invert_tx;
    if (conf0 & UART_CONF0_RXD_INV) result |= dmuart_invert_rx;
    return (dmuart_invert_t)result;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_loopback, ( dmuart_instance_t instance, dmuart_loopback_t loopback ))
{
    if (validate_instance(instance) != 0) return -1;
    if (loopback == dmuart_loopback_on) *ureg(instance, UART_CONF0) |= UART_CONF0_LOOPBACK;
    else                                *ureg(instance, UART_CONF0) &= ~UART_CONF0_LOOPBACK;
    update_registers(instance);
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_loopback_t, _get_loopback, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_loopback_off;
    return (*ureg(instance, UART_CONF0) & UART_CONF0_LOOPBACK) ? dmuart_loopback_on : dmuart_loopback_off;
}

/* ---- interrupts ---- */

static uint32_t trigger_to_mask(dmuart_int_trigger_t trigger)
{
    uint32_t mask = 0U;
    if (trigger & dmuart_int_trigger_rx_not_empty) mask |= UART_INT_RXFIFO_FULL | UART_INT_RXFIFO_TOUT;
    if (trigger & dmuart_int_trigger_tx_empty)     mask |= UART_INT_TXFIFO_EMPTY;
    if (trigger & dmuart_int_trigger_tx_complete)  mask |= UART_INT_TX_DONE;
    if (trigger & dmuart_int_trigger_idle)         mask |= UART_INT_RXFIFO_TOUT;
    if (trigger & dmuart_int_trigger_error)        mask |= UART_INT_PARITY_ERR | UART_INT_FRM_ERR | UART_INT_RXFIFO_OVF;
    return mask;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t trigger ))
{
    if (validate_instance(instance) != 0) return -1;
    uart_state_t *u = &s_uart[instance];

    u->trigger = trigger;
    uint32_t mask = trigger_to_mask(trigger);
    *ureg(instance, UART_INT_CLR) = UART_INT_ALL;
    *ureg(instance, UART_INT_ENA) = mask;
    cpu_line(instance, mask != 0U);
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _read_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t *out_trigger ))
{
    if (validate_instance(instance) != 0 || out_trigger == NULL) return -1;
    *out_trigger = s_uart[instance].trigger;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _add_interrupt_handler,
    ( dmuart_instance_t instance, dmuart_port_interrupt_handler_t handler, void *user_ptr ))
{
    if (validate_instance(instance) != 0 || handler == NULL) return -1;
    s_uart[instance].user_ptr = user_ptr;
    s_uart[instance].handler = handler;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _remove_interrupt_handler, ( dmuart_instance_t instance, void *user_ptr ))
{
    if (validate_instance(instance) != 0) return -1;
    if (s_uart[instance].user_ptr == user_ptr)
    {
        s_uart[instance].handler = NULL;
        s_uart[instance].user_ptr = NULL;
    }
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_rx_ring, ( dmuart_instance_t instance, dm_sw_ring_t ring ))
{
    if (validate_instance(instance) != 0) return -1;
    s_uart[instance].ring = ring;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_rx_dma, ( dmuart_instance_t instance, bool enabled ))
{
    (void)enabled;      /* the receiver of this UART has a FIFO and an interrupt, no DMA is used */
    return validate_instance(instance);
}

/* ---- the interrupt handlers (one line of the CPU per UART, see esp32s3_uart.h) ---- */

static void uart_irq_handler(dmuart_instance_t instance)
{
    uart_state_t *u = &s_uart[instance];
    uint32_t st = *ureg(instance, UART_INT_ST);
    if (st == 0U) return;

    dmuart_int_trigger_t trigger = dmuart_int_trigger_off;
    uint8_t data = 0;

    if (st & (UART_INT_RXFIFO_FULL | UART_INT_RXFIFO_TOUT))
    {
        /* the FIFO into the ring, whatever it holds */
        uint8_t buf[RX_DRAIN_MAX];
        uint32_t n = rx_count(instance);
        if (n > RX_DRAIN_MAX) n = RX_DRAIN_MAX;
        for (uint32_t i = 0; i < n; i++)
            buf[i] = (uint8_t)(*ureg(instance, UART_FIFO) & 0xFFU);
        if (n != 0U)
        {
            data = buf[n - 1U];
            if (u->ring != NULL)
                dm_sw_ring_write(u->ring, buf, (dm_sw_ring_capacity_t)n);
            if (u->trigger & dmuart_int_trigger_rx_not_empty)
                trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_rx_not_empty);
        }
        if ((st & UART_INT_RXFIFO_TOUT) && (u->trigger & dmuart_int_trigger_idle))
            trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_idle);
    }
    if (st & UART_INT_TXFIFO_EMPTY)
    {
        /* the condition lasts while the FIFO is below the threshold: report it once, the trigger is armed again by
           dmuart_port_set_interrupt_trigger() */
        *ureg(instance, UART_INT_ENA) &= ~UART_INT_TXFIFO_EMPTY;
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_tx_empty);
    }
    if (st & UART_INT_TX_DONE)
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_tx_complete);
    if (st & (UART_INT_PARITY_ERR | UART_INT_FRM_ERR | UART_INT_RXFIFO_OVF))
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_error);

    *ureg(instance, UART_INT_CLR) = st;

    dmuart_port_interrupt_handler_t handler = u->handler;
    if (handler != NULL && trigger != dmuart_int_trigger_off)
        handler(u->user_ptr, instance, trigger, data);
}

DMOD_IRQ_HANDLER(3)  { uart_irq_handler(0); }   /* UART0: interrupt 3 of the CPU */
DMOD_IRQ_HANDLER(9)  { uart_irq_handler(1); }   /* UART1: interrupt 9 */
DMOD_IRQ_HANDLER(12) { uart_irq_handler(2); }   /* UART2: interrupt 12 */
