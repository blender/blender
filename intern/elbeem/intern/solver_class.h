/** \file elbeem/intern/solver_class.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - the visual lattice boltzmann freesurface simulator
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann standard solver classes
 *
 *****************************************************************************/


#ifndef LBM_SOLVERCLASS_H
#define LBM_SOLVERCLASS_H

#include "utilities.h"
#include "solver_interface.h"
#include "ntl_ray.h"
#include <stdio.h>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#if PARALLEL==1
#include <omp.h>
#endif // PARALLEL=1
#ifndef PARALLEL
#define PARALLEL 0
#endif // PARALLEL


// general solver setting defines

//! debug coordinate accesses and the like? (much slower)
//  might be enabled by compilation
#ifndef FSGR_STRICT_DEBUG
#define FSGR_STRICT_DEBUG 0
#endif // FSGR_STRICT_DEBUG

//! debug coordinate accesses and the like? (much slower)
#define FSGR_OMEGA_DEBUG 0

//! OPT3D quick LES on/off, only debug/benchmarking
#define USE_LES 1

//! order of interpolation (0=always current/1=interpolate/2=always other)
//#define TIMEINTORDER 0
// TODO remove interpol t param, also interTime

// use optimized 3D code?
#if LBMDIM==2
#define OPT3D 0
#else
// determine with debugging...
#	if FSGR_STRICT_DEBUG==1
#		define OPT3D 0
#	else // FSGR_STRICT_DEBUG==1
// usually switch optimizations for 3d on, when not debugging
#		define OPT3D 1
// COMPRT
//#		define OPT3D 0
#	endif // FSGR_STRICT_DEBUG==1
#endif

//! invalid mass value for unused mass data
#define MASS_INVALID -1000.0

// empty/fill cells without fluid/empty NB's by inserting them into the full/empty lists?
#define FSGR_LISTTRICK          1
#define FSGR_LISTTTHRESHEMPTY   0.10
#define FSGR_LISTTTHRESHFULL    0.90
#define FSGR_MAGICNR            0.025
//0.04

//! maxmimum no. of grid levels
#define FSGR_MAXNOOFLEVELS 5

// enable/disable fine grid compression for finest level
// make sure this is same as useGridComp in calculateMemreqEstimate
#if LBMDIM==3
#define COMPRESSGRIDS 1
#else 
#define COMPRESSGRIDS 0
#endif 

// helper for comparing floats with epsilon
#define GFX_FLOATNEQ(x,y) ( ABS((x)-(y)) > (VECTOR_EPSILON) )
#define LBM_FLOATNEQ(x,y) ( ABS((x)-(y)) > (10.0*LBM_EPSILON) )


// macros for loops over all DFs
#define FORDF0 for(int l= 0; l< LBM_DFNUM; ++l)
#define FORDF1 for(int l= 1; l< LBM_DFNUM; ++l)
// and with different loop var to prevent shadowing
#define FORDF0M for(int m= 0; m< LBM_DFNUM; ++m)
#define FORDF1M for(int m= 1; m< LBM_DFNUM; ++m)

// iso value defines
// border for marching cubes
#define ISOCORR 3

#define LBM_INLINED  inline

// sirdude fix for solaris
#if !defined(linux) && defined(sun)
#include "ieeefp.h"
#ifndef expf
#define expf(x) exp((double)(x))
#endif
#endif

#include "solver_control.h"

#if LBM_INCLUDE_TESTSOLVERS==1
#include "solver_test.h"
#endif // LBM_INCLUDE_TESTSOLVERS==1

/*****************************************************************************/
/*! cell access classes */
class UniformFsgrCellIdentifier : 
	public CellIdentifierInterface , public LbmCellContents
{
	public:
		//! which grid level?
		int level;
		//! location in grid
		int x,y,z;

		//! reset constructor
		UniformFsgrCellIdentifier() :
			x(0), y(0), z(0) { };

		// implement CellIdentifierInterface
		virtual string getAsString() {
			std::ostringstream ret;
			ret <<"{ i"<<x<<",j"<<y;
			if(LBMDIM>2) ret<<",k"<<z;
			ret <<" }";
			return ret.str();
		}

		virtual bool equal(CellIdentifierInterface* other) {
			UniformFsgrCellIdentifier *cid = (UniformFsgrCellIdentifier *)( other );
			if(!cid) return false;
			if( x==cid->x && y==cid->y && z==cid->z && level==cid->level ) return true;
			return false;
		}

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:UniformFsgrCellIdentifier")
#endif
};

