/** \file elbeem/intern/solver_interface.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * Header for Combined 2D/3D Lattice Boltzmann Interface Class
 * 
 *****************************************************************************/
#ifndef LBMINTERFACE_H
#define LBMINTERFACE_H

//! include gui support?
#ifndef NOGUI
#define LBM_USE_GUI      1
#else
#define LBM_USE_GUI      0
#endif

#if LBM_USE_GUI==1
#define USE_GLUTILITIES
// for debug display
//#include <GL/gl.h>
#include "../gui/guifuncs.h"
#endif

#include <sstream>
#include "utilities.h"
#include "ntl_bsptree.h"
#include "ntl_geometryobject.h"
#include "parametrizer.h"
#include "attributes.h"
#include "isosurface.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

class ParticleTracer;
class ParticleObject;

// use which fp-precision for LBM? 1=float, 2=double
#ifdef PRECISION_LBM_SINGLE
#define LBM_PRECISION 1
#else
#ifdef PRECISION_LBM_DOUBLE
#define LBM_PRECISION 2
#else
// default to floats
#define LBM_PRECISION 1
#endif
#endif

#if LBM_PRECISION==1
/* low precision for LBM solver */
typedef float LbmFloat;
typedef ntlVec3f LbmVec;
#define LBM_EPSILON (1e-5)
#else
/* standard precision for LBM solver */
typedef double LbmFloat;
typedef ntlVec3d LbmVec;
#define LBM_EPSILON (1e-10)
#endif

// long integer, needed e.g. for memory calculations
#ifndef USE_MSVC6FIXES
#define LONGINT long long int
#else
#define LONGINT _int64
#endif


// default to 3dim
#ifndef LBMDIM
#define LBMDIM 3
#endif // LBMDIM

#if LBMDIM==2
#define LBM_DFNUM 9
#else
#define LBM_DFNUM 19
#endif

// conversions (lbm and parametrizer)
template<class T> inline LbmVec     vec2L(T v) { return LbmVec(v[0],v[1],v[2]); }
template<class T> inline ParamVec   vec2P(T v) { return ParamVec(v[0],v[1],v[2]); }

template<class Scalar> class ntlMatrix4x4;


// bubble id type
typedef int BubbleId;

// basic cell type distinctions
#define CFUnused              (1<< 0)
#define CFEmpty               (1<< 1)
#define CFBnd                 (1<< 2)
#define CFMbndInflow          (1<< 3)
#define CFMbndOutflow         (1<< 4)
#define CFFluid               (1<< 5)
#define CFInter               (1<< 6)
// additional for fluid (needed separately for adaptive grids)
#define CFNoBndFluid          (1<< 7)
#define CFNoDelete            (1<< 8)

// additional bnd add flags
#define CFBndNoslip           (1<< 9)
#define CFBndFreeslip         (1<<10)
#define CFBndPartslip         (1<<11)
#define CFBndMoving           (1<<12)

// additional for fluid/interface
// force symmetry for flag reinit 
#define CFNoInterpolSrc       (1<<13) 
#define CFNoNbFluid           (1<<14)
#define CFNoNbEmpty           (1<<15)
	
// cell treated normally on coarser grids
#define CFGrNorm              (1<<16)
#define CFGrCoarseInited      (1<<17)

// (the following values shouldnt overlap to ensure
// proper coarsening)
// border cells to be interpolated from finer grid
#define CFGrFromFine          (1<<18)
// 32k aux border marker 
#define CFGrToFine            (1<<19)
// also needed on finest level
#define CFGrFromCoarse        (1<<20)
// additional refinement tags (coarse grids only?)
// */

// above 24 is used to encode in/outflow object type
#define CFPersistMask (0xFF000000 | CFMbndInflow | CFMbndOutflow)
#define CFNoPersistMask (~CFPersistMask)


// nk
#define CFInvalid             (CellFlagType)(1<<31)

// use 32bit flag types
//#ifdef __x86_64__
 //typedef int cfINT32;
//#else
 //typedef long cfINT32;
//#endif // defined (_IA64)  
//#define CellFlagType cfINT32
#define CellFlagType int
#define CellFlagTypeSize 4


// aux. field indices (same for 2d)
#define dFfrac 19
#define dMass 20
#define dFlux 21
// max. no. of cell values for 3d
#define dTotalNum 22


/*****************************************************************************/
/*! a single lbm cell */
/*  the template is only needed for 
 *  dimension dependend constants e.g. 
 *  number of df's in model */
