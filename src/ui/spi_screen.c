#include "ui/spi_screen.h"

#include "athread/athread.h"
#include "ui/encoder.h"

#include <avr/io.h>
#include <stdint.h>

#define TFT_WIDTH   128
#define TFT_HEIGHT  160

// Arduino Mega 2560 wiring, ST7735 4-wire SPI mode:
// TFT SDA/MOSI/DIN -> D51/PB2/MOSI, SCK/CLK -> D52/PB1/SCK, CS -> D53/PB0
// TFT DC/A0        -> D49/PL0,       RST/RES -> D48/PL1
// TFT VCC          -> 5V,            GND -> GND,             BL/LED -> 5V
#define TFT_CS_DDR    DDRB
#define TFT_CS_PORT   PORTB
#define TFT_CS_BIT    PB0

#define TFT_DC_DDR    DDRL
#define TFT_DC_PORT   PORTL
#define TFT_DC_BIT    PL0

#define TFT_RST_DDR   DDRL
#define TFT_RST_PORT  PORTL
#define TFT_RST_BIT   PL1

#define SPI_DDR       DDRB
#define SPI_MOSI_BIT  PB2
#define SPI_SCK_BIT   PB1

#define LED_L_DDR     DDRB
#define LED_L_PORT    PORTB
#define LED_L_BIT     PB7

#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A
#define ST7735_INVON   0x21
#define ST7735_NORON   0x13

// This ST7735 module displays correctly only with INVON enabled. These color
// values are therefore pre-inverted: COLOR_BLACK sends white bits so the panel
// physically shows black, COLOR_WHITE sends black bits so it shows white, etc.
#define COLOR_BLACK    0xFFFF
#define COLOR_WHITE    0x0000
#define COLOR_RED      0x9FFF
#define COLOR_GREEN    0xFCDF
#define COLOR_BLUE     0xFFEF
#define COLOR_YELLOW   0x741F
#define COLOR_CYAN     0xFC10
#define COLOR_MAGENTA  0x97EF
#define COLOR_GRAY     0xBDF7
#define COLOR_BG       0xFFFF
#define COLOR_PANEL    0xF7BE
#define COLOR_PANEL2   0xEF7D
#define COLOR_SELECTED 0xDEFB
#define COLOR_ACCENT   0xFBAE
#define COLOR_EDIT     0xA7EF
#define COLOR_MUTED    0xC618
#define COLOR_GRID     0xEF5D

__attribute__((no_instrument_function))
static void delay_loops(uint16_t loops) {
    while (loops--) {
        for (volatile uint16_t i = 0; i < 500; i++) {
            __asm__ __volatile__("nop");
        }
    }
}

__attribute__((no_instrument_function))
static void cs_low(void) {
    TFT_CS_PORT &= (uint8_t)~(1 << TFT_CS_BIT);
}

__attribute__((no_instrument_function))
static void cs_high(void) {
    TFT_CS_PORT |= (1 << TFT_CS_BIT);
}

__attribute__((no_instrument_function))
static void dc_command(void) {
    TFT_DC_PORT &= (uint8_t)~(1 << TFT_DC_BIT);
}

__attribute__((no_instrument_function))
static void dc_data(void) {
    TFT_DC_PORT |= (1 << TFT_DC_BIT);
}

__attribute__((no_instrument_function))
static void spi_write(uint8_t value) {
    SPDR = value;
    while ((SPSR & (1 << SPIF)) == 0) {
    }
}

__attribute__((no_instrument_function))
static void tft_command(uint8_t command) {
    dc_command();
    cs_low();
    spi_write(command);
    cs_high();
}

__attribute__((no_instrument_function))
static void tft_data(uint8_t data) {
    dc_data();
    cs_low();
    spi_write(data);
    cs_high();
}

__attribute__((no_instrument_function))
static void tft_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    tft_command(ST7735_CASET);
    dc_data();
    cs_low();
    spi_write(0);
    spi_write(x0);
    spi_write(0);
    spi_write(x1);
    cs_high();

    tft_command(ST7735_RASET);
    dc_data();
    cs_low();
    spi_write(0);
    spi_write(y0);
    spi_write(0);
    spi_write(y1);
    cs_high();

    tft_command(ST7735_RAMWR);
}

