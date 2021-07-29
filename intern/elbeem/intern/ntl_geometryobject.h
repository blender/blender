/** \file elbeem/intern/ntl_geometryobject.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
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

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

class ntlRenderGlobals;
class ntlTriangle;

#define DUMP_FULLGEOMETRY 1
#define DUMP_PARTIAL      2

#define VOLUMEINIT_VOLUME 1
#define VOLUMEINIT_SHELL  2
#define VOLUMEINIT_BOTH   (VOLUMEINIT_SHELL|VOLUMEINIT_VOLUME)

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
		virtual void getTriangles(double t, vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId ) = 0;
		
		/*! notify object that dump is in progress (e.g. for particles) */
		virtual void notifyOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename, double simtime);

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

		/*! Returns the geo init typ */
		inline void setGeoInitType(int set) { mGeoInitType=set; }
		/*! Returns the geo init typ */
		inline int getGeoInitType() const { return mGeoInitType; }

		/*! Set/get the intersect init flag */
		inline bool getGeoInitIntersect() const { return mGeoInitIntersect; }
		inline void setGeoInitIntersect(bool set) { mGeoInitIntersect=set; }

		/*! Set/get the part slip value*/
		inline float getGeoPartSlipValue() const { return mGeoPartSlipValue; }
		inline void setGeoPartSlipValue(float set) { mGeoPartSlipValue=set; }

		/*! Set/get the impact corr factor channel */
		inline float getGeoImpactFactor(double t) { return mcGeoImpactFactor.get(t); }
		inline void setGeoImpactFactor(float set) { mcGeoImpactFactor = AnimChannel<float>(set); }

		/*! Set/get the part slip value*/
		inline int getVolumeInit() const { return mVolumeInit; }
		inline void setVolumeInit(int set) { mVolumeInit=set; }

		/*! Set/get the cast initial veocity attribute */
		void setInitialVelocity(ntlVec3Gfx set);
		ntlVec3Gfx getInitialVelocity(double t);

		/*! Set/get the local inivel coords flag */
		inline bool getLocalCoordInivel() const { return mLocalCoordInivel; }
		inline void setLocalCoordInivel(bool set) { mLocalCoordInivel=set; }
		
		/****************************************/
		/* fluid control features */
		/****************************************/
		/*! Set/get the particle control set attract force strength */
		inline float getCpsTimeStart() const { return mCpsTimeStart; }
		inline void setCpsTimeStart(float set) { mCpsTimeStart=set; }
		
		/*! Set/get the particle control set attract force strength */
		inline float getCpsTimeEnd() const { return mCpsTimeEnd; }
		inline void setCpsTimeEnd(float set) { mCpsTimeEnd=set; }
		
		/*! Set/get the particle control set quality */
		inline float getCpsQuality() const { return mCpsQuality; }
		inline void setCpsQuality(float set) { mCpsQuality=set; }
		
		inline AnimChannel<float> getCpsAttrFStr() const { return mcAttrFStr; }
		inline AnimChannel<float> getCpsAttrFRad() const { return mcAttrFRad; }
		inline AnimChannel<float> getCpsVelFStr() const { return mcVelFStr; }
		inline AnimChannel<float> getCpsVelFRad() const { return mcVelFRad; }
		
		/****************************************/
		
		/*! Init channels from float arrays (for elbeem API) */
		void initChannels(
				int nTrans, float *trans, int nRot, float *rot, int nScale, float *scale,
				int nAct, float *act, int nIvel, float *ivel,
				int nAttrFStr, float *attrFStr,
				int nAttrFRad, float *attrFRad,
				int nVelFStr, float *velFStr,
				int nVelFRad, float *velFRad
				);

		/*! is the object animated? */
		inline bool getIsAnimated() const { return mIsAnimated; }
		/*! init object anim flag */
		bool checkIsAnimated();
		/*! is the mesh animated? */
		virtual bool getMeshAnimated();
		/*! init triangle divisions */
		virtual void calcTriangleDivs(vector<ntlVec3Gfx> &verts, vector<ntlTriangle> &tris, gfxReal fsTri);

		/*! apply object translation at time t*/
		void applyTransformation(double t, vector<ntlVec3Gfx> *verts, vector<ntlVec3Gfx> *norms, int vstart, int vend, int forceTrafo);

		/*! Prepare points for moving objects */
		void initMovingPoints(double time, gfxReal featureSize);
		/*! Prepare points for animated objects */
		void initMovingPointsAnim(
		 double srctime, vector<ntlVec3Gfx> &srcpoints,
		 double dsttime, vector<ntlVec3Gfx> &dstpoints,
		 vector<ntlVec3Gfx> *dstnormals,
		 gfxReal featureSize, ntlVec3Gfx geostart, ntlVec3Gfx geoend );
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
		void sceneAddTriangleNoVert(int *trips,
				ntlVec3Gfx trin, bool smooth,
				vector<ntlTriangle> *triangles);

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
		/*! obstacle impact correction factor */
		AnimChannel<float> mcGeoImpactFactor;
		/*! only init as thin object, dont fill? */
		int mVolumeInit;

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
		/*! precomputed triangle divisions */
		vector<int> mTriangleDivs1,mTriangleDivs2;
		/*! inited? */
		float mMovPntsInited;
		/*! point with max. distance from center */
		int mMaxMovPnt;

		/*! animated channels for in/outflow on/off */
		AnimChannel<float> mcGeoActive;
		
		/* fluid control settings */
		float mCpsTimeStart;
		float mCpsTimeEnd;
		float mCpsQuality;
		AnimChannel<float> mcAttrFStr, mcAttrFRad, mcVelFStr, mcVelFRad;

	public:

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:ntlGeometryObject")
#endif
};

#endif

