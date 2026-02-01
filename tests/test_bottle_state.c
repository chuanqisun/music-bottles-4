/**
 * Unit tests for bottle state matching logic
 * 
 * These tests verify the weight-based state detection algorithm that
 * determines which bottles/caps are present or removed.
 */

#include "test_framework.h"
#include <stdlib.h>

/*
 * State definitions for each bottle:
 * 0 = bottle + cap present
 * 1 = bottle present, cap removed  
 * 2 = bottle + cap removed
 * 
 * Total combinations: 3^3 = 27 states
 */

#define STABLE_THRESH 43  /* Threshold from musicBottles.c */

/* Test weight values (from runBottlesSquare.sh, divided by 100) */
#define BOT1_WEIGHT 1890
#define CAP1_WEIGHT 629
#define BOT2_WEIGHT 1685
#define CAP2_WEIGHT 728
#define BOT3_WEIGHT 1561
#define CAP3_WEIGHT 426

/**
 * Calculate expected weight delta for a given state combination
 * 
 * @param a State of bottle 1 (0, 1, or 2)
 * @param b State of bottle 2 (0, 1, or 2)
 * @param c State of bottle 3 (0, 1, or 2)
 * @param bot1..cap3 Weight values for each component
 * @return Expected weight delta from tare (negative = items removed)
 */
static int calculate_weight_target(int a, int b, int c,
                                   int bot1, int cap1,
                                   int bot2, int cap2,
                                   int bot3, int cap3) {
    int weightTarget = 0;
    
    /* Bottle 1 */
    if (a == 1) {
        weightTarget -= cap1;
    } else if (a == 2) {
        weightTarget -= (bot1 + cap1);
    }
    
    /* Bottle 2 */
    if (b == 1) {
        weightTarget -= cap2;
    } else if (b == 2) {
        weightTarget -= (bot2 + cap2);
    }
    
    /* Bottle 3 */
    if (c == 1) {
        weightTarget -= cap3;
    } else if (c == 2) {
        weightTarget -= (bot3 + cap3);
    }
    
    return weightTarget;
}

/**
 * Find the best matching state for a given weight measurement
 * 
 * @param weight Measured weight delta
 * @param out_a, out_b, out_c Output state for each bottle
 * @param out_found Number of states within threshold
 * @return 1 if unique match found, 0 otherwise
 */
static int find_matching_state(int weight,
                               int *out_a, int *out_b, int *out_c,
                               int *out_found,
                               int bot1, int cap1,
                               int bot2, int cap2,
                               int bot3, int cap3) {
    int i;
    int found = 0;
    int bestDistance = 9999;
    int bestA = 0, bestB = 0, bestC = 0;
    
    for (i = 0; i < 27; i++) {
        int a = i % 3;
        int b = (i / 3) % 3;
        int c = (i / 9) % 3;
        
        int weightTarget = calculate_weight_target(a, b, c,
                                                   bot1, cap1,
                                                   bot2, cap2,
                                                   bot3, cap3);
        
        int distance = abs(weightTarget - weight);
        
        if (distance < STABLE_THRESH) {
            found++;
            if (distance < bestDistance) {
                bestA = a;
                bestB = b;
                bestC = c;
                bestDistance = distance;
            }
        }
    }
    
    if (out_a) *out_a = bestA;
    if (out_b) *out_b = bestB;
    if (out_c) *out_c = bestC;
    if (out_found) *out_found = found;
    
    return (found == 1) ? 1 : 0;
}

/**
 * Encode state triple to single index (0-26)
 */
static int encode_state(int a, int b, int c) {
    return a + (b * 3) + (c * 9);
}

/* ==================== Test Cases ==================== */

/* Test weight calculations */

