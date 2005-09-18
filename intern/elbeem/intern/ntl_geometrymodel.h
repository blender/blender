/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A model laoded from Wavefront .obj file
 *
 *****************************************************************************/
#ifndef NTL_GEOMODEL_H
#define NTL_GEOMODEL_H

#include "ntl_geometryobject.h"

/*! A simple box object generatedd by 12 triangles */
class ntlGeometryObjModel : public ntlGeometryObject
{
	public:
		/* Init constructor */
		ntlGeometryObjModel( void );
		/* Init constructor */
		//ntlGeometryObjModel( ntlVec3Gfx start, ntlVec3Gfx end );
		/* Destructor */
		virtual ~ntlGeometryObjModel( void );

		//! Return type id
		virtual int getTypeId() { return GEOCLASSTID_OBJMODEL; }

		/*! Filename setting etc. */
		virtual void initialize(ntlRenderGlobals *glob);


		/* create triangles from obj */
		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId );

		/*! load model from .bobj file, returns !=0 upon error */
		int loadBobjModel(string filename);

	private:

		/*! Start and end points of box */
		ntlVec3Gfx mvStart, mvEnd;

		/*! was the model loaded? */
		bool mLoaded;

		/*! filename of the obj file */
		string mFilename;

		/*! for bobj models */
		vector<int> mTriangles;
		vector<ntlVec3Gfx> mVertices;
		vector<ntlVec3Gfx> mNormals;

	public:

		/* Access methods */
		/*! Access start vector */
		inline ntlVec3Gfx getStart( void ){ return mvStart; }
		inline void setStart( const ntlVec3Gfx &set ){ mvStart = set; }
		/*! Access end vector */
		inline ntlVec3Gfx getEnd( void ){ return mvEnd; }
		inline void setEnd( const ntlVec3Gfx &set ){ mvEnd = set; }

		/*! set data file name */
		inline void setFilename(string set) { mFilename = set; }
};

#endif

