#ifndef     __FTPoint__
#define     __FTPoint__

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "FTGL.h"

/**
 * FTPoint class is a basic 3 dimensional point or vector.
 */
class FTGL_EXPORT FTPoint
{
    public:
        /**
         * Default constructor. Point is set to zero.
         */
        FTPoint()
        : x(0), y(0), z(0)
        {}
        
        /**
         * Constructor.
         *
         * @param X
         * @param Y
         * @param Z
         */
        FTPoint( const FTGL_DOUBLE X, const FTGL_DOUBLE Y, const FTGL_DOUBLE Z)
        : x(X), y(Y), z(Z)
        {}
        
        /**
         * Constructor. This converts an FT_Vector to an FT_Point
         *
         * @param ft_vector A freetype vector
         */
        FTPoint( const FT_Vector& ft_vector)
        : x(ft_vector.x), y(ft_vector.y), z(0)
        {}
        
        /**
         * Operator +=
         *
         * @param point
         * @return this plus point.
         */
        FTPoint& operator += ( const FTPoint& point)
        {
            x += point.x;
            y += point.y;
            z += point.z;
        
            return *this;
        }

        /**
         * Operator == Tests for eqaulity
         *
         * @param a
         * @param b
         * @return
         */
        friend bool operator == ( const FTPoint &a, const FTPoint &b);

        /**
         * Operator != Tests for non equality
         *
         * @param a
         * @param b
         * @return
         */
        friend bool operator != ( const FTPoint &a, const FTPoint &b);
        
        /**
         * The point data
         */
        FTGL_DOUBLE x, y, z; // FIXME make private
        
    private:
};

#endif  //  __FTPoint__

