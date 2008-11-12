/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * ray class
 *
 *****************************************************************************/
#ifndef NTL_RAY_H
#define NTL_RAY_H

#include <sstream>
#include "ntl_vector3dim.h"
#include "ntl_lighting.h"
#include "ntl_geometryobject.h"
#include "ntl_bsptree.h"

class ntlTriangle;
class ntlRay;
class ntlTree;
class ntlScene;
class ntlRenderGlobals;
class ntlGeometryObject;

//! store data for an intersection of a ray and a triangle
// NOT YET USED
class ntlIntersection {
	public:

		ntlIntersection() :
			distance(-1.0), normal(0.0),
			ray(NULL), tri(NULL), flags(0) { };

		gfxReal distance;
		ntlVec3Gfx normal;
		ntlRay *ray; 
		ntlTriangle *tri;
		char flags;
};

//! the main ray class
class ntlRay
{
public:
  // CONSTRUCTORS
  //! Initialize ray memebers, prints error message
  ntlRay();
  //! Copy constructor, copy all members
  ntlRay(const ntlRay &r);
  //! Explicitly init member variables with global render object
  ntlRay(const ntlVec3Gfx &o, const ntlVec3Gfx &d, unsigned int i, gfxReal contrib, ntlRenderGlobals *glob);
  //! Destructor
  ~ntlRay();

  //! Set the refraction flag for refracted rays
  inline void setRefracted(unsigned char set) { mIsRefracted = set; }
  inline void setReflected(unsigned char set) { mIsReflected = set; }

  //! main ray recursion function
  /*!
   * First get closest object intersection, return background color if nothing
   * was hit, else calculate shading and reflection components 
   * and return mixed color */
  const ntlColor shade() /*const*/;

	/*! Trace a photon through the scene */
	void tracePhoton(ntlColor) const;

  //! intersect ray with AABB
  void intersectFrontAABB(ntlVec3Gfx mStart, ntlVec3Gfx mEnd, gfxReal &t, ntlVec3Gfx &normal, ntlVec3Gfx &retcoord) const;
  void intersectBackAABB(ntlVec3Gfx mStart, ntlVec3Gfx mEnd, gfxReal &t, ntlVec3Gfx &normal, ntlVec3Gfx &retcoord) const;
  void intersectCompleteAABB(ntlVec3Gfx mStart, ntlVec3Gfx mEnd, gfxReal &tmin, gfxReal &tmax) const;
	// intersection routines in bsptree.cpp
  //! optimized intersect ray with triangle
  inline void intersectTriangle(vector<ntlVec3Gfx> *mpV, ntlTriangle *tri, gfxReal &t, gfxReal &u, gfxReal &v) const;
  //! optimized intersect ray with triangle along +X axis dir
  inline void intersectTriangleX(vector<ntlVec3Gfx> *mpV, ntlTriangle *tri, gfxReal &t, gfxReal &u, gfxReal &v) const;
  //! intersect only with front side
  inline void intersectTriangleFront(vector<ntlVec3Gfx> *mpV, ntlTriangle *tri, gfxReal &t, gfxReal &u, gfxReal &v) const;
  //! intersect ray only with backsides
  inline void intersectTriangleBack(vector<ntlVec3Gfx> *mpV, ntlTriangle *tri, gfxReal &t, gfxReal &u, gfxReal &v) const;

  // access methods
  //! Returns the ray origin
  inline ntlVec3Gfx getOrigin() const { return ntlVec3Gfx(mOrigin); }
  //! Returns the ray direction
  inline ntlVec3Gfx getDirection() const { return ntlVec3Gfx(mDirection); }
  /*! Returns the ray relfection normal */
  inline ntlVec3Gfx getNormal() const { return ntlVec3Gfx(mvNormal); }
		//! Is this ray refracted?
  inline unsigned char getRefracted() const  { return mIsRefracted; }
  inline unsigned char getReflected() const  { return mIsReflected; }
  /*! Get position along ray */
  inline ntlVec3Gfx getPositionAt(gfxReal t) const { return (mOrigin+(mDirection*t)); }
	/*! Get render globals pointer of this ray */
	inline ntlRenderGlobals *getRenderglobals( void ) const { return mpGlob; }
	/*! get this ray's ID */
	inline int getID( void ) const { return mID; }

  /*! Set origin of this ray */
  inline void setOrigin(ntlVec3Gfx set) { mOrigin = set; }
	/*! Set direction of this ray */
  inline void setDirection(ntlVec3Gfx set) { mDirection = set; }
  /*! Set normal of this ray */
  inline void setNormal(ntlVec3Gfx set) { mvNormal = set; }

protected:
  /* Calulates the Lambertian and Specular color for
   * the given reflection and returns it */
  const ntlColor getShadedColor(ntlLightObject *light, const ntlRay &reflectedray, 
																const ntlVec3Gfx &normal, ntlMaterial *surf) const;
  
private:
  /*! Origin of ray */
  ntlVec3Gfx     mOrigin;
  /*! Normalized direction vector of ray */
  ntlVec3Gfx     mDirection;
  /*! For reflected/refracted rays, the normal is stored here */
  ntlVec3Gfx     mvNormal;
  /*! recursion depth */
  unsigned int mDepth;
	/*! How much does this ray contribute to the surface color? abort if too small */
	gfxReal mContribution;

