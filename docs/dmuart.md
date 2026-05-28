# DMUART - DMOD UART Driver Module

## Overview

DMUART is a DMOD module providing a hardware-abstracted UART (Universal Asynchronous Receiver-Transmitter) driver interface. It follows the DMOD driver interface (DMDRVI) pattern for consistent device access across different hardware platforms.

## Architecture

```
┌──────────────────────────────────────┐
│           Application                │
├──────────────────────────────────────┤
│         DMDRVI Interface             │
│   (open/close/read/write/ioctl)      │
├──────────────────────────────────────┤
│          DMUART Core                 │
│   (configuration, state management)  │
├──────────────────────────────────────┤
│        DMUART Port Layer             │
│   (hardware-specific implementation) │
├──────────────────────────────────────┤
│          Hardware (UART)             │
└──────────────────────────────────────┘
```

## Features

- Standard DMDRVI device interface (read/write/ioctl)
- Configurable baud rate, data bits, parity, stop bits, and flow control
- Multiple UART instance support
- Platform-independent core with hardware-specific port layer
- INI-based configuration via DMINI

## Device Path

UART devices are registered as `/dev/dmuart<N>` where N is the minor number (instance).

## Dependencies

- `dmdrvi` - DMOD Driver Interface
- `dmini` - INI configuration parser