//! information needed for each level in the simulation
class FsgrLevelData {
public:
	int id; // level number

	//! node size on this level (geometric, in world coordinates, not simulation units!) 
	LbmFloat nodeSize;
	//! node size on this level in simulation units 
	LbmFloat simCellSize;
	//! quadtree node relaxation parameter 
	LbmFloat omega;
	//! size this level was advanced to 
	LbmFloat time;
	//! size of a single lbm step in time units on this level 
	LbmFloat timestep;
	//! step count
	int lsteps;
	//! gravity force for this level
	LbmVec gravity;
	//! level array 
	LbmFloat *mprsCells[2];
	CellFlagType *mprsFlags[2];

	//! smago params and precalculated values
	LbmFloat lcsmago;
	LbmFloat lcsmago_sqr;
	LbmFloat lcnu;

	// LES statistics per level
	double avgOmega;
	double avgOmegaCnt;

	//! current set of dist funcs 
	int setCurr;
	//! target/other set of dist funcs 
	int setOther;

	//! mass&volume for this level
	LbmFloat lmass;
	LbmFloat lvolume;
	LbmFloat lcellfactor;

	//! local storage of mSizes
	int lSizex, lSizey, lSizez;
	int lOffsx, lOffsy, lOffsz;

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:FsgrLevelData")
#endif
};



/*****************************************************************************/
/*! class for solving a LBM problem */
class LbmFsgrSolver : 
	public LbmSolverInterface // this means, the solver is a lbmData object and implements the lbmInterface
{

	public:
		//! Constructor 
		LbmFsgrSolver();
		//! Destructor 
		virtual ~LbmFsgrSolver();

		//! initilize variables fom attribute list 
		virtual void parseAttrList();
		//! Initialize omegas and forces on all levels (for init/timestep change)
		void initLevelOmegas();

		// multi step solver init
		/*! finish the init with config file values (allocate arrays...) */
		virtual bool initializeSolverMemory();
		/*! init solver arrays */
		virtual bool initializeSolverGrids();
		/*! prepare actual simulation start, setup viz etc */
		virtual bool initializeSolverPostinit();

		//! notify object that dump is in progress (e.g. for field dump) 
		virtual void notifySolverOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename);

#		if LBM_USE_GUI==1
		//! show simulation info (implement LbmSolverInterface pure virtual func)
		virtual void debugDisplay(int set);
#		endif
		
		// implement CellIterator<UniformFsgrCellIdentifier> interface
		typedef UniformFsgrCellIdentifier stdCellId;
		virtual CellIdentifierInterface* getFirstCell( );
		virtual void advanceCell( CellIdentifierInterface* );
		virtual bool noEndCell( CellIdentifierInterface* );
		virtual void deleteCellIterator( CellIdentifierInterface** );
		virtual CellIdentifierInterface* getCellAt( ntlVec3Gfx pos );
		virtual int        getCellSet      ( CellIdentifierInterface* );
		virtual ntlVec3Gfx getCellOrigin   ( CellIdentifierInterface* );
		virtual ntlVec3Gfx getCellSize     ( CellIdentifierInterface* );
		virtual int        getCellLevel    ( CellIdentifierInterface* );
		virtual LbmFloat   getCellDensity  ( CellIdentifierInterface* ,int set);
		virtual LbmVec     getCellVelocity ( CellIdentifierInterface* ,int set);
		virtual LbmFloat   getCellDf       ( CellIdentifierInterface* ,int set, int dir);
		virtual LbmFloat   getCellMass     ( CellIdentifierInterface* ,int set);
		virtual LbmFloat   getCellFill     ( CellIdentifierInterface* ,int set);
		virtual CellFlagType getCellFlag   ( CellIdentifierInterface* ,int set);
		virtual LbmFloat   getEquilDf      ( int );
		virtual ntlVec3Gfx getVelocityAt   (float x, float y, float z);
		// convert pointers
		stdCellId* convertBaseCidToStdCid( CellIdentifierInterface* basecid);

		//! perform geometry init (if switched on) 
		bool initGeometryFlags();
		//! init part for all freesurface testcases 
		void initFreeSurfaces();
		//! init density gradient if enabled
		void initStandingFluidGradient();

 		/*! init a given cell with flag, density, mass and equilibrium dist. funcs */
		LBM_INLINED void initEmptyCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass);
		LBM_INLINED void initVelocityCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass, LbmVec vel);
		LBM_INLINED void changeFlag(int level, int xx,int yy,int zz,int set,CellFlagType newflag);
		LBM_INLINED void forceChangeFlag(int level, int xx,int yy,int zz,int set,CellFlagType newflag);
		LBM_INLINED void initInterfaceVars(int level, int i,int j,int k,int workSet, bool initMass);
		//! interpolate velocity and density at a given position
		void interpolateCellValues(int level,int ei,int ej,int ek,int workSet, LbmFloat &retrho, LbmFloat &retux, LbmFloat &retuy, LbmFloat &retuz);

		/*! perform a single LBM step */
		void stepMain();
		//! advance fine grid
		void fineAdvance();
		//! advance coarse grid
		void coarseAdvance(int lev);
		//! update flux area values on coarse grids
		void coarseCalculateFluxareas(int lev);
		// adaptively coarsen grid
		bool adaptGrid(int lev);
		// restrict fine grid DFs to coarse grid
		void coarseRestrictFromFine(int lev);

		/* simulation object interface, just calls stepMain */
		virtual void step();
		/*! init particle positions */
		virtual int initParticles();
		/*! move all particles */
		virtual void advanceParticles();
		/*! move a particle at a boundary */
		void handleObstacleParticle(ParticleObject *p);
		/*! check whether to add particle 
		bool checkAddParticle();
		void performAddParticle();*/


		/*! debug object display (used e.g. for preview surface) */
		virtual vector<ntlGeometryObject*> getDebugObjects();
	
		// gui/output debugging functions
