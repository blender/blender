/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
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
#include <GL/gl.h>
#include "../gui/guifuncs.h"
#endif

#include <sstream>
#include "utilities.h"
#include "ntl_bsptree.h"
#include "ntl_geometryobject.h"
#include "ntl_rndstream.h"
#include "parametrizer.h"
#include "attributes.h"
#include "particletracer.h"
#include "isosurface.h"

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

// default to 3dim
#ifndef LBMDIM
#define LBMDIM 3
#endif // LBMDIM

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

// conversions (lbm and parametrizer)
template<class T> inline LbmVec     vec2L(T v) { return LbmVec(v[0],v[1],v[2]); }
template<class T> inline ParamVec   vec2P(T v) { return ParamVec(v[0],v[1],v[2]); }


// bubble id type
typedef int BubbleId;

// for both short int/char
#define CFUnused              (1<< 0)
#define CFEmpty               (1<< 1)
#define CFBnd                 (1<< 2)
#define CFBndNoslip           (1<< 3)
#define CFBndFreeslip         (1<< 4)
#define CFBndPartslip         (1<< 5)
// force symmetry for flag reinit
#define CFNoInterpolSrc       (1<< 6) 
#define CFFluid               (1<< 7)
#define CFInter               (1<< 8)
#define CFNoNbFluid           (1<< 9)
#define CFNoNbEmpty           (1<<10)
#define CFNoDelete            (1<<11)
#define CFNoBndFluid          (1<<12)
	
//! refinement tags
// cell treated normally on coarser grids
#define CFGrNorm              (1<<13)
// border cells to be interpolated from finer grid
#define CFGrFromFine          (1<<14)
#define CFGrFromCoarse        (1<<15)
#define CFGrCoarseInited      (1<<16)
// 32k aux border marker 
#define CFGrToFine            (1<<17)
#define CFMbndInflow          (1<<18)
#define CFMbndOutflow         (1<<19)

// above 24 is used to encode in/outflow object type
#define CFPersistMask (0xFF000000 | CFMbndInflow | CFMbndOutflow)

// nk
#define CFInvalid             (CellFlagType)(1<<31)

// use 32bit flag types
#ifdef __x86_64__
 typedef int cfINT32;
#else
 typedef long cfINT32;
#endif // defined (_IA64)  
#define CellFlagType cfINT32
#define CellFlagTypeSize 4



/*****************************************************************************/
/*! a single lbm cell */
/*  the template is only needed for 
 *  dimension dependend constants e.g. 
 *  number of df's in model */
template<typename D>
class LbmCellTemplate {
	public:
		LbmFloat     df[ 27 ]; // be on the safe side here...
  	LbmFloat     rho;
		LbmVec       vel;
  	LbmFloat     mass;
		CellFlagType flag;
		BubbleId     bubble;
  	LbmFloat     ffrac;

		//! test if a flag is set 
		inline bool test(CellFlagType t) {
			return ((flag & t)==t);
		}
		//! test if any of the given flags is set 
		inline bool testAny(CellFlagType t) {
			return ((flag & t)!=0);
		}
		//! test if the cell is empty 
		inline bool isEmpty() {
			return (flag == CFEmpty);
		}

		//! init default values for a certain flag type
		inline void initDefaults(CellFlagType type) {
			flag = type;
			vel = LbmVec(0.0);
			for(int l=0; l<D::cDfNum;l++) df[l] = D::dfEquil[l];
				
			if(type & CFFluid) {
				rho = mass = ffrac = 1.0;
				bubble = -1;
			}
			else if(type & CFInter) {
				rho = mass = ffrac = 0.0;
				bubble = 0;
			}
			else if(type & CFBnd) {
				rho = mass = ffrac = 0.0;
				bubble = -1;
			}
			else if(type & CFEmpty) {
				rho = mass = ffrac = 0.0;
				bubble = 0;
			} else {
				// ?
				rho = mass = ffrac = 0.0;
				bubble = -1;
			}
		}

		//TODO add init method?
};


