#include "../Include/timer.h"

// ============================================
// PORT I/O
// ============================================
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// ============================================
// TSC
// ============================================
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// ============================================
// RTC (CMOS)
// ============================================
#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static void rtc_wait_ready(void) {
    int timeout = 100000;
    while ((cmos_read(0x0A) & 0x80) && --timeout > 0);
}

static uint8_t rtc_second(void) {
    rtc_wait_ready();
    return cmos_read(0x00);
}

// ============================================
// STATE
// ============================================
static uint64_t cpu_hz = 0;

static uint64_t calibrate_tsc(void) {
    uint8_t start_sec = rtc_second();
    int timeout = 2000000;

    while (rtc_second() == start_sec && --timeout > 0);
    if (timeout <= 0) return 2000000000ULL; // Fallback 2.0 GHz

    uint64_t tsc_start = rdtsc();

    uint8_t sec = rtc_second();
    timeout = 2000000;
    while (rtc_second() == sec && --timeout > 0);
    if (timeout <= 0) return 2000000000ULL; // Fallback 2.0 GHz

    uint64_t tsc_end = rdtsc();
    uint64_t diff = tsc_end - tsc_start;

    return diff;
}

// ============================================
// PUBLIC API
// ============================================
void timer_init(void) {
    cpu_hz = calibrate_tsc();

    // If QEMU RTC calibration produces unrealistic CPU frequency (< 100 MHz or > 10 GHz)
    if (cpu_hz < 100000000ULL || cpu_hz > 10000000000ULL) {
        cpu_hz = 2000000000ULL; // Default to 2.0 GHz for accurate millisecond timing
    }
}

uint64_t timer_cpu_hz(void) {
    return cpu_hz;
}

uint64_t timer_ms(void) {
    if (cpu_hz == 0) return 0;
    uint64_t cycles = rdtsc();
    return (cycles / (cpu_hz / 1000ULL));
}
