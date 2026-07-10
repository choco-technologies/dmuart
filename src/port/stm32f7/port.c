#define DMOD_ENABLE_REGISTRATION    ON
#include "dmuart_port.h"
#include "dmod.h"
#include "dmosi.h"
#include "dm_sw_ring.h"
#include "dmclk_port.h"
#include "../stm32_common/stm32_common.h"
#include "port/stm32_common_regs.h"
#include "port/stm32f7_regs.h"

/* UART instance base addresses */
static const uint32_t uart_base_addresses[STM32F7_UART_MAX_INSTANCES] = {
    STM32F7_USART1_BASE,
    STM32F7_USART2_BASE,
    STM32F7_USART3_BASE,
    STM32F7_UART4_BASE,
    STM32F7_UART5_BASE,
    STM32F7_USART6_BASE,
    STM32F7_UART7_BASE,
    STM32F7_UART8_BASE,
};

/* Per-instance interrupt handler registration */
static dmuart_port_interrupt_handler_t irq_handlers[STM32F7_UART_MAX_INSTANCES];
static void *irq_user_ptrs[STM32F7_UART_MAX_INSTANCES];

/* Per-instance SW ring buffers — set via dmuart_port_set_rx_ring() */
static dm_sw_ring_t rx_rings[STM32F7_UART_MAX_INSTANCES];

/* NVIC IRQ numbers for STM32F7 UART instances (must match DMOD_IRQ_HANDLER list below) */
static const uint32_t uart_irqn[STM32F7_UART_MAX_INSTANCES] = {
    37, 38, 39, 52, 53, 71, 82, 83
};

static void nvic_enable_irq(uint32_t irqn)
{
    /* NVIC priority defaults to 0 (highest, non-maskable by FreeRTOS critical
     * sections) after reset. Any interrupt that may call dmosi/FreeRTOS
     * ISR-safe API (as this one does, via dm_sw_ring signaling a semaphore
     * from within the IRQ handler) must be configured at or below the
     * priority dmosi_get_min_interrupt_priority() reports, or the RTOS
     * cannot safely mask it during its own critical sections. */
    NVIC_IP[irqn] = (uint8_t)dmosi_get_min_interrupt_priority();
    NVIC_ISER[irqn >> 5U] = 1U << (irqn & 0x1FU);
}

static void nvic_disable_irq(uint32_t irqn)
{
    NVIC_ICER[irqn >> 5U] = 1U << (irqn & 0x1FU);
}

static int validate_instance(dmuart_instance_t instance)
{
    return (instance >= 1 && instance <= STM32F7_UART_MAX_INSTANCES) ? 0 : -1;
}

static volatile USART_TypeDef *get_usart(dmuart_instance_t instance)
{
    return (volatile USART_TypeDef *)uart_base_addresses[instance - 1];
}

/* Convert a 3-bit RCC_CFGR PPREx field to its division factor.
 * Encoding (same on STM32F4/F7): MSB=0 -> not divided (/1);
 * 100->/2, 101->/4, 110->/8, 111->/16. */
static uint32_t apb_prescaler_div(uint32_t ppre_bits)
{
    if ((ppre_bits & 0x4U) == 0U) return 1U;
    return 2U << (ppre_bits & 0x3U);
}

static uint32_t get_uart_clock(dmuart_instance_t instance)
{
    volatile UART_RCC_TypeDef *RCC = (UART_RCC_TypeDef *)STM32F7_RCC_BASE;
    uint32_t cfgr = RCC->CFGR;

    /* Query the real, currently-configured core clock from dmclk instead of
     * assuming the reset-default HSI — dmclk may have switched to a PLL/HSE
     * derived frequency before this module is configured. */
    uint32_t sysclk = (uint32_t)dmclk_port_get_current_frequency();
    if (sysclk == 0U)
        sysclk = HSI_VALUE; /* defensive fallback, should not normally happen */

    /* USART1 and USART6 are on APB2, others are on APB1 */
    if (instance == 1 || instance == 6)
    {
        uint32_t ppre2 = (cfgr & RCC_CFGR_PPRE2_Msk) >> RCC_CFGR_PPRE2_Pos;
        return sysclk / apb_prescaler_div(ppre2);
    }

    uint32_t ppre1 = (cfgr & RCC_CFGR_PPRE1_Msk) >> RCC_CFGR_PPRE1_Pos;
    return sysclk / apb_prescaler_div(ppre1);
}

