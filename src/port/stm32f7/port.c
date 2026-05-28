#define DMOD_ENABLE_REGISTRATION    ON
#include "dmuart_port.h"
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

/**
 * @brief Get the peripheral clock frequency for a UART instance
 * 
 * @param instance UART instance (1-8)
 * 
 * @return uint32_t Peripheral clock frequency in Hz
 */
static uint32_t get_uart_clock(uint32_t instance)
{
    /* USART1 and USART6 are on APB2, others are on APB1 */
    if (instance == 1 || instance == 6)
    {
        return STM32F7_APB2_FREQ;
    }
    return STM32F7_APB1_FREQ;
}

/**
 * @brief Enable the UART peripheral clock
 * 
 * @param instance UART instance (1-8)
 */
static void enable_uart_clock(uint32_t instance)
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

/**
 * @brief Initialize the DMUART port module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    Dmod_Printf("DMUART port module initialized (STM32F7)\n");
    return 0;
}

/**
 * @brief Deinitialize the DMUART port module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    Dmod_Printf("DMUART port module deinitialized (STM32F7)\n");
    return 0;
}

/**
 * @brief Initialize a UART instance
 * 
 * @param instance UART instance number (1-8)
 * @param baudrate Desired baud rate
 * @param databits Number of data bits (7, 8, or 9)
 * @param parity Parity setting (0=none, 1=even, 2=odd)
 * @param stopbits Stop bits (0=1 bit, 1=2 bits)
 * @param flowcontrol Flow control (0=none, 1=RTS/CTS)
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmuart_port_api_declaration(1.0, int, _init, ( uint32_t instance, dmuart_baudrate_t baudrate, dmuart_databits_t databits, uint8_t parity, uint8_t stopbits, uint8_t flowcontrol) )
{
    if (instance < 1 || instance > STM32F7_UART_MAX_INSTANCES)
    {
        return -1;
    }

    /* Enable peripheral clock */
    enable_uart_clock(instance);

    volatile USART_TypeDef *USART = (USART_TypeDef *)uart_base_addresses[instance - 1];

    /* Disable USART for configuration */
    USART->CR1 &= ~USART_CR1_UE;

    /* Configure word length */
    USART->CR1 &= ~(USART_CR1_M0 | USART_CR1_M1);
    if (databits == 9)
    {
        USART->CR1 |= USART_CR1_M0;
    }
    else if (databits == 7)
    {
        USART->CR1 |= USART_CR1_M1;
    }
    /* 8 bits: M0=0, M1=0 (default) */

    /* Configure parity */
    USART->CR1 &= ~(USART_CR1_PCE | USART_CR1_PS);
    if (parity == 1) /* even */
    {
        USART->CR1 |= USART_CR1_PCE;
    }
    else if (parity == 2) /* odd */
    {
        USART->CR1 |= USART_CR1_PCE | USART_CR1_PS;
    }

    /* Configure stop bits */
    USART->CR2 &= ~USART_CR2_STOP_Msk;
    if (stopbits == 1) /* 2 stop bits */
    {
        USART->CR2 |= USART_CR2_STOP_2BIT;
    }

    /* Configure flow control */
    USART->CR3 &= ~(USART_CR3_RTSE | USART_CR3_CTSE);
    if (flowcontrol == 1) /* RTS/CTS */
    {
        USART->CR3 |= USART_CR3_RTSE | USART_CR3_CTSE;
    }

    /* Configure baud rate */
    uint32_t pclk = get_uart_clock(instance);
    USART->BRR = (pclk + (baudrate / 2)) / baudrate;

    /* Enable USART, transmitter and receiver */
    USART->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    return 0;
}

/**
 * @brief Deinitialize a UART instance
 * 
 * @param instance UART instance number (1-8)
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmuart_port_api_declaration(1.0, int, _deinit, ( uint32_t instance ) )
{
    if (instance < 1 || instance > STM32F7_UART_MAX_INSTANCES)
    {
        return -1;
    }

    volatile USART_TypeDef *USART = (USART_TypeDef *)uart_base_addresses[instance - 1];

    /* Disable USART */
    USART->CR1 &= ~(USART_CR1_UE | USART_CR1_TE | USART_CR1_RE);

    return 0;
}