void test_all_bottles_present_weight() {
    /* State 0,0,0: all bottles and caps present */
    int weight = calculate_weight_target(0, 0, 0,
                                         BOT1_WEIGHT, CAP1_WEIGHT,
                                         BOT2_WEIGHT, CAP2_WEIGHT,
                                         BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(0, weight);
}

void test_cap1_removed_weight() {
    /* State 1,0,0: cap1 removed only */
    int weight = calculate_weight_target(1, 0, 0,
                                         BOT1_WEIGHT, CAP1_WEIGHT,
                                         BOT2_WEIGHT, CAP2_WEIGHT,
                                         BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(-CAP1_WEIGHT, weight);
}

void test_bottle1_removed_weight() {
    /* State 2,0,0: bottle1 and cap1 removed */
    int weight = calculate_weight_target(2, 0, 0,
                                         BOT1_WEIGHT, CAP1_WEIGHT,
                                         BOT2_WEIGHT, CAP2_WEIGHT,
                                         BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(-(BOT1_WEIGHT + CAP1_WEIGHT), weight);
}

void test_all_caps_removed_weight() {
    /* State 1,1,1: all caps removed */
    int weight = calculate_weight_target(1, 1, 1,
                                         BOT1_WEIGHT, CAP1_WEIGHT,
                                         BOT2_WEIGHT, CAP2_WEIGHT,
                                         BOT3_WEIGHT, CAP3_WEIGHT);
    int expected = -(CAP1_WEIGHT + CAP2_WEIGHT + CAP3_WEIGHT);
    ASSERT_EQUAL(expected, weight);
}

void test_all_bottles_removed_weight() {
    /* State 2,2,2: all bottles and caps removed */
    int weight = calculate_weight_target(2, 2, 2,
                                         BOT1_WEIGHT, CAP1_WEIGHT,
                                         BOT2_WEIGHT, CAP2_WEIGHT,
                                         BOT3_WEIGHT, CAP3_WEIGHT);
    int expected = -(BOT1_WEIGHT + CAP1_WEIGHT + 
                     BOT2_WEIGHT + CAP2_WEIGHT + 
                     BOT3_WEIGHT + CAP3_WEIGHT);
    ASSERT_EQUAL(expected, weight);
}

void test_mixed_state_weight() {
    /* State 0,1,2: bottle1 present, cap2 removed, bottle3 removed */
    int weight = calculate_weight_target(0, 1, 2,
                                         BOT1_WEIGHT, CAP1_WEIGHT,
                                         BOT2_WEIGHT, CAP2_WEIGHT,
                                         BOT3_WEIGHT, CAP3_WEIGHT);
    int expected = -CAP2_WEIGHT - (BOT3_WEIGHT + CAP3_WEIGHT);
    ASSERT_EQUAL(expected, weight);
}

/* Test state matching */

void test_exact_match_all_present() {
    int a, b, c, found;
    int result = find_matching_state(0, &a, &b, &c, &found,
                                     BOT1_WEIGHT, CAP1_WEIGHT,
                                     BOT2_WEIGHT, CAP2_WEIGHT,
                                     BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(1, result);  /* Unique match */
    ASSERT_EQUAL(0, a);
    ASSERT_EQUAL(0, b);
    ASSERT_EQUAL(0, c);
}

void test_exact_match_cap1_removed() {
    int a, b, c, found;
    int result = find_matching_state(-CAP1_WEIGHT, &a, &b, &c, &found,
                                     BOT1_WEIGHT, CAP1_WEIGHT,
                                     BOT2_WEIGHT, CAP2_WEIGHT,
                                     BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(1, result);  /* Unique match */
    ASSERT_EQUAL(1, a);
    ASSERT_EQUAL(0, b);
    ASSERT_EQUAL(0, c);
}

void test_within_threshold_match() {
    /* Weight slightly off from exact cap1 removed */
    int weight = -CAP1_WEIGHT + (STABLE_THRESH - 10);
    int a, b, c, found;
    int result = find_matching_state(weight, &a, &b, &c, &found,
                                     BOT1_WEIGHT, CAP1_WEIGHT,
                                     BOT2_WEIGHT, CAP2_WEIGHT,
                                     BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(1, result);  /* Should still match uniquely */
    ASSERT_EQUAL(1, a);
    ASSERT_EQUAL(0, b);
    ASSERT_EQUAL(0, c);
}

void test_no_match_outside_threshold() {
    /* Weight that doesn't match any state */
    int weight = -10000;  /* Arbitrary value far from any valid state */
    int a, b, c, found;
    int result = find_matching_state(weight, &a, &b, &c, &found,
                                     BOT1_WEIGHT, CAP1_WEIGHT,
                                     BOT2_WEIGHT, CAP2_WEIGHT,
                                     BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_EQUAL(0, result);  /* No unique match */
    ASSERT_EQUAL(0, found);
}

void test_27_unique_states_exist() {
    /* Verify all 27 states can be enumerated */
    int count = 0;
    for (int i = 0; i < 27; i++) {
        int a = i % 3;
        int b = (i / 3) % 3;
        int c = (i / 9) % 3;
        
        /* Each component should be 0, 1, or 2 */
        if (a >= 0 && a <= 2 && b >= 0 && b <= 2 && c >= 0 && c <= 2) {
            count++;
        }
    }
    ASSERT_EQUAL(27, count);
}

void test_encode_state_boundary() {
    /* Test encoding at boundaries */
    ASSERT_EQUAL(0, encode_state(0, 0, 0));
    ASSERT_EQUAL(26, encode_state(2, 2, 2));
    ASSERT_EQUAL(13, encode_state(1, 1, 1));
}

void test_weight_monotonicity() {
    /* Verify that removing more weight gives more negative values */
    int w_000 = calculate_weight_target(0, 0, 0,
                                        BOT1_WEIGHT, CAP1_WEIGHT,
                                        BOT2_WEIGHT, CAP2_WEIGHT,
                                        BOT3_WEIGHT, CAP3_WEIGHT);
    int w_100 = calculate_weight_target(1, 0, 0,
                                        BOT1_WEIGHT, CAP1_WEIGHT,
                                        BOT2_WEIGHT, CAP2_WEIGHT,
                                        BOT3_WEIGHT, CAP3_WEIGHT);
    int w_200 = calculate_weight_target(2, 0, 0,
                                        BOT1_WEIGHT, CAP1_WEIGHT,
                                        BOT2_WEIGHT, CAP2_WEIGHT,
                                        BOT3_WEIGHT, CAP3_WEIGHT);
    
    /* More items removed = more negative weight */
    ASSERT_TRUE(w_000 > w_100);
    ASSERT_TRUE(w_100 > w_200);
}

/* Test threshold behavior */

void test_stable_thresh_value() {
    /* Verify threshold constant is set correctly */
    ASSERT_EQUAL(43, STABLE_THRESH);
}

void test_threshold_boundary_inclusive() {
    /* Test at exactly threshold-1 distance (should match) */
    int target = -CAP1_WEIGHT;
    int weight = target + (STABLE_THRESH - 1);
    int a, b, c, found;
    find_matching_state(weight, &a, &b, &c, &found,
                        BOT1_WEIGHT, CAP1_WEIGHT,
                        BOT2_WEIGHT, CAP2_WEIGHT,
                        BOT3_WEIGHT, CAP3_WEIGHT);
    ASSERT_TRUE(found >= 1);  /* Should find at least one match */
}

void test_threshold_boundary_exclusive() {
    /* Test at exactly threshold distance - behavior depends on < vs <= */
    /* The code uses distance < STABLE_THRESH, so at exactly threshold it won't match */
    int target = 0;  /* All present */
    int weight = target + STABLE_THRESH;  /* Exactly at threshold */
    int a, b, c, found;
    find_matching_state(weight, &a, &b, &c, &found,
                        BOT1_WEIGHT, CAP1_WEIGHT,
                        BOT2_WEIGHT, CAP2_WEIGHT,
                        BOT3_WEIGHT, CAP3_WEIGHT);
    /* At exactly threshold, should NOT match (< not <=) */
    ASSERT_EQUAL(0, found);
}

/* ==================== Main ==================== */

int main(void) {
    TEST_SUITE_START("Bottle State Matching Tests");
    
    printf("\n-- Weight Calculation --\n");
    RUN_TEST(test_all_bottles_present_weight);
    RUN_TEST(test_cap1_removed_weight);
    RUN_TEST(test_bottle1_removed_weight);
    RUN_TEST(test_all_caps_removed_weight);
    RUN_TEST(test_all_bottles_removed_weight);
    RUN_TEST(test_mixed_state_weight);
    
    printf("\n-- State Matching --\n");
    RUN_TEST(test_exact_match_all_present);
    RUN_TEST(test_exact_match_cap1_removed);
    RUN_TEST(test_within_threshold_match);
    RUN_TEST(test_no_match_outside_threshold);
    RUN_TEST(test_27_unique_states_exist);
    RUN_TEST(test_encode_state_boundary);
    RUN_TEST(test_weight_monotonicity);
    
    printf("\n-- Threshold Behavior --\n");
    RUN_TEST(test_stable_thresh_value);
    RUN_TEST(test_threshold_boundary_inclusive);
    RUN_TEST(test_threshold_boundary_exclusive);
    
    TEST_SUITE_END();
    PRINT_TEST_SUMMARY();
    
    return TEST_EXIT_CODE();
}
