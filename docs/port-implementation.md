# DMUART Port Implementation Guide

## Overview

This guide explains how to add UART hardware support for a new microcontroller family.

## Port Layer Architecture

The port layer provides hardware-specific UART implementations. Each MCU family has its own port directory:

```
src/port/
├── CMakeLists.txt          # Port build configuration
├── stm32_common/           # Common STM32 code
│   ├── stm32_common.c
│   └── stm32_common.h
└── stm32f7/                # STM32F7-specific port
    ├── config.cmake        # Architecture toolchain config
    └── port.c              # UART implementation
```

## Required Port Functions

A port must implement all functions declared in `include/dmuart_port.h`:

### dmuart_port_init

```c
int dmuart_port_init(uint32_t instance, dmuart_baudrate_t baudrate, 
                     dmuart_databits_t databits, uint8_t parity, 
                     uint8_t stopbits, uint8_t flowcontrol);
```

Initialize the UART hardware:
1. Enable peripheral clock
2. Configure GPIO pins (TX/RX, and optionally RTS/CTS)
3. Set word length, parity, stop bits
4. Configure baud rate
5. Enable UART peripheral

### dmuart_port_deinit

```c
int dmuart_port_deinit(uint32_t instance);
```

Disable the UART peripheral and release resources.

### dmuart_port_transmit

```c
int dmuart_port_transmit(uint32_t instance, const uint8_t* data, size_t size);
```

Transmit data bytes. Can be blocking (polling) or use interrupts/DMA.

### dmuart_port_receive

```c
int dmuart_port_receive(uint32_t instance, uint8_t* data, size_t size, size_t* received);
```

Receive data bytes. Should handle timeouts gracefully.

### dmuart_port_set_baudrate

```c
int dmuart_port_set_baudrate(uint32_t instance, dmuart_baudrate_t baudrate);
```

Change baud rate at runtime. Must disable/re-enable UART.

### dmuart_port_get_baudrate

```c
dmuart_baudrate_t dmuart_port_get_baudrate(uint32_t instance);
```

Return current baud rate by reading hardware registers.

## Adding a New Port

### Step 1: Create Port Directory

```bash
mkdir src/port/<mcu_family>
```

### Step 2: Create config.cmake

Set the appropriate toolchain:

```cmake
set(DMOD_TOOLS_NAME "arch/armv7/cortex-m4" CACHE STRING "Name of the tools configuration")
```

### Step 3: Implement port.c

Implement all port API functions for your hardware.

### Step 4: Add Register Definitions

Create `include/port/<mcu_family>_regs.h` with UART register definitions.

### Step 5: Update Build Configuration

The port CMakeLists.txt should automatically handle new MCU families when `DMUART_MCU_SERIES` matches.

## Example: Adding STM32F4 Support

1. Create `src/port/stm32f4/config.cmake`:
```cmake
set(DMOD_TOOLS_NAME "arch/armv7/cortex-m4" CACHE STRING "Name of the tools configuration")
```

2. Create `src/port/stm32f4/port.c` implementing all port functions

3. Create `include/port/stm32f4_regs.h` with STM32F4 UART base addresses

4. Build with:
```bash
cmake -DDMUART_MCU_SERIES=stm32f4 -B build
cmake --build build
```
