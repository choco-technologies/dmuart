#define DMOD_ENABLE_REGISTRATION    ON
#include "dmuart.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmuart_port.h"
#include <errno.h>
#include <string.h>

// Magic set to UART
#define DMUART_CONTEXT_MAGIC    0x44555254

/**
 * @brief Configuration structure
 */
struct config
{
    dmuart_baudrate_t baudrate;
    dmuart_databits_t databits;
    dmuart_parity_t parity;
    dmuart_stopbits_t stopbits;
    dmuart_flowcontrol_t flowcontrol;
    uint32_t instance;
};

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t magic;                    /**< Magic number for validation */
    struct config config;              /**< Configuration parameters */
    dmuart_baudrate_t current_baudrate;  /**< Current baud rate */
};

/**
 * @brief Validate DMDRVI context
 * 
 * @param context DMDRVI context to validate
 * 
 * @return int 1 if valid, 0 otherwise
 */
static int is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMUART_CONTEXT_MAGIC);
}

/**
 * @brief Convert parity enum to string
 * 
 * @param parity Parity setting
 * 
 * @return const char* String representation of parity
 */
static const char* parity_to_string(dmuart_parity_t parity)
{
    switch (parity)
    {
        case dmuart_parity_none:
            return "none";
        case dmuart_parity_even:
            return "even";
        case dmuart_parity_odd:
            return "odd";
        default:
            return "unknown";
    }
}

/**
 * @brief Convert string to parity enum
 * 
 * @param parity_str String representation of parity
 * 
 * @return dmuart_parity_t Parity enum
 */
static dmuart_parity_t string_to_parity(const char* parity_str)
{
    if (parity_str != NULL)
    {
        if (strcmp(parity_str, "none") == 0)
        {
            return dmuart_parity_none;
        }
        else if (strcmp(parity_str, "even") == 0)
        {
            return dmuart_parity_even;
        }
        else if (strcmp(parity_str, "odd") == 0)
        {
            return dmuart_parity_odd;
        }
    }
    return dmuart_parity_none;
}

/**
 * @brief Convert string to stopbits enum
 * 
 * @param stopbits_str String representation of stop bits
 * 
 * @return dmuart_stopbits_t Stop bits enum
 */
static dmuart_stopbits_t string_to_stopbits(const char* stopbits_str)
{
    if (stopbits_str != NULL)
    {
        if (strcmp(stopbits_str, "2") == 0)
        {
            return dmuart_stopbits_2;
        }
    }
    return dmuart_stopbits_1;
}

/**
 * @brief Convert string to flow control enum
 * 
 * @param fc_str String representation of flow control
 * 
 * @return dmuart_flowcontrol_t Flow control enum
 */
static dmuart_flowcontrol_t string_to_flowcontrol(const char* fc_str)
{
    if (fc_str != NULL)
    {
        if (strcmp(fc_str, "rts_cts") == 0)
        {
            return dmuart_flowcontrol_rts_cts;
        }
    }
    return dmuart_flowcontrol_none;
}

/**
 * @brief Check configuration parameters
 * 
 * @param cfg Configuration structure
 * 
 * @return int 0 if valid, non-zero otherwise
 */
static int check_config_parameters(struct config* cfg)
{
    if (cfg->baudrate == 0)
    {
        DMOD_LOG_ERROR("Baud rate not set in configuration\n");
        return -EINVAL;
    }
    else if (cfg->databits < 7 || cfg->databits > 9)
    {
        DMOD_LOG_ERROR("Invalid data bits in configuration (must be 7, 8, or 9)\n");
        return -EINVAL;
    }
    return 0;
}

/**
 * @brief Read configuration parameters from Dmini context
 * 
 * @param context DMDRVI context
 * @param config Dmini context with configuration data
 * 
 * @return int 0 on success, non-zero on failure
 */
static int read_config_parameters(dmdrvi_context_t context, dmini_context_t config)
{   
    context->config.baudrate = (dmuart_baudrate_t)dmini_get_int(config, "dmuart", "baudrate", 0);
    context->config.databits = (dmuart_databits_t)dmini_get_int(config, "dmuart", "databits", 8);
    context->config.parity = string_to_parity(dmini_get_string(config, "dmuart", "parity", "none"));
    context->config.stopbits = string_to_stopbits(dmini_get_string(config, "dmuart", "stopbits", "1"));
    context->config.flowcontrol = string_to_flowcontrol(dmini_get_string(config, "dmuart", "flowcontrol", "none"));
    context->config.instance = (uint32_t)dmini_get_int(config, "dmuart", "instance", 1);
    
    return check_config_parameters(&context->config);
}

