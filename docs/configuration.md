# DMUART Configuration Guide

## Configuration File Format

DMUART uses INI-format configuration files parsed by the DMINI module.

## Configuration Parameters

All parameters are in the `[dmuart]` section:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `baudrate` | integer | (required) | Baud rate in bps |
| `databits` | integer | 8 | Data bits (7, 8, or 9) |
| `parity` | string | "none" | Parity: "none", "even", "odd" |
| `stopbits` | string | "1" | Stop bits: "1", "2" |
| `flowcontrol` | string | "none" | Flow control: "none", "rts_cts" |
| `instance` | integer | 1 | UART instance number (1-8) |
| `rx_ring_size` | integer | 256 | Capacity of the RX ring buffer, in bytes (0 disables it) |
| `rx_ring_wait_mode` | string | "some_data" | RX ring read-wait behavior: "none" (return immediately), "some_data" (wait for at least one byte), "all_data" (wait for the full requested length) |

### Receiving by DMA (STM32F7)

On STM32F7 a UART with an RX ring receives by DMA: a circular 1 KiB buffer
(from the `dma` heap when there is one - uncached DTCM -, else the D-cache
is invalidated for it) that the USART's idle line and the DMA's half / end
of the buffer drain into the ring. No byte is lost to an interrupt that
runs late: the USART has no FIFO, at 921600 baud a byte comes every ~11 us.
The stream is one of the two the UART can use (dmdma's leases); none free -
the UART receives by interrupt, as without DMA.

The ring is then what holds the bytes until the reader is scheduled: size
it for that, not for a line. At 921600 baud 256 bytes are 2.8 ms; a reader
reading byte by byte behind a busy thread (a display's rendering) misses
them - the old ones are dropped. The F7 boards' consoles have 4096 (~44 ms).

## Examples

### Basic 115200 8N1

```ini
[dmuart]
baudrate=115200
databits=8
parity=none
stopbits=1
flowcontrol=none
instance=1
```

### 9600 baud with even parity

```ini
[dmuart]
baudrate=9600
databits=8
parity=even
stopbits=1
flowcontrol=none
instance=2
```

### High-speed with hardware flow control

```ini
[dmuart]
baudrate=921600
databits=8
parity=none
stopbits=1
flowcontrol=rts_cts
instance=1
```

### 7-bit data with 2 stop bits

```ini
[dmuart]
baudrate=19200
databits=7
parity=even
stopbits=2
flowcontrol=none
instance=3
```

## Usage with DMDRVI

```c
#include "dmuart.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};

// Create UART device
dmdrvi_context_t uart_ctx = dmuart_dmdrvi_create(config, &dev_num);

// Open device for read/write
void* handle = dmuart_dmdrvi_open(uart_ctx, DMDRVI_O_RDWR);

// Write data
const char* msg = "Hello UART!\n";
dmuart_dmdrvi_write(uart_ctx, handle, msg, strlen(msg), 0);

// Read data
char buffer[64];
size_t n = dmuart_dmdrvi_read(uart_ctx, handle, buffer, sizeof(buffer), 0);

// Change baud rate via ioctl
dmuart_baudrate_t new_baud = 9600;
dmuart_dmdrvi_ioctl(uart_ctx, handle, dmuart_ioctl_cmd_set_baudrate, &new_baud);
dmuart_dmdrvi_ioctl(uart_ctx, handle, dmuart_ioctl_cmd_reconfigure, NULL);

// Cleanup
dmuart_dmdrvi_close(uart_ctx, handle);
dmuart_dmdrvi_free(uart_ctx);
dmini_free(config);
```

## Pre-configured Boards

See [`configs/README.md`](../configs/README.md) for available board and MCU configurations.
