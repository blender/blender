/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a geometry object
 * all other geometry objects are derived from this one
 *
 *****************************************************************************/
#ifndef NTL_GEOMETRYOBJECT_H
#define NTL_GEOMETRYOBJECT_H

#include "ntl_geometryclass.h"
#include "ntl_lighting.h"
#include "ntl_ray.h"
class ntlRenderGlobals;
class ntlTriangle;


class ntlGeometryObject : public ntlGeometryClass
{

	public:
		//! Default constructor
		ntlGeometryObject();
		//! Default destructor
		virtual ~ntlGeometryObject();

		//! Return type id
		virtual int getTypeId() { return GEOCLASSTID_OBJECT; }

		/*! Init attributes etc. of this object */
		virtual void initialize(ntlRenderGlobals *glob);

		/*! Get the triangles from this object */
		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId ) = 0;
		
		/*! notify object that dump is in progress (e.g. for particles) */
		virtual void notifyOfDump(int frameNr,char *frameNrStr,string outfilename);

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
		inline void setGeoInitId(int set) { mGeoInitId=set; }
		/*! Returns the geo init typ */
		inline void setGeoInitType(int set) { mGeoInitType=set; }
		/*! Returns the geo init id */
		inline int getGeoInitId() const { return mGeoInitId; }
		/*! Returns the geo init typ */
		inline int getGeoInitType() const { return mGeoInitType; }

		/*! Set/get the intersect init flag */
		inline bool getGeoInitIntersect() const { return mGeoInitIntersect; }
		inline void setGeoInitIntersect(bool set) { mGeoInitIntersect=set; }

		/*! Set/get the part slip value*/
		inline float getGeoPartSlipValue() const { return mGeoPartSlipValue; }
		inline void setGeoPartSlipValue(float set) { mGeoPartSlipValue=set; }

		/*! Set/get the part slip value*/
		inline bool getOnlyThinInit() const { return mOnlyThinInit; }
		inline void setOnlyThinInit(float set) { mOnlyThinInit=set; }

		/*! Set/get the cast initial veocity attribute */
		void setInitialVelocity(ntlVec3Gfx set);
		ntlVec3Gfx getInitialVelocity(double t);

		/*! Set/get the local inivel coords flag */
		inline bool getLocalCoordInivel() const { return mLocalCoordInivel; }
		inline void setLocalCoordInivel(bool set) { mLocalCoordInivel=set; }

		/*! Init channels from float arrays (for elbeem API) */
		void initChannels(
				int nTrans, float *trans, int nRot, float *rot, int nScale, float *scale,
			  int nAct, float *act, int nIvel, float *ivel
				);

		/*! is the object animated? */
		inline bool getIsAnimated() const { return mIsAnimated; }

		/*! apply object translation at time t*/
		void applyTransformation(double t, vector<ntlVec3Gfx> *verts, vector<ntlVec3Gfx> *norms, int vstart, int vend, int forceTrafo);

		/*! Prepare points for moving objects */
		void initMovingPoints(gfxReal featureSize);
		/*! Prepare points for moving objects (copy into ret) */
		void getMovingPoints(vector<ntlVec3Gfx> &ret, vector<ntlVec3Gfx> *norms = NULL);
		/*! Calculate max. velocity on object from t1 to t2 */
		ntlVec3Gfx calculateMaxVel(double t1, double t2);
		/*! get translation at time t*/
		ntlVec3Gfx getTranslation(double t);
		/*! get active flag time t*/
		float getGeoActive(double t);

		/*! add triangle to scene and init flags */
		//  helper function for getTriangles
		void sceneAddTriangle(
				ntlVec3Gfx  p1,ntlVec3Gfx  p2,ntlVec3Gfx  p3,
				ntlVec3Gfx pn1,ntlVec3Gfx pn2,ntlVec3Gfx pn3,
				ntlVec3Gfx trin, bool smooth,
				vector<ntlTriangle> *triangles,
				vector<ntlVec3Gfx>  *vertices,
				vector<ntlVec3Gfx>  *vertNormals);

	protected:

		/* initialized for scene? */
		bool mIsInitialized; 

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
		AnimChannel<ntlVec3Gfx> mcInitialVelocity;
		/*! use object local inflow? */
		bool mLocalCoordInivel;
		/*! perform more accurate intersecting geo init for this object? */
		bool mGeoInitIntersect;
		/*! part slip bc value */
		float mGeoPartSlipValue;
		/*! only init as thin object, dont fill? */
		bool mOnlyThinInit;

		/*! initial offset for rot/scale */
		ntlVec3Gfx mInitialPos;
		/*! animated channels for postition, rotation and scale */
		AnimChannel<ntlVec3Gfx> mcTrans, mcRot, mcScale;
		/*! easy check for animation */
		bool mIsAnimated;
		
		/*! moving point/normal storage */
		vector<ntlVec3Gfx> mMovPoints;
		vector<ntlVec3Gfx> mMovNormals;
		/*! cached points for non moving objects/timeslots */
		bool mHaveCachedMov;
		vector<ntlVec3Gfx> mCachedMovPoints;
		vector<ntlVec3Gfx> mCachedMovNormals;
		/*! inited? */
		float mMovPntsInited;
		/*! point with max. distance from center */
		int mMaxMovPnt;

		/*! animated channels for in/outflow on/off */
		AnimChannel<double> mcGeoActive;

	public:

};

#endif

