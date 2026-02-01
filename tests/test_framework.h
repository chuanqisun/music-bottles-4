/**
 * Simple unit testing framework for Music Bottles
 * 
 * Provides basic test assertions and test organization macros.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test result macros
#define TEST_PASS() do { tests_passed++; printf("  PASS: %s\n", __func__); } while(0)
#define TEST_FAIL(msg) do { tests_failed++; printf("  FAIL: %s - %s\n", __func__, msg); } while(0)

// Assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        tests_run++; \
        if (!(condition)) { \
            TEST_FAIL("Expected true, got false: " #condition); \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(condition) \
    do { \
        tests_run++; \
        if (condition) { \
            TEST_FAIL("Expected false, got true: " #condition); \
            return; \
        } \
    } while(0)

#define ASSERT_EQUAL(expected, actual) \
    do { \
        tests_run++; \
        if ((expected) != (actual)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "Expected %ld, got %ld", (long)(expected), (long)(actual)); \
            TEST_FAIL(_msg); \
            return; \
        } \
    } while(0)

#define ASSERT_NOT_EQUAL(expected, actual) \
    do { \
        tests_run++; \
        if ((expected) == (actual)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "Expected not equal to %ld", (long)(expected)); \
            TEST_FAIL(_msg); \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQUAL(expected, actual) \
    do { \
        tests_run++; \
        if (strcmp((expected), (actual)) != 0) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "Expected '%s', got '%s'", (expected), (actual)); \
            TEST_FAIL(_msg); \
            return; \
        } \
    } while(0)

// Hexadecimal comparison with detailed output
#define ASSERT_HEX_EQUAL(expected, actual) \
    do { \
        tests_run++; \
        if ((expected) != (actual)) { \
            char _msg[256]; \
            snprintf(_msg, sizeof(_msg), "Expected 0x%lX, got 0x%lX", (unsigned long)(expected), (unsigned long)(actual)); \
            TEST_FAIL(_msg); \
            return; \
        } \
    } while(0)

// Run a test function
#define RUN_TEST(test_func) \
    do { \
        int _prev_failed = tests_failed; \
        test_func(); \
        if (tests_failed == _prev_failed) { \
            TEST_PASS(); \
        } \
    } while(0)

// Test suite organization
#define TEST_SUITE_START(name) \
    printf("\n=== Test Suite: %s ===\n", name)

#define TEST_SUITE_END() \
    printf("=================================\n")

// Print summary
#define PRINT_TEST_SUMMARY() \
    do { \
        printf("\n=================================\n"); \
        printf("Tests Run:    %d\n", tests_run); \
        printf("Tests Passed: %d\n", tests_passed); \
        printf("Tests Failed: %d\n", tests_failed); \
        printf("=================================\n"); \
    } while(0)

// Return exit code based on test results
#define TEST_EXIT_CODE() (tests_failed > 0 ? 1 : 0)

#endif /* TEST_FRAMEWORK_H */
