# DMUART Configuration Files

This directory contains pre-configured UART settings for various development boards and MCUs.

## Directory Structure

```
configs/
├── board/                          # Board-specific configurations
│   ├── nucleo-f401re/              # NUCLEO-F401RE
│   │   ├── usart1.ini
│   │   └── usart2.ini
│   ├── nucleo-f411re/              # NUCLEO-F411RE
│   │   ├── usart1.ini
│   │   └── usart2.ini
│   ├── nucleo-f446re/              # NUCLEO-F446RE
│   │   ├── usart1.ini
│   │   └── usart2.ini
│   ├── nucleo-f767zi/              # NUCLEO-F767ZI
│   │   ├── usart3.ini
│   │   └── usart6.ini
│   ├── stm32f4-discovery/          # STM32F4-DISCOVERY
│   │   ├── usart1.ini
│   │   └── usart2.ini
│   ├── stm32f429i-discovery/       # STM32F429I-DISCOVERY
│   │   └── usart1.ini
│   ├── stm32f746g-disco/           # STM32F746G-DISCO
│   │   ├── usart1.ini
│   │   └── usart6.ini
│   └── stm32f769i-discovery/       # STM32F769I-DISCOVERY
│       ├── usart1.ini
│       └── usart6.ini
└── mcu/                            # MCU-specific configurations
    └── stm32f746zg.ini
```

## Configuration Format

Each UART configuration file contains:
- A UART section with `driver_name=dmuart` and UART parameters
- GPIO pin sections with `driver_name=dmgpio` for TX/RX pin configuration

This allows `dmdevfs` to automatically configure both the UART and its GPIO pins.

### Example (stm32f746g-disco/usart1.ini)

```ini
[stlink_vcp]
driver_name=dmuart
baudrate=921600
databits=8
parity=none
stopbits=1
flowcontrol=none
instance=1

[stlink_vcp_tx]
driver_name=dmgpio
pin=PA9
mode=alternate
alternate_function=7
speed=maximum
output_circuit=push_pull
pull=up

[stlink_vcp_rx]
driver_name=dmgpio
pin=PB7
mode=alternate
alternate_function=7
pull=up
```

## Board Configurations

| Board | Folder | Default Baud Rate | UART Instances |
|-------|--------|-------------------|----------------|
| NUCLEO-F401RE | `board/nucleo-f401re/` | 921600 | USART1, USART2 |
| NUCLEO-F411RE | `board/nucleo-f411re/` | 921600 | USART1, USART2 |
| NUCLEO-F446RE | `board/nucleo-f446re/` | 921600 | USART1, USART2 |
| NUCLEO-F767ZI | `board/nucleo-f767zi/` | 921600 | USART3, USART6 |
| STM32F4-DISCOVERY | `board/stm32f4-discovery/` | 921600 | USART1, USART2 |
| STM32F429I-DISCOVERY | `board/stm32f429i-discovery/` | 921600 | USART1 |
| STM32F746G-DISCO | `board/stm32f746g-disco/` | 921600 | USART1, USART6 |
| STM32F769I-DISCOVERY | `board/stm32f769i-discovery/` | 921600 | USART1, USART6 |

## MCU Configurations

| MCU | File | Notes |
|-----|------|-------|
| STM32F746ZG | `mcu/stm32f746zg.ini` | Default 921600 8N1 |
