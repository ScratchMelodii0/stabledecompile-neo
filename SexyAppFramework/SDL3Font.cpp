#include "SDL3Font.h"
#include "../LawnApp.h"
#include "Graphics.h"

using namespace Sexy;

std::vector<SDL3Font*> SDL3Font::gSDLFonts;

// SexyString stores UTF-8 encoded text (see Common.h). SexyChar is just a
// single byte of that encoding, so iterating a SexyString one SexyChar at a
// time (as the code below used to do) splits every multi-byte character into
// several bogus "glyphs". DecodeUTF8Codepoints walks the byte string and
// returns the actual Unicode codepoints so glyph lookup/caching and string
// measurement work correctly for non-ASCII text. Malformed/truncated
// sequences fall back to treating the offending byte as a Latin-1 codepoint
// so we never lose or desync on bad input.
static std::vector<Uint32> DecodeUTF8Codepoints(const SexyString& theString)
{
	std::vector<Uint32> aCodepoints;
	aCodepoints.reserve(theString.length());

	size_t i = 0;
	size_t aLen = theString.length();
	while (i < aLen)
	{
		unsigned char c0 = (unsigned char)theString[i];
		int aExtraBytes;
		Uint32 aCodepoint;

		if (c0 < 0x80)			{ aCodepoint = c0;			aExtraBytes = 0; }
		else if ((c0 & 0xE0) == 0xC0)	{ aCodepoint = c0 & 0x1F;	aExtraBytes = 1; }
		else if ((c0 & 0xF0) == 0xE0)	{ aCodepoint = c0 & 0x0F;	aExtraBytes = 2; }
		else if ((c0 & 0xF8) == 0xF0)	{ aCodepoint = c0 & 0x07;	aExtraBytes = 3; }
		else							{ aCodepoints.push_back(c0); ++i; continue; }

		if (i + aExtraBytes >= aLen)
		{
			aCodepoints.push_back(c0);
			++i;
			continue;
		}

		bool valid = true;
		Uint32 aValue = aCodepoint;
		for (int b = 1; b <= aExtraBytes; ++b)
		{
			unsigned char cc = (unsigned char)theString[i + b];
			if ((cc & 0xC0) != 0x80) { valid = false; break; }
			aValue = (aValue << 6) | (cc & 0x3F);
		}

		if (!valid)
		{
			aCodepoints.push_back(c0);
			++i;
			continue;
		}

		aCodepoints.push_back(aValue);
		i += aExtraBytes + 1;
	}

	return aCodepoints;
}

void SDL3Font::RebuildFonts(float scale)
{
	float snappedScale = floor(scale * 4.0f) / 4.0f;

	for (SDL3Font* font : gSDLFonts) {
		if (font->mScale == snappedScale || !font->mIsActive) continue;

		font->mScale = snappedScale;
		font->mScaleOffset = scale - snappedScale;
		font->Rebuild();
		font->mIsActive = true;
	}
}

void SDL3Font::Rebuild()
{
	for (auto& pair : mGlyphCache)
	{
		SDL_DestroyTexture(pair.second);
	}
	mGlyphCache.clear();
	mGlyphWidth.clear();
	mGlyphHeight.clear();
	if (mFont)
	{
		TTF_CloseFont(mFont);
		mFont = nullptr;
	}
	Init(mApp, mPathFile, mPointSize, mBold, mItalic, mUnderline);
}

void SDL3Font::SetActive(bool active)
{
	bool prevActive = mIsActive;
	mIsActive = active;
	if (prevActive != mIsActive && active) {
		float scale = 1.0f;
		int pw, ph;
		SDL_GetWindowSizeInPixels(LawnApp::mSDLWindow, &pw, &ph);
		float gScale = max(static_cast<float>(pw) / 800.0f, static_cast<float>(ph) / 600.0f);
		if (pw >= 800 || ph >= 600) scale = max(floor(gScale * 4.0f) / 4.0f, 1.0f);
		float prevMScale = mScale;
		if (prevMScale != scale)
		{
			mScale = scale;
			mScaleOffset = gScale - scale;
			Rebuild();
			mIsActive = true;
		}
	}
}

void SDL3Font::Init(SexyAppBase* theApp, const std::string& theFace, int thePointSize, bool bold, bool italics, bool underline)
{
	if (mInitializedScale) {
		mInitializedScale = true;
		float scale = 1.0f;
		int pw, ph;
		SDL_GetWindowSizeInPixels(LawnApp::mSDLWindow, &pw, &ph);
		float gScale = max(static_cast<float>(pw) / 800.0f, static_cast<float>(ph) / 600.0f);
		if (pw >= 800 || ph >= 600) scale = max(floor(gScale * 4.0f) / 4.0f, 1.0f);
		mScale = scale;
		mScaleOffset = gScale - scale;
	}

	mIsActive = false;
	mApp = theApp;
	mPointSize = thePointSize;
	mPathFile = theFace;
	mFont = TTF_OpenFont(mPathFile.c_str(), int(floor(thePointSize * (mScale * mDefaultScale))));

	int style = TTF_STYLE_NORMAL;
	if (bold) style |= TTF_STYLE_BOLD;
	if (italics) style |= TTF_STYLE_ITALIC;
	if (underline) style |= TTF_STYLE_UNDERLINE;
	TTF_SetFontStyle(mFont, style);

	TTF_SetFontHinting(mFont, TTF_HINTING_LIGHT_SUBPIXEL);
	TTF_SetFontKerning(mFont, true);

	mBold = bold;
	mItalic = italics;
	mUnderline = underline;

	Color defaultColor(255, 255, 255, 255);
	for (Uint32 c = 32; c < 127; ++c)
		CacheGlyph(c, defaultColor);
}

