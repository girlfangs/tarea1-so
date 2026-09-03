CC      := gcc
CFLAGS  := -Wall -Wextra -O2
TARGET  := shell

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(@D)
	$(CC) $(OBJ) -o $@

build/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
