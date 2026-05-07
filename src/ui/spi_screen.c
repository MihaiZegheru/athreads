#include "ui/spi_screen.h"

#include "athread/athread.h"
#include "ui/encoder.h"

#include <avr/io.h>
#include <stdint.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64
#define OLED_PAGES  (OLED_HEIGHT / 8)

// Arduino Mega 2560 wiring, SSD1306 4-wire SPI mode:
// OLED DIN -> D51/PB2/MOSI, CLK -> D52/PB1/SCK, CS -> D53/PB0
// OLED DC  -> D49/PL0,       RES -> D48/PL1
//
// Waveshare 0.96inch OLED (A/B): set BS0=0 and BS1=0 for 4-wire SPI.
#define OLED_CS_DDR   DDRB
#define OLED_CS_PORT  PORTB
#define OLED_CS_BIT   PB0

#define OLED_DC_DDR   DDRL
#define OLED_DC_PORT  PORTL
#define OLED_DC_BIT   PL0

#define OLED_RES_DDR  DDRL
#define OLED_RES_PORT PORTL
#define OLED_RES_BIT  PL1

#define SPI_DDR       DDRB
#define SPI_MOSI_BIT  PB2
#define SPI_SCK_BIT   PB1

#define LED_L_DDR     DDRB
#define LED_L_PORT    PORTB
#define LED_L_BIT     PB7

__attribute__((no_instrument_function))
static void short_delay(uint16_t loops) {
    while (loops--) {
        for (volatile uint16_t i = 0; i < 500; i++) {
            __asm__ __volatile__("nop");
        }
    }
}

__attribute__((no_instrument_function))
static void cs_low(void) {
    OLED_CS_PORT &= (uint8_t)~(1 << OLED_CS_BIT);
}

__attribute__((no_instrument_function))
static void cs_high(void) {
    OLED_CS_PORT |= (1 << OLED_CS_BIT);
}

__attribute__((no_instrument_function))
static void dc_command(void) {
    OLED_DC_PORT &= (uint8_t)~(1 << OLED_DC_BIT);
}

__attribute__((no_instrument_function))
static void dc_data(void) {
    OLED_DC_PORT |= (1 << OLED_DC_BIT);
}

__attribute__((no_instrument_function))
static void spi_write(uint8_t value) {
    SPDR = value;
    while ((SPSR & (1 << SPIF)) == 0) {
    }
}

__attribute__((no_instrument_function))
static void write_command(uint8_t command) {
    dc_command();
    cs_low();
    spi_write(command);
    cs_high();
}

__attribute__((no_instrument_function))
static void set_page_column(uint8_t page, uint8_t column) {
    write_command((uint8_t)(0xB0 | (page & 0x07)));
    write_command((uint8_t)(0x00 | (column & 0x0F)));
    write_command((uint8_t)(0x10 | (column >> 4)));
}

__attribute__((no_instrument_function))
static void fill_screen(uint8_t pattern) {
    for (uint8_t page = 0; page < OLED_PAGES; page++) {
        set_page_column(page, 0);
        dc_data();
        cs_low();
        for (uint8_t x = 0; x < OLED_WIDTH; x++) {
            spi_write(pattern);
        }
        cs_high();
    }
}

__attribute__((no_instrument_function))
static const uint8_t *glyph_for(char c) {
    static const uint8_t space[5] = {0x00,0x00,0x00,0x00,0x00};
    static const uint8_t arrow[5] = {0x08,0x1C,0x3E,0x1C,0x08};
    static const uint8_t star[5]  = {0x14,0x08,0x3E,0x08,0x14};
    static const uint8_t colon[5] = {0x00,0x36,0x36,0x00,0x00};
    static const uint8_t digit[10][5] = {
        {0x3E,0x51,0x49,0x45,0x3E},
        {0x00,0x42,0x7F,0x40,0x00},
        {0x42,0x61,0x51,0x49,0x46},
        {0x21,0x41,0x45,0x4B,0x31},
        {0x18,0x14,0x12,0x7F,0x10},
        {0x27,0x45,0x45,0x45,0x39},
        {0x3C,0x4A,0x49,0x49,0x30},
        {0x01,0x71,0x09,0x05,0x03},
        {0x36,0x49,0x49,0x49,0x36},
        {0x06,0x49,0x49,0x29,0x1E}
    };
    static const uint8_t upper[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22}, {0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A}, {0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41}, {0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E}, {0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46},
        {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F},
        {0x3F,0x40,0x38,0x40,0x3F}, {0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
    };

    if (c == ' ') return space;
    if (c == '>') return arrow;
    if (c == '*') return star;
    if (c == ':') return colon;
    if (c >= '0' && c <= '9') return digit[c - '0'];
    if (c >= 'A' && c <= 'Z') return upper[c - 'A'];
    return space;
}

__attribute__((no_instrument_function))
static void draw_char(uint8_t x, uint8_t page, char c) {
    const uint8_t *glyph = glyph_for(c);
    set_page_column(page, x);
    dc_data();
    cs_low();
    for (uint8_t i = 0; i < 5; i++) {
        spi_write(glyph[i]);
    }
    spi_write(0x00);
    cs_high();
}

__attribute__((no_instrument_function))
static void draw_text(uint8_t x, uint8_t page, const char *text) {
    while (*text && x < (OLED_WIDTH - 5u)) {
        draw_char(x, page, *text++);
        x = (uint8_t)(x + 6u);
    }
}

__attribute__((no_instrument_function))
static void draw_u8(uint8_t x, uint8_t page, uint8_t value) {
    if (value >= 100) {
        draw_char(x, page, (char)('0' + (value / 100u)));
        x = (uint8_t)(x + 6u);
    }
    if (value >= 10) {
        draw_char(x, page, (char)('0' + ((value / 10u) % 10u)));
        x = (uint8_t)(x + 6u);
    }
    draw_char(x, page, (char)('0' + (value % 10u)));
}