#		if LBM_USE_GUI==1
		virtual void debugDisplayNode(int dispset, CellIdentifierInterface* cell );
		virtual void lbmDebugDisplay(int dispset);
		virtual void lbmMarkedCellDisplay();
#		endif // LBM_USE_GUI==1
		virtual void debugPrintNodeInfo(CellIdentifierInterface* cell, int forceSet=-1);

		//! for raytracing, preprocess
		void prepareVisualization( void );

		/* surface generation settings */
		virtual void setSurfGenSettings(short value);

	protected:

		//! internal quick print function (for debugging) 
		void printLbmCell(int level, int i, int j, int k,int set);
		// debugging use CellIterator interface to mark cell
		void debugMarkCellCall(int level, int vi,int vj,int vk);
		
		// loop over grid, stream&collide update
		void mainLoop(int lev);
		// change time step size
		void adaptTimestep();
		//! init mObjectSpeeds for current parametrization
		void recalculateObjectSpeeds();
		//! init moving obstacles for next sim step sim 
		void initMovingObstacles(bool staticInit);
		//! flag reinit step - always works on finest grid!
		void reinitFlags( int workSet );
		//! mass dist weights
		LbmFloat getMassdWeight(bool dirForw, int i,int j,int k,int workSet, int l);
		//! compute surface normals: fluid, fluid more accurate, and for obstacles
		void computeFluidSurfaceNormal(LbmFloat *cell, CellFlagType *cellflag,    LbmFloat *snret);
		void computeFluidSurfaceNormalAcc(LbmFloat *cell, CellFlagType *cellflag, LbmFloat *snret);
		void computeObstacleSurfaceNormal(LbmFloat *cell, CellFlagType *cellflag, LbmFloat *snret);
		void computeObstacleSurfaceNormalAcc(int i,int j,int k, LbmFloat *snret);
		//! add point to mListNewInter list
		LBM_INLINED void addToNewInterList( int ni, int nj, int nk );	
		//! cell is interpolated from coarse level (inited into set, source sets are determined by t)
		void interpolateCellFromCoarse(int lev, int i, int j,int k, int dstSet, LbmFloat t, CellFlagType flagSet,bool markNbs);
		void coarseRestrictCell(int lev, int i,int j,int k, int srcSet, int dstSet);

		//! minimal and maximal z-coords (for 2D/3D loops)
		LBM_INLINED int getForZMinBnd();
		LBM_INLINED int getForZMin1();
		LBM_INLINED int getForZMaxBnd(int lev);
		LBM_INLINED int getForZMax1(int lev);
		LBM_INLINED bool checkDomainBounds(int lev,int i,int j,int k);
		LBM_INLINED bool checkDomainBoundsPos(int lev,LbmVec pos);

		// touch grid and flags once
		void preinitGrids();
		// one relaxation step for standing fluid
		void standingFluidPreinit();


		// member vars

		//! mass calculated during streaming step
		LbmFloat mCurrentMass;
		LbmFloat mCurrentVolume;
		LbmFloat mInitialMass;

		//! count problematic cases, that occured so far...
		int mNumProblems;

		// average mlsups, count how many so far...
		double mAvgMLSUPS;
		double mAvgMLSUPSCnt;

		//! Mcubes object for surface reconstruction 
		IsoSurface *mpPreviewSurface;
		
		//! use time adaptivity? 
		bool mTimeAdap;
		//! force smaller timestep for next LBM step? (eg for mov obj)
		bool mForceTimeStepReduce;

		//! fluid vol height
		LbmFloat mFVHeight;
		LbmFloat mFVArea;
		bool mUpdateFVHeight;

		//! force quit for gfx
		LbmFloat mGfxEndTime;
		//! smoother surface initialization?
		int mInitSurfaceSmoothing;
		//! surface generation settings, default is all off (=0)
		//  each flag switches side on off,  fssgNoObs is for obstacle sides
		//  -1 equals all on
		typedef enum {
			 fssgNormal   =  0,
			 fssgNoNorth  =  1,
			 fssgNoSouth  =  2,
			 fssgNoEast   =  4,
			 fssgNoWest   =  8,
			 fssgNoTop    = 16,
			 fssgNoBottom = 32,
			 fssgNoObs    = 64
		} fsSurfaceGen;
		int mFsSurfGenSetting;

		//! lock time step down switching
		int mTimestepReduceLock;
		//! count no. of switches
		int mTimeSwitchCounts;
		// only switch of maxvel is higher for several steps...
		int mTimeMaxvelStepCnt;

		//! total simulation time so far 
		LbmFloat mSimulationTime, mLastSimTime;
		//! smallest and largest step size so far 
		LbmFloat mMinTimestep, mMaxTimestep;
		//! track max. velocity
		LbmFloat mMxvx, mMxvy, mMxvz, mMaxVlen;

		//! list of the cells to empty at the end of the step 
		vector<LbmPoint> mListEmpty;
		//! list of the cells to make fluid at the end of the step 
		vector<LbmPoint> mListFull;
		//! list of new interface cells to init
  	vector<LbmPoint> mListNewInter;
		//! class for handling redist weights in reinit flag function
		class lbmFloatSet {
			public:
				LbmFloat val[dTotalNum];
				LbmFloat numNbs;
		};
		//! normalized vectors for all neighboring cell directions (for e.g. massdweight calc)
		LbmVec mDvecNrm[27];
		
		
		//! debugging
		bool checkSymmetry(string idstring);
		//! kepp track of max/min no. of filled cells
		int mMaxNoCells, mMinNoCells;
		LONGINT mAvgNumUsedCells;

		//! precalculated objects speeds for current parametrization
		vector<LbmVec> mObjectSpeeds;
		//! partslip bc. values for obstacle boundary conditions
		vector<LbmFloat> mObjectPartslips;
		//! moving object mass boundary condition values
		vector<LbmFloat> mObjectMassMovnd;

		//! permanent movobj vert storage
	  vector<ntlVec3Gfx>  mMOIVertices;
  	vector<ntlVec3Gfx>  mMOIVerticesOld;
	  vector<ntlVec3Gfx>  mMOINormals;

		//! get isofield weights
		int mIsoWeightMethod;
		float mIsoWeight[27];

		// grid coarsening vars
		
		/*! vector for the data for each level */
		FsgrLevelData mLevel[FSGR_MAXNOOFLEVELS];

		/*! minimal and maximal refinement levels */
		int mMaxRefine;

		/*! df scale factors for level up/down */
		LbmFloat mDfScaleUp, mDfScaleDown;

		/*! precomputed cell area values */
		LbmFloat mFsgrCellArea[27];
		/*! restriction interpolation weights */
		LbmFloat mGaussw[27];

		/*! LES C_smago paramter for finest grid */
		float mInitialCsmago;
		/*! LES stats for non OPT3D */
		LbmFloat mDebugOmegaRet;
		/*! remember last init for animated params */
		LbmFloat mLastOmega;
		LbmVec   mLastGravity;

		//! fluid stats
		int mNumInterdCells;
		int mNumInvIfCells;
		int mNumInvIfTotal;
		int mNumFsgrChanges;

		//! debug function to disable standing f init
		int mDisableStandingFluidInit;
		//! init 2d with skipped Y/Z coords
		bool mInit2dYZ;
		//! debug function to force tadap syncing
		int mForceTadapRefine;
		//! border cutoff value
		int mCutoff;

		// strict debug interface