static void enable_uart_clock(dmuart_instance_t instance)
{
    volatile UART_RCC_TypeDef *RCC = (UART_RCC_TypeDef *)STM32F7_RCC_BASE;

    switch (instance)
    {
        case 1: RCC->APB2ENR |= RCC_APB2ENR_USART1EN; break;
        case 2: RCC->APB1ENR |= RCC_APB1ENR_USART2EN; break;
        case 3: RCC->APB1ENR |= RCC_APB1ENR_USART3EN; break;
        case 4: RCC->APB1ENR |= RCC_APB1ENR_UART4EN; break;
        case 5: RCC->APB1ENR |= RCC_APB1ENR_UART5EN; break;
        case 6: RCC->APB2ENR |= RCC_APB2ENR_USART6EN; break;
        case 7: RCC->APB1ENR |= RCC_APB1ENR_UART7EN; break;
        case 8: RCC->APB1ENR |= RCC_APB1ENR_UART8EN; break;
        default: break;
    }
}

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMUART port module initialized (STM32F7)\n");
    for (int i = 0; i < STM32F7_UART_MAX_INSTANCES; i++)
    {
        irq_handlers[i] = NULL;
        irq_user_ptrs[i] = NULL;
        rx_rings[i]      = NULL;
    }
    return 0;
}

int dmod_deinit(void)
{
    Dmod_Printf("DMUART port module deinitialized (STM32F7)\n");
    return 0;
}

/* ---- Port API implementation ---- */

