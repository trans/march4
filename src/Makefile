# March C Compiler Build System

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -O0
LDFLAGS = -lsqlite3 -lcrypto

# Source files
CORE_SRCS = cells.c tokens.c dictionary.c database.c primitives.c compiler.c loader.c runner.c
CORE_OBJS = $(CORE_SRCS:.c=.o)

# Test files
TEST_SRCS = test_cells.c test_dict.c test_database.c test_primitives.c test_compiler.c test_loader.c
TEST_BINS = $(TEST_SRCS:.c=)

# VM library
VM_LIB = ../build/libmarch_vm.a

.PHONY: all clean test

all: $(CORE_OBJS)

# Core objects
%.o: %.c %.h types.h
	$(CC) $(CFLAGS) -c $< -o $@

# Test binaries
test_cells: test_cells.c cells.o
	$(CC) $(CFLAGS) $^ -o $@

test_dict: test_dict.c dictionary.o
	$(CC) $(CFLAGS) $^ -o $@

test_database: test_database.c database.o cells.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test_primitives: test_primitives.c primitives.o dictionary.o
	$(CC) $(CFLAGS) $^ -o $@

test_compiler: test_compiler.c compiler.o primitives.o dictionary.o database.o cells.o tokens.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test_loader: test_loader.c loader.o runner.o compiler.o primitives.o dictionary.o database.o cells.o tokens.o $(VM_LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

# Run all tests
test: test_cells test_dict test_database test_primitives test_compiler test_loader
	@echo "\n=== Running Cell Tests ==="
	@./test_cells
	@echo "\n=== Running Dictionary Tests ==="
	@./test_dict
	@echo "\n=== Running Database Tests ==="
	@./test_database
	@echo "\n=== Running Primitives Tests ==="
	@./test_primitives
	@echo "\n=== Running Compiler Tests ==="
	@./test_compiler
	@echo "\n=== Running Loader/Runner Tests ==="
	@./test_loader
	@echo "\nAll tests complete!"

clean:
	rm -f *.o $(TEST_BINS)
