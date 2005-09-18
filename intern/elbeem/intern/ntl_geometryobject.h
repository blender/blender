/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a geometry object
 * all other geometry objects are derived from this one
 *
 *****************************************************************************/
#ifndef NTL_GEOMETRYOBJECT_HH

#include "ntl_geometryclass.h"
#include "ntl_material.h"
#include "ntl_triangle.h"
class ntlRay;
class ntlRenderGlobals;


class ntlGeometryObject : public ntlGeometryClass
{

	public:
		//! Default constructor
		ntlGeometryObject();
		//! Default destructor
		virtual ~ntlGeometryObject();

		//! Return type id
		virtual int getTypeId() { return GEOCLASSTID_OBJECT; }

		/*! Get the triangles from this object */
		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId ) = 0;

		/*! Init attributes etc. of this object */
		virtual void initialize(ntlRenderGlobals *glob);

		/*! Search the material for this object from the material list */
		void searchMaterial(vector<ntlMaterial *> *mat);

		/* Acces methods */
		/*! Set the property of this object */
		inline void setMaterial(ntlMaterial *p) { mpMaterial = p; }
		/*! Get the surface property of this object */
		inline ntlMaterial *getMaterial( void ) { return mpMaterial; }
		/*! Set the object property name */
		inline void setMaterialName(string set) { mMaterialName = set; }
		/*! Get the object property name */
		inline string getMaterialName( void ) { return mMaterialName; }

		/*! Sets the receive shadows attribute */
		inline void setReceiveShadows(int set) { mReceiveShadows=set; }
		/*! Returns the receive shadows attribute */
		inline int getReceiveShadows() const { return mReceiveShadows; }

		/*! Sets the cast shadows attribute */
		inline void setCastShadows(int set) { mCastShadows=set; }
		/*! Returns the cast shadows attribute */
		inline int getCastShadows() const { return mCastShadows; }

		/*! Returns the geo init id */
		inline int getGeoInitId() const { return mGeoInitId; }
		/*! Returns the geo init typ */
		inline int getGeoInitType() const { return mGeoInitType; }

		/*! Set/get the cast initial veocity attribute */
		inline void       setInitialVelocity(ntlVec3Gfx set) { mInitialVelocity=set; }
		inline ntlVec3Gfx getInitialVelocity() const         { return mInitialVelocity; }

		/*! Set/get the intersect init flag */
		inline bool getGeoInitIntersect() const { return mGeoInitIntersect; }
		inline void setGeoInitIntersect(bool set) { mGeoInitIntersect=set; }

	protected:

		/*! Point to a property object describing the surface of this object */
		ntlMaterial *mpMaterial;

		/*! Name of the surcace property */
		string mMaterialName;

		/*! Cast shadows on/off */
		int mCastShadows;
		/*! REceive shadows on/off */
		int mReceiveShadows;

		/* fluid init data */
		/*! id of fluid init (is used in solver initialization) */
		int mGeoInitId;
		/*! fluid object type (fluid, obstacle, accelerator etc.) */
		int mGeoInitType;
		/*! initial velocity for fluid objects */
		ntlVec3Gfx mInitialVelocity;
		/*! perform more accurate intersecting geo init for this object? */
		bool mGeoInitIntersect;

	public:

};

#define NTL_GEOMETRYOBJECT_HH
#endif

