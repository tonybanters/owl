CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I include -I src $(shell pkg-config --cflags wayland-server libdrm gbm egl libinput libudev xkbcommon)
LDFLAGS = $(shell pkg-config --libs wayland-server libdrm gbm egl glesv2 libinput libudev xkbcommon)

SRC_DIR = src
OBJ_DIR = build
LIB_DIR = lib

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

LIBRARY = $(LIB_DIR)/libowl.a

.PHONY: all clean examples

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS) | $(LIB_DIR)
	ar rcs $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

examples: $(LIBRARY)
	$(CC) $(CFLAGS) examples/simple_wm.c -L$(LIB_DIR) -lowl $(LDFLAGS) -o examples/simple_wm

run: examples
	./examples/simple_wm

runlog: examples
	./examples/simple_wm > /tmp/owl.log 2>&1; cat /tmp/owl.log

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) examples/simple_wm
