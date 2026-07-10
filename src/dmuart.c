#define DMOD_ENABLE_REGISTRATION    ON
#include "dmod.h"
#include "dmuart.h"
#include "dmuart_port.h"
#include "dmdrvi.h"
#include "dmhaman.h"
#include "dmini.h"
#include "dm_sw_ring.h"
#include <errno.h>
#include <string.h>

/* Magic set to UART */
#define DMUART_CONTEXT_MAGIC    0x44555254

/**
 * @brief DMDRVI context structure
 */
struct dmdrvi_context
{
    uint32_t            magic;                      /**< Magic number for validation */
    dmuart_config_t     config;                     /**< Configuration parameters */
    dmuart_baudrate_t   current_baudrate;           /**< Current baud rate */
    char               *interrupt_handler_name;     /**< dmhaman handler name (NULL = not used) */
    dm_sw_ring_t        rx_ring;                    /**< Software ring buffer for received bytes */
    uint32_t            rx_ring_size;               /**< Capacity of the RX ring buffer (from config) */
    dm_sw_ring_flags_t  rx_ring_wait_flags;          /**< RX ring read-wait behavior (from config) */
};

static int is_valid_context(dmdrvi_context_t context)
{
    return (context != NULL && context->magic == DMUART_CONTEXT_MAGIC);
}

/* ---- Interrupt dispatch ---- */

/* Dispatches port interrupt events to a dmhaman-registered handler. */
static void internal_interrupt_handler(void *user_ptr, dmuart_instance_t instance,
                                        dmuart_int_trigger_t trigger, uint8_t data)
{
    dmdrvi_context_t ctx = (dmdrvi_context_t)user_ptr;

    if (ctx->interrupt_handler_name != NULL)
    {
        dmuart_interrupt_params_t params;
        params.instance = instance;
        params.trigger  = trigger;
        params.data     = data;
        dmhaman_call_handler(ctx->interrupt_handler_name, &params);
    }
}

/* ---- String conversion helpers ---- */

static dmuart_parity_t string_to_parity(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "even") == 0) return dmuart_parity_even;
        if (strcmp(s, "odd")  == 0) return dmuart_parity_odd;
    }
    return dmuart_parity_none;
}

static dmuart_stop_bit_t string_to_stop_bit(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "2") == 0) return dmuart_stop_bit_2;
    }
    return dmuart_stop_bit_1;
}

static dmuart_flow_control_t string_to_flow_control(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "rts_cts") == 0) return dmuart_flow_control_rts_cts;
    }
    return dmuart_flow_control_none;
}

static dmuart_bit_order_t string_to_bit_order(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "msb_first") == 0) return dmuart_bit_order_msb_first;
    }
    return dmuart_bit_order_lsb_first;
}

static dmuart_invert_t string_to_invert(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "tx")   == 0) return dmuart_invert_tx;
        if (strcmp(s, "rx")   == 0) return dmuart_invert_rx;
        if (strcmp(s, "both") == 0) return dmuart_invert_both;
    }
    return dmuart_invert_none;
}

static dmuart_loopback_t string_to_loopback(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "on") == 0) return dmuart_loopback_on;
    }
    return dmuart_loopback_off;
}

static dmuart_int_trigger_t string_to_interrupt_trigger(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "rx_not_empty") == 0) return dmuart_int_trigger_rx_not_empty;
        if (strcmp(s, "tx_empty")     == 0) return dmuart_int_trigger_tx_empty;
        if (strcmp(s, "tx_complete")  == 0) return dmuart_int_trigger_tx_complete;
        if (strcmp(s, "idle")         == 0) return dmuart_int_trigger_idle;
        if (strcmp(s, "error")        == 0) return dmuart_int_trigger_error;
    }
    return dmuart_int_trigger_off;
}

static dm_sw_ring_flags_t string_to_rx_ring_wait_flags(const char *s)
{
    if (s != NULL)
    {
        if (strcmp(s, "none")     == 0) return 0;
        if (strcmp(s, "all_data") == 0) return dm_sw_ring_flags_wait_for_all_data;
    }
    return dm_sw_ring_flags_wait_for_some_data;
}

static dmuart_word_length_t int_to_word_length(int val)
{
    switch (val)
    {
        case 7:  return dmuart_word_length_7;
        case 9:  return dmuart_word_length_9;
        default: return dmuart_word_length_8;
    }
}

