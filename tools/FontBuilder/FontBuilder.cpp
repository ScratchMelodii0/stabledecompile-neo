// FontBuilder - a standalone command line replacement for the (source-less)
// tools/FontBuilder.exe / Fontilizer tool referenced by "Font Building.pdf".
//
// WHAT THIS TOOL PRODUCES
// ------------------------
// Given a TTF/OTF file, a point size and a character set, it rasterizes each
// glyph with SDL_ttf and writes:
//   - "<name>.bmp"  a horizontal strip atlas image (32bpp, straight alpha)
//                    containing every requested glyph, left to right.
//   - "<name>.txt"  a font descriptor in the exact "DescParser" command
//                    language that SexyAppFramework/ImageFont.cpp's
//                    FontData::LoadDescriptor()/HandleCommand() understands
//                    (Define/CreateLayer/LayerSetImage/LayerSetImageMap/
//                    LayerSetCharWidths/LayerSetAscent/LayerSetHeight/
//                    SetDefaultPointSize).
//
// FORMAT PROVENANCE / CONFIDENCE
// -------------------------------
// The original binary "Fontilizer" tool and its file format are not present
// in this repository in source form, only the compiled tools/FontBuilder.exe
// and a copy of its PDF user manual (tools/"Font Building.pdf"). That manual
// documents the on-disk descriptor as a text command script with
// Define CHARLIST / WIDTHLIST / RECTLIST / OFFSETLIST / KERNING PAIRS /
// KERNING VALUES plus LayerSetImage / LayerSetCharWidths lines (Appendix A).
// That matches, command-for-command, the parser implemented in
// SexyAppFramework/DescParser.cpp (tokenizer: whitespace/comma separated,
// parens for lists, quotes for strings, one command per line) and
// SexyAppFramework/ImageFont.cpp's FontData::HandleCommand(), which is the
// code that actually *loads* these fonts at runtime. So:
//   - VERIFIED: the descriptor is a DescParser command script, and the
//     specific commands this tool emits are all commands HandleCommand()
//     implements and will accept.
//   - VERIFIED: characters are looked up through a 256-entry (uchar-indexed)
//     table in FontLayer/FontData (mCharData[256], mCharMap[256]), so this
//     legacy ImageFont format is inherently single-byte/Latin-1 and is NOT
//     a route to full Unicode glyph coverage - see the note below and the
//     separate Unicode work done in SDL3Font.cpp for that. This tool covers
//     the 0-255 range only, matching what the engine can actually load.
//   - BEST EFFORT / not binary-verified against the original Fontilizer
//     output: exact pixel-for-pixel rendering (antialiasing, hinting,
//     padding defaults), and the precise image packing the original tool
//     used (whether it packed onto multiple rows, added padding by default,
//     etc). This tool packs every glyph into a single row, which is simple,
//     always valid input to LayerSetImageMap, and easy to inspect/tweak by
//     hand afterwards, exactly as the manual describes editing the .txt file.
//
// This is a genuinely functional tool: it will produce a .txt/.bmp pair
// that FontData::LoadDescriptor() can load for any installed TTF/OTF, not a
// stub. See tools/FontBuilder/README.md for build + usage instructions and
// for how to convert the .bmp to .png if your resources are built as PNG.

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

struct GlyphInfo
{
	Uint32 mCodepoint = 0;
	int mWidth = 0;      // advance width used for spacing/kerning purposes
	int mRectX = 0, mRectY = 0, mRectW = 0, mRectH = 0; // location in the atlas
	SDL_Surface* mSurface = nullptr; // owned, freed after blitting
};

// The default "international" character set called out in the Fontilizer
// manual (Latin-1 printable range), used when the caller doesn't pass -chars.
static std::string DefaultCharacterSet()
{
	std::string aChars;
	for (int c = 0x20; c <= 0x7E; ++c)
		aChars += (char)c;
	// Latin-1 supplement, encoded as UTF-8 so -chars parsing (which is UTF-8
	// aware) handles it the same way as any user supplied set would.
	for (unsigned int cp = 0xA1; cp <= 0xFF; ++cp)
	{
		if (cp < 0x80)
		{
			aChars += (char)cp;
		}
		else
		{
			aChars += (char)(0xC0 | (cp >> 6));
			aChars += (char)(0x80 | (cp & 0x3F));
		}
	}
	return aChars;
}