/* struct for the coordinates of a cell in the grid */
typedef struct {
  int x,y,z;
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

//! settings for a debug display
typedef struct fluidDispSettings_T {
	int            type;  // what to display
	bool           on;    // display enabled?
	float          scale; // additional scale param
} fluidDispSettings;



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
		virtual ~LbmSolverInterface() { };
		//! id string of solver
		virtual string getIdString() = 0;

		/*! finish the init with config file values (allocate arrays...) */
		virtual bool initialize( ntlTree *tree, vector<ntlGeometryObject*> *objects ) = 0;
		/*! generic test case setup using iterator interface */
		bool initGenericTestCases();

		/*! parse a boundary flag string */
		CellFlagType readBoundaryFlagInt(string name, int defaultValue, string source,string target, bool needed);
		/*! parse standard attributes */
		void parseStdAttrList();
		/*! initilize variables fom attribute list (should at least call std parse) */
		virtual void parseAttrList() = 0;

		virtual void step() = 0;
		virtual void prepareVisualization() { /* by default off */ };

		/*! particle handling */
		virtual int initParticles(ParticleTracer *partt) = 0;
		virtual void advanceParticles(ParticleTracer *partt ) = 0;
		/*! get surface object (NULL if no surface) */
		ntlGeometryObject* getSurfaceGeoObj() { return mpIso; }

		/*! debug object display */
		virtual vector<ntlGeometryObject*> getDebugObjects() { vector<ntlGeometryObject*> empty(0); return empty; }

#if LBM_USE_GUI==1
		/*! show simulation info */
		virtual void debugDisplay(fluidDispSettings *) = 0;
#endif

		/*! init tree for certain geometry init */
		void initGeoTree(int id);
		/*! destroy tree etc. when geometry init done */
		void freeGeoTree();
		/*! check for a certain flag type at position org (needed for e.g. quadtree refinement) */
		bool geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId, gfxReal &distance);
		bool geoInitCheckPointInside(ntlVec3Gfx org, ntlVec3Gfx dir, int flags, int &OId, gfxReal &distance, 
				const gfxReal halfCellsize, bool &thinHit, bool recurse);
		/*! set render globals, for scene/tree access */
		void setRenderGlobals(ntlRenderGlobals *glob) { mpGlob = glob; };
		/*! get max. velocity of all objects to initialize as fluid regions */
		ntlVec3Gfx getGeoMaxInitialVelocity();

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

		/*! set attr list pointer */
		void setAttrList(AttributeList *set) { mpAttrs = set; }
		/*! Returns the attribute list pointer */
		inline AttributeList *getAttributeList() { return mpAttrs; }

		/*! set parametrizer pointer */
		inline void setParametrizer(Parametrizer *set) { mpParam = set; }
		/*! get parametrizer pointer */
		inline Parametrizer *getParametrizer() { return mpParam; }

		/*! set density gradient init from e.g. init test cases */
		inline void setInitDensityGradient(bool set) { mInitDensityGradient = set; }

		/*! access geometry start vector */
		inline void setGeoStart(ntlVec3Gfx set)	{ mvGeoStart = set; }
		inline ntlVec3Gfx getGeoStart() const	{ return mvGeoStart; }

		/*! access geometry end vector */
		inline void setGeoEnd(ntlVec3Gfx set)	{ mvGeoEnd = set; }
		inline ntlVec3Gfx getGeoEnd() const	{ return mvGeoEnd; }

		/*! access name string */
		inline void setName(string set)	{ mName = set; }
		inline string getName() const	{ return mName; }

		/*! access string for node info debugging output */
		inline string getNodeInfoString() const { return mNodeInfoString; }

		/*! get panic flag */
		inline bool getPanic() { return mPanic; }

		//! set silent mode?
		inline void setSilent(bool set){ mSilent = set; }


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
		/*! get number of distribution functions */
		virtual int        getDfNum        ( ) = 0;
		/*! redundant cell functions */
		virtual LbmFloat   getCellDf       ( CellIdentifierInterface* ,int set, int dir) = 0;
		virtual LbmFloat   getCellMass     ( CellIdentifierInterface* ,int set) = 0;
		virtual LbmFloat   getCellFill     ( CellIdentifierInterface* ,int set) = 0;
		virtual CellFlagType getCellFlag   ( CellIdentifierInterface* ,int set) = 0;

		// gui/output debugging functions
