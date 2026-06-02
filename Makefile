CC             = gcc

CFLAGS         := -Wall -g -std=c23 -Wextra -Wformat-security -Wformat-overflow=2 
# CFLAGS		   += -O3 -s # strip bin
# Debugging params 
CFLAGS		   += -ggdb
# CFLAGS 		   += -fsanitize=address
# CFLAGS 		   += -fno-omit-frame-pointer
# CFLAGS		   += -fsanitize-recover=address


INCLUDE        := -Iinclude -Ilib

LDLIBS         =  -lm -ldl -lpthread

SRC            := $(wildcard src/*.c)
PROXY_SRC      := $(wildcard src/proxy/*.c)
CORE_SRC       := $(wildcard src/core/*.c)
CONF_SRC       := $(wildcard src/conf/*.c)
ALL_SRC        := $(SRC) $(PROXY_SRC) $(CORE_SRC)

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

run-no-debug:
	-@$(SET_GROUP) $(SM3T_GROUP) "$(BIN)"

run:
	-@$(SET_GROUP) $(SM3T_GROUP) "$(BIN) --debug"
	
.PHONY:  test all clean install uninstall

clean:
	@echo "[+] Cleaning complete"
	@rm -rf build $(BIN_DIR)

