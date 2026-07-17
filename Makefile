###############################################################################
# tmbridge
###############################################################################

TARGET := tmbridge

CC := gcc

BUILD ?= debug

SRC := $(wildcard src/*.c)
OBJ := $(patsubst %.c,%.o,$(SRC))

CURL_CFLAGS := $(shell pkg-config --cflags libcurl)
CURL_LIBS   := $(shell pkg-config --libs libcurl)

WARNINGS := \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Wshadow \
    -Wconversion \
    -Wstrict-prototypes \
    -Wmissing-prototypes \
    -Wwrite-strings \
    -Wundef

COMMON_CFLAGS := \
    -std=c17 \
    $(WARNINGS) \
    -Isrc \
    -Iinclude \
    $(CURL_CFLAGS)

COMMON_LDFLAGS := \
    -pthread \
    $(CURL_LIBS)

ifeq ($(BUILD),release)

CFLAGS  := $(COMMON_CFLAGS) -O2 -DNDEBUG
LDFLAGS := $(COMMON_LDFLAGS)

else

CFLAGS  := $(COMMON_CFLAGS) \
            -g3 \
            -O0 \
            -fsanitize=address \
            -fsanitize=undefined

LDFLAGS := $(COMMON_LDFLAGS) \
            -fsanitize=address \
            -fsanitize=undefined

endif

###############################################################################

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean all

run: all
	./$(TARGET)

###############################################################################

.PHONY: all clean rebuild run
