#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
// #include "pico/stdio_usb.h"
#include "pico/time.h" // Required for hardware absolute timing
#include "hardware/uart.h"

// FreeRTOS Includes
#include "FreeRTOS.h"
#include "task.h"

#define MAX_LINE_LEN 80
#define MENU_DISCONNECTED -2
#define MENU_TIMEOUT -3 // New timeout exception sentinel

// ANSI Terminal Utility Escape Sequences
#define CLEAR_SCREEN() printf("\033[2J")
#define CURSOR_HOME() printf("\033[H")
#define HIDE_CURSOR() printf("\033[?25l")
#define SHOW_CURSOR() printf("\033[?25h")
#define HIGHLIGHT_ON() printf("\033[7m")
#define HIGHLIGHT_OFF() printf("\033[0m")
#define CLEAR_TO_LINE_END() printf("\033[K")

// Non-blocking retrieval check optimized for the Debug Probe UART stream
int pico_freertos_getc_uart(void)
{
    // Check if the hardware UART ring buffer actually has bytes waiting
    if (uart_is_readable(uart_default))
    {
        return (int)uart_getc(uart_default);
    }

    // If no character is waiting, yield to FreeRTOS so it doesn't starve the core
    vTaskDelay(pdMS_TO_TICKS(10));
    return PICO_ERROR_TIMEOUT;
}

// Dynamic Interface Status Guard
bool is_serial_interface_connected(void)
{
#if PICO_STDIO_USB
    // In USB mode, actively check DTR/RTS lines for terminal dropouts
    return stdio_usb_connected();
#else
    // In hardware UART pin mode, there is no automatic DTR signal.
    // Return true so the menu remains functional over raw pin streams.
    return true;
#endif
}

