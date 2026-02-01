/**
 * Unit tests for GPIO peripheral base address detection
 * 
 * These tests verify the Pi model detection and peripheral base address
 * selection logic, which is critical for Pi 3 to Pi 4 migration.
 */

#include "test_framework.h"
#include <stdint.h>

/*
 * Expected peripheral base addresses:
 * - Pi 1 (BCM2835): 0x20000000
 * - Pi 2/3 (BCM2836/BCM2837): 0x3F000000  
 * - Pi 4 (BCM2711): 0xFE000000
 */
#define PI1_PERI_BASE    0x20000000
#define PI2_3_PERI_BASE  0x3F000000
#define PI4_PERI_BASE    0xFE000000

/* Bus addresses */
#define PI1_BUS_ADDR     0x40000000
#define PI2_3_BUS_ADDR   0xC0000000
#define PI4_BUS_ADDR     0xC0000000

/* GPIO offsets - should be same across all Pi models */
#define GPIO_OFFSET      0x200000
#define PWM_OFFSET       0x20C000
#define CLOCK_OFFSET     0x101000
#define SPI0_OFFSET      0x204000

/**
 * Simulated function to detect Pi model from /proc/cpuinfo content
 * This mirrors the logic in minimal_gpio.c gpioHardwareRevision()
 * 
 * Returns: 1=Pi1, 2=Pi2/3, 4=Pi4
 */
static int detect_pi_model_from_cpuinfo(const char *model_name, const char *revision) {
    if (model_name == NULL) return 0;
    
    /* Check for ARMv6 (Pi 1) */
    if (strstr(model_name, "ARMv6") != NULL) {
        return 1;
    }
    
    /* Check for ARMv7 (Pi 2/3) */
    if (strstr(model_name, "ARMv7") != NULL) {
        return 2;
    }
    
    /* Check for ARMv8 (Pi 3 64-bit or Pi 4) */
    if (strstr(model_name, "ARMv8") != NULL) {
        /* Need to check revision to distinguish Pi 3 vs Pi 4 */
        /* Pi 4 revision codes start with 'a03111', 'b03111', 'c03111', etc. */
        /* Pi 4 revisions have format: uuuuuuuu FAQQQQQu CCCCpppp TTTTrrrr */
        /* Where T=type: 17=4B for Pi 4 */
        if (revision != NULL) {
            unsigned int rev_num = 0;
            if (sscanf(revision, "%x", &rev_num) == 1) {
                /* New-style revision with bit 23 set */
                if (rev_num & (1 << 23)) {
                    /* Extract type field (bits 4-11) */
                    int type = (rev_num >> 4) & 0xFF;
                    /* Type 17 (0x11) = Pi 4 Model B */
                    /* Type 19 (0x13) = Pi 400 */
                    /* Type 20 (0x14) = CM4 */
                    if (type == 0x11 || type == 0x13 || type == 0x14) {
                        return 4;
                    }
                }
            }
        }
        return 2; /* Default to Pi 2/3 for other ARMv8 */
    }
    
    return 0; /* Unknown */
}

/**
 * Get peripheral base address for a given Pi model
 */
static uint32_t get_peri_base_for_model(int model) {
    switch (model) {
        case 1: return PI1_PERI_BASE;
        case 2: return PI2_3_PERI_BASE;
        case 4: return PI4_PERI_BASE;
        default: return 0;
    }
}

/**
 * Get bus address for a given Pi model
 */
static uint32_t get_bus_addr_for_model(int model) {
    switch (model) {
        case 1: return PI1_BUS_ADDR;
        case 2: return PI2_3_BUS_ADDR;
        case 4: return PI4_BUS_ADDR;
        default: return 0;
    }
}

/* ==================== Test Cases ==================== */

void test_pi1_detection() {
    int model = detect_pi_model_from_cpuinfo("ARMv6-compatible processor rev 7 (v6l)", NULL);
    ASSERT_EQUAL(1, model);
}

void test_pi2_detection() {
    int model = detect_pi_model_from_cpuinfo("ARMv7 Processor rev 5 (v7l)", "a01041");
    ASSERT_EQUAL(2, model);
}

void test_pi3_armv7_detection() {
    int model = detect_pi_model_from_cpuinfo("ARMv7 Processor rev 4 (v7l)", "a02082");
    ASSERT_EQUAL(2, model);
}

void test_pi3_armv8_detection() {
    /* Pi 3 running 64-bit kernel shows ARMv8 */
    int model = detect_pi_model_from_cpuinfo("ARMv8 Processor", "a02082");
    ASSERT_EQUAL(2, model);
}

void test_pi4_detection() {
    /* Pi 4 Model B revision with new-style code */
    /* 0xa03111 = 10100000001100010001 in binary */
    /* Bit 23 set, Type=0x11 (17) = Pi 4 Model B */
    int model = detect_pi_model_from_cpuinfo("ARMv8 Processor", "a03111");
    ASSERT_EQUAL(4, model);
}

