# pulltab: tunnel arbitrary streams through HTTP proxies.
# Copyright (C) 2014 Cyphar

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# 1. The above copyright notice and this permission notice shall be included in
#    all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN DESTECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

.PHONY: all build binary debug clean

CC ?= gcc
BINARY = pulltab

BUILD_DIR = bin
INCLUDE_DIR = include
SRC_DIR = src
SRC = $(wildcard $(SRC_DIR)/*.c)

WARNINGS = -Wall -Wextra# -pedantic
CFLAGS = -ansi -I$(INCLUDE_DIR)/

all: clean binary

clean:
	rm -rf $(BUILD_DIR)

build: clean
	mkdir -p $(BUILD_DIR)

binary: build
	$(CC) $(SRC) $(CFLAGS) $(LFLAGS) -o $(BUILD_DIR)/$(BINARY) $(WARNINGS)

debug: build
	$(CC) $(SRC) $(CFLAGS) $(LFLAGS) -O0 -ggdb -o $(BUILD_DIR)/$(BINARY)-dbg $(WARNINGS)