#		if FSGR_STRICT_DEBUG==1
		int debLBMGI(int level, int ii,int ij,int ik, int is);
		CellFlagType& debRFLAG(int level, int xx,int yy,int zz,int set);
		CellFlagType& debRFLAG_NB(int level, int xx,int yy,int zz,int set, int dir);
		CellFlagType& debRFLAG_NBINV(int level, int xx,int yy,int zz,int set, int dir);
		int debLBMQI(int level, int ii,int ij,int ik, int is, int l);
		LbmFloat& debQCELL(int level, int xx,int yy,int zz,int set,int l);
		LbmFloat& debQCELL_NB(int level, int xx,int yy,int zz,int set, int dir,int l);
		LbmFloat& debQCELL_NBINV(int level, int xx,int yy,int zz,int set, int dir,int l);
		LbmFloat* debRACPNT(int level,  int ii,int ij,int ik, int is );
		LbmFloat& debRAC(LbmFloat* s,int l);
#		endif // FSGR_STRICT_DEBUG==1

		LbmControlData *mpControl;

		void initCpdata();
		void handleCpdata();
		void cpDebugDisplay(int dispset); 

		bool mUseTestdata;
#		if LBM_INCLUDE_TESTSOLVERS==1
		// test functions
		LbmTestdata *mpTest;
		void initTestdata();
		void destroyTestdata();
		void handleTestdata();
		void set3dHeight(int ,int );

		int mMpNum,mMpIndex;
		int mOrgSizeX;
		LbmFloat mOrgStartX;
		LbmFloat mOrgEndX;
		void mrSetup();
		void mrExchange(); 
		void mrIsoExchange(); 
		LbmFloat mrInitTadap(LbmFloat max); 
		void gcFillBuffer(  LbmGridConnector *gc, int *retSizeCnt, const int *bdfs);
		void gcUnpackBuffer(LbmGridConnector *gc, int *retSizeCnt, const int *bdfs);
	public:
		// needed for testdata
		void find3dHeight(int i,int j, LbmFloat prev, LbmFloat &ret, LbmFloat *retux, LbmFloat *retuy, LbmFloat *retuz);
		// mptest
		int getMpIndex() { return mMpIndex; };
