#include <dmod.h>
#include "dmuart_port.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief dmuart_test – UART loopback test application.
 *
 * Usage:
 *   dmuart_test <instance> [baudrate]
 *
 * The application initializes the specified UART instance and performs a
 * loopback test by transmitting a known pattern and verifying that the
 * same data is received back. Requires TX connected to RX (loopback).
 */

int main(int argc, char* argv[])
{
    Dmod_Printf("\n=== DMUART Loopback Test ===\n\n");

    if (argc < 2)
    {
        Dmod_Printf("Usage: dmuart_test <instance> [baudrate]\n");
        Dmod_Printf("Example: dmuart_test 1 115200\n\n");
        Dmod_Printf("Performs a UART loopback test on the specified instance.\n");
        Dmod_Printf("Requires TX connected to RX externally.\n");
        return -1;
    }

    uint32_t instance = 0;
    if (Dmod_Sscanf(argv[1], "%u", &instance) <= 0 || instance == 0)
    {
        Dmod_Printf("Error: invalid instance: '%s'\n", argv[1]);
        return -1;
    }

    uint32_t baudrate = 115200; /* default */
    if (argc >= 3)
    {
        if (Dmod_Sscanf(argv[2], "%u", &baudrate) <= 0 || baudrate == 0)
        {
            Dmod_Printf("Error: invalid baudrate: '%s'\n", argv[2]);
            return -1;
        }
    }

    Dmod_Printf("UART instance : %u\n", instance);
    Dmod_Printf("Baud rate     : %u\n\n", baudrate);

    /* Initialize UART: 8N1, no flow control */
    int ret = dmuart_port_init(instance, baudrate, 8, 0, 0, 0);
    if (ret != 0)
    {
        Dmod_Printf("Error: failed to initialize UART%u\n", instance);
        return -1;
    }

    Dmod_Printf("UART%u initialized successfully.\n", instance);

    /* Test pattern */
    const char* test_pattern = "Hello DMUART!";
    size_t pattern_len = strlen(test_pattern);

    Dmod_Printf("Transmitting: \"%s\" (%u bytes)\n", test_pattern, (unsigned)pattern_len);

    ret = dmuart_port_transmit(instance, (const uint8_t*)test_pattern, pattern_len);
    if (ret != 0)
    {
        Dmod_Printf("Error: transmission failed\n");
        dmuart_port_deinit(instance);
        return -1;
    }

    Dmod_Printf("Transmission complete. Waiting for loopback...\n");

    /* Receive data */
    uint8_t rx_buffer[64] = {0};
    size_t received = 0;

    ret = dmuart_port_receive(instance, rx_buffer, pattern_len, &received);
    if (ret != 0)
    {
        Dmod_Printf("Error: reception failed\n");
        dmuart_port_deinit(instance);
        return -1;
    }

    Dmod_Printf("Received %u bytes: \"%.*s\"\n", (unsigned)received, (int)received, rx_buffer);

    /* Verify */
    if (received == pattern_len && memcmp(rx_buffer, test_pattern, pattern_len) == 0)
    {
        Dmod_Printf("\n=== LOOPBACK TEST PASSED ===\n");
    }
    else
    {
        Dmod_Printf("\n=== LOOPBACK TEST FAILED ===\n");
        Dmod_Printf("Expected %u bytes, got %u\n", (unsigned)pattern_len, (unsigned)received);
    }

    /* Get current baudrate */
    dmuart_baudrate_t current_baud = dmuart_port_get_baudrate(instance);
    Dmod_Printf("Current baudrate: %u bps\n", current_baud);

    /* Cleanup */
    dmuart_port_deinit(instance);
    Dmod_Printf("UART%u deinitialized.\n\n", instance);

    return 0;
}
