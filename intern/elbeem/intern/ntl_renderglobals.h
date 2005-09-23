/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * main renderer class
 *
 *****************************************************************************/
#ifndef NTL_RENDERGLOBALS_HH
#define NTL_RENDERGLOBALS_HH


#include "ntl_vector3dim.h"
#include "ntl_rndstream.h"
#include "ntl_geometryobject.h"
#include "ntl_material.h"
#include "ntl_lightobject.h"
class ntlScene;
class SimulationObject;



//! Display mode
#define DM_VIZ  0
#define DM_RAY  1
#define DM_LBM  2



//! Class that handles global rendering parameters
class ntlRenderGlobals
{
	public:
		//! Standard constructor
		inline ntlRenderGlobals();
		//! Destructor
		~ntlRenderGlobals();

		//! Returns the scene manager
		inline ntlScene *getScene(void) { return mpScene; }
		//! Set the scene manager
		inline void setScene(ntlScene *set) { mpScene = set;} 

		//! Returns the object list
		//inline vector<ntlGeometryObject*> *getObjectList(void) { return mpObjList; }
		//! Set the object list
		//inline void setObjectList(vector<ntlGeometryObject*> *set) { mpObjList = set;} 

		//! Returns the light object list
		inline vector<ntlLightObject*> *getLightList(void) { return mpLightList; }
		//! Set the light list
		inline void setLightList(vector<ntlLightObject*> *set) { mpLightList = set;} 

		//! Returns the property object list
		inline vector<ntlMaterial*> *getMaterials(void) { return mpMaterials; }
		//! Set the property list
		inline void setMaterials(vector<ntlMaterial*> *set) { mpMaterials = set;} 

		//! Returns the list of simulations
		inline vector<SimulationObject*> *getSims(void) { return mpSims; }
		//! Set the pointer to the list of simulations
		inline void setSims(vector<SimulationObject*> *set) { mpSims = set;} 

		//! Set the x resolution
		inline void setResX(unsigned int set) { mResX = set; }
		//! Set the y resolution
		inline void setResY(unsigned int set) { mResY = set; }
		//! Set the anti-aliasing depth
		inline void setAADepth(int set) { mAADepth = set; }
		//! Set the max color value
		inline void setMaxColVal(unsigned int set) { mMaxColVal = set; }
		//! Set the maximum ray recursion
		inline void setRayMaxDepth(unsigned int set) { mRayMaxDepth = set; }
		//! Set the eye point
		inline void setEye(ntlVec3Gfx set) { mvEye = set; }
		//! Set the look at vector
		inline void setLookat(ntlVec3Gfx set) { mvLookat = set; }
		//! Set the up vector
		inline void setUpVec(ntlVec3Gfx set) { mvUpvec = set; }
		//! Set the image aspect
		inline void setAspect(float set) { mAspect = set; }
		//! Set the field of view
		inline void setFovy(float set) { mFovy = set; }
		//! Set the background color
		inline void setBackgroundCol(ntlColor set) { mcBackgr = set; }
		//! Set the ambient lighting color
		inline void setAmbientLight(ntlColor set) { mcAmbientLight = set; }
		//! Set the debug output var 
		inline void setDebugOut(int  set) { mDebugOut = set; }

		//! Set the animation start time
		inline void setAniStart(int set) { mAniStart = set; }
		//! Set the animation number of frames
		inline void setAniFrames(int set) { mAniFrames = set; }
		//! Set the animation
		inline void setAniCount(int set) { mAniCount = set; }
		//! Set the ray counter
		inline void setCounterRays(int set) { mCounterRays = set; }
		//! Set the ray shades counter
		inline void setCounterShades(int set) { mCounterShades = set; }
		//! Set the scenen intersection counter
		inline void setCounterSceneInter(int set) { mCounterSceneInter = set; }
		//! Set if existing frames should be skipped
		inline void setFrameSkip(int set) { mFrameSkip = set; }

		//! Set the outfilename
		inline void setOutFilename(string set) { mOutFilename = set; }

		//! get Maximum depth for BSP tree
		inline void setTreeMaxDepth( int set ) { mTreeMaxDepth = set; }
		//! get Maxmimum nr of triangles per BSP tree node
		inline void setTreeMaxTriangles( int set ) { mTreeMaxTriangles = set; }

		//! set the enable flag of the test sphere
		inline void setTestSphereEnabled( bool set ) { mTestSphereEnabled = set; }
		//! set the center of the test sphere
		inline void setTestSphereCenter( ntlVec3Gfx set ) { mTestSphereCenter = set; }
		//! set the radius of the test sphere
		inline void setTestSphereRadius( gfxReal set ) { mTestSphereRadius = set; }
		//! set the material name of the test sphere
		inline void setTestSphereMaterialName( char* set ) { mTestSphereMaterialName = set; }
		//! set debugging pixel coordinates
		inline void setDebugPixel( int setx, int sety ) { mDebugPixelX = setx; mDebugPixelY = sety; }
		//! set test mode flag
		inline void setTestMode( bool set ) { mTestMode = set; }
		//! set single frame mode flag
		inline void setSingleFrameMode(bool set) {mSingleFrameMode = set; };
		//! set single frame mode filename
		inline void setSingleFrameFilename(string set) {mSingleFrameFilename = set; };
		

