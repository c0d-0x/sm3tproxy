CC             = gcc

CFLAGS         ?= -Wall -g -std=c23 -Wextra -Wformat-security -Wformat-overflow=2 -fsanitize=address -fno-omit-frame-pointer -g

INCLUDE        ?= -Iinclude -Ilib

LDLIBS         =  -lm -ldl -lpthread

SRC            := $(wildcard src/*.c)
PROXY_SRC        := $(wildcard src/proxy/*.c)
CMD_SRC        := $(wildcard src/cmd/*.c)
ALL_SRC        := $(SRC) $(PROXY_SRC) $(CMD_SRC)

OBJ            := $(ALL_SRC:src/%.c=build/%.o)

NAT			   := ./nat.sh
SETUP		   := setup
CLEANUP		   := cleanup
BIN            := bin/sm3tproxy
BIN_DIR		   := bin
SET_GROUP      := sg
SM3T_GROUP     := sm3tproxy


all: $(BIN)
	@echo '[+] Build complete (dev).'

$(BIN): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

setup-nat:
	-$(NAT) $(SETUP)

clean-nat: 
	-$(NAT) $(CLEANUP)

run:
	-@$(SET_GROUP) $(SM3T_GROUP) $(BIN)
	
.PHONY:  test all clean install uninstall

clean:
	@echo "[+] Cleaning complete"
	@rm -rf build $(BIN_DIR)