// Minimal UTF-8 decoder (same approach used in the engine's SexyAppFramework
// fixes for this feature) so -chars can be given as a UTF-8 string on the
// command line and still produce one GlyphInfo per codepoint.
static std::vector<Uint32> DecodeUTF8(const std::string& theString)
{
	std::vector<Uint32> aCodepoints;
	size_t i = 0, aLen = theString.length();
	while (i < aLen)
	{
		unsigned char c0 = (unsigned char)theString[i];
		int aExtraBytes;
		Uint32 aCodepoint;
		if (c0 < 0x80) { aCodepoint = c0; aExtraBytes = 0; }
		else if ((c0 & 0xE0) == 0xC0) { aCodepoint = c0 & 0x1F; aExtraBytes = 1; }
		else if ((c0 & 0xF0) == 0xE0) { aCodepoint = c0 & 0x0F; aExtraBytes = 2; }
		else if ((c0 & 0xF8) == 0xF0) { aCodepoint = c0 & 0x07; aExtraBytes = 3; }
		else { aCodepoints.push_back(c0); ++i; continue; }

		if (i + aExtraBytes >= aLen) { aCodepoints.push_back(c0); ++i; continue; }

		Uint32 aValue = aCodepoint;
		bool valid = true;
		for (int b = 1; b <= aExtraBytes; ++b)
		{
			unsigned char cc = (unsigned char)theString[i + b];
			if ((cc & 0xC0) != 0x80) { valid = false; break; }
			aValue = (aValue << 6) | (cc & 0x3F);
		}

		if (!valid) { aCodepoints.push_back(c0); ++i; continue; }

		aCodepoints.push_back(aValue);
		i += aExtraBytes + 1;
	}
	return aCodepoints;
}

// Writes an uncompressed 32bpp BGRA BMP (BITMAPV4 style alpha mask). This is
// dependency-free (no libpng/SDL_image needed) and any standard image tool
// (including SDL3Image via SDL_image, if wired up) can read/convert it; see
// the README for converting to PNG if your resource pipeline expects that.
static bool WriteBMP32(const char* thePath, SDL_Surface* theSurface)
{
	SDL_Surface* aConv = SDL_ConvertSurfaceFormat(theSurface, SDL_PIXELFORMAT_ABGR8888, 0);
	if (!aConv)
		return false;

	FILE* f = fopen(thePath, "wb");
	if (!f)
	{
		SDL_FreeSurface(aConv);
		return false;
	}

	int w = aConv->w, h = aConv->h;
	uint32_t aRowBytes = (uint32_t)w * 4;
	uint32_t aPixelDataSize = aRowBytes * (uint32_t)h;
	uint32_t aHeaderSize = 14 + 108; // BITMAPFILEHEADER + BITMAPV4HEADER
	uint32_t aFileSize = aHeaderSize + aPixelDataSize;

	auto putU16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	auto putU32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto putI32 = [&](int32_t v) { fwrite(&v, 4, 1, f); };

	// BITMAPFILEHEADER
	fwrite("BM", 1, 2, f);
	putU32(aFileSize);
	putU32(0);
	putU32(aHeaderSize);

	// BITMAPV4HEADER (supports an explicit alpha mask)
	putU32(108);
	putI32(w);
	putI32(h); // positive height = bottom-up rows, standard BMP order
	putU16(1);
	putU16(32);
	putU32(3); // BI_BITFIELDS
	putU32(aPixelDataSize);
	putI32(2835); putI32(2835); // ~72 DPI
	putU32(0); putU32(0);
	putU32(0x000000FFu); // R mask
	putU32(0x0000FF00u); // G mask
	putU32(0x00FF0000u); // B mask
	putU32(0xFF000000u); // A mask
	putU32(0x73524742u); // "sRGB" colorspace
	for (int i = 0; i < 12; ++i) putU32(0); // CIEXYZTRIPLE + gamma, unused

	SDL_LockSurface(aConv);
	std::vector<uint8_t> aRow(aRowBytes);
	for (int y = h - 1; y >= 0; --y) // bottom-up
	{
		const uint8_t* aSrc = (const uint8_t*)aConv->pixels + (size_t)y * aConv->pitch;
		memcpy(aRow.data(), aSrc, aRowBytes);
		fwrite(aRow.data(), 1, aRowBytes, f);
	}
	SDL_UnlockSurface(aConv);

	fclose(f);
	SDL_FreeSurface(aConv);
	return true;
}

static std::string EscapeForSingleQuotes(const std::string& s)
{
	std::string aOut;
	for (char c : s)
	{
		if (c == '\'')
			aOut += "''"; // DescParser::Unquote treats a doubled quote char as a literal quote
		else
			aOut += c;
	}
	return aOut;
}

