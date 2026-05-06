# Makefile — Mini-UnionFS (Member 1)
# Compiles all four source files into a single binary.

CC      = gcc
CFLAGS  = $(shell pkg-config --cflags fuse3) -Wall -Wextra -g -O2
LDFLAGS = $(shell pkg-config --libs fuse3)

TARGET  = mini_unionfs
SRCS    = src/unionfs.c src/ops_read.c src/ops_write.c src/ops_delete.c
OBJS    = $(SRCS:.c=.o)

# Default target — build the binary
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

src/%.o: src/%.c src/unionfs.h
	$(CC) $(CFLAGS) -c $< -o $@

# Convenience: mount the filesystem for manual testing
mount: $(TARGET)
	mkdir -p lower upper mnt
	./$(TARGET) lower upper mnt

# Unmount
unmount:
	fusermount -u mnt 2>/dev/null || umount mnt 2>/dev/null || true

# Run automated test suite
test: $(TARGET)
	bash tests/test_unionfs.sh

# Remove build artefacts and unmount
clean: unmount
	rm -f $(TARGET) $(OBJS)
	rm -rf lower upper mnt

.PHONY: all mount unmount test clean