/* ---- Configuration ---- */

/**
 * @brief Detect which INI section holds the UART configuration.
 *
 * The driver supports two config file layouts:
 *   1. Standard: a [dmuart] section (e.g. examples/config.ini).
 *   2. Board / device-named: a section whose name is the device label
 *      (e.g. [stlink_vcp] in board config files).
 *
 * Detection strategy:
 *   a) If [dmuart] contains a 'baudrate' or 'instance' key → use "dmuart".
 *   b) Otherwise serialise the INI to a temporary string and scan for
 *      the first named section (skipping [main]) that contains 'baudrate'
 *      or 'instance'.  Copy its name into section_buf and return it.
 *   c) Fall back to "dmuart" so the caller produces a meaningful error.
 *
 * @param ini            INI context to inspect.
 * @param section_buf    Caller-supplied buffer that receives the name when
 *                       a non-"dmuart" section is chosen.
 * @param section_buf_sz Size of section_buf in bytes.
 * @return Pointer to the chosen section name (either the literal "dmuart"
 *         or section_buf).
 */
static const char *detect_config_section(dmini_context_t ini,
                                          char *section_buf, size_t section_buf_sz)
{
    /* Fast path – standard [dmuart] section present */
    if (dmini_has_key(ini, "dmuart", "baudrate") || dmini_has_key(ini, "dmuart", "instance"))
        return "dmuart";

    /* Slow path – serialise the INI and scan for the first usable section */
    int needed = dmini_generate_string(ini, NULL, 0);
    if (needed <= 1)
        return "dmuart";

    /* dmini_generate_string follows snprintf convention: the first call returns
     * the number of characters that would be written, NOT including the null
     * terminator.  Allocate one extra byte so the null terminator always fits
     * and the scanning loop below is guaranteed to find it within the buffer. */
    char *ini_str = (char *)Dmod_Malloc((size_t)needed + 1);
    if (ini_str == NULL)
        return "dmuart";

    if (dmini_generate_string(ini, ini_str, (size_t)needed + 1) <= 0)
    {
        Dmod_Free(ini_str);
        return "dmuart";
    }
    ini_str[needed] = '\0'; /* belt-and-suspenders: ensure the buffer is always
                             * null-terminated regardless of dmini API behaviour */

    const char *result = "dmuart";
    char *p = ini_str;
    while (*p != '\0')
    {
        if (*p == '[')
        {
            char *name_start = p + 1;
            char *name_end   = name_start;
            while (*name_end != '\0' && *name_end != ']' &&
                   *name_end != '\n'  && *name_end != '\r')
                name_end++;

            if (*name_end == ']')
            {
                size_t name_len = (size_t)(name_end - name_start);
                if (name_len > 0 && name_len < section_buf_sz)
                {
                    /* Copy to section_buf so we have a null-terminated name */
                    memcpy(section_buf, name_start, name_len);
                    section_buf[name_len] = '\0';

                    if (strcmp(section_buf, "main") != 0 &&
                        (dmini_has_key(ini, section_buf, "baudrate") ||
                         dmini_has_key(ini, section_buf, "instance")))
                    {
                        result = section_buf;
                        break;
                    }
                }
            }
        }
        p++;
    }

    Dmod_Free(ini_str);
    return result;
}

static int check_config_parameters(dmuart_config_t *cfg)
{
    if (cfg->baudrate == 0)
    {
        DMOD_LOG_ERROR("Baud rate not set in configuration\n");
        return -EINVAL;
    }
    if (cfg->word_length != dmuart_word_length_7 &&
        cfg->word_length != dmuart_word_length_8 &&
        cfg->word_length != dmuart_word_length_9)
    {
        DMOD_LOG_ERROR("Invalid word length in configuration (must be 7, 8, or 9)\n");
        return -EINVAL;
    }
    return 0;
}

