# DMUART Configuration Files

This directory contains pre-configured UART settings for various development boards and MCUs.

## Directory Structure

```
configs/
├── board/          # Board-specific configurations
│   └── stm32f746g-disco.ini
└── mcu/            # MCU-specific configurations
    └── stm32f746zg.ini
```

## Usage

```bash
# Install with board-specific configuration
echo "dmuart@latest board/stm32f746g-disco.ini" > deps.dmd
dmf-get -d deps.dmd --config-dir ./config
```

## Board Configurations

| Board | File | Default Baud Rate | UART Instance |
|-------|------|-------------------|---------------|
| STM32F746G-DISCO | `board/stm32f746g-disco.ini` | 115200 | USART1 |

## MCU Configurations

| MCU | File | Notes |
|-----|------|-------|
| STM32F746ZG | `mcu/stm32f746zg.ini` | Default 115200 8N1 |
