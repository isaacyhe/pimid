# Makefile for BFS with PIM Device-Side Offloading

# Compiler
MPICC = mpicc

# Compiler flags
CFLAGS = -O3 -Wall -std=c99

# Target executable
TARGET = bfs_pim

# Source files
SRCS = bfs_pim.c

# Object files
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJS)
	$(MPICC) $(CFLAGS) -o $(TARGET) $(OBJS) -lm

# Compile source files to object files
%.o: %.c
	$(MPICC) $(CFLAGS) -c $< -o $@

# Run the program (4 processes, 1000 nodes)
run: $(TARGET)
	mpirun -np 4 ./$(TARGET) 1000

# Run with larger graph (4 processes, 10000 nodes)
run-large: $(TARGET)
	mpirun -np 4 ./$(TARGET) 10000

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all run run-large clean
