#include "FTPolyGlyph.h"
#include "FTVectoriser.h"


FTPolyGlyph::FTPolyGlyph( FT_GlyphSlot glyph)
:   FTGlyph( glyph),
    glList(0)
{
    if( ft_glyph_format_outline != glyph->format)
    {
        err = 0x14; // Invalid_Outline
        return;
    }

    FTVectoriser vectoriser( glyph);

    if(( vectoriser.ContourCount() < 1) || ( vectoriser.PointCount() < 3))
    {
        return;
    }

    vectoriser.MakeMesh( 1.0);
    
    glList = glGenLists( 1);
    glNewList( glList, GL_COMPILE);

        const FTMesh* mesh = vectoriser.GetMesh();
        for( unsigned int index = 0; index < mesh->TesselationCount(); ++index)
        {
            const FTTesselation* subMesh = mesh->Tesselation( index);
            unsigned int polyonType = subMesh->PolygonType();

            glBegin( polyonType);
                for( unsigned int x = 0; x < subMesh->PointCount(); ++x)
                {
                    glVertex3f( subMesh->Point(x).x / 64.0f,
                                subMesh->Point(x).y / 64.0f,
                                0.0f);
                }
            glEnd();
        }
    glEndList();
}


FTPolyGlyph::~FTPolyGlyph()
{
    glDeleteLists( glList, 1);
}


float FTPolyGlyph::Render( const FTPoint& pen)
{
    if( glList)
    {
        glTranslatef(  pen.x,  pen.y, 0);
        glCallList( glList);    
        glTranslatef( -pen.x, -pen.y, 0);
    }
    
    return advance;
}
