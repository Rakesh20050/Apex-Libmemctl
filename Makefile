CC       = gcc
AR       ?= ar
NM       ?= nm

CPPFLAGS += -D_GNU_SOURCE -Iinclude
CFLAGS   ?= -O2 -g
WARNINGS  = -std=c11 -Wall -Wextra -Wpedantic -Werror -Wshadow \
            -Wstrict-prototypes -Wmissing-prototypes
NO_ALLOC_BUILTINS = -fno-builtin-malloc -fno-builtin-free \
                    -fno-builtin-calloc -fno-builtin-realloc
LDLIBS   += -pthread

BUILD_DIR = build
LIB       = $(BUILD_DIR)/libmemctl.a
ALLOC_OBJ = $(BUILD_DIR)/allocator.o
TEST_BIN  = $(BUILD_DIR)/test_runner

.PHONY: all test check check-symbols clean

all: $(LIB) $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(ALLOC_OBJ): src/allocator.c include/libmemctl.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(NO_ALLOC_BUILTINS) -c $< -o $@

$(LIB): $(ALLOC_OBJ)
	$(AR) rcs $@ $^

$(TEST_BIN): tests/test_runner.c include/libmemctl.h $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $< $(LIB) $(LDLIBS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

check-symbols: $(ALLOC_OBJ)
	@bad="$$( $(NM) -u $(ALLOC_OBJ) | sed 's/.* U //' | sed 's/@.*//' | \
		grep -E '^(malloc|free|calloc|realloc|aligned_alloc|posix_memalign|valloc|pvalloc|sbrk)$$' || true )"; \
	if [ -n "$$bad" ]; then \
		echo "Forbidden allocator symbol(s) referenced by allocator.o:"; \
		echo "$$bad"; exit 1; \
	else \
		echo "Allocator symbol check passed."; \
	fi

check: check-symbols test

clean:
	rm -rf $(BUILD_DIR)
