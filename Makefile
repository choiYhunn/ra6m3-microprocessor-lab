CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
BUILD := build

TESTS := $(BUILD)/test_tcu $(BUILD)/test_elevator

.PHONY: test clean

test: $(TESTS)
	./$(BUILD)/test_tcu
	./$(BUILD)/test_elevator

$(BUILD)/test_tcu: src/tcu.c tests/test_tcu.c include/tcu.h | $(BUILD)
	$(CC) $(CFLAGS) -Iinclude src/tcu.c tests/test_tcu.c -o $@

$(BUILD)/test_elevator: src/elevator.c tests/test_elevator.c include/elevator.h | $(BUILD)
	$(CC) $(CFLAGS) -Iinclude src/elevator.c tests/test_elevator.c -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