/**
 * @brief Configure the UART based on context parameters
 * 
 * @param context DMDRVI context
 * 
 * @return int 0 on success, non-zero on failure
 */
static int configure(dmdrvi_context_t context)
{
    int ret = dmuart_port_init(
        context->config.instance,
        context->config.baudrate,
        context->config.databits,
        (uint8_t)context->config.parity,
        (uint8_t)context->config.stopbits,
        (uint8_t)context->config.flowcontrol
    );

    if (ret == 0)
    {
        DMOD_LOG_INFO("UART%u configured: %u baud, %u%c%s\n",
            context->config.instance,
            context->config.baudrate,
            context->config.databits,
            (context->config.parity == dmuart_parity_none) ? 'N' :
            (context->config.parity == dmuart_parity_even) ? 'E' : 'O',
            (context->config.stopbits == dmuart_stopbits_1) ? "1" : "2");
        context->current_baudrate = dmuart_port_get_baudrate(context->config.instance);
    }
    else 
    {
        DMOD_LOG_ERROR("Failed to configure UART%u\n", context->config.instance);
    }
    return ret;
}

/**
 * @brief Update configuration parameters in context
 *
 * @param cfg Configuration structure to update
 * @param command IOCTL command
 * @param arg Argument for the command
 * 
 * @return int 0 on success, non-zero on failure
 */
static int update_configuration(struct config* cfg, int command, void* arg)
{
    int ret = 0;
    switch (command)
    {
        case dmuart_ioctl_cmd_set_baudrate:
            cfg->baudrate = *(dmuart_baudrate_t*)arg;
            break;
        case dmuart_ioctl_cmd_set_parity:
            cfg->parity = *(dmuart_parity_t*)arg;
            break;
        case dmuart_ioctl_cmd_set_stopbits:
            cfg->stopbits = *(dmuart_stopbits_t*)arg;
            break;
        case dmuart_ioctl_cmd_set_flowcontrol:
            cfg->flowcontrol = *(dmuart_flowcontrol_t*)arg;
            break;
        case dmuart_ioctl_cmd_set_databits:
            cfg->databits = *(dmuart_databits_t*)arg;
            break;
        default:
            DMOD_LOG_ERROR("Invalid configuration command %d in update_configuration\n", command);
            ret = -EINVAL;
            break;
    }
    if (ret == 0)
    {
        ret = check_config_parameters(cfg);
    }
    return ret;
}

/**
 * @brief Read configuration parameters from context
 * 
 * @param context DMDRVI context
 * @param command IOCTL command
 * @param arg Argument for the command
 * 
 * @return int 0 on success, non-zero on failure
 */
static int read_configuration(dmdrvi_context_t context, int command, void* arg)
{
    int ret = 0;
    switch (command)
    {
        case dmuart_ioctl_cmd_get_baudrate:
            *(dmuart_baudrate_t*)arg = context->config.baudrate;
            break;
        case dmuart_ioctl_cmd_get_parity:
            *(dmuart_parity_t*)arg = context->config.parity;
            break;
        case dmuart_ioctl_cmd_get_stopbits:
            *(dmuart_stopbits_t*)arg = context->config.stopbits;
            break;
        case dmuart_ioctl_cmd_get_flowcontrol:
            *(dmuart_flowcontrol_t*)arg = context->config.flowcontrol;
            break;
        case dmuart_ioctl_cmd_get_databits:
            *(dmuart_databits_t*)arg = context->config.databits;
            break;
        default:
            DMOD_LOG_ERROR("Invalid configuration command %d in read_configuration\n", command);
            ret = -EINVAL;
            break;
    }
    return ret;
}

/**
 * @brief Initialize the DMDRVI module
 * 
 * @param Config Pointer to Dmod_Config_t structure with configuration parameters
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_init(const Dmod_Config_t *Config)
{
    DMOD_LOG_INFO("DMUART interface module initialized\n");
    return 0;
}

/**
 * @brief Deinitialize the DMDRVI module
 * 
 * @return int 0 on success, non-zero on failure
 */
int dmod_deinit(void)
{
    DMOD_LOG_INFO("DMUART interface module deinitialized\n");   
    return 0;
}

