# Copyright © 2023 Samuel Kunst <samuel at kunst.me>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.

.POSIX:

PREFIX   ?= /usr/local
BINDIR   = $(PREFIX)/bin
MANDIR   = $(PREFIX)/share/man

SYSTEMD        ?=
SYSTEMD_PREFIX ?= /etc/systemd

OUT = gulp
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

CC = gcc

VERSION_CMD ?= git describe --tags --dirty --abbrev=7 2> /dev/null
VERSION     ?= $(shell $(VERSION_CMD) || cat ./version.txt)

MANDOC      = gulp.1.md

CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Werror
CFLAGS  += -DVERSION=\"$(VERSION)\" -D_POSIX_C_SOURCE=200809L

LDLIBS   = -lX11 -lprocps -lxcb -lXRes -ltoml

all: $(OUT)

$(OUT): $(OBJ)

$(OBJ): Makefile

install:
	install -Dm755 $(OUT) $(BINDIR)/$(OUT)
ifneq ( ,$(shell command -V pandoc 2>/dev/null))
	$(MAKE) manpage
	mkdir -p $(MANDIR)/man1
	gzip -c $(OUT).1 > $(MANDIR)/man1/$(OUT).1.gz
	chmod 644 $(MANDIR)/man1/$(OUT).1.gz
endif
ifdef SYSTEMD
	mkdir -p $(SYSTEMD_PREFIX)/user
	sed "s,/usr/local/bin/gulp,$(BINDIR)/$(OUT),g" ./systemd/gulp.service > $(SYSTEMD_PREFIX)/user/$(OUT).service
	chmod 644 $(SYSTEMD_PREFIX)/user/$(OUT).service
endif

manpage: $(MANDOC)
	pandoc --standalone $(MANDOC) --to man | sed "s/VERSION/$(VERSION)/g" > $(OUT).1

uninstall:
	rm -f $(BINDIR)/$(OUT)
	rm -f $(MANDIR)/$(OUT).1
	rm -f $(SYSTEMD_PREFIX)/user/$(OUT).service

clean:
	rm -rf $(OUT) $(OBJ) $(OUT).1 $(OUT).1.gz

format:
	clang-format -i $(shell find . -name '*.[ch]')

fmt: format

.PHONY: all install uninstall clean format fmt
