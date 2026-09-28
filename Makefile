CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=gnu11
TARGET  ?= shell

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

ifeq ($(DEBUG), 1)
        CFLAGS += -g
else
        CFLAGS += -O2
endif

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
