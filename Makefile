# After Dark Studio -- full build.
#
#   make dist      build everything into dist/ (what the installer packages)
#   make rewrite   build the independent Classic rewrites
#   make test      run the ABI layout check and the catalogue tests
#   make clean
#
# Cross-building from Linux needs mingw-w64 (both i686 and x86_64) plus the
# .NET 10 SDK. On Windows use MSVC for the two native pieces and `dotnet
# publish` for the shell; the outputs are identical in layout.

CC32    ?= i686-w64-mingw32-gcc
CC64    ?= x86_64-w64-mingw32-gcc
WINDRES ?= i686-w64-mingw32-windres
PYTHON  ?= python3
CFLAGS  ?= -O2 -Wall -Wextra
DIST    ?= dist

.PHONY: dist prepare-dist native shell rewrite test clean installer

dist: prepare-dist native shell
	@echo "dist/ ready:"
	@ls -1 $(DIST) | sed 's/^/  /'

native: prepare-dist
	$(CC32) $(CFLAGS) -o $(DIST)/admhost32.exe host/admhost32.c -lgdi32 -luser32
	$(CC64) $(CFLAGS) -o $(DIST)/AfterDarkModern.scr scr/afterdark_modern.c \
		-lgdi32 -luser32 -lshell32 -ladvapi32 -mwindows

# Self-contained so the installer needs no .NET runtime prerequisite.
shell: prepare-dist
	dotnet publish src/AfterDark.Studio -c Release -r win-x64 \
		--self-contained true -p:PublishSingleFile=false -o $(DIST)

rewrite: prepare-dist
	$(PYTHON) -c "from pathlib import Path; [Path(p).mkdir(parents=True, exist_ok=True) for p in ('$(DIST)/rewrite-build', '$(DIST)/rewrites')]"
	$(PYTHON) rewrites/build_mandelbrot_resources.py $(DIST)/rewrite-build
	$(WINDRES) $(DIST)/rewrite-build/mandelbrot32.rc \
		-O coff -o $(DIST)/rewrite-build/mandelbrot32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/MANDEL32.AD rewrites/mandelbrot32.c \
		$(DIST)/rewrite-build/mandelbrot32-res.o -lgdi32
	$(PYTHON) rewrites/build_shapes_resources.py $(DIST)/rewrite-build
	$(WINDRES) $(DIST)/rewrite-build/shapes32.rc \
		-O coff -o $(DIST)/rewrite-build/shapes32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SHAPES32.AD rewrites/shapes32.c \
		$(DIST)/rewrite-build/shapes32-res.o -lgdi32 -lm

prepare-dist:
	$(PYTHON) -c "from pathlib import Path; Path('$(DIST)').mkdir(parents=True, exist_ok=True)"

test:
	cc -Wall -Wextra -o /tmp/ad_abitest tests/test_ad_module32_layout.c && /tmp/ad_abitest
	dotnet test src/AfterDark.Catalog.Tests --nologo -v q

# Requires Inno Setup 7 (iscc.exe) on PATH; Windows only.
installer: dist
	iscc installer/AfterDarkStudio.iss

clean:
	rm -rf $(DIST) src/*/bin src/*/obj
