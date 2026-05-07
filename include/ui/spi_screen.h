#ifndef SPI_SCREEN_H__
#define SPI_SCREEN_H__

/**
 * Thread entry for the SPI OLED menu process.
 *
 * Initializes the SSD1306 display and renders the scheduler process menu. The
 * rotary encoder is used to select a thread and edit its quantum through the
 * scheduler API.
 *
 * @param info Unused thread argument.
 */
void spi_screen_thread(void *info) __attribute__((no_instrument_function));

/**
 * Runs the SPI OLED menu without creating a scheduler thread.
 *
 * This helper is useful for display bring-up tests where the screen process
 * should own execution directly.
 */
void spi_screen_run_forever(void) __attribute__((no_instrument_function));

#endif // SPI_SCREEN_H__