void SDL3Font::CacheGlyph(Uint32 c, const Color& color)
{
	if (!mFont || mGlyphCache.find(c) != mGlyphCache.end()) return;

	SDL_Color sdlColor = { Uint8(color.mRed), Uint8(color.mGreen), Uint8(color.mBlue), Uint8(color.mAlpha) };
	SDL_Surface* surface = TTF_RenderGlyph_Blended(mFont, c, sdlColor);
	if (!surface) return;

	SDL_Texture* texture = SDL_CreateTextureFromSurface(LawnApp::mSDLRenderer, surface);
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

	mGlyphCache[c] = texture;
	mGlyphWidth[c] = surface->w;
	mGlyphHeight[c] = surface->h;

	SDL_DestroySurface(surface);
}

SDL3Font::SDL3Font(const std::string& theFace, int thePointSize, bool bold, bool italics, bool underline)
{
	Init(gSexyAppBase, theFace, thePointSize, bold, italics, underline);
	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), this);
	if (it == gSDLFonts.end())
		gSDLFonts.push_back(this);
}

SDL3Font::SDL3Font(SexyAppBase* theApp, const std::string& theFace, int thePointSize, bool bold, bool italics, bool underline)
{
	Init(theApp, theFace, thePointSize, bold, italics, underline);
	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), this);
	if (it == gSDLFonts.end())
		gSDLFonts.push_back(this);
}

SDL3Font::~SDL3Font()
{
	for (auto& pair : mGlyphCache)
		SDL_DestroyTexture(pair.second);
	mGlyphCache.clear();

	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), this);
	if (it != gSDLFonts.end())
		gSDLFonts.erase(it);

	if (mFont)
	{
		TTF_CloseFont(mFont);
		mFont = nullptr;
	}
}

float SDL3Font::GetRelativeScale()
{
	return (mScale - mScaleOffset) * mDefaultScale;
}

int SDL3Font::StringWidth(const SexyString& theString)
{
	int w = 0;
	for (Uint32 c : DecodeUTF8Codepoints(theString))
	{
		auto it = mGlyphWidth.find(c);
		if (it != mGlyphWidth.end())
			w += it->second;
		else
			w += TTF_GetGlyphImageForIndex(mFont, c, NULL) ? TTF_GetGlyphImageForIndex(mFont, c, NULL)->w : 0;
	}
	return w / GetRelativeScale();
}

void SDL3Font::DrawString(Graphics* g, int theX, int theY, const SexyString& theString, const Color& theColor, const Rect& theClipRect)
{
	if (!mFont) return;

	int x = theX;

	if (theClipRect.mWidth > 0 && theClipRect.mHeight > 0)
	{
		SDL_Rect clip = { theClipRect.mX, theClipRect.mY, theClipRect.mWidth, theClipRect.mHeight };
		SDL_SetRenderClipRect(LawnApp::mSDLRenderer, &clip);
	}

	Uint32 prevChar = 0;
	for (Uint32 c : DecodeUTF8Codepoints(theString))
	{
		if (prevChar)
		{
			int kern = 0;
			TTF_GetGlyphKerning(mFont, prevChar, c, &kern);
			x += kern / GetRelativeScale();
		}

		if (mGlyphCache.find(c) == mGlyphCache.end())
			CacheGlyph(c, theColor);

		SDL_Texture* tex = mGlyphCache[c];
		if (tex)
		{
			SDL_FRect dst = { float(x), float(theY - GetAscent()), float(mGlyphWidth[c]) / GetRelativeScale(), float(mGlyphHeight[c]) / GetRelativeScale()};
			SDL_SetTextureColorMod(tex, theColor.mRed, theColor.mGreen, theColor.mBlue);
			SDL_SetTextureAlphaMod(tex, theColor.mAlpha);
			SDL_SetTextureScaleMode(tex, g->GetLinearBlend() ? SDL_ScaleMode::SDL_SCALEMODE_LINEAR : g->mIsPixelArt ? SDL_ScaleMode::SDL_SCALEMODE_PIXELART : SDL_ScaleMode::SDL_SCALEMODE_NEAREST);
			SDL_RenderTexture(LawnApp::mSDLRenderer, tex, nullptr, &dst);
			x += mGlyphWidth[c] / GetRelativeScale();
		}

		prevChar = c;
	}

	SDL_SetRenderClipRect(LawnApp::mSDLRenderer, nullptr);
}

SDL3Font* SDL3Font::Duplicate()
{
	SDL3Font* newFont = new SDL3Font(mApp, mPathFile, mPointSize, mBold, mItalic, mUnderline);
	auto it = std::find(gSDLFonts.begin(), gSDLFonts.end(), newFont);
	if (it == gSDLFonts.end())
		gSDLFonts.push_back(newFont);
	return newFont;
}

int	SDL3Font::GetAscent() { return TTF_GetFontAscent(mFont) / GetRelativeScale(); }
int	SDL3Font::GetAscentPadding() { return 0; }
int	SDL3Font::GetDescent() { return TTF_GetFontDescent(mFont) / GetRelativeScale(); }
int	SDL3Font::GetHeight() { return TTF_GetFontHeight(mFont) / GetRelativeScale(); }
int SDL3Font::GetLineSpacingOffset() { return GetHeight() + GetLineSpacing(); }
int SDL3Font::GetLineSpacing() { return TTF_GetFontLineSkip(mFont) / GetRelativeScale(); }
int SDL3Font::CharWidth(SexyChar theChar) { return mGlyphWidth[theChar] / GetRelativeScale(); }
int SDL3Font::CharWidthKern(SexyChar theChar, SexyChar thePrevChar) { int kern; TTF_GetGlyphKerning(mFont, thePrevChar, theChar, &kern); return kern / GetRelativeScale(); }
