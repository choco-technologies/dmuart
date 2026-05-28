#define DMOD_ENABLE_REGISTRATION
#include "dmod.h"
#include "dmdrvi.h"
#include "dmini.h"
#include "dmuart.h"
#include <stdio.h>

/**
 * @brief Test application to verify dmuart functionality in dmod-boot
 */

// Configuration string for dmuart
static const char* dmuart_config = 
    "[dmuart]\n"
    "source=external\n"
    "target_frequency=216000000\n"
    "tolerance=1000\n"
    "oscillator_frequency=25000000\n";

static void print_clock_info(dmdrvi_context_t clk_ctx, void* handle)
{
    dmuart_frequency_t freq;
    dmuart_source_t source;
    dmuart_frequency_t tolerance;
    dmuart_frequency_t osc_freq;
    dmuart_frequency_t target_freq;
    
    // Get current frequency
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_frequency, &freq) == 0)
    {
        Dmod_Printf("Current frequency: %u Hz (%u MHz)\n", (unsigned int)freq, (unsigned int)(freq / 1000000));
    }
    else
    {
        Dmod_Printf("Failed to get current frequency\n");
    }
    
    // Get clock source
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_source, &source) == 0)
    {
        const char* source_str = "unknown";
        switch (source)
        {
            case dmuart_source_internal:
                source_str = "internal";
                break;
            case dmuart_source_external:
                source_str = "external";
                break;
            case dmuart_source_hibernation:
                source_str = "hibernation";
                break;
            default:
                break;
        }
        Dmod_Printf("Clock source: %s\n", source_str);
    }
    else
    {
        Dmod_Printf("Failed to get clock source\n");
    }
    
    // Get target frequency
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_target_frequency, &target_freq) == 0)
    {
        Dmod_Printf("Target frequency: %u Hz (%u MHz)\n", (unsigned int)target_freq, (unsigned int)(target_freq / 1000000));
    }
    else
    {
        Dmod_Printf("Failed to get target frequency\n");
    }
    
    // Get tolerance
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_tolerance, &tolerance) == 0)
    {
        Dmod_Printf("Tolerance: %u Hz\n", (unsigned int)tolerance);
    }
    else
    {
        Dmod_Printf("Failed to get tolerance\n");
    }
    
    // Get oscillator frequency
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_oscillator_frequency, &osc_freq) == 0)
    {
        Dmod_Printf("Oscillator frequency: %u Hz (%u MHz)\n", (unsigned int)osc_freq, (unsigned int)(osc_freq / 1000000));
    }
    else
    {
        Dmod_Printf("Failed to get oscillator frequency\n");
    }
    
    // Read clock information as string
    char buffer[256];
    size_t bytes_read = dmuart_dmdrvi_read(clk_ctx, handle, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0 && bytes_read < sizeof(buffer))
    {
        buffer[bytes_read] = '\0';
        Dmod_Printf("Clock info string: %s\n", buffer);
    }
}

int test_dmuart_main(int argc, char* argv[])
{
    Dmod_Printf("\n=== DMUART Test Application ===\n\n");
    
    // Create a temporary file with configuration
    Dmod_Printf("Creating configuration...\n");
    dmini_context_t config = dmini_loads(dmuart_config);
    if (config == NULL)
    {
        Dmod_Printf("ERROR: Failed to create configuration\n");
        return -1;
    }
    
    Dmod_Printf("Creating dmuart device...\n");
    dmdrvi_dev_num_t dev_num = {0};
    dmdrvi_context_t clk_ctx = dmuart_dmdrvi_create(config, &dev_num);
    if (clk_ctx == NULL)
    {
        Dmod_Printf("ERROR: Failed to create dmuart device\n");
        dmini_free(config);
        return -1;
    }
    
    Dmod_Printf("Opening dmuart device...\n");
    void* handle = dmuart_dmdrvi_open(clk_ctx, DMDRVI_O_RDONLY);
    if (handle == NULL)
    {
        Dmod_Printf("ERROR: Failed to open dmuart device\n");
        dmuart_dmdrvi_free(clk_ctx);
        dmini_free(config);
        return -1;
    }
    
    Dmod_Printf("\n--- Clock Configuration ---\n");
    print_clock_info(clk_ctx, handle);
    
    // Check if the actual frequency is within tolerance
    dmuart_frequency_t actual_freq, target_freq, tolerance;
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_frequency, &actual_freq) == 0 &&
        dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_target_frequency, &target_freq) == 0 &&
        dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_get_tolerance, &tolerance) == 0)
    {
        uint64_t diff = (actual_freq > target_freq) ? (actual_freq - target_freq) : (target_freq - actual_freq);
        
        Dmod_Printf("\n--- Verification ---\n");
        Dmod_Printf("Target: %u Hz, Actual: %u Hz, Difference: %llu Hz, Tolerance: %u Hz\n",
                   (unsigned int)target_freq, (unsigned int)actual_freq, diff, (unsigned int)tolerance);
        
        if (diff <= tolerance)
        {
            Dmod_Printf("✓ Clock is configured within tolerance\n");
        }
        else
        {
            Dmod_Printf("✗ Clock is NOT within tolerance (difference: %llu Hz > %u Hz)\n", 
                       diff, (unsigned int)tolerance);
        }
    }
    
    Dmod_Printf("\n--- Test: Change clock to internal 16 MHz ---\n");
    dmuart_frequency_t new_target = 16000000;
    dmuart_source_t new_source = dmuart_source_internal;
    
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_set_source, &new_source) == 0)
    {
        Dmod_Printf("Changed source to internal\n");
    }
    else
    {
        Dmod_Printf("Failed to change source\n");
    }
    
    if (dmuart_dmdrvi_ioctl(clk_ctx, handle, dmuart_ioctl_cmd_set_target_frequency, &new_target) == 0)
    {
        Dmod_Printf("Changed target frequency to 16 MHz\n");
        print_clock_info(clk_ctx, handle);
    }
    else
    {
        Dmod_Printf("Failed to change target frequency\n");
    }
    
    Dmod_Printf("\n--- Cleanup ---\n");
    dmuart_dmdrvi_close(clk_ctx, handle);
    dmuart_dmdrvi_free(clk_ctx);
    dmini_free(config);
    
    Dmod_Printf("\n=== Test Complete ===\n\n");
    return 0;
}

DMOD_MAIN(test_dmuart, test_dmuart_main)