static int read_config_parameters(dmdrvi_context_t context, dmini_context_t config)
{
    char section_buf[64];
    const char *section = detect_config_section(config, section_buf, sizeof(section_buf));
    context->config.baudrate    = (dmuart_baudrate_t)dmini_get_int(config, section, "baudrate", 0);
    context->config.word_length = int_to_word_length(dmini_get_int(config, section, "databits", 8));
    context->config.parity      = string_to_parity(dmini_get_string(config, section, "parity", "none"));
    context->config.stop_bit    = string_to_stop_bit(dmini_get_string(config, section, "stopbits", "1"));
    context->config.flow_control = string_to_flow_control(dmini_get_string(config, section, "flowcontrol", "none"));
    context->config.bit_order   = string_to_bit_order(dmini_get_string(config, section, "bit_order", "lsb_first"));
    context->config.invert      = string_to_invert(dmini_get_string(config, section, "invert", "none"));
    context->config.loopback    = string_to_loopback(dmini_get_string(config, section, "loopback", "off"));
    context->config.instance    = (dmuart_instance_t)dmini_get_int(config, section, "instance", 1);
    context->config.interrupt_trigger = string_to_interrupt_trigger(dmini_get_string(config, section, "interrupt_trigger", "off"));
    context->config.interrupt_handler = NULL;

    const char *handler_name = dmini_get_string(config, section, "interrupt_handler", NULL);
    context->interrupt_handler_name = (handler_name != NULL) ? Dmod_StrDup(handler_name) : NULL;

    context->rx_ring_size = (uint32_t)dmini_get_int(config, section, "rx_ring_size", 256);
    context->rx_ring_wait_flags = string_to_rx_ring_wait_flags(
        dmini_get_string(config, section, "rx_ring_wait_mode", "some_data"));

    return check_config_parameters(&context->config);
}

/**
 * @brief Apply configuration to the port layer
 */
static int configure(dmdrvi_context_t context)
{
    dmuart_config_t *c = &context->config;
    int ret;

    ret = dmuart_port_init(c->instance);
    if (ret != 0)
    {
        DMOD_LOG_ERROR("Failed to initialize UART%u\n", c->instance);
        return ret;
    }

    ret = dmuart_port_set_baudrate(c->instance, c->baudrate);
    if (ret != 0) goto err;

    ret = dmuart_port_set_word_length(c->instance, c->word_length);
    if (ret != 0) goto err;

    ret = dmuart_port_set_parity(c->instance, c->parity);
    if (ret != 0) goto err;

    ret = dmuart_port_set_stop_bit(c->instance, c->stop_bit);
    if (ret != 0) goto err;

    ret = dmuart_port_set_bit_order(c->instance, c->bit_order);
    if (ret != 0) goto err;

    ret = dmuart_port_set_invert(c->instance, c->invert);
    if (ret != 0) goto err;

    ret = dmuart_port_set_loopback(c->instance, c->loopback);
    if (ret != 0) goto err;

    dmuart_int_trigger_t trigger = c->interrupt_trigger;
    if (context->rx_ring_size > 0)
        trigger |= dmuart_int_trigger_rx_not_empty;

    if (trigger != dmuart_int_trigger_off)
    {
        ret = dmuart_port_set_interrupt_trigger(c->instance, trigger);
        if (ret != 0) goto err;
    }

    context->current_baudrate = dmuart_port_get_baudrate(c->instance);

    DMOD_LOG_INFO("UART%u configured: %u baud, %u%c%s\n",
        c->instance, c->baudrate,
        (unsigned)c->word_length,
        (c->parity == dmuart_parity_none) ? 'N' :
        (c->parity == dmuart_parity_even) ? 'E' : 'O',
        (c->stop_bit == dmuart_stop_bit_1) ? "1" : "2");

    return 0;

err:
    DMOD_LOG_ERROR("Failed to configure UART%u\n", c->instance);
    dmuart_port_deinit(c->instance);
    return ret;
}

/* ---- IOCTL helpers ---- */

