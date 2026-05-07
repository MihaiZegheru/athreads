#include "encoder.h"

#include "athread.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// Rotary encoder wiring on Arduino Mega 2560:
// CLK -> D43/PL6, DT -> D45/PL4, SW -> D47/PL2
#define ENC_DDR      DDRL
#define ENC_PORT     PORTL
#define ENC_PIN      PINL
#define ENC_CLK_BIT  PL6
#define ENC_DT_BIT   PL4
#define ENC_SW_BIT   PL2

#define ENCODER_STABLE_POLLS 1
#define BUTTON_STABLE_POLLS  8

static volatile int16_t s_encoder_value = 0;
static volatile uint8_t s_button_presses = 0;
static volatile uint8_t s_encoder_initialized = 0;

static uint8_t s_raw_state = 0;
static uint8_t s_stable_state = 0;
static uint8_t s_stable_count = 0;
static int8_t s_accumulated_delta = 0;

static uint8_t s_raw_switch = 0;
static uint8_t s_stable_switch = 0;
static uint8_t s_previous_stable_switch = 0;
static uint8_t s_switch_stable_count = 0;

__attribute__((no_instrument_function))
static uint8_t read_clk_dt(void) {
    uint8_t pins = ENC_PIN;
    uint8_t clk = (pins & (1 << ENC_CLK_BIT)) ? 1u : 0u;
    uint8_t dt = (pins & (1 << ENC_DT_BIT)) ? 1u : 0u;
    return (uint8_t)((clk << 1) | dt);
}

__attribute__((no_instrument_function))
static uint8_t switch_is_pressed(void) {
    return (ENC_PIN & (1 << ENC_SW_BIT)) == 0;
}

__attribute__((no_instrument_function))
static void encoder_init(void) {
    ENC_DDR &= (uint8_t)~((1 << ENC_CLK_BIT) | (1 << ENC_DT_BIT) | (1 << ENC_SW_BIT));
    ENC_PORT |= (1 << ENC_CLK_BIT) | (1 << ENC_DT_BIT) | (1 << ENC_SW_BIT);

    s_raw_state = read_clk_dt();
    s_stable_state = s_raw_state;
    s_stable_count = 0;
    s_accumulated_delta = 0;

    s_raw_switch = switch_is_pressed();
    s_stable_switch = s_raw_switch;
    s_previous_stable_switch = s_stable_switch;
    s_switch_stable_count = 0;
    s_encoder_initialized = 1;
}

__attribute__((no_instrument_function))
int16_t encoder_get_value(void) {
    uint8_t sreg = SREG;
    cli();
    int16_t value = s_encoder_value;
    SREG = sreg;
    return value;
}

__attribute__((no_instrument_function))
uint8_t encoder_get_button_presses(void) {
    uint8_t sreg = SREG;
    cli();
    uint8_t presses = s_button_presses;
    SREG = sreg;
    return presses;
}

__attribute__((no_instrument_function))
void encoder_poll_tick(void) {
    static const int8_t transition_delta[16] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
    };

    if (!s_encoder_initialized) {
        return;
    }

    uint8_t sample = read_clk_dt();

    if (sample == s_raw_state) {
        if (s_stable_count < ENCODER_STABLE_POLLS) {
            s_stable_count++;
        }
    } else {
        s_raw_state = sample;
        s_stable_count = 0;
    }

    if (s_stable_count >= ENCODER_STABLE_POLLS && s_stable_state != s_raw_state) {
        uint8_t previous_stable_state = s_stable_state;
        s_stable_state = s_raw_state;
        int8_t delta = transition_delta[(previous_stable_state << 2) | s_stable_state];

        if (delta != 0) {
            s_accumulated_delta = (int8_t)(s_accumulated_delta + delta);
            if (s_accumulated_delta > 4) {
                s_accumulated_delta = 4;
            } else if (s_accumulated_delta < -4) {
                s_accumulated_delta = -4;
            }
        }

        if (s_stable_state == 0x03 && s_accumulated_delta != 0) {
            if (s_accumulated_delta > 0) {
                s_encoder_value++;
            } else {
                s_encoder_value--;
            }
            s_accumulated_delta = 0;
        }
    }

    uint8_t switch_sample = switch_is_pressed();
    if (switch_sample == s_raw_switch) {
        if (s_switch_stable_count < BUTTON_STABLE_POLLS) {
            s_switch_stable_count++;
        }
    } else {
        s_raw_switch = switch_sample;
        s_switch_stable_count = 0;
    }

    if (s_switch_stable_count >= BUTTON_STABLE_POLLS && s_stable_switch != s_raw_switch) {
        s_previous_stable_switch = s_stable_switch;
        s_stable_switch = s_raw_switch;
    }

    if (s_stable_switch && !s_previous_stable_switch) {
        s_button_presses++;
    }
    s_previous_stable_switch = s_stable_switch;
}

__attribute__((no_instrument_function))
void encoder_thread(void *info) {
    (void)info;

    encoder_init();

    while (1) {
        athread_yield();
    }
}