__attribute__((no_instrument_function))
static void oled_init(void) {
    OLED_CS_DDR |= (1 << OLED_CS_BIT);
    OLED_DC_DDR |= (1 << OLED_DC_BIT);
    OLED_RES_DDR |= (1 << OLED_RES_BIT);
    SPI_DDR |= (1 << SPI_MOSI_BIT) | (1 << SPI_SCK_BIT) | (1 << OLED_CS_BIT);

    LED_L_DDR |= (1 << LED_L_BIT);
    LED_L_PORT &= (uint8_t)~(1 << LED_L_BIT);

    cs_high();
    OLED_RES_PORT |= (1 << OLED_RES_BIT);
    short_delay(20);
    OLED_RES_PORT &= (uint8_t)~(1 << OLED_RES_BIT);
    short_delay(20);
    OLED_RES_PORT |= (1 << OLED_RES_BIT);
    short_delay(80);

    // SPI enable, master, fosc/4. SSD1306 samples SPI mode 0.
    SPCR = (1 << SPE) | (1 << MSTR);
    SPSR = 0;

    write_command(0xAE); // display off
    write_command(0x20); // memory addressing mode
    write_command(0x02); // page addressing
    write_command(0xB0); // page start
    write_command(0xC8); // COM scan direction remapped
    write_command(0x00); // low column
    write_command(0x10); // high column
    write_command(0x40); // start line
    write_command(0x81); // contrast
    write_command(0x7F);
    write_command(0xA1); // segment remap
    write_command(0xA6); // normal display
    write_command(0xA8); // multiplex
    write_command(0x3F);
    write_command(0xA4); // output follows RAM
    write_command(0xD3); // display offset
    write_command(0x00);
    write_command(0xD5); // display clock
    write_command(0x80);
    write_command(0xD9); // pre-charge
    write_command(0xF1);
    write_command(0xDA); // COM pins
    write_command(0x12);
    write_command(0xDB); // VCOMH deselect
    write_command(0x40);
    write_command(0x8D); // charge pump
    write_command(0x14);
    write_command(0x2E); // deactivate scroll
    write_command(0xAF); // display on
    short_delay(40);
}

__attribute__((no_instrument_function))
static const char *thread_name(uint8_t tid) {
    switch (tid) {
        case 0: return "IDLE";
        case 1: return "MAIN";
        case 2: return "ENC";
        case 3: return "OLED";
        case 4: return "WRK1";
        case 5: return "WRK2";
        case 6: return "WRK3";
        case 7: return "WRK4";
        default: return "T";
    }
}

__attribute__((no_instrument_function))
static void draw_menu(uint8_t selected_tid, uint8_t edit_mode) {
    uint8_t count = athread_get_thread_count();

    fill_screen(0x00);
    draw_text(0, 0, edit_mode ? "EDIT QUANTUM" : "SELECT THREAD");

    uint8_t first_tid = (uint8_t)((selected_tid / 6u) * 6u);

    for (uint8_t row = 0; row < 6; row++) {
        uint8_t tid = (uint8_t)(first_tid + row);
        uint8_t page = (uint8_t)(row + 2u);
        uint8_t marker = (tid == selected_tid) ? (edit_mode ? '*' : '>') : ' ';
        uint8_t q = athread_get_quantum(tid);

        if (tid >= count) {
            break;
        }

        draw_char(0, page, (char)marker);
        draw_char(8, page, 'T');
        draw_u8(14, page, tid);
        draw_text(32, page, thread_name(tid));
        draw_char(86, page, 'Q');
        draw_u8(94, page, q);
    }
}

__attribute__((no_instrument_function))
void spi_screen_thread(void *info) {
    (void)info;

    uint8_t selected_tid = 0;
    uint8_t edit_mode = 0;
    int16_t last_encoder_value = encoder_get_value();
    uint8_t last_button_presses = encoder_get_button_presses();

    oled_init();
    draw_menu(selected_tid, edit_mode);

    while (1) {
        uint8_t needs_redraw = 0;
        uint8_t count = athread_get_thread_count();
        int16_t encoder_value = encoder_get_value();
        int16_t delta = (int16_t)(encoder_value - last_encoder_value);
        uint8_t button_presses = encoder_get_button_presses();

        if (count == 0) {
            count = 1;
        }
        if (selected_tid >= count) {
            selected_tid = (uint8_t)(count - 1u);
            needs_redraw = 1;
        }

        if (button_presses != last_button_presses) {
            edit_mode = edit_mode ? 0u : 1u;
            last_button_presses = button_presses;
            needs_redraw = 1;
        }

        if (delta != 0) {
            if (edit_mode) {
                int16_t q = (int16_t)athread_get_quantum(selected_tid) + delta;
                if (q < ATHREAD_MIN_QUANTUM_TICKS) {
                    q = ATHREAD_MIN_QUANTUM_TICKS;
                }
                if (q > 20) {
                    q = 20;
                }
                athread_set_quantum(selected_tid, (uint8_t)q);
            } else {
                int16_t next = (int16_t)selected_tid + delta;
                if (next < 0) {
                    next = 0;
                }
                if (next >= count) {
                    next = (int16_t)(count - 1u);
                }
                selected_tid = (uint8_t)next;
            }
            last_encoder_value = encoder_value;
            needs_redraw = 1;
        }

        if (needs_redraw) {
            draw_menu(selected_tid, edit_mode);
        }
        athread_yield();
    }
}

__attribute__((no_instrument_function))
void spi_screen_run_forever(void) {
    spi_screen_thread(0);
}