__attribute__((no_instrument_function))
static void tft_push_color(uint16_t color) {
    spi_write((uint8_t)(color >> 8));
    spi_write((uint8_t)color);
}

__attribute__((no_instrument_function))
static void tft_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT || w == 0 || h == 0) {
        return;
    }
    if ((uint16_t)x + w > TFT_WIDTH) {
        w = (uint8_t)(TFT_WIDTH - x);
    }
    if ((uint16_t)y + h > TFT_HEIGHT) {
        h = (uint8_t)(TFT_HEIGHT - y);
    }

    tft_window(x, y, (uint8_t)(x + w - 1u), (uint8_t)(y + h - 1u));
    dc_data();
    cs_low();
    for (uint16_t i = 0; i < ((uint16_t)w * h); i++) {
        tft_push_color(color);
    }
    cs_high();
}

__attribute__((no_instrument_function))
static void tft_clear(void) {
    tft_fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BLACK);
}

__attribute__((no_instrument_function))
static const uint8_t *glyph_for(char c) {
    static const uint8_t space[5] = {0x00,0x00,0x00,0x00,0x00};
    static const uint8_t arrow[5] = {0x08,0x1C,0x3E,0x1C,0x08};
    static const uint8_t star[5]  = {0x14,0x08,0x3E,0x08,0x14};
    static const uint8_t colon[5] = {0x00,0x36,0x36,0x00,0x00};
    static const uint8_t slash[5] = {0x20,0x10,0x08,0x04,0x02};
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
    if (c == '/') return slash;
    if (c >= '0' && c <= '9') return digit[c - '0'];
    if (c >= 'A' && c <= 'Z') return upper[c - 'A'];
    return space;
}

__attribute__((no_instrument_function))
static void draw_char(uint8_t x, uint8_t y, char c, uint16_t fg, uint16_t bg) {
    const uint8_t *glyph = glyph_for(c);

    if (x > (TFT_WIDTH - 6u) || y > (TFT_HEIGHT - 8u)) {
        return;
    }

    tft_window(x, y, (uint8_t)(x + 5u), (uint8_t)(y + 7u));
    dc_data();
    cs_low();
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 6; col++) {
            uint8_t bits = (col < 5) ? glyph[col] : 0x00;
            tft_push_color((bits & (1 << row)) ? fg : bg);
        }
    }
    cs_high();
}

__attribute__((no_instrument_function))
static void draw_text(uint8_t x, uint8_t y, const char *text, uint16_t fg, uint16_t bg) {
    while (*text && x < (TFT_WIDTH - 5u)) {
        draw_char(x, y, *text++, fg, bg);
        x = (uint8_t)(x + 6u);
    }
}

__attribute__((no_instrument_function))
static void draw_u8(uint8_t x, uint8_t y, uint8_t value, uint16_t fg, uint16_t bg) {
    if (value >= 100) {
        draw_char(x, y, (char)('0' + (value / 100u)), fg, bg);
        x = (uint8_t)(x + 6u);
    }
    if (value >= 10) {
        draw_char(x, y, (char)('0' + ((value / 10u) % 10u)), fg, bg);
        x = (uint8_t)(x + 6u);
    }
    draw_char(x, y, (char)('0' + (value % 10u)), fg, bg);
}

__attribute__((no_instrument_function))
static void tft_init(void) {
    TFT_CS_DDR |= (1 << TFT_CS_BIT);
    TFT_DC_DDR |= (1 << TFT_DC_BIT);
    TFT_RST_DDR |= (1 << TFT_RST_BIT);
    SPI_DDR |= (1 << SPI_MOSI_BIT) | (1 << SPI_SCK_BIT) | (1 << TFT_CS_BIT);

    LED_L_DDR |= (1 << LED_L_BIT);
    LED_L_PORT &= (uint8_t)~(1 << LED_L_BIT);

    cs_high();
    TFT_RST_PORT |= (1 << TFT_RST_BIT);
    delay_loops(20);
    TFT_RST_PORT &= (uint8_t)~(1 << TFT_RST_BIT);
    delay_loops(20);
    TFT_RST_PORT |= (1 << TFT_RST_BIT);
    delay_loops(80);

    // SPI mode 0, master, fosc/4.
    SPCR = (1 << SPE) | (1 << MSTR);
    SPSR = 0;

    tft_command(ST7735_SWRESET);
    delay_loops(120);
    tft_command(ST7735_SLPOUT);
    delay_loops(120);
    tft_command(ST7735_COLMOD);
    tft_data(0x05); // RGB565.
    tft_command(ST7735_MADCTL);
    tft_data(0xC8); // Portrait orientation, BGR order
    tft_command(ST7735_INVON);
    tft_command(ST7735_NORON);
    delay_loops(20);
    tft_command(ST7735_DISPON);
    delay_loops(80);

    tft_clear();
}