#if LBM_USE_GUI==1
		virtual void debugDisplayNode(fluidDispSettings *dispset, CellIdentifier cell ) = 0;
		virtual void lbmDebugDisplay(fluidDispSettings *dispset) = 0;
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

#ifndef LBMDIM
		LBMDIM has to be defined
#endif
#if LBMDIM==2
		//! minimal and maximal z-coords (for 2D/3D loops) , this is always 0-1 for 2D
		int getForZMinBnd() { return 0; };
		int getForZMaxBnd() { return 1; };
		int getForZMin1()   { return 0; };
		int getForZMax1()   { return 1; };
#else // LBMDIM==2
		//! minimal and maximal z-coords (for 2D/3D loops)
		int getForZMinBnd() { return            0; };
		int getForZMaxBnd() { return getSizeZ()-0; };
		int getForZMin1()   { return            1; };
		int getForZMax1()   { return getSizeZ()-1; };
#endif // LBMDIM==2


	protected:

		/*! abort simulation on error... */
		bool mPanic;


		/*! Size of the array in x,y,z direction */
		int mSizex, mSizey, mSizez;


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

		/*! pointer to the attribute list */
		AttributeList *mpAttrs;

		/*! get parameters from this parametrize in finishInit */
		Parametrizer *mpParam;

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


		/*! an own random stream */
		ntlRandomStream mRandom;


		// geo init vars
		// TODO deprecate SimulationObject vars

		/*! for display - start and end vectors for geometry */
		ntlVec3Gfx mvGeoStart, mvGeoEnd;

		/*! perform geometry init? */
		bool mPerformGeoInit;
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

		/*! geometry init id */
		int mGeoInitId;
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

		// list for marked cells
		vector<CellIdentifierInterface *> mMarkedCells;
		int mMarkedCellIndex;
};


//! shorten static const definitions
#define STCON static const


/*****************************************************************************/
/*! class for solver templating - 3D implementation */
class LbmD3Q19 {

	public:

		// constructor, init interface
		LbmD3Q19() {};
		// virtual destructor 
		virtual ~LbmD3Q19() {};
		//! id string of solver
		string getIdString() { return string("3D"); }

		//! how many dimensions?
		STCON int cDimension;

		// Wi factors for collide step 
		STCON LbmFloat cCollenZero;
		STCON LbmFloat cCollenOne;
		STCON LbmFloat cCollenSqrtTwo;

		//! threshold value for filled/emptied cells 
		STCON LbmFloat cMagicNr2;
		STCON LbmFloat cMagicNr2Neg;
		STCON LbmFloat cMagicNr;
		STCON LbmFloat cMagicNrNeg;

		//! size of a single set of distribution functions 
		STCON int    cDfNum;
		//! direction vector contain vecs for all spatial dirs, even if not used for LBM model
		STCON int    cDirNum;

		//! distribution functions directions 
		typedef enum {
			 cDirInv=  -1,
			 cDirC  =  0,
			 cDirN  =  1,
			 cDirS  =  2,
			 cDirE  =  3,
			 cDirW  =  4,
			 cDirT  =  5,
			 cDirB  =  6,
			 cDirNE =  7,
			 cDirNW =  8,
			 cDirSE =  9,
			 cDirSW = 10,
			 cDirNT = 11,
			 cDirNB = 12,
			 cDirST = 13,
			 cDirSB = 14,
			 cDirET = 15,
			 cDirEB = 16,
			 cDirWT = 17,
			 cDirWB = 18
		} dfDir;

