#ifndef DMUART_H
#define DMUART_H

#include <stdint.h>
#include "dmod.h"
#include "dmuart_defs.h"

/**
 * @brief UART parity configuration
 */
typedef enum 
{
    dmuart_parity_none = 0,         /**< No parity */
    dmuart_parity_even,             /**< Even parity */
    dmuart_parity_odd,              /**< Odd parity */
} dmuart_parity_t;

/**
 * @brief UART stop bits configuration
 */
typedef enum 
{
    dmuart_stopbits_1 = 0,          /**< 1 stop bit */
    dmuart_stopbits_2,              /**< 2 stop bits */
} dmuart_stopbits_t;

/**
 * @brief UART flow control configuration
 */
typedef enum 
{
    dmuart_flowcontrol_none = 0,    /**< No flow control */
    dmuart_flowcontrol_rts_cts,     /**< RTS/CTS hardware flow control */
} dmuart_flowcontrol_t;

/**
 * @brief IOCTL commands for DMUART device
 */
typedef enum 
{
    dmuart_ioctl_cmd_get_baudrate = 1,      /**< Get current baud rate */
    dmuart_ioctl_cmd_set_baudrate,          /**< Set baud rate */
    dmuart_ioctl_cmd_get_parity,            /**< Get parity setting */
    dmuart_ioctl_cmd_set_parity,            /**< Set parity setting */
    dmuart_ioctl_cmd_get_stopbits,          /**< Get stop bits setting */
    dmuart_ioctl_cmd_set_stopbits,          /**< Set stop bits setting */
    dmuart_ioctl_cmd_get_flowcontrol,       /**< Get flow control setting */
    dmuart_ioctl_cmd_set_flowcontrol,       /**< Set flow control setting */
    dmuart_ioctl_cmd_get_databits,          /**< Get data bits setting */
    dmuart_ioctl_cmd_set_databits,          /**< Set data bits setting */
    dmuart_ioctl_cmd_reconfigure,           /**< Reconfigure UART with current settings */

    dmuart_ioctl_cmd_max

} dmuart_ioctl_cmd_t;

#endif // DMUART_H