// Thread-safe character retrieval wrapper
int pico_freertos_getc_safe(void)
{
    int c = PICO_ERROR_TIMEOUT;
    while (c == PICO_ERROR_TIMEOUT)
    {
        // Uses our smart conditional check
        if (!is_serial_interface_connected())
            return MENU_DISCONNECTED;

        c = getchar_timeout_us(0); // Works identically for USB or UART pins
        if (c == PICO_ERROR_TIMEOUT)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    return c;
}

static void redraw_menu_pico(const char **file_lines, int total_lines, int highlighted_idx, int timeout_seconds)
{
    CURSOR_HOME();
    printf("Pico 2 Bootloader Menu (Use Arrow Keys, Press Enter):\r\n");

    if (timeout_seconds >= 0)
    {
        printf("System will auto-boot image in %d seconds... ", timeout_seconds);
    }
    else
    {
        printf("Manual mode active.");
    }

    // CRITICAL WIPE: Clears old data (like "ds...") on this header line
    CLEAR_TO_LINE_END();
    printf("\r\n\n");

    for (int i = 0; i < total_lines; i++)
    {
        if (i == highlighted_idx)
        {
            HIGHLIGHT_ON();
            printf("> %s", file_lines[i]);
            HIGHLIGHT_OFF();
        }
        else
        {
            printf("  %s", file_lines[i]);
        }
        CLEAR_TO_LINE_END();
        printf("\r\n");
    }
    fflush(stdout);
}

int pico_select_line_timed(const char **file_lines, int total_lines, char *output_buffer, size_t max_len, int initial_timeout_ms)
{
    if (total_lines <= 0)
        return -1;

    int current_selection = 0;
    bool is_running = true;

    // Dynamic timing variables
    int timeout_ms = initial_timeout_ms;
    bool countdown_active = (timeout_ms > 0);
    bool key_pressed = false; // Tracks if the 30-second extension has been granted

    absolute_time_t start_time = get_absolute_time();
    int last_reported_seconds = -2;

    CLEAR_SCREEN();
    CURSOR_HOME();
    HIDE_CURSOR();

    while (is_running)
    {
        // 1. TIMING LOGIC EVALUATION
        int current_remaining_seconds = -1;
        if (countdown_active)
        {
            int64_t elapsed_us = absolute_time_diff_us(start_time, get_absolute_time());
            int64_t remaining_ms = timeout_ms - (elapsed_us / 1000);

            if (remaining_ms <= 0)
            {
                strncpy(output_buffer, file_lines[current_selection], max_len - 1);
                output_buffer[max_len - 1] = '\0';
                return MENU_TIMEOUT;
            }
            current_remaining_seconds = (int)(remaining_ms / 1000);
        }

        // 2. TIMED REDRAW
        if (current_remaining_seconds != last_reported_seconds)
        {
            redraw_menu_pico(file_lines, total_lines, current_selection, current_remaining_seconds);
            last_reported_seconds = current_remaining_seconds;
        }

        // 3. FETCH BYTES FROM THE HARDWARE REGISTER
        int c = PICO_ERROR_TIMEOUT;
        if (uart_is_readable(uart_default))
        {
            c = (int)uart_getc(uart_default);
        }

        if (c == PICO_ERROR_TIMEOUT)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // give users a one-time extension to 30 seconds the first time a key is pressed
        if (!key_pressed)
        {
            key_pressed = true;
            timeout_ms = 30000;               // Extend deadline window to 30 seconds
            start_time = get_absolute_time(); // Restart hardware elapsed microsecond clock anchor

            // Re-calculate the remaining time window for the immediate redraw loop
            int64_t elapsed_us = absolute_time_diff_us(start_time, get_absolute_time());
            current_remaining_seconds = (int)((timeout_ms - (elapsed_us / 1000)) / 1000);
        }

        // parse key combinations
        if (c == '\r' || c == '\n')
        {
            is_running = false;
        }
        else if (c == '\033')
        {
            sleep_us(2000);
            if (uart_is_readable(uart_default))
            {
                int next1 = (int)uart_getc(uart_default);
                if (next1 == '[' || next1 == 'O')
                {
                    sleep_us(2000);
                    if (uart_is_readable(uart_default))
                    {
                        int direction = (int)uart_getc(uart_default);
                        if (direction == 'A')
                        { // UP
                            if (current_selection > 0)
                                current_selection--;
                        }
                        else if (direction == 'B')
                        { // DOWN
                            if (current_selection < total_lines - 1)
                                current_selection++;
                        }
                    }
                }
            }
        }

        // Force an immediate screen redraw with the brand new 30-second timeline calculation
        redraw_menu_pico(file_lines, total_lines, current_selection, current_remaining_seconds);
        last_reported_seconds = current_remaining_seconds;
    }

    CLEAR_SCREEN();
    CURSOR_HOME();
    SHOW_CURSOR();

    strncpy(output_buffer, file_lines[current_selection], max_len - 1);
    output_buffer[max_len - 1] = '\0';

    return current_selection;
}

void vMenuInterfaceTask(void *pvParameters)
{
    //TODO: use mmap() to access the boot options from boot.cfg
    const char *mock_file_lines[] = {
        "Boot Image A (Production OS v2.0) [DEFAULT]",
        "Boot Image B (Fallback Recovery)",
        "Run Hardware Diagnostic Self-Test"};
    int total_lines = sizeof(mock_file_lines) / sizeof(mock_file_lines[0]);
    char selected_line[MAX_LINE_LEN];

    while (1)
    {
#if PICO_STDIO_USB
        while (!stdio_usb_connected())
        {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
#else
        vTaskDelay(pdMS_TO_TICKS(500));
#endif

        // Run menu loop
        int result = pico_select_line_timed(mock_file_lines, total_lines, selected_line, MAX_LINE_LEN, 10000);

        // Clear screen cleanly after any type of menu exit
        CLEAR_SCREEN();
        CURSOR_HOME();
        SHOW_CURSOR();

        if (result == MENU_TIMEOUT)
        {
            // Evaluated perfectly. The buffer already contains whichever line they were hovering on!
            printf("\r\n[TIMEOUT]: Selection deadline reached.\r\n");
        }
        else
        {
            printf("\r\n[BOOT SELECTION CONFIRMED]\r\n");
        }

        // Both code paths gracefully land here using the correct text payload
        printf("Executing Profile: \"%s\"\r\n\n", selected_line);
        vTaskSuspend(NULL);
    }
}
