#ifndef		__FTOutlineGlyph__
#define		__FTOutlineGlyph__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "FTGL.h"
#include "FTGlyph.h"

class FTVectoriser;


/**
 * FTOutlineGlyph is a specialisation of FTGlyph for creating outlines.
 * 
 * @see FTGlyphContainer
 * @see FTVectoriser
 *
 */
class FTGL_EXPORT FTOutlineGlyph : public FTGlyph
{
    public:
        /**
         * Constructor. Sets the Error to Invalid_Outline if the glyphs isn't an outline.
         *
         * @param glyph The Freetype glyph to be processed
         */
        FTOutlineGlyph( FT_GlyphSlot glyph);

        /**
         * Destructor
         */
        virtual ~FTOutlineGlyph();

        /**
         * Renders this glyph at the current pen position.
         *
         * @param pen	The current pen position.
         * @return		The advance distance for this glyph.
         */
        virtual float Render( const FTPoint& pen);
        
    private:		
        /**
         * OpenGL display list
         */
        GLuint glList;
	
};


#endif	//	__FTOutlineGlyph__

