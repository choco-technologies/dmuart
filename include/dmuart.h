#ifndef DMUART_H
#define DMUART_H

#include "dmuart_defs.h"
#include "dmuart_types.h"

/**
 * @brief UART driver configuration structure
 */
typedef struct
{
    dmuart_instance_t       instance;           /**< UART instance number (1-based) */
    dmuart_baudrate_t       baudrate;           /**< Baud rate */
    dmuart_word_length_t    word_length;        /**< Word length (data bits) */
    dmuart_parity_t         parity;             /**< Parity setting */
    dmuart_stop_bit_t       stop_bit;           /**< Stop bit setting */
    dmuart_flow_control_t   flow_control;       /**< Flow control setting */
    dmuart_bit_order_t      bit_order;          /**< Bit order (LSB/MSB first) */
    dmuart_invert_t         invert;             /**< Signal inversion */
    dmuart_loopback_t       loopback;           /**< Loopback mode */
    dmuart_int_trigger_t    interrupt_trigger;  /**< Interrupt trigger source */
    dmuart_interrupt_handler_t interrupt_handler; /**< Interrupt handler (NULL = not used) */
} dmuart_config_t;

#endif // DMUART_H