dmod_dmuart_port_api_declaration(1.0, int, _init, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;

    enable_uart_clock(instance);

    volatile USART_TypeDef *USART = get_usart(instance);

    /* Disable USART for configuration */
    USART->CR1 &= ~USART_CR1_UE;

    /* Default: 8N1, no flow control */
    USART->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1 | USART_CR1_PCE | USART_CR1_PS);
    USART->CR2 &= ~USART_CR2_STOP_Msk;
    USART->CR3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE);

    /* Enable USART, transmitter and receiver */
    USART->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _deinit, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~(USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);

    nvic_disable_irq(uart_irqn[instance - 1]);

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _transmit, ( dmuart_instance_t instance, const uint8_t* data, size_t size ))
{
    if (validate_instance(instance) != 0 || data == NULL) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);

    for (size_t i = 0; i < size; i++)
    {
        uint32_t timeout = UART_TIMEOUT_VALUE;
        while (!(USART->ISR & USART_ISR_TXE))
        {
            if (--timeout == 0) return -1;
        }
        USART->TDR = data[i];
    }

    /* Wait for transmission complete */
    uint32_t timeout = UART_TIMEOUT_VALUE;
    while (!(USART->ISR & USART_ISR_TC))
    {
        if (--timeout == 0) return -1;
    }

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _receive, ( dmuart_instance_t instance, uint8_t* data, size_t size, size_t* received ))
{
    if (validate_instance(instance) != 0 || data == NULL || received == NULL) return -1;

    dm_sw_ring_t ring = rx_rings[instance - 1];
    if (ring != NULL)
    {
        *received = (size_t)dm_sw_ring_read(ring, data, (dm_sw_ring_capacity_t)size);
        return 0;
    }

    volatile USART_TypeDef *USART = get_usart(instance);
    *received = 0;

    for (size_t i = 0; i < size; i++)
    {
        uint32_t timeout = UART_TIMEOUT_VALUE;
        while (!(USART->ISR & USART_ISR_RXNE))
        {
            if (--timeout == 0) return 0;
        }

        if (USART->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_PE))
        {
            USART->ICR |= USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF;
            return -1;
        }

        data[i] = (uint8_t)(USART->RDR & 0xFF);
        (*received)++;
    }

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _flush, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);

    /* Wait for TX complete */
    uint32_t timeout = UART_TIMEOUT_VALUE;
    while (!(USART->ISR & USART_ISR_TC))
    {
        if (--timeout == 0) return -1;
    }

    /* Discard any pending RX data */
    while (USART->ISR & USART_ISR_RXNE)
    {
        (void)USART->RDR;
    }

    /* Clear any error flags */
    USART->ICR |= USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF;

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_baudrate, ( dmuart_instance_t instance, dmuart_baudrate_t baudrate ))
{
    if (validate_instance(instance) != 0 || baudrate == 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    uint32_t pclk = get_uart_clock(instance);
    USART->BRR = (pclk + (baudrate / 2)) / baudrate;

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_baudrate_t, _get_baudrate, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return 0;

    volatile USART_TypeDef *USART = get_usart(instance);
    uint32_t pclk = get_uart_clock(instance);
    uint32_t brr = USART->BRR;

    if (brr == 0) return 0;
    return (dmuart_baudrate_t)(pclk / brr);
}

dmod_dmuart_port_api_declaration(1.0, int, _set_word_length, ( dmuart_instance_t instance, dmuart_word_length_t word_length ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    USART->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1);
    if (word_length == dmuart_word_length_9)
    {
        USART->CR1 |= USART_CR1_M0;
    }
    else if (word_length == dmuart_word_length_7)
    {
        USART->CR1 |= USART_CR1_M1;
    }

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_word_length_t, _get_word_length, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_word_length_8;

    volatile USART_TypeDef *USART = get_usart(instance);

    if (USART->CR1 & USART_CR1_M0) return dmuart_word_length_9;
    if (USART->CR1 & USART_CR1_M1) return dmuart_word_length_7;
    return dmuart_word_length_8;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_parity, ( dmuart_instance_t instance, dmuart_parity_t parity ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    USART->CR1 &= ~(USART_CR1_PCE | USART_CR1_PS);
    if (parity == dmuart_parity_even)
    {
        USART->CR1 |= USART_CR1_PCE;
    }
    else if (parity == dmuart_parity_odd)
    {
        USART->CR1 |= USART_CR1_PCE | USART_CR1_PS;
    }

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_parity_t, _get_parity, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_parity_none;

    volatile USART_TypeDef *USART = get_usart(instance);

    if (!(USART->CR1 & USART_CR1_PCE)) return dmuart_parity_none;
    if (USART->CR1 & USART_CR1_PS) return dmuart_parity_odd;
    return dmuart_parity_even;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_stop_bit, ( dmuart_instance_t instance, dmuart_stop_bit_t stop_bit ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    USART->CR2 &= ~USART_CR2_STOP_Msk;
    if (stop_bit == dmuart_stop_bit_2)
    {
        USART->CR2 |= USART_CR2_STOP_2BIT;
    }

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_stop_bit_t, _get_stop_bit, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_stop_bit_1;

    volatile USART_TypeDef *USART = get_usart(instance);

    if ((USART->CR2 & USART_CR2_STOP_Msk) == USART_CR2_STOP_2BIT)
        return dmuart_stop_bit_2;
    return dmuart_stop_bit_1;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_bit_order, ( dmuart_instance_t instance, dmuart_bit_order_t bit_order ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    /* STM32F7 CR2 bit 19 = MSBFIRST */
    if (bit_order == dmuart_bit_order_msb_first)
    {
        USART->CR2 |= (1U << 19);
    }
    else
    {
        USART->CR2 &= ~(1U << 19);
    }

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_bit_order_t, _get_bit_order, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_bit_order_lsb_first;

    volatile USART_TypeDef *USART = get_usart(instance);

    if (USART->CR2 & (1U << 19)) return dmuart_bit_order_msb_first;
    return dmuart_bit_order_lsb_first;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_invert, ( dmuart_instance_t instance, dmuart_invert_t invert ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    /* STM32F7 CR2: bit 17 = TXINV, bit 16 = RXINV */
    USART->CR2 &= ~((1U << 17) | (1U << 16));
    if (invert & dmuart_invert_tx) USART->CR2 |= (1U << 17);
    if (invert & dmuart_invert_rx) USART->CR2 |= (1U << 16);

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_invert_t, _get_invert, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_invert_none;

    volatile USART_TypeDef *USART = get_usart(instance);
    dmuart_invert_t result = dmuart_invert_none;

    if (USART->CR2 & (1U << 17)) result = (dmuart_invert_t)(result | dmuart_invert_tx);
    if (USART->CR2 & (1U << 16)) result = (dmuart_invert_t)(result | dmuart_invert_rx);
    return result;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_loopback, ( dmuart_instance_t instance, dmuart_loopback_t loopback ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~USART_CR1_UE;

    /* STM32F7 does not have a true internal loopback mode.
     * SWAP bit (CR2 bit 15) swaps TX/RX pin assignment, which combined with
     * external loopback wiring can be useful for testing. */
    if (loopback == dmuart_loopback_on)
    {
        USART->CR2 |= (1U << 15); /* SWAP */
    }
    else
    {
        USART->CR2 &= ~(1U << 15);
    }

    USART->CR1 |= USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_loopback_t, _get_loopback, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_loopback_off;

    volatile USART_TypeDef *USART = get_usart(instance);

    if (USART->CR2 & (1U << 15)) return dmuart_loopback_on;
    return dmuart_loopback_off;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t trigger ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);

    /* Clear all interrupt enable bits first */
    USART->CR1 &= ~((1U << 5) | (1U << 7) | (1U << 6) | (1U << 4));  /* RXNEIE, TXEIE, TCIE, IDLEIE */
    USART->CR3 &= ~(1U << 0);  /* EIE */

    /* Set requested triggers */
    if (trigger & dmuart_int_trigger_rx_not_empty)
        USART->CR1 |= (1U << 5);  /* RXNEIE */
    if (trigger & dmuart_int_trigger_tx_empty)
        USART->CR1 |= (1U << 7);  /* TXEIE */
    if (trigger & dmuart_int_trigger_tx_complete)
        USART->CR1 |= (1U << 6);  /* TCIE */
    if (trigger & dmuart_int_trigger_idle)
        USART->CR1 |= (1U << 4);  /* IDLEIE */
    if (trigger & dmuart_int_trigger_error)
        USART->CR3 |= (1U << 0);  /* EIE */

    /* Peripheral-level enable bits above only make the USART assert its
     * interrupt line; the NVIC must also unmask that line or the CPU will
     * never service it. */
    if (trigger != dmuart_int_trigger_off)
        nvic_enable_irq(uart_irqn[instance - 1]);
    else
        nvic_disable_irq(uart_irqn[instance - 1]);

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _read_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t *out_trigger ))
{
    if (validate_instance(instance) != 0 || out_trigger == NULL) return -1;

    volatile USART_TypeDef *USART = get_usart(instance);
    *out_trigger = dmuart_int_trigger_off;

    if (USART->CR1 & (1U << 5)) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_rx_not_empty);
    if (USART->CR1 & (1U << 7)) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_tx_empty);
    if (USART->CR1 & (1U << 6)) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_tx_complete);
    if (USART->CR1 & (1U << 4)) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_idle);
    if (USART->CR3 & (1U << 0)) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_error);

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _add_interrupt_handler,
    ( dmuart_instance_t instance, dmuart_port_interrupt_handler_t handler, void *user_ptr ))
{
    if (validate_instance(instance) != 0 || handler == NULL) return -1;

    irq_handlers[instance - 1] = handler;
    irq_user_ptrs[instance - 1] = user_ptr;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _remove_interrupt_handler,
    ( dmuart_instance_t instance, void *user_ptr ))
{
    if (validate_instance(instance) != 0) return -1;

    if (irq_user_ptrs[instance - 1] == user_ptr)
    {
        irq_handlers[instance - 1] = NULL;
        irq_user_ptrs[instance - 1] = NULL;
    }
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_rx_ring,
    ( dmuart_instance_t instance, dm_sw_ring_t ring ))
{
    if (validate_instance(instance) != 0) return -1;
    rx_rings[instance - 1] = ring;
    return 0;
}

