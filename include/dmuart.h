#ifndef DMUART_H
#define DMUART_H

#include <stdint.h>
#include "dmod.h"
#include "dmuart_defs.h"

/**
 * @brief Source of the clock signal
 */
typedef enum 
{
    dmuart_source_unkown = 0,        /**< Unknown clock source */
    dmuart_source_internal,          /**< Internal RC oscillator */
    dmuart_source_external,          /**< External crystal or oscillator */
    dmuart_source_hibernation,       /**< Low-power hibernation clock source */
} dmuart_source_t;

/**
 * @brief IOCTL commands for DMUART device
 */
typedef enum 
{
    dmuart_ioctl_cmd_get_frequency = 1,   /**< Get current clock frequency */
    dmuart_ioctl_cmd_set_source,          /**< Set clock source */
    dmuart_ioctl_cmd_get_source,          /**< Get clock source */
    dmuart_ioctl_cmd_set_tolerance,       /**< Set frequency tolerance */
    dmuart_ioctl_cmd_get_tolerance,       /**< Get frequency tolerance */
    dmuart_ioctl_cmd_set_oscillator_frequency, /**< Set oscillator frequency */
    dmuart_ioctl_cmd_get_oscillator_frequency, /**< Get oscillator frequency */
    dmuart_ioctl_cmd_set_target_frequency,    /**< Set target frequency */
    dmuart_ioctl_cmd_get_target_frequency,    /**< Get target frequency */
    dmuart_ioctl_cmd_reconfigure,             /**< Reconfigure clock with current settings */

    dmuart_ioctl_cmd_max

} dmuart_ioctl_cmd_t;

#endif // DMUART_H