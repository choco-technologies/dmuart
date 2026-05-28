# DMUART API Reference

## Types

### dmuart_frequency_t

```c
typedef uint64_t dmuart_frequency_t;
```

Represents clock frequency in Hertz (Hz). Uses 64-bit unsigned integer to support high-frequency clocks.

### dmuart_time_us_t

```c
typedef uint64_t dmuart_time_us_t;
```

Represents time in microseconds (µs). Used for delay operations.

### dmuart_source_t

```c
typedef enum 
{
    dmuart_source_unkown = 0,        /**< Unknown clock source */
    dmuart_source_internal,          /**< Internal RC oscillator */
    dmuart_source_external,          /**< External crystal or oscillator */
    dmuart_source_hibernation,       /**< Low-power hibernation clock source */
} dmuart_source_t;
```

Enumerates available clock sources.

### dmuart_ioctl_cmd_t

```c
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
```

IOCTL commands for clock device control.

## DMDRVI Interface Functions

### dmuart_dmdrvi_create

```c
dmdrvi_context_t dmuart_dmdrvi_create(dmini_context_t config, const dmdrvi_dev_num_t* dev_num);
```

Creates and initializes a new DMUART device context.

**Parameters:**
- `config`: DMINI configuration context with clock parameters
- `dev_num`: Device number structure (reserved for future use)

**Returns:**
- Valid context handle on success
- NULL on failure

**Configuration Requirements:**
The `config` parameter must contain a `[dmuart]` section with:
- `source`: Clock source string ("internal", "external", "hibernation")
- `target_frequency`: Target frequency in Hz
- `tolerance`: Acceptable frequency deviation in Hz
- `oscillator_frequency`: Oscillator frequency (required for external/hibernation sources)

**Example:**
```c
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t ctx = dmuart_dmdrvi_create(config, &dev_num);
if (ctx == NULL) {
    // Handle error
}
```

### dmuart_dmdrvi_free

```c
void dmuart_dmdrvi_free(dmdrvi_context_t context);
```

Frees resources associated with a DMUART device context.

**Parameters:**
- `context`: Device context to free

**Note:** Always call this function when done with a context to prevent memory leaks.

### dmuart_dmdrvi_open

```c
void* dmuart_dmdrvi_open(dmdrvi_context_t context, int flags);
```

Opens a clock device handle for operations.

**Parameters:**
- `context`: Device context from `dmuart_dmdrvi_create`
- `flags`: Open flags (DMDRVI_O_RDONLY supported, DMDRVI_O_WRONLY not supported)

**Returns:**
- Valid device handle on success
- NULL on failure

**Example:**
```c
void* handle = dmuart_dmdrvi_open(ctx, DMDRVI_O_RDONLY);
if (handle == NULL) {
    // Handle error
}
```

### dmuart_dmdrvi_close

```c
void dmuart_dmdrvi_close(dmdrvi_context_t context, void* handle);
```

Closes a device handle.

**Parameters:**
- `context`: Device context
- `handle`: Device handle to close

### dmuart_dmdrvi_read

```c
size_t dmuart_dmdrvi_read(dmdrvi_context_t context, void* handle, void* buffer, size_t size);
```

Reads current clock configuration as a formatted string.

**Parameters:**
- `context`: Device context
- `handle`: Device handle
- `buffer`: Buffer to receive data
- `size`: Size of buffer in bytes

**Returns:** Number of bytes written to buffer

**Output Format:**
```
frequency=<current_freq>;source=<source_name>;oscillator_frequency=<osc_freq>
```

**Example:**
```c
char buffer[256];
size_t bytes = dmuart_dmdrvi_read(ctx, handle, buffer, sizeof(buffer));
// buffer contains: "frequency=84000000;source=external;oscillator_frequency=8000000"
```

### dmuart_dmdrvi_write

```c
size_t dmuart_dmdrvi_write(dmdrvi_context_t context, void* handle, const void* buffer, size_t size);
```

Write operation - **not implemented**.

**Returns:** 0 (operation not supported)

### dmuart_dmdrvi_ioctl

```c
int dmuart_dmdrvi_ioctl(dmdrvi_context_t context, void* handle, int command, void* arg);
```

Performs control operations on the clock device.

**Parameters:**
- `context`: Device context
- `handle`: Device handle
- `command`: IOCTL command from `dmuart_ioctl_cmd_t`
- `arg`: Command-specific argument (pointer to data)

**Returns:**
- 0 on success
- Negative error code on failure

#### Get Commands

##### dmuart_ioctl_cmd_get_frequency

Gets the current actual clock frequency.

```c
dmuart_frequency_t freq;
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_frequency, &freq);
```

##### dmuart_ioctl_cmd_get_source

Gets the current clock source.

```c
dmuart_source_t source;
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_source, &source);
```

##### dmuart_ioctl_cmd_get_tolerance

Gets the frequency tolerance.

```c
dmuart_frequency_t tolerance;
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_tolerance, &tolerance);
```

##### dmuart_ioctl_cmd_get_oscillator_frequency

Gets the oscillator frequency.

```c
dmuart_frequency_t osc_freq;
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_oscillator_frequency, &osc_freq);
```

