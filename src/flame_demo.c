#include "flame_demo.h"

#include <stdint.h>
#include <avr/interrupt.h>

__attribute__((no_instrument_function))
static void spin_delay(uint16_t n) {
    volatile uint16_t i;
    for (i = 0; i < n; i++) {
        __asm__ __volatile__("nop");
    }
}

__attribute__((no_instrument_function))
static uint8_t checksum8(uint8_t seed) {
    uint8_t x = seed;
    for (uint8_t i = 0; i < 24; i++) {
        x ^= (uint8_t)(i * 13u);
        x = (uint8_t)((x << 1) | (x >> 7));
        spin_delay(8);
    }
    return x;
}

__attribute__((no_instrument_function))
static uint16_t lcg16(uint16_t x) {
    return (uint16_t)(x * 109u + 89u);
}

/* -------------------------- Parse pipeline -------------------------- */

static void decode_magic(void) {
    spin_delay(110);
}

static void decode_header_flags(void) {
    spin_delay(90);
}

static void decode_header_crc(void) {
    (void)checksum8(0x12);
    spin_delay(60);
}

static void decode_header(void) {
    decode_magic();
    decode_header_flags();
    decode_header_crc();
}

static void unpack_token_stream(void) {
    for (uint8_t i = 0; i < 3; i++) {
        spin_delay(80);
    }
}

static void unpack_payload_chunks(void) {
    for (uint8_t i = 0; i < 4; i++) {
        spin_delay(70);
        (void)checksum8((uint8_t)(0x20u + i));
    }
}

static void validate_payload_crc(void) {
    (void)checksum8(0xA7);
    spin_delay(50);
}

static void decode_payload(void) {
    unpack_token_stream();
    unpack_payload_chunks();
    validate_payload_crc();
}

static void parse_frame(void) {
    decode_header();
    decode_payload();
}

/* ------------------------ Signal pipeline --------------------------- */

static void pre_emphasis(void) {
    for (uint8_t i = 0; i < 4; i++) {
        spin_delay(55);
    }
}

static void window_signal(void) {
    for (uint8_t i = 0; i < 4; i++) {
        spin_delay(60);
    }
}

static void filter_stage1(void) {
    pre_emphasis();
    window_signal();
}

static void convolve_kernel_a(void) {
    for (uint8_t i = 0; i < 3; i++) {
        spin_delay(75);
    }
}

static void convolve_kernel_b(void) {
    for (uint8_t i = 0; i < 2; i++) {
        spin_delay(95);
    }
}

static void filter_stage2(void) {
    convolve_kernel_a();
    convolve_kernel_b();
}

static void accumulate_bins(void) {
    for (uint8_t i = 0; i < 5; i++) {
        spin_delay(50);
    }
}

static void normalize_bins(void) {
    for (uint8_t i = 0; i < 3; i++) {
        spin_delay(65);
    }
}

static void aggregate_bins(void) {
    accumulate_bins();
    normalize_bins();
}

static void estimate_peak(void) {
    spin_delay(120);
}

static void compute_signal(void) {
    filter_stage1();
    filter_stage2();
    aggregate_bins();
    estimate_peak();
}

/* ------------------------- Logging / stats -------------------------- */

static void snapshot_counters(void) {
    spin_delay(90);
}

static void update_histograms(void) {
    for (uint8_t i = 0; i < 3; i++) {
        spin_delay(55);
    }
}

static void update_stats(void) {
    snapshot_counters();
    update_histograms();
}

static void encode_log_header(void) {
    spin_delay(70);
}

static void encode_log_payload(void) {
    for (uint8_t i = 0; i < 2; i++) {
        spin_delay(80);
    }
}

static void flush_log_chunk(void) {
    encode_log_header();
    encode_log_payload();
}

static void log_stats(void) {
    update_stats();
    flush_log_chunk();
}

/* ------------------------- Housekeeping ----------------------------- */

static void scan_free_list(void) {
    for (uint8_t i = 0; i < 4; i++) {
        spin_delay(45);
    }
}

static void coalesce_blocks(void) {
    for (uint8_t i = 0; i < 3; i++) {
        spin_delay(60);
    }
}

static void compact_heap_like(void) {
    scan_free_list();
    coalesce_blocks();
}

static void poll_watchdog_status(void) {
    spin_delay(85);
}

static void refresh_watchdog_state(void) {
    poll_watchdog_status();
    spin_delay(45);
}

static void housekeeping(void) {
    compact_heap_like();
    refresh_watchdog_state();
}

/* -------------------------- Rare deep path -------------------------- */

static void capture_error_context(void) {
    spin_delay(80);
}

static void classify_fault(void) {
    (void)checksum8(0xE1);
    spin_delay(70);
}

static void rewind_parser_state(void) {
    spin_delay(90);
}

static void resynchronize_stream(void) {
    spin_delay(95);
}

static void recover_stream_alignment(void) {
    rewind_parser_state();
    resynchronize_stream();
}

static void emit_fault_record(void) {
    spin_delay(100);
}

static void rare_error_path(void) {
    capture_error_context();
    classify_fault();
    recover_stream_alignment();
    emit_fault_record();
}

/* --------------------------- Rare audit path ------------------------ */

static void collect_audit_snapshot(void) {
    spin_delay(75);
}

static void hash_audit_window(void) {
    (void)checksum8(0x6B);
    spin_delay(60);
}

static void commit_audit_record(void) {
    spin_delay(85);
}

static void audit_path(void) {
    collect_audit_snapshot();
    hash_audit_window();
    commit_audit_record();
}

/* ----------------------- Strategy / top-level ----------------------- */

static void fast_path(uint16_t iter) {
    parse_frame();
    compute_signal();

    if ((iter & 0x03u) == 0u) {
        log_stats();
    }
}

static void balanced_path(uint16_t iter) {
    parse_frame();
    log_stats();
    compute_signal();

    if ((iter % 5u) == 0u) {
        housekeeping();
    }
}

static void maintenance_path(uint16_t iter) {
    parse_frame();
    housekeeping();
    compute_signal();

    if ((iter % 3u) == 0u) {
        log_stats();
    }
}

static void worker_iteration(uint16_t iter, uint8_t tid_hint) {
    uint16_t mix = lcg16((uint16_t)(iter + ((uint16_t)tid_hint << 8)));

    if ((tid_hint % 3u) == 0u) {
        fast_path(iter);
    } else if ((tid_hint % 3u) == 1u) {
        balanced_path(iter);
    } else {
        maintenance_path(iter);
    }

    if ((mix & 0x001Fu) == 0x0007u) {
        rare_error_path();
    }

    if ((mix & 0x003Fu) == 0x0013u) {
        audit_path();
    }
}

void flame_demo_work(uint8_t tid) {
    static uint16_t iter = 0;

    for (uint8_t i = 0; i < 4; i++) {
        worker_iteration(iter++, tid);
    }
}