static int update_configuration(dmuart_config_t *cfg, int command, void *arg)
{
    switch (command)
    {
        case dmuart_ioctl_cmd_set_baudrate:
            cfg->baudrate = *(dmuart_baudrate_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_word_length:
            cfg->word_length = *(dmuart_word_length_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_parity:
            cfg->parity = *(dmuart_parity_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_stop_bit:
            cfg->stop_bit = *(dmuart_stop_bit_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_flow_control:
            cfg->flow_control = *(dmuart_flow_control_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_bit_order:
            cfg->bit_order = *(dmuart_bit_order_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_invert:
            cfg->invert = *(dmuart_invert_t *)arg;
            break;
        case dmuart_ioctl_cmd_set_loopback:
            cfg->loopback = *(dmuart_loopback_t *)arg;
            break;
        default:
            return -EINVAL;
    }
    return check_config_parameters(cfg);
}

static int read_configuration(dmdrvi_context_t context, int command, void *arg)
{
    dmuart_config_t *c = &context->config;
    switch (command)
    {
        case dmuart_ioctl_cmd_get_baudrate:
            *(dmuart_baudrate_t *)arg = c->baudrate;
            break;
        case dmuart_ioctl_cmd_get_word_length:
            *(dmuart_word_length_t *)arg = c->word_length;
            break;
        case dmuart_ioctl_cmd_get_parity:
            *(dmuart_parity_t *)arg = c->parity;
            break;
        case dmuart_ioctl_cmd_get_stop_bit:
            *(dmuart_stop_bit_t *)arg = c->stop_bit;
            break;
        case dmuart_ioctl_cmd_get_flow_control:
            *(dmuart_flow_control_t *)arg = c->flow_control;
            break;
        case dmuart_ioctl_cmd_get_bit_order:
            *(dmuart_bit_order_t *)arg = c->bit_order;
            break;
        case dmuart_ioctl_cmd_get_invert:
            *(dmuart_invert_t *)arg = c->invert;
            break;
        case dmuart_ioctl_cmd_get_loopback:
            *(dmuart_loopback_t *)arg = c->loopback;
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

/* ---- DMOD lifecycle ---- */

int dmod_init(const Dmod_Config_t *Config)
{
    DMOD_LOG_INFO("DMUART interface module initialized\n");
    return 0;
}

int dmod_deinit(void)
{
    DMOD_LOG_INFO("DMUART interface module deinitialized\n");
    return 0;
}

/* ---- DMDRVI interface ---- */

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, dmdrvi_context_t, _create, ( dmini_context_t config, dmdrvi_dev_num_t* dev_num ))
{
    if (config == NULL || dev_num == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters to dmuart_dmdrvi_create\n");
        return NULL;
    }

    dmdrvi_context_t context = Dmod_Malloc(sizeof(struct dmdrvi_context));
    if (context == NULL)
        return NULL;

    memset(context, 0, sizeof(*context));
    context->magic = DMUART_CONTEXT_MAGIC;

    if (read_config_parameters(context, config) != 0 ||
        configure(context) != 0)
    {
        DMOD_LOG_ERROR("Failed to create DMDRVI context with provided configuration\n");
        Dmod_Free(context->interrupt_handler_name);
        Dmod_Free(context);
        return NULL;
    }

    /* Create RX ring buffer */
    if (context->rx_ring_size > 0)
    {
        /* No dm_sw_ring_flags_mutex_sync: this ring is written to directly from
         * the UART RX interrupt handler, and taking a FreeRTOS mutex from ISR
         * context is invalid (asserts in vPortEnterCritical). The ring's
         * lock-free critical-section path (used when no mutex is configured)
         * is safe for this single-ISR-producer / task-consumer pattern. */
        context->rx_ring = dm_sw_ring_create(context->rx_ring_size,
                                              dm_sw_ring_flags_drop_old_data | context->rx_ring_wait_flags);
        if (context->rx_ring == NULL)
        {
            DMOD_LOG_ERROR("Failed to create RX ring buffer (size=%u)\n", context->rx_ring_size);
        }
        else
        {
            dmuart_port_set_rx_ring(context->config.instance, context->rx_ring);
        }
    }

    /* Register interrupt handler if ring buffer or dmhaman handler is active */
    if (context->rx_ring != NULL || context->interrupt_handler_name != NULL)
    {
        if (dmuart_port_add_interrupt_handler(context->config.instance,
                internal_interrupt_handler, context) != 0)
        {
            DMOD_LOG_ERROR("Failed to register interrupt handler\n");
        }
    }

    DMOD_LOG_INFO("UART configured at %u baud\n", context->current_baudrate);

    /* Populate dev_num: minor = UART instance number (1-based). */
    dev_num->flags = DMDRVI_NUM_MINOR;
    dev_num->major = 0;
    dev_num->minor = (dmdrvi_dev_id_t)context->config.instance;

    /* If the config uses a named section (e.g. [stlink_vcp]) populate alt_name
     * so the device filesystem registers the device under that human-friendly
     * name instead of a numeric path. */
    char section_buf[DMDRVI_ALT_NAME_MAX_LEN + 1];
    const char *section = detect_config_section(config, section_buf, sizeof(section_buf));
    if (strcmp(section, "dmuart") != 0)
    {
        size_t name_len = strlen(section);
        if (name_len <= DMDRVI_ALT_NAME_MAX_LEN)
        {
            dev_num->flags |= DMDRVI_NUM_ALT_NAME;
            memcpy(dev_num->alt_name, section, name_len + 1);
        }
    }

    return context;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, void, _free, ( dmdrvi_context_t context ))
{
    if (is_valid_context(context))
    {
        if (context->rx_ring != NULL || context->interrupt_handler_name != NULL)
            dmuart_port_remove_interrupt_handler(context->config.instance, context);

        if (context->rx_ring != NULL)
        {
            dmuart_port_set_rx_ring(context->config.instance, NULL);
            dm_sw_ring_destroy(context->rx_ring);
            context->rx_ring = NULL;
        }

        Dmod_Free(context->interrupt_handler_name);
        dmuart_port_deinit(context->config.instance);
        context->magic = 0;
        Dmod_Free(context);
    }
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, void*, _open, ( dmdrvi_context_t context, int flags ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmuart_dmdrvi_open\n");
        return NULL;
    }
    return context;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, void, _close, ( dmdrvi_context_t context, void* handle ))
{
    /* No specific action needed to close the UART device handle */
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, size_t, _read, ( dmdrvi_context_t context, void* handle, void* buffer, size_t size, uint32_t offset ))
{
    if (!is_valid_context(context) || buffer == NULL || size == 0)
        return 0;

    size_t received = 0;
    int ret = dmuart_port_receive(context->config.instance, (uint8_t *)buffer, size, &received);
    if (ret != 0)
        return 0;
    return received;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, size_t, _write, ( dmdrvi_context_t context, void* handle, const void* buffer, size_t size, uint32_t offset ))
{
    if (!is_valid_context(context) || buffer == NULL || size == 0)
    {
        return 0;
    }

    int ret = dmuart_port_transmit(context->config.instance, (const uint8_t *)buffer, size);

    if (ret != 0)
    {
        return 0;
    }
    return size;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, int, _ioctl, ( dmdrvi_context_t context, void* handle, int command, void* arg ))
{
    int ret = 0;
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmuart_dmdrvi_ioctl\n");
        return -EINVAL;
    }

    if (command >= dmuart_ioctl_cmd_max)
    {
        DMOD_LOG_ERROR("Invalid ioctl command %d\n", command);
        return -EINVAL;
    }

    if (command == dmuart_ioctl_cmd_reconfigure)
    {
        dmuart_port_deinit(context->config.instance);
        ret = configure(context);
        return ret;
    }

    if (command == dmuart_ioctl_cmd_set_interrupt_handler)
    {
        if (arg == NULL)
        {
            /* Remove handler */
            dmuart_port_remove_interrupt_handler(context->config.instance, context);
            return 0;
        }
        return dmuart_port_add_interrupt_handler(
            context->config.instance,
            (dmuart_port_interrupt_handler_t)*(dmuart_interrupt_handler_t *)arg,
            context);
    }

    if (arg == NULL)
    {
        DMOD_LOG_ERROR("Null argument for ioctl command %d\n", command);
        return -EINVAL;
    }

    /* Try read first */
    ret = read_configuration(context, command, arg);
    if (ret != 0)
    {
        /* Not a read command - try write */
        dmuart_config_t new_config;
        memcpy(&new_config, &context->config, sizeof(dmuart_config_t));

        ret = update_configuration(&new_config, command, arg);
        if (ret == 0)
        {
            memcpy(&context->config, &new_config, sizeof(dmuart_config_t));
            dmuart_port_deinit(context->config.instance);
            ret = configure(context);
        }
    }

    return ret;
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, int, _flush, ( dmdrvi_context_t context, void* handle ))
{
    if (!is_valid_context(context))
    {
        DMOD_LOG_ERROR("Invalid DMDRVI context in dmuart_dmdrvi_flush\n");
        return -EINVAL;
    }

    return dmuart_port_flush(context->config.instance);
}

dmod_dmdrvi_dif_api_declaration(1.0, dmuart, int, _stat, ( dmdrvi_context_t context, const char* path, dmdrvi_stat_t* stat ))
{
    if (!is_valid_context(context) || stat == NULL)
    {
        DMOD_LOG_ERROR("Invalid parameters in dmuart_dmdrvi_stat\n");
        return -EINVAL;
    }

    stat->size = 0; /* Stream device, no fixed size */
    stat->mode = 0666; /* Read-write permissions */
    return 0;
}
