# admhost32 -- 32-bit host for After Dark 4 modules.
#
# MUST build 32-bit x86. A 64-bit process cannot load these modules.
#
#   make                 cross-compile with MinGW (i686-w64-mingw32-gcc)
#   make test            build and run the ABI layout check
#   nmake -f Makefile.msvc   (or just: cl /O2 admhost32.c user32.lib gdi32.lib)

CC      ?= i686-w64-mingw32-gcc
CFLAGS  ?= -O2 -Wall -Wextra
LDLIBS  ?= -lgdi32 -luser32
TARGET  ?= admhost32.exe

$(TARGET): admhost32.c ../include/ad_module32.h
	$(CC) $(CFLAGS) -o $@ admhost32.c $(LDLIBS)

.PHONY: test clean
test:
	$(CC:i686-w64-mingw32-gcc=cc) -Wall -Wextra -o /tmp/ad_abitest ../tests/test_ad_module32_layout.c
	/tmp/ad_abitest

clean:
	rm -f $(TARGET)
