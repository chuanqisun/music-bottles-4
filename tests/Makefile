# Test Makefile for Music Bottles unit tests

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I.

BIN_DIR = bin

# Test executables
TEST_GPIO_BASE = $(BIN_DIR)/test_gpio_base
TEST_BOTTLE_STATE = $(BIN_DIR)/test_bottle_state

# All test targets
ALL_TESTS = $(TEST_GPIO_BASE) $(TEST_BOTTLE_STATE)

.PHONY: all test test-gpio test-bottle clean-tests create-test-dirs

# Create test binary directory
create-test-dirs:
	@mkdir -p $(BIN_DIR)

# Build and run all tests
test: create-test-dirs $(ALL_TESTS)
	@echo "Running all tests..."
	@echo ""
	@$(TEST_GPIO_BASE)
	@echo ""
	@$(TEST_BOTTLE_STATE)
	@echo ""
	@echo "All tests completed."

# Build individual test executables
$(TEST_GPIO_BASE): test_gpio_base.c test_framework.h
	$(CC) $(CFLAGS) -o $@ test_gpio_base.c

$(TEST_BOTTLE_STATE): test_bottle_state.c test_framework.h
	$(CC) $(CFLAGS) -o $@ test_bottle_state.c

# Individual test targets
test-gpio: create-test-dirs $(TEST_GPIO_BASE)
	@$(TEST_GPIO_BASE)

test-bottle: create-test-dirs $(TEST_BOTTLE_STATE)
	@$(TEST_BOTTLE_STATE)

# Clean test artifacts
clean-tests:
	rm -rf $(BIN_DIR)
