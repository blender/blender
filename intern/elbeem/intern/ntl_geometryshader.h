/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Interface for a geometry shader
 *
 *****************************************************************************/
#ifndef NTL_GEOMETRYSHADER_H
#define NTL_GEOMETRYSHADER_H

#include "ntl_geometryclass.h"
class ntlGeometryObject;
class ntlRenderGlobals;

class ntlGeometryShader : 
	public ntlGeometryClass
{

	public:

		//! Default constructor
		inline ntlGeometryShader() :
			ntlGeometryClass() {};
		//! Default destructor
		virtual ~ntlGeometryShader() {};

		//! Return type id
		virtual int getTypeId() { return GEOCLASSTID_SHADER; }

		/*! Initialize object, should return !=0 upon error */
		virtual int initializeShader() = 0;

		/*! Do further object initialization after all geometry has been constructed, should return !=0 upon error */
		virtual int postGeoConstrInit(ntlRenderGlobals *glob) { glob=NULL; /*unused*/ return 0; };

		/*! Get start iterator for all objects */
		virtual std::vector<ntlGeometryObject *>::iterator getObjectsBegin() { return mObjects.begin(); }
		/*! Get end iterator for all objects */
		virtual std::vector<ntlGeometryObject *>::iterator getObjectsEnd() { return mObjects.end(); }

	protected:

		//! vector for the objects
		std::vector<ntlGeometryObject *> mObjects;

};

#endif

