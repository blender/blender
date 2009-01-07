#include    "FTBufferGlyph.h"

FTBufferGlyph::FTBufferGlyph( FT_GlyphSlot glyph, unsigned char* b)
:   FTGlyph( glyph),
    destWidth(0),
    destHeight(0),
    data(0),
    buffer(b)
{
    err = FT_Render_Glyph( glyph, FT_RENDER_MODE_NORMAL);
    if( err || ft_glyph_format_bitmap != glyph->format)
    {
        return;
    }

    FT_Bitmap bitmap = glyph->bitmap;

    unsigned int srcWidth = bitmap.width;
    unsigned int srcHeight = bitmap.rows;
    unsigned int srcPitch = bitmap.pitch;
    
    destWidth = srcWidth;
    destHeight = srcHeight;
    destPitch = srcPitch;    

    if( destWidth && destHeight)
    {
        data = new unsigned char[destPitch * destHeight];
        unsigned char* dest = data + (( destHeight - 1) * destPitch);

        unsigned char* src = bitmap.buffer;

        for( unsigned int y = 0; y < srcHeight; ++y)
        {
            memcpy( dest, src, srcPitch);
            dest -= destPitch;
            src += srcPitch;
        }
    }

    pos.x = glyph->bitmap_left;
    pos.y = srcHeight - glyph->bitmap_top;
}


FTBufferGlyph::~FTBufferGlyph()
{
    delete [] data;
}


float FTBufferGlyph::Render( const FTPoint& pen)
{
    if( data && buffer)
    {
    }

    return advance;
}