		/* Vector Order 3D:
		 *  0   1  2   3  4   5  6       7  8  9 10  11 12 13 14  15 16 17 18     19 20 21 22  23 24 25 26
		 *  0,  0, 0,  1,-1,  0, 0,      1,-1, 1,-1,  0, 0, 0, 0,  1, 1,-1,-1,     1,-1, 1,-1,  1,-1, 1,-1
		 *  0,  1,-1,  0, 0,  0, 0,      1, 1,-1,-1,  1, 1,-1,-1,  0, 0, 0, 0,     1, 1,-1,-1,  1, 1,-1,-1
		 *  0,  0, 0,  0, 0,  1,-1,      0, 0, 0, 0,  1,-1, 1,-1,  1,-1, 1,-1,     1, 1, 1, 1, -1,-1,-1,-1
		 */

		/*! name of the dist. function 
			 only for nicer output */
		STCON char* dfString[ 19 ];

		/*! index of normal dist func, not used so far?... */
		STCON int dfNorm[ 19 ];

		/*! index of inverse dist func, not fast, but useful... */
		STCON int dfInv[ 19 ];

		/*! index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefX[ 19 ];
		/*! index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefY[ 19 ];
		/*! index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefZ[ 19 ];

		/*! dist func vectors */
		STCON int dfVecX[ 27 ];
		STCON int dfVecY[ 27 ];
		STCON int dfVecZ[ 27 ];

		/*! arrays as before with doubles */
		STCON LbmFloat dfDvecX[ 27 ];
		STCON LbmFloat dfDvecY[ 27 ];
		STCON LbmFloat dfDvecZ[ 27 ];

		/*! principal directions */
		STCON int princDirX[ 2*3 ];
		STCON int princDirY[ 2*3 ];
		STCON int princDirZ[ 2*3 ];

		/*! vector lengths */
		STCON LbmFloat dfLength[ 19 ];

		/*! equilibrium distribution functions, precalculated = getCollideEq(i, 0,0,0,0) */
		static LbmFloat dfEquil[ 19 ];

		/*! arrays for les model coefficients */
		static LbmFloat lesCoeffDiag[ (3-1)*(3-1) ][ 27 ];
		static LbmFloat lesCoeffOffdiag[ 3 ][ 27 ];

}; // LbmData3D



/*****************************************************************************/
//! class for solver templating - 2D implementation 
class LbmD2Q9 {
	
	public:

		// constructor, init interface
		LbmD2Q9() {};
		// virtual destructor 
		virtual ~LbmD2Q9() {};
		//! id string of solver
		string getIdString() { return string("2D"); }

		//! how many dimensions?
		STCON int cDimension;

		//! Wi factors for collide step 
		STCON LbmFloat cCollenZero;
		STCON LbmFloat cCollenOne;
		STCON LbmFloat cCollenSqrtTwo;

		//! threshold value for filled/emptied cells 
		STCON LbmFloat cMagicNr2;
		STCON LbmFloat cMagicNr2Neg;
		STCON LbmFloat cMagicNr;
		STCON LbmFloat cMagicNrNeg;

		//! size of a single set of distribution functions 
		STCON int    cDfNum;
		STCON int    cDirNum;

		//! distribution functions directions 
		typedef enum {
			 cDirInv=  -1,
			 cDirC  =  0,
			 cDirN  =  1,
			 cDirS  =  2,
			 cDirE  =  3,
			 cDirW  =  4,
			 cDirNE =  5,
			 cDirNW =  6,
			 cDirSE =  7,
			 cDirSW =  8
		} dfDir;

		/* Vector Order 2D:
		 * 0  1 2  3  4  5  6 7  8
		 * 0, 0,0, 1,-1, 1,-1,1,-1 
		 * 0, 1,-1, 0,0, 1,1,-1,-1  */

		/* name of the dist. function 
			 only for nicer output */
		STCON char* dfString[ 9 ];

		/* index of normal dist func, not used so far?... */
		STCON int dfNorm[ 9 ];

		/* index of inverse dist func, not fast, but useful... */
		STCON int dfInv[ 9 ];