void test_pi4_8gb_detection() {
    /* Pi 4 8GB revision code */
    int model = detect_pi_model_from_cpuinfo("ARMv8 Processor", "d03114");
    ASSERT_EQUAL(4, model);
}

void test_pi400_detection() {
    /* Pi 400 revision code (type=0x13) */
    int model = detect_pi_model_from_cpuinfo("ARMv8 Processor", "c03130");
    ASSERT_EQUAL(4, model);
}

void test_pi1_peri_base() {
    uint32_t base = get_peri_base_for_model(1);
    ASSERT_HEX_EQUAL(PI1_PERI_BASE, base);
}

void test_pi2_3_peri_base() {
    uint32_t base = get_peri_base_for_model(2);
    ASSERT_HEX_EQUAL(PI2_3_PERI_BASE, base);
}

void test_pi4_peri_base() {
    uint32_t base = get_peri_base_for_model(4);
    ASSERT_HEX_EQUAL(PI4_PERI_BASE, base);
}

void test_gpio_offset_consistency() {
    /* GPIO offset from peripheral base should be 0x200000 for all models */
    uint32_t pi1_gpio = PI1_PERI_BASE + GPIO_OFFSET;
    uint32_t pi23_gpio = PI2_3_PERI_BASE + GPIO_OFFSET;
    uint32_t pi4_gpio = PI4_PERI_BASE + GPIO_OFFSET;
    
    ASSERT_HEX_EQUAL(0x20200000, pi1_gpio);
    ASSERT_HEX_EQUAL(0x3F200000, pi23_gpio);
    ASSERT_HEX_EQUAL(0xFE200000, pi4_gpio);
}

void test_pwm_offset_consistency() {
    /* PWM offset from peripheral base should be 0x20C000 for all models */
    uint32_t pi1_pwm = PI1_PERI_BASE + PWM_OFFSET;
    uint32_t pi23_pwm = PI2_3_PERI_BASE + PWM_OFFSET;
    uint32_t pi4_pwm = PI4_PERI_BASE + PWM_OFFSET;
    
    ASSERT_HEX_EQUAL(0x2020C000, pi1_pwm);
    ASSERT_HEX_EQUAL(0x3F20C000, pi23_pwm);
    ASSERT_HEX_EQUAL(0xFE20C000, pi4_pwm);
}

void test_pi1_bus_addr() {
    uint32_t addr = get_bus_addr_for_model(1);
    ASSERT_HEX_EQUAL(PI1_BUS_ADDR, addr);
}

void test_pi2_3_bus_addr() {
    uint32_t addr = get_bus_addr_for_model(2);
    ASSERT_HEX_EQUAL(PI2_3_BUS_ADDR, addr);
}

void test_pi4_bus_addr() {
    uint32_t addr = get_bus_addr_for_model(4);
    ASSERT_HEX_EQUAL(PI4_BUS_ADDR, addr);
}

void test_unknown_model_returns_zero() {
    int model = detect_pi_model_from_cpuinfo("Unknown Processor", NULL);
    ASSERT_EQUAL(0, model);
    
    uint32_t base = get_peri_base_for_model(0);
    ASSERT_EQUAL(0, base);
}

void test_null_model_name() {
    int model = detect_pi_model_from_cpuinfo(NULL, NULL);
    ASSERT_EQUAL(0, model);
}

/* ==================== Main ==================== */

int main(void) {
    TEST_SUITE_START("GPIO Peripheral Base Tests");
    
    printf("\n-- Pi Model Detection --\n");
    RUN_TEST(test_pi1_detection);
    RUN_TEST(test_pi2_detection);
    RUN_TEST(test_pi3_armv7_detection);
    RUN_TEST(test_pi3_armv8_detection);
    RUN_TEST(test_pi4_detection);
    RUN_TEST(test_pi4_8gb_detection);
    RUN_TEST(test_pi400_detection);
    
    printf("\n-- Peripheral Base Addresses --\n");
    RUN_TEST(test_pi1_peri_base);
    RUN_TEST(test_pi2_3_peri_base);
    RUN_TEST(test_pi4_peri_base);
    
    printf("\n-- Offset Consistency --\n");
    RUN_TEST(test_gpio_offset_consistency);
    RUN_TEST(test_pwm_offset_consistency);
    
    printf("\n-- Bus Addresses --\n");
    RUN_TEST(test_pi1_bus_addr);
    RUN_TEST(test_pi2_3_bus_addr);
    RUN_TEST(test_pi4_bus_addr);
    
    printf("\n-- Edge Cases --\n");
    RUN_TEST(test_unknown_model_returns_zero);
    RUN_TEST(test_null_model_name);
    
    TEST_SUITE_END();
    PRINT_TEST_SUMMARY();
    
    return TEST_EXIT_CODE();
}
