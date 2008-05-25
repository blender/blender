#ifndef POINT_H
#define POINT_H

typedef int QCOORD; 

class Point
{
public:
    Point();
    Point( int xpos, int ypos );

    bool   isNull()	const;

    int	   x()		const;
    int	   y()		const;
    void   setX( int x );
    void   setY( int y );

    int manhattanLength() const;

    QCOORD &rx();
    QCOORD &ry();

    Point &operator+=( const Point &p );
    Point &operator-=( const Point &p );
    Point &operator*=( int c );
    Point &operator*=( double c );
    Point &operator/=( int c );
    Point &operator/=( double c );

    friend inline bool	 operator==( const Point &, const Point & );
    friend inline bool	 operator!=( const Point &, const Point & );
    friend inline const Point operator+( const Point &, const Point & );
    friend inline const Point operator-( const Point &, const Point & );
    friend inline const Point operator*( const Point &, int );
    friend inline const Point operator*( int, const Point & );
    friend inline const Point operator*( const Point &, double );
    friend inline const Point operator*( double, const Point & );
    friend inline const Point operator-( const Point & );
    friend inline const Point operator/( const Point &, int );
    friend inline const Point operator/( const Point &, double );

private:
    QCOORD xp;
    QCOORD yp;
};

static void warningDivByZero() {
	// cout << "warning: dividing by zero"
}


/*****************************************************************************
  Point inline functions
 *****************************************************************************/

inline Point::Point()
{ xp=0; yp=0; }

inline Point::Point( int xpos, int ypos )
{ xp=(QCOORD)xpos; yp=(QCOORD)ypos; }

inline bool Point::isNull() const
{ return xp == 0 && yp == 0; }

inline int Point::x() const
{ return xp; }

inline int Point::y() const
{ return yp; }

inline void Point::setX( int x )
{ xp = (QCOORD)x; }

inline void Point::setY( int y )
{ yp = (QCOORD)y; }

inline QCOORD &Point::rx()
{ return xp; }

inline QCOORD &Point::ry()
{ return yp; }

inline Point &Point::operator+=( const Point &p )
{ xp+=p.xp; yp+=p.yp; return *this; }

inline Point &Point::operator-=( const Point &p )
{ xp-=p.xp; yp-=p.yp; return *this; }

inline Point &Point::operator*=( int c )
{ xp*=(QCOORD)c; yp*=(QCOORD)c; return *this; }

inline Point &Point::operator*=( double c )
{ xp=(QCOORD)(xp*c); yp=(QCOORD)(yp*c); return *this; }

inline bool operator==( const Point &p1, const Point &p2 )
{ return p1.xp == p2.xp && p1.yp == p2.yp; }

inline bool operator!=( const Point &p1, const Point &p2 )
{ return p1.xp != p2.xp || p1.yp != p2.yp; }

inline const Point operator+( const Point &p1, const Point &p2 )
{ return Point(p1.xp+p2.xp, p1.yp+p2.yp); }

inline const Point operator-( const Point &p1, const Point &p2 )
{ return Point(p1.xp-p2.xp, p1.yp-p2.yp); }

inline const Point operator*( const Point &p, int c )
{ return Point(p.xp*c, p.yp*c); }

inline const Point operator*( int c, const Point &p )
{ return Point(p.xp*c, p.yp*c); }

inline const Point operator*( const Point &p, double c )
{ return Point((QCOORD)(p.xp*c), (QCOORD)(p.yp*c)); }

inline const Point operator*( double c, const Point &p )
{ return Point((QCOORD)(p.xp*c), (QCOORD)(p.yp*c)); }

inline const Point operator-( const Point &p )
{ return Point(-p.xp, -p.yp); }

inline Point &Point::operator/=( int c )
{
    if ( c == 0 )
		warningDivByZero();

    xp/=(QCOORD)c;
    yp/=(QCOORD)c;
    return *this;
}

inline Point &Point::operator/=( double c )
{
    if ( c == 0.0 )
		warningDivByZero();

    xp=(QCOORD)(xp/c);
    yp=(QCOORD)(yp/c);
    return *this;
}

inline const Point operator/( const Point &p, int c )
{
    if ( c == 0 )
		warningDivByZero();
	
    return Point(p.xp/c, p.yp/c);
}

inline const Point operator/( const Point &p, double c )
{
    if ( c == 0.0 )
		warningDivByZero();

    return Point((QCOORD)(p.xp/c), (QCOORD)(p.yp/c));
}

#endif // POINT_H