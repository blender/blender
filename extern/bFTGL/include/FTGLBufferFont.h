#ifndef     __FTGLBufferFont__
#define     __FTGLBufferFont__


#include "FTFont.h"
#include "FTGL.h"


class FTGlyph;


/**
 * FTGLBufferFont is a specialisation of the FTFont class for handling
 * Pixmap (Grey Scale) fonts
 *
 * @see     FTFont
 */
class FTGL_EXPORT FTGLBufferFont : public FTFont
{
    public:
        /**
         * Open and read a font file. Sets Error flag.
         *
         * @param fontname  font file name.
         */
        FTGLBufferFont( const char* fontname);
        
        /**
         * Open and read a font from a buffer in memory. Sets Error flag.
         *
         * @param pBufferBytes  the in-memory buffer
         * @param bufferSizeInBytes  the length of the buffer in bytes
         */
        FTGLBufferFont( const unsigned char *pBufferBytes, size_t bufferSizeInBytes);
        
        
        void SetClientBuffer( unsigned char* b)
        {
            buffer = b;
        }
        
        
        /**
         * Destructor
         */
        ~FTGLBufferFont();
        
        /**
         * Renders a string of characters
         * 
         * @param string    'C' style string to be output.   
         */
        void Render( const char* string);
        
        /**
         * Renders a string of characters
         * 
         * @param string    wchar_t string to be output.     
         */
        void Render( const wchar_t* string);

    private:
        /**
         * Construct a FTBufferGlyph.
         *
         * @param g The glyph index NOT the char code.
         * @return  An FTBufferGlyph or <code>null</code> on failure.
         */
        inline virtual FTGlyph* MakeGlyph( unsigned int g);
        
        unsigned char* buffer;
};


#endif  //  __FTGLBufferFont__