__attribute__((no_instrument_function))
static const char *thread_name(uint8_t tid) {
    switch (tid) {
        case 0: return "IDLE";
        case 1: return "MAIN";
        case 2: return "ENC";
        case 3: return "TFT";
        case 4: return "WRK1";
        case 5: return "WRK2";
        case 6: return "WRK3";
        case 7: return "WRK4";
        default: return "T";
    }
}

__attribute__((no_instrument_function))
static uint16_t thread_color(uint8_t tid) {
    switch (tid) {
        case 0: return COLOR_MUTED;
        case 1: return COLOR_ACCENT;
        case 2: return 0x9CDF;
        case 3: return COLOR_GREEN;
        case 4: return COLOR_RED;
        case 5: return COLOR_MAGENTA;
        case 6: return COLOR_CYAN;
        case 7: return COLOR_GRAY;
        default: return COLOR_WHITE;
    }
}

__attribute__((no_instrument_function))
static void draw_row(uint8_t row, uint8_t tid, uint8_t selected_tid, uint8_t edit_mode) {
    uint8_t y = (uint8_t)(22u + row * 12u);
    uint16_t bg = (tid == selected_tid) ? COLOR_SELECTED : ((row & 1u) ? COLOR_PANEL : COLOR_BG);
    uint16_t fg = (tid == selected_tid) ? COLOR_WHITE : COLOR_MUTED;
    uint16_t accent = thread_color(tid);
    uint8_t q = athread_get_quantum(tid);
    uint8_t bar_w = q;

    if (bar_w > 20u) {
        bar_w = 20u;
    }

    tft_fill_rect(0, y, TFT_WIDTH, 10, bg);
    tft_fill_rect(0, y, 3, 10, accent);
    tft_fill_rect(4, (uint8_t)(y + 9u), 124, 1, COLOR_GRID);

    if (tid == selected_tid) {
        tft_fill_rect(4, y, 1, 10, edit_mode ? COLOR_EDIT : COLOR_ACCENT);
    }

    draw_char(7, y + 1u, tid == selected_tid ? (edit_mode ? '*' : '>') : ' ', COLOR_WHITE, bg);
    draw_char(17, y + 1u, 'T', fg, bg);
    draw_u8(25, y + 1u, tid, fg, bg);
    draw_text(42, y + 1u, thread_name(tid), fg, bg);
    draw_char(78, y + 1u, 'Q', COLOR_MUTED, bg);
    draw_u8(86, y + 1u, q, COLOR_WHITE, bg);

    tft_fill_rect(104, (uint8_t)(y + 3u), 20, 4, COLOR_PANEL);
    tft_fill_rect(104, (uint8_t)(y + 3u), bar_w, 4, edit_mode && tid == selected_tid ? COLOR_EDIT : accent);
}

__attribute__((no_instrument_function))
static uint8_t page_first_tid(uint8_t selected_tid) {
    return (uint8_t)((selected_tid / 8u) * 8u);
}

__attribute__((no_instrument_function))
static uint8_t page_row_for_tid(uint8_t tid) {
    return (uint8_t)(tid & 0x07u);
}

