#include "FTFace.h"
#include "FTLibrary.h"

#include FT_TRUETYPE_TABLES_H

FTFace::FTFace( const char* filename)
:   numGlyphs(0),
    fontEncodingList(0),
    err(0)
{
    const FT_Long DEFAULT_FACE_INDEX = 0;
    ftFace = new FT_Face;

    err = FT_New_Face( *FTLibrary::Instance().GetLibrary(), filename, DEFAULT_FACE_INDEX, ftFace);

    if( err)
    {
        delete ftFace;
        ftFace = 0;
    }
    else
    {
        numGlyphs = (*ftFace)->num_glyphs;
    }
}


FTFace::FTFace( const unsigned char *pBufferBytes, size_t bufferSizeInBytes)
:   numGlyphs(0),
    err(0)
{
    const FT_Long DEFAULT_FACE_INDEX = 0;
    ftFace = new FT_Face;

    err = FT_New_Memory_Face( *FTLibrary::Instance().GetLibrary(), (FT_Byte *)pBufferBytes, bufferSizeInBytes, DEFAULT_FACE_INDEX, ftFace);

    if( err)
    {
        delete ftFace;
        ftFace = 0;
    }
    else
    {
        numGlyphs = (*ftFace)->num_glyphs;
    }
}


FTFace::~FTFace()
{
    Close();
}


bool FTFace::Attach( const char* filename)
{
    err = FT_Attach_File( *ftFace, filename);
    return !err;
}


bool FTFace::Attach( const unsigned char *pBufferBytes, size_t bufferSizeInBytes)
{
    FT_Open_Args open;

    open.flags = FT_OPEN_MEMORY;
    open.memory_base = (FT_Byte *)pBufferBytes;
    open.memory_size = bufferSizeInBytes;

    err = FT_Attach_Stream( *ftFace, &open);
    return !err;
}


void FTFace::Close()
{
    if( ftFace)
    {
        FT_Done_Face( *ftFace);
        delete ftFace;
        ftFace = 0;
    }
}


const FTSize& FTFace::Size( const unsigned int size, const unsigned int res)
{
    charSize.CharSize( ftFace, size, res, res);
    err = charSize.Error();

    return charSize;
}


unsigned int FTFace::CharMapCount()
{
    return (*ftFace)->num_charmaps;
}


FT_Encoding* FTFace::CharMapList()
{
    if( 0 == fontEncodingList)
    {
        fontEncodingList = new FT_Encoding[CharMapCount()];
        for( size_t encodingIndex = 0; encodingIndex < CharMapCount(); ++encodingIndex)
        {
            fontEncodingList[encodingIndex] = (*ftFace)->charmaps[encodingIndex]->encoding;
        }
    }
    
    return fontEncodingList;
}


unsigned int FTFace::UnitsPerEM() const
{
    return (*ftFace)->units_per_EM;
}


FTPoint FTFace::KernAdvance( unsigned int index1, unsigned int index2)
{
    float x, y;
    x = y = 0.0f;

    if( FT_HAS_KERNING((*ftFace)) && index1 && index2)
    {
        FT_Vector kernAdvance;
        kernAdvance.x = kernAdvance.y = 0;

        err = FT_Get_Kerning( *ftFace, index1, index2, ft_kerning_unfitted, &kernAdvance);
        if( !err)
        {   
            x = static_cast<float>( kernAdvance.x) / 64.0f;
            y = static_cast<float>( kernAdvance.y) / 64.0f;
        }
    }
    
    return FTPoint( x, y, 0.0);
}


FT_GlyphSlot FTFace::Glyph( unsigned int index, FT_Int load_flags)
{
    err = FT_Load_Glyph( *ftFace, index, load_flags);
    if( err)
    {
        return NULL;
    }

    return (*ftFace)->glyph;
}

