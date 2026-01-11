CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I include $(shell pkg-config --cflags wayland-server)
LDFLAGS = $(shell pkg-config --libs wayland-server)

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

clean:
	rm -rf $(OBJ_DIR) $(LIB_DIR) examples/simple_wm
