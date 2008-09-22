/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Base class for geometry shaders and objects
 *
 *****************************************************************************/


#ifndef NTL_GEOMETRYCLASS_H
#define NTL_GEOMETRYCLASS_H

#include "attributes.h"

//! geometry class type ids
#define GEOCLASSTID_OBJECT   1
#define GEOCLASSTID_SHADER   2
#define GEOCLASSTID_BOX      (GEOCLASSTID_OBJECT| 4)
#define GEOCLASSTID_OBJMODEL (GEOCLASSTID_OBJECT| 8)
#define GEOCLASSTID_SPHERE   (GEOCLASSTID_OBJECT| 16)

class ntlGeometryClass 
{

	public:

		//! Default constructor
		inline ntlGeometryClass() :
			mVisible( 1 ), mName( "[ObjNameUndef]" ),
			mObjectId(-1), mpAttrs( NULL ), mGeoInitId(-1) 
		{ 
				mpAttrs = new AttributeList("objAttrs"); 
				mpSwsAttrs = new AttributeList("swsAttrs"); 
		};

		//! Default destructor
		virtual ~ntlGeometryClass() {
			delete mpAttrs; 
			delete mpSwsAttrs;
		};

		//! Return type id
		virtual int getTypeId() = 0;

		/*! Set the object name */
		inline void setName(string set) { mName = set; }
		/*! Get the object name */
		inline string getName( void ) { return mName; }

		/*! Sets the visibility attribute 
		 * visibility can be determined at shader _and_ object level , hiding a shader
		 * means comepletely decativating it */
		inline void setVisible(int set) { mVisible=set; }
		/*! Returns the visibility attribute */
		inline int getVisible() const { return mVisible; }

		/*! Sets the attribute list pointer */
		inline void setAttributeList(AttributeList *set) { mpAttrs=set; }
		/*! Returns the attribute list pointer */
		inline AttributeList *getAttributeList() { return mpAttrs; }

		/*! Get/Sets the attribute list pointer */
		inline void setSwsAttributeList(AttributeList *set) { mpSwsAttrs=set; }
		inline AttributeList *getSwsAttributeList() { return mpSwsAttrs; }

		/*! for easy GUI detection get start of axis aligned bounding box, return NULL of no BB */
		virtual inline ntlVec3Gfx *getBBStart() { return NULL; }
		virtual inline ntlVec3Gfx *getBBEnd() 	{ return NULL; }

		/*! Set/get the object id*/
		inline void setObjectId(int set) { mObjectId=set; }
		inline int getObjectId() const { return mObjectId; }

		/*! GUI - this function is called for selected objects to display debugging information with OpenGL */
		virtual void drawDebugDisplay() { /* do nothing by default */ }
		/*! GUI - this function is called for selected objects to display interactive information with OpenGL */
		virtual void drawInteractiveDisplay() { /* do nothing by default */ }
		/*! GUI - handle mouse movement for selection */
		virtual void setMousePos(int ,int , ntlVec3Gfx , ntlVec3Gfx ) { /* do nothing by default */ }
		/*! GUI - notify object that mouse was clicked at last pos */
		virtual void setMouseClick() { /* do nothing by default */ }

		/*! Returns the geo init id */
		inline void setGeoInitId(int set) { mGeoInitId=set; }
		/*! Returns the geo init id */
		inline int getGeoInitId() const { return mGeoInitId; }

	protected:

		/*! Object visible on/off */
		int mVisible;

		/*! Name of this object */
		string mName;

		/*! global scene object id */
		int mObjectId;

		/*! configuration attributes */
		AttributeList *mpAttrs;
		/*! sws configuration attributes */
		AttributeList *mpSwsAttrs;

		/* fluid init data */
		/*! id of fluid init (is used in solver initialization), additional data stored only for objects */
		int mGeoInitId;

	private:

};



#endif