class LbmCellContents {
	public:
		LbmFloat     df[ 27 ]; // be on the safe side here...
  	LbmFloat     rho;
		LbmVec       vel;
  	LbmFloat     mass;
		CellFlagType flag;
		BubbleId     bubble;
  	LbmFloat     ffrac;

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:LbmCellContents")
#endif
};

/* struct for the coordinates of a cell in the grid */
typedef struct {
  int x,y,z;
	int flag; // special handling?
} LbmPoint;

/* struct for the coordinates of a cell in the grid */
typedef struct {
	char active;            // bubble in use, oder may be overwritten?
  LbmFloat volume;          // volume of this bubble (0 vor atmosphere)
	LbmFloat mass;            // "mass" of bubble 
	int i,j,k;              // index of a cell in the bubble
} LbmBubble;




//! choose which data to display
#define FLUIDDISPINVALID    0
#define FLUIDDISPNothing    1
#define FLUIDDISPCelltypes  2
#define FLUIDDISPVelocities 3
#define FLUIDDISPCellfills  4
#define FLUIDDISPDensity    5
#define FLUIDDISPGrid       6
#define FLUIDDISPSurface    7



/*****************************************************************************/
//! cell identifier interface
class CellIdentifierInterface {
	public:
		//! reset constructor
		CellIdentifierInterface():mEnd(false) { };
		//! virtual destructor
		virtual ~CellIdentifierInterface() {};

		//! return node as string (with some basic info)
		virtual string getAsString() = 0;

		//! compare cids
		virtual bool equal(CellIdentifierInterface* other) = 0;

		//! set/get end flag for grid traversal (not needed for marked cells)
		inline void setEnd(bool set){ mEnd = set;  }
		inline bool getEnd( )       { return mEnd; }

		//! has the grid been traversed?
		bool mEnd;

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:CellIdentifierInterface")
#endif
};



/*****************************************************************************/
/*! class defining abstract function interface */
/*  has to provide iterating functionality */
class LbmSolverInterface 
{
	public:
		//! Constructor 
		LbmSolverInterface();
		//! Destructor 
		virtual ~LbmSolverInterface();
		//! id string of solver
		virtual string getIdString() = 0;

		// multi step solver init
		/*! finish the init with config file values (allocate arrays...) */
		virtual bool initializeSolverMemory() =0;
		/*! init solver arrays */
		virtual bool initializeSolverGrids() =0;
		/*! prepare actual simulation start, setup viz etc */
		virtual bool initializeSolverPostinit() =0;
		
		/*! notify object that dump is in progress (e.g. for field dump) */
		virtual void notifySolverOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename) = 0;

		/*! parse a boundary flag string */
		CellFlagType readBoundaryFlagInt(string name, int defaultValue, string source,string target, bool needed);
		/*! parse standard attributes */
		void parseStdAttrList();
		/*! initilize variables fom attribute list (should at least call std parse) */
		virtual void parseAttrList() = 0;

		virtual void step() = 0;
		virtual void prepareVisualization() { /* by default off */ };

		/*! particle handling */
		virtual int initParticles() = 0;
		virtual void advanceParticles() = 0;
		/*! get surface object (NULL if no surface) */
		IsoSurface* getSurfaceGeoObj() { return mpIso; }

		/*! debug object display */
		virtual vector<ntlGeometryObject*> getDebugObjects() { vector<ntlGeometryObject*> empty(0); return empty; }

		/* surface generation settings */
		virtual void setSurfGenSettings(short value) = 0;

#if LBM_USE_GUI==1
		/*! show simulation info */
		virtual void debugDisplay(int) = 0;
