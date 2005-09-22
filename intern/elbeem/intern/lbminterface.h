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
#ifdef _WIN32
#include <windows.h>
#endif
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
// 1
#define CFUnused              (1<< 0)
// 2
#define CFEmpty               (1<< 1)
// 4
#define CFBnd                 (1<< 2)
// 8, force symmetry for flag reinit
#define CFNoInterpolSrc       (1<< 3) 
// 16
#define CFFluid               (1<< 4)
// 32
#define CFInter               (1<< 5)
// 64
#define CFNoNbFluid           (1<< 6)
// 128
#define CFNoNbEmpty           (1<< 7)
// 256
#define CFNoDelete            (1<< 8)

// 512
#define CFNoBndFluid          (1<< 9)
// 1024
#define CFBndMARK             (1<<10) 
	
//! refinement tags
// cell treated normally on coarser grids
// 2048
#define CFGrNorm              (1<<11)
// border cells to be interpolated from finer grid
// 4096
#define CFGrFromFine          (1<<12)
// 8192
#define CFGrFromCoarse        (1<<13)
// 16384
#define CFGrCoarseInited      (1<<14)
// 32k (aux marker, no real action)
#define CFGrToFine            (1<<15)

// nk
#define CFInvalid             (CellFlagType)(1<<31)

// use 16bit flag types
//#define CellFlagType unsigned short int
// use 32bit flag types
#define CellFlagType unsigned long int


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
		CellIdentifierInterface() : mEnd (false) { };
		//! virtual destructor
		virtual ~CellIdentifierInterface() {};

		//! return node as string (with some basic info)
		virtual std::string getAsString() = 0;

		//! compare cids
		virtual bool equal(CellIdentifierInterface* other) = 0;

		//! set/get end flag
		inline void setEnd(bool set){ mEnd = set;  }
		inline bool getEnd( )       { return mEnd; }

	protected:
		
		//! has the grid been traversed?
		bool mEnd;

};


/*****************************************************************************/
/*! marked cell access class *
class MarkedCellIdentifier : 
	public CellIdentifierInterface 
{
	public:
		//! cell pointer
		CellIdentifierInterface *mpCell;
		//! location in mMarkedCells vector
		int mIndex;

		//! reset constructor
		MarkedCellIdentifier() :
			mpCell( NULL ), mIndex(0)
			{ };

		// implement CellIdentifierInterface
		virtual std::string getAsString() {
			std::ostringstream ret;
			ret <<"{MC i"<<mIndex<<" }";
			return ret.str();
		}

		virtual bool equal(CellIdentifierInterface* other) {
			MarkedCellIdentifier *cid = dynamic_cast<MarkedCellIdentifier *>( other );
			if(!cid) return false;
			if( mpCell==cid->mpCell ) return true;
			return false;
		}
}; */



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
		/*! get fluid init type at certain position */
		// DEPRECATED CellFlagType geoInitGetPointType(ntlVec3Gfx org, ntlVec3Gfx nodesize, ntlGeometryObject **mpObj, gfxReal &distance);
		/*! check for a certain flag type at position org (needed for e.g. quadtree refinement) */
		bool geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId, gfxReal &distance);
		/*! set render globals, for scene/tree access */
		void setRenderGlobals(ntlRenderGlobals *glob) { mpGlob = glob; };
		/*! get max. velocity of all objects to initialize as fluid regions */
		ntlVec3Gfx getGeoMaxInitialVelocity();

		/* rt interface functions */
		unsigned int getIsoVertexCount() { return mpIso->getIsoVertexCount(); }
		unsigned int getIsoIndexCount() { return mpIso->getIsoIndexCount(); }
		char* getIsoVertexArray() { return mpIso->getIsoVertexArray(); }
		unsigned int *getIsoIndexArray() { return mpIso->getIsoIndexArray(); }
		void triangulateSurface() { return mpIso->triangulate(); }
		// drop stuff
		//virtual void addDrop(bool active, float mx, float my) = 0;
		//! avg. used cell count stats
		//virtual void printCellStats() = 0;
		//! check end time for gfx ani
		//virtual int checkGfxEndTime() = 0;
		//virtual int getGfxGeoSetup() = 0;

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


		// debugging cell marker functions

		//! add cell to mMarkedCells list
		void addCellToMarkedList( CellIdentifierInterface *cid );
		//! marked cell iteration methods
		CellIdentifierInterface* markedGetFirstCell( );
		void markedAdvanceCell( CellIdentifierInterface* pcid );
		bool markedNoEndCell( CellIdentifierInterface* cid );
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


		/*! initial mass to display changes */
		LbmFloat mInitialMass;
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
		/*! remember globals */
		ntlRenderGlobals *mpGlob;

		// list for marked cells
		std::vector<CellIdentifierInterface *> mMarkedCells;
};


//! helper function to convert flag to string (for debuggin)
std::string convertCellFlagType2String( CellFlagType flag );
std::string convertSingleFlag2String(CellFlagType cflag);

#endif // LBMINTERFACE_H