#		endif // LBM_INCLUDE_TESTSOLVERS==1

		// former LbmModelLBGK  functions
		// relaxation funtions - implemented together with relax macros
		static inline LbmFloat getVelVecLen(int l, LbmFloat ux,LbmFloat uy,LbmFloat uz);
		static inline LbmFloat getCollideEq(int l, LbmFloat rho,  LbmFloat ux, LbmFloat uy, LbmFloat uz);
		inline LbmFloat getLesNoneqTensorCoeff( LbmFloat df[], 				LbmFloat feq[] );
		inline LbmFloat getLesOmega(LbmFloat omega, LbmFloat csmago, LbmFloat Qo);
		inline void collideArrays( int lev, int i, int j, int k, // position - more for debugging
				LbmFloat df[], LbmFloat &outrho, // out only!
				// velocity modifiers (returns actual velocity!)
				LbmFloat &mux, LbmFloat &muy, LbmFloat &muz, 
				LbmFloat omega, LbmVec gravity, LbmFloat csmago, 
				LbmFloat *newOmegaRet, LbmFloat *newQoRet);


		// former LBM models
		//! shorten static const definitions
#		define STCON static const

#		if LBMDIM==3
		
		//! id string of solver
		virtual string getIdString() { return string("FreeSurfaceFsgrSolver[BGK_D3Q19]"); }

		//! how many dimensions? UNUSED? replace by LBMDIM?
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
		static LbmFloat dfEquil[ dTotalNum ];

		/*! arrays for les model coefficients */
		static LbmFloat lesCoeffDiag[ (3-1)*(3-1) ][ 27 ];
		static LbmFloat lesCoeffOffdiag[ 3 ][ 27 ];

#		else // end LBMDIM==3 , LBMDIM==2
		
		//! id string of solver
		virtual string getIdString() { return string("FreeSurfaceFsgrSolver[BGK_D2Q9]"); }

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
		static LbmFloat dfEquil[ dTotalNum ];

		/*! arrays for les model coefficients */
		static LbmFloat lesCoeffDiag[ (2-1)*(2-1) ][ 9 ];
		static LbmFloat lesCoeffOffdiag[ 2 ][ 9 ];

#		endif  // LBMDIM==2

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:LbmFsgrSolver")
#endif
};

#undef STCON


