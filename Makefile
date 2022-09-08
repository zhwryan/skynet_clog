SKYNET_PATH ?= ../skynet/
LUA_INC ?= $(SKYNET_PATH)3rd/lua
SKYNET_INC ?= $(SKYNET_PATH)skynet-src

TARGET_NAME = clog
TARGET = ${TARGET_NAME}.so
OBJS = ${TARGET_NAME}.o

LNX_LDFLAGS = -shared
MAC_LDFLAGS = -bundle -undefined dynamic_lookup

CC = gcc
LDFLAGS = $(MYLDFLAGS)

CFLAGS = -std=gnu99 -O3 -Wall -pedantic -DNDEBUG -fPIC
INC_FLAGS = -I$(LUA_INC) -I$(SKYNET_INC)

all:
	@echo "Usage: $(MAKE) <platform>"
	@echo "  * linux"
	@echo "  * macosx"

.c.o:
	$(CC) -c $(CFLAGS) $(INC_FLAGS) -o $@ $<

linux:
	@$(MAKE) $(TARGET) MYLDFLAGS="$(LNX_LDFLAGS)"

macosx:
	@$(MAKE) $(TARGET) MYLDFLAGS="$(MAC_LDFLAGS)"

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f *.o *.so