/**
 * @brief Create a new DMDRVI context
 * 
 * @param config Pointer to configuration data
 * @param dev_num Output pointer to device number structure - driver fills in major, minor, and flags
 * 
 * @return dmdrvi_context_t New DMDRVI context
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if(config == NULL || dev_num == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters to dmuart_dmdrvi_create\n");
        return NULL;
    }

    // Set device numbering - dmuart uses minor numbering for instance (device path: /dev/dmuart0, /dev/dmuart1, etc.)
    dev_num->major = 0;
    dev_num->minor = 0;
    dev_num->flags = DMDRVI_NUM_MINOR;

    dmdrvi_context_t context = Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context != NULL)
    {
        memset(context, 0, sizeof(*context));
        context->magic = DMUART_CONTEXT_MAGIC;
        if (read_config_parameters(context, config) != 0
         || configure(context) != 0)
        {
            DMOD_LOG_ERROR("Failed to create DMDRVI context with provided configuration\n");
            Dmod_Free(context);
            return NULL;
        }
        else 
        {
            DMOD_LOG_INFO("UART configured at %u baud\n", context->current_baudrate);
        }
    }
    return context;
}

/**
 * @brief Free the DMDRVI context
 * 
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
    {
        dmuart_port_deinit(context->config.instance);
        context->magic = 0; // Invalidate context
        Dmod_Free(context);
    }
}

/**
 * @brief Open a device handle
 * 
 * @param context DMDRVI context
 * @param flags Open flags
 * 
 * @return void* Device handle
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, void*, _open, ( dmdrvi_context_t context, int flags ))
{
    if(!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmuart_dmdrvi_open\n");
        return NULL;
    }
    return context;
}

/**
 * @brief Close a device handle
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @return void
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    // No specific action needed to close the UART device handle
}

/**
 * @brief Read from the device
 * 
 * Reads data from the UART receive buffer.
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer to read data into
 * @param size Size of the buffer
 * @param offset Not used for UART (stream device)
 * 
 * @return size_t Number of bytes read
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, size_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, uint32_t offset ))
{
    if (!is_valid_context(context) || buffer == NULL || size == 0)
    {
        return 0;
    }

    size_t received = 0;
    int ret = dmuart_port_receive(context->config.instance, (uint8_t*)buffer, size, &received);
    if (ret != 0)
    {
        return 0;
    }
    return received;
}

/**
 * @brief Write to the device
 * 
 * Transmits data via UART.
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param buffer Buffer with data to write
 * @param size Number of bytes to write
 * @param offset Not used for UART (stream device)
 * 
 * @return size_t Number of bytes written
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, size_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, uint32_t offset ))
{
    if (!is_valid_context(context) || buffer == NULL || size == 0)
    {
        return 0;
    }

    int ret = dmuart_port_transmit(context->config.instance, (const uint8_t*)buffer, size);
    if (ret != 0)
    {
        return 0;
    }
    return size;
}

/**
 * @brief Ioctl operation on the device
 * 
 * List of supported commands can be found in #dmuart_ioctl_cmd_t.
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * @param command Ioctl command
 * @param arg Argument for the command
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    int ret = 0;
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmuart_dmdrvi_ioctl\n");
        return -EINVAL;
    }

    if(command >= dmuart_ioctl_cmd_max)
    {
        DMOD_LOG_ERROR("Invalid ioctl command %d in dmuart_dmdrvi_ioctl\n", command);
        return -EINVAL;
    }
    else if(command == dmuart_ioctl_cmd_reconfigure)
    {
        dmuart_port_deinit(context->config.instance);
        ret = configure(context);
        if (ret == 0)
        {
            DMOD_LOG_INFO("UART reconfigured at %u baud\n", context->current_baudrate);
        }
    }
    else if(arg == NULL)  
    {
        DMOD_LOG_ERROR("Null argument for ioctl command %d in dmuart_dmdrvi_ioctl\n", command);
        return -EINVAL;
    }
    else 
    {
        struct config new_config = {0};
        memcpy(&new_config, &context->config, sizeof(struct config));

        ret = read_configuration(context, command, arg); 
        if(ret != 0)
        {
            // Write operation
            ret = update_configuration(&new_config, command, arg);
            if (ret == 0)
            {
                // Apply new configuration
                memcpy(&context->config, &new_config, sizeof(struct config));
                dmuart_port_deinit(context->config.instance);
                ret = configure(context);
                if (ret == 0)
                {
                    DMOD_LOG_INFO("UART reconfigured at %u baud\n", context->current_baudrate);
                }
            }
        }
    }

    return ret;
}

/**
 * @brief Flush device buffers
 * 
 * @param context DMDRVI context
 * @param handle Device handle
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    return 0;
}

/**
 * @brief Get device statistics
 * 
 * @param context DMDRVI context
 * @param path Device path
 * @param stat Pointer to dmdrvi_stat_t structure to fill
 * 
 * @return int 0 on success, non-zero on failure
 */
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    if(!is_valid_context(context) || stat == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters in dmuart_dmdrvi_stat\n");
        return -EINVAL;
    }

    stat->size = 0; // Stream device, no fixed size
    stat->mode = 0666; // Read-write permissions
    return 0;
}
