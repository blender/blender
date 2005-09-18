/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a single triangle
 *
 *****************************************************************************/
#ifndef NTL_TRIANGLE_HH
#define NTL_TRIANGLE_HH


#include "ntl_vector3dim.h"
#include "ntl_material.h"
class ntlRay;


/*! Triangle flag defines */
#define TRI_GEOMETRY      (1<<0)
#define TRI_CASTSHADOWS   (1<<1)
#define TRI_MAKECAUSTICS  (1<<2)
#define TRI_NOCAUSTICS    (1<<3)


class ntlTriangle
{
public:
  /* CONSTRUCTORS */
  /*! Default constructor */
  inline ntlTriangle( void );
  /*! Constructor with parameters */
  inline ntlTriangle(int *p, bool smooth, int obj, ntlVec3Gfx norm, int setflags);
  /*! Copy - Constructor */
  inline ntlTriangle(const ntlTriangle &tri);
  /*! Destructor */
  inline ~ntlTriangle() {}

	/* Access methods */

	/*! Acces to points of triangle */
	inline int *getPoints( void ) { return mPoints; }
	/*! Acces normal smoothing */
	inline bool getSmoothNormals( void ) const { return mSmoothNormals; }
	inline void setSmoothNormals( bool set){ mSmoothNormals = set; }
	/*! Access object */
	inline int getObjectId( void ) const { return mObjectId; }
	inline void setObjectId( int set) { mObjectId = set; }
	/*! Acces normal index */
	inline ntlVec3Gfx getNormal( void ) const { return mNormal; }
	inline void setNormal( ntlVec3Gfx set ) { mNormal = set; }
	/*! Acces flags */
	inline int getFlags( void ) const { return mFlags; }
	inline void setFlags( int set ) { mFlags = set; }
	/*! Access last intersection ray ID */
	inline int  getLastRay( void ) const { return mLastRay; }
	inline void setLastRay( int set ) { mLastRay = set; }
	/*! Acces bbox id */
	inline int getBBoxId( void ) const { return mBBoxId; }
	inline void setBBoxId( int set ) { mBBoxId = set; }

	/*! Get average of the three points for this axis */
	inline gfxReal getAverage( int axis ) const;

	/*! operator < for sorting, uses global sorting axis */
	inline friend bool operator<(const ntlTriangle &lhs, const ntlTriangle &rhs);
	/*! operator > for sorting, uses global sorting axis */
	inline friend bool operator>(const ntlTriangle &lhs, const ntlTriangle &rhs);

protected:

private:

	/*! indices to the three points of the triangle */
	int mPoints[3];

	/*! bounding box id (for tree generation), -1 if invalid */
	int mBBoxId;

	/*! Should the normals of this triangle get smoothed? */
	bool mSmoothNormals;

	/*! Id of parent object */
	int mObjectId;

	/*! Index to normal (for not smooth triangles) */
	//int mNormalIndex; ??
	ntlVec3Gfx mNormal;

	/*! Flags for object attributes cast shadows, make caustics etc. */
	int mFlags;

	/*! ID of last ray that an intersection was calculated for */
	int mLastRay;

};


	

/******************************************************************************
 * Default Constructor
 *****************************************************************************/
ntlTriangle::ntlTriangle( void ) :
	mBBoxId(-1),
	mLastRay( 0 )
{
	mPoints[0] = mPoints[1] = mPoints[2] = 0;
	mSmoothNormals = 0;
	mObjectId = 0;
	mNormal = ntlVec3Gfx(0.0);
	mFlags = 0;
}


/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlTriangle::ntlTriangle(int *p, bool smooth, int obj, ntlVec3Gfx norm, int setflags) :
	mBBoxId(-1),
	mLastRay( 0 )
{
	mPoints[0] = p[0];
	mPoints[1] = p[1];
	mPoints[2] = p[2];
	mSmoothNormals = smooth;
	mObjectId = obj;
	mNormal = norm;
	mFlags = setflags;
}


/******************************************************************************
 * Copy Constructor
 *****************************************************************************/
ntlTriangle::ntlTriangle(const ntlTriangle &tri) :
	mBBoxId(-1),
	mLastRay( 0 )
{
	mPoints[0] = tri.mPoints[0];
	mPoints[1] = tri.mPoints[1];
	mPoints[2] = tri.mPoints[2];
	mSmoothNormals = tri.mSmoothNormals;
	mObjectId      = tri.mObjectId;
	mNormal        = tri.mNormal;
	mFlags         = tri.mFlags;
}




/******************************************************************************
 * Triangle sorting functions
 *****************************************************************************/

/* variables imported from ntl_bsptree.cc, necessary for using the stl sort funtion */
/* Static global variable for sorting direction */
extern int globalSortingAxis;
/* Access to points array for sorting */
extern vector<ntlVec3Gfx> *globalSortingPoints;
	

gfxReal ntlTriangle::getAverage( int axis ) const
{ 
	return ( ( (*globalSortingPoints)[ mPoints[0] ][axis] + 
						 (*globalSortingPoints)[ mPoints[1] ][axis] + 
						 (*globalSortingPoints)[ mPoints[2] ][axis] )/3.0);
}

bool operator<(const ntlTriangle &lhs,const ntlTriangle &rhs)
{
	return ( lhs.getAverage(globalSortingAxis) < 
					 rhs.getAverage(globalSortingAxis) );
}

bool operator>(const ntlTriangle &lhs,const ntlTriangle &rhs)
{
	return ( lhs.getAverage(globalSortingAxis) > 
					 rhs.getAverage(globalSortingAxis) );
}


#endif

