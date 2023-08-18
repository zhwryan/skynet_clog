SKYNET_PATH ?= ../skynet/
LUA_INC := $(SKYNET_PATH)3rd/lua
SKYNET_INC := $(SKYNET_PATH)skynet-src

CC := gcc
CFLAGS := -std=gnu99 -O3 -Wall -pedantic -DNDEBUG -fPIC

TARGET := clog.so
SRC_FILES := clog.c
OBJ_FILES := $(SRC_FILES:.c=.o)

.PHONY: all linux macosx clean

all:
	@echo "missing <platform> [linux/macosx]"

.c.o:
	$(CC) -c $(CFLAGS) -I$(LUA_INC) -I$(SKYNET_INC) -o $@ $<

linux: LDFLAGS := -shared
linux: $(TARGET)

macosx: LDFLAGS := -bundle -undefined dynamic_lookup
macosx: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(LDFLAGS) -o $@ $^
	@echo "Build $@ success!"

clean:
	rm -f $(OBJ_FILES) $(TARGET)
