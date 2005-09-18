/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A simple sphere object
 *
 *****************************************************************************/

#ifndef NTL_GEOSPHERE_H

#include "ntl_geometryobject.h"


/*! A simple box object generatedd by 12 triangles */
class ntlGeometrySphere : public ntlGeometryObject 
{

	public:
		/* Init constructor */
		ntlGeometrySphere( void );

		//! Return type id
		virtual int getTypeId() { return GEOCLASSTID_SPHERE; }

		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId );

		/*! for easy GUI detection get start of axis aligned bounding box, return NULL of no BB */
		virtual inline ntlVec3Gfx *getBBStart() 	{ return &mvBBStart; }
		virtual inline ntlVec3Gfx *getBBEnd() 		{ return &mvBBEnd; }

		/*! Init refinement attribute */
		virtual void initialize(ntlRenderGlobals *glob);

	private:

		/*! Center of the sphere */
		ntlVec3Gfx mvCenter;

		/*! radius */
		gfxReal mRadius;

		/*! refinement factor along polar angle */
		int mRefPolar;
		/*! refinement factor per segment (azimuthal angle) */
		int mRefAzim;

		/*! Start and end points of bounding box */
		ntlVec3Gfx mvBBStart, mvBBEnd;

	public:

		/* Access methods */
		/*! Access start vector */
		inline ntlVec3Gfx getCenter( void ){ return mvCenter; }
		inline void setCenter( const ntlVec3Gfx &set ){ mvCenter = set; }

};



#define NTL_GEOSPHERE_H
#endif
