#include    "FTSize.h"


FTSize::FTSize()
:   ftFace(0),
    ftSize(0),
    size(0),
    err(0)
{}


FTSize::~FTSize()
{}


bool FTSize::CharSize( FT_Face* face, unsigned int point_size, unsigned int x_resolution, unsigned int y_resolution )
{
    err = FT_Set_Char_Size( *face, 0L, point_size * 64, x_resolution, y_resolution);

    if( !err)
    {
        ftFace = face;
        size = point_size;
        ftSize = (*ftFace)->size;
    }
    else
    {
        ftFace = 0;
        size = 0;
        ftSize = 0;
    }
    
    return !err;
}


unsigned int FTSize::CharSize() const
{
    return size;
}


float FTSize::Ascender() const
{
    return ftSize == 0 ? 0.0f : static_cast<float>( ftSize->metrics.ascender) / 64.0f;
}


float FTSize::Descender() const
{
    return ftSize == 0 ? 0.0f : static_cast<float>( ftSize->metrics.descender) / 64.0f;
}


float FTSize::Height() const
{
    if( 0 == ftSize)
    {
        return 0.0f;
    }
    
    if( FT_IS_SCALABLE((*ftFace)))
    {
        return ( (*ftFace)->bbox.yMax - (*ftFace)->bbox.yMin) * ( (float)ftSize->metrics.y_ppem / (float)(*ftFace)->units_per_EM);
    }
    else
    {
        return static_cast<float>( ftSize->metrics.height) / 64.0f;
    }
}


float FTSize::Width() const
{
    if( 0 == ftSize)
    {
        return 0.0f;
    }
    
    if( FT_IS_SCALABLE((*ftFace)))
    {
        return ( (*ftFace)->bbox.xMax - (*ftFace)->bbox.xMin) * ( static_cast<float>(ftSize->metrics.x_ppem) / static_cast<float>((*ftFace)->units_per_EM));
    }
    else
    {
        return static_cast<float>( ftSize->metrics.max_advance) / 64.0f;
    }
}


float FTSize::Underline() const
{
    return 0.0f;
}

unsigned int FTSize::XPixelsPerEm() const
{
    return ftSize == 0 ? 0 : ftSize->metrics.x_ppem;
}

unsigned int FTSize::YPixelsPerEm() const
{
    return ftSize == 0 ? 0 : ftSize->metrics.y_ppem;
}