/*****************************************************************************/
// relaxation_macros



// cell mark debugging
#if FSGR_STRICT_DEBUG==10
#define debugMarkCell(lev,x,y,z) \
	errMsg("debugMarkCell",this->mName<<" step: "<<this->mStepCnt<<" lev:"<<(lev)<<" marking "<<PRINT_VEC((x),(y),(z))<<" line "<< __LINE__ ); \
	debugMarkCellCall((lev),(x),(y),(z));
#else // FSGR_STRICT_DEBUG==1
#define debugMarkCell(lev,x,y,z) \
	debugMarkCellCall((lev),(x),(y),(z));
#endif // FSGR_STRICT_DEBUG==1


// flag array defines -----------------------------------------------------------------------------------------------

// lbm testsolver get index define, note - ignores is (set) as flag
// array is only a single entry
#define _LBMGI(level, ii,ij,ik, is) ( (LONGINT)((LONGINT)mLevel[level].lOffsy*(LONGINT)(ik)) + ((LONGINT)mLevel[level].lOffsx*(LONGINT)(ij)) + (LONGINT)(ii) )

//! flag array acces macro
#define _RFLAG(level,xx,yy,zz,set) mLevel[level].mprsFlags[set][ (LONGINT)LBMGI((level),(xx),(yy),(zz),(set)) ]
#define _RFLAG_NB(level,xx,yy,zz,set, dir) mLevel[level].mprsFlags[set][ (LONGINT)LBMGI((level),(xx)+this->dfVecX[dir],(yy)+this->dfVecY[dir],(zz)+this->dfVecZ[dir],set) ]
#define _RFLAG_NBINV(level,xx,yy,zz,set, dir) mLevel[level].mprsFlags[set][ (LONGINT)LBMGI((level),(xx)+this->dfVecX[this->dfInv[dir]],(yy)+this->dfVecY[this->dfInv[dir]],(zz)+this->dfVecZ[this->dfInv[dir]],set) ]

// array handling  -----------------------------------------------------------------------------------------------

#define _LBMQI(level, ii,ij,ik, is, lunused) ( (LONGINT)((LONGINT)mLevel[level].lOffsy*(LONGINT)(ik)) + (LONGINT)((LONGINT)mLevel[level].lOffsx*(LONGINT)(ij)) + (LONGINT)(ii) )
#define _QCELL(level,xx,yy,zz,set,l) (mLevel[level].mprsCells[(set)][ (LONGINT)LBMQI((level),(xx),(yy),(zz),(set), l)*(LONGINT)dTotalNum +(LONGINT)(l)])
#define _QCELL_NB(level,xx,yy,zz,set, dir,l) (mLevel[level].mprsCells[(set)][ (LONGINT)LBMQI((level),(xx)+this->dfVecX[dir],(yy)+this->dfVecY[dir],(zz)+this->dfVecZ[dir],set, l)*dTotalNum +(l)])
#define _QCELL_NBINV(level,xx,yy,zz,set, dir,l) (mLevel[level].mprsCells[(set)][ (LONGINT)LBMQI((level),(xx)+this->dfVecX[this->dfInv[dir]],(yy)+this->dfVecY[this->dfInv[dir]],(zz)+this->dfVecZ[this->dfInv[dir]],set, l)*dTotalNum +(l)])

#define QCELLSTEP dTotalNum
#define _RACPNT(level, ii,ij,ik, is )  &QCELL(level,ii,ij,ik,is,0)
#define _RAC(s,l) (s)[(l)]


#if FSGR_STRICT_DEBUG==1

#define LBMGI(level,ii,ij,ik, is)                 debLBMGI(level,ii,ij,ik, is)         
#define RFLAG(level,xx,yy,zz,set)                 debRFLAG(level,xx,yy,zz,set)            
#define RFLAG_NB(level,xx,yy,zz,set, dir)         debRFLAG_NB(level,xx,yy,zz,set, dir)    
#define RFLAG_NBINV(level,xx,yy,zz,set, dir)      debRFLAG_NBINV(level,xx,yy,zz,set, dir) 

#define LBMQI(level,ii,ij,ik, is, l)              debLBMQI(level,ii,ij,ik, is, l)         
#define QCELL(level,xx,yy,zz,set,l)               debQCELL(level,xx,yy,zz,set,l)         
#define QCELL_NB(level,xx,yy,zz,set, dir,l)       debQCELL_NB(level,xx,yy,zz,set, dir,l)
#define QCELL_NBINV(level,xx,yy,zz,set, dir,l)    debQCELL_NBINV(level,xx,yy,zz,set, dir,l)
#define RACPNT(level, ii,ij,ik, is )              debRACPNT(level, ii,ij,ik, is )          
#define RAC(s,l)                            			debRAC(s,l)                  