__attribute__((no_instrument_function))
static void draw_header(uint8_t selected_tid, uint8_t edit_mode, uint8_t count) {
    uint8_t first_tid = page_first_tid(selected_tid);
    uint8_t page = (uint8_t)(first_tid / 8u + 1u);
    uint8_t pages = (uint8_t)((count + 7u) / 8u);

    if (pages == 0) {
        pages = 1;
    }

    tft_fill_rect(0, 0, TFT_WIDTH, 16, COLOR_PANEL);
    tft_fill_rect(0, 15, TFT_WIDTH, 1, edit_mode ? COLOR_EDIT : COLOR_ACCENT);
    tft_fill_rect(0, 0, 3, 16, edit_mode ? COLOR_EDIT : COLOR_ACCENT);
    draw_text(4, 4, edit_mode ? "EDIT QUANT" : "SELECT TID", COLOR_WHITE,
              COLOR_PANEL);
    draw_char(100, 4, 'P', COLOR_MUTED, COLOR_PANEL);
    draw_u8(108, 4, page, COLOR_WHITE, COLOR_PANEL);
    draw_char(114, 4, '/', COLOR_MUTED, COLOR_PANEL);
    draw_u8(120, 4, pages, COLOR_WHITE, COLOR_PANEL);
}

__attribute__((no_instrument_function))
static void draw_menu_page(uint8_t selected_tid, uint8_t edit_mode) {
    uint8_t count = athread_get_thread_count();
    uint8_t first_tid = page_first_tid(selected_tid);

    if (count == 0) {
        count = 1;
    }

    draw_header(selected_tid, edit_mode, count);
    tft_fill_rect(0, 18, TFT_WIDTH, (uint8_t)(TFT_HEIGHT - 18u), COLOR_BG);

    for (uint8_t row = 0; row < 8; row++) {
        uint8_t tid = (uint8_t)(first_tid + row);
        if (tid >= count || tid >= ATHREAD_MAX_THREADS) {
            break;
        }
        draw_row(row, tid, selected_tid, edit_mode);
    }
}

__attribute__((no_instrument_function))
void spi_screen_thread(void *info) {
    (void)info;

    uint8_t selected_tid = 0;
    uint8_t edit_mode = 0;
    uint8_t last_count = 0;
    int16_t last_encoder_value = encoder_get_value();
    uint8_t last_button_presses = encoder_get_button_presses();

    tft_init();
    draw_menu_page(selected_tid, edit_mode);
    last_count = athread_get_thread_count();

    while (1) {
        uint8_t redraw_page = 0;
        uint8_t count = athread_get_thread_count();
        int16_t encoder_value = encoder_get_value();
        int16_t delta = (int16_t)(encoder_value - last_encoder_value);
        uint8_t button_presses = encoder_get_button_presses();

        if (count == 0) {
            count = 1;
        }
        if (selected_tid >= count) {
            selected_tid = (uint8_t)(count - 1u);
            redraw_page = 1;
        }
        if (count != last_count) {
            last_count = count;
            redraw_page = 1;
        }

        if (button_presses != last_button_presses) {
            edit_mode = edit_mode ? 0u : 1u;
            last_button_presses = button_presses;
            draw_header(selected_tid, edit_mode, count);
            draw_row(page_row_for_tid(selected_tid), selected_tid, selected_tid, edit_mode);
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
                draw_row(page_row_for_tid(selected_tid), selected_tid, selected_tid, edit_mode);
            } else {
                uint8_t old_selected_tid = selected_tid;
                uint8_t old_first_tid = page_first_tid(old_selected_tid);
                int16_t next = (int16_t)selected_tid + delta;
                if (next < 0) {
                    next = 0;
                }
                if (next >= count) {
                    next = (int16_t)(count - 1u);
                }
                selected_tid = (uint8_t)next;
                if (page_first_tid(selected_tid) != old_first_tid) {
                    redraw_page = 1;
                } else if (selected_tid != old_selected_tid) {
                    draw_row(page_row_for_tid(old_selected_tid), old_selected_tid,
                             selected_tid, edit_mode);
                    draw_row(page_row_for_tid(selected_tid), selected_tid,
                             selected_tid, edit_mode);
                }
            }
            last_encoder_value = encoder_value;
        }

        if (redraw_page) {
            draw_menu_page(selected_tid, edit_mode);
        }
        athread_yield();
    }
}

__attribute__((no_instrument_function))
void spi_screen_run_forever(void) {
    spi_screen_thread(0);
}
