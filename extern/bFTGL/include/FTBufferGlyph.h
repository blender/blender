#ifndef     __FTBufferGlyph__
#define     __FTBufferGlyph__


#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "FTGL.h"
#include "FTGlyph.h"


/**
 * FTBufferGlyph is a specialisation of FTGlyph for creating pixmaps.
 * 
 * @see FTGlyphContainer
 *
 */
class  FTGL_EXPORT FTBufferGlyph : public FTGlyph
{
    public:
        /**
         * Constructor
         *
         * @param glyph The Freetype glyph to be processed
         */
        FTBufferGlyph( FT_GlyphSlot glyph, unsigned char* clientBuffer);

        /**
         * Destructor
         */
        virtual ~FTBufferGlyph();

        /**
         * Renders this glyph at the current pen position.
         *
         * @param pen   The current pen position.
         * @return      The advance distance for this glyph.
         */
        virtual float Render( const FTPoint& pen);
        
        // attributes

    private:
        /**
         * The width of the glyph 'image'
         */
        int destWidth;

        /**
         * The height of the glyph 'image'
         */
        int destHeight;

        /**
         * The pitch of the glyph 'image'
         */
        unsigned int destPitch;
        
        /**
         * Vector from the pen position to the topleft corner of the pixmap
         */
        FTPoint pos;
        
        /**
         * Pointer to the 'image' data
         */
        unsigned char* data;
        
        
        unsigned char* buffer;
        
};


#endif  //  __FTBufferGlyph__