#else // FSGR_STRICT_DEBUG==1

#define LBMGI(level,ii,ij,ik, is)                 _LBMGI(level,ii,ij,ik, is)         
#define RFLAG(level,xx,yy,zz,set)                 _RFLAG(level,xx,yy,zz,set)            
#define RFLAG_NB(level,xx,yy,zz,set, dir)         _RFLAG_NB(level,xx,yy,zz,set, dir)    
#define RFLAG_NBINV(level,xx,yy,zz,set, dir)      _RFLAG_NBINV(level,xx,yy,zz,set, dir) 

#define LBMQI(level,ii,ij,ik, is, l)              _LBMQI(level,ii,ij,ik, is, l)         
#define QCELL(level,xx,yy,zz,set,l)               _QCELL(level,xx,yy,zz,set,l)         
#define QCELL_NB(level,xx,yy,zz,set, dir,l)       _QCELL_NB(level,xx,yy,zz,set, dir, l)
#define QCELL_NBINV(level,xx,yy,zz,set, dir,l)    _QCELL_NBINV(level,xx,yy,zz,set, dir,l)
#define RACPNT(level, ii,ij,ik, is )              _RACPNT(level, ii,ij,ik, is )          
#define RAC(s,l)                                  _RAC(s,l)                  

#endif // FSGR_STRICT_DEBUG==1

// general defines -----------------------------------------------------------------------------------------------

// replace TESTFLAG
#define FLAGISEXACT(flag, compflag)  ((flag & compflag)==compflag)

#if LBMDIM==2
#define dC 0
#define dN 1
#define dS 2
#define dE 3
#define dW 4
#define dNE 5
#define dNW 6
#define dSE 7
#define dSW 8
#else
// direction indices
#define dC 0
#define dN 1
#define dS 2
#define dE 3
#define dW 4
#define dT 5
#define dB 6
#define dNE 7
#define dNW 8
#define dSE 9
#define dSW 10
#define dNT 11
#define dNB 12
#define dST 13
#define dSB 14
#define dET 15
#define dEB 16
#define dWT 17
#define dWB 18
#endif
//? #define dWB 18

// default init for dFlux values
#define FLUX_INIT 0.5f * (float)(this->cDfNum)

// only for non DF dir handling!
#define dNET 19
#define dNWT 20
#define dSET 21
#define dSWT 22
#define dNEB 23
#define dNWB 24
#define dSEB 25
#define dSWB 26

//! fill value for boundary cells
#define BND_FILL 0.0

#define DFL1 (1.0/ 3.0)
#define DFL2 (1.0/18.0)
#define DFL3 (1.0/36.0)

// loops over _all_ cells (including boundary layer)
#define FSGR_FORIJK_BOUNDS(leveli) \
	for(int k= getForZMinBnd(); k< getForZMaxBnd(leveli); ++k) \
   for(int j=0;j<mLevel[leveli].lSizey-0;++j) \
    for(int i=0;i<mLevel[leveli].lSizex-0;++i) \
	
// loops over _only inner_ cells 
#define FSGR_FORIJK1(leveli) \
	for(int k= getForZMin1(); k< getForZMax1(leveli); ++k) \
   for(int j=1;j<mLevel[leveli].lSizey-1;++j) \
    for(int i=1;i<mLevel[leveli].lSizex-1;++i) \

// relaxation_macros end



/******************************************************************************/
/*! equilibrium functions */
/******************************************************************************/

/*! calculate length of velocity vector */
inline LbmFloat LbmFsgrSolver::getVelVecLen(int l, LbmFloat ux,LbmFloat uy,LbmFloat uz) {
	return ((ux)*dfDvecX[l]+(uy)*dfDvecY[l]+(uz)*dfDvecZ[l]);
};

/*! calculate equilibrium DF for given values */
inline LbmFloat LbmFsgrSolver::getCollideEq(int l, LbmFloat rho,  LbmFloat ux, LbmFloat uy, LbmFloat uz) {
#if FSGR_STRICT_DEBUG==1
	if((l<0)||(l>LBM_DFNUM)) { errFatal("LbmFsgrSolver::getCollideEq","Invalid DFEQ call "<<l, SIMWORLD_PANIC ); /* no access to mPanic here */	}
#endif // FSGR_STRICT_DEBUG==1
	LbmFloat tmp = getVelVecLen(l,ux,uy,uz); 
	return( dfLength[l] *( 
				+ rho - (3.0/2.0*(ux*ux + uy*uy + uz*uz)) 
				+ 3.0 *tmp 
				+ 9.0/2.0 *(tmp*tmp) )
		 	); // */
};

