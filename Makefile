# Hooray for Captain Spaulding - a paper-doll screensaver of Captain Spaulding
VERSION  := 1.1.0
PREFIX   ?= /usr
SYSCONFDIR ?= /etc
DESTDIR  ?=
CC       ?= cc
# gnu11 keeps the binary free of very recent glibc symbols, so packages built on a
# new distribution still run on older ones
CFLAGS   ?= -O2 -g
override CFLAGS += -std=gnu11 -Wall -Isrc
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)

MINGW    ?= x86_64-w64-mingw32
MINGW_ROOT ?= /usr/$(MINGW)/sys-root/mingw

all: build/captain-spaulding

# the pictures in assets/art are built into the program, so it must be compiled from here
build/captain-spaulding: src/spaulding.c src/stb_image.h src/stb_vorbis.c src/doll.h src/song.h $(wildcard assets/art/*)
	@mkdir -p build
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDL_CFLAGS) -o $@ src/spaulding.c $(LDFLAGS) $(SDL_LIBS) -lm

check: build/captain-spaulding
	SDL_VIDEODRIVER=dummy ./build/captain-spaulding --snapshot build/check.bmp --time 120 --mute --size 640x400 && echo "ran a full round"

install: all
	install -Dm755 build/captain-spaulding $(DESTDIR)$(PREFIX)/bin/captain-spaulding
	install -Dm755 packaging/linux/captain-spaulding-screensaver $(DESTDIR)$(PREFIX)/bin/captain-spaulding-screensaver
	install -Dm644 packaging/linux/captain-spaulding.desktop $(DESTDIR)$(PREFIX)/share/applications/captain-spaulding.desktop
	install -Dm644 packaging/linux/captain-spaulding-saver.desktop $(DESTDIR)$(PREFIX)/share/screensaver-service/savers/captain-spaulding.desktop
	install -Dm644 packaging/linux/captain-spaulding-autostart.desktop $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/captain-spaulding-screensaver.desktop
	install -Dm644 packaging/captain-spaulding.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/captain-spaulding.png
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/captain-spaulding/README.md
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/doc/captain-spaulding/LICENSE

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/captain-spaulding $(DESTDIR)$(PREFIX)/bin/captain-spaulding-screensaver
	rm -f $(DESTDIR)$(PREFIX)/share/applications/captain-spaulding.desktop $(DESTDIR)$(SYSCONFDIR)/xdg/autostart/captain-spaulding-screensaver.desktop
	rm -f $(DESTDIR)$(PREFIX)/share/screensaver-service/savers/captain-spaulding.desktop
	rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/captain-spaulding.png
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/captain-spaulding

# ---- packages (written to dist/) ----
ARCH_DEB := $(shell dpkg --print-architecture 2>/dev/null || echo amd64)

deb: all
	rm -rf build/deb && mkdir -p build/deb/DEBIAN dist
	$(MAKE) install DESTDIR=build/deb PREFIX=/usr
	strip build/deb/usr/bin/captain-spaulding
	sed -e 's/@VERSION@/$(VERSION)/' -e 's/@ARCH@/$(ARCH_DEB)/' \
	    -e "s/@SIZE@/$$(du -sk build/deb/usr | cut -f1)/" packaging/linux/control.in > build/deb/DEBIAN/control
	echo /etc/xdg/autostart/captain-spaulding-screensaver.desktop > build/deb/DEBIAN/conffiles
	dpkg-deb --root-owner-group --build build/deb dist/captain-spaulding_$(VERSION)_$(ARCH_DEB).deb

rpm:
	mkdir -p dist build/rpm/SOURCES
	git archive --format=tar.gz --prefix=captain-spaulding-$(VERSION)/ -o build/rpm/SOURCES/captain-spaulding-$(VERSION).tar.gz HEAD
	rpmbuild -bb $(RPMFLAGS) --define "_topdir $(CURDIR)/build/rpm" --define "version $(VERSION)" packaging/linux/captain-spaulding.spec
	cp build/rpm/RPMS/*/captain-spaulding-$(VERSION)-*.rpm dist/

# Windows: the screensaver itself, and a small CaptainSpaulding.scr that goes in the
# Windows system folder (so it is listed with the other screensavers) and starts it
windows:
	@mkdir -p build/win
	$(MINGW)-gcc -O2 -std=gnu11 -Wall -Isrc -I$(MINGW_ROOT)/include/SDL2 -Dmain=SDL_main \
	    -o build/win/captain-spaulding.exe src/spaulding.c \
	    -L$(MINGW_ROOT)/lib -lmingw32 -lSDL2main -lSDL2 -lm -mwindows -static-libgcc
	$(MINGW)-windres packaging/windows/stub.rc -O coff -o build/win/stub.res
	$(MINGW)-gcc -O2 -Wall -o build/win/CaptainSpaulding.scr packaging/windows/stub.c build/win/stub.res -mwindows -static
	$(MINGW)-strip build/win/captain-spaulding.exe build/win/CaptainSpaulding.scr
	cp $(MINGW_ROOT)/bin/SDL2.dll build/win/
	[ ! -f $(MINGW_ROOT)/bin/SDL3.dll ] || cp $(MINGW_ROOT)/bin/SDL3.dll build/win/

installer: windows
	mkdir -p dist
	makensis -V2 -DVERSION=$(VERSION) packaging/windows/captain-spaulding.nsi

dist: deb rpm installer

clean:
	rm -rf build dist

.PHONY: all check install uninstall deb rpm windows installer dist clean