		/* index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefX[ 9 ];
		/* index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefY[ 9 ];
		/* index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefZ[ 9 ];

		/* dist func vectors */
		STCON int dfVecX[ 9 ];
		STCON int dfVecY[ 9 ];
		/* Z, 2D values are all 0! */
		STCON int dfVecZ[ 9 ];

		/* arrays as before with doubles */
		STCON LbmFloat dfDvecX[ 9 ];
		STCON LbmFloat dfDvecY[ 9 ];
		/* Z, 2D values are all 0! */
		STCON LbmFloat dfDvecZ[ 9 ];

		/*! principal directions */
		STCON int princDirX[ 2*2 ];
		STCON int princDirY[ 2*2 ];
		STCON int princDirZ[ 2*2 ];

		/* vector lengths */
		STCON LbmFloat dfLength[ 9 ];

		/* equilibrium distribution functions, precalculated = getCollideEq(i, 0,0,0,0) */
		static LbmFloat dfEquil[ 9 ];

		/*! arrays for les model coefficients */
		static LbmFloat lesCoeffDiag[ (2-1)*(2-1) ][ 9 ];
		static LbmFloat lesCoeffOffdiag[ 2 ][ 9 ];

}; // LbmData3D



// lbmdimensions

// not needed hereafter
#undef STCON



/*****************************************************************************/
//! class for solver templating - lbgk (srt) model implementation 
template<class DQ>
class LbmModelLBGK : public DQ , public LbmSolverInterface {
	public:

		/*! type for cells contents, needed for cell id interface */
		typedef DQ LbmCellContents;
		/*! type for cells */
		typedef LbmCellTemplate< LbmCellContents > LbmCell;

		// constructor
		LbmModelLBGK() : DQ(), LbmSolverInterface() {};
		// virtual destructor 
		virtual ~LbmModelLBGK() {};
		//! id string of solver
		string getIdString() { return DQ::getIdString() + string("lbgk]"); }

		/*! calculate length of velocity vector */
		static inline LbmFloat getVelVecLen(int l, LbmFloat ux,LbmFloat uy,LbmFloat uz) {
			return ((ux)*DQ::dfDvecX[l]+(uy)*DQ::dfDvecY[l]+(uz)*DQ::dfDvecZ[l]);
		};

		/*! calculate equilibrium DF for given values */
		static inline LbmFloat getCollideEq(int l, LbmFloat rho,  LbmFloat ux, LbmFloat uy, LbmFloat uz) {
			LbmFloat tmp = getVelVecLen(l,ux,uy,uz); 
			return( DQ::dfLength[l] *( 
						+ rho - (3.0/2.0*(ux*ux + uy*uy + uz*uz)) 
						+ 3.0 *tmp 
						+ 9.0/2.0 *(tmp*tmp) ) 
					);
		};


		// input mux etc. as acceleration
		// outputs rho,ux,uy,uz
		/*inline void collideArrays_org(LbmFloat df[19], 				
				LbmFloat &outrho, // out only!
				// velocity modifiers (returns actual velocity!)
				LbmFloat &mux, LbmFloat &muy, LbmFloat &muz, 
				LbmFloat omega
			) {
			LbmFloat rho=df[0]; 
			LbmFloat ux = mux;
			LbmFloat uy = muy;
			LbmFloat uz = muz;
			for(int l=1; l<DQ::cDfNum; l++) { 
				rho += df[l]; 
				ux  += (DQ::dfDvecX[l]*df[l]); 
				uy  += (DQ::dfDvecY[l]*df[l]);  
				uz  += (DQ::dfDvecZ[l]*df[l]);  
			}  
			for(int l=0; l<DQ::cDfNum; l++) { 
				//LbmFloat tmp = (ux*DQ::dfDvecX[l]+uy*DQ::dfDvecY[l]+uz*DQ::dfDvecZ[l]); 
				df[l] = (1.0-omega ) * df[l] + omega * ( getCollideEq(l,rho,ux,uy,uz) ); 
			}  

			mux = ux;
			muy = uy;
			muz = uz;
			outrho = rho;
		};*/
		