		//! Return the x resolution
		inline unsigned int getResX(void) { return mResX; }
		//! Return the y resolution
		inline unsigned int getResY(void) { return mResY; }
		//! Return the anti-aliasing depth
		inline int getAADepth(void) { return mAADepth; }
		//! Return the max color value for ppm
		inline unsigned int getMaxColVal(void) { return mMaxColVal; }
		//! Return the maximum ray recursion
		inline unsigned int getRayMaxDepth(void) { return mRayMaxDepth; }
		//! Return the eye point
		inline ntlVec3Gfx getEye(void) { return mvEye; }
		//! Return the look at vector
		inline ntlVec3Gfx getLookat(void) { return mvLookat; }
		//! Return the up vector
		inline ntlVec3Gfx getUpVec(void) { return mvUpvec; }
		//! Return the image aspect 
		inline float getAspect(void) { return mAspect; }
		//! Return the field of view
		inline float getFovy(void) { return mFovy; }
		//! Return the background color
		inline ntlColor getBackgroundCol(void) { return mcBackgr; }
		//! Return the ambient color
		inline ntlColor getAmbientLight(void) { return mcAmbientLight; }
		//! Return the debug mode setting
		inline int getDebugOut(void) { return mDebugOut; }

		//! Return the animation start time
		inline int getAniStart(void) { return mAniStart; }
		//! Return the animation frame number
		inline int getAniFrames(void) { return mAniFrames; }
		//! Return the animation counter
		inline int getAniCount(void) { return mAniCount; }
		//! Return the ray counter
		inline int getCounterRays(void) { return mCounterRays; }
		//! Return the ray shades counter
		inline int getCounterShades(void) { return mCounterShades; }
		//! Return the scene intersection counter 
		inline int getCounterSceneInter(void) { return mCounterSceneInter; }
		//! Check if existing frames should be skipped
		inline int getFrameSkip( void ) { return mFrameSkip; }


		//! Return the outfilename
		inline string getOutFilename(void) { return mOutFilename; }

		//! get Maximum depth for BSP tree
		inline int getTreeMaxDepth( void ) { return mTreeMaxDepth; }
		//! get Maxmimum nr of triangles per BSP tree node
		inline int getTreeMaxTriangles( void ) { return mTreeMaxTriangles; }
		
		//! get open gl attribute list
		inline AttributeList* getOpenGlAttributes( void ) { return mpOpenGlAttr; }
		//! get blender output attribute list
		inline AttributeList* getBlenderAttributes( void ) { return mpBlenderAttr; }
		
		//! is the test sphere enabled? 
		inline bool getTestSphereEnabled( void ) { return mTestSphereEnabled; }
		//! get the center of the test sphere
		inline ntlVec3Gfx getTestSphereCenter( void ) { return mTestSphereCenter; }
		//! get the radius of the test sphere
		inline gfxReal getTestSphereRadius( void) { return mTestSphereRadius; }
		//! get the materialname of the test sphere
		inline char *getTestSphereMaterialName( void) { return mTestSphereMaterialName; }
		//! get the debug pixel coordinate
		inline int getDebugPixelX( void ) { return mDebugPixelX; }
		//! get the debug pixel coordinate
		inline int getDebugPixelY( void ) { return mDebugPixelY; }
		//! get test mode flag
		inline bool getTestMode( void ) { return mTestMode; }
		//! set single frame mode flag
		inline bool getSingleFrameMode() { return mSingleFrameMode; };
		//! set single frame mode filename
		inline string getSingleFrameFilename() { return mSingleFrameFilename; };


		// random number functions
		//! init random numbers for photon directions
		inline void initRandomDirections( int seed ) { if(mpRndDirections) delete mpRndDirections; mpRndDirections = new ntlRandomStream( seed ); } 
		//! get the next random photon direction
		inline ntlVec3Gfx getRandomDirection( void );
		//! init random numbers for russian roulette
		inline void initRandomRoulette( int seed ) { if(mpRndRoulette) delete mpRndRoulette; mpRndRoulette = new ntlRandomStream( seed ); } 
		//! get the next random  number for russion roulette
		inline gfxReal getRandomRoulette( void ) { return mpRndRoulette->getGfxReal(); }


protected:
  
private:

	/*! Scene storage */
	ntlScene *mpScene;

