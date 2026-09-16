# FontBuilder

A from-source, command-line replacement for the prebuilt (source-less)
`tools/FontBuilder.exe` / "Fontilizer" tool documented in
`tools/Font Building.pdf`.

It takes a TTF/OTF font and a character set and produces the two files the
engine's legacy `ImageFont` loader (`SexyAppFramework/ImageFont.cpp`,
`FontData::LoadDescriptor`/`HandleCommand`) expects:

- `<name>.txt` — a font descriptor written in the same command script
  language `SexyAppFramework/DescParser.cpp` parses (`Define`, `CreateLayer`,
  `LayerSetImage`, `LayerSetImageMap`, `LayerSetCharWidths`, `LayerSetAscent`,
  `LayerSetHeight`, `SetDefaultPointSize`).
- `<name>.bmp` — a single-row glyph atlas (32bpp BGRA, straight alpha)
  referenced by the descriptor's `LayerSetImage` line.

See the header comment in `FontBuilder.cpp` for exactly how the descriptor
format was confirmed against the engine source and the original manual, and
what's verified vs. best-effort (in short: the command syntax and the
commands used are verified against `DescParser`/`ImageFont`'s loader code;
the precise packing/antialiasing choices of the original binary tool were
not available to compare against, so this tool makes its own reasonable,
documented choices).

**Important limitation**: this legacy `ImageFont` format is inherently
single-byte (glyphs are looked up through a 256-entry table keyed by
`unsigned char`), so it only supports the 0x00–0xFF codepoint range
(ASCII + Latin-1). It is not a path to full Unicode/multi-language glyph
coverage — that work was done separately for the TrueType-backed
`SDL3Font` renderer (see `SexyAppFramework/SDL3Font.cpp`), which decodes
full UTF-8 codepoints and renders directly from the .ttf at runtime instead
of going through a prebaked 256-glyph atlas.

## Building

This tool is not wired into `PlantsVsZombies.sln` (adding a matching vcxproj
for a standalone CLI tool didn't have a natural home there, since the .sln
only builds the game and its native dependencies). Build it standalone with
CMake instead:

```sh
# Needs SDL2 and SDL2_ttf development packages (e.g. `apt install
# libsdl2-dev libsdl2-ttf-dev`, `vcpkg install sdl2 sdl2-ttf`, or the
# equivalent for your platform).
cd tools/FontBuilder
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

This produces a `FontBuilder` (or `FontBuilder.exe`) executable.

## Usage

```
FontBuilder -font <file.ttf> -size <points> -out <name> [options]
  -font <path>      TrueType/OpenType font file to rasterize (required)
  -size <points>    Point size to render at (required)
  -out <name>       Output basename; writes <name>.txt and <name>.bmp (required)
  -chars <utf8>     Characters to include (UTF-8). Codepoints above U+00FF are
                     skipped with a warning. Default: ASCII 0x20-0x7E + Latin-1
                     supplement 0xA1-0xFF (the same default set Fontilizer's
                     manual describes as its "international character set").
  -bold             Render bold
  -italic           Render italic
  -image <name>     Image filename referenced by LayerSetImage in the .txt
                     (default: <name>.bmp)
```

Example:

```sh
./build/FontBuilder -font /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    -size 16 -out MyFont
```

writes `MyFont.txt` and `MyFont.bmp`.

If your resource pipeline expects PNG rather than BMP, convert the `.bmp`
with any image tool (`magick MyFont.bmp MyFont.png`, GIMP, etc.) and update
the `LayerSetImage` line in the `.txt` (or pass `-image MyFont.png` up
front) to match.

You can hand-edit the generated `.txt` afterwards exactly as described in
`tools/Font Building.pdf` Appendix A (e.g. tweak `LayerSetCharWidths` for the
space character, or add extra `LayerSetOffset`/kerning commands).
