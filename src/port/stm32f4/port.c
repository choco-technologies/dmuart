#define DMOD_ENABLE_REGISTRATION    ON
#include "dmuart_port.h"
#include "port/stm32f4_regs.h"

/* UART instance base addresses */
static const uint32_t uart_base_addresses[STM32F4_UART_MAX_INSTANCES] = {
    STM32F4_USART1_BASE,
    STM32F4_USART2_BASE,
    STM32F4_USART3_BASE,
    STM32F4_UART4_BASE,
    STM32F4_UART5_BASE,
    STM32F4_USART6_BASE,
};

/* Per-instance interrupt handler registration */
static dmuart_port_interrupt_handler_t irq_handlers[STM32F4_UART_MAX_INSTANCES];
static void *irq_user_ptrs[STM32F4_UART_MAX_INSTANCES];

/* Timeout value */
#define STM32F4_UART_TIMEOUT    10000U

/* RCC register structure (subset) */
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
} STM32F4_RCC_TypeDef;

static int validate_instance(dmuart_instance_t instance)
{
    return (instance >= 1 && instance <= STM32F4_UART_MAX_INSTANCES) ? 0 : -1;
}

static volatile STM32F4_USART_TypeDef *get_usart(dmuart_instance_t instance)
{
    return (volatile STM32F4_USART_TypeDef *)uart_base_addresses[instance - 1];
}

static uint32_t get_uart_clock(dmuart_instance_t instance)
{
    if (instance == 1 || instance == 6)
    {
        return STM32F4_APB2_FREQ;
    }
    return STM32F4_APB1_FREQ;
}

static void enable_uart_clock(dmuart_instance_t instance)
{
    volatile STM32F4_RCC_TypeDef *RCC = (STM32F4_RCC_TypeDef *)STM32F4_RCC_BASE;

    switch (instance)
    {
        case 1: RCC->APB2ENR |= STM32F4_RCC_APB2ENR_USART1EN; break;
        case 2: RCC->APB1ENR |= STM32F4_RCC_APB1ENR_USART2EN; break;
        case 3: RCC->APB1ENR |= STM32F4_RCC_APB1ENR_USART3EN; break;
        case 4: RCC->APB1ENR |= STM32F4_RCC_APB1ENR_UART4EN; break;
        case 5: RCC->APB1ENR |= STM32F4_RCC_APB1ENR_UART5EN; break;
        case 6: RCC->APB2ENR |= STM32F4_RCC_APB2ENR_USART6EN; break;
        default: break;
    }
}

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMUART port module initialized (STM32F4)\n");
    for (int i = 0; i < STM32F4_UART_MAX_INSTANCES; i++)
    {
        irq_handlers[i] = NULL;
        irq_user_ptrs[i] = NULL;
    }
    return 0;
}

int dmod_deinit(void)
{
    Dmod_Printf("DMUART port module deinitialized (STM32F4)\n");
    return 0;
}

/* ---- Port API implementation ---- */

