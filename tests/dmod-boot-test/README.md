# DMUART Testing in dmod-boot Emulation Mode

## Overview

This document describes the testing of dmuart (DMOD Clock Configuration Module) in dmod-boot's Renode emulation mode for STM32F7. The purpose is to verify that the clock configuration works correctly in an emulated environment.

## Test Setup

### Prerequisites

1. Docker with `chocotechnologies/dmod:1.0.4` image
2. dmod-boot repository cloned from https://github.com/choco-technologies/dmod-boot
3. dmuart repository with modules built for STM32F7

### Building dmuart Modules

The dmuart and dmuart_port modules were built for STM32F7 (Cortex-M7) architecture:

```bash
cd /path/to/dmuart
docker run --rm -v $(pwd):/workspace -w /workspace chocotechnologies/dmod:1.0.4 bash -c \
  "mkdir -p build_stm32f7 && cd build_stm32f7 && \
   cmake .. -DDMOD_MODE=DMOD_MODULE -DDMOD_TOOLS_NAME=arch/armv7/cortex-m7 -DDMUART_MCU_SERIES=stm32f7 && \
   cmake --build . --target dmuart && \
   cmake --build . --target dmuart_port"
```

Output modules:
- `build_stm32f7/dmf/dmuart.dmf` (5.5 KB)
- `build_stm32f7/dmf/dmuart_port.dmf` (492 bytes)

### Creating DMP Package

A DMP package was created containing both dmuart modules:

```bash
docker run --rm -v $(pwd):/workspace -w /workspace chocotechnologies/dmod:1.0.4 \
  todmp dmuart_modules /path/to/modules /path/to/dmuart_modules.dmp
```

Result: `dmuart_modules.dmp` (6.1 KB)

### Integration with dmod-boot

#### Test Function

A test function was created in `src/test_dmuart.c` that:
1. Creates a dmuart device with configuration for STM32F7 (25MHz external oscillator, target 216MHz)
2. Queries clock information using IOCTL commands
3. Verifies the clock frequency is within tolerance
4. Tests reading clock information as a formatted string
5. Reports success/failure

#### Building dmod-boot with Test

```bash
cd /path/to/dmod-boot
git submodule update --init --recursive
docker run --rm -v $(pwd):/workspace -w /workspace chocotechnologies/dmod:1.0.4 bash -c \
  "mkdir -p build && cd build && \
   cmake .. -DDMBOOT_EMULATION=ON -DSTARTUP_DMP_FILE=/workspace/dmuart_modules.dmp -DTARGET=STM32F746xG && \
   cmake --build ."
```

### Running in Renode

To run the test in Renode emulation:

```bash
cd /path/to/dmod-boot/build
cmake --build . --target install-firmware  # Install firmware for Renode
cmake --build . --target connect           # Start Renode with GDB server
```

In another terminal:
```bash
cmake --build . --target monitor-gdb       # Monitor logs from firmware
```

## Test Configuration

The test uses the following clock configuration for STM32F7:

```ini
[dmuart]
source=external
target_frequency=216000000      # 216 MHz (STM32F7 max frequency)
tolerance=1000                  # ±1 kHz
oscillator_frequency=25000000   # 25 MHz external oscillator (STM32F7 Discovery)
```

## Bugs Found and Fixed

### 1. Config File Parameter Name Bug

**Location**: `examples/config.ini`

**Issue**: The example configuration file used `frequency` instead of `target_frequency`:

```ini
# Before (incorrect)
frequency=16000000

# After (correct)
target_frequency=16000000
```

**Impact**: Configuration would fail to load properly as the code expects `target_frequency`.

**Fix**: Changed parameter name to match the code's expectations.

### 2. Missing Return Statement in Read Function

**Location**: `src/dmuart.c`, function `dmdrvi_dmuart_read()`

**Issue**: The read function was missing a return statement, causing a compiler warning and undefined behavior:

```c
// Before (incorrect)
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, size_t, _read, ...)
{
    Dmod_SnPrintf(buffer, size, "frequency=%lu;source=%s;oscillator_frequency=%lu",
                  context->current_frequency,
                  source_to_string(context->config.source),
                  context->config.oscillator_frequency);
}

// After (correct)
dmod_dmdrvi_dif_api_declaration(1.0, dmuart, size_t, _read, ...)
{
    int written = Dmod_SnPrintf(buffer, size, "frequency=%lu;source=%s;oscillator_frequency=%lu",
                  context->current_frequency,
                  source_to_string(context->config.source),
                  context->config.oscillator_frequency);
    return (written > 0) ? (size_t)written : 0;
}
```

**Impact**: Undefined behavior when calling read function; callers would not know how many bytes were written.

**Fix**: Added proper return statement to return the number of bytes written.

## Test Results

The test setup has been prepared and all bugs found during integration have been fixed. The modules build successfully without warnings (after fixes).

### Expected Test Output

When running in Renode emulation, the test should output:

```
=== Testing DMUART Module ===
Creating configuration...
Creating dmuart device context...
Opening dmuart device...

--- Clock Configuration ---
Current frequency: 216000000 Hz (216 MHz)
Clock source: external
Target frequency: 216000000 Hz (216 MHz)
Tolerance: 1000 Hz
Oscillator frequency: 25000000 Hz (25 MHz)
Clock info string: frequency=216000000;source=external;oscillator_frequency=25000000

--- Verification ---
Target: 216000000 Hz, Actual: 216000000 Hz, Difference: 0 Hz, Tolerance: 1000 Hz
✓ Clock is configured within tolerance - TEST PASSED

=== DMUART Test Complete ===
```

## Files Modified

1. `examples/config.ini` - Fixed parameter name from `frequency` to `target_frequency`
2. `src/dmuart.c` - Added missing return statement in `dmdrvi_dmuart_read()` function
3. `tests/dmod-boot-test/` - Added test application structure (test_dmuart.c, CMakeLists.txt)

## Next Steps

1. Complete the dmod-boot build with the integrated test
2. Run the emulation in Renode
3. Verify the test output shows clock is configured correctly
4. Test with different clock configurations (internal oscillator, different frequencies)
5. Verify clock switching functionality

## Notes

- The test infrastructure demonstrates proper integration of dmuart with the DMOD framework
- The DMP packaging system allows easy distribution of modules
- Renode emulation provides a good environment for testing without physical hardware
- The bugs found were straightforward and have been fixed
