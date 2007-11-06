#ifndef     __FTExtrdGlyph__
#define     __FTExtrdGlyph__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "FTGL.h"
#include "FTGlyph.h"

class FTVectoriser;

/**
 * FTExtrdGlyph is a specialisation of FTGlyph for creating tessellated
 * extruded polygon glyphs.
 * 
 * @see FTGlyphContainer
 * @see FTVectoriser
 *
 */
class FTGL_EXPORT FTExtrdGlyph : public FTGlyph
{
    public:
        /**
         * Constructor. Sets the Error to Invalid_Outline if the glyphs isn't an outline.
         *
         * @param glyph The Freetype glyph to be processed
         * @param depth The distance along the z axis to extrude the glyph
         */
        FTExtrdGlyph( FT_GlyphSlot glyph, float depth);

        /**
         * Destructor
         */
        virtual ~FTExtrdGlyph();

        /**
         * Renders this glyph at the current pen position.
         *
         * @param pen   The current pen position.
         * @return      The advance distance for this glyph.
         */
        virtual float Render( const FTPoint& pen);
        
    private:
        /**
         * Calculate the normal vector to 2 points. This is 2D and ignores
         * the z component. The normal will be normalised
         *
         * @param a
         * @param b
         * @return
         */
        FTPoint GetNormal( const FTPoint &a, const FTPoint &b);
        
        
        /**
         * OpenGL display list
         */
        GLuint glList;
        
        /**
         * Distance to extrude the glyph
         */
        float depth;
    
};


#endif  //  __FTExtrdGlyph__

