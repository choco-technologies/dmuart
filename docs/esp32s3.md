# DMUART on the ESP32-S3

Build with `-DDMUART_MCU_SERIES=esp32s3` (tools: `arch/xtensa/esp32s3`). Instances are **0, 1, 2** (UART0, UART1, UART2).

| | |
|---|---|
| Baud rate | from the XTAL (40 MHz), so it does not move when dmclk changes the CPU / APB frequency; 12-bit integer + 4-bit fraction divider, and a divider of the source clock for the low rates: 300 baud to 5 Mbaud tested |
| Word, parity, stop bits | 7 or 8 bits (`-ENOTSUP` for 9), none / even / odd, 1 or 2 stop bits |
| Bit order | LSB first only (`msb_first` is `-ENOTSUP`) |
| Invert, loopback | TXD / RXD inversion, the loopback of the chip (TX to RX inside the UART) |
| FIFO | 128 bytes in each direction. The receiver interrupt moves it into the ring of the driver when it holds 32 bytes or after ten quiet bit times |
| DMA | not used (`rx_dma` has no effect) |
| Flow control | RTS / CTS when `rts_pin` / `cts_pin` are given: RTS drops when the FIFO has 16 bytes left, CTS stops the transmitter |

## Pins

The signals go to the pads through the GPIO matrix, so any pad can carry any signal. The configuration names them:

```ini
[dmuart]
instance=1
baudrate=921600
tx_pin=17       ; GPIO number
rx_pin=18
rts_pin=15      ; optional
cts_pin=16      ; optional
```

A UART0 without pins uses GPIO43 (TX) and GPIO44 (RX), the pads of the console of the ROM. The other UARTs have no default pads.
GPIO22-25 do not exist; mind the protected ones (dmgpio): GPIO26-37 are the flash and the PSRAM of the ESP32-S3FN16R8, GPIO19 / 20 the USB.

## Interrupts of the CPU

The source of each UART goes to a level 1 interrupt of the CPU that nothing else takes, and the handler is the `DMOD_IRQ_HANDLER` of
this module (the firmware hands the interrupts of the CPU to dmod):

| UART | interrupt of the CPU |
|------|----------------------|
| UART0 | 3 |
| UART1 | 9 |
| UART2 | 12 |

(The GPIO of dmgpio has 2.)

Triggers: `rx_not_empty` (the FIFO is full to the threshold or the line was quiet), `idle` (the line was quiet), `tx_empty` (**once**:
the condition lasts while the FIFO is low, so the trigger is armed again by `set_interrupt_trigger`), `tx_complete`, `error` (parity,
framing, FIFO overflow).

## Limits

- Changing the line settings of a running line (the inversion, the loopback) can produce one garbage byte.
- Blocking `transmit` sleeps while the FIFO is full at rates below 460800 baud and spins above.
