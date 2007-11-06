#include    "FTGLBufferFont.h"
#include    "FTBufferGlyph.h"


FTGLBufferFont::FTGLBufferFont( const char* fontname)
:   FTFont( fontname),
    buffer(0)
{}


FTGLBufferFont::FTGLBufferFont( const unsigned char *pBufferBytes, size_t bufferSizeInBytes)
:   FTFont( pBufferBytes, bufferSizeInBytes),
    buffer(0)
{}


FTGLBufferFont::~FTGLBufferFont()
{}


FTGlyph* FTGLBufferFont::MakeGlyph( unsigned int g)
{
    FT_GlyphSlot ftGlyph = face.Glyph( g, FT_LOAD_NO_HINTING);

    if( ftGlyph)
    {
        FTBufferGlyph* tempGlyph = new FTBufferGlyph( ftGlyph, buffer);
        return tempGlyph;
    }

    err = face.Error();
    return NULL;
}


void FTGLBufferFont::Render( const char* string)
{   
    if( NULL != buffer)
    {
        FTFont::Render( string);
    }
}


void FTGLBufferFont::Render( const wchar_t* string)
{   
    if( NULL != buffer)
    {
        FTFont::Render( string);
    }
}