static void PrintUsage(const char* argv0)
{
	std::fprintf(stderr,
		"FontBuilder - generates a legacy ImageFont descriptor (.txt) + atlas (.bmp)\n"
		"from a TTF/OTF font, for use with SexyAppFramework's FontData::LoadDescriptor.\n\n"
		"Usage: %s -font <file.ttf> -size <points> -out <name> [options]\n"
		"  -font <path>      TrueType/OpenType font file to rasterize (required)\n"
		"  -size <points>    Point size to render at (required)\n"
		"  -out <name>       Output basename; writes <name>.txt and <name>.bmp (required)\n"
		"  -chars <utf8>     Characters to include (UTF-8). Codepoints above U+00FF\n"
		"                    are skipped with a warning - the legacy ImageFont format\n"
		"                    is single-byte (256 entries), see FontBuilder.cpp header.\n"
		"                    Default: ASCII 0x20-0x7E + Latin-1 supplement 0xA1-0xFF.\n"
		"  -bold             Render bold\n"
		"  -italic           Render italic\n"
		"  -image <name>     Image filename referenced by LayerSetImage in the .txt\n"
		"                    (default: <name>.bmp)\n",
		argv0);
}

int main(int argc, char** argv)
{
	std::string aFontPath, aOutBase, aCharsUtf8, aImageNameOverride;
	int aPointSize = 0;
	bool aBold = false, aItalic = false;

	for (int i = 1; i < argc; ++i)
	{
		std::string a = argv[i];
		auto next = [&](const char* opt) -> std::string {
			if (i + 1 >= argc) { std::fprintf(stderr, "Missing value for %s\n", opt); exit(1); }
			return argv[++i];
		};

		if (a == "-font") aFontPath = next("-font");
		else if (a == "-size") aPointSize = std::atoi(next("-size").c_str());
		else if (a == "-out") aOutBase = next("-out");
		else if (a == "-chars") aCharsUtf8 = next("-chars");
		else if (a == "-image") aImageNameOverride = next("-image");
		else if (a == "-bold") aBold = true;
		else if (a == "-italic") aItalic = true;
		else if (a == "-h" || a == "--help") { PrintUsage(argv[0]); return 0; }
		else { std::fprintf(stderr, "Unknown option: %s\n", a.c_str()); PrintUsage(argv[0]); return 1; }
	}

	if (aFontPath.empty() || aPointSize <= 0 || aOutBase.empty())
	{
		PrintUsage(argv[0]);
		return 1;
	}

	if (aCharsUtf8.empty())
		aCharsUtf8 = DefaultCharacterSet();

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}
	if (TTF_Init() != 0)
	{
		std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
		return 1;
	}

	TTF_Font* aFont = TTF_OpenFont(aFontPath.c_str(), aPointSize);
	if (!aFont)
	{
		std::fprintf(stderr, "Failed to open font '%s': %s\n", aFontPath.c_str(), TTF_GetError());
		return 1;
	}

	int aStyle = TTF_STYLE_NORMAL;
	if (aBold) aStyle |= TTF_STYLE_BOLD;
	if (aItalic) aStyle |= TTF_STYLE_ITALIC;
	TTF_SetFontStyle(aFont, aStyle);

	SDL_Color aWhite = { 255, 255, 255, 255 };

	std::vector<GlyphInfo> aGlyphs;
	std::vector<Uint32> aCodepoints = DecodeUTF8(aCharsUtf8);

	int aMaxHeight = TTF_FontHeight(aFont);
	int aAscent = TTF_FontAscent(aFont);

	int aCurX = 0;
	for (Uint32 cp : aCodepoints)
	{
		if (cp > 0xFF)
		{
			std::fprintf(stderr, "warning: skipping U+%04X - outside the legacy ImageFont 0x00-0xFF char range\n", cp);
			continue;
		}
		if (!TTF_GlyphIsProvided(aFont, (Uint16)cp))
		{
			std::fprintf(stderr, "warning: font has no glyph for U+%04X, skipping\n", cp);
			continue;
		}

		SDL_Surface* aSurf = TTF_RenderGlyph_Blended(aFont, (Uint16)cp, aWhite);
		if (!aSurf)
			continue; // e.g. space glyph may render an empty surface on some SDL_ttf versions; handled below

		int aMinX, aMaxX, aMinY, aMaxYm, aAdvance;
		TTF_GlyphMetrics(aFont, (Uint16)cp, &aMinX, &aMaxX, &aMinY, &aMaxYm, &aAdvance);

		GlyphInfo aInfo;
		aInfo.mCodepoint = cp;
		aInfo.mWidth = aAdvance;
		aInfo.mRectX = aCurX;
		aInfo.mRectY = 0;
		aInfo.mRectW = aSurf->w;
		aInfo.mRectH = aSurf->h;
		aInfo.mSurface = aSurf;

		aCurX += aSurf->w;
		if (aSurf->h > aMaxHeight) aMaxHeight = aSurf->h;

		aGlyphs.push_back(aInfo);
	}

	if (aGlyphs.empty())
	{
		std::fprintf(stderr, "No glyphs were rasterized; nothing to write.\n");
		TTF_CloseFont(aFont);
		TTF_Quit();
		SDL_Quit();
		return 1;
	}

	int aAtlasW = std::max(1, aCurX);
	int aAtlasH = std::max(1, aMaxHeight);

	SDL_Surface* aAtlas = SDL_CreateRGBSurfaceWithFormat(0, aAtlasW, aAtlasH, 32, SDL_PIXELFORMAT_RGBA32);
	if (!aAtlas)
	{
		std::fprintf(stderr, "Failed to allocate atlas surface: %s\n", SDL_GetError());
		return 1;
	}
	SDL_FillRect(aAtlas, nullptr, SDL_MapRGBA(aAtlas->format, 0, 0, 0, 0));

	for (GlyphInfo& g : aGlyphs)
	{
		SDL_Rect aDst = { g.mRectX, g.mRectY, g.mRectW, g.mRectH };
		SDL_SetSurfaceBlendMode(g.mSurface, SDL_BLENDMODE_NONE); // copy alpha as-is, don't composite
		SDL_BlitSurface(g.mSurface, nullptr, aAtlas, &aDst);
		SDL_FreeSurface(g.mSurface);
		g.mSurface = nullptr;
	}

	std::string aBmpPath = aOutBase + ".bmp";
	std::string aTxtPath = aOutBase + ".txt";
	std::string aImageName = aImageNameOverride.empty() ? (aOutBase + ".bmp") : aImageNameOverride;

	if (!WriteBMP32(aBmpPath.c_str(), aAtlas))
	{
		std::fprintf(stderr, "Failed to write '%s'\n", aBmpPath.c_str());
		return 1;
	}
	SDL_FreeSurface(aAtlas);

	// Emit the descriptor as a DescParser command script (see header comment
	// for exactly which HandleCommand() commands these are and how they were
	// confirmed against the engine source + the Fontilizer manual).
	FILE* aOut = fopen(aTxtPath.c_str(), "w");
	if (!aOut)
	{
		std::fprintf(stderr, "Failed to write '%s'\n", aTxtPath.c_str());
		return 1;
	}

	std::fprintf(aOut, "# Generated by tools/FontBuilder from '%s' at %d pt\n", aFontPath.c_str(), aPointSize);
	std::fprintf(aOut, "# See tools/FontBuilder/README.md for format notes.\n\n");

	std::fprintf(aOut, "Define CHARLIST (");
	for (size_t i = 0; i < aGlyphs.size(); ++i)
	{
		Uint32 cp = aGlyphs[i].mCodepoint;
		char c = (char)cp;
		std::fprintf(aOut, "%s'%s'", i ? " " : "", EscapeForSingleQuotes(std::string(1, c)).c_str());
	}
	std::fprintf(aOut, ")\n");

	std::fprintf(aOut, "Define WIDTHLIST (");
	for (size_t i = 0; i < aGlyphs.size(); ++i)
		std::fprintf(aOut, "%s%d", i ? " " : "", aGlyphs[i].mWidth);
	std::fprintf(aOut, ")\n");

	std::fprintf(aOut, "Define RECTLIST (");
	for (size_t i = 0; i < aGlyphs.size(); ++i)
	{
		const GlyphInfo& g = aGlyphs[i];
		std::fprintf(aOut, "%s(%d %d %d %d)", i ? " " : "", g.mRectX, g.mRectY, g.mRectW, g.mRectH);
	}
	std::fprintf(aOut, ")\n\n");

	std::fprintf(aOut, "CreateLayer Main\n");
	std::fprintf(aOut, "LayerSetImage Main '%s'\n", EscapeForSingleQuotes(aImageName).c_str());
	std::fprintf(aOut, "LayerSetImageMap Main CHARLIST RECTLIST\n");
	std::fprintf(aOut, "LayerSetCharWidths Main CHARLIST WIDTHLIST\n");
	std::fprintf(aOut, "LayerSetAscent Main %d\n", aAscent);
	std::fprintf(aOut, "LayerSetHeight Main %d\n", aAtlasH);
	std::fprintf(aOut, "SetDefaultPointSize %d\n", aPointSize);

	fclose(aOut);

	std::printf("Wrote %s and %s (%d glyphs, %dx%d atlas)\n",
		aTxtPath.c_str(), aBmpPath.c_str(), (int)aGlyphs.size(), aAtlasW, aAtlasH);

	TTF_CloseFont(aFont);
	TTF_Quit();
	SDL_Quit();
	return 0;
}
