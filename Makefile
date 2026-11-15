CPPFLAGS += -Iinclude
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror

SRC := $(wildcard src/*.c)
HEADERS := $(wildcard include/*.h)

.PHONY: all clean

all: minihttpd

minihttpd: $(SRC) $(HEADERS) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	$(RM) minihttpd
