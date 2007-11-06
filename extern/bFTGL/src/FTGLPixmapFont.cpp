#include    "FTGLPixmapFont.h"
#include    "FTPixmapGlyph.h"


FTGLPixmapFont::FTGLPixmapFont( const char* fontname)
:   FTFont( fontname)
{}


FTGLPixmapFont::FTGLPixmapFont( const unsigned char *pBufferBytes, size_t bufferSizeInBytes)
:   FTFont( pBufferBytes, bufferSizeInBytes)
{}


FTGLPixmapFont::~FTGLPixmapFont()
{}


FTGlyph* FTGLPixmapFont::MakeGlyph( unsigned int g)
{
    FT_GlyphSlot ftGlyph = face.Glyph( g, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);

    if( ftGlyph)
    {
        FTPixmapGlyph* tempGlyph = new FTPixmapGlyph( ftGlyph);
        return tempGlyph;
    }

    err = face.Error();
    return NULL;
}


void FTGLPixmapFont::Render( const char* string)
{   
    glPushAttrib( GL_ENABLE_BIT | GL_PIXEL_MODE_BIT | GL_COLOR_BUFFER_BIT);
    glPushClientAttrib( GL_CLIENT_PIXEL_STORE_BIT);

    glEnable(GL_BLEND);
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable( GL_TEXTURE_2D);

    FTFont::Render( string);

    glPopClientAttrib();
    glPopAttrib();
}


void FTGLPixmapFont::Render( const wchar_t* string)
{   
    //glPushAttrib( GL_ENABLE_BIT | GL_PIXEL_MODE_BIT | GL_COLOR_BUFFER_BIT);
   // glPushClientAttrib( GL_CLIENT_PIXEL_STORE_BIT);
        
    glEnable(GL_BLEND);
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable( GL_TEXTURE_2D);

    FTFont::Render( string);

	glDisable(GL_BLEND);
   // glPopClientAttrib();
   // glPopAttrib();
}