  //! List of geometry objects
  //vector<ntlGeometryObject*>  *mpObjList;
  //! List of light objects
  vector<ntlLightObject*> *mpLightList;
  //! List of surface properties
  vector<ntlMaterial*> *mpMaterials;
	/*! storage for simulations */
	vector<SimulationObject*> *mpSims;

  //! resolution of the picture
  unsigned int mResX, mResY;
  //! Anti-Aliasing depth
  int mAADepth;
  //! max color value for ppm
  unsigned int mMaxColVal;
  /* Maximal ray recursion depth */
  int mRayMaxDepth;
  //! The eye point
  ntlVec3Gfx  mvEye;
  //! The look at point
  ntlVec3Gfx  mvLookat;
  //! The up vector
  ntlVec3Gfx  mvUpvec;
  //! The image aspect = Xres/Yres
  float  mAspect;
  //! The horizontal field of view
  float  mFovy;
  //! The background color
  ntlColor  mcBackgr;
  //! The ambient color
  ntlColor  mcAmbientLight;
  //! how much debug output is needed? off by default
  char mDebugOut;


  //! animation properties, start time
  int mAniStart;
  //! animation properties, number of frames to render
  int mAniFrames;
  //! animation status, current frame number
  int mAniCount;
	/*! Should existing picture frames be skipped? */
	int mFrameSkip;


  //! count the total number of rays created (also used for ray ID's)
  int  mCounterRays;
  //! count the total number of rays shaded
  int  mCounterShades;
  //! count the total number of scene intersections
  int  mCounterSceneInter;

	/*! filename of output pictures (without suffix or numbers) */
  string mOutFilename;

	//! get Maximum depth for BSP tree
	int mTreeMaxDepth;
	//! get Maxmimum nr of triangles per BSP tree node
	int mTreeMaxTriangles; 

	//! attribute list for opengl renderer
	AttributeList *mpOpenGlAttr;
	//! attribute list for blender output 
	AttributeList *mpBlenderAttr;


	//! Enable test sphere?
	bool mTestSphereEnabled;
	//! Center of the test sphere
	ntlVec3Gfx mTestSphereCenter;
	//! Radius of the test sphere
	gfxReal mTestSphereRadius;
	//! Materialname of the test sphere
	char *mTestSphereMaterialName;
	//! coordinates of the debugging pixel
	int mDebugPixelX, mDebugPixelY;

	//! test mode for quick rendering activated?, inited in ntl_scene::buildScene
	bool mTestMode;

	//! single frame flag
	bool mSingleFrameMode;
	//! filename for single frame mode
	string mSingleFrameFilename;

	/*! Two random number streams for photon generation (one for the directions, the other for russion roulette) */
	ntlRandomStream *mpRndDirections, *mpRndRoulette;

};




/*****************************************************************************/
/* Constructor with standard value init */
inline ntlRenderGlobals::ntlRenderGlobals() :
  mpLightList( NULL ), mpMaterials( NULL ), mpSims( NULL ),
  mResX(320), mResY(200), mAADepth(-1), mMaxColVal(255), 
  mRayMaxDepth( 5 ),
  mvEye(0.0,0.0,5.0), mvLookat(0.0,0.0,0.0), mvUpvec(0.0,1.0,0.0), 
  mAspect(320.0/200.0), 
  mFovy(45), mcBackgr(0.0,0.0,0.0), mcAmbientLight(0.0,0.0,0.0), 
  mDebugOut( 0 ),
  mAniStart(0), mAniFrames( -1 ), mAniCount( 0 ),
	mFrameSkip( 0 ),
  mCounterRays( 0 ), mCounterShades( 0 ), mCounterSceneInter( 0 ),
	mOutFilename( "pic" ),
	mTreeMaxDepth( 30 ), mTreeMaxTriangles( 30 ),
	mpOpenGlAttr(NULL),
	mpBlenderAttr(NULL),
	mTestSphereEnabled( false ),
	mDebugPixelX( -1 ), mDebugPixelY( -1 ), mTestMode(false),
	mSingleFrameMode(false), mSingleFrameFilename(""),
	mpRndDirections( NULL ), mpRndRoulette( NULL )
{ 
	// create internal attribute list for opengl renderer
	mpOpenGlAttr = new AttributeList("__ntlOpenGLRenderer");
	mpBlenderAttr = new AttributeList("__ntlBlenderAttr");
};


/*****************************************************************************/
/* Destructor */
inline ntlRenderGlobals::~ntlRenderGlobals() {
	if(mpOpenGlAttr) delete mpOpenGlAttr;
	if(mpBlenderAttr) delete mpBlenderAttr;
}


/*****************************************************************************/
//! get the next random photon direction
inline ntlVec3Gfx ntlRenderGlobals::getRandomDirection( void ) { 
	return ntlVec3Gfx( 
			(mpRndDirections->getGfxReal()-0.5), 
			(mpRndDirections->getGfxReal()-0.5),  
			(mpRndDirections->getGfxReal()-0.5) ); 
} 

#endif