		// LES functions
		inline LbmFloat getLesNoneqTensorCoeff(
				LbmFloat df[], 				
				LbmFloat feq[] ) {
			LbmFloat Qo = 0.0;
			for(int m=0; m< ((DQ::cDimension*DQ::cDimension)-DQ::cDimension)/2 ; m++) { 
				LbmFloat qadd = 0.0;
				for(int l=1; l<DQ::cDfNum; l++) { 
					if(DQ::lesCoeffOffdiag[m][l]==0.0) continue;
					qadd += DQ::lesCoeffOffdiag[m][l]*(df[l]-feq[l]);
				}
				Qo += (qadd*qadd);
			}
			Qo *= 2.0; // off diag twice
			for(int m=0; m<DQ::cDimension; m++) { 
				LbmFloat qadd = 0.0;
				for(int l=1; l<DQ::cDfNum; l++) { 
					if(DQ::lesCoeffDiag[m][l]==0.0) continue;
					qadd += DQ::lesCoeffDiag[m][l]*(df[l]-feq[l]);
				}
				Qo += (qadd*qadd);
			}
			Qo = sqrt(Qo);
			return Qo;
		}
		inline LbmFloat getLesOmega(LbmFloat omega, LbmFloat csmago, LbmFloat Qo) {
			const LbmFloat tau = 1.0/omega;
			const LbmFloat nu = (2.0*tau-1.0) * (1.0/6.0);
			const LbmFloat C = csmago;
			const LbmFloat Csqr = C*C;
			LbmFloat S = -nu + sqrt( nu*nu + 18.0*Csqr*Qo ) / (6.0*Csqr);
			return( 1.0/( 3.0*( nu+Csqr*S ) +0.5 ) );
		}

		// "normal" collision
		inline void collideArrays(LbmFloat df[], 				
				LbmFloat &outrho, // out only!
				// velocity modifiers (returns actual velocity!)
				LbmFloat &mux, LbmFloat &muy, LbmFloat &muz, 
				LbmFloat omega, LbmFloat csmago, LbmFloat *newOmegaRet = NULL
			) {
			LbmFloat rho=df[0]; 
			LbmFloat ux = mux;
			LbmFloat uy = muy;
			LbmFloat uz = muz; 
			for(int l=1; l<DQ::cDfNum; l++) { 
				rho += df[l]; 
				ux  += (DQ::dfDvecX[l]*df[l]); 
				uy  += (DQ::dfDvecY[l]*df[l]);  
				uz  += (DQ::dfDvecZ[l]*df[l]);  
			}  
			LbmFloat feq[19];
			for(int l=0; l<DQ::cDfNum; l++) { 
				feq[l] = getCollideEq(l,rho,ux,uy,uz); 
			}

			LbmFloat omegaNew;
			if(csmago>0.0) {
				LbmFloat Qo = getLesNoneqTensorCoeff(df,feq);
				omegaNew = getLesOmega(omega,csmago,Qo);
			} else {
				omegaNew = omega; // smago off...
			}
			if(newOmegaRet) *newOmegaRet=omegaNew; // return value for stats

			for(int l=0; l<DQ::cDfNum; l++) { 
				df[l] = (1.0-omegaNew ) * df[l] + omegaNew * feq[l]; 
			}  

			mux = ux;
			muy = uy;
			muz = uz;
			outrho = rho;
		};

}; // LBGK

#ifdef LBMMODEL_DEFINED
// force compiler error!
ERROR - Dont include several LBM models at once...
#endif
#define LBMMODEL_DEFINED 1


typedef LbmModelLBGK<  LbmD2Q9 > LbmBGK2D;
typedef LbmModelLBGK< LbmD3Q19 > LbmBGK3D;


//! helper function to convert flag to string (for debuggin)
string convertCellFlagType2String( CellFlagType flag );
string convertSingleFlag2String(CellFlagType cflag);

#endif // LBMINTERFACE_H
