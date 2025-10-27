CC             = gcc

CFLAGS         ?= -Wall -g -std=c23 -Wextra -Wformat-security -Wformat-overflow=2 -O2

INCLUDE        ?= -Iinclude

LDLIBS         =  -lm -ldl -lpthread

SRC            := $(wildcard src/*.c)
PROXY_SRC        := $(wildcard src/proxy/*.c)
CLI_SRC        := $(wildcard src/cli/*.c)
ALL_SRC        := $(SRC) $(PROXY_SRC) $(CLI_SRC)

OBJ            := $(ALL_SRC:src/%.c=build/%.o)

BIN            := bin/sm3tproxy


all: $(BIN)
	@echo 'Build complete (dev).'

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

.PHONY:  test all clean install uninstall

clean:
	@rm -rf build $(BIN)
	@echo "Clean up complete..."