  /*! Global rendering settings */
  ntlRenderGlobals *mpGlob;

  /*! If this ray is a refracted one, this flag has to be set
   *  This is necessary to for example also give the background color
   *  to refracted rays. Otherwise things may look strange... 
   */
  unsigned char mIsRefracted;
  unsigned char mIsReflected;

	/*! ID of this ray (from renderglobals */
	int mID;

};


/******************************************************************************
 *
 * a single triangle
 *
 *****************************************************************************/

// triangle intersection code in bsptree.cpp
// intersectTriangle(vector<ntlVec3Gfx> *mpV, ntlTriangle *tri, gfxReal &t, gfxReal &u, gfxReal &v);

/*! Triangle flag defines */
#define TRI_GEOMETRY      (1<<0)
#define TRI_CASTSHADOWS   (1<<1)


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

	/*! Flags for object attributes cast shadows */
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



/******************************************************************************
 *
 * Scene object, that contains and manages all geometry objects
 *
 *****************************************************************************/



class ntlScene
{
public:
  /* CONSTRUCTORS */
  /*! Default constructor */
  ntlScene( ntlRenderGlobals *glob, bool del=true );
  /*! Default destructor  */
  ~ntlScene();

	/*! Add an object to the scene */
	inline void addGeoClass(ntlGeometryClass *geo) { 
		mGeos.push_back( geo ); 
		geo->setObjectId(mGeos.size());
	}
	/*! Add a geo object to the scene, warning - only needed for hand init */
	inline void addGeoObject(ntlGeometryObject *geo) { mObjects.push_back( geo ); }

	/*! Acces a certain object */
	inline ntlGeometryObject *getObject(int id) { 
		if(!mSceneBuilt) { errFatal("ntlScene::getObject","Scene not inited!", SIMWORLD_INITERROR); }
		return mObjects[id]; }

	/*! Acces object array */
	inline vector<ntlGeometryObject*> *getObjects() { 
		if(!mSceneBuilt) { errFatal("ntlScene::getObjects[]","Scene not inited!", SIMWORLD_INITERROR); }
		return &mObjects; }

	/*! Acces geo class array */
	inline vector<ntlGeometryClass*> *getGeoClasses() { 
		if(!mSceneBuilt) { errFatal("ntlScene::getGeoClasses[]","Scene not inited!", SIMWORLD_INITERROR); }
		return &mGeos; }

	/*! draw scene with opengl */
	//void draw();
	
	/*! Build/first init the scene arrays */
	void buildScene(double time, bool firstInit);
	
	//! Prepare the scene triangles and maps for raytracing
	void prepareScene(double time);
	//! Do some memory cleaning, when frame is finished
	void cleanupScene( void );

	/*! Intersect a ray with the scene triangles */
	void intersectScene(const ntlRay &r, gfxReal &distance, ntlVec3Gfx &normal, ntlTriangle *&tri, int flags) const;

	/*! return a vertex */
	ntlVec3Gfx getVertex(int index) { return mVertices[index]; } 

	// for tree generation 
	/*! return pointer to vertices vector */
	vector<ntlVec3Gfx> *getVertexPointer( void ) { return &mVertices; }
	/*! return pointer to vertices vector */
	vector<ntlVec3Gfx> *getVertexNormalPointer( void ) { return &mVertNormals; }
	/*! return pointer to vertices vector */
	vector<ntlTriangle> *getTrianglePointer( void ) { return &mTriangles; }

private:

	/*! Global settings */
	ntlRenderGlobals *mpGlob;

	/*! free objects? (only necessary for render scene, which  contains all) */
	bool mSceneDel;

  /*! List of geometry classes */
  vector<ntlGeometryClass *> mGeos;

  /*! List of geometry objects */
  vector<ntlGeometryObject *> mObjects;

  /*! List of triangles */
  vector<ntlTriangle> mTriangles;
  /*! List of vertices */
  vector<ntlVec3Gfx>  mVertices;
  /*! List of normals */
  vector<ntlVec3Gfx>  mVertNormals;
  /*! List of triangle normals */
  vector<ntlVec3Gfx>  mTriangleNormals;

	/*! Tree to store quickly intersect triangles */
	ntlTree *mpTree;

	/*! id of dislpay list for raytracer stuff */
	int mDisplayListId;

	/*! was the scene successfully built? only then getObject(i) requests are valid */
	bool mSceneBuilt;

	/*! shader/obj initializations are only done on first init */
	bool mFirstInitDone;

};


#endif