/*****************************************************************************/
/* init a given cell with flag, density, mass and equilibrium dist. funcs */

void LbmFsgrSolver::forceChangeFlag(int level, int xx,int yy,int zz,int set,CellFlagType newflag) {
	// also overwrite persisting flags
	// function is useful for tracking accesses...
	RFLAG(level,xx,yy,zz,set) = newflag;
}
void LbmFsgrSolver::changeFlag(int level, int xx,int yy,int zz,int set,CellFlagType newflag) {
	CellFlagType pers = RFLAG(level,xx,yy,zz,set) & CFPersistMask;
	RFLAG(level,xx,yy,zz,set) = newflag | pers;
}

void 
LbmFsgrSolver::initEmptyCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass) {
  /* init eq. dist funcs */
	LbmFloat *ecel;
	int workSet = mLevel[level].setCurr;

	ecel = RACPNT(level, i,j,k, workSet);
	FORDF0 { RAC(ecel, l) = this->dfEquil[l] * rho; }
	RAC(ecel, dMass) = mass;
	RAC(ecel, dFfrac) = mass/rho;
	RAC(ecel, dFlux) = FLUX_INIT;
	changeFlag(level, i,j,k, workSet, flag);

  workSet ^= 1;
	changeFlag(level, i,j,k, workSet, flag);
	return;
}

void 
LbmFsgrSolver::initVelocityCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass, LbmVec vel) {
	LbmFloat *ecel;
	int workSet = mLevel[level].setCurr;

	ecel = RACPNT(level, i,j,k, workSet);
	FORDF0 { RAC(ecel, l) = getCollideEq(l, rho,vel[0],vel[1],vel[2]); }
	RAC(ecel, dMass) = mass;
	RAC(ecel, dFfrac) = mass/rho;
	RAC(ecel, dFlux) = FLUX_INIT;
	changeFlag(level, i,j,k, workSet, flag);

  workSet ^= 1;
	changeFlag(level, i,j,k, workSet, flag);
	return;
}

int LbmFsgrSolver::getForZMinBnd() { 
	return 0; 
}
int LbmFsgrSolver::getForZMin1()   { 
	if(LBMDIM==2) return 0;
	return 1; 
}

int LbmFsgrSolver::getForZMaxBnd(int lev) { 
	if(LBMDIM==2) return 1;
	return mLevel[lev].lSizez -0;
}
int LbmFsgrSolver::getForZMax1(int lev)   { 
	if(LBMDIM==2) return 1;
	return mLevel[lev].lSizez -1;
}

bool LbmFsgrSolver::checkDomainBounds(int lev,int i,int j,int k) { 
	if(i<0) return false;
	if(j<0) return false;
	if(k<0) return false;
	if(i>mLevel[lev].lSizex-1) return false;
	if(j>mLevel[lev].lSizey-1) return false;
	if(k>mLevel[lev].lSizez-1) return false;
	return true;
}
bool LbmFsgrSolver::checkDomainBoundsPos(int lev,LbmVec pos) { 
	const int i= (int)pos[0]; 
	if(i<0) return false;
	if(i>mLevel[lev].lSizex-1) return false;
	const int j= (int)pos[1]; 
	if(j<0) return false;
	if(j>mLevel[lev].lSizey-1) return false;
	const int k= (int)pos[2];
	if(k<0) return false;
	if(k>mLevel[lev].lSizez-1) return false;
	return true;
}

void LbmFsgrSolver::initInterfaceVars(int level, int i,int j,int k,int workSet, bool initMass) {
	LbmFloat *ccel = &QCELL(level ,i,j,k, workSet,0);
	LbmFloat nrho = 0.0;
	FORDF0 { nrho += RAC(ccel,l); }
	if(initMass) {
		RAC(ccel,dMass) = nrho;
  	RAC(ccel, dFfrac) = 1.;
	} else {
		// preinited, e.g. from reinitFlags
		RAC(ccel, dFfrac) = RAC(ccel, dMass)/nrho;
		RAC(ccel, dFlux) = FLUX_INIT;
	}
}


#endif


