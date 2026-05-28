#include <dmod.h>
#include "dmuart.h"
#include "dmdrvi.h"
#include "dmini.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief dmuart_test – UART loopback test application.
 *
 * Usage:
 *   dmuart_test <config_file>
 *
 * The application loads the given UART configuration file, creates a dmuart
 * device via the dmdrvi interface, and performs a loopback test by transmitting
 * a known pattern and verifying that the same data is received back.
 * Requires TX connected to RX (loopback).
 */

int main(int argc, char* argv[])
{
    Dmod_Printf("\n=== DMUART Loopback Test ===\n\n");

    if (argc < 2)
    {
        Dmod_Printf("Usage: dmuart_test <config_file>\n");
        Dmod_Printf("Example: dmuart_test configs/board/stm32f746g-disco/usart1.ini\n\n");
        Dmod_Printf("Performs a UART loopback test using the specified configuration.\n");
        Dmod_Printf("Requires TX connected to RX externally.\n");
        return -1;
    }

    const char *config_path = argv[1];
    Dmod_Printf("Config file: %s\n\n", config_path);

    /* Load configuration */
    dmini_context_t config = dmini_load(config_path);
    if (config == NULL)
    {
        Dmod_Printf("Error: failed to load configuration file '%s'\n", config_path);
        return -1;
    }

    /* Create UART device via dmdrvi */
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t ctx = dmuart_dmdrvi_create(config, &dev_num);
    if (ctx == NULL)
    {
        Dmod_Printf("Error: failed to create UART device from configuration\n");
        dmini_free(config);
        return -1;
    }

    Dmod_Printf("UART device created successfully.\n");

    /* Open device */
    void *handle = dmuart_dmdrvi_open(ctx, DMDRVI_O_RDWR);
    if (handle == NULL)
    {
        Dmod_Printf("Error: failed to open UART device\n");
        dmuart_dmdrvi_free(ctx);
        dmini_free(config);
        return -1;
    }

    /* Test pattern */
    const char *test_pattern = "Hello DMUART!";
    size_t pattern_len = strlen(test_pattern);

    Dmod_Printf("Transmitting: \"%s\" (%u bytes)\n", test_pattern, (unsigned)pattern_len);

    size_t written = dmuart_dmdrvi_write(ctx, handle, test_pattern, pattern_len, 0);
    if (written != pattern_len)
    {
        Dmod_Printf("Error: transmission failed (wrote %u of %u bytes)\n",
            (unsigned)written, (unsigned)pattern_len);
        dmuart_dmdrvi_close(ctx, handle);
        dmuart_dmdrvi_free(ctx);
        dmini_free(config);
        return -1;
    }

    Dmod_Printf("Transmission complete. Waiting for loopback...\n");

    /* Receive data */
    uint8_t rx_buffer[64] = {0};
    size_t received = dmuart_dmdrvi_read(ctx, handle, rx_buffer, pattern_len, 0);

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

    /* Test flush */
    int flush_ret = dmuart_dmdrvi_flush(ctx, handle);
    Dmod_Printf("Flush result: %d\n", flush_ret);

    /* Cleanup */
    dmuart_dmdrvi_close(ctx, handle);
    dmuart_dmdrvi_free(ctx);
    dmini_free(config);

    Dmod_Printf("Test complete.\n\n");

    return 0;
}
