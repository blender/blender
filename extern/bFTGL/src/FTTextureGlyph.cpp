#include    "FTTextureGlyph.h"


FTTextureGlyph::FTTextureGlyph( FT_GlyphSlot glyph, int id, int xOffset, int yOffset, GLsizei width, GLsizei height)
:   FTGlyph( glyph),
    destWidth(0),
    destHeight(0),
    glTextureID(id),
    activeTextureID(0)
{
    err = FT_Render_Glyph( glyph, FT_RENDER_MODE_NORMAL);
    if( err || glyph->format != ft_glyph_format_bitmap)
    {
        return;
    }

    FT_Bitmap      bitmap = glyph->bitmap;

    destWidth  = bitmap.width;
    destHeight = bitmap.rows;
    
    if( destWidth && destHeight)
    {
        glPushClientAttrib( GL_CLIENT_PIXEL_STORE_BIT);
        glPixelStorei( GL_UNPACK_LSB_FIRST, GL_FALSE);
        glPixelStorei( GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei( GL_UNPACK_ALIGNMENT, 1);

        glBindTexture( GL_TEXTURE_2D, glTextureID);
        glTexSubImage2D( GL_TEXTURE_2D, 0, xOffset, yOffset, destWidth, destHeight, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.buffer);

        glPopClientAttrib();
    }


//      0    
//      +----+
//      |    |
//      |    |
//      |    |
//      +----+
//           1
    
    uv[0].x = static_cast<float>(xOffset) / static_cast<float>(width);
    uv[0].y = static_cast<float>(yOffset) / static_cast<float>(height);
    uv[1].x = static_cast<float>( xOffset + destWidth) / static_cast<float>(width);
    uv[1].y = static_cast<float>( yOffset + destHeight) / static_cast<float>(height);
    
    pos.x = glyph->bitmap_left;
    pos.y = glyph->bitmap_top;
}


FTTextureGlyph::~FTTextureGlyph()
{}

#include <math.h>

float FTTextureGlyph::Render( const FTPoint& pen)
{
	float dx;
	
    glGetIntegerv( GL_TEXTURE_2D_BINDING_EXT, &activeTextureID);
    if( activeTextureID != glTextureID)
    {
        glBindTexture( GL_TEXTURE_2D, (GLuint)glTextureID);
    }
    
	dx= floor( (pen.x + pos.x ) );
	
    glBegin( GL_QUADS);
        glTexCoord2f( uv[0].x, uv[0].y);
        glVertex2f( dx,             pen.y + pos.y);

        glTexCoord2f( uv[0].x, uv[1].y);
        glVertex2f( dx,             pen.y + pos.y - destHeight);

        glTexCoord2f( uv[1].x, uv[1].y);
        glVertex2f( dx + destWidth, pen.y + pos.y - destHeight);
        
        glTexCoord2f( uv[1].x, uv[0].y);
        glVertex2f( dx + destWidth, pen.y + pos.y);
    glEnd();

    return advance;
}

