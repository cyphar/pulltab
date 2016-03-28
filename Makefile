# pulltab: tunnel arbitrary streams through HTTP proxies.
# Copyright (C) 2014 Aleksa Sarai
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

.PHONY: all build binary debug clean

CC ?= gcc
STRIP ?= strip
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
	$(STRIP) $(BUILD_DIR)/$(BINARY)

debug: build
	$(CC) $(SRC) $(CFLAGS) -DDEBUG $(LFLAGS) -O0 -ggdb -o $(BUILD_DIR)/$(BINARY) $(WARNINGS)