##### dmuart_ioctl_cmd_get_target_frequency

Gets the target frequency.

```c
dmuart_frequency_t target;
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_target_frequency, &target);
```

#### Set Commands

All set commands automatically trigger a clock reconfiguration after updating the parameter.

##### dmuart_ioctl_cmd_set_source

Sets the clock source.

```c
dmuart_source_t source = dmuart_source_external;
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_set_source, &source);
```

##### dmuart_ioctl_cmd_set_tolerance

Sets the frequency tolerance.

```c
dmuart_frequency_t tolerance = 1000; // ±1 kHz
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_set_tolerance, &tolerance);
```

##### dmuart_ioctl_cmd_set_oscillator_frequency

Sets the oscillator frequency.

```c
dmuart_frequency_t osc_freq = 8000000; // 8 MHz
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_set_oscillator_frequency, &osc_freq);
```

##### dmuart_ioctl_cmd_set_target_frequency

Sets the target frequency.

```c
dmuart_frequency_t target = 100000000; // 100 MHz
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_set_target_frequency, &target);
```

##### dmuart_ioctl_cmd_reconfigure

Reconfigures the clock with current settings without changing any parameters.

```c
int ret = dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_reconfigure, NULL);
```

### dmuart_dmdrvi_flush

```c
int dmuart_dmdrvi_flush(dmdrvi_context_t context, void* handle);
```

Flushes device buffers - no operation for clock device.

**Returns:** 0

### dmuart_dmdrvi_stat

```c
int dmuart_dmdrvi_stat(dmdrvi_context_t context, void* handle, dmdrvi_stat_t* stat);
```

Gets device statistics.

**Parameters:**
- `context`: Device context
- `handle`: Device handle
- `stat`: Pointer to stat structure to fill

**Returns:**
- 0 on success
- Negative error code on failure

**Stat Fields:**
- `size`: Size of the read data string
- `mode`: File permissions (0444 - read-only)

## Port Layer API

The port layer implements hardware-specific clock configuration. These functions are called internally by the core module.

### dmuart_port_configure_internal

```c
int dmuart_port_configure_internal(dmuart_frequency_t target_freq, dmuart_frequency_t tolerance);
```

Configures the system clock using the internal oscillator.

### dmuart_port_configure_external

```c
int dmuart_port_configure_external(dmuart_frequency_t target_freq, dmuart_frequency_t tolerance, dmuart_frequency_t oscillator_freq);
```

Configures the system clock using an external crystal or oscillator.

### dmuart_port_configure_hibernatation

```c
int dmuart_port_configure_hibernatation(dmuart_frequency_t target_freq, dmuart_frequency_t tolerance, dmuart_frequency_t oscillator_freq);
```

Configures the system clock using the hibernation clock source.

**Note:** Function name contains a typo (should be "hibernation") but is kept for API compatibility.

### dmuart_port_delay_us

```c
void dmuart_port_delay_us(dmuart_time_us_t time_us);
```

Delays execution for the specified number of microseconds.

### dmuart_port_get_current_frequency

```c
dmuart_frequency_t dmuart_port_get_current_frequency(void);
```

Returns the current system clock frequency in Hz.

## Error Codes

The module uses standard errno error codes:

| Code | Description |
|------|-------------|
| 0 | Success |
| -EINVAL | Invalid parameter or configuration |
| -ENOMEM | Memory allocation failure |

Error messages are logged using the DMOD logging system (DMOD_LOG_ERROR, DMOD_LOG_INFO).

## Thread Safety

The DMUART module is not inherently thread-safe. If multiple threads need to access the same device context, external synchronization must be provided by the application.

## Example: Complete Usage

```c
#include "dmuart.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdio.h>

int main(void)
{
    // Load configuration file
    dmini_context_t config = dmini_load("config.ini");
    if (config == NULL) {
        printf("Failed to load config\n");
        return -1;
    }

    // Create clock device
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmuart_dmdrvi_create(config, &dev_num);
    if (ctx == NULL) {
        printf("Failed to create clock device\n");
        dmini_free(config);
        return -1;
    }

    // Open device
    void* handle = dmuart_dmdrvi_open(ctx, DMDRVI_O_RDONLY);
    if (handle == NULL) {
        printf("Failed to open device\n");
        dmuart_dmdrvi_free(ctx);
        dmini_free(config);
        return -1;
    }

    // Read current configuration
    char buffer[256];
    dmuart_dmdrvi_read(ctx, handle, buffer, sizeof(buffer));
    printf("Current config: %s\n", buffer);

    // Get actual frequency
    dmuart_frequency_t freq;
    dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_frequency, &freq);
    printf("Actual frequency: %lu Hz\n", freq);

    // Change to 100 MHz
    dmuart_frequency_t new_freq = 100000000;
    if (dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_set_target_frequency, &new_freq) == 0) {
        dmuart_dmdrvi_ioctl(ctx, handle, dmuart_ioctl_cmd_get_frequency, &freq);
        printf("New frequency: %lu Hz\n", freq);
    }

    // Cleanup
    dmuart_dmdrvi_close(ctx, handle);
    dmuart_dmdrvi_free(ctx);
    dmini_free(config);

    return 0;
}
```
