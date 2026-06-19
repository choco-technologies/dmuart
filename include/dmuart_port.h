#ifndef DMUART_PORT_H
#define DMUART_PORT_H

#include "dmod_types.h"
#include "dmuart_port_defs.h"
#include "dmuart_types.h"
#include "dm_sw_ring.h"

/* --- Lifecycle --- */

dmod_dmuart_port_api(1.0, int, _init,   ( dmuart_instance_t instance ) );
dmod_dmuart_port_api(1.0, int, _deinit, ( dmuart_instance_t instance ) );

/* --- Data transfer --- */

dmod_dmuart_port_api(1.0, int, _transmit, ( dmuart_instance_t instance, const uint8_t* data, size_t size ) );
dmod_dmuart_port_api(1.0, int, _receive,  ( dmuart_instance_t instance, uint8_t* data, size_t size, size_t* received ) );
dmod_dmuart_port_api(1.0, int, _flush,    ( dmuart_instance_t instance ) );

/* --- Baud rate --- */

dmod_dmuart_port_api(1.0, int,              _set_baudrate, ( dmuart_instance_t instance, dmuart_baudrate_t baudrate ) );
dmod_dmuart_port_api(1.0, dmuart_baudrate_t,_get_baudrate, ( dmuart_instance_t instance ) );

/* --- Word length --- */

dmod_dmuart_port_api(1.0, int,                _set_word_length, ( dmuart_instance_t instance, dmuart_word_length_t word_length ) );
dmod_dmuart_port_api(1.0, dmuart_word_length_t, _get_word_length, ( dmuart_instance_t instance ) );

/* --- Parity --- */

dmod_dmuart_port_api(1.0, int,            _set_parity, ( dmuart_instance_t instance, dmuart_parity_t parity ) );
dmod_dmuart_port_api(1.0, dmuart_parity_t, _get_parity, ( dmuart_instance_t instance ) );

/* --- Stop bit --- */

dmod_dmuart_port_api(1.0, int,              _set_stop_bit, ( dmuart_instance_t instance, dmuart_stop_bit_t stop_bit ) );
dmod_dmuart_port_api(1.0, dmuart_stop_bit_t, _get_stop_bit, ( dmuart_instance_t instance ) );

/* --- Bit order --- */

dmod_dmuart_port_api(1.0, int,               _set_bit_order, ( dmuart_instance_t instance, dmuart_bit_order_t bit_order ) );
dmod_dmuart_port_api(1.0, dmuart_bit_order_t, _get_bit_order, ( dmuart_instance_t instance ) );

/* --- Signal inversion --- */

dmod_dmuart_port_api(1.0, int,            _set_invert, ( dmuart_instance_t instance, dmuart_invert_t invert ) );
dmod_dmuart_port_api(1.0, dmuart_invert_t, _get_invert, ( dmuart_instance_t instance ) );

/* --- Loopback --- */

dmod_dmuart_port_api(1.0, int,              _set_loopback, ( dmuart_instance_t instance, dmuart_loopback_t loopback ) );
dmod_dmuart_port_api(1.0, dmuart_loopback_t, _get_loopback, ( dmuart_instance_t instance ) );

/* --- Interrupt trigger --- */

dmod_dmuart_port_api(1.0, int, _set_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t trigger ) );
dmod_dmuart_port_api(1.0, int, _read_interrupt_trigger, ( dmuart_instance_t instance, dmuart_int_trigger_t *out_trigger ) );

/* --- Interrupt handler registration --- */

dmod_dmuart_port_api(1.0, int, _add_interrupt_handler,
    ( dmuart_instance_t instance, dmuart_port_interrupt_handler_t handler, void *user_ptr ) );
dmod_dmuart_port_api(1.0, int, _remove_interrupt_handler,
    ( dmuart_instance_t instance, void *user_ptr ) );

/* --- SW ring buffer --- */

dmod_dmuart_port_api(1.0, int, _set_rx_ring,
    ( dmuart_instance_t instance, dm_sw_ring_t ring ) );

#endif // DMUART_PORT_H