/* ---- ISR handlers ---- */

/* Upper bound on bytes drained from RDR per ISR entry. RDR itself holds only
 * one byte at a time, but servicing latency can let several bytes complete
 * reception before the ISR gets to run, so we keep draining while RXNE is
 * set instead of handling a single byte per interrupt. */
#define STM32F7_UART_RX_DRAIN_MAX   32U

static void stm32f7_uart_irq_handler(dmuart_instance_t instance)
{
    volatile USART_TypeDef *USART = get_usart(instance);
    uint32_t idx = (uint32_t)(instance - 1);

    uint8_t data = 0;
    dmuart_int_trigger_t trigger = dmuart_int_trigger_off;

    if (USART->ISR & USART_ISR_RXNE)
    {
        uint8_t rx_buf[STM32F7_UART_RX_DRAIN_MAX];
        uint32_t rx_count = 0;

        while ((USART->ISR & USART_ISR_RXNE) && rx_count < STM32F7_UART_RX_DRAIN_MAX)
        {
            rx_buf[rx_count++] = (uint8_t)(USART->RDR & 0xFF);  /* reading RDR clears RXNE */
        }

        data = rx_buf[rx_count - 1];
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_rx_not_empty);

        if (rx_rings[idx] != NULL)
            dm_sw_ring_write(rx_rings[idx], rx_buf, (dm_sw_ring_capacity_t)rx_count);
    }
    if (USART->ISR & USART_ISR_TXE)
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_tx_empty);
    if (USART->ISR & USART_ISR_TC)
    {
        USART->ICR |= (1U << 6); /* TCCF */
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_tx_complete);
    }
    if (USART->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_PE))
    {
        USART->ICR |= USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF;
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_error);
    }

    dmuart_port_interrupt_handler_t handler = irq_handlers[idx];
    if (handler != NULL && trigger != dmuart_int_trigger_off)
        handler(irq_user_ptrs[idx], instance, trigger, data);
}

/* NVIC IRQ numbers for STM32F7 UART instances */
DMOD_IRQ_HANDLER(37) { stm32f7_uart_irq_handler(1); }  /* USART1 */
DMOD_IRQ_HANDLER(38) { stm32f7_uart_irq_handler(2); }  /* USART2 */
DMOD_IRQ_HANDLER(39) { stm32f7_uart_irq_handler(3); }  /* USART3 */
DMOD_IRQ_HANDLER(52) { stm32f7_uart_irq_handler(4); }  /* UART4 */
DMOD_IRQ_HANDLER(53) { stm32f7_uart_irq_handler(5); }  /* UART5 */
DMOD_IRQ_HANDLER(71) { stm32f7_uart_irq_handler(6); }  /* USART6 */
DMOD_IRQ_HANDLER(82) { stm32f7_uart_irq_handler(7); }  /* UART7 */
DMOD_IRQ_HANDLER(83) { stm32f7_uart_irq_handler(8); }  /* UART8 */
