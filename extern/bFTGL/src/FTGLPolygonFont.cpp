#include    "FTGLPolygonFont.h"
#include    "FTPolyGlyph.h"


FTGLPolygonFont::FTGLPolygonFont( const char* fontname)
:   FTFont( fontname)
{}


FTGLPolygonFont::FTGLPolygonFont( const unsigned char *pBufferBytes, size_t bufferSizeInBytes)
:   FTFont( pBufferBytes, bufferSizeInBytes)
{}


FTGLPolygonFont::~FTGLPolygonFont()
{}


FTGlyph* FTGLPolygonFont::MakeGlyph( unsigned int g)
{
    FT_GlyphSlot ftGlyph = face.Glyph( g, FT_LOAD_NO_HINTING);

    if( ftGlyph)
    {
        FTPolyGlyph* tempGlyph = new FTPolyGlyph( ftGlyph);
        return tempGlyph;
    }

    err = face.Error();
    return NULL;
}