dmod_dmuart_port_api_declaration(1.0, int, _init, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;

    enable_uart_clock(instance);

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    /* Disable USART for configuration */
    USART->CR1 &= ~STM32F4_USART_CR1_UE;

    /* Default: 8N1, no flow control */
    USART->CR1 &= ~(STM32F4_USART_CR1_M | STM32F4_USART_CR1_PCE | STM32F4_USART_CR1_PS);
    USART->CR2 &= ~STM32F4_USART_CR2_STOP_Msk;
    USART->CR3 &= ~(STM32F4_USART_CR3_RTSE | STM32F4_USART_CR3_CTSE);

    /* Enable USART, transmitter and receiver */
    USART->CR1 |= STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE | STM32F4_USART_CR1_RE;

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _deinit, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~(STM32F4_USART_CR1_UE | STM32F4_USART_CR1_TE | STM32F4_USART_CR1_RE);

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _transmit, ( dmuart_instance_t instance, const uint8_t* data, size_t size ))
{
    if (validate_instance(instance) != 0 || data == NULL) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    for (size_t i = 0; i < size; i++)
    {
        uint32_t timeout = STM32F4_UART_TIMEOUT;
        while (!(USART->SR & STM32F4_USART_SR_TXE))
        {
            if (--timeout == 0) return -1;
        }
        USART->DR = data[i];
    }

    uint32_t timeout = STM32F4_UART_TIMEOUT;
    while (!(USART->SR & STM32F4_USART_SR_TC))
    {
        if (--timeout == 0) return -1;
    }

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _receive, ( dmuart_instance_t instance, uint8_t* data, size_t size, size_t* received ))
{
    if (validate_instance(instance) != 0 || data == NULL || received == NULL) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    *received = 0;

    for (size_t i = 0; i < size; i++)
    {
        uint32_t timeout = STM32F4_UART_TIMEOUT;
        while (!(USART->SR & STM32F4_USART_SR_RXNE))
        {
            if (--timeout == 0) return 0;
        }

        if (USART->SR & (STM32F4_USART_SR_ORE | STM32F4_USART_SR_FE | STM32F4_USART_SR_PE))
        {
            /* Clear errors by reading SR then DR */
            (void)USART->DR;
            return -1;
        }

        data[i] = (uint8_t)(USART->DR & 0xFF);
        (*received)++;
    }

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _flush, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    /* Wait for TX complete */
    uint32_t timeout = STM32F4_UART_TIMEOUT;
    while (!(USART->SR & STM32F4_USART_SR_TC))
    {
        if (--timeout == 0) return -1;
    }

    /* Discard pending RX data */
    while (USART->SR & STM32F4_USART_SR_RXNE)
    {
        (void)USART->DR;
    }

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_baudrate, ( dmuart_instance_t instance, dmuart_baudrate_t baudrate ))
{
    if (validate_instance(instance) != 0 || baudrate == 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~STM32F4_USART_CR1_UE;

    uint32_t pclk = get_uart_clock(instance);
    USART->BRR = (pclk + (baudrate / 2)) / baudrate;

    USART->CR1 |= STM32F4_USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_baudrate_t, _get_baudrate, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return 0;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    uint32_t pclk = get_uart_clock(instance);
    uint32_t brr = USART->BRR;

    if (brr == 0) return 0;
    return (dmuart_baudrate_t)(pclk / brr);
}

dmod_dmuart_port_api_declaration(1.0, int, _set_word_length, ( dmuart_instance_t instance, dmuart_word_length_t word_length ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~STM32F4_USART_CR1_UE;

    /* STM32F4 only supports 8-bit and 9-bit (M bit) */
    USART->CR1 &= ~STM32F4_USART_CR1_M;
    if (word_length == dmuart_word_length_9)
    {
        USART->CR1 |= STM32F4_USART_CR1_M;
    }

    USART->CR1 |= STM32F4_USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_word_length_t, _get_word_length, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_word_length_8;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    if (USART->CR1 & STM32F4_USART_CR1_M) return dmuart_word_length_9;
    return dmuart_word_length_8;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_parity, ( dmuart_instance_t instance, dmuart_parity_t parity ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~STM32F4_USART_CR1_UE;

    USART->CR1 &= ~(STM32F4_USART_CR1_PCE | STM32F4_USART_CR1_PS);
    if (parity == dmuart_parity_even)
    {
        USART->CR1 |= STM32F4_USART_CR1_PCE;
    }
    else if (parity == dmuart_parity_odd)
    {
        USART->CR1 |= STM32F4_USART_CR1_PCE | STM32F4_USART_CR1_PS;
    }

    USART->CR1 |= STM32F4_USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_parity_t, _get_parity, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_parity_none;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    if (!(USART->CR1 & STM32F4_USART_CR1_PCE)) return dmuart_parity_none;
    if (USART->CR1 & STM32F4_USART_CR1_PS) return dmuart_parity_odd;
    return dmuart_parity_even;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_stop_bit, ( dmuart_instance_t instance, dmuart_stop_bit_t stop_bit ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~STM32F4_USART_CR1_UE;

    USART->CR2 &= ~STM32F4_USART_CR2_STOP_Msk;
    if (stop_bit == dmuart_stop_bit_2)
    {
        USART->CR2 |= STM32F4_USART_CR2_STOP_2BIT;
    }

    USART->CR1 |= STM32F4_USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_stop_bit_t, _get_stop_bit, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_stop_bit_1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    if ((USART->CR2 & STM32F4_USART_CR2_STOP_Msk) == STM32F4_USART_CR2_STOP_2BIT)
        return dmuart_stop_bit_2;
    return dmuart_stop_bit_1;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_bit_order, ( dmuart_instance_t instance, dmuart_bit_order_t bit_order ))
{
    if (validate_instance(instance) != 0) return -1;

    /* STM32F4 does not support MSBFIRST - only LSB first is available */
    if (bit_order == dmuart_bit_order_msb_first) return -1;

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_bit_order_t, _get_bit_order, ( dmuart_instance_t instance ))
{
    /* STM32F4 only supports LSB first */
    return dmuart_bit_order_lsb_first;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_invert, ( dmuart_instance_t instance, dmuart_invert_t invert ))
{
    if (validate_instance(instance) != 0) return -1;

    /* STM32F4 does not support signal inversion */
    if (invert != dmuart_invert_none) return -1;

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_invert_t, _get_invert, ( dmuart_instance_t instance ))
{
    return dmuart_invert_none;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_loopback, ( dmuart_instance_t instance, dmuart_loopback_t loopback ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    USART->CR1 &= ~STM32F4_USART_CR1_UE;

    /* STM32F4 does not have a true internal loopback mode.
     * HDSEL (half-duplex) mode is used here as the closest available option;
     * note that it is not a true loopback — it shares the TX pin for both
     * directions, so external wiring is still needed for loopback testing. */
    if (loopback == dmuart_loopback_on)
    {
        USART->CR3 |= (1U << 3); /* HDSEL */
    }
    else
    {
        USART->CR3 &= ~(1U << 3);
    }

    USART->CR1 |= STM32F4_USART_CR1_UE;
    return 0;
}

dmod_dmuart_port_api_declaration(1.0, dmuart_loopback_t, _get_loopback, ( dmuart_instance_t instance ))
{
    if (validate_instance(instance) != 0) return dmuart_loopback_off;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    if (USART->CR3 & (1U << 3)) return dmuart_loopback_on;
    return dmuart_loopback_off;
}

dmod_dmuart_port_api_declaration(1.0, int, _set_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t trigger ))
{
    if (validate_instance(instance) != 0) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);

    /* Clear all interrupt enable bits */
    USART->CR1 &= ~(STM32F4_USART_CR1_RXNEIE | STM32F4_USART_CR1_TXEIE |
                     STM32F4_USART_CR1_TCIE | STM32F4_USART_CR1_IDLEIE);
    USART->CR3 &= ~STM32F4_USART_CR3_EIE;

    if (trigger & dmuart_int_trigger_rx_not_empty)
        USART->CR1 |= STM32F4_USART_CR1_RXNEIE;
    if (trigger & dmuart_int_trigger_tx_empty)
        USART->CR1 |= STM32F4_USART_CR1_TXEIE;
    if (trigger & dmuart_int_trigger_tx_complete)
        USART->CR1 |= STM32F4_USART_CR1_TCIE;
    if (trigger & dmuart_int_trigger_idle)
        USART->CR1 |= STM32F4_USART_CR1_IDLEIE;
    if (trigger & dmuart_int_trigger_error)
        USART->CR3 |= STM32F4_USART_CR3_EIE;

    return 0;
}

dmod_dmuart_port_api_declaration(1.0, int, _read_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t *out_trigger ))
{
    if (validate_instance(instance) != 0 || out_trigger == NULL) return -1;

    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    *out_trigger = dmuart_int_trigger_off;

    if (USART->CR1 & STM32F4_USART_CR1_RXNEIE) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_rx_not_empty);
    if (USART->CR1 & STM32F4_USART_CR1_TXEIE)  *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_tx_empty);
    if (USART->CR1 & STM32F4_USART_CR1_TCIE)   *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_tx_complete);
    if (USART->CR1 & STM32F4_USART_CR1_IDLEIE) *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_idle);
    if (USART->CR3 & STM32F4_USART_CR3_EIE)    *out_trigger = (dmuart_int_trigger_t)(*out_trigger | dmuart_int_trigger_error);

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

