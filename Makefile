# Futoshiki solver

CC       ?= cc
CSTD     := -std=c11
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow
OPT      := -O2
CFLAGS   := $(CSTD) $(OPT) $(WARNINGS)
LDFLAGS  := -lm

TARGET   := futoshiki
SRC      := futoshiki.c

# Debug build with sanitizers: `make debug`
DEBUG_CFLAGS := $(CSTD) -g -O0 $(WARNINGS) -fsanitize=address,undefined -DDEBUG_SEED

.PHONY: all debug format lint test clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

debug: $(SRC)
	$(CC) $(DEBUG_CFLAGS) -o $(TARGET)-debug $< $(LDFLAGS)

format:
	@command -v clang-format >/dev/null 2>&1 || { echo "clang-format not found."; \
		echo "macOS: xcrun clang-format -i $(SRC)  (or: brew install clang-format)"; exit 1; }
	clang-format -i $(SRC)

lint:
	@command -v clang-tidy >/dev/null 2>&1 || { echo "clang-tidy not found (brew install llvm). Skipping."; }
	@command -v cppcheck >/dev/null 2>&1 || { echo "cppcheck not found (brew install cppcheck). Skipping."; }
	@command -v clang-tidy >/dev/null 2>&1 && clang-tidy $(SRC) -- $(CFLAGS) || true
	@command -v cppcheck >/dev/null 2>&1 && cppcheck --enable=all --std=c11 --quiet $(SRC) || true

test: $(TARGET)
	./tests/run_tests.sh ./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET)-debug *.o
