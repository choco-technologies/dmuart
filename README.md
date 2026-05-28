# DMUART - DMOD UART Driver Module

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A DMOD (Dynamic Modular System) module for UART (Universal Asynchronous Receiver-Transmitter) communication on embedded microcontrollers.

## Features

- **Standard DMDRVI Interface**: Read/write/ioctl device access pattern
- **Configurable Parameters**: Baud rate, data bits, parity, stop bits, flow control
- **Multiple Instances**: Support for multiple UART peripherals simultaneously
- **Hardware Abstraction**: Platform-independent API with hardware-specific implementations
- **DMDRVI Integration**: Full DMOD driver interface implementation
- **STM32 Support**: STM32F7 family currently supported
- **Extensible**: Easy to add support for additional microcontroller families

## Quick Start

### Installation

Using `dmf-get` from the DMOD release package:

```bash
dmf-get install dmuart
```

Or install with a pre-configured setup for your board:

```bash
# Create a dependencies file (deps.dmd)
echo "dmuart@latest board/stm32f746g-disco.ini" > deps.dmd

# Install with configuration
dmf-get -d deps.dmd --config-dir ./config
```

### Basic Usage

1. **Create a configuration file** (`config.ini`):

```ini
[dmuart]
baudrate=115200
databits=8
parity=none
stopbits=1
flowcontrol=none
instance=1
```

2. **Use in your code**:

```c
#include "dmuart.h"
#include "dmdrvi.h"
#include "dmini.h"

// Load configuration and create device
dmini_context_t config = dmini_load("config.ini");
dmdrvi_dev_num_t dev_num = {0};
dmdrvi_context_t uart_ctx = dmuart_dmdrvi_create(config, &dev_num);

// Open device for read/write
void* handle = dmuart_dmdrvi_open(uart_ctx, DMDRVI_O_RDWR);

// Write data
const char* msg = "Hello UART!\n";
dmuart_dmdrvi_write(uart_ctx, handle, msg, strlen(msg), 0);

// Read data
char buffer[64];
size_t n = dmuart_dmdrvi_read(uart_ctx, handle, buffer, sizeof(buffer), 0);

// Cleanup
dmuart_dmdrvi_close(uart_ctx, handle);
dmuart_dmdrvi_free(uart_ctx);
dmini_free(config);
```

## Building

### Prerequisites

- CMake 3.18 or higher
- ARM GCC toolchain (for embedded targets)
- DMOD framework (automatically fetched)

### Build Commands

```bash
# Configure for STM32F7
cmake -DDMUART_MCU_SERIES=stm32f7 -B build

# Build
cmake --build build
```

## Documentation

Comprehensive documentation is available in the `docs/` directory:

- **[dmuart.md](docs/dmuart.md)** - Module overview and architecture
- **[api-reference.md](docs/api-reference.md)** - Complete API documentation
- **[configuration.md](docs/configuration.md)** - Configuration guide with examples
- **[port-implementation.md](docs/port-implementation.md)** - Guide for adding hardware support

View documentation using `dmf-man`:

```bash
dmf-man dmuart          # Main documentation
dmf-man dmuart api      # API reference
dmf-man dmuart config   # Configuration guide
dmf-man dmuart port     # Port implementation guide
```

## Supported Platforms

| Platform | Status | Notes |
|----------|--------|-------|
| STM32F7  | ✅ Supported | Full UART support (USART1-6, UART4-5, UART7-8) |
| Other STM32 | 🔧 In Progress | Easy to add using STM32 common code |
| Other MCUs | 📋 Planned | Contributions welcome |

## Configuration Examples

### Standard 115200 8N1

```ini
[dmuart]
baudrate=115200
databits=8
parity=none
stopbits=1
flowcontrol=none
instance=1
```

### 9600 Baud with Even Parity

```ini
[dmuart]
baudrate=9600
databits=8
parity=even
stopbits=1
flowcontrol=none
instance=2
```

### High-Speed with Hardware Flow Control

```ini
[dmuart]
baudrate=921600
databits=8
parity=none
stopbits=1
flowcontrol=rts_cts
instance=1
```

## Development

### Project Structure

```
dmuart/
├── configs/           # Pre-configured board and MCU configurations
│   ├── board/        # Board-specific configurations
│   └── mcu/          # MCU-specific configurations
├── docs/              # Documentation (markdown format)
├── examples/          # Example configurations
├── include/           # Public headers
│   ├── dmuart.h      # Main API
│   ├── dmuart_port.h # Port layer API
│   └── port/         # Port-specific headers
├── src/
│   ├── dmuart.c      # Core implementation
│   └── port/         # Hardware-specific implementations
│       ├── stm32_common/  # Common STM32 code
│       └── stm32f7/       # STM32F7 port
├── tests/             # Test applications
├── CMakeLists.txt    # Build configuration
└── manifest.dmm      # DMOD manifest
```

### Adding New Platform Support

See [Port Implementation Guide](docs/port-implementation.md) for detailed instructions on adding support for new microcontrollers.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Authors

- Patryk Kubiak - Initial work

## Related Projects

- [DMOD](https://github.com/choco-technologies/dmod) - Dynamic Modular System framework
- [DMINI](https://github.com/choco-technologies/dmini) - INI configuration parser for DMOD
- [DMDRVI](https://github.com/choco-technologies/dmdrvi) - DMOD Driver Interface
- [DMGPIO](https://github.com/choco-technologies/dmgpio) - GPIO driver module
- [DMCLK](https://github.com/choco-technologies/dmclk) - Clock configuration module

## Support

For issues, questions, or contributions:

- Open an issue on GitHub
- Check the documentation in `docs/`
- Use `dmf-man dmuart` for command-line help
