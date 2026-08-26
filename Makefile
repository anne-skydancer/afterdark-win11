# After Dark Studio -- full build.
#
#   make dist      build everything into dist/ (what the installer packages)
#   make test      run the ABI layout check and the catalogue tests
#   make clean
#
# Cross-building from Linux needs mingw-w64 (both i686 and x86_64) plus the
# .NET 8 SDK. On Windows use MSVC for the two native pieces and `dotnet
# publish` for the shell; the outputs are identical in layout.

CC32    ?= i686-w64-mingw32-gcc
CC64    ?= x86_64-w64-mingw32-gcc
CFLAGS  ?= -O2 -Wall -Wextra
DIST    ?= dist

.PHONY: dist native shell test clean installer

dist: native shell
	@echo "dist/ ready:"
	@ls -1 $(DIST) | sed 's/^/  /'

native: $(DIST)
	$(CC32) $(CFLAGS) -o $(DIST)/admhost32.exe host/admhost32.c -lgdi32 -luser32
	$(CC64) $(CFLAGS) -o $(DIST)/AfterDarkModern.scr scr/afterdark_modern.c \
		-lgdi32 -luser32 -lshell32 -mwindows

# Self-contained so the installer needs no .NET runtime prerequisite.
shell: $(DIST)
	dotnet publish src/AfterDark.Studio -c Release -r win-x64 \
		--self-contained true -p:PublishSingleFile=false -o $(DIST)

$(DIST):
	mkdir -p $(DIST)

test:
	cc -Wall -Wextra -o /tmp/ad_abitest tests/test_ad_module32_layout.c && /tmp/ad_abitest
	dotnet test src/AfterDark.Catalog.Tests --nologo -v q

# Requires Inno Setup 7 (iscc.exe) on PATH; Windows only.
installer: dist
	iscc installer/AfterDarkStudio.iss

clean:
	rm -rf $(DIST) src/*/bin src/*/obj
