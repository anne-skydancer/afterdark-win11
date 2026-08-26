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

dist: prepare-dist native shell rewrite
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
	$(PYTHON) rewrites/build_procedural_resources.py $(DIST)/rewrite-build
	$(WINDRES) $(DIST)/rewrite-build/spiral32.rc \
		-O coff -o $(DIST)/rewrite-build/spiral32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SPIRAL32.AD rewrites/spiral32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/spiral32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/tunnel32.rc \
		-O coff -o $(DIST)/rewrite-build/tunnel32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/TUNNEL32.AD rewrites/tunnel32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/tunnel32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/zot32.rc \
		-O coff -o $(DIST)/rewrite-build/zot32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/ZOT32.AD rewrites/zot32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/zot32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/warp32.rc \
		-O coff -o $(DIST)/rewrite-build/warp32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/WARP32.AD rewrites/warp32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/warp32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/spheres32.rc \
		-O coff -o $(DIST)/rewrite-build/spheres32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SPHERES32.AD rewrites/spheres32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/spheres32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/stained32.rc \
		-O coff -o $(DIST)/rewrite-build/stained32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/STAINED32.AD rewrites/stained32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/stained32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/string32.rc \
		-O coff -o $(DIST)/rewrite-build/string32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/STRING32.AD rewrites/string32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/string32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/photon32.rc \
		-O coff -o $(DIST)/rewrite-build/photon32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/PHOTON32.AD rewrites/photon32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/photon32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/strange32.rc \
		-O coff -o $(DIST)/rewrite-build/strange32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/STRANGE32.AD rewrites/strange32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/strange32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/rain32.rc \
		-O coff -o $(DIST)/rewrite-build/rain32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/RAIN32.AD rewrites/rain32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/rain32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/spot32.rc \
		-O coff -o $(DIST)/rewrite-build/spot32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SPOT32.AD rewrites/spot32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/spot32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/draino32.rc \
		-O coff -o $(DIST)/rewrite-build/draino32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/DRAINO32.AD rewrites/draino32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/draino32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/mountain32.rc \
		-O coff -o $(DIST)/rewrite-build/mountain32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/MOUNTAIN32.AD rewrites/mountain32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/mountain32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/vertigo32.rc \
		-O coff -o $(DIST)/rewrite-build/vertigo32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/VERTIGO32.AD rewrites/vertigo32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/vertigo32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/sunburst32.rc \
		-O coff -o $(DIST)/rewrite-build/sunburst32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SUNBURST32.AD rewrites/sunburst32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/sunburst32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/satori32.rc \
		-O coff -o $(DIST)/rewrite-build/satori32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SATORI32.AD rewrites/satori32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/satori32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/snake32.rc \
		-O coff -o $(DIST)/rewrite-build/snake32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/SNAKE32.AD rewrites/snake32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/snake32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/nirvana32.rc \
		-O coff -o $(DIST)/rewrite-build/nirvana32-res.o
	$(CC32) $(CFLAGS) -shared -Wl,--kill-at \
		-o $(DIST)/rewrites/NIRVANA32.AD rewrites/nirvana32.c rewrites/admkit.c \
		$(DIST)/rewrite-build/nirvana32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/gravity32.rc -O coff -o $(DIST)/rewrite-build/gravity32-res.o
	$(CC32) $(CFLAGS) -DADM_GRAVITY -shared -Wl,--kill-at -o $(DIST)/rewrites/GRAVITY32.AD rewrites/physics32.c rewrites/admkit.c $(DIST)/rewrite-build/gravity32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/punch32.rc -O coff -o $(DIST)/rewrite-build/punch32-res.o
	$(CC32) $(CFLAGS) -DADM_PUNCH -shared -Wl,--kill-at -o $(DIST)/rewrites/PUNCH32.AD rewrites/physics32.c rewrites/admkit.c $(DIST)/rewrite-build/punch32-res.o -lgdi32 -lm
	$(WINDRES) $(DIST)/rewrite-build/worms32.rc -O coff -o $(DIST)/rewrite-build/worms32-res.o
	$(CC32) $(CFLAGS) -DADM_WORMS -shared -Wl,--kill-at -o $(DIST)/rewrites/WORMS32.AD rewrites/physics32.c rewrites/admkit.c $(DIST)/rewrite-build/worms32-res.o -lgdi32 -lm

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