/* ---- ISR handlers ---- */

static void stm32f4_uart_irq_handler(dmuart_instance_t instance)
{
    volatile STM32F4_USART_TypeDef *USART = get_usart(instance);
    dmuart_port_interrupt_handler_t handler = irq_handlers[instance - 1];

    if (handler == NULL) return;

    uint8_t data = 0;
    dmuart_int_trigger_t trigger = dmuart_int_trigger_off;

    if (USART->SR & STM32F4_USART_SR_RXNE)
    {
        data = (uint8_t)(USART->DR & 0xFF);
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_rx_not_empty);
    }
    if (USART->SR & STM32F4_USART_SR_TXE)
    {
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_tx_empty);
    }
    if (USART->SR & STM32F4_USART_SR_TC)
    {
        USART->SR &= ~STM32F4_USART_SR_TC;
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_tx_complete);
    }
    if (USART->SR & (STM32F4_USART_SR_ORE | STM32F4_USART_SR_FE | STM32F4_USART_SR_PE))
    {
        /* Clear errors by reading SR then DR */
        (void)USART->DR;
        trigger = (dmuart_int_trigger_t)(trigger | dmuart_int_trigger_error);
    }

    if (trigger != dmuart_int_trigger_off)
    {
        handler(irq_user_ptrs[instance - 1], instance, trigger, data);
    }
}

/* NVIC IRQ numbers for STM32F4 UART instances */
DMOD_IRQ_HANDLER(37) { stm32f4_uart_irq_handler(1); }  /* USART1 */
DMOD_IRQ_HANDLER(38) { stm32f4_uart_irq_handler(2); }  /* USART2 */
DMOD_IRQ_HANDLER(39) { stm32f4_uart_irq_handler(3); }  /* USART3 */
DMOD_IRQ_HANDLER(52) { stm32f4_uart_irq_handler(4); }  /* UART4 */
DMOD_IRQ_HANDLER(53) { stm32f4_uart_irq_handler(5); }  /* UART5 */
DMOD_IRQ_HANDLER(71) { stm32f4_uart_irq_handler(6); }  /* USART6 */