#endif

		/*! init tree for certain geometry init */
		void initGeoTree();
		/*! destroy tree etc. when geometry init done */
		void freeGeoTree();
		/*! check for a certain flag type at position org (needed for e.g. quadtree refinement) */
		bool geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId, gfxReal &distance,int shootDir=0);
		bool geoInitCheckPointInside(ntlVec3Gfx org, ntlVec3Gfx dir, int flags, int &OId, gfxReal &distance, 
				const gfxReal halfCellsize, bool &thinHit, bool recurse);
		/*! set render globals, for scene/tree access */
		void setRenderGlobals(ntlRenderGlobals *glob) { mpGlob = glob; };
		/*! get max. velocity of all objects to initialize as fluid regions, and of all moving objects */
		ntlVec3Gfx getGeoMaxMovementVelocity(LbmFloat simtime, LbmFloat stepsize);

		/* rt interface functions */
		unsigned int getIsoVertexCount()  { return mpIso->getIsoVertexCount(); }
		unsigned int getIsoIndexCount()   { return mpIso->getIsoIndexCount(); }
		char* getIsoVertexArray()         { return mpIso->getIsoVertexArray(); }
		unsigned int *getIsoIndexArray()  { return mpIso->getIsoIndexArray(); }
		void triangulateSurface()         { mpIso->triangulate(); }

		/* access functions */

		/*! return grid sizes */
		int getSizeX( void ) { return mSizex; }
		int getSizeY( void ) { return mSizey; }
		int getSizeZ( void ) { return mSizez; }
		/*! return grid sizes */
		void setSizeX( int ns ) { mSizex = ns; }
		void setSizeY( int ns ) { mSizey = ns; }
		void setSizeZ( int ns ) { mSizez = ns; }
		/*! access fluid only simulation flag */
		void setAllfluid(bool set) { mAllfluid=set; }
		bool getAllfluid()         { return mAllfluid; }

		/*! set attr list pointer */
		void setAttrList(AttributeList *set) { mpSifAttrs = set; }
		/*! Returns the attribute list pointer */
		inline AttributeList *getAttributeList() { return mpSifAttrs; }
		/*! set sws attr list pointer */
		void setSwsAttrList(AttributeList *set) { mpSifSwsAttrs = set; }
		inline AttributeList *getSwsAttributeList() { return mpSifSwsAttrs; }

		/*! set parametrizer pointer */
		inline void setParametrizer(Parametrizer *set) { mpParam = set; }
		/*! get parametrizer pointer */
		inline Parametrizer *getParametrizer() { return mpParam; }
		/*! get/set particle pointer */
		inline void setParticleTracer(ParticleTracer *set) { mpParticles = set; }
		inline ParticleTracer *getParticleTracer() { return mpParticles; }

		/*! set density gradient init from e.g. init test cases */
		inline void setInitDensityGradient(bool set) { mInitDensityGradient = set; }

		/*! access geometry start vector */
		inline void setGeoStart(ntlVec3Gfx set)	{ mvGeoStart = set; }
		inline ntlVec3Gfx getGeoStart() const	{ return mvGeoStart; }

		/*! access geometry end vector */
		inline void setGeoEnd(ntlVec3Gfx set)	{ mvGeoEnd = set; }
		inline ntlVec3Gfx getGeoEnd() const	{ return mvGeoEnd; }

		/*! access geo init vars */
		inline void setLbmInitId(int set)	{ mLbmInitId = set; }
		inline int getLbmInitId() const	{ return mLbmInitId; }

		/*! init domain transformation matrix from float array */
		void initDomainTrafo(float *mat);
		/*! get domain transformation matrix to have object centered fluid vertices */
		inline ntlMatrix4x4<gfxReal> *getDomainTrafo() { return mpSimTrafo; }

		/*! access name string */
		inline void setName(string set)	{ mName = set; }
		inline string getName() const	{ return mName; }

		/*! access string for node info debugging output */
		inline string getNodeInfoString() const { return mNodeInfoString; }

		/*! get panic flag */
		inline bool getPanic() { return mPanic; }

		//! set silent mode?
		inline void setSilent(bool set){ mSilent = set; }

		//! set amount of surface/normal smoothing
		inline void setSmoothing(float setss,float setns){ mSmoothSurface=setss; mSmoothNormals=setns; }
		//! set amount of iso subdivisions
		inline void setIsoSubdivs(int s){ mIsoSubdivs=s; }
		//! set desired refinement
		inline void setPreviewSize(int set){ mOutputSurfacePreview = set; }
		//! set desired refinement
		inline void setRefinementDesired(int set){ mRefinementDesired = set; }

		//! set/get dump velocities flag
		inline void setDumpVelocities(bool set)	{ mDumpVelocities = set; }
		inline bool getDumpVelocities() const	{ return mDumpVelocities; }

		//! set/get particle generation prob.
		inline void setGenerateParticles(LbmFloat set)	{ mPartGenProb = set; }
		inline LbmFloat getGenerateParticles() const	{ return mPartGenProb; }

		//! set/get dump velocities flag
		inline void setDomainBound(string set)	{ mDomainBound = set; }
		inline string getDomainBound() const	{ return mDomainBound; }
		//! set/get dump velocities flag
		inline void setDomainPartSlip(LbmFloat set)	{ mDomainPartSlipValue = set; }
		inline LbmFloat getDomainPartSlip() const	{ return mDomainPartSlipValue; }
		//! set/get far field size
		inline void setFarFieldSize(LbmFloat set)	{ mFarFieldSize = set; }
		inline LbmFloat getFarFieldSize() const	{ return mFarFieldSize; }
		//! set/get cp stage
		inline void setCpStage(int set)	{ mCppfStage = set; }
		inline int getCpStage() const	{ return mCppfStage; }
		//! set/get dump modes
		inline void setDumpRawText(bool set)	{ mDumpRawText = set; }
		inline bool getDumpRawText() const	{ return mDumpRawText; }
		inline void setDumpRawBinary(bool set)	{ mDumpRawBinary = set; }
		inline bool getDumpRawBinary() const	{ return mDumpRawBinary; }
		inline void setDumpRawBinaryZip(bool set)	{ mDumpRawBinaryZip = set; }
		inline bool getDumpRawBinaryZip() const	{ return mDumpRawBinaryZip; }
		//! set/get debug vel scale
		inline void setDebugVelScale(LbmFloat set)	{ mDebugVelScale = set; }
		inline LbmFloat getDebugVelScale() const	{ return mDebugVelScale; }

		// cell iterator interface
		
		// cell id type
		typedef CellIdentifierInterface* CellIdentifier;

		//! cell iteration methods
		virtual CellIdentifierInterface* getFirstCell( ) = 0;
		virtual void advanceCell( CellIdentifierInterface* ) = 0;
		virtual bool noEndCell( CellIdentifierInterface* ) = 0;
		//! clean up iteration, this should be called, when the iteration is not completely finished
		virtual void deleteCellIterator( CellIdentifierInterface** ) = 0;

		//! find cell at a given position (returns NULL if not in domain)
		virtual CellIdentifierInterface* getCellAt( ntlVec3Gfx pos ) = 0;

		//! return node information
		virtual int        getCellSet      ( CellIdentifierInterface* ) = 0;
		virtual ntlVec3Gfx getCellOrigin   ( CellIdentifierInterface* ) = 0;
		virtual ntlVec3Gfx getCellSize     ( CellIdentifierInterface* ) = 0;
		virtual int        getCellLevel    ( CellIdentifierInterface* ) = 0;
		virtual LbmFloat   getCellDensity  ( CellIdentifierInterface*,int ) = 0;
		virtual LbmVec     getCellVelocity ( CellIdentifierInterface*,int ) = 0;
		/*! get equilibrium distribution functions */
		virtual LbmFloat   getEquilDf      ( int ) = 0;
		/*! redundant cell functions */
		virtual LbmFloat   getCellDf       ( CellIdentifierInterface* ,int set, int dir) = 0;
		virtual LbmFloat   getCellMass     ( CellIdentifierInterface* ,int set) = 0;
		virtual LbmFloat   getCellFill     ( CellIdentifierInterface* ,int set) = 0;
		virtual CellFlagType getCellFlag   ( CellIdentifierInterface* ,int set) = 0;

		/*! get velocity directly from position */
		virtual ntlVec3Gfx getVelocityAt(float x, float y, float z) = 0;

		// gui/output debugging functions
