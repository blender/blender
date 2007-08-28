#include    "FTPixmapGlyph.h"

FTPixmapGlyph::FTPixmapGlyph( FT_GlyphSlot glyph)
:   FTGlyph( glyph),
    destWidth(0),
    destHeight(0),
    data(0)
{
    err = FT_Render_Glyph( glyph, FT_RENDER_MODE_NORMAL);
    if( err || ft_glyph_format_bitmap != glyph->format)
    {
        return;
    }

    FT_Bitmap bitmap = glyph->bitmap;

    //check the pixel mode
    //ft_pixel_mode_grays
        
    int srcWidth = bitmap.width;
    int srcHeight = bitmap.rows;
    
   // FIXME What about dest alignment?
    destWidth = srcWidth;
    destHeight = srcHeight;
    
    if( destWidth && destHeight)
    {
        data = new unsigned char[destWidth * destHeight * 4];
    
        // Get the current glColor.
        float ftglColour[4];
//        glGetFloatv( GL_CURRENT_COLOR, ftglColour);
		ftglColour[0] = ftglColour[1] = ftglColour[2] = ftglColour[3] = 1.0;

        unsigned char redComponent =   static_cast<unsigned char>( ftglColour[0] * 255.0f);
        unsigned char greenComponent = static_cast<unsigned char>( ftglColour[1] * 255.0f);
        unsigned char blueComponent =  static_cast<unsigned char>( ftglColour[2] * 255.0f);

        unsigned char* src = bitmap.buffer;

        unsigned char* dest = data + ((destHeight - 1) * destWidth) * 4;
        size_t destStep = destWidth * 4 * 2;

        if( ftglColour[3] == 1.0f)
        {
            for( int y = 0; y < srcHeight; ++y)
            {
                for( int x = 0; x < srcWidth; ++x)
                {
                    *dest++ = redComponent;
                    *dest++ = greenComponent;
                    *dest++ = blueComponent;
                    *dest++ = *src++;
                }
                dest -= destStep;
            }
        }
        else
        {
            for( int y = 0; y < srcHeight; ++y)
            {
                for( int x = 0; x < srcWidth; ++x)
                {
                    *dest++ = redComponent;
                    *dest++ = greenComponent;
                    *dest++ = blueComponent;
                    *dest++ = static_cast<unsigned char>(ftglColour[3] * *src++);
                }
                dest -= destStep;
            }
        }

        destHeight = srcHeight;
    }

    pos.x = glyph->bitmap_left;
    pos.y = srcHeight - glyph->bitmap_top;
}


FTPixmapGlyph::~FTPixmapGlyph()
{
    delete [] data;
}

#include <math.h>
float FTPixmapGlyph::Render( const FTPoint& pen)
{
    if( data)
    {
		float dx, dy;
		
		dx= floor( (pen.x + pos.x ) );
		dy= ( (pen.y - pos.y ) );
		
        // Move the glyph origin
        glBitmap( 0, 0, 0.0f, 0.0f, dx, dy, (const GLubyte*)0);

        glPixelStorei( GL_UNPACK_ROW_LENGTH, 0);

        glDrawPixels( destWidth, destHeight, GL_RGBA, GL_UNSIGNED_BYTE, (const GLvoid*)data);
        
        // Restore the glyph origin
        glBitmap( 0, 0, 0.0f, 0.0f, -dx, -dy, (const GLubyte*)0);
    }

    return advance;
}
