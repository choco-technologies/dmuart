#ifndef DMUART_PORT_H
#define DMUART_PORT_H

#include "dmod.h"
#include "dmuart_port_defs.h"

/**
 * @brief UART baud rate type
 */
typedef uint32_t dmuart_baudrate_t;

/**
 * @brief UART data bits type
 */
typedef uint8_t dmuart_databits_t;

dmod_dmuart_port_api(1.0, int, _init, ( uint32_t instance, dmuart_baudrate_t baudrate, dmuart_databits_t databits, uint8_t parity, uint8_t stopbits, uint8_t flowcontrol) );
dmod_dmuart_port_api(1.0, int, _deinit, ( uint32_t instance ) );
dmod_dmuart_port_api(1.0, int, _transmit, ( uint32_t instance, const uint8_t* data, size_t size ) );
dmod_dmuart_port_api(1.0, int, _receive, ( uint32_t instance, uint8_t* data, size_t size, size_t* received ) );
dmod_dmuart_port_api(1.0, int, _set_baudrate, ( uint32_t instance, dmuart_baudrate_t baudrate ) );
dmod_dmuart_port_api(1.0, dmuart_baudrate_t, _get_baudrate, ( uint32_t instance ) );

#endif // DMUART_PORT_H
