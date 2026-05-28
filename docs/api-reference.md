# DMUART API Reference

## DMDRVI Interface Functions

### dmuart_dmdrvi_create

```c
dmdrvi_context_t dmuart_dmdrvi_create(dmini_context_t config, dmdrvi_dev_num_t* dev_num);
```

Creates a new UART device context from INI configuration.

**Parameters:**
- `config` - DMINI context with UART configuration
- `dev_num` - Output pointer for device numbering

**Returns:** DMDRVI context on success, NULL on failure.

---

### dmuart_dmdrvi_free

```c
void dmuart_dmdrvi_free(dmdrvi_context_t context);
```

Frees the UART device context and deinitializes the hardware.

---

### dmuart_dmdrvi_open

```c
void* dmuart_dmdrvi_open(dmdrvi_context_t context, int flags);
```

Opens a handle to the UART device.

**Parameters:**
- `context` - DMDRVI context
- `flags` - Open flags (DMDRVI_O_RDONLY, DMDRVI_O_WRONLY, DMDRVI_O_RDWR)

**Returns:** Device handle on success, NULL on failure.

---

### dmuart_dmdrvi_close

```c
void dmuart_dmdrvi_close(dmdrvi_context_t context, void* handle);
```

Closes the device handle.

---

### dmuart_dmdrvi_read

```c
size_t dmuart_dmdrvi_read(dmdrvi_context_t context, void* handle, void* buffer, size_t size, uint32_t offset);
```

Reads received data from the UART.

**Parameters:**
- `context` - DMDRVI context
- `handle` - Device handle
- `buffer` - Buffer to read data into
- `size` - Maximum bytes to read
- `offset` - Not used (stream device)

**Returns:** Number of bytes actually read.

---

### dmuart_dmdrvi_write

```c
size_t dmuart_dmdrvi_write(dmdrvi_context_t context, void* handle, const void* buffer, size_t size, uint32_t offset);
```

Transmits data via UART.

**Parameters:**
- `context` - DMDRVI context
- `handle` - Device handle
- `buffer` - Data to transmit
- `size` - Number of bytes to transmit
- `offset` - Not used (stream device)

**Returns:** Number of bytes transmitted.

---

### dmuart_dmdrvi_ioctl

```c
int dmuart_dmdrvi_ioctl(dmdrvi_context_t context, void* handle, int command, void* arg);
```

Performs I/O control operations.

**Commands (dmuart_ioctl_cmd_t):**

| Command | Direction | Arg Type | Description |
|---------|-----------|----------|-------------|
| `dmuart_ioctl_cmd_get_baudrate` | Read | `dmuart_baudrate_t*` | Get current baud rate |
| `dmuart_ioctl_cmd_set_baudrate` | Write | `dmuart_baudrate_t*` | Set baud rate |
| `dmuart_ioctl_cmd_get_parity` | Read | `dmuart_parity_t*` | Get parity setting |
| `dmuart_ioctl_cmd_set_parity` | Write | `dmuart_parity_t*` | Set parity setting |
| `dmuart_ioctl_cmd_get_stopbits` | Read | `dmuart_stopbits_t*` | Get stop bits |
| `dmuart_ioctl_cmd_set_stopbits` | Write | `dmuart_stopbits_t*` | Set stop bits |
| `dmuart_ioctl_cmd_get_flowcontrol` | Read | `dmuart_flowcontrol_t*` | Get flow control |
| `dmuart_ioctl_cmd_set_flowcontrol` | Write | `dmuart_flowcontrol_t*` | Set flow control |
| `dmuart_ioctl_cmd_get_databits` | Read | `dmuart_databits_t*` | Get data bits |
| `dmuart_ioctl_cmd_set_databits` | Write | `dmuart_databits_t*` | Set data bits |
| `dmuart_ioctl_cmd_reconfigure` | - | NULL | Reconfigure with current settings |

---

## Port Layer API

### dmuart_port_init

```c
int dmuart_port_init(uint32_t instance, dmuart_baudrate_t baudrate, dmuart_databits_t databits, uint8_t parity, uint8_t stopbits, uint8_t flowcontrol);
```

Initialize a UART hardware instance.

---

### dmuart_port_deinit

```c
int dmuart_port_deinit(uint32_t instance);
```

Deinitialize a UART hardware instance.

---

### dmuart_port_transmit

```c
int dmuart_port_transmit(uint32_t instance, const uint8_t* data, size_t size);
```

Transmit data bytes over UART.

---

### dmuart_port_receive

```c
int dmuart_port_receive(uint32_t instance, uint8_t* data, size_t size, size_t* received);
```

Receive data bytes from UART.

---

### dmuart_port_set_baudrate

```c
int dmuart_port_set_baudrate(uint32_t instance, dmuart_baudrate_t baudrate);
```

Change the baud rate at runtime.

---

### dmuart_port_get_baudrate

```c
dmuart_baudrate_t dmuart_port_get_baudrate(uint32_t instance);
```

Get the current baud rate.
