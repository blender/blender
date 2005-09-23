/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * ray class
 *
 *****************************************************************************/
#ifndef NTL_RAY_HH
#define NTL_RAY_HH

#include "ntl_vector3dim.h"
#include "ntl_lightobject.h"
#include "ntl_geometryobject.h"
#include "ntl_renderglobals.h"


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




// triangle intersection code in bsptree.cpp
// intersectTriangle(vector<ntlVec3Gfx> *mpV, ntlTriangle *tri, gfxReal &t, gfxReal &u, gfxReal &v);
// ...



#endif

