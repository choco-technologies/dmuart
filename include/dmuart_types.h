#ifndef DMUART_TYPES_H
#define DMUART_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief UART instance type
 */
typedef uint8_t dmuart_instance_t;

/**
 * @brief UART baud rate type
 */
typedef uint32_t dmuart_baudrate_t;

/**
 * @brief UART data bits (word length) configuration
 */
typedef enum
{
    dmuart_word_length_7 = 7,           /**< 7 data bits */
    dmuart_word_length_8 = 8,           /**< 8 data bits */
    dmuart_word_length_9 = 9,           /**< 9 data bits */
} dmuart_word_length_t;

/**
 * @brief UART parity configuration
 */
typedef enum
{
    dmuart_parity_none = 0,             /**< No parity */
    dmuart_parity_even,                 /**< Even parity */
    dmuart_parity_odd,                  /**< Odd parity */
} dmuart_parity_t;

/**
 * @brief UART stop bits configuration
 */
typedef enum
{
    dmuart_stop_bit_1 = 0,              /**< 1 stop bit */
    dmuart_stop_bit_2,                  /**< 2 stop bits */
} dmuart_stop_bit_t;

/**
 * @brief UART flow control configuration
 */
typedef enum
{
    dmuart_flow_control_none = 0,       /**< No flow control */
    dmuart_flow_control_rts_cts,        /**< RTS/CTS hardware flow control */
} dmuart_flow_control_t;

/**
 * @brief UART bit order configuration
 */
typedef enum
{
    dmuart_bit_order_lsb_first = 0,     /**< LSB first (standard) */
    dmuart_bit_order_msb_first,         /**< MSB first */
} dmuart_bit_order_t;

/**
 * @brief UART signal inversion
 */
typedef enum
{
    dmuart_invert_none = 0,             /**< No inversion */
    dmuart_invert_tx   = (1 << 0),      /**< Invert TX line */
    dmuart_invert_rx   = (1 << 1),      /**< Invert RX line */
    dmuart_invert_both = (1 << 0) | (1 << 1),  /**< Invert both TX and RX */
} dmuart_invert_t;

/**
 * @brief UART loopback mode
 */
typedef enum
{
    dmuart_loopback_off = 0,            /**< Loopback disabled */
    dmuart_loopback_on,                 /**< Loopback enabled (TX connected to RX internally) */
} dmuart_loopback_t;

/**
 * @brief UART interrupt trigger source
 */
typedef enum
{
    dmuart_int_trigger_off          = 0,        /**< Interrupts disabled */
    dmuart_int_trigger_rx_not_empty = (1 << 0), /**< RX data register not empty */
    dmuart_int_trigger_tx_empty     = (1 << 1), /**< TX data register empty */
    dmuart_int_trigger_tx_complete  = (1 << 2), /**< Transmission complete */
    dmuart_int_trigger_idle         = (1 << 3), /**< Idle line detected */
    dmuart_int_trigger_error        = (1 << 4), /**< Error (overrun, framing, parity) */
} dmuart_int_trigger_t;

/**
 * @brief IOCTL commands for DMUART device
 */
typedef enum
{
    dmuart_ioctl_cmd_get_baudrate = 1,          /**< Get current baud rate */
    dmuart_ioctl_cmd_set_baudrate,              /**< Set baud rate */
    dmuart_ioctl_cmd_get_word_length,           /**< Get word length */
    dmuart_ioctl_cmd_set_word_length,           /**< Set word length */
    dmuart_ioctl_cmd_get_parity,                /**< Get parity setting */
    dmuart_ioctl_cmd_set_parity,                /**< Set parity setting */
    dmuart_ioctl_cmd_get_stop_bit,              /**< Get stop bit setting */
    dmuart_ioctl_cmd_set_stop_bit,              /**< Set stop bit setting */
    dmuart_ioctl_cmd_get_flow_control,          /**< Get flow control setting */
    dmuart_ioctl_cmd_set_flow_control,          /**< Set flow control setting */
    dmuart_ioctl_cmd_get_bit_order,             /**< Get bit order setting */
    dmuart_ioctl_cmd_set_bit_order,             /**< Set bit order setting */
    dmuart_ioctl_cmd_get_invert,                /**< Get signal inversion setting */
    dmuart_ioctl_cmd_set_invert,                /**< Set signal inversion setting */
    dmuart_ioctl_cmd_get_loopback,              /**< Get loopback mode */
    dmuart_ioctl_cmd_set_loopback,              /**< Set loopback mode */
    dmuart_ioctl_cmd_set_interrupt_handler,     /**< Set interrupt handler; arg = dmuart_interrupt_handler_t* */
    dmuart_ioctl_cmd_reconfigure,               /**< Reconfigure UART with current settings */

    dmuart_ioctl_cmd_max
} dmuart_ioctl_cmd_t;

/**
 * @brief Opaque driver context type (forward declaration)
 */
struct dmdrvi_context;
typedef struct dmdrvi_context *dmdrvi_context_t;

/**
 * @brief Parameters passed to a dmhaman-registered interrupt handler.
 */
typedef struct
{
    dmuart_instance_t   instance;       /**< UART instance that generated the interrupt */
    dmuart_int_trigger_t trigger;       /**< Which trigger fired */
    uint8_t             data;           /**< Received data byte (valid for rx_not_empty trigger) */
} dmuart_interrupt_params_t;

/**
 * @brief UART interrupt handler function type
 *
 * @param context   Context of the driver instance
 * @param instance  UART instance that generated the interrupt
 * @param trigger   Which interrupt trigger fired
 * @param data      Received data byte (valid when trigger includes rx_not_empty)
 */
typedef void (*dmuart_interrupt_handler_t)(dmdrvi_context_t context, dmuart_instance_t instance, dmuart_int_trigger_t trigger, uint8_t data);

/**
 * @brief UART port interrupt handler function type
 *
 * Called by the port layer when an interrupt occurs on a UART instance.
 *
 * @param user_ptr  User pointer supplied at registration time (e.g. driver context)
 * @param instance  UART instance that generated the interrupt
 * @param trigger   Which interrupt trigger fired
 * @param data      Received data byte (valid when trigger includes rx_not_empty)
 */
typedef void (*dmuart_port_interrupt_handler_t)(void *user_ptr, dmuart_instance_t instance, dmuart_int_trigger_t trigger, uint8_t data);

#endif /* DMUART_TYPES_H */
