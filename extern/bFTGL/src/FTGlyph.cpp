#include    "FTGlyph.h"


FTGlyph::FTGlyph( FT_GlyphSlot glyph)
:   advance(0.0f),
    err(0)  
{
    if( glyph)
    {
        bBox = FTBBox( glyph);
        advance = static_cast<float>( glyph->advance.x) / 64.0f;
    }
}


FTGlyph::~FTGlyph()
{}