#if LBM_USE_GUI==1
		virtual void debugDisplayNode(int dispset, CellIdentifier cell ) = 0;
		virtual void lbmDebugDisplay(int dispset) = 0;
		virtual void lbmMarkedCellDisplay() = 0;
#endif // LBM_USE_GUI==1
		virtual void debugPrintNodeInfo(CellIdentifier cell, int forceSet=-1) = 0;

		// debugging cell marker functions

		//! add cell to mMarkedCells list
		void addCellToMarkedList( CellIdentifierInterface *cid );
		//! marked cell iteration methods
		CellIdentifierInterface* markedGetFirstCell( );
		CellIdentifierInterface* markedAdvanceCell();
		void markedClearList();

#if PARALLEL==1
		void setNumOMPThreads(int num_threads);
#endif  // PARALLEL==1
	protected:

		/*! abort simulation on error... */
		bool mPanic;


		/*! Size of the array in x,y,z direction */
		int mSizex, mSizey, mSizez;
		/*! only fluid in sim? */
		bool mAllfluid;


		/*! step counter */
		int mStepCnt;

		/*! mass change from one step to the next, for extreme cases fix globally */
		LbmFloat mFixMass;

		// deprecated param vars
		/*! omega for lbm */
		LbmFloat mOmega;
		/*! gravity strength in neg. z direction */
		LbmVec mGravity;
		/*! Surface tension of the fluid */
		LbmFloat mSurfaceTension;


		/* boundary inits */
		CellFlagType mBoundaryEast, mBoundaryWest, 
		  mBoundaryNorth, mBoundarySouth, 
		  mBoundaryTop, mBoundaryBottom;

		/*! initialization from config file done? */
		int mInitDone;

		/*! init density gradient? */
		bool mInitDensityGradient;

		/*! pointer to the attribute list, only pointer to obj attrs */
		AttributeList *mpSifAttrs;
		AttributeList *mpSifSwsAttrs;

		/*! get parameters from this parametrize in finishInit */
		Parametrizer *mpParam;
		//! store particle tracer
		ParticleTracer *mpParticles;

		/*! number of particles lost so far */
		int mNumParticlesLost;
		/*! number of particles lost so far */
		int mNumInvalidDfs;
		/*! no of filled/emptied cells per time step */
		int mNumFilledCells, mNumEmptiedCells;
		/*! counter number of used cells for performance */
		int mNumUsedCells;
		/*! MLSUPS counter */
		LbmFloat mMLSUPS;
		/*! debug - velocity output scaling factor */
		LbmFloat mDebugVelScale;
		/*! string for node info debugging output */
		string mNodeInfoString;

		// geo init vars
		// TODO deprecate SimulationObject vars

		/*! for display - start and end vectors for geometry */
		ntlVec3Gfx mvGeoStart, mvGeoEnd;
		//! domain vertex trafos
		ntlMatrix4x4<gfxReal> *mpSimTrafo;

		/*! perform accurate geometry init? */
		bool mAccurateGeoinit;

		/*! name of this lbm object (for debug output) */
		string mName;

		//! Mcubes object for surface reconstruction 
		IsoSurface *mpIso;
		/*! isolevel value for marching cubes surface reconstruction */
		LbmFloat mIsoValue;

		//! debug output?
		bool mSilent;

		/*! geometry init id, passed from ntl_geomclass */
		int mLbmInitId;
		/*! tree object for geomerty initialization */
		ntlTree *mpGiTree;
		/*! object vector for geo init */
		vector<ntlGeometryObject*> *mpGiObjects;
		/*! inside which objects? */
		vector<int> mGiObjInside;
		/*! inside which objects? */
		vector<gfxReal> mGiObjDistance;
		vector<gfxReal> mGiObjSecondDist;
		/*! remember globals */
		ntlRenderGlobals *mpGlob;
		
		//! use refinement/coarsening?
		int mRefinementDesired;

		//! output surface preview? if >0 yes, and use as reduzed size 
		int mOutputSurfacePreview;
		LbmFloat mPreviewFactor;

		/*! enable surface and normals smoothing? */
		float mSmoothSurface;
		float mSmoothNormals;
		/*! isosurface subdivisions */
		int mIsoSubdivs;

		//! particle generation probability
		LbmFloat mPartGenProb;

		//! dump velocities?
		bool mDumpVelocities;

		// list for marked cells
		vector<CellIdentifierInterface *> mMarkedCells;
		int mMarkedCellIndex;

		//! domain boundary free/no slip type
		string mDomainBound;
		//! part slip value for domain
		LbmFloat mDomainPartSlipValue;

		// size of far field area
		LbmFloat mFarFieldSize;
		// amount of drop mass to subtract
		LbmFloat mPartDropMassSub;
		// use physical drop model for particles?
		bool mPartUsePhysModel;

		//! test vars
		// strength of applied force
		LbmFloat mTForceStrength;
		int mCppfStage;

		//! dumping modes
		bool mDumpRawText;
		bool mDumpRawBinary;
		bool mDumpRawBinaryZip;

#if PARALLEL==1
		int mNumOMPThreads;
#endif  // PARALLEL==1

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:LbmSolverInterface")
#endif
};


// helper function to create consistent grid resolutions
void initGridSizes(int &mSizex, int &mSizey, int &mSizez,
		ntlVec3Gfx &mvGeoStart, ntlVec3Gfx &mvGeoEnd, 
		int mMaxRefine, bool parallel);
// return the amount of memory required in total (reqret)
// and for the finest grid only (reqretFine, can be NULL)
void calculateMemreqEstimate(int resx,int resy,int resz, int refine,
		float farfieldsize, double *reqret, double *reqretFine, string *reqstr);

//! helper function to convert flag to string (for debuggin)
string convertCellFlagType2String( CellFlagType flag );
string convertSingleFlag2String(CellFlagType cflag);

#endif // LBMINTERFACE_H
