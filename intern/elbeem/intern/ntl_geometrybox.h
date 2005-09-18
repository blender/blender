/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A simple box object
 *
 *****************************************************************************/

#ifndef NTL_GEOBOX_HH
#define NTL_GEOBOX_HH

#include "ntl_geometryobject.h"


/*! A simple box object generatedd by 12 triangles */
class ntlGeometryBox : public ntlGeometryObject 
{

	public:
		/* Init constructor */
		ntlGeometryBox( void );
		/* Init constructor */
		//ntlGeometryBox( ntlVec3Gfx start, ntlVec3Gfx end );

		//! Return type id
		virtual int getTypeId() { return GEOCLASSTID_BOX; }

		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId );

		/*! for easy GUI detection get start of axis aligned bounding box, return NULL of no BB */
		virtual inline ntlVec3Gfx *getBBStart() 	{ return &mvStart; }
		virtual inline ntlVec3Gfx *getBBEnd() 		{ return &mvEnd; }

		/*! Init refinement attribute */
		virtual void initialize(ntlRenderGlobals *glob);

	private:

		/*! Start and end points of box */
		ntlVec3Gfx mvStart, mvEnd;

		/*! refinement factor */
		int mRefinement;


	public:

		/* Access methods */
		/*! Access start vector */
		inline ntlVec3Gfx getStart( void ){ return mvStart; }
		inline void setStart( const ntlVec3Gfx &set ){ mvStart = set; }
		/*! Access end vector */
		inline ntlVec3Gfx getEnd( void ){ return mvEnd; }
		inline void setEnd( const ntlVec3Gfx &set ){ mvEnd = set; }

};



#endif