/**
 * @brief Transmit data via UART
 * 
 * @param instance UART instance number (1-8)
 * @param data Pointer to data buffer
 * @param size Number of bytes to transmit
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmuart_port_api_declaration(1.0, int, _transmit, ( uint32_t instance, const uint8_t* data, size_t size ) )
{
    if (instance < 1 || instance > STM32F7_UART_MAX_INSTANCES || data == NULL)
    {
        return -1;
    }

    volatile USART_TypeDef *USART = (USART_TypeDef *)uart_base_addresses[instance - 1];

    for (size_t i = 0; i < size; i++)
    {
        uint32_t timeout = UART_TIMEOUT_VALUE;
        while (!(USART->ISR & USART_ISR_TXE))
        {
            if (--timeout == 0)
            {
                return -1;
            }
        }
        USART->TDR = data[i];
    }

    /* Wait for transmission complete */
    uint32_t timeout = UART_TIMEOUT_VALUE;
    while (!(USART->ISR & USART_ISR_TC))
    {
        if (--timeout == 0)
        {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Receive data via UART
 * 
 * @param instance UART instance number (1-8)
 * @param data Pointer to receive buffer
 * @param size Maximum number of bytes to receive
 * @param received Pointer to store actual number of bytes received
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmuart_port_api_declaration(1.0, int, _receive, ( uint32_t instance, uint8_t* data, size_t size, size_t* received ) )
{
    if (instance < 1 || instance > STM32F7_UART_MAX_INSTANCES || data == NULL || received == NULL)
    {
        return -1;
    }

    volatile USART_TypeDef *USART = (USART_TypeDef *)uart_base_addresses[instance - 1];
    *received = 0;

    for (size_t i = 0; i < size; i++)
    {
        uint32_t timeout = UART_TIMEOUT_VALUE;
        while (!(USART->ISR & USART_ISR_RXNE))
        {
            if (--timeout == 0)
            {
                /* Timeout - return what we have so far */
                return 0;
            }
        }

        /* Check for errors */
        if (USART->ISR & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_PE))
        {
            /* Clear error flags */
            USART->ICR |= USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_PECF;
            return -1;
        }

        data[i] = (uint8_t)(USART->RDR & 0xFF);
        (*received)++;
    }

    return 0;
}

/**
 * @brief Set the baud rate for a UART instance
 * 
 * @param instance UART instance number (1-8)
 * @param baudrate Desired baud rate
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmuart_port_api_declaration(1.0, int, _set_baudrate, ( uint32_t instance, dmuart_baudrate_t baudrate ) )
{
    if (instance < 1 || instance > STM32F7_UART_MAX_INSTANCES || baudrate == 0)
    {
        return -1;
    }

    volatile USART_TypeDef *USART = (USART_TypeDef *)uart_base_addresses[instance - 1];

    /* Disable USART for reconfiguration */
    USART->CR1 &= ~USART_CR1_UE;

    /* Recalculate BRR */
    uint32_t pclk = get_uart_clock(instance);
    USART->BRR = (pclk + (baudrate / 2)) / baudrate;

    /* Re-enable USART */
    USART->CR1 |= USART_CR1_UE;

    return 0;
}

/**
 * @brief Get the current baud rate for a UART instance
 * 
 * @param instance UART instance number (1-8)
 * 
 * @return dmuart_baudrate_t Current baud rate in bps
 */
dmod_dmuart_port_api_declaration(1.0, dmuart_baudrate_t, _get_baudrate, ( uint32_t instance ) )
{
    if (instance < 1 || instance > STM32F7_UART_MAX_INSTANCES)
    {
        return 0;
    }

    volatile USART_TypeDef *USART = (USART_TypeDef *)uart_base_addresses[instance - 1];

    uint32_t pclk = get_uart_clock(instance);
    uint32_t brr = USART->BRR;

    if (brr == 0)
    {
        return 0;
    }

    return (dmuart_baudrate_t)(pclk / brr);
}
