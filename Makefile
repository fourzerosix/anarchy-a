CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu99 -D_POSIX_C_SOURCE=200809L
LDFLAGS ?= -lm
PREFIX  ?= /usr/local
BINDIR  := $(PREFIX)/bin
MANDIR  := $(PREFIX)/share/man/man6
BINARY  := anarchy-a
SRC     := anarchy_a/anarchy-a.c
VERSION := $(shell cat VERSION 2>/dev/null || echo "1.0.4")
TARBALL := anarchy-a-$(VERSION).tar.gz

.PHONY: all install uninstall clean dist check

all: $(BINARY)

$(BINARY): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: $(BINARY)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BINARY) $(DESTDIR)$(BINDIR)/$(BINARY)
	@echo "Installed anarchy-a to $(DESTDIR)$(BINDIR)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BINARY)

clean:
	rm -f $(BINARY) $(TARBALL)

dist: clean
	mkdir -p /tmp/anarchy-a-$(VERSION)
	cp -r . /tmp/anarchy-a-$(VERSION)
	rm -rf /tmp/anarchy-a-$(VERSION)/.git
	tar -czf $(TARBALL) -C /tmp anarchy-a-$(VERSION)
	rm -rf /tmp/anarchy-a-$(VERSION)
	@echo "Created $(TARBALL)"

check: $(BINARY)
	@echo "==> Fast mode test"
	./$(BINARY) -f --plain -s 2 > /dev/null && echo "PASS: fast+plain"
	@echo "==> Help test"
	./$(BINARY) --help > /dev/null && echo "PASS: --help"
	@echo "All checks passed."
