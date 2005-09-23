/******************************************************************************
 *
 * El'Beem - the visual lattice boltzmann freesurface simulator
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann standard solver classes
 *
 *****************************************************************************/


#ifndef LBMFSGRSOLVER_H
#include "utilities.h"
#include "arrays.h"
#include "lbmdimensions.h"
#include "lbmfunctions.h"
#include "ntl_scene.h"
#include <stdio.h>

#if PARALLEL==1
#include <omp.h>
#endif // PARALLEL=1
#ifndef PARALLEL
#define PARALLEL 0
#endif // PARALLEL

// blender interface
#if ELBEEM_BLENDER==1
#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_mutex.h"
extern "C" {
	void simulateThreadIncreaseFrame(void);
	extern SDL_mutex *globalBakeLock;
	extern int        globalBakeState;
	extern int        globalBakeFrame;
}
#endif // ELBEEM_BLENDER==1

#ifndef LBMMODEL_DEFINED
// force compiler error!
ERROR - define model first!
#endif // LBMMODEL_DEFINED


// general solver setting defines

//! debug coordinate accesses and the like? (much slower)
#define FSGR_STRICT_DEBUG 0

//! debug coordinate accesses and the like? (much slower)
#define FSGR_OMEGA_DEBUG 0

//! OPT3D quick LES on/off, only debug/benachmarking
#define USE_LES 1

//! order of interpolation (1/2)
#define INTORDER 1

//! order of interpolation (0=always current/1=interpolate/2=always other)
#define TIMEINTORDER 0

//! refinement border method (1 = small border / 2 = larger)
#define REFINEMENTBORDER 1

// use optimized 3D code?
#if LBMDIM==2
#define OPT3D false
#else
// determine with debugging...
#	if FSGR_STRICT_DEBUG==1
#		define OPT3D false
#	else // FSGR_STRICT_DEBUG==1
// usually switch optimizations for 3d on, when not debugging
#		define OPT3D true
// COMPRT
//#		define OPT3D false
#	endif // FSGR_STRICT_DEBUG==1
#endif

// enable/disable fine grid compression for finest level
#if LBMDIM==3
#define COMPRESSGRIDS 1
#else 
#define COMPRESSGRIDS 0
#endif 


//! threshold for level set fluid generation/isosurface
#define LS_FLUIDTHRESHOLD 0.5

//! invalid mass value for unused mass data
#define MASS_INVALID -1000.0

// empty/fill cells without fluid/empty NB's by inserting them into the full/empty lists?
#define FSGR_LISTTRICK          true
#define FSGR_LISTTTHRESHEMPTY   0.10
#define FSGR_LISTTTHRESHFULL    0.90
#define FSGR_MAGICNR            0.025
//0.04


// helper for comparing floats with epsilon
#define GFX_FLOATNEQ(x,y) ( ABS((x)-(y)) > (VECTOR_EPSILON) )
#define LBM_FLOATNEQ(x,y) ( ABS((x)-(y)) > (10.0*LBM_EPSILON) )


// macros for loops over all DFs
#define FORDF0 for(int l= 0; l< LBM_DFNUM; ++l)
#define FORDF1 for(int l= 1; l< LBM_DFNUM; ++l)
// and with different loop var to prevent shadowing
#define FORDF0M for(int m= 0; m< LBM_DFNUM; ++m)
#define FORDF1M for(int m= 1; m< LBM_DFNUM; ++m)

// aux. field indices (same for 2d)
#define dFfrac 19
#define dMass 20
#define dFlux 21
// max. no. of cell values for 3d
#define dTotalNum 22

// iso value defines
// border for marching cubes
#define ISOCORR 3


/*****************************************************************************/
/*! cell access classes */
template<typename D>
class UniformFsgrCellIdentifier : 
	public CellIdentifierInterface 
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
			if(D::cDimension>2) ret<<",k"<<z;
			ret <<" }";
			return ret.str();
		}

		virtual bool equal(CellIdentifierInterface* other) {
			//UniformFsgrCellIdentifier<D> *cid = dynamic_cast<UniformFsgrCellIdentifier<D> *>( other );
			UniformFsgrCellIdentifier<D> *cid = (UniformFsgrCellIdentifier<D> *)( other );
			if(!cid) return false;
			if( x==cid->x && y==cid->y && z==cid->z && level==cid->level ) return true;
			return false;
		}
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
	LbmFloat stepsize;
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

};



/*****************************************************************************/
/*! class for solving a LBM problem */
template<class D>
class LbmFsgrSolver : 
	public /*? virtual */ D // this means, the solver is a lbmData object and implements the lbmInterface
{

	public:
		//! Constructor 
		LbmFsgrSolver();
		//! Destructor 
		virtual ~LbmFsgrSolver();
		//! id string of solver
		virtual string getIdString() { return string("FsgrSolver[") + D::getIdString(); }

		//! initilize variables fom attribute list 
		virtual void parseAttrList();
		//! Initialize omegas and forces on all levels (for init/timestep change)
		void initLevelOmegas();
		//! finish the init with config file values (allocate arrays...) 
		virtual bool initialize( ntlTree* /*tree*/, vector<ntlGeometryObject*>* /*objects*/ );

#if LBM_USE_GUI==1
		//! show simulation info (implement SimulationObject pure virtual func)
		virtual void debugDisplay(fluidDispSettings *set);
#endif
		
		
		// implement CellIterator<UniformFsgrCellIdentifier> interface
		typedef UniformFsgrCellIdentifier<typename D::LbmCellContents> stdCellId;
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
		virtual int        getDfNum        ( );
		// convert pointers
		stdCellId* convertBaseCidToStdCid( CellIdentifierInterface* basecid);

		//! perform geometry init (if switched on) 
		bool initGeometryFlags();
		//! init part for all freesurface testcases 
		void initFreeSurfaces();
		//! init density gradient if enabled
		void initStandingFluidGradient();

 		/*! init a given cell with flag, density, mass and equilibrium dist. funcs */
		inline void initEmptyCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass);
		inline void initVelocityCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass, LbmVec vel);
		inline void changeFlag(int level, int xx,int yy,int zz,int set,CellFlagType newflag);

		/*! perform a single LBM step */
		virtual void step() { stepMain(); }
		void stepMain();
		void fineAdvance();
		void coarseAdvance(int lev);
		void coarseCalculateFluxareas(int lev);
		// coarsen a given level (returns true if sth. was changed)
		bool performRefinement(int lev);
		bool performCoarsening(int lev);
		//void oarseInterpolateToFineSpaceTime(int lev,LbmFloat t);
		void interpolateFineFromCoarse(int lev,LbmFloat t);
		void coarseRestrictFromFine(int lev);

		/*! init particle positions */
		virtual int initParticles(ParticleTracer *partt);
		/*! move all particles */
		virtual void advanceParticles(ParticleTracer *partt );


		/*! debug object display (used e.g. for preview surface) */
		virtual vector<ntlGeometryObject*> getDebugObjects();

		//! access the fillfrac field (for viz)
		inline float getFillFrac(int i, int j, int k);

		//! retrieve the fillfrac field ready to display
		void getIsofieldWeighted(float *iso);
		void getIsofield(float *iso){ return getIsofieldWeighted(iso); }
		//! for raytracing, preprocess
		void prepareVisualization( void );

		// rt interface
		void addDrop(bool active, float mx, float my);
		void initDrop(float mx, float my);
		void printCellStats();
		int checkGfxEndTime(); // {return 9;};
		//! get gfx geo setup id
		int getGfxGeoSetup() { return mGfxGeoSetup; }

		/*! type for cells */
		typedef typename D::LbmCell LbmCell;
		
	protected:

		//! internal quick print function (for debugging) 
		void printLbmCell(int level, int i, int j, int k,int set);
		// debugging use CellIterator interface to mark cell
		void debugMarkCellCall(int level, int vi,int vj,int vk);

		void mainLoop(int lev);
		void adaptTimestep();
		//! init mObjectSpeeds for current parametrization
		void recalculateObjectSpeeds();
		//! flag reinit step - always works on finest grid!
		void reinitFlags( int workSet );
		//! mass dist weights
		LbmFloat getMassdWeight(bool dirForw, int i,int j,int k,int workSet, int l);
		//! add point to mListNewInter list
		inline void addToNewInterList( int ni, int nj, int nk );	
		//! cell is interpolated from coarse level (inited into set, source sets are determined by t)
		inline void interpolateCellFromCoarse(int lev, int i, int j,int k, int dstSet, LbmFloat t, CellFlagType flagSet,bool markNbs);

		//! minimal and maximal z-coords (for 2D/3D loops)
		int getForZMinBnd() { return 0; }
		int getForZMin1()   { 
			if(D::cDimension==2) return 0;
			return 1; 
		}

		int getForZMaxBnd(int lev) { 
			if(D::cDimension==2) return 1;
			return mLevel[lev].lSizez -0;
		}
		int getForZMax1(int lev)   { 
			if(D::cDimension==2) return 1;
			return mLevel[lev].lSizez -1;
		}


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
		int mLoopSubdivs;
		float mSmoothSurface;
		float mSmoothNormals;
		
		//! use time adaptivity? 
		bool mTimeAdap;

		//! output surface preview? if >0 yes, and use as reduzed size 
		int mOutputSurfacePreview;
		LbmFloat mPreviewFactor;
		//! fluid vol height
		LbmFloat mFVHeight;
		LbmFloat mFVArea;
		bool mUpdateFVHeight;

		//! require some geo setup from the viz?
		int mGfxGeoSetup;
		//! force quit for gfx
		LbmFloat mGfxEndTime;
		//! smoother surface initialization?
		int mInitSurfaceSmoothing;

		int mTimestepReduceLock;
		int mTimeSwitchCounts;
		//! total simulation time so far 
		LbmFloat mSimulationTime;
		//! smallest and largest step size so far 
		LbmFloat mMinStepTime, mMaxStepTime;
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
		//! symmetric init?
		//bool mStartSymm;
		//! kepp track of max/min no. of filled cells
		int mMaxNoCells, mMinNoCells;
		long long int mAvgNumUsedCells;

		//! for interactive - how to drop drops?
		int mDropMode;
		LbmFloat mDropSize;
		LbmVec mDropSpeed;
		//! dropping variables
		bool mDropping;
		LbmFloat mDropX, mDropY;
		LbmFloat mDropHeight;
		//! precalculated objects speeds for current parametrization
		vector<LbmVec> mObjectSpeeds;

		//! get isofield weights
		int mIsoWeightMethod;
		float mIsoWeight[27];

		// grid coarsening vars
		
		/*! vector for the data for each level */
#		define MAX_LEV 5
		FsgrLevelData mLevel[MAX_LEV];

		/*! minimal and maximal refinement levels */
		int mMaxRefine;

		/*! df scale factors for level up/down */
		LbmFloat mDfScaleUp, mDfScaleDown;

		/*! precomputed cell area values */
		LbmFloat mFsgrCellArea[27];

		/*! LES C_smago paramter for finest grid */
		float mInitialCsmago;
		/*! LES C_smago paramter for coarser grids */
		float mInitialCsmagoCoarse;
		/*! LES stats for non OPT3D */
		LbmFloat mDebugOmegaRet;

		//! fluid stats
		int mNumInterdCells;
		int mNumInvIfCells;
		int mNumInvIfTotal;
		int mNumFsgrChanges;

		//! debug function to disable standing f init
		int mDisableStandingFluidInit;
		//! debug function to force tadap syncing
		int mForceTadapRefine;


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
};



/*****************************************************************************/
// relaxation_macros



// cell mark debugging
#if FSGR_STRICT_DEBUG==10
#define debugMarkCell(lev,x,y,z) \
	errMsg("debugMarkCell",D::mName<<" step: "<<D::mStepCnt<<" lev:"<<(lev)<<" marking "<<PRINT_VEC((x),(y),(z))<<" line "<< __LINE__ ); \
	debugMarkCellCall((lev),(x),(y),(z));
#else // FSGR_STRICT_DEBUG==1
#define debugMarkCell(lev,x,y,z) \
	debugMarkCellCall((lev),(x),(y),(z));
#endif // FSGR_STRICT_DEBUG==1


// flag array defines -----------------------------------------------------------------------------------------------

// lbm testsolver get index define
#define _LBMGI(level, ii,ij,ik, is) ( (mLevel[level].lOffsy*(ik)) + (mLevel[level].lOffsx*(ij)) + (ii) )

//! flag array acces macro
#define _RFLAG(level,xx,yy,zz,set) mLevel[level].mprsFlags[set][ LBMGI((level),(xx),(yy),(zz),(set)) ]
#define _RFLAG_NB(level,xx,yy,zz,set, dir) mLevel[level].mprsFlags[set][ LBMGI((level),(xx)+D::dfVecX[dir],(yy)+D::dfVecY[dir],(zz)+D::dfVecZ[dir],set) ]
#define _RFLAG_NBINV(level,xx,yy,zz,set, dir) mLevel[level].mprsFlags[set][ LBMGI((level),(xx)+D::dfVecX[D::dfInv[dir]],(yy)+D::dfVecY[D::dfInv[dir]],(zz)+D::dfVecZ[D::dfInv[dir]],set) ]

// array data layouts
// standard array layout  -----------------------------------------------------------------------------------------------
#define ALSTRING "Standard Array Layout"
//#define _LBMQI(level, ii,ij,ik, is, lunused) ( ((is)*mLevel[level].lOffsz) + (mLevel[level].lOffsy*(ik)) + (mLevel[level].lOffsx*(ij)) + (ii) )
#define _LBMQI(level, ii,ij,ik, is, lunused) ( (mLevel[level].lOffsy*(ik)) + (mLevel[level].lOffsx*(ij)) + (ii) )
#define _QCELL(level,xx,yy,zz,set,l) (mLevel[level].mprsCells[(set)][ LBMQI((level),(xx),(yy),(zz),(set), l)*dTotalNum +(l)])
#define _QCELL_NB(level,xx,yy,zz,set, dir,l) (mLevel[level].mprsCells[(set)][ LBMQI((level),(xx)+D::dfVecX[dir],(yy)+D::dfVecY[dir],(zz)+D::dfVecZ[dir],set, l)*dTotalNum +(l)])
#define _QCELL_NBINV(level,xx,yy,zz,set, dir,l) (mLevel[level].mprsCells[(set)][ LBMQI((level),(xx)+D::dfVecX[D::dfInv[dir]],(yy)+D::dfVecY[D::dfInv[dir]],(zz)+D::dfVecZ[D::dfInv[dir]],set, l)*dTotalNum +(l)])

#define QCELLSTEP dTotalNum
#define _RACPNT(level, ii,ij,ik, is )  &QCELL(level,ii,ij,ik,is,0)
#define _RAC(s,l) (s)[(l)]

// standard arrays
#define CSRC_C    RAC(ccel                                , dC )
#define CSRC_E    RAC(ccel + (-1)             *(dTotalNum), dE )
#define CSRC_W    RAC(ccel + (+1)             *(dTotalNum), dW )
#define CSRC_N    RAC(ccel + (-mLevel[lev].lOffsx)        *(dTotalNum), dN )
#define CSRC_S    RAC(ccel + (+mLevel[lev].lOffsx)        *(dTotalNum), dS )
#define CSRC_NE   RAC(ccel + (-mLevel[lev].lOffsx-1)      *(dTotalNum), dNE)
#define CSRC_NW   RAC(ccel + (-mLevel[lev].lOffsx+1)      *(dTotalNum), dNW)
#define CSRC_SE   RAC(ccel + (+mLevel[lev].lOffsx-1)      *(dTotalNum), dSE)
#define CSRC_SW   RAC(ccel + (+mLevel[lev].lOffsx+1)      *(dTotalNum), dSW)
#define CSRC_T    RAC(ccel + (-mLevel[lev].lOffsy)        *(dTotalNum), dT )
#define CSRC_B    RAC(ccel + (+mLevel[lev].lOffsy)        *(dTotalNum), dB )
#define CSRC_ET   RAC(ccel + (-mLevel[lev].lOffsy-1)      *(dTotalNum), dET)
#define CSRC_EB   RAC(ccel + (+mLevel[lev].lOffsy-1)      *(dTotalNum), dEB)
#define CSRC_WT   RAC(ccel + (-mLevel[lev].lOffsy+1)      *(dTotalNum), dWT)
#define CSRC_WB   RAC(ccel + (+mLevel[lev].lOffsy+1)      *(dTotalNum), dWB)
#define CSRC_NT   RAC(ccel + (-mLevel[lev].lOffsy-mLevel[lev].lOffsx) *(dTotalNum), dNT)
#define CSRC_NB   RAC(ccel + (+mLevel[lev].lOffsy-mLevel[lev].lOffsx) *(dTotalNum), dNB)
#define CSRC_ST   RAC(ccel + (-mLevel[lev].lOffsy+mLevel[lev].lOffsx) *(dTotalNum), dST)
#define CSRC_SB   RAC(ccel + (+mLevel[lev].lOffsy+mLevel[lev].lOffsx) *(dTotalNum), dSB)

#define XSRC_C(x)    RAC(ccel + (x)                 *dTotalNum, dC )
#define XSRC_E(x)    RAC(ccel + ((x)-1)             *dTotalNum, dE )
#define XSRC_W(x)    RAC(ccel + ((x)+1)             *dTotalNum, dW )
#define XSRC_N(x)    RAC(ccel + ((x)-mLevel[lev].lOffsx)        *dTotalNum, dN )
#define XSRC_S(x)    RAC(ccel + ((x)+mLevel[lev].lOffsx)        *dTotalNum, dS )
#define XSRC_NE(x)   RAC(ccel + ((x)-mLevel[lev].lOffsx-1)      *dTotalNum, dNE)
#define XSRC_NW(x)   RAC(ccel + ((x)-mLevel[lev].lOffsx+1)      *dTotalNum, dNW)
#define XSRC_SE(x)   RAC(ccel + ((x)+mLevel[lev].lOffsx-1)      *dTotalNum, dSE)
#define XSRC_SW(x)   RAC(ccel + ((x)+mLevel[lev].lOffsx+1)      *dTotalNum, dSW)
#define XSRC_T(x)    RAC(ccel + ((x)-mLevel[lev].lOffsy)        *dTotalNum, dT )
#define XSRC_B(x)    RAC(ccel + ((x)+mLevel[lev].lOffsy)        *dTotalNum, dB )
#define XSRC_ET(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy-1)      *dTotalNum, dET)
#define XSRC_EB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy-1)      *dTotalNum, dEB)
#define XSRC_WT(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy+1)      *dTotalNum, dWT)
#define XSRC_WB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy+1)      *dTotalNum, dWB)
#define XSRC_NT(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy-mLevel[lev].lOffsx) *dTotalNum, dNT)
#define XSRC_NB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy-mLevel[lev].lOffsx) *dTotalNum, dNB)
#define XSRC_ST(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy+mLevel[lev].lOffsx) *dTotalNum, dST)
#define XSRC_SB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy+mLevel[lev].lOffsx) *dTotalNum, dSB)



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

#define TESTFLAG(flag, compflag) ((flag & compflag)==compflag)

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
#define LBM_DFNUM 9
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
#define LBM_DFNUM 19
#endif
//? #define dWB 18

// default init for dFlux values
#define FLUX_INIT 0.5f * (float)(D::cDfNum)

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

#define OMEGA(l) mLevel[(l)].omega

#define EQC (  DFL1*(rho - usqr))
#define EQN (  DFL2*(rho + uy*(4.5*uy + 3.0) - usqr))
#define EQS (  DFL2*(rho + uy*(4.5*uy - 3.0) - usqr))
#define EQE (  DFL2*(rho + ux*(4.5*ux + 3.0) - usqr))
#define EQW (  DFL2*(rho + ux*(4.5*ux - 3.0) - usqr))
#define EQT (  DFL2*(rho + uz*(4.5*uz + 3.0) - usqr))
#define EQB (  DFL2*(rho + uz*(4.5*uz - 3.0) - usqr))
                    
#define EQNE ( DFL3*(rho + (+ux+uy)*(4.5*(+ux+uy) + 3.0) - usqr))
#define EQNW ( DFL3*(rho + (-ux+uy)*(4.5*(-ux+uy) + 3.0) - usqr))
#define EQSE ( DFL3*(rho + (+ux-uy)*(4.5*(+ux-uy) + 3.0) - usqr))
#define EQSW ( DFL3*(rho + (-ux-uy)*(4.5*(-ux-uy) + 3.0) - usqr))
#define EQNT ( DFL3*(rho + (+uy+uz)*(4.5*(+uy+uz) + 3.0) - usqr))
#define EQNB ( DFL3*(rho + (+uy-uz)*(4.5*(+uy-uz) + 3.0) - usqr))
#define EQST ( DFL3*(rho + (-uy+uz)*(4.5*(-uy+uz) + 3.0) - usqr))
#define EQSB ( DFL3*(rho + (-uy-uz)*(4.5*(-uy-uz) + 3.0) - usqr))
#define EQET ( DFL3*(rho + (+ux+uz)*(4.5*(+ux+uz) + 3.0) - usqr))
#define EQEB ( DFL3*(rho + (+ux-uz)*(4.5*(+ux-uz) + 3.0) - usqr))
#define EQWT ( DFL3*(rho + (-ux+uz)*(4.5*(-ux+uz) + 3.0) - usqr))
#define EQWB ( DFL3*(rho + (-ux-uz)*(4.5*(-ux-uz) + 3.0) - usqr))


// this is a bit ugly, but necessary for the CSRC_ access...
#define MSRC_C    m[dC ]
#define MSRC_N    m[dN ]
#define MSRC_S    m[dS ]
#define MSRC_E    m[dE ]
#define MSRC_W    m[dW ]
#define MSRC_T    m[dT ]
#define MSRC_B    m[dB ]
#define MSRC_NE   m[dNE]
#define MSRC_NW   m[dNW]
#define MSRC_SE   m[dSE]
#define MSRC_SW   m[dSW]
#define MSRC_NT   m[dNT]
#define MSRC_NB   m[dNB]
#define MSRC_ST   m[dST]
#define MSRC_SB   m[dSB]
#define MSRC_ET   m[dET]
#define MSRC_EB   m[dEB]
#define MSRC_WT   m[dWT]
#define MSRC_WB   m[dWB]

// this is a bit ugly, but necessary for the ccel local access...
#define CCEL_C    RAC(ccel, dC )
#define CCEL_N    RAC(ccel, dN )
#define CCEL_S    RAC(ccel, dS )
#define CCEL_E    RAC(ccel, dE )
#define CCEL_W    RAC(ccel, dW )
#define CCEL_T    RAC(ccel, dT )
#define CCEL_B    RAC(ccel, dB )
#define CCEL_NE   RAC(ccel, dNE)
#define CCEL_NW   RAC(ccel, dNW)
#define CCEL_SE   RAC(ccel, dSE)
#define CCEL_SW   RAC(ccel, dSW)
#define CCEL_NT   RAC(ccel, dNT)
#define CCEL_NB   RAC(ccel, dNB)
#define CCEL_ST   RAC(ccel, dST)
#define CCEL_SB   RAC(ccel, dSB)
#define CCEL_ET   RAC(ccel, dET)
#define CCEL_EB   RAC(ccel, dEB)
#define CCEL_WT   RAC(ccel, dWT)
#define CCEL_WB   RAC(ccel, dWB)
// for coarse to fine interpol access
#define CCELG_C(f)    (RAC(ccel, dC )*mGaussw[(f)])
#define CCELG_N(f)    (RAC(ccel, dN )*mGaussw[(f)])
#define CCELG_S(f)    (RAC(ccel, dS )*mGaussw[(f)])
#define CCELG_E(f)    (RAC(ccel, dE )*mGaussw[(f)])
#define CCELG_W(f)    (RAC(ccel, dW )*mGaussw[(f)])
#define CCELG_T(f)    (RAC(ccel, dT )*mGaussw[(f)])
#define CCELG_B(f)    (RAC(ccel, dB )*mGaussw[(f)])
#define CCELG_NE(f)   (RAC(ccel, dNE)*mGaussw[(f)])
#define CCELG_NW(f)   (RAC(ccel, dNW)*mGaussw[(f)])
#define CCELG_SE(f)   (RAC(ccel, dSE)*mGaussw[(f)])
#define CCELG_SW(f)   (RAC(ccel, dSW)*mGaussw[(f)])
#define CCELG_NT(f)   (RAC(ccel, dNT)*mGaussw[(f)])
#define CCELG_NB(f)   (RAC(ccel, dNB)*mGaussw[(f)])
#define CCELG_ST(f)   (RAC(ccel, dST)*mGaussw[(f)])
#define CCELG_SB(f)   (RAC(ccel, dSB)*mGaussw[(f)])
#define CCELG_ET(f)   (RAC(ccel, dET)*mGaussw[(f)])
#define CCELG_EB(f)   (RAC(ccel, dEB)*mGaussw[(f)])
#define CCELG_WT(f)   (RAC(ccel, dWT)*mGaussw[(f)])
#define CCELG_WB(f)   (RAC(ccel, dWB)*mGaussw[(f)])


#if PARALLEL==1
#define CSMOMEGA_STATS(dlev, domega) 
#else // PARALLEL==1
#if FSGR_OMEGA_DEBUG==1
#define CSMOMEGA_STATS(dlev, domega) \
	mLevel[dlev].avgOmega += domega; mLevel[dlev].avgOmegaCnt+=1.0; 
#else // FSGR_OMEGA_DEBUG==1
#define CSMOMEGA_STATS(dlev, domega) 
#endif // FSGR_OMEGA_DEBUG==1
#endif // PARALLEL==1


// used for main loops and grav init
// source set
#define SRCS(l) mLevel[(l)].setCurr
// target set
#define TSET(l) mLevel[(l)].setOther


// complete default stream&collide, 2d/3d
/* read distribution funtions of adjacent cells = sweep step */ 
#if OPT3D==false 

#if FSGR_STRICT_DEBUG==1
#define MARKCELLCHECK \
	debugMarkCell(lev,i,j,k); D::mPanic=1;
#define STREAMCHECK(ni,nj,nk,nl) \
	if((m[l] < -1.0) || (m[l]>1.0)) {\
		errMsg("STREAMCHECK","Invalid streamed DF l"<<l<<" value:"<<m[l]<<" at "<<PRINT_IJK<<" from "<<PRINT_VEC(ni,nj,nk)<<" nl"<<(nl)<<\
				" nfc"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr)<<" nfo"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setOther)  ); \
		MARKCELLCHECK; \
	}
#define COLLCHECK \
	if( (rho>2.0) || (rho<-1.0) || (ABS(ux)>1.0) || (ABS(uy)>1.0) |(ABS(uz)>1.0) ) {\
		errMsg("COLLCHECK","Invalid collision values r:"<<rho<<" u:"PRINT_VEC(ux,uy,uz)<<" at? "<<PRINT_IJK ); \
		MARKCELLCHECK; \
	}
#else
#define STREAMCHECK(ni,nj,nk,nl) 
#define COLLCHECK
#endif

// careful ux,uy,uz need to be inited before!

#define DEFAULT_STREAM \
		m[dC] = RAC(ccel,dC); \
		FORDF1 { \
			if(NBFLAG( D::dfInv[l] )&CFBnd) { \
				m[l] = RAC(ccel, D::dfInv[l] ); \
				STREAMCHECK(i,j,k, D::dfInv[l]); \
			} else { \
				m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l); \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			} \
		}   

// careful ux,uy,uz need to be inited before!
#define DEFAULT_COLLIDE \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago, &mDebugOmegaRet ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			FORDF0 { RAC(tcel,l) = m[l]; }   \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
			COLLCHECK;
#define OPTIMIZED_STREAMCOLLIDE \
			m[0] = RAC(ccel,0); \
			FORDF1 { /* df0 is set later on... */ \
				/* FIXME CHECK INV ? */\
				if(RFLAG_NBINV(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); D::mPanic=1;  \
				} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			}   \
			rho=m[0]; ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago , &mDebugOmegaRet  ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			FORDF0 { RAC(tcel,l) = m[l]; } \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
			COLLCHECK;

#else  // 3D, opt OPT3D==true

#define DEFAULT_STREAM \
		m[dC] = RAC(ccel,dC); \
		/* explicit streaming */ \
		if((!nbored & CFBnd)) { \
			/* no boundary near?, no real speed diff.? */ \
			m[dN ] = CSRC_N ; m[dS ] = CSRC_S ; \
			m[dE ] = CSRC_E ; m[dW ] = CSRC_W ; \
			m[dT ] = CSRC_T ; m[dB ] = CSRC_B ; \
			m[dNE] = CSRC_NE; m[dNW] = CSRC_NW; m[dSE] = CSRC_SE; m[dSW] = CSRC_SW; \
			m[dNT] = CSRC_NT; m[dNB] = CSRC_NB; m[dST] = CSRC_ST; m[dSB] = CSRC_SB; \
			m[dET] = CSRC_ET; m[dEB] = CSRC_EB; m[dWT] = CSRC_WT; m[dWB] = CSRC_WB; \
		} else { \
			/* explicit streaming */ \
			if(NBFLAG(dS )&CFBnd) { m[dN ] = RAC(ccel,dS ); } else { m[dN ] = CSRC_N ; } \
			if(NBFLAG(dN )&CFBnd) { m[dS ] = RAC(ccel,dN ); } else { m[dS ] = CSRC_S ; } \
			if(NBFLAG(dW )&CFBnd) { m[dE ] = RAC(ccel,dW ); } else { m[dE ] = CSRC_E ; } \
			if(NBFLAG(dE )&CFBnd) { m[dW ] = RAC(ccel,dE ); } else { m[dW ] = CSRC_W ; } \
			if(NBFLAG(dB )&CFBnd) { m[dT ] = RAC(ccel,dB ); } else { m[dT ] = CSRC_T ; } \
			if(NBFLAG(dT )&CFBnd) { m[dB ] = RAC(ccel,dT ); } else { m[dB ] = CSRC_B ; } \
 			\
			if(NBFLAG(dSW)&CFBnd) { m[dNE] = RAC(ccel,dSW); } else { m[dNE] = CSRC_NE; } \
			if(NBFLAG(dSE)&CFBnd) { m[dNW] = RAC(ccel,dSE); } else { m[dNW] = CSRC_NW; } \
			if(NBFLAG(dNW)&CFBnd) { m[dSE] = RAC(ccel,dNW); } else { m[dSE] = CSRC_SE; } \
			if(NBFLAG(dNE)&CFBnd) { m[dSW] = RAC(ccel,dNE); } else { m[dSW] = CSRC_SW; } \
			if(NBFLAG(dSB)&CFBnd) { m[dNT] = RAC(ccel,dSB); } else { m[dNT] = CSRC_NT; } \
			if(NBFLAG(dST)&CFBnd) { m[dNB] = RAC(ccel,dST); } else { m[dNB] = CSRC_NB; } \
			if(NBFLAG(dNB)&CFBnd) { m[dST] = RAC(ccel,dNB); } else { m[dST] = CSRC_ST; } \
			if(NBFLAG(dNT)&CFBnd) { m[dSB] = RAC(ccel,dNT); } else { m[dSB] = CSRC_SB; } \
			if(NBFLAG(dWB)&CFBnd) { m[dET] = RAC(ccel,dWB); } else { m[dET] = CSRC_ET; } \
			if(NBFLAG(dWT)&CFBnd) { m[dEB] = RAC(ccel,dWT); } else { m[dEB] = CSRC_EB; } \
			if(NBFLAG(dEB)&CFBnd) { m[dWT] = RAC(ccel,dEB); } else { m[dWT] = CSRC_WT; } \
			if(NBFLAG(dET)&CFBnd) { m[dWB] = RAC(ccel,dET); } else { m[dWB] = CSRC_WB; } \
		} 



#define COLL_CALCULATE_DFEQ(dstarray) \
			dstarray[dN ] = EQN ; dstarray[dS ] = EQS ; \
			dstarray[dE ] = EQE ; dstarray[dW ] = EQW ; \
			dstarray[dT ] = EQT ; dstarray[dB ] = EQB ; \
			dstarray[dNE] = EQNE; dstarray[dNW] = EQNW; dstarray[dSE] = EQSE; dstarray[dSW] = EQSW; \
			dstarray[dNT] = EQNT; dstarray[dNB] = EQNB; dstarray[dST] = EQST; dstarray[dSB] = EQSB; \
			dstarray[dET] = EQET; dstarray[dEB] = EQEB; dstarray[dWT] = EQWT; dstarray[dWB] = EQWB; 
#define COLL_CALCULATE_NONEQTENSOR(csolev, srcArray ) \
			lcsmqadd  = (srcArray##NE - lcsmeq[ dNE ]); \
			lcsmqadd -= (srcArray##NW - lcsmeq[ dNW ]); \
			lcsmqadd -= (srcArray##SE - lcsmeq[ dSE ]); \
			lcsmqadd += (srcArray##SW - lcsmeq[ dSW ]); \
			lcsmqo = (lcsmqadd*    lcsmqadd); \
			lcsmqadd  = (srcArray##ET - lcsmeq[  dET ]); \
			lcsmqadd -= (srcArray##EB - lcsmeq[  dEB ]); \
			lcsmqadd -= (srcArray##WT - lcsmeq[  dWT ]); \
			lcsmqadd += (srcArray##WB - lcsmeq[  dWB ]); \
			lcsmqo += (lcsmqadd*    lcsmqadd); \
			lcsmqadd  = (srcArray##NT - lcsmeq[  dNT ]); \
			lcsmqadd -= (srcArray##NB - lcsmeq[  dNB ]); \
			lcsmqadd -= (srcArray##ST - lcsmeq[  dST ]); \
			lcsmqadd += (srcArray##SB - lcsmeq[  dSB ]); \
			lcsmqo += (lcsmqadd*    lcsmqadd); \
			lcsmqo *= 2.0; \
			lcsmqadd  = (srcArray##E  -  lcsmeq[ dE  ]); \
			lcsmqadd += (srcArray##W  -  lcsmeq[ dW  ]); \
			lcsmqadd += (srcArray##NE -  lcsmeq[ dNE ]); \
			lcsmqadd += (srcArray##NW -  lcsmeq[ dNW ]); \
			lcsmqadd += (srcArray##SE -  lcsmeq[ dSE ]); \
			lcsmqadd += (srcArray##SW -  lcsmeq[ dSW ]); \
			lcsmqadd += (srcArray##ET  - lcsmeq[ dET ]); \
			lcsmqadd += (srcArray##EB  - lcsmeq[ dEB ]); \
			lcsmqadd += (srcArray##WT  - lcsmeq[ dWT ]); \
			lcsmqadd += (srcArray##WB  - lcsmeq[ dWB ]); \
			lcsmqo += (lcsmqadd*    lcsmqadd); \
			lcsmqadd  = (srcArray##N  -  lcsmeq[ dN  ]); \
			lcsmqadd += (srcArray##S  -  lcsmeq[ dS  ]); \
			lcsmqadd += (srcArray##NE -  lcsmeq[ dNE ]); \
			lcsmqadd += (srcArray##NW -  lcsmeq[ dNW ]); \
			lcsmqadd += (srcArray##SE -  lcsmeq[ dSE ]); \
			lcsmqadd += (srcArray##SW -  lcsmeq[ dSW ]); \
			lcsmqadd += (srcArray##NT  - lcsmeq[ dNT ]); \
			lcsmqadd += (srcArray##NB  - lcsmeq[ dNB ]); \
			lcsmqadd += (srcArray##ST  - lcsmeq[ dST ]); \
			lcsmqadd += (srcArray##SB  - lcsmeq[ dSB ]); \
			lcsmqo += (lcsmqadd*    lcsmqadd); \
			lcsmqadd  = (srcArray##T  -  lcsmeq[ dT  ]); \
			lcsmqadd += (srcArray##B  -  lcsmeq[ dB  ]); \
			lcsmqadd += (srcArray##NT -  lcsmeq[ dNT ]); \
			lcsmqadd += (srcArray##NB -  lcsmeq[ dNB ]); \
			lcsmqadd += (srcArray##ST -  lcsmeq[ dST ]); \
			lcsmqadd += (srcArray##SB -  lcsmeq[ dSB ]); \
			lcsmqadd += (srcArray##ET  - lcsmeq[ dET ]); \
			lcsmqadd += (srcArray##EB  - lcsmeq[ dEB ]); \
			lcsmqadd += (srcArray##WT  - lcsmeq[ dWT ]); \
			lcsmqadd += (srcArray##WB  - lcsmeq[ dWB ]); \
			lcsmqo += (lcsmqadd*    lcsmqadd); \
			lcsmqo = sqrt(lcsmqo); /* FIXME check effect of sqrt*/ \

//			COLL_CALCULATE_CSMOMEGAVAL(csolev, lcsmomega); 

// careful - need lcsmqo 
#define COLL_CALCULATE_CSMOMEGAVAL(csolev, dstomega ) \
			dstomega =  1.0/\
					( 3.0*( mLevel[(csolev)].lcnu+mLevel[(csolev)].lcsmago_sqr*(\
							-mLevel[(csolev)].lcnu + sqrt( mLevel[(csolev)].lcnu*mLevel[(csolev)].lcnu + 18.0*mLevel[(csolev)].lcsmago_sqr* lcsmqo ) \
							/ (6.0*mLevel[(csolev)].lcsmago_sqr)) \
						) +0.5 ); 

#define DEFAULT_COLLIDE_LES \
			rho = + MSRC_C  + MSRC_N  \
				+ MSRC_S  + MSRC_E  \
				+ MSRC_W  + MSRC_T  \
				+ MSRC_B  + MSRC_NE \
				+ MSRC_NW + MSRC_SE \
				+ MSRC_SW + MSRC_NT \
				+ MSRC_NB + MSRC_ST \
				+ MSRC_SB + MSRC_ET \
				+ MSRC_EB + MSRC_WT \
				+ MSRC_WB; \
 			\
			ux += MSRC_E - MSRC_W \
				+ MSRC_NE - MSRC_NW \
				+ MSRC_SE - MSRC_SW \
				+ MSRC_ET + MSRC_EB \
				- MSRC_WT - MSRC_WB ;  \
 			\
			uy += MSRC_N - MSRC_S \
				+ MSRC_NE + MSRC_NW \
				- MSRC_SE - MSRC_SW \
				+ MSRC_NT + MSRC_NB \
				- MSRC_ST - MSRC_SB ;  \
 			\
			uz += MSRC_T - MSRC_B \
				+ MSRC_NT - MSRC_NB \
				+ MSRC_ST - MSRC_SB \
				+ MSRC_ET - MSRC_EB \
				+ MSRC_WT - MSRC_WB ;  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			COLL_CALCULATE_DFEQ(lcsmeq); \
			COLL_CALCULATE_NONEQTENSOR(lev, MSRC_)\
			COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
			CSMOMEGA_STATS(lev,lcsmomega); \
 			\
			RAC(tcel,dC ) = (1.0-lcsmomega)*MSRC_C  + lcsmomega*EQC ; \
 			\
			RAC(tcel,dN ) = (1.0-lcsmomega)*MSRC_N  + lcsmomega*lcsmeq[ dN ]; \
			RAC(tcel,dS ) = (1.0-lcsmomega)*MSRC_S  + lcsmomega*lcsmeq[ dS ]; \
			RAC(tcel,dE ) = (1.0-lcsmomega)*MSRC_E  + lcsmomega*lcsmeq[ dE ]; \
			RAC(tcel,dW ) = (1.0-lcsmomega)*MSRC_W  + lcsmomega*lcsmeq[ dW ]; \
			RAC(tcel,dT ) = (1.0-lcsmomega)*MSRC_T  + lcsmomega*lcsmeq[ dT ]; \
			RAC(tcel,dB ) = (1.0-lcsmomega)*MSRC_B  + lcsmomega*lcsmeq[ dB ]; \
 			\
			RAC(tcel,dNE) = (1.0-lcsmomega)*MSRC_NE + lcsmomega*lcsmeq[ dNE]; \
			RAC(tcel,dNW) = (1.0-lcsmomega)*MSRC_NW + lcsmomega*lcsmeq[ dNW]; \
			RAC(tcel,dSE) = (1.0-lcsmomega)*MSRC_SE + lcsmomega*lcsmeq[ dSE]; \
			RAC(tcel,dSW) = (1.0-lcsmomega)*MSRC_SW + lcsmomega*lcsmeq[ dSW]; \
			RAC(tcel,dNT) = (1.0-lcsmomega)*MSRC_NT + lcsmomega*lcsmeq[ dNT]; \
			RAC(tcel,dNB) = (1.0-lcsmomega)*MSRC_NB + lcsmomega*lcsmeq[ dNB]; \
			RAC(tcel,dST) = (1.0-lcsmomega)*MSRC_ST + lcsmomega*lcsmeq[ dST]; \
			RAC(tcel,dSB) = (1.0-lcsmomega)*MSRC_SB + lcsmomega*lcsmeq[ dSB]; \
			RAC(tcel,dET) = (1.0-lcsmomega)*MSRC_ET + lcsmomega*lcsmeq[ dET]; \
			RAC(tcel,dEB) = (1.0-lcsmomega)*MSRC_EB + lcsmomega*lcsmeq[ dEB]; \
			RAC(tcel,dWT) = (1.0-lcsmomega)*MSRC_WT + lcsmomega*lcsmeq[ dWT]; \
			RAC(tcel,dWB) = (1.0-lcsmomega)*MSRC_WB + lcsmomega*lcsmeq[ dWB]; 

#define DEFAULT_COLLIDE_NOLES \
			rho = + MSRC_C  + MSRC_N  \
				+ MSRC_S  + MSRC_E  \
				+ MSRC_W  + MSRC_T  \
				+ MSRC_B  + MSRC_NE \
				+ MSRC_NW + MSRC_SE \
				+ MSRC_SW + MSRC_NT \
				+ MSRC_NB + MSRC_ST \
				+ MSRC_SB + MSRC_ET \
				+ MSRC_EB + MSRC_WT \
				+ MSRC_WB; \
 			\
			ux += MSRC_E - MSRC_W \
				+ MSRC_NE - MSRC_NW \
				+ MSRC_SE - MSRC_SW \
				+ MSRC_ET + MSRC_EB \
				- MSRC_WT - MSRC_WB ;  \
 			\
			uy += MSRC_N - MSRC_S \
				+ MSRC_NE + MSRC_NW \
				- MSRC_SE - MSRC_SW \
				+ MSRC_NT + MSRC_NB \
				- MSRC_ST - MSRC_SB ;  \
 			\
			uz += MSRC_T - MSRC_B \
				+ MSRC_NT - MSRC_NB \
				+ MSRC_ST - MSRC_SB \
				+ MSRC_ET - MSRC_EB \
				+ MSRC_WT - MSRC_WB ;  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
 			\
			RAC(tcel,dC ) = (1.0-OMEGA(lev))*MSRC_C  + OMEGA(lev)*EQC ; \
 			\
			RAC(tcel,dN ) = (1.0-OMEGA(lev))*MSRC_N  + OMEGA(lev)*EQN ; \
			RAC(tcel,dS ) = (1.0-OMEGA(lev))*MSRC_S  + OMEGA(lev)*EQS ; \
			RAC(tcel,dE ) = (1.0-OMEGA(lev))*MSRC_E  + OMEGA(lev)*EQE ; \
			RAC(tcel,dW ) = (1.0-OMEGA(lev))*MSRC_W  + OMEGA(lev)*EQW ; \
			RAC(tcel,dT ) = (1.0-OMEGA(lev))*MSRC_T  + OMEGA(lev)*EQT ; \
			RAC(tcel,dB ) = (1.0-OMEGA(lev))*MSRC_B  + OMEGA(lev)*EQB ; \
 			\
			RAC(tcel,dNE) = (1.0-OMEGA(lev))*MSRC_NE + OMEGA(lev)*EQNE; \
			RAC(tcel,dNW) = (1.0-OMEGA(lev))*MSRC_NW + OMEGA(lev)*EQNW; \
			RAC(tcel,dSE) = (1.0-OMEGA(lev))*MSRC_SE + OMEGA(lev)*EQSE; \
			RAC(tcel,dSW) = (1.0-OMEGA(lev))*MSRC_SW + OMEGA(lev)*EQSW; \
			RAC(tcel,dNT) = (1.0-OMEGA(lev))*MSRC_NT + OMEGA(lev)*EQNT; \
			RAC(tcel,dNB) = (1.0-OMEGA(lev))*MSRC_NB + OMEGA(lev)*EQNB; \
			RAC(tcel,dST) = (1.0-OMEGA(lev))*MSRC_ST + OMEGA(lev)*EQST; \
			RAC(tcel,dSB) = (1.0-OMEGA(lev))*MSRC_SB + OMEGA(lev)*EQSB; \
			RAC(tcel,dET) = (1.0-OMEGA(lev))*MSRC_ET + OMEGA(lev)*EQET; \
			RAC(tcel,dEB) = (1.0-OMEGA(lev))*MSRC_EB + OMEGA(lev)*EQEB; \
			RAC(tcel,dWT) = (1.0-OMEGA(lev))*MSRC_WT + OMEGA(lev)*EQWT; \
			RAC(tcel,dWB) = (1.0-OMEGA(lev))*MSRC_WB + OMEGA(lev)*EQWB; 



#define OPTIMIZED_STREAMCOLLIDE_LES \
			/* only surrounded by fluid cells...!, so safe streaming here... */ \
			m[dC ] = CSRC_C ; \
			m[dN ] = CSRC_N ; m[dS ] = CSRC_S ; \
			m[dE ] = CSRC_E ; m[dW ] = CSRC_W ; \
			m[dT ] = CSRC_T ; m[dB ] = CSRC_B ; \
			m[dNE] = CSRC_NE; m[dNW] = CSRC_NW; m[dSE] = CSRC_SE; m[dSW] = CSRC_SW; \
			m[dNT] = CSRC_NT; m[dNB] = CSRC_NB; m[dST] = CSRC_ST; m[dSB] = CSRC_SB; \
			m[dET] = CSRC_ET; m[dEB] = CSRC_EB; m[dWT] = CSRC_WT; m[dWB] = CSRC_WB; \
			\
			rho = MSRC_C  + MSRC_N + MSRC_S  + MSRC_E + MSRC_W  + MSRC_T  \
				+ MSRC_B  + MSRC_NE + MSRC_NW + MSRC_SE + MSRC_SW + MSRC_NT \
				+ MSRC_NB + MSRC_ST + MSRC_SB + MSRC_ET + MSRC_EB + MSRC_WT + MSRC_WB; \
			ux = MSRC_E - MSRC_W + MSRC_NE - MSRC_NW + MSRC_SE - MSRC_SW \
				+ MSRC_ET + MSRC_EB - MSRC_WT - MSRC_WB + mLevel[lev].gravity[0];  \
			uy = MSRC_N - MSRC_S + MSRC_NE + MSRC_NW - MSRC_SE - MSRC_SW \
				+ MSRC_NT + MSRC_NB - MSRC_ST - MSRC_SB + mLevel[lev].gravity[1];  \
			uz = MSRC_T - MSRC_B + MSRC_NT - MSRC_NB + MSRC_ST - MSRC_SB \
				+ MSRC_ET - MSRC_EB + MSRC_WT - MSRC_WB + mLevel[lev].gravity[2];  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			COLL_CALCULATE_DFEQ(lcsmeq); \
			COLL_CALCULATE_NONEQTENSOR(lev, MSRC_) \
			COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
			CSMOMEGA_STATS(lev,lcsmomega); \
			\
			RAC(tcel,dC ) = (1.0-lcsmomega)*MSRC_C  + lcsmomega*EQC ; \
			RAC(tcel,dN ) = (1.0-lcsmomega)*MSRC_N  + lcsmomega*lcsmeq[ dN ];  \
			RAC(tcel,dS ) = (1.0-lcsmomega)*MSRC_S  + lcsmomega*lcsmeq[ dS ];  \
			RAC(tcel,dE ) = (1.0-lcsmomega)*MSRC_E  + lcsmomega*lcsmeq[ dE ]; \
			RAC(tcel,dW ) = (1.0-lcsmomega)*MSRC_W  + lcsmomega*lcsmeq[ dW ];  \
			RAC(tcel,dT ) = (1.0-lcsmomega)*MSRC_T  + lcsmomega*lcsmeq[ dT ];  \
			RAC(tcel,dB ) = (1.0-lcsmomega)*MSRC_B  + lcsmomega*lcsmeq[ dB ]; \
			\
			RAC(tcel,dNE) = (1.0-lcsmomega)*MSRC_NE + lcsmomega*lcsmeq[ dNE];  \
			RAC(tcel,dNW) = (1.0-lcsmomega)*MSRC_NW + lcsmomega*lcsmeq[ dNW];  \
			RAC(tcel,dSE) = (1.0-lcsmomega)*MSRC_SE + lcsmomega*lcsmeq[ dSE];  \
			RAC(tcel,dSW) = (1.0-lcsmomega)*MSRC_SW + lcsmomega*lcsmeq[ dSW]; \
			\
			RAC(tcel,dNT) = (1.0-lcsmomega)*MSRC_NT + lcsmomega*lcsmeq[ dNT];  \
			RAC(tcel,dNB) = (1.0-lcsmomega)*MSRC_NB + lcsmomega*lcsmeq[ dNB];  \
			RAC(tcel,dST) = (1.0-lcsmomega)*MSRC_ST + lcsmomega*lcsmeq[ dST];  \
			RAC(tcel,dSB) = (1.0-lcsmomega)*MSRC_SB + lcsmomega*lcsmeq[ dSB]; \
			\
			RAC(tcel,dET) = (1.0-lcsmomega)*MSRC_ET + lcsmomega*lcsmeq[ dET];  \
			RAC(tcel,dEB) = (1.0-lcsmomega)*MSRC_EB + lcsmomega*lcsmeq[ dEB]; \
			RAC(tcel,dWT) = (1.0-lcsmomega)*MSRC_WT + lcsmomega*lcsmeq[ dWT];  \
			RAC(tcel,dWB) = (1.0-lcsmomega)*MSRC_WB + lcsmomega*lcsmeq[ dWB];  \

#define OPTIMIZED_STREAMCOLLIDE_UNUSED \
			/* only surrounded by fluid cells...!, so safe streaming here... */ \
			rho = CSRC_C  + CSRC_N + CSRC_S  + CSRC_E + CSRC_W  + CSRC_T  \
				+ CSRC_B  + CSRC_NE + CSRC_NW + CSRC_SE + CSRC_SW + CSRC_NT \
				+ CSRC_NB + CSRC_ST + CSRC_SB + CSRC_ET + CSRC_EB + CSRC_WT + CSRC_WB; \
			ux = CSRC_E - CSRC_W + CSRC_NE - CSRC_NW + CSRC_SE - CSRC_SW \
				+ CSRC_ET + CSRC_EB - CSRC_WT - CSRC_WB + mLevel[lev].gravity[0];  \
			uy = CSRC_N - CSRC_S + CSRC_NE + CSRC_NW - CSRC_SE - CSRC_SW \
				+ CSRC_NT + CSRC_NB - CSRC_ST - CSRC_SB + mLevel[lev].gravity[1];  \
			uz = CSRC_T - CSRC_B + CSRC_NT - CSRC_NB + CSRC_ST - CSRC_SB \
				+ CSRC_ET - CSRC_EB + CSRC_WT - CSRC_WB + mLevel[lev].gravity[2];  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			COLL_CALCULATE_DFEQ(lcsmeq); \
			COLL_CALCULATE_NONEQTENSOR(lev, CSRC_) \
			COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
			\
			RAC(tcel,dC ) = (1.0-lcsmomega)*CSRC_C  + lcsmomega*EQC ; \
			RAC(tcel,dN ) = (1.0-lcsmomega)*CSRC_N  + lcsmomega*lcsmeq[ dN ];  \
			RAC(tcel,dS ) = (1.0-lcsmomega)*CSRC_S  + lcsmomega*lcsmeq[ dS ];  \
			RAC(tcel,dE ) = (1.0-lcsmomega)*CSRC_E  + lcsmomega*lcsmeq[ dE ]; \
			RAC(tcel,dW ) = (1.0-lcsmomega)*CSRC_W  + lcsmomega*lcsmeq[ dW ];  \
			RAC(tcel,dT ) = (1.0-lcsmomega)*CSRC_T  + lcsmomega*lcsmeq[ dT ];  \
			RAC(tcel,dB ) = (1.0-lcsmomega)*CSRC_B  + lcsmomega*lcsmeq[ dB ]; \
			\
			RAC(tcel,dNE) = (1.0-lcsmomega)*CSRC_NE + lcsmomega*lcsmeq[ dNE];  \
			RAC(tcel,dNW) = (1.0-lcsmomega)*CSRC_NW + lcsmomega*lcsmeq[ dNW];  \
			RAC(tcel,dSE) = (1.0-lcsmomega)*CSRC_SE + lcsmomega*lcsmeq[ dSE];  \
			RAC(tcel,dSW) = (1.0-lcsmomega)*CSRC_SW + lcsmomega*lcsmeq[ dSW]; \
			\
			RAC(tcel,dNT) = (1.0-lcsmomega)*CSRC_NT + lcsmomega*lcsmeq[ dNT];  \
			RAC(tcel,dNB) = (1.0-lcsmomega)*CSRC_NB + lcsmomega*lcsmeq[ dNB];  \
			RAC(tcel,dST) = (1.0-lcsmomega)*CSRC_ST + lcsmomega*lcsmeq[ dST];  \
			RAC(tcel,dSB) = (1.0-lcsmomega)*CSRC_SB + lcsmomega*lcsmeq[ dSB]; \
			\
			RAC(tcel,dET) = (1.0-lcsmomega)*CSRC_ET + lcsmomega*lcsmeq[ dET];  \
			RAC(tcel,dEB) = (1.0-lcsmomega)*CSRC_EB + lcsmomega*lcsmeq[ dEB]; \
			RAC(tcel,dWT) = (1.0-lcsmomega)*CSRC_WT + lcsmomega*lcsmeq[ dWT];  \
			RAC(tcel,dWB) = (1.0-lcsmomega)*CSRC_WB + lcsmomega*lcsmeq[ dWB];  \

#define OPTIMIZED_STREAMCOLLIDE_NOLES \
			/* only surrounded by fluid cells...!, so safe streaming here... */ \
			rho = CSRC_C  + CSRC_N + CSRC_S  + CSRC_E + CSRC_W  + CSRC_T  \
				+ CSRC_B  + CSRC_NE + CSRC_NW + CSRC_SE + CSRC_SW + CSRC_NT \
				+ CSRC_NB + CSRC_ST + CSRC_SB + CSRC_ET + CSRC_EB + CSRC_WT + CSRC_WB; \
			ux = CSRC_E - CSRC_W + CSRC_NE - CSRC_NW + CSRC_SE - CSRC_SW \
				+ CSRC_ET + CSRC_EB - CSRC_WT - CSRC_WB + mLevel[lev].gravity[0];  \
			uy = CSRC_N - CSRC_S + CSRC_NE + CSRC_NW - CSRC_SE - CSRC_SW \
				+ CSRC_NT + CSRC_NB - CSRC_ST - CSRC_SB + mLevel[lev].gravity[1];  \
			uz = CSRC_T - CSRC_B + CSRC_NT - CSRC_NB + CSRC_ST - CSRC_SB \
				+ CSRC_ET - CSRC_EB + CSRC_WT - CSRC_WB + mLevel[lev].gravity[2];  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			RAC(tcel,dC ) = (1.0-OMEGA(lev))*CSRC_C  + OMEGA(lev)*EQC ; \
			RAC(tcel,dN ) = (1.0-OMEGA(lev))*CSRC_N  + OMEGA(lev)*EQN ;  \
			RAC(tcel,dS ) = (1.0-OMEGA(lev))*CSRC_S  + OMEGA(lev)*EQS ;  \
			RAC(tcel,dE ) = (1.0-OMEGA(lev))*CSRC_E  + OMEGA(lev)*EQE ; \
			RAC(tcel,dW ) = (1.0-OMEGA(lev))*CSRC_W  + OMEGA(lev)*EQW ;  \
			RAC(tcel,dT ) = (1.0-OMEGA(lev))*CSRC_T  + OMEGA(lev)*EQT ;  \
			RAC(tcel,dB ) = (1.0-OMEGA(lev))*CSRC_B  + OMEGA(lev)*EQB ; \
			 \
			RAC(tcel,dNE) = (1.0-OMEGA(lev))*CSRC_NE + OMEGA(lev)*EQNE;  \
			RAC(tcel,dNW) = (1.0-OMEGA(lev))*CSRC_NW + OMEGA(lev)*EQNW;  \
			RAC(tcel,dSE) = (1.0-OMEGA(lev))*CSRC_SE + OMEGA(lev)*EQSE;  \
			RAC(tcel,dSW) = (1.0-OMEGA(lev))*CSRC_SW + OMEGA(lev)*EQSW; \
			 \
			RAC(tcel,dNT) = (1.0-OMEGA(lev))*CSRC_NT + OMEGA(lev)*EQNT;  \
			RAC(tcel,dNB) = (1.0-OMEGA(lev))*CSRC_NB + OMEGA(lev)*EQNB;  \
			RAC(tcel,dST) = (1.0-OMEGA(lev))*CSRC_ST + OMEGA(lev)*EQST;  \
			RAC(tcel,dSB) = (1.0-OMEGA(lev))*CSRC_SB + OMEGA(lev)*EQSB; \
			 \
			RAC(tcel,dET) = (1.0-OMEGA(lev))*CSRC_ET + OMEGA(lev)*EQET;  \
			RAC(tcel,dEB) = (1.0-OMEGA(lev))*CSRC_EB + OMEGA(lev)*EQEB; \
			RAC(tcel,dWT) = (1.0-OMEGA(lev))*CSRC_WT + OMEGA(lev)*EQWT;  \
			RAC(tcel,dWB) = (1.0-OMEGA(lev))*CSRC_WB + OMEGA(lev)*EQWB;  \


// debug version1
#define STREAMCHECK(ni,nj,nk,nl) 
#define COLLCHECK
#define OPTIMIZED_STREAMCOLLIDE_DEBUG \
			m[0] = RAC(ccel,0); \
			FORDF1 { /* df0 is set later on... */ \
				if(RFLAG_NB(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); D::mPanic=1;  \
				} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			}   \
			rho=m[0]; ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago , &mDebugOmegaRet  ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			FORDF0 { RAC(tcel,l) = m[l]; } \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
			COLLCHECK;



// more debugging
/*DEBUG \
			m[0] = RAC(ccel,0); \
			FORDF1 { \
				if(RFLAG_NB(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); D::mPanic=1;  \
				} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
			}   \
f__printf(stderr,"QSDM at %d,%d,%d  lcsmqo=%25.15f, lcsmomega=%f \n", i,j,k, lcsmqo,lcsmomega ); \
			rho=m[0]; ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago  , &mDebugOmegaRet ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			*/
#if USE_LES==1
#define DEFAULT_COLLIDE DEFAULT_COLLIDE_LES
#define OPTIMIZED_STREAMCOLLIDE OPTIMIZED_STREAMCOLLIDE_LES
#else 
#define DEFAULT_COLLIDE DEFAULT_COLLIDE_NOLES
#define OPTIMIZED_STREAMCOLLIDE OPTIMIZED_STREAMCOLLIDE_NOLES
#endif

#endif

#define USQRMAXCHECK(Cusqr,Cux,Cuy,Cuz,  CmMaxVlen,CmMxvx,CmMxvy,CmMxvz) \
			if(Cusqr>CmMaxVlen) { \
				CmMxvx = Cux; CmMxvy = Cuy; CmMxvz = Cuz; CmMaxVlen = Cusqr; \
			} /* stats */ 



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


/******************************************************************************
 * Lbm Constructor
 *****************************************************************************/
template<class D>
LbmFsgrSolver<D>::LbmFsgrSolver() :
	D(),
	mCurrentMass(0.0), mCurrentVolume(0.0),
	mNumProblems(0), 
	mAvgMLSUPS(0.0), mAvgMLSUPSCnt(0.0),
	mpPreviewSurface(NULL), 
	mLoopSubdivs(0), mSmoothSurface(0.0), mSmoothNormals(0.0),
	mTimeAdap(false), 
	mOutputSurfacePreview(0), mPreviewFactor(0.25),
	mFVHeight(0.0), mFVArea(1.0), mUpdateFVHeight(false),
	mGfxGeoSetup(0), mGfxEndTime(-1.0), mInitSurfaceSmoothing(0),
	mTimestepReduceLock(0),
	mTimeSwitchCounts(0), 
	mSimulationTime(0.0),
	mMinStepTime(0.0), mMaxStepTime(0.0),
	mMaxNoCells(0), mMinNoCells(0), mAvgNumUsedCells(0),
	mDropMode(1), mDropSize(0.15), mDropSpeed(0.0),
	mDropping(false),
	mDropX(0.0), mDropY(0.0), mDropHeight(0.8),
	mIsoWeightMethod(2),
	mMaxRefine(1), 
	mDfScaleUp(-1.0), mDfScaleDown(-1.0),
	mInitialCsmago(0.04), mInitialCsmagoCoarse(1.0), mDebugOmegaRet(0.0),
	mNumInvIfTotal(0), mNumFsgrChanges(0),
	mDisableStandingFluidInit(0),
	mForceTadapRefine(-1)
{
  // not much to do here... 
	D::mpIso = new IsoSurface( D::mIsoValue, false );

  // init equilibrium dist. func 
  LbmFloat rho=1.0;
  FORDF0 {
		D::dfEquil[l] = D::getCollideEq( l,rho,  0.0, 0.0, 0.0);
  }

	// init LES
	int odm = 0;
	for(int m=0; m<D::cDimension; m++) { 
		for(int l=0; l<D::cDfNum; l++) { 
			D::lesCoeffDiag[m][l] = 
			D::lesCoeffOffdiag[m][l] = 0.0;
		}
	}
	for(int m=0; m<D::cDimension; m++) { 
		for(int n=0; n<D::cDimension; n++) { 
			for(int l=1; l<D::cDfNum; l++) { 
				LbmFloat em;
				switch(m) {
					case 0: em = D::dfDvecX[l]; break;
					case 1: em = D::dfDvecY[l]; break;
					case 2: em = D::dfDvecZ[l]; break;
					default: em = -1.0; errFatal("SMAGO1","err m="<<m, SIMWORLD_GENERICERROR);
				}
				LbmFloat en;
				switch(n) {
					case 0: en = D::dfDvecX[l]; break;
					case 1: en = D::dfDvecY[l]; break;
					case 2: en = D::dfDvecZ[l]; break;
					default: en = -1.0; errFatal("SMAGO2","err n="<<n, SIMWORLD_GENERICERROR);
				}
				const LbmFloat coeff = em*en;
				if(m==n) {
					D::lesCoeffDiag[m][l] = coeff;
				} else {
					if(m>n) {
						D::lesCoeffOffdiag[odm][l] = coeff;
					}
				}
			}

			if(m==n) {
			} else {
				if(m>n) odm++;
			}
		}
	}

	mDvecNrm[0] = LbmVec(0.0);
  FORDF1 {
		mDvecNrm[l] = getNormalized( 
			LbmVec(D::dfDvecX[D::dfInv[l]], D::dfDvecY[D::dfInv[l]], D::dfDvecZ[D::dfInv[l]] ) 
			) * -1.0; 
	}

	addDrop(false,0,0);
}

/*****************************************************************************/
/* Destructor */
/*****************************************************************************/
template<class D>
LbmFsgrSolver<D>::~LbmFsgrSolver()
{
  if(!D::mInitDone){ debugOut("LbmFsgrSolver::LbmFsgrSolver : not inited...",0); return; }

#if COMPRESSGRIDS==1
	delete mLevel[mMaxRefine].mprsCells[1];
	mLevel[mMaxRefine].mprsCells[0] = mLevel[mMaxRefine].mprsCells[1] = NULL;
#endif // COMPRESSGRIDS==1

	for(int i=0; i<=mMaxRefine; i++) {
		for(int s=0; s<2; s++) {
			if(mLevel[i].mprsCells[s]) delete [] mLevel[i].mprsCells[s];
			if(mLevel[i].mprsFlags[s]) delete [] mLevel[i].mprsFlags[s];
		}
	}
	delete D::mpIso;
	if(mpPreviewSurface) delete mpPreviewSurface;

	// always output performance estimate
	debMsgStd("LbmFsgrSolver::~LbmFsgrSolver",DM_MSG," Avg. MLSUPS:"<<(mAvgMLSUPS/mAvgMLSUPSCnt), 5);
  if(!D::mSilent) debMsgStd("LbmFsgrSolver::~LbmFsgrSolver",DM_MSG,"Deleted...",10);
}




/******************************************************************************
 * initilize variables fom attribute list 
 *****************************************************************************/
template<class D>
void 
LbmFsgrSolver<D>::parseAttrList()
{
	LbmSolverInterface::parseStdAttrList();

	string matIso("default");
	matIso = D::mpAttrs->readString("material_surf", matIso, "SimulationLbm","mpIso->material", false );
	D::mpIso->setMaterialName( matIso );
	mOutputSurfacePreview = D::mpAttrs->readInt("surfacepreview", mOutputSurfacePreview, "SimulationLbm","mOutputSurfacePreview", false );
	mTimeAdap = D::mpAttrs->readBool("timeadap", mTimeAdap, "SimulationLbm","mTimeAdap", false );

	mIsoWeightMethod= D::mpAttrs->readInt("isoweightmethod", mIsoWeightMethod, "SimulationLbm","mIsoWeightMethod", false );
	mInitSurfaceSmoothing = D::mpAttrs->readInt("initsurfsmooth", mInitSurfaceSmoothing, "SimulationLbm","mInitSurfaceSmoothing", false );
	mLoopSubdivs = D::mpAttrs->readInt("loopsubdivs", mLoopSubdivs, "SimulationLbm","mLoopSubdivs", false );
	mSmoothSurface = D::mpAttrs->readFloat("smoothsurface", mSmoothSurface, "SimulationLbm","mSmoothSurface", false );
	mSmoothNormals = D::mpAttrs->readFloat("smoothnormals", mSmoothNormals, "SimulationLbm","mSmoothNormals", false );

	mInitialCsmago = D::mpAttrs->readFloat("csmago", mInitialCsmago, "SimulationLbm","mInitialCsmago", false );
	mInitialCsmagoCoarse = D::mpAttrs->readFloat("csmago_coarse", mInitialCsmagoCoarse, "SimulationLbm","mInitialCsmagoCoarse", false );

	// refinement
	mMaxRefine  = D::mpAttrs->readInt("maxrefine",  mMaxRefine ,"LbmFsgrSolver", "mMaxRefine", true);
	mDisableStandingFluidInit = D::mpAttrs->readInt("disable_stfluidinit", mDisableStandingFluidInit,"LbmFsgrSolver", "mDisableStandingFluidInit", false);
	mForceTadapRefine = D::mpAttrs->readInt("forcetadaprefine", mForceTadapRefine,"LbmFsgrSolver", "mForceTadapRefine", false);

	// demo mode settings
	mDropMode = D::mpAttrs->readInt("dropmode", mDropMode, "SimulationLbm","mDropMode", false );
	mDropSize = D::mpAttrs->readFloat("dropsize", mDropSize, "SimulationLbm","mDropSize", false );
	mDropHeight = D::mpAttrs->readFloat("dropheight", mDropHeight, "SimulationLbm","mDropHeight", false );
	mDropSpeed = vec2L( D::mpAttrs->readVec3d("dropspeed", ntlVec3d(0.0), "SimulationLbm","mDropSpeed", false ) );
	if( (mDropMode>2) || (mDropMode<-1) ) mDropMode=1;
	mGfxGeoSetup = D::mpAttrs->readInt("gfxgeosetup", mGfxGeoSetup, "SimulationLbm","mGfxGeoSetup", false );
	mGfxEndTime = D::mpAttrs->readFloat("gfxendtime", mGfxEndTime, "SimulationLbm","mGfxEndTime", false );
	mFVHeight = D::mpAttrs->readFloat("fvolheight", mFVHeight, "SimulationLbm","mFVHeight", false );
	mFVArea   = D::mpAttrs->readFloat("fvolarea", mFVArea, "SimulationLbm","mFArea", false );

}


/******************************************************************************
 * Initialize omegas and forces on all levels (for init/timestep change)
 *****************************************************************************/
template<class D>
void 
LbmFsgrSolver<D>::initLevelOmegas()
{
	// no explicit settings
	D::mOmega = D::mpParam->calculateOmega();
	D::mGravity = vec2L( D::mpParam->calculateGravity() );
	D::mSurfaceTension = D::mpParam->calculateSurfaceTension(); // unused

	if(mInitialCsmago<=0.0) {
		if(OPT3D==1) {
			errFatal("LbmFsgrSolver::initLevelOmegas","Csmago-LES = 0 not supported for optimized 3D version...",SIMWORLD_INITERROR); 
			return;
		}
	}

	// use Tau instead of Omega for calculations
	{ // init base level
		int i = mMaxRefine;
		mLevel[i].omega    = D::mOmega;
		mLevel[i].stepsize = D::mpParam->getStepTime();
		mLevel[i].lcsmago = mInitialCsmago; //CSMAGO_INITIAL;
		mLevel[i].lcsmago_sqr = mLevel[i].lcsmago*mLevel[i].lcsmago;
		mLevel[i].lcnu = (2.0* (1.0/mLevel[i].omega)-1.0) * (1.0/6.0);
	}

	// init all sub levels
	for(int i=mMaxRefine-1; i>=0; i--) {
		//mLevel[i].omega = 2.0 * (mLevel[i+1].omega-0.5) + 0.5;
		double nomega = 0.5 * (  (1.0/(double)mLevel[i+1].omega) -0.5) + 0.5;
		nomega                = 1.0/nomega;
		mLevel[i].omega       = (LbmFloat)nomega;
		mLevel[i].stepsize    = 2.0 * mLevel[i+1].stepsize;
		//mLevel[i].lcsmago     = mLevel[i+1].lcsmago*mCsmagoRefineMultiplier;
		//if(mLevel[i].lcsmago>1.0) mLevel[i].lcsmago = 1.0;
		//if(strstr(D::getName().c_str(),"Debug")){ 
		//mLevel[i].lcsmago = mLevel[mMaxRefine].lcsmago; // DEBUG
		// if(strstr(D::getName().c_str(),"Debug")) mLevel[i].lcsmago = mLevel[mMaxRefine].lcsmago * (LbmFloat)(mMaxRefine-i)*0.5+1.0; 
		//if(strstr(D::getName().c_str(),"Debug")) mLevel[i].lcsmago = mLevel[mMaxRefine].lcsmago * ((LbmFloat)(mMaxRefine-i)*1.0 + 1.0 ); 
		//if(strstr(D::getName().c_str(),"Debug")) mLevel[i].lcsmago = 0.99;
		mLevel[i].lcsmago = mInitialCsmagoCoarse;
		mLevel[i].lcsmago_sqr = mLevel[i].lcsmago*mLevel[i].lcsmago;
		mLevel[i].lcnu        = (2.0* (1.0/mLevel[i].omega)-1.0) * (1.0/6.0);
	}
	
	// for lbgk
	mLevel[ mMaxRefine ].gravity = D::mGravity / mLevel[ mMaxRefine ].omega;
	for(int i=mMaxRefine-1; i>=0; i--) {
		// should be the same on all levels...
		// for lbgk
		mLevel[i].gravity = (mLevel[i+1].gravity * mLevel[i+1].omega) * 2.0 / mLevel[i].omega;
	}

	// debug? invalidate old values...
	D::mGravity = -100.0;
	D::mOmega = -100.0;

	for(int i=0; i<=mMaxRefine; i++) {
		if(!D::mSilent) {
			errMsg("LbmFsgrSolver", "Level init "<<i<<" - sizes:"<<mLevel[i].lSizex<<","<<mLevel[i].lSizey<<","<<mLevel[i].lSizez<<" offs:"<<mLevel[i].lOffsx<<","<<mLevel[i].lOffsy<<","<<mLevel[i].lOffsz 
					<<" omega:"<<mLevel[i].omega<<" grav:"<<mLevel[i].gravity<< ", "
					<<" cmsagp:"<<mLevel[i].lcsmago<<", "
					<< " ss"<<mLevel[i].stepsize<<" ns"<<mLevel[i].nodeSize<<" cs"<<mLevel[i].simCellSize );
		} else {
			if(!D::mInitDone) {
				debMsgStd("LbmFsgrSolver", DM_MSG, "Level init "<<i<<" - sizes:"<<mLevel[i].lSizex<<","<<mLevel[i].lSizey<<","<<mLevel[i].lSizez<<" "
						<<"omega:"<<mLevel[i].omega<<" grav:"<<mLevel[i].gravity , 5);
			}
		}
	}
	if(mMaxRefine>0) {
		mDfScaleUp   = (mLevel[0  ].stepsize/mLevel[0+1].stepsize)* (1.0/mLevel[0  ].omega-1.0)/ (1.0/mLevel[0+1].omega-1.0); // yu
		mDfScaleDown = (mLevel[0+1].stepsize/mLevel[0  ].stepsize)* (1.0/mLevel[0+1].omega-1.0)/ (1.0/mLevel[0  ].omega-1.0); // yu
	}
}


/******************************************************************************
 * Init Solver (values should be read from config file)
 *****************************************************************************/
template<class D>
bool 
LbmFsgrSolver<D>::initialize( ntlTree* /*tree*/, vector<ntlGeometryObject*>* /*objects*/ )
{
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Init start... (Layout:"<<ALSTRING<<") ",1);

	// fix size inits to force cubic cells and mult4 level dimensions
	const int debugGridsizeInit = 1;
	mPreviewFactor = (LbmFloat)mOutputSurfacePreview / (LbmFloat)D::mSizex;
	int maxGridSize = D::mSizex; // get max size
	if(D::mSizey>maxGridSize) maxGridSize = D::mSizey;
	if(D::mSizez>maxGridSize) maxGridSize = D::mSizez;
	LbmFloat maxGeoSize = (D::mvGeoEnd[0]-D::mvGeoStart[0]); // get max size
	if((D::mvGeoEnd[1]-D::mvGeoStart[1])>maxGridSize) maxGeoSize = (D::mvGeoEnd[1]-D::mvGeoStart[1]);
	if((D::mvGeoEnd[2]-D::mvGeoStart[2])>maxGridSize) maxGeoSize = (D::mvGeoEnd[2]-D::mvGeoStart[2]);
	// FIXME better divide max geo size by corresponding resolution rather than max? no prob for rx==ry==rz though
	LbmFloat cellSize = (maxGeoSize / (LbmFloat)maxGridSize);
  if(debugGridsizeInit) debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Start:"<<D::mvGeoStart<<" End:"<<D::mvGeoEnd<<" maxS:"<<maxGeoSize<<" maxG:"<<maxGridSize<<" cs:"<<cellSize, 10);
	// force grid sizes according to geom. size, rounded
	D::mSizex = (int) ((D::mvGeoEnd[0]-D::mvGeoStart[0]) / cellSize +0.5);
	D::mSizey = (int) ((D::mvGeoEnd[1]-D::mvGeoStart[1]) / cellSize +0.5);
	D::mSizez = (int) ((D::mvGeoEnd[2]-D::mvGeoStart[2]) / cellSize +0.5);
	// match refinement sizes, round downwards to multiple of 4
	int sizeMask = 0;
	int maskBits = mMaxRefine;
	if(PARALLEL==1) maskBits+=2;
	for(int i=0; i<maskBits; i++) { sizeMask |= (1<<i); }
	sizeMask = ~sizeMask;
  if(debugGridsizeInit) debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Size X:"<<D::mSizex<<" Y:"<<D::mSizey<<" Z:"<<D::mSizez<<" m"<<convertCellFlagType2String(sizeMask) ,10);
	D::mSizex &= sizeMask;
	D::mSizey &= sizeMask;
	D::mSizez &= sizeMask;
	// force geom size to match rounded grid sizes
	D::mvGeoEnd[0] = D::mvGeoStart[0] + cellSize*(LbmFloat)D::mSizex;
	D::mvGeoEnd[1] = D::mvGeoStart[1] + cellSize*(LbmFloat)D::mSizey;
	D::mvGeoEnd[2] = D::mvGeoStart[2] + cellSize*(LbmFloat)D::mSizez;

  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Final domain size X:"<<D::mSizex<<" Y:"<<D::mSizey<<" Z:"<<D::mSizez<<
			", Domain: "<<D::mvGeoStart<<":"<<D::mvGeoEnd<<", "<<(D::mvGeoEnd-D::mvGeoStart) ,2);
  //debMsgStd("LbmFsgrSolver::initialize",DM_MSG, ,2);
	D::mpParam->setSize(D::mSizex, D::mSizey, D::mSizez);

  //debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Size X:"<<D::mSizex<<" Y:"<<D::mSizey<<" Z:"<<D::mSizez ,2);

#if ELBEEM_BLENDER!=1
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Definitions: "
		<<"LBM_EPSILON="<<LBM_EPSILON       <<" "
		<<"FSGR_STRICT_DEBUG="<<FSGR_STRICT_DEBUG       <<" "
		<<"INTORDER="<<INTORDER        <<" "
		<<"TIMEINTORDER="<<TIMEINTORDER        <<" "
		<<"REFINEMENTBORDER="<<REFINEMENTBORDER        <<" "
		<<"OPT3D="<<OPT3D        <<" "
		<<"COMPRESSGRIDS="<<COMPRESSGRIDS<<" "
		<<"LS_FLUIDTHRESHOLD="<<LS_FLUIDTHRESHOLD        <<" "
		<<"MASS_INVALID="<<MASS_INVALID        <<" "
		<<"FSGR_LISTTRICK="<<FSGR_LISTTRICK            <<" "
		<<"FSGR_LISTTTHRESHEMPTY="<<FSGR_LISTTTHRESHEMPTY          <<" "
		<<"FSGR_LISTTTHRESHFULL="<<FSGR_LISTTTHRESHFULL           <<" "
		<<"FSGR_MAGICNR="<<FSGR_MAGICNR              <<" " 
		<<"USE_LES="<<USE_LES              <<" " 
		,10);
#endif // ELBEEM_BLENDER!=1

	// perform 2D corrections...
	if(D::cDimension == 2) D::mSizez = 1;

	D::mpParam->setSimulationMaxSpeed(0.0);
	if(mFVHeight>0.0) D::mpParam->setFluidVolumeHeight(mFVHeight);
	D::mpParam->setTadapLevels( mMaxRefine+1 );

	if(mForceTadapRefine>mMaxRefine) {
		D::mpParam->setTadapLevels( mForceTadapRefine+1 );
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Forcing a t-adap refine level of "<<mForceTadapRefine, 6);
	}

	if(!D::mpParam->calculateAllMissingValues()) {
		errFatal("LbmFsgrSolver::initialize","Fatal: failed to init parameters! Aborting...",SIMWORLD_INITERROR);
		return false;
	}
	// recalc objects speeds in geo init



	// init vectors
	if(mMaxRefine >= MAX_LEV) {
		errFatal("LbmFsgrSolver::initializeLbmGridref"," error: Too many levels!", SIMWORLD_INITERROR);
		return false;
	}
	for(int i=0; i<=mMaxRefine; i++) {
		mLevel[i].id = i;
		mLevel[i].nodeSize = 0.0; 
		mLevel[i].simCellSize = 0.0; 
		mLevel[i].omega = 0.0; 
		mLevel[i].time = 0.0; 
		mLevel[i].stepsize = 1.0;
		mLevel[i].gravity = LbmVec(0.0); 
		mLevel[i].mprsCells[0] = NULL;
		mLevel[i].mprsCells[1] = NULL;
		mLevel[i].mprsFlags[0] = NULL;
		mLevel[i].mprsFlags[1] = NULL;

		mLevel[i].avgOmega = 0.0; 
		mLevel[i].avgOmegaCnt = 0.0;
	}

	// init sizes
	mLevel[mMaxRefine].lSizex = D::mSizex;
	mLevel[mMaxRefine].lSizey = D::mSizey;
	mLevel[mMaxRefine].lSizez = D::mSizez;
	for(int i=mMaxRefine-1; i>=0; i--) {
		mLevel[i].lSizex = mLevel[i+1].lSizex/2;
		mLevel[i].lSizey = mLevel[i+1].lSizey/2;
		mLevel[i].lSizez = mLevel[i+1].lSizez/2;
		/*if( ((mLevel[i].lSizex % 4) != 0) || ((mLevel[i].lSizey % 4) != 0) || ((mLevel[i].lSizez % 4) != 0) ) {
			errMsg("LbmFsgrSolver","Init: error invalid sizes on level "<<i<<" "<<PRINT_VEC(mLevel[i].lSizex,mLevel[i].lSizey,mLevel[i].lSizez) );
			xit(1);
		}// old QUAD handling */
	}

	// estimate memory usage
	{
		unsigned long int memCnt = 0;
		unsigned long int rcellSize = ((mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez) *dTotalNum);
		memCnt += sizeof(CellFlagType) * (rcellSize/dTotalNum +4) *2;
#if COMPRESSGRIDS==0
		memCnt += sizeof(LbmFloat) * (rcellSize +4) *2;
#else // COMPRESSGRIDS==0
		unsigned long int compressOffset = (mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*dTotalNum*2);
		memCnt += sizeof(LbmFloat) * (rcellSize+compressOffset +4);
#endif // COMPRESSGRIDS==0
		for(int i=mMaxRefine-1; i>=0; i--) {
			rcellSize = ((mLevel[i].lSizex*mLevel[i].lSizey*mLevel[i].lSizez) *dTotalNum);
			memCnt += sizeof(CellFlagType) * (rcellSize/dTotalNum +4) *2;
			memCnt += sizeof(LbmFloat) * (rcellSize +4) *2;
		}
		double memd = memCnt;
		char *sizeStr = "";
		const double sfac = 1000.0;
		if(memd>sfac){ memd /= sfac; sizeStr="KB"; }
		if(memd>sfac){ memd /= sfac; sizeStr="MB"; }
		if(memd>sfac){ memd /= sfac; sizeStr="GB"; }
		if(memd>sfac){ memd /= sfac; sizeStr="TB"; }
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Required Grid memory: "<< memd <<" "<< sizeStr<<" ",4);
	}

	// safety check
	if(sizeof(CellFlagType) != CellFlagTypeSize) {
		errFatal("LbmFsgrSolver::initialize","Fatal Error: CellFlagType has wrong size! Is:"<<sizeof(CellFlagType)<<", should be:"<<CellFlagTypeSize, SIMWORLD_GENERICERROR);
		return false;
	}

	mLevel[ mMaxRefine ].nodeSize = ((D::mvGeoEnd[0]-D::mvGeoStart[0]) / (LbmFloat)(D::mSizex));
	mLevel[ mMaxRefine ].simCellSize = D::mpParam->getCellSize();
	mLevel[ mMaxRefine ].lcellfactor = 1.0;
	unsigned long int rcellSize = ((mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez) *dTotalNum);
	// +4 for safety ?
	mLevel[ mMaxRefine ].mprsFlags[0] = new CellFlagType[ rcellSize/dTotalNum +4 ];
	mLevel[ mMaxRefine ].mprsFlags[1] = new CellFlagType[ rcellSize/dTotalNum +4 ];

#if COMPRESSGRIDS==0
	mLevel[ mMaxRefine ].mprsCells[0] = new LbmFloat[ rcellSize +4 ];
	mLevel[ mMaxRefine ].mprsCells[1] = new LbmFloat[ rcellSize +4 ];
#else // COMPRESSGRIDS==0
	unsigned long int compressOffset = (mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*dTotalNum*2);
	mLevel[ mMaxRefine ].mprsCells[1] = new LbmFloat[ rcellSize +compressOffset +4 ];
	mLevel[ mMaxRefine ].mprsCells[0] = mLevel[ mMaxRefine ].mprsCells[1]+compressOffset;
	//errMsg("CGD","rcs:"<<rcellSize<<" cpff:"<<compressOffset<< " c0:"<<mLevel[ mMaxRefine ].mprsCells[0]<<" c1:"<<mLevel[ mMaxRefine ].mprsCells[1]<< " c0e:"<<(mLevel[ mMaxRefine ].mprsCells[0]+rcellSize)<<" c1:"<<(mLevel[ mMaxRefine ].mprsCells[1]+rcellSize)); // DEBUG
#endif // COMPRESSGRIDS==0

	LbmFloat lcfdimFac = 8.0;
	if(D::cDimension==2) lcfdimFac = 4.0;
	for(int i=mMaxRefine-1; i>=0; i--) {
		mLevel[i].nodeSize = 2.0 * mLevel[i+1].nodeSize;
		mLevel[i].simCellSize = 2.0 * mLevel[i+1].simCellSize;
		mLevel[i].lcellfactor = mLevel[i+1].lcellfactor * lcfdimFac;

		if(D::cDimension==2){ mLevel[i].lSizez = 1; } // 2D
		rcellSize = ((mLevel[i].lSizex*mLevel[i].lSizey*mLevel[i].lSizez) *dTotalNum);
		mLevel[i].mprsFlags[0] = new CellFlagType[ rcellSize/dTotalNum +4 ];
		mLevel[i].mprsFlags[1] = new CellFlagType[ rcellSize/dTotalNum +4 ];
		mLevel[i].mprsCells[0] = new LbmFloat[ rcellSize +4 ];
		mLevel[i].mprsCells[1] = new LbmFloat[ rcellSize +4 ];
	}

	// init sizes for _all_ levels
	for(int i=mMaxRefine; i>=0; i--) {
		mLevel[i].lOffsx = mLevel[i].lSizex;
		mLevel[i].lOffsy = mLevel[i].lOffsx*mLevel[i].lSizey;
		mLevel[i].lOffsz = mLevel[i].lOffsy*mLevel[i].lSizez;
  	mLevel[i].setCurr  = 0;
  	mLevel[i].setOther = 1;
  	mLevel[i].lsteps = 0;
  	mLevel[i].lmass = 0.0;
  	mLevel[i].lvolume = 0.0;
	}

	// calc omega, force for all levels
	initLevelOmegas();
	mMinStepTime = D::mpParam->getStepTime();
	mMaxStepTime = D::mpParam->getStepTime();

	// init isosurf
	D::mpIso->setIsolevel( D::mIsoValue );
	D::mpIso->setLoopSubdivs( mLoopSubdivs );
	// approximate feature size with mesh resolution
	float featureSize = mLevel[ mMaxRefine ].nodeSize*0.5;
	D::mpIso->setSmoothSurface( mSmoothSurface * featureSize );
	D::mpIso->setSmoothNormals( mSmoothNormals * featureSize );

	// init iso weight values mIsoWeightMethod
	int wcnt = 0;
	float totw = 0.0;
	for(int ak=-1;ak<=1;ak++) 
		for(int aj=-1;aj<=1;aj++) 
			for(int ai=-1;ai<=1;ai++)  {
				switch(mIsoWeightMethod) {
				case 1: // light smoothing
					mIsoWeight[wcnt] = sqrt(3.0) - sqrt( (LbmFloat)(ak*ak + aj*aj + ai*ai) );
					break;
				case 2: // very light smoothing
					mIsoWeight[wcnt] = sqrt(3.0) - sqrt( (LbmFloat)(ak*ak + aj*aj + ai*ai) );
					mIsoWeight[wcnt] *= mIsoWeight[wcnt];
					break;
				case 3: // no smoothing
					if(ai==0 && aj==0 && ak==0) mIsoWeight[wcnt] = 1.0;
					else mIsoWeight[wcnt] = 0.0;
					break;
				default: // strong smoothing (=0)
					mIsoWeight[wcnt] = 1.0;
					break;
				}
				totw += mIsoWeight[wcnt];
				wcnt++;
			}
	for(int i=0; i<27; i++) mIsoWeight[i] /= totw;

	LbmVec isostart = vec2L(D::mvGeoStart);
	LbmVec isoend   = vec2L(D::mvGeoEnd);
	int twodOff = 0; // 2d slices
	if(D::cDimension==2) {
		LbmFloat sn,se;
		sn = isostart[2]+(isoend[2]-isostart[2])*0.5 - ((isoend[0]-isostart[0]) / (LbmFloat)(D::mSizex+1.0))*0.5;
		se = isostart[2]+(isoend[2]-isostart[2])*0.5 + ((isoend[0]-isostart[0]) / (LbmFloat)(D::mSizex+1.0))*0.5;
		isostart[2] = sn;
		isoend[2] = se;
		twodOff = 2;
	}
	//errMsg(" SETISO ", " "<<isostart<<" - "<<isoend<<" "<<(((isoend[0]-isostart[0]) / (LbmFloat)(D::mSizex+1.0))*0.5)<<" "<<(LbmFloat)(D::mSizex+1.0)<<" " );
	D::mpIso->setStart( vec2G(isostart) );
	D::mpIso->setEnd(   vec2G(isoend) );
	LbmVec isodist = isoend-isostart;
	D::mpIso->initializeIsosurface( D::mSizex+2, D::mSizey+2, D::mSizez+2+twodOff, vec2G(isodist) );
	for(int ak=0;ak<D::mSizez+2+twodOff;ak++) 
		for(int aj=0;aj<D::mSizey+2;aj++) 
			for(int ai=0;ai<D::mSizex+2;ai++) { *D::mpIso->getData(ai,aj,ak) = 0.0; }

  /* init array (set all invalid first) */
	for(int lev=0; lev<=mMaxRefine; lev++) {
		FSGR_FORIJK_BOUNDS(lev) {
			RFLAG(lev,i,j,k,0) = RFLAG(lev,i,j,k,0) = 0; // reset for changeFlag usage
			initEmptyCell(lev, i,j,k, CFEmpty, -1.0, -1.0); 
		}
	}

	// init defaults
	mAvgNumUsedCells = 0;
	D::mFixMass= 0.0;

  /* init boundaries */
  debugOut("LbmFsgrSolver::initialize : Boundary init...",10);


	// use the density init?
	initGeometryFlags();
	D::initGenericTestCases();
	
	// new - init noslip 1 everywhere...
	// half fill boundary cells?

  for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
			initEmptyCell(mMaxRefine, i,0,k, CFBnd, 0.0, BND_FILL); 
			initEmptyCell(mMaxRefine, i,mLevel[mMaxRefine].lSizey-1,k, CFBnd, 0.0, BND_FILL); 
    }

	if(D::cDimension == 3) {
		// only for 3D
		for(int j=0;j<mLevel[mMaxRefine].lSizey;j++)
			for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
				initEmptyCell(mMaxRefine, i,j,0, CFBnd, 0.0, BND_FILL); 
				initEmptyCell(mMaxRefine, i,j,mLevel[mMaxRefine].lSizez-1, CFBnd, 0.0, BND_FILL); 
			}
	}

  for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int j=0;j<mLevel[mMaxRefine].lSizey;j++) {
			initEmptyCell(mMaxRefine, 0,j,k, CFBnd, 0.0, BND_FILL); 
			initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-1,j,k, CFBnd, 0.0, BND_FILL); 
			// DEBUG BORDER!
			//initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-2,j,k, CFBnd, 0.0, BND_FILL); 
    }

	// TEST!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11
  /*for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int j=0;j<mLevel[mMaxRefine].lSizey;j++) {
			initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-2,j,k, CFBnd, 0.0, BND_FILL); 
    }
  for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
			initEmptyCell(mMaxRefine, i,1,k, CFBnd, 0.0, BND_FILL); 
    }
	// */

	/*for(int ii=0; ii<(int)pow(2.0,mMaxRefine)-1; ii++) {
		errMsg("BNDTESTSYMM","set "<<mLevel[mMaxRefine].lSizex-2-ii );
		for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
			for(int j=0;j<mLevel[mMaxRefine].lSizey;j++) {
				initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-2-ii,j,k, CFBnd, 0.0, BND_FILL);  // SYMM!? 2D?
			}
		for(int j=0;j<mLevel[mMaxRefine].lSizey;j++)
			for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
				initEmptyCell(mMaxRefine, i,j,mLevel[mMaxRefine].lSizez-2-ii, CFBnd, 0.0, BND_FILL);   // SYMM!? 3D?
			}
	}
	// Symmetry tests */

	// prepare interface cells
	initFreeSurfaces();
	initStandingFluidGradient();

	// perform first step to init initial mass
	mInitialMass = 0.0;
	int inmCellCnt = 0;
	FSGR_FORIJK1(mMaxRefine) {
		if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFFluid) ) {
			LbmFloat fluidRho = QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, 0); 
			FORDF1 { fluidRho += QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, l); }
			mInitialMass += fluidRho;
			inmCellCnt ++;
		} else if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFInter) ) {
			mInitialMass += QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, dMass);
			inmCellCnt ++;
		}
	}
	mCurrentVolume = mCurrentMass = mInitialMass;

	ParamVec cspv = D::mpParam->calculateCellSize();
	if(D::cDimension==2) cspv[2] = 1.0;
	inmCellCnt = 1;
	double nrmMass = (double)mInitialMass / (double)(inmCellCnt) *cspv[0]*cspv[1]*cspv[2] * 1000.0;
	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Initial Mass:"<<mInitialMass<<" normalized:"<<nrmMass, 3);
	mInitialMass = 0.0; // reset, and use actual value after first step

	//mStartSymm = false;
#if ELBEEM_BLENDER!=1
	if((D::cDimension==2)&&(D::mSizex<200)) {
		if(!checkSymmetry("init")) {
			errMsg("LbmFsgrSolver::initialize","Unsymmetric init...");
		} else {
			errMsg("LbmFsgrSolver::initialize","Symmetric init!");
		}
	}
#endif // ELBEEM_BLENDER!=1
	

	// ----------------------------------------------------------------------
	// coarsen region
	myTime_t fsgrtstart = getTime(); 
	for(int lev=mMaxRefine-1; lev>=0; lev--) {
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Coarsening level "<<lev<<".",8);
		performRefinement(lev);
		performCoarsening(lev);
		coarseRestrictFromFine(lev);
		performRefinement(lev);
		performCoarsening(lev);
		coarseRestrictFromFine(lev);
		
		//while( performRefinement(lev) | performCoarsening(lev)){
			//coarseRestrictFromFine(lev);
			//debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Coarsening level "<<lev<<".",8);
		//}
	}
	D::markedClearList();
	myTime_t fsgrtend = getTime(); 
	if(!D::mSilent){ debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"FSGR init done ("<< ((fsgrtend-fsgrtstart)/(double)1000.0)<<"s), changes:"<<mNumFsgrChanges , 10 ); }
	mNumFsgrChanges = 0;

	for(int l=0; l<D::cDirNum; l++) { 
		LbmFloat area = 0.5 * 0.5 *0.5;
		if(D::cDimension==2) area = 0.5 * 0.5;

		if(D::dfVecX[l]!=0) area *= 0.5;
		if(D::dfVecY[l]!=0) area *= 0.5;
		if(D::dfVecZ[l]!=0) area *= 0.5;
		mFsgrCellArea[l] = area;
	} // l
	/*for(int lev=0; lev<mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		if( RFLAG(lev, i,j,k,mLevel[lev].setCurr) & CFFluid) {
			if( RFLAG(lev+1, i*2,j*2,k*2,mLevel[lev+1].setCurr) & CFGrFromCoarse) {
				LbmFloat totArea = mFsgrCellArea[0]; // for l=0
				for(int l=1; l<D::cDirNum; l++) { 
					int ni=(2*i)+D::dfVecX[l], nj=(2*j)+D::dfVecY[l], nk=(2*k)+D::dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, mLevel[lev+1].setCurr)&
							(CFGrFromCoarse|CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
							//(CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
							) { 
						//LbmFloat area = 0.25; if(D::dfVecX[l]!=0) area *= 0.5; if(D::dfVecY[l]!=0) area *= 0.5; if(D::dfVecZ[l]!=0) area *= 0.5;
						totArea += mFsgrCellArea[l];
					}
				} // l
				QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) = totArea;
			} else if( RFLAG(lev+1, i*2,j*2,k*2,mLevel[lev+1].setCurr) & CFEmpty) {
				QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) = 1.0;
			} else {
				QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) = 0.0;
			}
			errMsg("DFINI"," at l"<<lev<<" "<<PRINT_IJK<<" v:"<<QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) );
		}
	} } // */

	// now really done...
  debugOut("LbmFsgrSolver::initialize : Init done ...",10);
	D::mInitDone = 1;

	// make sure both sets are ok
	// copy from other to curr
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		RFLAG(lev, i,j,k,mLevel[lev].setOther) = RFLAG(lev, i,j,k,mLevel[lev].setCurr);
	} } // first copy flags */


	
	if(mOutputSurfacePreview) {
		if(D::cDimension==2) {
			errFatal("LbmFsgrSolver::init","No preview in 2D allowed!",SIMWORLD_INITERROR); return false;
		}

		//int previewSize = mOutputSurfacePreview;
		// same as normal one, but use reduced size
		mpPreviewSurface = new IsoSurface( D::mIsoValue, false );
		mpPreviewSurface->setMaterialName( mpPreviewSurface->getMaterialName() );
		mpPreviewSurface->setIsolevel( D::mIsoValue );
		// usually dont display for rendering
		mpPreviewSurface->setVisible( false );

		mpPreviewSurface->setStart( vec2G(isostart) );
		mpPreviewSurface->setEnd(   vec2G(isoend) );
		LbmVec pisodist = isoend-isostart;
		mpPreviewSurface->initializeIsosurface( (int)(mPreviewFactor*D::mSizex)+2, (int)(mPreviewFactor*D::mSizey)+2, (int)(mPreviewFactor*D::mSizez)+2, vec2G(pisodist) );
		//mpPreviewSurface->setName( D::getName() + "preview" );
		mpPreviewSurface->setName( "preview" );
	
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Preview with sizes "<<(mPreviewFactor*D::mSizex)<<","<<(mPreviewFactor*D::mSizey)<<","<<(mPreviewFactor*D::mSizez)<<" enabled",10);
	}

#if ELBEEM_BLENDER==1
	// make sure fill fracs are right for first surface generation
	stepMain();
#endif // ELBEEM_BLENDER==1

	// prepare once...
	prepareVisualization();
	// copy again for stats counting
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		RFLAG(lev, i,j,k,mLevel[lev].setOther) = RFLAG(lev, i,j,k,mLevel[lev].setCurr);
	} } // first copy flags */

	/*{ int lev=mMaxRefine;
		FSGR_FORIJK_BOUNDS(lev) { // COMPRT deb out
		debMsgDirect("\n x="<<PRINT_IJK);
		for(int l=0; l<D::cDfNum; l++) {
			debMsgDirect(" df="<< QCELL(lev, i,j,k,mLevel[lev].setCurr, l) );
		}
		debMsgDirect(" m="<< QCELL(lev, i,j,k,mLevel[lev].setCurr, dMass) );
		debMsgDirect(" f="<< QCELL(lev, i,j,k,mLevel[lev].setCurr, dFfrac) );
		debMsgDirect(" x="<< QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) );
	} } // COMPRT ON */
	return true;
}



/*****************************************************************************/
/*! perform geometry init (if switched on) */
/*****************************************************************************/
template<class D>
bool 
LbmFsgrSolver<D>::initGeometryFlags() {
	int level = mMaxRefine;
	myTime_t geotimestart = getTime(); 
	ntlGeometryObject *pObj;
	// getCellSize (due to forced cubes, use x values)
	ntlVec3Gfx dvec( (D::mvGeoEnd[0]-D::mvGeoStart[0])/ ((LbmFloat)D::mSizex*2.0));
	// real cell size from now on...
	dvec *= 2.0; 
	ntlVec3Gfx nodesize = ntlVec3Gfx(mLevel[level].nodeSize); //dvec*1.0;
	dvec = nodesize;
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Performing geometry init ("<< D::mGeoInitId <<") v"<<dvec,3);

	/* set interface cells */
	D::initGeoTree(D::mGeoInitId);
	ntlVec3Gfx maxIniVel = vec2G( D::mpParam->calculateLattVelocityFromRw( vec2P(D::getGeoMaxInitialVelocity()) ));
	D::mpParam->setSimulationMaxSpeed( norm(maxIniVel) + norm(mLevel[level].gravity) );
	LbmFloat allowMax = D::mpParam->getTadapMaxSpeed();  // maximum allowed velocity
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Maximum Velocity from geo init="<< maxIniVel <<", allowed Max="<<allowMax ,5);
	if(D::mpParam->getSimulationMaxSpeed() > allowMax) {
		// similar to adaptTimestep();
		LbmFloat nextmax = D::mpParam->getSimulationMaxSpeed();
		LbmFloat newdt = D::mpParam->getStepTime() * (allowMax / nextmax); // newtr
		debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Performing reparametrization, newdt="<< newdt<<" prevdt="<< D::mpParam->getStepTime() <<" ",5);
		D::mpParam->setDesiredStepTime( newdt );
		D::mpParam->calculateAllMissingValues( D::mSilent );
		maxIniVel = vec2G( D::mpParam->calculateLattVelocityFromRw( vec2P(D::getGeoMaxInitialVelocity()) ));
		debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"New maximum Velocity from geo init="<< maxIniVel,5);
	}
	recalculateObjectSpeeds();

	ntlVec3Gfx pos,iniPos; // position of current cell
	LbmFloat rhomass = 0.0;
	int savedNodes = 0;
	int OId = -1;
	gfxReal distance;

	// 2d display as rectangles
	if(D::cDimension==2) {
		dvec[2] = 0.0; 
		iniPos =(D::mvGeoStart + ntlVec3Gfx( 0.0, 0.0, (D::mvGeoEnd[2]-D::mvGeoStart[2])*0.5 ))-(dvec*0.0);
		//iniPos =(D::mvGeoStart + ntlVec3Gfx( 0.0 ))+dvec;
	} else {
		iniPos =(D::mvGeoStart + ntlVec3Gfx( 0.0 ))-(dvec*0.0);
		iniPos[2] = D::mvGeoStart[2] + dvec[2]*getForZMin1();
	}


	// first init boundary conditions
#define GETPOS(i,j,k) \
						ntlVec3Gfx( iniPos[0]+ dvec[0]*(gfxReal)(i), \
						iniPos[1]+ dvec[1]*(gfxReal)(j), \
						iniPos[2]+ dvec[2]*(gfxReal)(k) )
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int j=1;j<mLevel[level].lSizey-1;j++) {
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				CellFlagType ntype = CFInvalid;
				if(D::geoInitCheckPointInside( GETPOS(i,j,k) , FGI_ALLBOUNDS, OId, distance)) {
					pObj = (*D::mpGiObjects)[OId];
					switch( pObj->getGeoInitType() ){
					case FGI_MBNDINFLOW:  
						rhomass = 1.0;
						ntype = CFFluid|CFMbndInflow; 
						break;
					case FGI_MBNDOUTFLOW: 
						rhomass = 0.0;
						ntype = CFEmpty|CFMbndOutflow; 
						break;
					default:
						rhomass = BND_FILL;
						ntype = CFBnd; break;
					}
				}
				if(ntype != CFInvalid) {
					// initDefaultCell
					if((ntype == CFMbndInflow) || (ntype == CFMbndOutflow) ) {
						ntype |= (OId<<24);
					}

					initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
				}

				// walk along x until hit for following inits
				if(distance<=-1.0) { distance = 100.0; }
				if(distance>0.0) {
					gfxReal dcnt=dvec[0];
					while(( dcnt< distance )&&(i+1<mLevel[level].lSizex-1)) {
						dcnt += dvec[0]; i++;
						savedNodes++;
						if(ntype != CFInvalid) {
							// rhomass are still inited from above
							initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
						}
					}
				} 
				// */

			} 
		} 
	} // zmax


	// now init fluid layer
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int j=1;j<mLevel[level].lSizey-1;j++) {
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;

				CellFlagType ntype = CFInvalid;
				int inits = 0;
				if(D::geoInitCheckPointInside( GETPOS(i,j,k) , FGI_FLUID, OId, distance)) {
					ntype = CFFluid;
				}
				if(ntype != CFInvalid) {
					// initDefaultCell
					rhomass = 1.0;
					initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
					inits++;
				}

				// walk along x until hit for following inits
				if(distance<=-1.0) { distance = 100.0; }
				if(distance>0.0) {
					gfxReal dcnt=dvec[0];
					while((dcnt< distance )&&(i+1<mLevel[level].lSizex-1)) {
						dcnt += dvec[0]; i++;
						savedNodes++;
						if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;
						if(ntype != CFInvalid) {
							// rhomass are still inited from above
							initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
							inits++;
						}
					}
				} // distance>0
				
			} 
		} 
	} // zmax

	D::freeGeoTree();
	myTime_t geotimeend = getTime(); 
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Geometry init done ("<< ((geotimeend-geotimestart)/(double)1000.0)<<"s,"<<savedNodes<<") " , 10 ); 
	//errMsg(" SAVED "," "<<savedNodes<<" of "<<(mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez));
	return true;
}

/*****************************************************************************/
/* init part for all freesurface testcases */
template<class D>
void 
LbmFsgrSolver<D>::initFreeSurfaces() {
	double interfaceFill = 0.45;   // filling level of interface cells

	// set interface cells 
	FSGR_FORIJK1(mMaxRefine) {

		/*if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFEmpty )) {
			int initInter = 0;
			// check for neighboring fluid cells 
			FORDF1 {
				if( TESTFLAG( RFLAG_NBINV(mMaxRefine, i, j, k,  mLevel[mMaxRefine].setCurr,l), CFFluid ) ) {
					initInter = 1;
				}
			}

			if(initInter) {
				initEmptyCell(mMaxRefine, i,j,k, CFInter, 1.0, interfaceFill);
			}

		} // empty cells  OLD */

		if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFFluid )) {
			int initInter = 0; // check for neighboring empty cells 
			FORDF1 {
				if( TESTFLAG( RFLAG_NBINV(mMaxRefine, i, j, k,  mLevel[mMaxRefine].setCurr,l), CFEmpty ) ) {
					initInter = 1;
				}
			}
			if(initInter) {
				QCELL(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr, dMass) = 
					//QCELL(mMaxRefine,i,j,k,mLevel[mMaxRefine].setOther, dMass) =  // COMPRT OFF
					interfaceFill;
				RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) = RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setOther) = CFInter;
			}
		}
	}

	// remove invalid interface cells 
	FSGR_FORIJK1(mMaxRefine) {
		// remove invalid interface cells 
		if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFInter) ) {
			int delit = 0;
			int NBs = 0; // neighbor flags or'ed 
			int noEmptyNB = 1;

			FORDF1 {
				if( TESTFLAG( RFLAG_NBINV(mMaxRefine, i, j, k, mLevel[mMaxRefine].setCurr,l ), CFEmpty ) ) {
					noEmptyNB = 0;
				}
				NBs |= RFLAG_NBINV(mMaxRefine, i, j, k, mLevel[mMaxRefine].setCurr, l);
			}

			// remove cells with no fluid or interface neighbors
			if((NBs & CFFluid)==0) { delit = 1; }
			if((NBs & CFInter)==0) { delit = 1; }

			// remove cells with no empty neighbors
			if(noEmptyNB) { delit = 2; }

			// now we can remove the cell 
			if(delit==1) {
				initEmptyCell(mMaxRefine, i,j,k, CFEmpty, 1.0, 0.0);
			}
			if(delit==2) {
				initEmptyCell(mMaxRefine, i,j,k, CFFluid, 1.0, 1.0);
			}
		} // interface 
	}

	// another brute force init, make sure the fill values are right...
	// and make sure both sets are equal
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		if( (RFLAG(lev, i,j,k,0) & (CFBnd)) ) { 
			QCELL(lev, i,j,k,mLevel[mMaxRefine].setCurr, dFfrac) = BND_FILL;
			continue;
		}
		if( (RFLAG(lev, i,j,k,0) & (CFEmpty)) ) { 
			QCELL(lev, i,j,k,mLevel[mMaxRefine].setCurr, dFfrac) = 0.0;
			continue;
		}
	} }

	// ----------------------------------------------------------------------
	// smoother surface...
	if(mInitSurfaceSmoothing>0) {
		debMsgStd("Surface Smoothing init", DM_MSG, "Performing "<<(mInitSurfaceSmoothing)<<" smoothing steps ",10);
#if COMPRESSGRIDS==1
		errFatal("NYI","COMPRESSGRIDS mInitSurfaceSmoothing",SIMWORLD_INITERROR); return;
#endif // COMPRESSGRIDS==0
	}
	for(int s=0; s<mInitSurfaceSmoothing; s++) {
		FSGR_FORIJK1(mMaxRefine) {
			if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFInter) ) {
				LbmFloat mass = 0.0;
				//LbmFloat nbdiv;
				FORDF0 {
					int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
					if( RFLAG(mMaxRefine, ni,nj,nk, mLevel[mMaxRefine].setCurr) & CFFluid ){
						mass += 1.0;
					}
					if( RFLAG(mMaxRefine, ni,nj,nk, mLevel[mMaxRefine].setCurr) & CFInter ){
						mass += QCELL(mMaxRefine, ni,nj,nk, mLevel[mMaxRefine].setCurr, dMass);
					}
					//nbdiv+=1.0;
				}

				//errMsg(" I ", PRINT_IJK<<" m"<<mass );
				QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setOther, dMass) = (mass/19.0);
				QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setOther, dFfrac) = QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setOther, dMass);
			}
		}

		mLevel[mMaxRefine].setOther = mLevel[mMaxRefine].setCurr;
		mLevel[mMaxRefine].setCurr ^= 1;
	}
	// copy back...?

}

/*****************************************************************************/
/* init part for all freesurface testcases */
template<class D>
void 
LbmFsgrSolver<D>::initStandingFluidGradient() {
	// ----------------------------------------------------------------------
	// standing fluid preinit
	const int debugStandingPreinit = 0;
	int haveStandingFluid = 0;

#define STANDFLAGCHECK(iindex) \
				if( ( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFInter)) ) || \
						( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFEmpty)) ) ){  \
					if((iindex)>1) { haveStandingFluid=(iindex); } \
					j = mLevel[mMaxRefine].lSizey; i=mLevel[mMaxRefine].lSizex; k=D::getForZMaxBnd(); \
					continue; \
				} 
	int gravIndex[3] = {0,0,0};
	int gravDir[3] = {1,1,1};
	int maxGravComp = 1; // by default y
	int gravComp1 = 0; // by default y
	int gravComp2 = 2; // by default y
	if( ABS(mLevel[mMaxRefine].gravity[0]) > ABS(mLevel[mMaxRefine].gravity[1]) ){ maxGravComp = 0; gravComp1=1; gravComp2=2; }
	if( ABS(mLevel[mMaxRefine].gravity[2]) > ABS(mLevel[mMaxRefine].gravity[0]) ){ maxGravComp = 2; gravComp1=0; gravComp2=1; }

	int gravIMin[3] = { 0 , 0 , 0 };
	int gravIMax[3] = {
		mLevel[mMaxRefine].lSizex + 0,
		mLevel[mMaxRefine].lSizey + 0,
		mLevel[mMaxRefine].lSizez + 0 };
	if(LBMDIM==2) gravIMax[2] = 1;

	//int gravDir = 1;
	if( mLevel[mMaxRefine].gravity[maxGravComp] > 0.0 ) {
		// swap directions
		int i=maxGravComp;
		int tmp = gravIMin[i];
		gravIMin[i] = gravIMax[i] - 1;
		gravIMax[i] = tmp - 1;
		gravDir[i] = -1;
	}
#define PRINTGDIRS \
	errMsg("Standing fp","X start="<<gravIMin[0]<<" end="<<gravIMax[0]<<" dir="<<gravDir[0] ); \
	errMsg("Standing fp","Y start="<<gravIMin[1]<<" end="<<gravIMax[1]<<" dir="<<gravDir[1] ); \
	errMsg("Standing fp","Z start="<<gravIMin[2]<<" end="<<gravIMax[2]<<" dir="<<gravDir[2] ); 
	// _PRINTGDIRS;

	bool gravAbort = false;
#define GRAVLOOP \
	gravAbort=false; \
	for(gravIndex[2]= gravIMin[2];     (gravIndex[2]!=gravIMax[2])&&(!gravAbort);  gravIndex[2] += gravDir[2]) \
		for(gravIndex[1]= gravIMin[1];   (gravIndex[1]!=gravIMax[1])&&(!gravAbort);  gravIndex[1] += gravDir[1]) \
			for(gravIndex[0]= gravIMin[0]; (gravIndex[0]!=gravIMax[0])&&(!gravAbort);  gravIndex[0] += gravDir[0]) 

	GRAVLOOP {
		int i = gravIndex[0], j = gravIndex[1], k = gravIndex[2];
		//if((gravIndex[gravComp1]==gravIMin[gravComp1]) && (gravIndex[gravComp2]==gravIMin[gravComp2])) {debMsgStd("Standing fluid preinit", DM_MSG, "fluidheightinit check "<<PRINT_IJK<<" "<< haveStandingFluid, 1 ); }
		//STANDFLAGCHECK(gravIndex[maxGravComp]);
		if( ( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFInter)) ) || 
				( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFEmpty)) ) ){  
			int fluidHeight = (ABS(gravIndex[maxGravComp] - gravIMin[maxGravComp]));
			if(debugStandingPreinit) errMsg("Standing fp","fh="<<fluidHeight<<" gmax="<<gravIMax[maxGravComp]<<" gi="<<gravIndex[maxGravComp] );
			//if(gravIndex[maxGravComp]>1)  
			if(fluidHeight>1) 
			{
				haveStandingFluid = fluidHeight; //gravIndex[maxGravComp]; 
				gravIMax[maxGravComp] = gravIndex[maxGravComp] + gravDir[maxGravComp];
			}
			gravAbort = true; continue; 
		} 
	} // GRAVLOOP
	// _PRINTGDIRS;

	LbmFloat fluidHeight;
	//if(gravDir>0) { fluidHeight = (LbmFloat)haveStandingFluid;
	//} else { fluidHeight = (LbmFloat)haveStandingFluid; }
	fluidHeight = (LbmFloat)(ABS(gravIMax[maxGravComp]-gravIMin[maxGravComp]));
	if(debugStandingPreinit) debMsgStd("Standing fluid preinit", DM_MSG, "fheight="<<fluidHeight<<" min="<<PRINT_VEC(gravIMin[0],gravIMin[1],	gravIMin[2])<<" max="<<PRINT_VEC(gravIMax[0], gravIMax[1],gravIMax[2])<<
			" mgc="<<maxGravComp<<" mc1="<<gravComp1<<" mc2="<<gravComp2<<" dir="<<gravDir[maxGravComp]<<" have="<<haveStandingFluid ,10);
				
	if(mDisableStandingFluidInit) {
		debMsgStd("Standing fluid preinit", DM_MSG, "Should be performed - but skipped due to mDisableStandingFluidInit flag set!", 2);
		haveStandingFluid=0;
	}

	// copy flags and init , as no flags will be changed during grav init
	// also important for corasening later on
	const int lev = mMaxRefine;
	CellFlagType nbflag[LBM_DFNUM], nbored; 
	for(int k=D::getForZMinBnd();k<D::getForZMaxBnd();++k) {
		for(int j=0;j<mLevel[lev].lSizey-0;++j) {
			for(int i=0;i<mLevel[lev].lSizex-0;++i) {
				if( (RFLAG(lev, i,j,k,SRCS(lev)) & (CFFluid)) ) {
					nbored = 0;
					FORDF1 {
						nbflag[l] = RFLAG_NB(lev, i,j,k, SRCS(lev),l);
						nbored |= nbflag[l];
					} 
					if(nbored&CFBnd) {
						RFLAG(lev, i,j,k,SRCS(lev)) &= (~CFNoBndFluid);
					} else {
						RFLAG(lev, i,j,k,SRCS(lev)) |= CFNoBndFluid;
					}
				}
				RFLAG(lev, i,j,k,TSET(lev)) = RFLAG(lev, i,j,k,SRCS(lev));
	} } }

	if(haveStandingFluid) {
		int rhoworkSet = mLevel[lev].setCurr;
		myTime_t timestart = getTime(); // FIXME use user time here?
#if OPT3D==true 
		LbmFloat lcsmqadd, lcsmqo, lcsmeq[LBM_DFNUM], lcsmomega;
#endif // OPT3D==true 

		GRAVLOOP {
			int i = gravIndex[0], j = gravIndex[1], k = gravIndex[2];
			//debMsgStd("Standing fluid preinit", DM_MSG, " init check "<<PRINT_IJK<<" "<< haveStandingFluid, 1 );
			if( ( (RFLAG(lev, i,j,k,rhoworkSet) & (CFInter)) ) ||
					( (RFLAG(lev, i,j,k,rhoworkSet) & (CFEmpty)) ) ){ 
				//gravAbort = true; 
				continue;
			}

			LbmFloat rho = 1.0;
			// 1/6 velocity from denisty gradient, 1/2 for delta of two cells
			rho += 1.0* (fluidHeight-gravIndex[maxGravComp]) * 
				(mLevel[lev].gravity[maxGravComp])* (-3.0/1.0)*(mLevel[lev].omega); 
			if(debugStandingPreinit) 
				if((gravIndex[gravComp1]==gravIMin[gravComp1]) && (gravIndex[gravComp2]==gravIMin[gravComp2])) { 
					errMsg("Standing fp","gi="<<gravIndex[maxGravComp]<<" rho="<<rho<<" at "<<PRINT_IJK); 
				}

			if( (RFLAG(lev, i,j,k, rhoworkSet) & CFFluid) ||
					(RFLAG(lev, i,j,k, rhoworkSet) & CFInter) ) {
				FORDF0 { QCELL(lev, i,j,k, rhoworkSet, l) *= rho; }
				QCELL(lev, i,j,k, rhoworkSet, dMass) *= rho;
			}

		} // GRAVLOOP
		debMsgStd("Standing fluid preinit", DM_MSG, "Density gradient inited", 8);
		
		int preinitSteps = (haveStandingFluid* ((mLevel[lev].lSizey+mLevel[lev].lSizez+mLevel[lev].lSizex)/3) );
		preinitSteps = (haveStandingFluid>>2); // not much use...?
		//preinitSteps = 4; // DEBUG!!!!
		//D::mInitDone = 1; // GRAVTEST
		//preinitSteps = 0;
		debMsgNnl("Standing fluid preinit", DM_MSG, "Performing "<<preinitSteps<<" prerelaxations ",10);
		for(int s=0; s<preinitSteps; s++) {
			int workSet = SRCS(lev); //mLevel[lev].setCurr;
			int otherSet = TSET(lev); //mLevel[lev].setOther;
			debMsgDirect(".");
			if(debugStandingPreinit) debMsgStd("Standing fluid preinit", DM_MSG, "s="<<s<<" curset="<<workSet<<" srcs"<<SRCS(lev), 10);
			LbmFloat *ccel;
			LbmFloat *tcel;
			LbmFloat m[LBM_DFNUM];

		// grav loop not necessary here
#define NBFLAG(l) (nbflag[(l)])
		LbmFloat rho, ux,uy,uz, usqr; 
		int kstart=D::getForZMinBnd(), kend=D::getForZMaxBnd();
#if COMPRESSGRIDS==0
		for(int k=kstart;k<kend;++k) {
#else // COMPRESSGRIDS==0
		int kdir = 1; // COMPRT ON
		if(mLevel[lev].setCurr==1) {
			kdir = -1;
			int temp = kend;
			kend = kstart-1;
			kstart = temp-1;
		} // COMPRT
		for(int k=kstart;k!=kend;k+=kdir) {

		//errMsg("LbmFsgrSolver::mainLoop","k="<<k<<" ks="<<kstart<<" ke="<<kend<<" kdir="<<kdir ); // debug
#endif // COMPRESSGRIDS==0

		for(int j=0;j<mLevel[lev].lSizey-0;++j) {
		for(int i=0;i<mLevel[lev].lSizex-0;++i) {
				const CellFlagType currFlag = RFLAG(lev, i,j,k,workSet);
				if( (currFlag & (CFEmpty|CFBnd)) ) continue;
				ccel = RACPNT(lev, i,j,k,workSet); 
				tcel = RACPNT(lev, i,j,k,otherSet);

				if( (currFlag & (CFInter)) ) {
					// copy all values
					for(int l=0; l<dTotalNum;l++) { RAC(tcel,l) = RAC(ccel,l); }
					continue;
				}

				if( (currFlag & CFNoBndFluid)) {
					OPTIMIZED_STREAMCOLLIDE;
				} else {
					FORDF1 {
						nbflag[l] = RFLAG_NB(lev, i,j,k, SRCS(lev),l);
					} 
					DEFAULT_STREAM;
					ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; 
					DEFAULT_COLLIDE;
				}
				for(int l=LBM_DFNUM; l<dTotalNum;l++) { RAC(tcel,l) = RAC(ccel,l); }
			} } } // GRAVLOOP

			mLevel[lev].setOther = mLevel[lev].setCurr;
			mLevel[lev].setCurr ^= 1;
		}
		//D::mInitDone = 0; // GRAVTEST
		// */

		myTime_t timeend = getTime();
		debMsgDirect(" done, "<<((timeend-timestart)/(double)1000.0)<<"s \n");
#undef  NBFLAG
	}
}



/*****************************************************************************/
/* init a given cell with flag, density, mass and equilibrium dist. funcs */

template<class D>
void LbmFsgrSolver<D>::changeFlag(int level, int xx,int yy,int zz,int set,CellFlagType newflag) {
	CellFlagType pers = RFLAG(level,xx,yy,zz,set) & CFPersistMask;
	RFLAG(level,xx,yy,zz,set) = newflag | pers;
}

template<class D>
void 
LbmFsgrSolver<D>::initEmptyCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass) {
  /* init eq. dist funcs */
	LbmFloat *ecel;
	int workSet = mLevel[level].setCurr;

	ecel = RACPNT(level, i,j,k, workSet);
	FORDF0 { RAC(ecel, l) = D::dfEquil[l] * rho; }
	RAC(ecel, dMass) = mass;
	RAC(ecel, dFfrac) = mass/rho;
	RAC(ecel, dFlux) = FLUX_INIT;
	//RFLAG(level, i,j,k, workSet)= flag;
	changeFlag(level, i,j,k, workSet, flag);

  workSet ^= 1;
	changeFlag(level, i,j,k, workSet, flag);
	return;
}

template<class D>
void 
LbmFsgrSolver<D>::initVelocityCell(int level, int i,int j,int k, CellFlagType flag, LbmFloat rho, LbmFloat mass, LbmVec vel) {
	LbmFloat *ecel;
	int workSet = mLevel[level].setCurr;

	ecel = RACPNT(level, i,j,k, workSet);
	FORDF0 { RAC(ecel, l) = D::getCollideEq(l, rho,vel[0],vel[1],vel[2]); }
	RAC(ecel, dMass) = mass;
	RAC(ecel, dFfrac) = mass/rho;
	RAC(ecel, dFlux) = FLUX_INIT;
	//RFLAG(level, i,j,k, workSet) = flag;
	changeFlag(level, i,j,k, workSet, flag);

  workSet ^= 1;
	changeFlag(level, i,j,k, workSet, flag);
	return;
}

template<class D>
bool 
LbmFsgrSolver<D>::checkSymmetry(string idstring)
{
	bool erro = false;
	bool symm = true;
	int msgs = 0;
	const int maxMsgs = 10;
	const bool markCells = false;

	//for(int lev=0; lev<=mMaxRefine; lev++) {
	{ int lev = mMaxRefine;

	// no point if not symm.
	if( (mLevel[lev].lSizex==mLevel[lev].lSizey) && (mLevel[lev].lSizex==mLevel[lev].lSizez)) {
		// ok
	} else {
		return false;
	}

	for(int s=0; s<2; s++) {
	FSGR_FORIJK1(lev) {
		if(i<(mLevel[lev].lSizex/2)) {
			int inb = (mLevel[lev].lSizey-1-i); 

			if(lev==mMaxRefine) inb -= 1;		// FSGR_SYMM_T

			if( RFLAG(lev, i,j,k,s) != RFLAG(lev, inb,j,k,s) ) { erro = true;
				if(D::cDimension==2) {
					if(msgs<maxMsgs) { msgs++;
						errMsg("EFLAG", PRINT_IJK<<"s"<<s<<" flag "<<RFLAG(lev, i,j,k,s)<<" , at "<<PRINT_VEC(inb,j,k)<<"s"<<s<<" flag "<<RFLAG(lev, inb,j,k,s) );
					}
				}
				if(markCells){ debugMarkCell(lev, i,j,k); debugMarkCell(lev, inb,j,k); }
				symm = false;
			}
			if( LBM_FLOATNEQ(QCELL(lev, i,j,k,s, dMass), QCELL(lev, inb,j,k,s, dMass)) ) { erro = true;
				if(D::cDimension==2) {
					if(msgs<maxMsgs) { msgs++;
						//debMsgDirect(" mass1 "<<QCELL(lev, i,j,k,s, dMass)<<" mass2 "<<QCELL(lev, inb,j,k,s, dMass) <<std::endl);
						errMsg("EMASS", PRINT_IJK<<"s"<<s<<" mass "<<QCELL(lev, i,j,k,s, dMass)<<" , at "<<PRINT_VEC(inb,j,k)<<"s"<<s<<" mass "<<QCELL(lev, inb,j,k,s, dMass) );
					}
				}
				if(markCells){ debugMarkCell(lev, i,j,k); debugMarkCell(lev, inb,j,k); }
				symm = false;
			}

			LbmFloat nbrho = QCELL(lev, i,j,k, s, dC);
 			FORDF1 { nbrho += QCELL(lev, i,j,k, s, l); }
			LbmFloat otrho = QCELL(lev, inb,j,k, s, dC);
 			FORDF1 { otrho += QCELL(lev, inb,j,k, s, l); }
			if( LBM_FLOATNEQ(nbrho, otrho) ) { erro = true;
				if(D::cDimension==2) {
					if(msgs<maxMsgs) { msgs++;
						//debMsgDirect(" rho 1 "<<nbrho <<" rho 2 "<<otrho  <<std::endl);
						errMsg("ERHO ", PRINT_IJK<<"s"<<s<<" rho  "<<nbrho <<" , at "<<PRINT_VEC(inb,j,k)<<"s"<<s<<" rho  "<<otrho  );
					}
				}
				if(markCells){ debugMarkCell(lev, i,j,k); debugMarkCell(lev, inb,j,k); }
				symm = false;
			}
		}
	} }
	} // lev
	LbmFloat maxdiv =0.0;
	if(erro) {
		errMsg("SymCheck Failed!", idstring<<" rho maxdiv:"<< maxdiv );
		//if(D::cDimension==2) D::mPanic = true; 
		//return false;
	} else {
		errMsg("SymCheck OK!", idstring<<" rho maxdiv:"<< maxdiv );
	}
	// all ok...
	return symm;
}// */


/*****************************************************************************/
/*! debug object display */
/*****************************************************************************/
template<class D>
vector<ntlGeometryObject*> LbmFsgrSolver<D>::getDebugObjects() { 
	vector<ntlGeometryObject*> debo; 
	if(mOutputSurfacePreview) {
		debo.push_back( mpPreviewSurface );
	}
	return debo; 
}

/*****************************************************************************/
/*! perform a single LBM step */
/*****************************************************************************/

template<class D>
void 
LbmFsgrSolver<D>::stepMain()
{
#if ELBEEM_BLENDER==1
		// update gui display
		SDL_mutexP(globalBakeLock);
		if(globalBakeState<0) {
			// this means abort... cause panic
			D::mPanic = 1;
			errMsg("LbmFsgrSolver::step","Got abort signal from GUI, causing panic, aborting...");
		}
		SDL_mutexV(globalBakeLock);
#endif // ELBEEM_BLENDER==1
	D::markedClearList(); // DMC clearMarkedCellsList
	if(mDropping) {
		initDrop(mDropX, mDropY);
	}
	if(mGfxGeoSetup==6) {
		// xobs init hack
		if(mSimulationTime<0.400) {
			if((mSimulationTime>0.25) && (mSimulationTime<0.325)) {
				// stop shortly...
				mDropping = false;
			} else {
				initDrop(0.0, 1.0);
			}
		} else {
			mDropping=false;
		}
	}

	// safety check, counter reset
	D::mNumUsedCells = 0;
	mNumInterdCells = 0;
	mNumInvIfCells = 0;

  //debugOutNnl("LbmFsgrSolver::step : "<<D::mStepCnt, 10);
  if(!D::mSilent){ debMsgNnl("LbmFsgrSolver::step", DM_MSG, D::mName<<" cnt:"<<D::mStepCnt<<"  ", 10); }
	//debMsgDirect(  "LbmFsgrSolver::step : "<<D::mStepCnt<<" ");
	myTime_t timestart = getTime();
	//myTime_t timestart = 0;
	//if(mStartSymm) { checkSymmetry("step1"); } // DEBUG 

	// important - keep for tadap
	mCurrentMass = D::mFixMass; // reset here for next step
	mCurrentVolume = 0.0;
	
	//stats
	mMaxVlen = mMxvz = mMxvy = mMxvx = 0.0;

	//change to single step advance!
	int levsteps = 0;
	int dsbits = D::mStepCnt ^ (D::mStepCnt-1);
	//errMsg("S"," step:"<<D::mStepCnt<<" s-1:"<<(D::mStepCnt-1)<<" xf:"<<convertCellFlagType2String(dsbits));
	for(int lev=0; lev<=mMaxRefine; lev++) {
		//if(! (D::mStepCnt&(1<<lev)) ) {
		if( dsbits & (1<<(mMaxRefine-lev)) ) {
			//errMsg("S"," l"<<lev);

			if(lev==mMaxRefine) {
				// always advance fine level...
				fineAdvance();
				//performRefinement(lev-1); // TEST here?
			} else {
				performRefinement(lev); // TEST here?
				performCoarsening(lev); // TEST here?
				coarseRestrictFromFine(lev);
				coarseAdvance(lev);
				//performRefinement(lev-1); // TEST here?
			}
#if FSGR_OMEGA_DEBUG==1
			errMsg("LbmFsgrSolver::step","LES stats l="<<lev<<" omega="<<mLevel[lev].omega<<" avgOmega="<< (mLevel[lev].avgOmega/mLevel[lev].avgOmegaCnt) );
			mLevel[lev].avgOmega = 0.0; mLevel[lev].avgOmegaCnt = 0.0;
#endif // FSGR_OMEGA_DEBUG==1

			LbmFloat interTime = -10.0;
#if TIMEINTORDER==1
			interTime = 0.5;
			if( D::mStepCnt & (1<<(mMaxRefine-lev)) ) interTime = 0.0;
			// TEST influence... interTime = 0.0;
#elif TIMEINTORDER==2
			interTime = 1.0;
#else // TIMEINTORDER==0
			interTime = 0.0;
#endif // TIMEINTORDER==1
			levsteps++;
		}
		mCurrentMass   += mLevel[lev].lmass;
		mCurrentVolume += mLevel[lev].lvolume;
	}

  // prepare next step
	D::mStepCnt++;


	// some dbugging output follows
	// calculate MLSUPS
	myTime_t timeend = getTime();

	D::mNumUsedCells += mNumInterdCells; // count both types for MLSUPS
	mAvgNumUsedCells += D::mNumUsedCells;
	D::mMLSUPS = (D::mNumUsedCells / ((timeend-timestart)/(double)1000.0) ) / (1000000);
	if(D::mMLSUPS>10000){ D::mMLSUPS = -1; }
	else { mAvgMLSUPS += D::mMLSUPS; mAvgMLSUPSCnt += 1.0; } // track average mlsups
	
	LbmFloat totMLSUPS = ( ((mLevel[mMaxRefine].lSizex-2)*(mLevel[mMaxRefine].lSizey-2)*(getForZMax1(mMaxRefine)-getForZMin1())) / ((timeend-timestart)/(double)1000.0) ) / (1000000);
	if(totMLSUPS>10000) totMLSUPS = -1;
	mNumInvIfTotal += mNumInvIfCells; // debug

  // do some formatting 
  if(!D::mSilent){ 
		string sepStr(""); // DEBUG
		debMsgDirect( 
			"mlsups(curr:"<<D::mMLSUPS<<
			" avg:"<<(mAvgMLSUPS/mAvgMLSUPSCnt)<<"), "<< sepStr<<
			" totcls:"<<(D::mNumUsedCells)<< sepStr<<
			" avgcls:"<< (int)(mAvgNumUsedCells/(long long int)D::mStepCnt)<< sepStr<<
			" intd:"<<mNumInterdCells<< sepStr<<
			" invif:"<<mNumInvIfCells<< sepStr<<
			" invift:"<<mNumInvIfTotal<< sepStr<<
			" fsgrcs:"<<mNumFsgrChanges<< sepStr<<
			" filled:"<<D::mNumFilledCells<<", emptied:"<<D::mNumEmptiedCells<< sepStr<<
			" mMxv:"<<mMxvx<<","<<mMxvy<<","<<mMxvz<<", tscnts:"<<mTimeSwitchCounts<< sepStr<<
			/*" rhoMax:"<<mRhoMax<<", rhoMin:"<<mRhoMin<<", vlenMax:"<<mMaxVlen<<", "*/
			" probs:"<<mNumProblems<< sepStr<<
			" simt:"<<mSimulationTime<< sepStr<<
			" for '"<<D::mName<<"' " );

		//wrong?
		//debMsgDirect(", dccd:"<< mCurrentMass<<"/"<<mCurrentVolume<<"(fix:"<<D::mFixMass<<",ini:"<<mInitialMass<<") ");
		debMsgDirect(std::endl);
		debMsgDirect(D::mStepCnt<<": dccd="<< mCurrentMass<<"/"<<mCurrentVolume<<"(fix="<<D::mFixMass<<",ini="<<mInitialMass<<") ");
		debMsgDirect(std::endl);

		// nicer output
		debMsgDirect(std::endl); // 
		//debMsgStd(" ",DM_MSG," ",10);
	} else {
		debMsgDirect(".");
		//if((mStepCnt%10)==9) debMsgDirect("\n");
	}

	if(D::mStepCnt==1) {
		mMinNoCells = mMaxNoCells = D::mNumUsedCells;
	} else {
		if(D::mNumUsedCells>mMaxNoCells) mMaxNoCells = D::mNumUsedCells;
		if(D::mNumUsedCells<mMinNoCells) mMinNoCells = D::mNumUsedCells;
	}
	
	// mass scale test
	if((mMaxRefine>0)&&(mInitialMass>0.0)) {
		LbmFloat mscale = mInitialMass/mCurrentMass;

		mscale = 1.0;
		const LbmFloat dchh = 0.001;
		if(mCurrentMass<mInitialMass) mscale = 1.0+dchh;
		if(mCurrentMass>mInitialMass) mscale = 1.0-dchh;

		// use mass rescaling?
		// with float precision this seems to be nonsense...
		const bool MREnable = false;

		const int MSInter = 2;
		static int mscount = 0;
		if( (MREnable) && ((mLevel[0].lsteps%MSInter)== (MSInter-1)) && ( ABS( (mInitialMass/mCurrentMass)-1.0 ) > 0.01) && ( dsbits & (1<<(mMaxRefine-0)) ) ){
			// example: FORCE RESCALE MASS! ini:1843.5, cur:1817.6, f=1.01425 step:22153 levstep:5539 msc:37
			// mass rescale MASS RESCALE check
			errMsg("MDTDD","\n\n");
			errMsg("MDTDD","FORCE RESCALE MASS! "
					<<"ini:"<<mInitialMass<<", cur:"<<mCurrentMass<<", f="<<ABS(mInitialMass/mCurrentMass)
					<<" step:"<<D::mStepCnt<<" levstep:"<<mLevel[0].lsteps<<" msc:"<<mscount<<" "
					);
			errMsg("MDTDD","\n\n");

			mscount++;
			for(int lev=mMaxRefine; lev>=0 ; lev--) {
				//for(int workSet = 0; workSet<=1; workSet++) {
				int wss = 0;
				int wse = 1;
#if COMPRESSGRIDS==1
				if(lev== mMaxRefine) wss = wse = mLevel[lev].setCurr;
#endif // COMPRESSGRIDS==1
				for(int workSet = wss; workSet<=wse; workSet++) { // COMPRT

					FSGR_FORIJK1(lev) {
						if( (RFLAG(lev,i,j,k, workSet) & (CFFluid| CFInter| CFGrFromCoarse| CFGrFromFine| CFGrNorm)) 
							) {

							FORDF0 { QCELL(lev, i,j,k,workSet, l) *= mscale; }
							QCELL(lev, i,j,k,workSet, dMass) *= mscale;
							QCELL(lev, i,j,k,workSet, dFfrac) *= mscale;

						} else {
							continue;
						}
					}
				}
				mLevel[lev].lmass *= mscale;
			}
		} 

		mCurrentMass *= mscale;
	}// if mass scale test */
	else {
		// use current mass after full step for initial setting
		if((mMaxRefine>0)&&(mInitialMass<=0.0) && (levsteps == (mMaxRefine+1))) {
			mInitialMass = mCurrentMass;
			debMsgStd("MDTDD",DM_NOTIFY,"Second Initial Mass Init: "<<mInitialMass, 2);
		}
	}

	// one of the last things to do - adapt timestep
	// was in fineAdvance before... 
	if(mTimeAdap) {
		adaptTimestep();
	} // time adaptivity

	// debug - raw dump of ffrac values
	/*if((mStepCnt%200)==1){
		std::ostringstream name;
		name <<"fill_" << mStepCnt <<".dump";
		FILE *file = fopen(name.str().c_str(),"w");
		for(int k= getForZMinBnd(mMaxRefine); k< getForZMaxBnd(mMaxRefine); ++k) 
		 for(int j=0;j<mLevel[mMaxRefine].lSizey-0;j++) 
			for(int i=0;i<mLevel[mMaxRefine].lSizex-0;i++) {
				float val = QCELL(mMaxRefine,i,j,k, mLevel[mMaxRefine].setCurr,dFfrac);
				fwrite( &val, sizeof(val), 1, file);
				//errMsg("W", PRINT_IJK<<" val:"<<val);
			}
		fclose(file);
	} // */

	/*
	if(1) { // DEBUG
		const int lev = mMaxRefine;
		int workSet = mLevel[lev].setCurr;
		FSGR_FORIJK1(lev) {
			if( 
					(RFLAG(lev,i,j,k, workSet) & CFFluid) || 
					(RFLAG(lev,i,j,k, workSet) & CFInter) ||
					(RFLAG(lev,i,j,k, workSet) & CFGrFromCoarse) || 
					(RFLAG(lev,i,j,k, workSet) & CFGrFromFine) || 
					(RFLAG(lev,i,j,k, workSet) & CFGrNorm) 
					) {
				// these cells have to be scaled...
			} else {
				continue;
			}

			// collide on current set
			LbmFloat rho, ux,uy,uz;
			rho=0.0; ux =  uy = uz = 0.0;
			for(int l=0; l<D::cDfNum; l++) {
				LbmFloat m = QCELL(lev, i, j, k, workSet, l); 
				rho += m;
				ux  += (D::dfDvecX[l]*m);
				uy  += (D::dfDvecY[l]*m); 
				uz  += (D::dfDvecZ[l]*m); 
			} 
			//errMsg("DEBUG"," "<<PRINT_IJK <<" rho="<<rho<<" vel="<<PRINT_VEC(ux,uy,uz) );
			f__printf(stdout,"D %d,%d rho=%+'.5f vel=[%+'.5f,%+'.5f] \n", i,j, rho, ux,uy );
		}
	}       // DEBUG */

}

template<class D>
void 
LbmFsgrSolver<D>::fineAdvance()
{
	// do the real thing...
	mainLoop( mMaxRefine );
	if(mUpdateFVHeight) {
		// warning assume -Y gravity...
		mFVHeight = mCurrentMass*mFVArea/((LbmFloat)(mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizez));
		if(mFVHeight<1.0) mFVHeight = 1.0;
		D::mpParam->setFluidVolumeHeight(mFVHeight);
	}

	// advance time before timestep change
	mSimulationTime += D::mpParam->getStepTime();
	// time adaptivity
	D::mpParam->setSimulationMaxSpeed( sqrt(mMaxVlen / 1.5) );
	//if(mStartSymm) { checkSymmetry("step2"); } // DEBUG 
	if(!D::mSilent){ errMsg("fineAdvance"," stepped from "<<mLevel[mMaxRefine].setCurr<<" to "<<mLevel[mMaxRefine].setOther<<" step"<< (mLevel[mMaxRefine].lsteps) ); }

	// update other set
  mLevel[mMaxRefine].setOther   = mLevel[mMaxRefine].setCurr;
  mLevel[mMaxRefine].setCurr   ^= 1;
  mLevel[mMaxRefine].lsteps++;

	// flag init... (work on current set, to simplify flag checks)
	reinitFlags( mLevel[mMaxRefine].setCurr );
	//if((D::cDimension==2)&&(mStartSymm)) { checkSymmetry("step3"); } // DEBUG 
	if(!D::mSilent){ errMsg("fineAdvance"," flags reinit on set "<< mLevel[mMaxRefine].setCurr ); }
}


/*****************************************************************************/
//! coarse/fine step functions
/*****************************************************************************/

// access to own dfs during step (may be changed to local array)
#define MYDF(l) RAC(ccel, l)

template<class D>
void 
LbmFsgrSolver<D>::mainLoop(int lev)
{
	// loops over _only inner_ cells  -----------------------------------------------------------------------------------
	LbmFloat calcCurrentMass = 0.0; //mCurrentMass;
	LbmFloat calcCurrentVolume = 0.0; //mCurrentVolume;
	int      calcCellsFilled = D::mNumFilledCells;
	int      calcCellsEmptied = D::mNumEmptiedCells;
	int      calcNumUsedCells = D::mNumUsedCells;

#if PARALLEL==1
#include "paraloop.h"
#else // PARALLEL==1
  { // main loop region
	int kstart=D::getForZMin1(), kend=D::getForZMax1();
#define PERFORM_USQRMAXCHECK USQRMAXCHECK(usqr,ux,uy,uz, mMaxVlen, mMxvx,mMxvy,mMxvz);
#endif // PARALLEL==1


	// local to loop
	CellFlagType nbflag[LBM_DFNUM]; 
#define NBFLAG(l) nbflag[(l)]
	// */
	
	LbmFloat *ccel = NULL;
	LbmFloat *tcel = NULL;
	int oldFlag;
	int newFlag;
	int nbored;
	LbmFloat m[LBM_DFNUM];
	LbmFloat rho, ux, uy, uz, tmp, usqr;
	LbmFloat mass, change;
	usqr = tmp = 0.0; 
#if OPT3D==true 
	LbmFloat lcsmqadd, lcsmqo, lcsmeq[LBM_DFNUM], lcsmomega;
#endif // OPT3D==true 

	
	// ifempty cell conversion flags
	bool iffilled, ifemptied;
	int recons[LBM_DFNUM];   // reconstruct this DF?
	int numRecons;           // how many are reconstructed?

	
	CellFlagType *pFlagSrc;
	CellFlagType *pFlagDst;
	pFlagSrc = &RFLAG(lev, 0,1, kstart,SRCS(lev)); // omp
	pFlagDst = &RFLAG(lev, 0,1, kstart,TSET(lev)); // omp
	ccel = RACPNT(lev, 0,1, kstart ,SRCS(lev)); // omp
	tcel = RACPNT(lev, 0,1, kstart ,TSET(lev)); // omp
	//CellFlagType *pFlagTar = NULL;
	int pFlagTarOff;
	if(mLevel[lev].setOther==1) pFlagTarOff = mLevel[lev].lOffsz;
	else pFlagTarOff = -mLevel[lev].lOffsz;
#define ADVANCE_POINTERS(p)	\
	ccel += (QCELLSTEP*(p));	\
	tcel += (QCELLSTEP*(p));	\
	pFlagSrc+= (p); \
	pFlagDst+= (p); \
	i+= (p);

	// nutshell outflow HACK
	if(mGfxGeoSetup==2) {
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
	{const int j=1;
  for(int i=1;i<mLevel[mMaxRefine].lSizex-1;++i) {
		if(RFLAG(lev, i,j,k,SRCS(lev)) & CFFluid) {
			RFLAG(lev, i,j,k,SRCS(lev)) = CFInter;
			QCELL(lev, i,j,k,SRCS(lev), dMass) = 0.1;
			QCELL(lev, i,j,k,SRCS(lev), dFfrac) = 0.1;
		}
		else if(RFLAG(lev, i,j,k,SRCS(lev)) & CFInter) {
			QCELL(lev, i,j,k,SRCS(lev), dMass) = 0.1;
			QCELL(lev, i,j,k,SRCS(lev), dFfrac) = 0.1;
		}
	} } } }
	


	// ---
	// now stream etc.

	// use template functions for 2D/3D
#if COMPRESSGRIDS==0
  for(int k=kstart;k<kend;++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=0;i<mLevel[lev].lSizex-2;   ) {
#else // COMPRESSGRIDS==0
	int kdir = 1; // COMPRT ON
	if(mLevel[mMaxRefine].setCurr==1) {
		kdir = -1;
		int temp = kend;
		kend = kstart-1;
		kstart = temp-1;
	} // COMPRT

#if PARALLEL==0
  const int id = 0, Nthrds = 1;
#endif // PARALLEL==1
  const int Nj = mLevel[mMaxRefine].lSizey;
	int jstart = 0+( id * (Nj / Nthrds) );
	int jend   = 0+( (id+1) * (Nj / Nthrds) );
  if( ((Nj/Nthrds) *Nthrds) != Nj) {
    errMsg("LbmFsgrSolver","Invalid domain size Nj="<<Nj<<" Nthrds="<<Nthrds);
  }
	// cutoff obstacle boundary
	if(jstart<1) jstart = 1;
	if(jend>mLevel[mMaxRefine].lSizey-1) jend = mLevel[mMaxRefine].lSizey-1;

#if PARALLEL==1
	errMsg("LbmFsgrSolver::mainLoop","id="<<id<<" js="<<jstart<<" je="<<jend<<" jdir="<<(1) ); // debug
#endif // PARALLEL==1
  for(int k=kstart;k!=kend;k+=kdir) {

	//errMsg("LbmFsgrSolver::mainLoop","k="<<k<<" ks="<<kstart<<" ke="<<kend<<" kdir="<<kdir<<" x*y="<<mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*dTotalNum ); // debug
  pFlagSrc = &RFLAG(lev, 0, jstart, k, SRCS(lev)); // omp test // COMPRT ON
  pFlagDst = &RFLAG(lev, 0, jstart, k, TSET(lev)); // omp test
  ccel = RACPNT(lev,     0, jstart, k, SRCS(lev)); // omp test
  tcel = RACPNT(lev,     0, jstart, k, TSET(lev)); // omp test // COMPRT ON

  //for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int j=jstart;j!=jend;++j) {
  for(int i=0;i<mLevel[lev].lSizex-2;   ) {
#endif // COMPRESSGRIDS==0

		ADVANCE_POINTERS(1); 
#if FSGR_STRICT_DEBUG==1
		rho = ux = uy = uz = tmp = usqr = -100.0; // DEBUG
		if( (&RFLAG(lev, i,j,k,mLevel[lev].setCurr) != pFlagSrc) || 
		    (&RFLAG(lev, i,j,k,mLevel[lev].setOther) != pFlagDst) ) {
			errMsg("LbmFsgrSolver::mainLoop","Err flagp "<<PRINT_IJK<<"="<<
					RFLAG(lev, i,j,k,mLevel[lev].setCurr)<<","<<RFLAG(lev, i,j,k,mLevel[lev].setOther)<<" but is "<<
					(*pFlagSrc)<<","<<(*pFlagDst) <<",  pointers "<<
          (int)(&RFLAG(lev, i,j,k,mLevel[lev].setCurr))<<","<<(int)(&RFLAG(lev, i,j,k,mLevel[lev].setOther))<<" but is "<<
          (int)(pFlagSrc)<<","<<(int)(pFlagDst)<<" "
					); 
			D::mPanic=1;
		}	
		if( (&QCELL(lev, i,j,k,mLevel[lev].setCurr,0) != ccel) || 
		    (&QCELL(lev, i,j,k,mLevel[lev].setOther,0) != tcel) ) {
			errMsg("LbmFsgrSolver::mainLoop","Err cellp "<<PRINT_IJK<<"="<<
          (int)(&QCELL(lev, i,j,k,mLevel[lev].setCurr,0))<<","<<(int)(&QCELL(lev, i,j,k,mLevel[lev].setOther,0))<<" but is "<<
          (int)(ccel)<<","<<(int)(tcel)<<" "
					); 
			D::mPanic=1;
		}	
#endif
		oldFlag = *pFlagSrc;
		// stream from current set to other, then collide and store
		
		// old INTCFCOARSETEST==1
		if( (oldFlag & (CFGrFromCoarse)) ) {  // interpolateFineFromCoarse test!
			if(( D::mStepCnt & (1<<(mMaxRefine-lev)) ) ==1) {
				FORDF0 { RAC(tcel,l) = RAC(ccel,l); }
			} else {
				interpolateCellFromCoarse( lev, i,j,k, TSET(lev), 0.0, CFFluid|CFGrFromCoarse, false);
				calcNumUsedCells++;
			}
			continue; // interpolateFineFromCoarse test!
		} // interpolateFineFromCoarse test!  old INTCFCOARSETEST==1
	
		if(oldFlag & (CFMbndInflow)) {
			// fluid & if are ok, fill if later on
			int isValid = oldFlag & (CFFluid|CFInter);
			const LbmFloat iniRho = 1.0;
			const int OId = oldFlag>>24;
			if(!isValid) {
				// make new if cell
				const LbmVec vel(mObjectSpeeds[OId]);
				// TODO add OPT3D treatment
				FORDF0 { RAC(tcel, l) = D::getCollideEq(l, iniRho,vel[0],vel[1],vel[2]); }
				RAC(tcel, dMass) = RAC(tcel, dFfrac) = iniRho;
				RAC(tcel, dFlux) = FLUX_INIT;
				changeFlag(lev, i,j,k, TSET(lev), CFInter);
				calcCurrentMass += iniRho; calcCurrentVolume += 1.0; calcNumUsedCells++;
				mInitialMass += iniRho;
				// dont treat cell until next step
				continue;
			} 
		} 
		else  // these are exclusive
		if(oldFlag & (CFMbndOutflow)) {
			//errMsg("OUTFLOW"," ar "<<PRINT_IJK );
			int isnotValid = oldFlag & (CFFluid);
			if(isnotValid) {
				// remove fluid cells, shouldnt be here anyway
				//const int OId = oldFlag>>24;
				LbmFloat fluidRho = m[0]; FORDF1 { fluidRho += m[l]; }
				mInitialMass -= fluidRho;
				const LbmFloat iniRho = 0.0;
				RAC(tcel, dMass) = RAC(tcel, dFfrac) = iniRho;
				RAC(tcel, dFlux) = FLUX_INIT;
				changeFlag(lev, i,j,k, TSET(lev), CFInter);

				// same as ifemptied for if below
				LbmPoint emptyp;
				emptyp.x = i; emptyp.y = j; emptyp.z = k;
#if PARALLEL==1
				calcListEmpty[id].push_back( emptyp );
#else // PARALLEL==1
				mListEmpty.push_back( emptyp );
#endif // PARALLEL==1
				calcCellsEmptied++;
				continue;
			}
		}

		if(oldFlag & (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)) { 
			*pFlagDst = oldFlag;
			//RAC(tcel,dFfrac) = 0.0;
			//RAC(tcel,dFlux) = FLUX_INIT; // necessary?
			continue;
		}
		/*if( oldFlag & CFNoBndFluid ) {  // TEST ME FASTER?
			OPTIMIZED_STREAMCOLLIDE; PERFORM_USQRMAXCHECK;
			RAC(tcel,dFfrac) = 1.0; 
			*pFlagDst = (CellFlagType)oldFlag; // newFlag;
			calcCurrentMass += rho; calcCurrentVolume += 1.0;
			calcNumUsedCells++;
			continue;
		}// TEST ME FASTER? */

		// only neighbor flags! not own flag
		nbored = 0;
		
#if OPT3D==false
		FORDF1 {
			nbflag[l] = RFLAG_NB(lev, i,j,k,SRCS(lev),l);
			nbored |= nbflag[l];
		} 
#else
		nbflag[dSB] = *(pFlagSrc + (-mLevel[lev].lOffsy+-mLevel[lev].lOffsx)); nbored |= nbflag[dSB];
		nbflag[dWB] = *(pFlagSrc + (-mLevel[lev].lOffsy+-1)); nbored |= nbflag[dWB];
		nbflag[ dB] = *(pFlagSrc + (-mLevel[lev].lOffsy)); nbored |= nbflag[dB];
		nbflag[dEB] = *(pFlagSrc + (-mLevel[lev].lOffsy+ 1)); nbored |= nbflag[dEB];
		nbflag[dNB] = *(pFlagSrc + (-mLevel[lev].lOffsy+ mLevel[lev].lOffsx)); nbored |= nbflag[dNB];

		nbflag[dSW] = *(pFlagSrc + (-mLevel[lev].lOffsx+-1)); nbored |= nbflag[dSW];
		nbflag[ dS] = *(pFlagSrc + (-mLevel[lev].lOffsx)); nbored |= nbflag[dS];
		nbflag[dSE] = *(pFlagSrc + (-mLevel[lev].lOffsx+ 1)); nbored |= nbflag[dSE];

		nbflag[ dW] = *(pFlagSrc + (-1)); nbored |= nbflag[dW];
		nbflag[ dE] = *(pFlagSrc + ( 1)); nbored |= nbflag[dE];

		nbflag[dNW] = *(pFlagSrc + ( mLevel[lev].lOffsx+-1)); nbored |= nbflag[dNW];
	  nbflag[ dN] = *(pFlagSrc + ( mLevel[lev].lOffsx)); nbored |= nbflag[dN];
		nbflag[dNE] = *(pFlagSrc + ( mLevel[lev].lOffsx+ 1)); nbored |= nbflag[dNE];

		nbflag[dST] = *(pFlagSrc + ( mLevel[lev].lOffsy+-mLevel[lev].lOffsx)); nbored |= nbflag[dST];
		nbflag[dWT] = *(pFlagSrc + ( mLevel[lev].lOffsy+-1)); nbored |= nbflag[dWT];
		nbflag[ dT] = *(pFlagSrc + ( mLevel[lev].lOffsy)); nbored |= nbflag[dT];
		nbflag[dET] = *(pFlagSrc + ( mLevel[lev].lOffsy+ 1)); nbored |= nbflag[dET];
		nbflag[dNT] = *(pFlagSrc + ( mLevel[lev].lOffsy+ mLevel[lev].lOffsx)); nbored |= nbflag[dNT];
		// */
#endif

		// pointer to destination cell
		calcNumUsedCells++;

		// FLUID cells 
		if( oldFlag & CFFluid ) { 
			// only standard fluid cells (with nothing except fluid as nbs

			if(oldFlag&CFMbndInflow) {
				// force velocity for inflow
				const int OId = oldFlag>>24;
				DEFAULT_STREAM;
				//const LbmFloat fluidRho = 1.0;
				// for submerged inflows, streaming would have to be performed...
				LbmFloat fluidRho = m[0]; FORDF1 { fluidRho += m[l]; }
				const LbmVec vel(mObjectSpeeds[OId]);
				ux=vel[0], uy=vel[1], uz=vel[2]; 
				usqr = 1.5 * (ux*ux + uy*uy + uz*uz);
				FORDF0 { RAC(tcel, l) = D::getCollideEq(l, fluidRho,ux,uy,uz); }
			} else {
				if(nbored&CFBnd) {
					DEFAULT_STREAM;
					ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; 
					DEFAULT_COLLIDE;
					oldFlag &= (~CFNoBndFluid);
				} else {
					// do standard stream/collide
					OPTIMIZED_STREAMCOLLIDE;
					// FIXME check for which cells this is executed!
					oldFlag |= CFNoBndFluid;
				} 
			}

			PERFORM_USQRMAXCHECK;
			// "normal" fluid cells
			RAC(tcel,dFfrac) = 1.0; 
			*pFlagDst = (CellFlagType)oldFlag; // newFlag;
			LbmFloat ofrho=RAC(ccel,0);
			for(int l=1; l<D::cDfNum; l++) { ofrho += RAC(ccel,l); }
			calcCurrentMass += ofrho; 
			calcCurrentVolume += 1.0;
			continue;
		}
		
		newFlag  = oldFlag; //cell(i,j,k, SRCS(lev)).flag;
		// make sure: check which flags to really unset...!
		newFlag = newFlag & (~( 
					CFNoNbFluid|CFNoNbEmpty| CFNoDelete 
					| CFNoInterpolSrc
					| CFNoBndFluid
					));
		// unnecessary for interface cells... !?
		//if(nbored&CFBnd) { } else { newFlag |= CFNoBndFluid; }

		// store own dfs and mass
		mass = RAC(ccel,dMass);

		// WARNING - only interface cells arrive here!
		// read distribution funtions of adjacent cells = sweep step // FIXME after empty?
		DEFAULT_STREAM;

		if((nbored & CFFluid)==0) { newFlag |= CFNoNbFluid; mNumInvIfCells++; }
		if((nbored & CFEmpty)==0) { newFlag |= CFNoNbEmpty; mNumInvIfCells++; }

		// calculate mass exchange for interface cells 
		LbmFloat myfrac = RAC(ccel,dFfrac);
#		define nbdf(l) m[ D::dfInv[(l)] ]

		// update mass 
		// only do boundaries for fluid cells, and interface cells without
		// any fluid neighbors (assume that interface cells _with_ fluid
		// neighbors are affected enough by these) 
		// which Df's have to be reconstructed? 
		// for fluid cells - just the f_i difference from streaming to empty cells  ----
		numRecons = 0;

		FORDF1 { // dfl loop
			recons[l] = 0;
			// finally, "normal" interface cells ----
			if( NBFLAG(l)&CFFluid ) {
				change = nbdf(l) - MYDF(l);
			}
			// interface cells - distuingish cells that shouldn't fill/empty 
			else if( NBFLAG(l) & CFInter ) {
				
				LbmFloat mynbfac = //numNbs[l] / numNbs[0];
					QCELL_NB(lev, i,j,k,SRCS(lev),l, dFlux) / QCELL(lev, i,j,k,SRCS(lev), dFlux);
				LbmFloat nbnbfac = 1.0/mynbfac;
				//mynbfac = nbnbfac = 1.0; // switch calc flux off
				// OLD
				if ((oldFlag|NBFLAG(l))&(CFNoNbFluid|CFNoNbEmpty)) {
				switch (oldFlag&(CFNoNbFluid|CFNoNbEmpty)) {
					case 0: 
						// we are a normal cell so... 
						switch (NBFLAG(l)&(CFNoNbFluid|CFNoNbEmpty)) {
							case CFNoNbFluid: 
								// just fill current cell = empty neighbor 
								change = nbnbfac*nbdf(l) ; goto changeDone; 
							case CFNoNbEmpty: 
								// just empty current cell = fill neighbor 
								change = - mynbfac*MYDF(l) ; goto changeDone; 
						}
						break;

					case CFNoNbFluid: 
						// we dont have fluid nb's so...
						switch (NBFLAG(l)&(CFNoNbFluid|CFNoNbEmpty)) {
							case 0: 
							case CFNoNbEmpty: 
								// we have no fluid nb's -> just empty
								change = - mynbfac*MYDF(l) ; goto changeDone; 
						}
						break;

					case CFNoNbEmpty: 
						// we dont have empty nb's so...
						switch (NBFLAG(l)&(CFNoNbFluid|CFNoNbEmpty)) {
							case 0: 
							case CFNoNbFluid: 
								// we have no empty nb's -> just fill
								change = nbnbfac*nbdf(l); goto changeDone; 
						}
						break;
				}} // inter-inter exchange

				// just do normal mass exchange...
				change = ( nbnbfac*nbdf(l) - mynbfac*MYDF(l) ) ;
			changeDone: ;
				change *=  ( myfrac + QCELL_NB(lev, i,j,k, SRCS(lev),l, dFfrac) ) * 0.5;
			} // the other cell is interface

			// last alternative - reconstruction in this direction
			else {
				//if(NBFLAG(l) & CFEmpty) { recons[l] = true; }
				recons[l] = 1; 
				numRecons++;
				change = 0.0; 
				// which case is this...? empty + bnd
			}

			// modify mass at SRCS
			mass += change;
		} // l
		// normal interface, no if empty/fluid

		LbmFloat nv1,nv2;
		LbmFloat nx,ny,nz;

		if(NBFLAG(dE) &(CFFluid|CFInter)){ nv1 = RAC((ccel+QCELLSTEP ),dFfrac); } else nv1 = 0.0;
		if(NBFLAG(dW) &(CFFluid|CFInter)){ nv2 = RAC((ccel-QCELLSTEP ),dFfrac); } else nv2 = 0.0;
		nx = 0.5* (nv2-nv1);
		if(NBFLAG(dN) &(CFFluid|CFInter)){ nv1 = RAC((ccel+(mLevel[lev].lOffsx*QCELLSTEP)),dFfrac); } else nv1 = 0.0;
		if(NBFLAG(dS) &(CFFluid|CFInter)){ nv2 = RAC((ccel-(mLevel[lev].lOffsx*QCELLSTEP)),dFfrac); } else nv2 = 0.0;
		ny = 0.5* (nv2-nv1);
#if LBMDIM==3
		if(NBFLAG(dT) &(CFFluid|CFInter)){ nv1 = RAC((ccel+(mLevel[lev].lOffsy*QCELLSTEP)),dFfrac); } else nv1 = 0.0;
		if(NBFLAG(dB) &(CFFluid|CFInter)){ nv2 = RAC((ccel-(mLevel[lev].lOffsy*QCELLSTEP)),dFfrac); } else nv2 = 0.0;
		nz = 0.5* (nv2-nv1);
#else // LBMDIM==3
		nz = 0.0;
#endif // LBMDIM==3

		if( (ABS(nx)+ABS(ny)+ABS(nz)) > LBM_EPSILON) {
			// normal ok and usable...
			FORDF1 {
				if( (D::dfDvecX[l]*nx + D::dfDvecY[l]*ny + D::dfDvecZ[l]*nz)  // dot Dvec,norml
						> LBM_EPSILON) {
					recons[l] = 2; 
					numRecons++;
				} 
			}
		}

		// calculate macroscopic cell values
		LbmFloat oldUx, oldUy, oldUz;
		LbmFloat oldRho; // OLD rho = ccel->rho;
#if OPT3D==false
			oldRho=RAC(ccel,0);
			oldUx = oldUy = oldUz = 0.0;
			for(int l=1; l<D::cDfNum; l++) {
				oldRho += RAC(ccel,l);
				oldUx  += (D::dfDvecX[l]*RAC(ccel,l));
				oldUy  += (D::dfDvecY[l]*RAC(ccel,l)); 
				oldUz  += (D::dfDvecZ[l]*RAC(ccel,l)); 
			} 
#else // OPT3D==false
		oldRho = + RAC(ccel,dC)  + RAC(ccel,dN )
				+ RAC(ccel,dS ) + RAC(ccel,dE )
				+ RAC(ccel,dW ) + RAC(ccel,dT )
				+ RAC(ccel,dB ) + RAC(ccel,dNE)
				+ RAC(ccel,dNW) + RAC(ccel,dSE)
				+ RAC(ccel,dSW) + RAC(ccel,dNT)
				+ RAC(ccel,dNB) + RAC(ccel,dST)
				+ RAC(ccel,dSB) + RAC(ccel,dET)
				+ RAC(ccel,dEB) + RAC(ccel,dWT)
				+ RAC(ccel,dWB);

			oldUx = + RAC(ccel,dE) - RAC(ccel,dW)
				+ RAC(ccel,dNE) - RAC(ccel,dNW)
				+ RAC(ccel,dSE) - RAC(ccel,dSW)
				+ RAC(ccel,dET) + RAC(ccel,dEB)
				- RAC(ccel,dWT) - RAC(ccel,dWB);

			oldUy = + RAC(ccel,dN) - RAC(ccel,dS)
				+ RAC(ccel,dNE) + RAC(ccel,dNW)
				- RAC(ccel,dSE) - RAC(ccel,dSW)
				+ RAC(ccel,dNT) + RAC(ccel,dNB)
				- RAC(ccel,dST) - RAC(ccel,dSB);

			oldUz = + RAC(ccel,dT) - RAC(ccel,dB)
				+ RAC(ccel,dNT) - RAC(ccel,dNB)
				+ RAC(ccel,dST) - RAC(ccel,dSB)
				+ RAC(ccel,dET) - RAC(ccel,dEB)
				+ RAC(ccel,dWT) - RAC(ccel,dWB);
#endif

		// now reconstruction
#define REFERENCE_PRESSURE 1.0 // always atmosphere...
#if OPT3D==false
		// NOW - construct dist funcs from empty cells
		FORDF1 {
			if(recons[ l ]) {
				m[ D::dfInv[l] ] = 
					D::getCollideEq(l, REFERENCE_PRESSURE, oldUx,oldUy,oldUz) + 
					D::getCollideEq(D::dfInv[l], REFERENCE_PRESSURE, oldUx,oldUy,oldUz) 
					- MYDF( l );
				/*errMsg("D", " "<<PRINT_IJK<<" l"<<l<<" eql"<<D::getCollideEq(l, REFERENCE_PRESSURE, oldUx,oldUy,oldUz)<<
						" eqInvl"<<D::getCollideEq(D::dfInv[l], REFERENCE_PRESSURE, oldUx,oldUy,oldUz)<<
						" mydfl"<< MYDF( l ) <<
						" newdf"<< m[ D::dfInv[l] ]<<" m"<<mass ); // MRT_FS_TEST */
			} // */
		}
#else
		ux=oldUx, uy=oldUy, uz=oldUz;  // no local vars, only for usqr
		rho = REFERENCE_PRESSURE;
		usqr = 1.5 * (ux*ux + uy*uy + uz*uz);
		if(recons[dN ]) { m[dS ] = EQN  + EQS  - MYDF(dN ); }
		if(recons[dS ]) { m[dN ] = EQS  + EQN  - MYDF(dS ); }
		if(recons[dE ]) { m[dW ] = EQE  + EQW  - MYDF(dE ); }
		if(recons[dW ]) { m[dE ] = EQW  + EQE  - MYDF(dW ); }
		if(recons[dT ]) { m[dB ] = EQT  + EQB  - MYDF(dT ); }
		if(recons[dB ]) { m[dT ] = EQB  + EQT  - MYDF(dB ); }
		if(recons[dNE]) { m[dSW] = EQNE + EQSW - MYDF(dNE); }
		if(recons[dNW]) { m[dSE] = EQNW + EQSE - MYDF(dNW); }
		if(recons[dSE]) { m[dNW] = EQSE + EQNW - MYDF(dSE); }
		if(recons[dSW]) { m[dNE] = EQSW + EQNE - MYDF(dSW); }
		if(recons[dNT]) { m[dSB] = EQNT + EQSB - MYDF(dNT); }
		if(recons[dNB]) { m[dST] = EQNB + EQST - MYDF(dNB); }
		if(recons[dST]) { m[dNB] = EQST + EQNB - MYDF(dST); }
		if(recons[dSB]) { m[dNT] = EQSB + EQNT - MYDF(dSB); }
		if(recons[dET]) { m[dWB] = EQET + EQWB - MYDF(dET); }
		if(recons[dEB]) { m[dWT] = EQEB + EQWT - MYDF(dEB); }
		if(recons[dWT]) { m[dEB] = EQWT + EQEB - MYDF(dWT); }
		if(recons[dWB]) { m[dET] = EQWB + EQET - MYDF(dWB); }
#endif		

		// mass streaming done... 
		// now collide new fluid or "old" if cells
		ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2];
		DEFAULT_COLLIDE;
		rho = m[dC];
		FORDF1 { rho+=m[l]; };
		// only with interface neighbors...?
		PERFORM_USQRMAXCHECK;

		if(oldFlag & (CFMbndInflow)) {
			// fill if cells in inflow region
			if(myfrac<0.5) { 
				mass += 0.25; 
				mInitialMass += 0.25;
			}
		} 

		// interface cell filled or emptied?
		iffilled = ifemptied = 0;
		// interface cells empty/full?, WARNING: to mark these cells, better do it at the end of reinitCellFlags
		// interface cell if full?
		if( (mass) >= (rho * (1.0+FSGR_MAGICNR)) ) { iffilled = 1; }
		// interface cell if empty?
		if( (mass) <= (rho * (   -FSGR_MAGICNR)) ) { ifemptied = 1; }

		if(oldFlag & (CFMbndOutflow)) {
			mInitialMass -= mass;
			mass = myfrac = 0.0;
			iffilled = 0; ifemptied = 1;
		}

		// looks much nicer... LISTTRICK
#if FSGR_LISTTRICK==true
		if(!iffilled) {
			// remove cells independent from amount of change...
			if( (oldFlag & CFNoNbEmpty)&&(newFlag & CFNoNbEmpty)&&
					( (mass>(rho*FSGR_LISTTTHRESHFULL))  || ((nbored&CFInter)==0)  )
				) { 
				//if((nbored&CFInter)==0){ errMsg("NBORED!CFINTER","filled "<<PRINT_IJK); };
				iffilled = 1; 
			} 
		}
		if(!ifemptied) {
			if( (oldFlag & CFNoNbFluid)&&(newFlag & CFNoNbFluid)&&
					( (mass<(rho*FSGR_LISTTTHRESHEMPTY)) || ((nbored&CFInter)==0)  )
					) 
			{ 
				//if((nbored&CFInter)==0){ errMsg("NBORED!CFINTER","emptied "<<PRINT_IJK); };
				ifemptied = 1; 
			} 
		} // */
#endif

		//iffilled = ifemptied = 0; // DEBUG!!!!!!!!!!!!!!!
		

		// now that all dfs are known, handle last changes
		if(iffilled) {
			LbmPoint filledp;
			filledp.x = i; filledp.y = j; filledp.z = k;
#if PARALLEL==1
			calcListFull[id].push_back( filledp );
#else // PARALLEL==1
			mListFull.push_back( filledp );
#endif // PARALLEL==1
			//D::mNumFilledCells++; // DEBUG
			calcCellsFilled++;
		}
		else if(ifemptied) {
			LbmPoint emptyp;
			emptyp.x = i; emptyp.y = j; emptyp.z = k;
#if PARALLEL==1
			calcListEmpty[id].push_back( emptyp );
#else // PARALLEL==1
			mListEmpty.push_back( emptyp );
#endif // PARALLEL==1
			//D::mNumEmptiedCells++; // DEBUG
			calcCellsEmptied++;
		} else {
			// ...
		}
		
		// dont cutoff values -> better cell conversions
		RAC(tcel,dFfrac)   = (mass/rho);

		// init new flux value
		float flux = 0.5*(float)(D::cDfNum); // dxqn on
		//flux = 50.0; // extreme on
		for(int nn=1; nn<D::cDfNum; nn++) { 
			if(RFLAG_NB(lev, i,j,k,SRCS(lev),nn) & (CFFluid|CFInter|CFBnd)) {
				flux += D::dfLength[nn];
			}
		} 
		//flux = FLUX_INIT; // calc flux off
		QCELL(lev, i,j,k,TSET(lev), dFlux) = flux; // */

		// perform mass exchange with streamed values
		QCELL(lev, i,j,k,TSET(lev), dMass) = mass; // MASST
		// set new flag 
		*pFlagDst = (CellFlagType)newFlag;
		calcCurrentMass += mass; 
		calcCurrentVolume += RAC(tcel,dFfrac);

		// interface cell handling done...
	} // i
	int i=0; //dummy
	ADVANCE_POINTERS(2);
	} // j

#if COMPRESSGRIDS==1
#if PARALLEL==1
	//fprintf(stderr," (id=%d k=%d) ",id,k);
# pragma omp barrier
#endif // PARALLEL==1
#else // COMPRESSGRIDS==1
	int i=0; //dummy
	ADVANCE_POINTERS(mLevel[lev].lSizex*2);
#endif // COMPRESSGRIDS==1
  } // all cell loop k,j,i

	} // main loop region

	// write vars from parallel computations to class
	//errMsg("DFINI"," maxr l"<<mMaxRefine<<" cm="<<calcCurrentMass<<" cv="<<calcCurrentVolume );
	mLevel[lev].lmass    = calcCurrentMass;
	mLevel[lev].lvolume  = calcCurrentVolume;
	//mCurrentMass   += calcCurrentMass;
	//mCurrentVolume += calcCurrentVolume;
	D::mNumFilledCells  = calcCellsFilled;
	D::mNumEmptiedCells = calcCellsEmptied;
	D::mNumUsedCells = calcNumUsedCells;
#if PARALLEL==1
	//errMsg("PARALLELusqrcheck"," curr: "<<mMaxVlen<<"|"<<mMxvx<<","<<mMxvy<<","<<mMxvz);
	for(int i=0; i<MAX_THREADS; i++) {
		for(int j=0; j<calcListFull[i].size() ; j++) mListFull.push_back( calcListFull[i][j] );
		for(int j=0; j<calcListEmpty[i].size(); j++) mListEmpty.push_back( calcListEmpty[i][j] );
		if(calcMaxVlen[i]>mMaxVlen) { 
			mMxvx = calcMxvx[i]; 
			mMxvy = calcMxvy[i]; 
			mMxvz = calcMxvz[i]; 
			mMaxVlen = calcMaxVlen[i]; 
		} 
		errMsg("PARALLELusqrcheck"," curr: "<<mMaxVlen<<"|"<<mMxvx<<","<<mMxvy<<","<<mMxvz<<
				"      calc["<<i<<": "<<calcMaxVlen[i]<<"|"<<calcMxvx[i]<<","<<calcMxvy[i]<<","<<calcMxvz[i]<<"]  " );
	}
#endif // PARALLEL==1

	// check other vars...?
}

template<class D>
void 
LbmFsgrSolver<D>::coarseCalculateFluxareas(int lev)
{
	//LbmFloat calcCurrentMass = 0.0;
	//LbmFloat calcCurrentVolume = 0.0;
	//LbmFloat *ccel = NULL;
	//LbmFloat *tcel = NULL;
	//LbmFloat m[LBM_DFNUM];
	//LbmFloat rho, ux, uy, uz, tmp, usqr;
#if OPT3D==true 
	//LbmFloat lcsmqadd, lcsmqo, lcsmeq[LBM_DFNUM], lcsmomega;
#endif // OPT3D==true 
	//m[0] = tmp = usqr = 0.0;

	//for(int lev=0; lev<mMaxRefine; lev++) { TEST DEBUG
	FSGR_FORIJK_BOUNDS(lev) {
		if( RFLAG(lev, i,j,k,mLevel[lev].setCurr) & CFFluid) {
			if( RFLAG(lev+1, i*2,j*2,k*2,mLevel[lev+1].setCurr) & CFGrFromCoarse) {
				LbmFloat totArea = mFsgrCellArea[0]; // for l=0
				for(int l=1; l<D::cDirNum; l++) { 
					int ni=(2*i)+D::dfVecX[l], nj=(2*j)+D::dfVecY[l], nk=(2*k)+D::dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, mLevel[lev+1].setCurr)&
							(CFGrFromCoarse|CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
							) { 
						totArea += mFsgrCellArea[l];
					}
				} // l
				QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) = totArea;
				//continue;
			} else
			if( RFLAG(lev+1, i*2,j*2,k*2,mLevel[lev+1].setCurr) & (CFEmpty|CFUnused)) {
				QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) = 1.0;
				//continue;
			} else {
				QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) = 0.0;
			}
		//errMsg("DFINI"," at l"<<lev<<" "<<PRINT_IJK<<" v:"<<QCELL(lev, i,j,k,mLevel[lev].setCurr, dFlux) ); 
		}
	} // } TEST DEBUG
	if(!D::mSilent){ debMsgStd("coarseCalculateFluxareas",DM_MSG,"level "<<lev<<" calculated", 7); }
}
	
template<class D>
void 
LbmFsgrSolver<D>::coarseAdvance(int lev)
{
	LbmFloat calcCurrentMass = 0.0;
	LbmFloat calcCurrentVolume = 0.0;

	LbmFloat *ccel = NULL;
	LbmFloat *tcel = NULL;
	LbmFloat m[LBM_DFNUM];
	LbmFloat rho, ux, uy, uz, tmp, usqr;
#if OPT3D==true 
	LbmFloat lcsmqadd, lcsmqo, lcsmeq[LBM_DFNUM], lcsmomega;
#endif // OPT3D==true 
	m[0] = tmp = usqr = 0.0;

	coarseCalculateFluxareas(lev);
	// copied from fineAdv.
	CellFlagType *pFlagSrc = &RFLAG(lev, 1,1,getForZMin1(),SRCS(lev));
	CellFlagType *pFlagDst = &RFLAG(lev, 1,1,getForZMin1(),TSET(lev));
	pFlagSrc -= 1;
	pFlagDst -= 1;
	ccel = RACPNT(lev, 1,1,getForZMin1() ,SRCS(lev)); // QTEST
	ccel -= QCELLSTEP;
	tcel = RACPNT(lev, 1,1,getForZMin1() ,TSET(lev)); // QTEST
	tcel -= QCELLSTEP;
	//if(strstr(D::getName().c_str(),"Debug")){ errMsg("DEBUG","DEBUG!!!!!!!!!!!!!!!!!!!!!!!"); }

	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {
#if FSGR_STRICT_DEBUG==1
		rho = ux = uy = uz = tmp = usqr = -100.0; // DEBUG
#endif
		pFlagSrc++;
		pFlagDst++;
		ccel += QCELLSTEP;
		tcel += QCELLSTEP;

		// from coarse cells without unused nbs are not necessary...! -> remove
		if( ((*pFlagSrc) & (CFGrFromCoarse)) ) { 
			bool invNb = false;
			FORDF1 { 
				if(RFLAG_NB(lev, i, j, k, SRCS(lev), l) & CFUnused) { invNb = true; }
			}   
			if(!invNb) {
				*pFlagSrc = CFFluid|CFGrNorm;
#if ELBEEM_BLENDER!=1
				errMsg("coarseAdvance","FC2NRM_CHECK Converted CFGrFromCoarse to Norm at "<<lev<<" "<<PRINT_IJK);
#endif // ELBEEM_BLENDER!=1
				// FIXME add debug check for these types of cells?, move to perform coarsening?
			}
		} // */

		//*(pFlagSrc+pFlagTarOff) = *pFlagSrc; // always set other set...
#if FSGR_STRICT_DEBUG==1
		*pFlagDst = *pFlagSrc; // always set other set...
#else
		*pFlagDst = (*pFlagSrc & (~CFGrCoarseInited)); // always set other set... , remove coarse inited flag
#endif

		// old INTCFCOARSETEST==1
		if((*pFlagSrc) & CFGrFromCoarse) {  // interpolateFineFromCoarse test!
			if(( D::mStepCnt & (1<<(mMaxRefine-lev)) ) ==1) {
				FORDF0 { RAC(tcel,l) = RAC(ccel,l); }
			} else {
				interpolateCellFromCoarse( lev, i,j,k, TSET(lev), 0.0, CFFluid|CFGrFromCoarse, false);
				D::mNumUsedCells++;
			}
			continue; // interpolateFineFromCoarse test!
		} // interpolateFineFromCoarse test! old INTCFCOARSETEST==1

		if( ((*pFlagSrc) & (CFFluid)) ) { 
			ccel = RACPNT(lev, i,j,k ,SRCS(lev)); 
			tcel = RACPNT(lev, i,j,k ,TSET(lev));

			if( ((*pFlagSrc) & (CFGrFromFine)) ) { 
				FORDF0 { RAC(tcel,l) = RAC(ccel,l); }    // always copy...?
				continue; // comes from fine grid
			}
			// also ignore CFGrFromCoarse
			else if( ((*pFlagSrc) & (CFGrFromCoarse)) ) { 
				FORDF0 { RAC(tcel,l) = RAC(ccel,l); }    // always copy...?
				continue; 
			}

			OPTIMIZED_STREAMCOLLIDE;
			*pFlagDst |= CFNoBndFluid; // test?
			calcCurrentVolume += RAC(ccel,dFlux); 
			calcCurrentMass   += RAC(ccel,dFlux)*rho;
			//ebugMarkCell(lev+1, 2*i+1,2*j+1,2*k  );
#if FSGR_STRICT_DEBUG==1
			if(rho<-1.0){ debugMarkCell(lev, i,j,k ); 
				errMsg("INVRHOCELL_CHECK"," l"<<lev<<" "<< PRINT_IJK<<" rho:"<<rho ); 
				D::mPanic = 1;
			}
#endif // FSGR_STRICT_DEBUG==1
			D::mNumUsedCells++;

		}
	} 
	pFlagSrc+=2; // after x
	pFlagDst+=2; // after x
	ccel += (QCELLSTEP*2);
	tcel += (QCELLSTEP*2);
	} 
	pFlagSrc+= mLevel[lev].lSizex*2; // after y
	pFlagDst+= mLevel[lev].lSizex*2; // after y
	ccel += (QCELLSTEP*mLevel[lev].lSizex*2);
	tcel += (QCELLSTEP*mLevel[lev].lSizex*2);
	} // all cell loop k,j,i
	

	//errMsg("coarseAdvance","level "<<lev<<" stepped from "<<mLevel[lev].setCurr<<" to "<<mLevel[lev].setOther);
	if(!D::mSilent){ errMsg("coarseAdvance","level "<<lev<<" stepped from "<<SRCS(lev)<<" to "<<TSET(lev)); }
	// */

	// update other set
  mLevel[lev].setOther   = mLevel[lev].setCurr;
  mLevel[lev].setCurr   ^= 1;
  mLevel[lev].lsteps++;
  mLevel[lev].lmass   = calcCurrentMass   * mLevel[lev].lcellfactor;
  mLevel[lev].lvolume = calcCurrentVolume * mLevel[lev].lcellfactor;
#ifndef ELBEEM_BLENDER
  errMsg("DFINI", " m l"<<lev<<" m="<<mLevel[lev].lmass<<" c="<<calcCurrentMass<<"  lcf="<< mLevel[lev].lcellfactor );
  errMsg("DFINI", " v l"<<lev<<" v="<<mLevel[lev].lvolume<<" c="<<calcCurrentVolume<<"  lcf="<< mLevel[lev].lcellfactor );
#endif // ELBEEM_BLENDER
}

/*****************************************************************************/
//! multi level functions
/*****************************************************************************/


// get dfs from level (lev+1) to (lev) coarse border nodes
template<class D>
void 
LbmFsgrSolver<D>::coarseRestrictFromFine(int lev)
{
	if((lev<0) || ((lev+1)>mMaxRefine)) return;
#if FSGR_STRICT_DEBUG==1
	// reset all unused cell values to invalid
	int unuCnt = 0;
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
	for(int j=1;j<mLevel[lev].lSizey-1;++j) {
	for(int i=1;i<mLevel[lev].lSizex-1;++i) {
		CellFlagType *pFlagSrc = &RFLAG(lev, i,j,k,mLevel[lev].setCurr);
		if( ((*pFlagSrc) & (CFFluid|CFGrFromFine)) == (CFFluid|CFGrFromFine) ) { 
			FORDF0{	QCELL(lev, i,j,k,mLevel[lev].setCurr, l) = -10000.0;	}
			unuCnt++;
			// set here
		} else if( ((*pFlagSrc) & (CFFluid|CFGrNorm)) == (CFFluid|CFGrNorm) ) { 
			// simulated...
		} else {
			// reset in interpolation
			//errMsg("coarseRestrictFromFine"," reset l"<<lev<<" "<<PRINT_IJK);
		}
		if( ((*pFlagSrc) & (CFEmpty|CFUnused)) ) {  // test, also reset?
			FORDF0{	QCELL(lev, i,j,k,mLevel[lev].setCurr, l) = -10000.0;	}
		} // test
	} } }
	errMsg("coarseRestrictFromFine"," reset l"<<lev<<" fluid|coarseBorder cells: "<<unuCnt);
#endif // FSGR_STRICT_DEBUG==1
	const int srcSet = mLevel[lev+1].setCurr;
	const int dstSet = mLevel[lev].setCurr;

	LbmFloat rho=0.0, ux=0.0, uy=0.0, uz=0.0;			
	LbmFloat *ccel = NULL;
	LbmFloat *tcel = NULL;
#if OPT3D==true 
	LbmFloat m[LBM_DFNUM];
	// for macro add
	LbmFloat usqr;
	//LbmFloat *addfcel, *dstcell;
	LbmFloat lcsmqadd, lcsmqo, lcsmeq[LBM_DFNUM];
	LbmFloat lcsmDstOmega, lcsmSrcOmega, lcsmdfscale;
#else // OPT3D==true 
	LbmFloat df[LBM_DFNUM];
	LbmFloat omegaDst, omegaSrc;
	LbmFloat feq[LBM_DFNUM];
	LbmFloat dfScale = mDfScaleUp;
#endif // OPT3D==true 

	LbmFloat mGaussw[27];
	LbmFloat totGaussw = 0.0;
	const LbmFloat alpha = 1.0;
	const LbmFloat gw = sqrt(2.0*D::cDimension);
#ifndef ELBEEM_BLENDER
errMsg("coarseRestrictFromFine", "TCRFF_DFDEBUG2 test df/dir num!");
#endif
	for(int n=0;(n<D::cDirNum); n++) { mGaussw[n] = 0.0; }
	//for(int n=0;(n<D::cDirNum); n++) { 
	for(int n=0;(n<D::cDfNum); n++) { 
		const LbmFloat d = norm(LbmVec(D::dfVecX[n], D::dfVecY[n], D::dfVecZ[n]));
		LbmFloat w = expf( -alpha*d*d ) - expf( -alpha*gw*gw );
		//errMsg("coarseRestrictFromFine", "TCRFF_DFDEBUG2 cell  n"<<n<<" d"<<d<<" w"<<w);
		mGaussw[n] = w;
		totGaussw += w;
	}
	for(int n=0;(n<D::cDirNum); n++) { 
		mGaussw[n] = mGaussw[n]/totGaussw;
	}
	//totGaussw = 1.0/totGaussw;

	//if(!D::mInitDone) {
//errMsg("coarseRestrictFromFine", "TCRFF_DFDEBUG2 test pre init");
		//mGaussw[0] = 1.0;
		//for(int n=1;(n<D::cDirNum); n++) { mGaussw[n] = 0.0; }
	//}

	//restrict
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
	for(int j=1;j<mLevel[lev].lSizey-1;++j) {
	for(int i=1;i<mLevel[lev].lSizex-1;++i) {
		CellFlagType *pFlagSrc = &RFLAG(lev, i,j,k,dstSet);
		if((*pFlagSrc) & (CFFluid)) { 
			if( ((*pFlagSrc) & (CFFluid|CFGrFromFine)) == (CFFluid|CFGrFromFine) ) { 
				// TODO? optimize?
				// do resctriction
				mNumInterdCells++;
				ccel = RACPNT(lev+1, 2*i,2*j,2*k,srcSet);
				tcel = RACPNT(lev  , i,j,k      ,dstSet);

#				if OPT3D==false
				// add up weighted dfs
				FORDF0{ df[l] = 0.0;}
				for(int n=0;(n<D::cDirNum); n++) { 
					int ni=2*i+1*D::dfVecX[n], nj=2*j+1*D::dfVecY[n], nk=2*k+1*D::dfVecZ[n];
					ccel = RACPNT(lev+1, ni,nj,nk,srcSet);// CFINTTEST
					const LbmFloat weight = mGaussw[n];
					FORDF0{
						LbmFloat cdf = weight * RAC(ccel,l);
#						if FSGR_STRICT_DEBUG==1
						if( cdf<-1.0 ){ errMsg("INVDFCREST_DFCHECK", PRINT_IJK<<" s"<<dstSet<<" from "<<PRINT_VEC(2*i,2*j,2*k)<<" s"<<srcSet<<" df"<<l<<":"<< df[l]); }
#						endif
						//errMsg("INVDFCREST_DFCHECK", PRINT_IJK<<" s"<<dstSet<<" from "<<PRINT_VEC(2*i,2*j,2*k)<<" s"<<srcSet<<" df"<<l<<":"<< df[l]<<" = "<<cdf<<" , w"<<weight); 
						df[l] += cdf;
					}
				}

				// calc rho etc. from weighted dfs
				rho = ux  = uy  = uz  = 0.0;
				FORDF0{
					LbmFloat cdf = df[l];
					rho += cdf; 
					ux  += (D::dfDvecX[l]*cdf); 
					uy  += (D::dfDvecY[l]*cdf);  
					uz  += (D::dfDvecZ[l]*cdf);  
				}

				FORDF0{ feq[l] = D::getCollideEq(l, rho,ux,uy,uz); }
				if(mLevel[lev  ].lcsmago>0.0) {
					const LbmFloat Qo = D::getLesNoneqTensorCoeff(df,feq);
					omegaDst  = D::getLesOmega(mLevel[lev  ].omega,mLevel[lev  ].lcsmago,Qo);
					omegaSrc = D::getLesOmega(mLevel[lev+1].omega,mLevel[lev+1].lcsmago,Qo);
				} else {
					omegaDst = mLevel[lev+0].omega; /* NEWSMAGOT*/ 
					omegaSrc = mLevel[lev+1].omega;
				}
				dfScale   = (mLevel[lev  ].stepsize/mLevel[lev+1].stepsize)* (1.0/omegaDst-1.0)/ (1.0/omegaSrc-1.0); // yu
				FORDF0{
					RAC(tcel, l) = feq[l]+ (df[l]-feq[l])*dfScale;
				} 
#				else // OPT3D
				// similar to OPTIMIZED_STREAMCOLLIDE_UNUSED
                      
				//rho = ux = uy = uz = 0.0;
				MSRC_C  = CCELG_C(0) ;
				MSRC_N  = CCELG_N(0) ;
				MSRC_S  = CCELG_S(0) ;
				MSRC_E  = CCELG_E(0) ;
				MSRC_W  = CCELG_W(0) ;
				MSRC_T  = CCELG_T(0) ;
				MSRC_B  = CCELG_B(0) ;
				MSRC_NE = CCELG_NE(0);
				MSRC_NW = CCELG_NW(0);
				MSRC_SE = CCELG_SE(0);
				MSRC_SW = CCELG_SW(0);
				MSRC_NT = CCELG_NT(0);
				MSRC_NB = CCELG_NB(0);
				MSRC_ST = CCELG_ST(0);
				MSRC_SB = CCELG_SB(0);
				MSRC_ET = CCELG_ET(0);
				MSRC_EB = CCELG_EB(0);
				MSRC_WT = CCELG_WT(0);
				MSRC_WB = CCELG_WB(0);
				for(int n=1;(n<D::cDirNum); n++) { 
					ccel = RACPNT(lev+1,  2*i+1*D::dfVecX[n], 2*j+1*D::dfVecY[n], 2*k+1*D::dfVecZ[n]  ,srcSet);
					MSRC_C  += CCELG_C(n) ;
					MSRC_N  += CCELG_N(n) ;
					MSRC_S  += CCELG_S(n) ;
					MSRC_E  += CCELG_E(n) ;
					MSRC_W  += CCELG_W(n) ;
					MSRC_T  += CCELG_T(n) ;
					MSRC_B  += CCELG_B(n) ;
					MSRC_NE += CCELG_NE(n);
					MSRC_NW += CCELG_NW(n);
					MSRC_SE += CCELG_SE(n);
					MSRC_SW += CCELG_SW(n);
					MSRC_NT += CCELG_NT(n);
					MSRC_NB += CCELG_NB(n);
					MSRC_ST += CCELG_ST(n);
					MSRC_SB += CCELG_SB(n);
					MSRC_ET += CCELG_ET(n);
					MSRC_EB += CCELG_EB(n);
					MSRC_WT += CCELG_WT(n);
					MSRC_WB += CCELG_WB(n);
				}
				rho = MSRC_C  + MSRC_N + MSRC_S  + MSRC_E + MSRC_W  + MSRC_T  
					+ MSRC_B  + MSRC_NE + MSRC_NW + MSRC_SE + MSRC_SW + MSRC_NT 
					+ MSRC_NB + MSRC_ST + MSRC_SB + MSRC_ET + MSRC_EB + MSRC_WT + MSRC_WB; 
				ux = MSRC_E - MSRC_W + MSRC_NE - MSRC_NW + MSRC_SE - MSRC_SW 
					+ MSRC_ET + MSRC_EB - MSRC_WT - MSRC_WB;  
				uy = MSRC_N - MSRC_S + MSRC_NE + MSRC_NW - MSRC_SE - MSRC_SW 
					+ MSRC_NT + MSRC_NB - MSRC_ST - MSRC_SB;  
				uz = MSRC_T - MSRC_B + MSRC_NT - MSRC_NB + MSRC_ST - MSRC_SB 
					+ MSRC_ET - MSRC_EB + MSRC_WT - MSRC_WB;  
				usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
				\
				lcsmeq[dC] = EQC ; \
				COLL_CALCULATE_DFEQ(lcsmeq); \
				COLL_CALCULATE_NONEQTENSOR(lev+0, MSRC_ )\
				COLL_CALCULATE_CSMOMEGAVAL(lev+0, lcsmDstOmega); \
				COLL_CALCULATE_CSMOMEGAVAL(lev+1, lcsmSrcOmega); \
				\
				lcsmdfscale   = (mLevel[lev+0].stepsize/mLevel[lev+1].stepsize)* (1.0/lcsmDstOmega-1.0)/ (1.0/lcsmSrcOmega-1.0);  \
				RAC(tcel, dC ) = (lcsmeq[dC ] + (MSRC_C -lcsmeq[dC ] )*lcsmdfscale);
				RAC(tcel, dN ) = (lcsmeq[dN ] + (MSRC_N -lcsmeq[dN ] )*lcsmdfscale);
				RAC(tcel, dS ) = (lcsmeq[dS ] + (MSRC_S -lcsmeq[dS ] )*lcsmdfscale);
				RAC(tcel, dE ) = (lcsmeq[dE ] + (MSRC_E -lcsmeq[dE ] )*lcsmdfscale);
				RAC(tcel, dW ) = (lcsmeq[dW ] + (MSRC_W -lcsmeq[dW ] )*lcsmdfscale);
				RAC(tcel, dT ) = (lcsmeq[dT ] + (MSRC_T -lcsmeq[dT ] )*lcsmdfscale);
				RAC(tcel, dB ) = (lcsmeq[dB ] + (MSRC_B -lcsmeq[dB ] )*lcsmdfscale);
				RAC(tcel, dNE) = (lcsmeq[dNE] + (MSRC_NE-lcsmeq[dNE] )*lcsmdfscale);
				RAC(tcel, dNW) = (lcsmeq[dNW] + (MSRC_NW-lcsmeq[dNW] )*lcsmdfscale);
				RAC(tcel, dSE) = (lcsmeq[dSE] + (MSRC_SE-lcsmeq[dSE] )*lcsmdfscale);
				RAC(tcel, dSW) = (lcsmeq[dSW] + (MSRC_SW-lcsmeq[dSW] )*lcsmdfscale);
				RAC(tcel, dNT) = (lcsmeq[dNT] + (MSRC_NT-lcsmeq[dNT] )*lcsmdfscale);
				RAC(tcel, dNB) = (lcsmeq[dNB] + (MSRC_NB-lcsmeq[dNB] )*lcsmdfscale);
				RAC(tcel, dST) = (lcsmeq[dST] + (MSRC_ST-lcsmeq[dST] )*lcsmdfscale);
				RAC(tcel, dSB) = (lcsmeq[dSB] + (MSRC_SB-lcsmeq[dSB] )*lcsmdfscale);
				RAC(tcel, dET) = (lcsmeq[dET] + (MSRC_ET-lcsmeq[dET] )*lcsmdfscale);
				RAC(tcel, dEB) = (lcsmeq[dEB] + (MSRC_EB-lcsmeq[dEB] )*lcsmdfscale);
				RAC(tcel, dWT) = (lcsmeq[dWT] + (MSRC_WT-lcsmeq[dWT] )*lcsmdfscale);
				RAC(tcel, dWB) = (lcsmeq[dWB] + (MSRC_WB-lcsmeq[dWB] )*lcsmdfscale);
#				endif // OPT3D==false

				//? if((lev<mMaxRefine)&&(D::cDimension==2)) { debugMarkCell(lev,i,j,k); }
#			if FSGR_STRICT_DEBUG==1
				//errMsg("coarseRestrictFromFine", "CRFF_DFDEBUG cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" " ); 
#			endif // FSGR_STRICT_DEBUG==1
				D::mNumUsedCells++;
			} // from fine & fluid
			else {
				if(RFLAG(lev+1, 2*i,2*j,2*k,srcSet) & CFGrFromCoarse) {
					RFLAG(lev, i,j,k,dstSet) |= CFGrToFine;
				} else {
					RFLAG(lev, i,j,k,dstSet) &= (~CFGrToFine);
				}
			}
		} // & fluid
	}}}
	if(!D::mSilent){ errMsg("coarseRestrictFromFine"," from l"<<(lev+1)<<",s"<<mLevel[lev+1].setCurr<<" to l"<<lev<<",s"<<mLevel[lev].setCurr); }
}

template<class D>
bool 
LbmFsgrSolver<D>::performRefinement(int lev) {
	if((lev<0) || ((lev+1)>mMaxRefine)) return false;
	bool change = false;
	//bool nbsok;
	// TIMEINTORDER ?
	LbmFloat interTime = 0.0;
	// update curr from other, as streaming afterwards works on curr
	// thus read only from srcSet, modify other
	const int srcSet = mLevel[lev].setOther;
	const int dstSet = mLevel[lev].setCurr;
	const int srcFineSet = mLevel[lev+1].setCurr;
	const bool debugRefinement = false;

	// use template functions for 2D/3D
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {

		if(RFLAG(lev, i,j,k, srcSet) & CFGrFromFine) {
			bool removeFromFine = false;
			const CellFlagType notAllowed = (CFInter|CFGrFromFine|CFGrToFine);
			CellFlagType reqType = CFGrNorm;
			if(lev+1==mMaxRefine) reqType = CFNoBndFluid;
			
#if REFINEMENTBORDER==1
			if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), srcFineSet) & reqType) &&
			    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), srcFineSet) & (notAllowed)) )  ){ // ok
			} else {
				removeFromFine=true;
			}
			/*if(strstr(D::getName().c_str(),"Debug"))
			if(lev+1==mMaxRefine) { // mixborder
				for(int l=0;((l<D::cDirNum) && (!removeFromFine)); l++) {  // FARBORD
					int ni=2*i+2*D::dfVecX[l], nj=2*j+2*D::dfVecY[l], nk=2*k+2*D::dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, srcFineSet)&CFBnd) { // NEWREFT
						removeFromFine=true;
					}
				}
			} // FARBORD */
#elif REFINEMENTBORDER==2 // REFINEMENTBORDER==1
			FIX
			for(int l=0;((l<D::cDirNum) && (!removeFromFine)); l++) { 
				int ni=2*i+D::dfVecX[l], nj=2*j+D::dfVecY[l], nk=2*k+D::dfVecZ[l];
				if(RFLAG(lev+1, ni,nj,nk, srcFineSet)&notSrcAllowed) { // NEWREFT
					removeFromFine=true;
				}
			}
			/*for(int l=0;((l<D::cDirNum) && (!removeFromFine)); l++) {  // FARBORD
				int ni=2*i+2*D::dfVecX[l], nj=2*j+2*D::dfVecY[l], nk=2*k+2*D::dfVecZ[l];
				if(RFLAG(lev+1, ni,nj,nk, srcFineSet)&notSrcAllowed) { // NEWREFT
					removeFromFine=true;
				}
			} // FARBORD */
#elif REFINEMENTBORDER==3 // REFINEMENTBORDER==1
			FIX
			if(lev+1==mMaxRefine) { // mixborder
				if(RFLAG(lev+1, 2*i,2*j,2*k, srcFineSet)&notSrcAllowed) { 
					removeFromFine=true;
				}
			} else { // mixborder
				for(int l=0; l<D::cDirNum; l++) { 
					int ni=2*i+D::dfVecX[l], nj=2*j+D::dfVecY[l], nk=2*k+D::dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, srcFineSet)&notSrcAllowed) { // NEWREFT
						removeFromFine=true;
					}
				}
			} // mixborder
			// also remove from fine cells that are above from fine
#else // REFINEMENTBORDER==1
			ERROR
#endif // REFINEMENTBORDER==1

			if(removeFromFine) {
				// dont turn CFGrFromFine above interface cells into CFGrNorm
				//errMsg("performRefinement","Removing CFGrFromFine on lev"<<lev<<" " <<PRINT_IJK<<" srcflag:"<<convertCellFlagType2String(RFLAG(lev+1, (2*i),(2*j),(2*k), srcFineSet)) <<" set:"<<dstSet );
				RFLAG(lev, i,j,k, dstSet) = CFEmpty;
#if FSGR_STRICT_DEBUG==1
				// for interpolation later on during fine grid fixing
				// these cells are still correctly inited
				RFLAG(lev, i,j,k, dstSet) |= CFGrCoarseInited;  // remove later on? FIXME?
#endif // FSGR_STRICT_DEBUG==1
				//RFLAG(lev, i,j,k, mLevel[lev].setOther) = CFEmpty; // FLAGTEST
				if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev,i,j,k); 
				change=true;
				mNumFsgrChanges++;
				for(int l=1; l<D::cDirNum; l++) { 
					int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
					//errMsg("performRefinement","On lev:"<<lev<<" check: "<<PRINT_VEC(ni,nj,nk)<<" set:"<<dstSet<<" = "<<convertCellFlagType2String(RFLAG(lev, ni,nj,nk, srcSet)) );
					if( (  RFLAG(lev, ni,nj,nk, srcSet)&CFFluid      ) &&
							(!(RFLAG(lev, ni,nj,nk, srcSet)&CFGrFromFine)) ) { // dont change status of nb. from fine cells
						// tag as inited for debugging, cell contains fluid DFs anyway
						RFLAG(lev, ni,nj,nk, dstSet) = CFFluid|CFGrFromFine|CFGrCoarseInited;
						//errMsg("performRefinement","On lev:"<<lev<<" set to from fine: "<<PRINT_VEC(ni,nj,nk)<<" set:"<<dstSet);
						//if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev,ni,nj,nk); 
					}
				} // l 

				// FIXME fix fine level?
			}

			// recheck from fine flag
		}
	}}} // TEST


	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) { // TEST
  for(int j=1;j<mLevel[lev].lSizey-1;++j) { // TEST
  for(int i=1;i<mLevel[lev].lSizex-1;++i) { // TEST

		// test from coarseAdvance
		// from coarse cells without unused nbs are not necessary...! -> remove
		/*if( ((*pFlagSrc) & (CFGrFromCoarse)) ) { 
			bool invNb = false;
			FORDF1 { 
				if(RFLAG_NB(lev, i, j, k, SRCS(lev), l) & CFUnused) { invNb = true; }
			}   
			if(!invNb) {
				*pFlagSrc = CFFluid|CFGrNorm;
				errMsg("coarseAdvance","FC2NRM_CHECK Converted CFGrFromCoarse to Norm at "<<lev<<" "<<PRINT_IJK);
			}
		} // */

		if(RFLAG(lev, i,j,k, srcSet) & CFGrFromCoarse) {

			// from coarse cells without unused nbs are not necessary...! -> remove
			bool invNb = false;
			bool fluidNb = false;
			for(int l=1; l<D::cDirNum; l++) { 
				if(RFLAG_NB(lev, i, j, k, srcSet, l) & CFUnused) { invNb = true; }
				if(RFLAG_NB(lev, i, j, k, srcSet, l) & (CFGrNorm)) { fluidNb = true; }
			}   
			if(!invNb) {
				// no unused cells around -> calculate normally from now on
				RFLAG(lev, i,j,k, dstSet) = CFFluid|CFGrNorm;
				if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev, i, j, k); 
				change=true;
				mNumFsgrChanges++;
			} // from advance */
			if(!fluidNb) {
				// no fluid cells near -> no transfer necessary
				RFLAG(lev, i,j,k, dstSet) = CFUnused;
				//RFLAG(lev, i,j,k, mLevel[lev].setOther) = CFUnused; // FLAGTEST
				if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev, i, j, k); 
				change=true;
				mNumFsgrChanges++;
			} // from advance */


			// dont allow double transfer
			// this might require fixing the neighborhood
			if(RFLAG(lev+1, 2*i,2*j,2*k, srcFineSet)&(CFGrFromCoarse)) { 
				// dont turn CFGrFromFine above interface cells into CFGrNorm
				//errMsg("performRefinement","Removing CFGrFromCoarse on lev"<<lev<<" " <<PRINT_IJK<<" due to finer from coarse cell " );
				RFLAG(lev, i,j,k, dstSet) = CFFluid|CFGrNorm;
				if(lev>0) RFLAG(lev-1, i/2,j/2,k/2, mLevel[lev-1].setCurr) &= (~CFGrToFine); // TODO add more of these?
				if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev, i, j, k); 
				change=true;
				mNumFsgrChanges++;
				for(int l=1; l<D::cDirNum; l++) { 
					int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
					if(RFLAG(lev, ni,nj,nk, srcSet)&(CFGrNorm)) { //ok
						for(int m=1; m<D::cDirNum; m++) { 
							int mi=  ni +D::dfVecX[m], mj=  nj +D::dfVecY[m], mk=  nk +D::dfVecZ[m];
							if(RFLAG(lev,  mi, mj, mk, srcSet)&CFUnused) {
								// norm cells in neighborhood with unused nbs have to be new border...
								RFLAG(lev, ni,nj,nk, dstSet) = CFFluid|CFGrFromCoarse;
								if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev,ni,nj,nk); 
							}
						}
						// these alreay have valid values...
					}
					else if(RFLAG(lev, ni,nj,nk, srcSet)&(CFUnused)) { //ok
						// this should work because we have a valid neighborhood here for now
						interpolateCellFromCoarse(lev,  ni, nj, nk, dstSet /*mLevel[lev].setCurr*/, interTime, CFFluid|CFGrFromCoarse, false);
						if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev,ni,nj,nk); 
						mNumFsgrChanges++;
					}
				} // l 
			} // double transer

		} // from coarse

	} } }


	// fix dstSet from fine cells here
	// warning - checks CFGrFromFine on dstset changed before!
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) { // TEST
  for(int j=1;j<mLevel[lev].lSizey-1;++j) { // TEST
  for(int i=1;i<mLevel[lev].lSizex-1;++i) { // TEST

		//if(RFLAG(lev, i,j,k, srcSet) & CFGrFromFine) {
		if(RFLAG(lev, i,j,k, dstSet) & CFGrFromFine) {
			// modify finer level border
			if((RFLAG(lev+1, 2*i,2*j,2*k, srcFineSet)&(CFGrFromCoarse))) { 
				//errMsg("performRefinement","Removing CFGrFromCoarse on lev"<<(lev+1)<<" from l"<<lev<<" " <<PRINT_IJK );
				CellFlagType setf = CFFluid;
				if(lev+1 < mMaxRefine) setf = CFFluid|CFGrNorm;
				RFLAG(lev+1, 2*i,2*j,2*k, srcFineSet)=setf;
				change=true;
				mNumFsgrChanges++;
				for(int l=1; l<D::cDirNum; l++) { 
					int bi=(2*i)+D::dfVecX[l], bj=(2*j)+D::dfVecY[l], bk=(2*k)+D::dfVecZ[l];
					if(RFLAG(lev+1,  bi, bj, bk, srcFineSet)&(CFGrFromCoarse)) {
						//errMsg("performRefinement","Removing CFGrFromCoarse on lev"<<(lev+1)<<" "<<PRINT_VEC(bi,bj,bk) );
						RFLAG(lev+1,  bi, bj, bk, srcFineSet) = setf;
						if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev+1,bi,bj,bk); 
					}
					else if(RFLAG(lev+1,  bi, bj, bk, srcFineSet)&(CFUnused      )) { 
						//errMsg("performRefinement","Removing CFUnused on lev"<<(lev+1)<<" "<<PRINT_VEC(bi,bj,bk) );
						interpolateCellFromCoarse(lev+1,  bi, bj, bk, srcFineSet, interTime, setf, false);
						if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev+1,bi,bj,bk); 
						mNumFsgrChanges++;
					}
				}
				for(int l=1; l<D::cDirNum; l++) { 
					int bi=(2*i)+D::dfVecX[l], bj=(2*j)+D::dfVecY[l], bk=(2*k)+D::dfVecZ[l];
					if(   (RFLAG(lev+1,  bi, bj, bk, srcFineSet)&CFFluid       ) &&
							(!(RFLAG(lev+1,  bi, bj, bk, srcFineSet)&CFGrFromCoarse)) ) {
						// all unused nbs now of coarse have to be from coarse
						for(int m=1; m<D::cDirNum; m++) { 
							int mi=  bi +D::dfVecX[m], mj=  bj +D::dfVecY[m], mk=  bk +D::dfVecZ[m];
							if(RFLAG(lev+1,  mi, mj, mk, srcFineSet)&CFUnused) {
								//errMsg("performRefinement","Changing CFUnused on lev"<<(lev+1)<<" "<<PRINT_VEC(mi,mj,mk) );
								interpolateCellFromCoarse(lev+1,  mi, mj, mk, srcFineSet, interTime, CFFluid|CFGrFromCoarse, false);
								if((D::cDimension==2)&&(debugRefinement)) debugMarkCell(lev+1,mi,mj,mk); 
								mNumFsgrChanges++;
							}
						}
						// nbs prepared...
					}
				}
			}
			
		} // convert regions of from fine
	}}} // TEST

	if(!D::mSilent){ errMsg("performRefinement"," for l"<<lev<<" done ("<<change<<") " ); }
	return change;
}


// done after refinement
template<class D>
bool 
LbmFsgrSolver<D>::performCoarsening(int lev) {
	//if(D::mInitDone){ errMsg("performCoarsening","skip"); return 0;} // DEBUG
					
	if((lev<0) || ((lev+1)>mMaxRefine)) return false;
	bool change = false;
	bool nbsok;
	// hence work on modified curr set
	const int srcSet = mLevel[lev].setCurr;
	const int dstlev = lev+1;
	const int dstFineSet = mLevel[dstlev].setCurr;
	const bool debugCoarsening = false;

	// use template functions for 2D/3D
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {

			// from coarse cells without unused nbs are not necessary...! -> remove
			// perform check from coarseAdvance here?
			if(RFLAG(lev, i,j,k, srcSet) & CFGrFromFine) {
				// remove from fine cells now that are completely in fluid
				// FIXME? check that new from fine in performRefinement never get deleted here afterwards?
				// or more general, one cell never changed more than once?
				const CellFlagType notAllowed = (CFInter|CFGrFromFine|CFGrToFine);
				//const CellFlagType notNbAllowed = (CFInter|CFBnd|CFGrFromFine); unused
				CellFlagType reqType = CFGrNorm;
				if(lev+1==mMaxRefine) reqType = CFNoBndFluid;

				nbsok = true;
				for(int l=0; l<D::cDirNum && nbsok; l++) { 
					int ni=(2*i)+D::dfVecX[l], nj=(2*j)+D::dfVecY[l], nk=(2*k)+D::dfVecZ[l];
					if(   (RFLAG(lev+1, ni,nj,nk, dstFineSet) & reqType) &&
							(!(RFLAG(lev+1, ni,nj,nk, dstFineSet) & (notAllowed)) )  ){
						// ok
					} else {
						nbsok=false;
					}
					/*if(strstr(D::getName().c_str(),"Debug"))
					if((nbsok)&&(lev+1==mMaxRefine)) { // mixborder
						for(int l=0;((l<D::cDirNum) && (nbsok)); l++) {  // FARBORD
							int ni=2*i+2*D::dfVecX[l], nj=2*j+2*D::dfVecY[l], nk=2*k+2*D::dfVecZ[l];
							if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&CFBnd) { // NEWREFT
								nbsok=false;
							}
						}
					} // FARBORD */
				}
				// dont turn CFGrFromFine above interface cells into CFGrNorm
				// now check nbs on same level
				for(int l=1; l<D::cDirNum && nbsok; l++) { 
					int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
					if(RFLAG(lev, ni,nj,nk, srcSet)&(CFFluid)) { //ok
					} else {
						nbsok = false;
					}
				} // l

				if(nbsok) {
					// conversion to coarse fluid cell
					change = true;
					mNumFsgrChanges++;
					RFLAG(lev, i,j,k, srcSet) = CFFluid|CFGrNorm;
					// dfs are already ok...
					//if(D::mInitDone) errMsg("performCoarsening","CFGrFromFine changed to CFGrNorm at lev"<<lev<<" " <<PRINT_IJK );
					if((D::cDimension==2)&&(debugCoarsening)) debugMarkCell(lev,i,j,k); 

					// only check complete cubes
					for(int dx=-1;dx<=1;dx+=2) {
					for(int dy=-1;dy<=1;dy+=2) {
					for(int dz=-1*(LBMDIM&1);dz<=1*(LBMDIM&1);dz+=2) { // 2d/3d
						// check for norm and from coarse, as the coarse level might just have been refined...
						/*if(D::mInitDone) errMsg("performCoarsening","CFGrFromFine subc check "<< "x"<<convertCellFlagType2String( RFLAG(lev, i+dx, j   , k   ,  srcSet))<<" "
									"y"<<convertCellFlagType2String( RFLAG(lev, i   , j+dy, k   ,  srcSet))<<" " "z"<<convertCellFlagType2String( RFLAG(lev, i   , j   , k+dz,  srcSet))<<" "
									"xy"<<convertCellFlagType2String( RFLAG(lev, i+dx, j+dy, k   ,  srcSet))<<" " "xz"<<convertCellFlagType2String( RFLAG(lev, i+dx, j   , k+dz,  srcSet))<<" "
									"yz"<<convertCellFlagType2String( RFLAG(lev, i   , j+dy, k+dz,  srcSet))<<" " "xyz"<<convertCellFlagType2String( RFLAG(lev, i+dx, j+dy, k+dz,  srcSet))<<" " ); // */
						if( 
								// we now the flag of the current cell! ( RFLAG(lev, i   , j   , k   ,  srcSet)&(CFGrNorm)) &&
								( RFLAG(lev, i+dx, j   , k   ,  srcSet)&(CFGrNorm|CFGrFromCoarse)) &&
								( RFLAG(lev, i   , j+dy, k   ,  srcSet)&(CFGrNorm|CFGrFromCoarse)) &&
								( RFLAG(lev, i   , j   , k+dz,  srcSet)&(CFGrNorm|CFGrFromCoarse)) &&

								( RFLAG(lev, i+dx, j+dy, k   ,  srcSet)&(CFGrNorm|CFGrFromCoarse)) &&
								( RFLAG(lev, i+dx, j   , k+dz,  srcSet)&(CFGrNorm|CFGrFromCoarse)) &&
								( RFLAG(lev, i   , j+dy, k+dz,  srcSet)&(CFGrNorm|CFGrFromCoarse)) &&
								( RFLAG(lev, i+dx, j+dy, k+dz,  srcSet)&(CFGrNorm|CFGrFromCoarse)) 
							) {
							// middle source node on higher level
							int dstx = (2*i)+dx;
							int dsty = (2*j)+dy;
							int dstz = (2*k)+dz;

							mNumFsgrChanges++;
							RFLAG(dstlev, dstx,dsty,dstz, dstFineSet) = CFUnused;
							RFLAG(dstlev, dstx,dsty,dstz, mLevel[dstlev].setOther) = CFUnused; // FLAGTEST
							//if(D::mInitDone) errMsg("performCoarsening","CFGrFromFine subcube init center unused set l"<<dstlev<<" at "<<PRINT_VEC(dstx,dsty,dstz) );

							for(int l=1; l<D::cDirNum; l++) { 
								int dstni=dstx+D::dfVecX[l], dstnj=dsty+D::dfVecY[l], dstnk=dstz+D::dfVecZ[l];
								if(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)&(CFFluid)) { 
									RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet) = CFFluid|CFGrFromCoarse;
								}
								if(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)&(CFInter)) { 
									//if(D::mInitDone) errMsg("performCoarsening","CFGrFromFine subcube init CHECK Warning - deleting interface cell...");
									D::mFixMass += QCELL( dstlev, dstni,dstnj,dstnk, dstFineSet, dMass);
									RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet) = CFFluid|CFGrFromCoarse;
								}
							} // l

							// again check nb flags of all surrounding cells to see if any from coarse
							// can be convted to unused
							for(int l=1; l<D::cDirNum; l++) { 
								int dstni=dstx+D::dfVecX[l], dstnj=dsty+D::dfVecY[l], dstnk=dstz+D::dfVecZ[l];
								// have to be at least from coarse here...
								//errMsg("performCoarsening","CFGrFromFine subcube init unused check l"<<dstlev<<" at "<<PRINT_VEC(dstni,dstnj,dstnk)<<" "<< convertCellFlagType2String(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)) );
								if(!(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)&(CFUnused) )) { 
									bool delok = true;
									// careful long range here... check domain bounds?
									for(int m=1; m<D::cDirNum; m++) { 										
										int chkni=dstni+D::dfVecX[m], chknj=dstnj+D::dfVecY[m], chknk=dstnk+D::dfVecZ[m];
										if(RFLAG(dstlev, chkni,chknj,chknk, dstFineSet)&(CFUnused|CFGrFromCoarse)) { 
											// this nb cell is ok for deletion
										} else { 
											delok=false; // keep it!
										}
										//errMsg("performCoarsening"," CHECK "<<PRINT_VEC(dstni,dstnj,dstnk)<<" to "<<PRINT_VEC( chkni,chknj,chknk )<<" f:"<< convertCellFlagType2String( RFLAG(dstlev, chkni,chknj,chknk, dstFineSet))<<" nbsok"<<delok );
									}
									//errMsg("performCoarsening","CFGrFromFine subcube init unused check l"<<dstlev<<" at "<<PRINT_VEC(dstni,dstnj,dstnk)<<" ok"<<delok );
									if(delok) {
										mNumFsgrChanges++;
										RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet) = CFUnused;
										RFLAG(dstlev, dstni,dstnj,dstnk, mLevel[dstlev].setOther) = CFUnused; // FLAGTEST
										if((D::cDimension==2)&&(debugCoarsening)) debugMarkCell(dstlev,dstni,dstnj,dstnk); 
									}
								}
							} // l
							// treat subcube
							//ebugMarkCell(lev,i+dx,j+dy,k+dz); 
							//if(D::mInitDone) errMsg("performCoarsening","CFGrFromFine subcube init, dir:"<<PRINT_VEC(dx,dy,dz) );
						}
					} } }

				}   // ?
			} // convert regions of from fine
	}}} // TEST!

					// reinit cell area value
					/*if( RFLAG(lev, i,j,k,srcSet) & CFFluid) {
						if( RFLAG(lev+1, i*2,j*2,k*2,dstFineSet) & CFGrFromCoarse) {
							LbmFloat totArea = mFsgrCellArea[0]; // for l=0
							for(int l=1; l<D::cDirNum; l++) { 
								int ni=(2*i)+D::dfVecX[l], nj=(2*j)+D::dfVecY[l], nk=(2*k)+D::dfVecZ[l];
								if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&
										(CFGrFromCoarse|CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
										//(CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
										) { 
									//LbmFloat area = 0.25; if(D::dfVecX[l]!=0) area *= 0.5; if(D::dfVecY[l]!=0) area *= 0.5; if(D::dfVecZ[l]!=0) area *= 0.5;
									totArea += mFsgrCellArea[l];
								}
							} // l
							QCELL(lev, i,j,k,mLevel[lev].setOther, dFlux) = 
							QCELL(lev, i,j,k,srcSet, dFlux) = totArea;
						} else {
							QCELL(lev, i,j,k,mLevel[lev].setOther, dFlux) = 
							QCELL(lev, i,j,k,srcSet, dFlux) = 1.0;
						}
						//errMsg("DFINI"," at l"<<lev<<" "<<PRINT_IJK<<" v:"<<QCELL(lev, i,j,k,srcSet, dFlux) );
					}
				// */

	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {


			if(RFLAG(lev, i,j,k, srcSet) & CFEmpty) {
				// check empty -> from fine conversion
				bool changeToFromFine = false;
				const CellFlagType notAllowed = (CFInter|CFGrFromFine|CFGrToFine);
				CellFlagType reqType = CFGrNorm;
				if(lev+1==mMaxRefine) reqType = CFNoBndFluid;

#if REFINEMENTBORDER==1
				if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & reqType) &&
				    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (notAllowed)) )  ){
					changeToFromFine=true;
				}
			/*if(strstr(D::getName().c_str(),"Debug"))
			if((changeToFromFine)&&(lev+1==mMaxRefine)) { // mixborder
				for(int l=0;((l<D::cDirNum) && (changeToFromFine)); l++) {  // FARBORD
					int ni=2*i+2*D::dfVecX[l], nj=2*j+2*D::dfVecY[l], nk=2*k+2*D::dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&CFBnd) { // NEWREFT
						changeToFromFine=false;
					}
				} 
			}// FARBORD */
#elif REFINEMENTBORDER==2 // REFINEMENTBORDER==1
				if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & reqType) &&
				    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (notAllowed)) )  ){
					changeToFromFine=true;
					for(int l=0; ((l<D::cDirNum)&&(changeToFromFine)); l++) { 
						int ni=2*i+D::dfVecX[l], nj=2*j+D::dfVecY[l], nk=2*k+D::dfVecZ[l];
						if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&(notNbAllowed)) { // NEWREFT
							changeToFromFine=false;
						}
					}
					/*for(int l=0; ((l<D::cDirNum)&&(changeToFromFine)); l++) {  // FARBORD
						int ni=2*i+2*D::dfVecX[l], nj=2*j+2*D::dfVecY[l], nk=2*k+2*D::dfVecZ[l];
						if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&(notNbAllowed)) { // NEWREFT
							changeToFromFine=false;
						}
					} // FARBORD*/
				}
#elif REFINEMENTBORDER==3 // REFINEMENTBORDER==3
				FIX!!!
				if(lev+1==mMaxRefine) { // mixborder
					if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (CFFluid|CFInter)) &&
							(!(RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (notAllowed)) )  ){
						changeToFromFine=true;
					}
				} else {
				if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (CFFluid)) &&
				    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (notAllowed)) )  ){
					changeToFromFine=true;
					for(int l=0; l<D::cDirNum; l++) { 
						int ni=2*i+D::dfVecX[l], nj=2*j+D::dfVecY[l], nk=2*k+D::dfVecZ[l];
						if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&(notNbAllowed)) { // NEWREFT
							changeToFromFine=false;
						}
					}
				} } // mixborder
#else // REFINEMENTBORDER==3
				ERROR
#endif // REFINEMENTBORDER==1
				if(changeToFromFine) {
					change = true;
					mNumFsgrChanges++;
					RFLAG(lev, i,j,k, srcSet) = CFFluid|CFGrFromFine;
					if((D::cDimension==2)&&(debugCoarsening)) debugMarkCell(lev,i,j,k); 
					// same as restr from fine func! not necessary ?!
					// coarseRestrictFromFine part */
				}
			} // only check empty cells

	}}} // TEST!

	if(!D::mSilent){ errMsg("performCoarsening"," for l"<<lev<<" done " ); }
	return change;
}


/*****************************************************************************/
/*! perform a single LBM step */
/*****************************************************************************/
template<class D>
void 
LbmFsgrSolver<D>::adaptTimestep()
{
	LbmFloat massTOld=0.0, massTNew=0.0;
	LbmFloat volTOld=0.0, volTNew=0.0;

	bool rescale = false;  // do any rescale at all?
	LbmFloat scaleFac = -1.0; // timestep scaling

	LbmFloat levOldOmega[MAX_LEV];
	LbmFloat levOldStepsize[MAX_LEV];
	for(int lev=mMaxRefine; lev>=0 ; lev--) {
		levOldOmega[lev] = mLevel[lev].omega;
		levOldStepsize[lev] = mLevel[lev].stepsize;
	}
	//if(mTimeSwitchCounts>0){ errMsg("DEB CSKIP",""); return; } // DEBUG

	LbmFloat fac = 0.8;          // modify time step by 20%, TODO? do multiple times for large changes?
	LbmFloat diffPercent = 0.05; // dont scale if less than 5%
	LbmFloat allowMax = D::mpParam->getTadapMaxSpeed();  // maximum allowed velocity
	LbmFloat nextmax = D::mpParam->getSimulationMaxSpeed() + norm(mLevel[mMaxRefine].gravity);

	//newdt = D::mpParam->getStepTime() * (allowMax/nextmax);
	LbmFloat newdt = D::mpParam->getStepTime(); // newtr
	if(nextmax>allowMax/fac) {
		newdt = D::mpParam->getStepTime() * fac;
	} else {
		if(nextmax<allowMax*fac) {
			newdt = D::mpParam->getStepTime() / fac;
		}
	} // newtr
	//errMsg("LbmFsgrSolver::adaptTimestep","nextmax="<<nextmax<<" allowMax="<<allowMax<<" fac="<<fac<<" simmaxv="<< D::mpParam->getSimulationMaxSpeed() );

	bool minCutoff = false;
	LbmFloat desireddt = newdt;
	if(newdt>D::mpParam->getMaxStepTime()){ newdt = D::mpParam->getMaxStepTime(); }
	if(newdt<D::mpParam->getMinStepTime()){ 
		newdt = D::mpParam->getMinStepTime(); 
		if(nextmax>allowMax/fac){	minCutoff=true; } // only if really large vels...
	}

	LbmFloat dtdiff = fabs(newdt - D::mpParam->getStepTime());
	if(!D::mSilent) {
		debMsgStd("LbmFsgrSolver::TAdp",DM_MSG, "new"<<newdt<<" max"<<D::mpParam->getMaxStepTime()<<" min"<<D::mpParam->getMinStepTime()<<" diff"<<dtdiff<<
			" simt:"<<mSimulationTime<<" minsteps:"<<(mSimulationTime/mMaxStepTime)<<" maxsteps:"<<(mSimulationTime/mMinStepTime) , 10); }

	// in range, and more than X% change?
	//if( newdt <  D::mpParam->getStepTime() ) // DEBUG
	LbmFloat rhoAvg = mCurrentMass/mCurrentVolume;
	if( (newdt<=D::mpParam->getMaxStepTime()) && (newdt>=D::mpParam->getMinStepTime()) 
			&& (dtdiff>(D::mpParam->getStepTime()*diffPercent)) ) {
		if((newdt>levOldStepsize[mMaxRefine])&&(mTimestepReduceLock)) {
			// wait some more...
			//debMsgNnl("LbmFsgrSolver::TAdp",DM_NOTIFY," Delayed... "<<mTimestepReduceLock<<" ",10);
			debMsgDirect("D");
		} else {
			D::mpParam->setDesiredStepTime( newdt );
			rescale = true;
			if(!D::mSilent) {
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"\n\n\n\n",10);
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"Timestep change: new="<<newdt<<" old="<<D::mpParam->getStepTime()<<" maxSpeed:"<<D::mpParam->getSimulationMaxSpeed()<<" next:"<<nextmax<<" step:"<<D::mStepCnt, 10 );
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"Timestep change: "<<
						"rhoAvg="<<rhoAvg<<" cMass="<<mCurrentMass<<" cVol="<<mCurrentVolume,10);
			}
		} // really change dt
	}

	if(mTimestepReduceLock>0) mTimestepReduceLock--;

	
	/*
	// forced back and forth switchting (for testing)
	const int tadtogInter = 300;
	const double tadtogSwitch = 0.66;
	errMsg("TIMESWITCHTOGGLETEST","warning enabled "<< tadtogSwitch<<","<<tadtogSwitch<<" !!!!!!!!!!!!!!!!!!!");
	if( ((D::mStepCnt% tadtogInter)== (tadtogInter/4*1)-1) ||
	    ((D::mStepCnt% tadtogInter)== (tadtogInter/4*2)-1) ){
		rescale = true; minCutoff = false;
		newdt = tadtogSwitch * D::mpParam->getStepTime();
		D::mpParam->setDesiredStepTime( newdt );
	} else 
	if( ((D::mStepCnt% tadtogInter)== (tadtogInter/4*3)-1) ||
	    ((D::mStepCnt% tadtogInter)== (tadtogInter/4*4)-1) ){
		rescale = true; minCutoff = false;
		newdt = D::mpParam->getStepTime()/tadtogSwitch ;
		D::mpParam->setDesiredStepTime( newdt );
	} else {
		rescale = false; minCutoff = false;
	}
	// */

	// test mass rescale

	scaleFac = newdt/D::mpParam->getStepTime();
	if(rescale) {
		// fixme - warum y, wird jetzt gemittelt...
		mTimestepReduceLock = 4*(mLevel[mMaxRefine].lSizey+mLevel[mMaxRefine].lSizez+mLevel[mMaxRefine].lSizex)/3;

		mTimeSwitchCounts++;
		D::mpParam->calculateAllMissingValues( D::mSilent );
		recalculateObjectSpeeds();
		// calc omega, force for all levels
		initLevelOmegas();
		if(D::mpParam->getStepTime()<mMinStepTime) mMinStepTime = D::mpParam->getStepTime();
		if(D::mpParam->getStepTime()>mMaxStepTime) mMaxStepTime = D::mpParam->getStepTime();

		for(int lev=mMaxRefine; lev>=0 ; lev--) {
			LbmFloat newSteptime = mLevel[lev].stepsize;
			LbmFloat dfScaleFac = (newSteptime/1.0)/(levOldStepsize[lev]/levOldOmega[lev]);

			if(!D::mSilent) {
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"Level: "<<lev<<" Timestep change: "<<
						" scaleFac="<<dfScaleFac<<" newDt="<<newSteptime<<" newOmega="<<mLevel[lev].omega,10);
			}
			if(lev!=mMaxRefine) coarseCalculateFluxareas(lev);

			int wss = 0, wse = 1;
			// FIXME always currset!?
			wss = wse = mLevel[lev].setCurr;
			for(int workSet = wss; workSet<=wse; workSet++) { // COMPRT
					// warning - check sets for higher levels...?
				FSGR_FORIJK1(lev) {
					if( 
							(RFLAG(lev,i,j,k, workSet) & CFFluid) || 
							(RFLAG(lev,i,j,k, workSet) & CFInter) ||
							(RFLAG(lev,i,j,k, workSet) & CFGrFromCoarse) || 
							(RFLAG(lev,i,j,k, workSet) & CFGrFromFine) || 
							(RFLAG(lev,i,j,k, workSet) & CFGrNorm) 
							) {
						// these cells have to be scaled...
					} else {
						continue;
					}

					// collide on current set
					LbmFloat rhoOld;
					LbmVec velOld;
					LbmFloat rho, ux,uy,uz;
					rho=0.0; ux =  uy = uz = 0.0;
					for(int l=0; l<D::cDfNum; l++) {
						LbmFloat m = QCELL(lev, i, j, k, workSet, l); 
						rho += m;
						ux  += (D::dfDvecX[l]*m);
						uy  += (D::dfDvecY[l]*m); 
						uz  += (D::dfDvecZ[l]*m); 
					} 
					rhoOld = rho;
					velOld = LbmVec(ux,uy,uz);

					LbmFloat rhoNew = (rhoOld-rhoAvg)*scaleFac +rhoAvg;
					LbmVec velNew = velOld * scaleFac;

					LbmFloat df[LBM_DFNUM];
					LbmFloat feqOld[LBM_DFNUM];
					LbmFloat feqNew[LBM_DFNUM];
					for(int l=0; l<D::cDfNum; l++) {
						feqOld[l] = D::getCollideEq(l,rhoOld, velOld[0],velOld[1],velOld[2] );
						feqNew[l] = D::getCollideEq(l,rhoNew, velNew[0],velNew[1],velNew[2] );
						df[l] = QCELL(lev, i,j,k,workSet, l);
					}
					const LbmFloat Qo = D::getLesNoneqTensorCoeff(df,feqOld);
					const LbmFloat oldOmega = D::getLesOmega(levOldOmega[lev], mLevel[lev].lcsmago,Qo);
					const LbmFloat newOmega = D::getLesOmega(mLevel[lev].omega,mLevel[lev].lcsmago,Qo);
					//newOmega = mLevel[lev].omega; // FIXME debug test

					//LbmFloat dfScaleFac = (newSteptime/1.0)/(levOldStepsize[lev]/levOldOmega[lev]);
					const LbmFloat dfScale = (newSteptime/newOmega)/(levOldStepsize[lev]/oldOmega);
					//dfScale = dfScaleFac/newOmega;
					
					for(int l=0; l<D::cDfNum; l++) {
						// org scaling
						//df = eqOld + (df-eqOld)*dfScale; df *= (eqNew/eqOld); // non-eq. scaling, important
						// new scaling
						LbmFloat dfn = feqNew[l] + (df[l]-feqOld[l])*dfScale*feqNew[l]/feqOld[l]; // non-eq. scaling, important
						//df = eqNew + (df-eqOld)*dfScale; // modified ig scaling, no real difference?
						QCELL(lev, i,j,k,workSet, l) = dfn;
					}

					if(RFLAG(lev,i,j,k, workSet) & CFInter) {
						//if(workSet==mLevel[lev].setCurr) 
						LbmFloat area = 1.0;
						if(lev!=mMaxRefine) area = QCELL(lev, i,j,k,workSet, dFlux);
						massTOld += QCELL(lev, i,j,k,workSet, dMass) * area;
						volTOld += QCELL(lev, i,j,k,workSet, dFfrac);

						// wrong... QCELL(i,j,k,workSet, dMass] = (QCELL(i,j,k,workSet, dFfrac]*rhoNew);
						QCELL(lev, i,j,k,workSet, dMass) = (QCELL(lev, i,j,k,workSet, dMass)/rhoOld*rhoNew);
						QCELL(lev, i,j,k,workSet, dFfrac) = (QCELL(lev, i,j,k,workSet, dMass)/rhoNew);

						//if(workSet==mLevel[lev].setCurr) 
						massTNew += QCELL(lev, i,j,k,workSet, dMass);
						volTNew += QCELL(lev, i,j,k,workSet, dFfrac);
					}
					if(RFLAG(lev,i,j,k, workSet) & CFFluid) { // DEBUG
						if(RFLAG(lev,i,j,k, workSet) & (CFGrFromFine|CFGrFromCoarse)) { // DEBUG
							// dont include 
						} else {
							LbmFloat area = 1.0;
							if(lev!=mMaxRefine) area = QCELL(lev, i,j,k,workSet, dFlux) * mLevel[lev].lcellfactor;
							//if(workSet==mLevel[lev].setCurr) 
							massTOld += rhoOld*area;
							//if(workSet==mLevel[lev].setCurr) 
							massTNew += rhoNew*area;
							volTOld += area;
							volTNew += area;
						}
					}

				} // IJK
			} // workSet

		} // lev

		if(!D::mSilent) {
			debMsgStd("LbmFsgrSolver::step",DM_MSG,"REINIT DONE "<<D::mStepCnt<<
					" no"<<mTimeSwitchCounts<<" maxdt"<<mMaxStepTime<<
					" mindt"<<mMinStepTime<<" currdt"<<mLevel[mMaxRefine].stepsize, 10);
			debMsgStd("LbmFsgrSolver::step",DM_MSG,"REINIT DONE  masst:"<<massTNew<<","<<massTOld<<" org:"<<mCurrentMass<<"; "<<
					" volt:"<<volTNew<<","<<volTOld<<" org:"<<mCurrentVolume, 10);
		} else {
			debMsgStd("\nLbmOptSolver::step",DM_MSG,"Timestep change by "<< (newdt/levOldStepsize[mMaxRefine]) <<" newDt:"<<newdt
					<<", oldDt:"<<levOldStepsize[mMaxRefine]<<" newOmega:"<<D::mOmega<<" gStar:"<<D::mpParam->getCurrentGStar() , 10);
		}
	} // rescale?
	
	//errMsg("adaptTimestep","Warning - brute force rescale off!"); minCutoff = false; // DEBUG
	if(minCutoff) {
		errMsg("adaptTimestep","Warning - performing Brute-Force rescale... (sim:"<<D::mName<<" step:"<<D::mStepCnt<<" newdt="<<desireddt<<" mindt="<<D::mpParam->getMinStepTime()<<") " );
		//brute force resacle all the time?

		for(int lev=mMaxRefine; lev>=0 ; lev--) {
		int rescs=0;
		int wss = 0, wse = 1;
#if COMPRESSGRIDS==1
		if(lev== mMaxRefine) wss = wse = mLevel[lev].setCurr;
#endif // COMPRESSGRIDS==1
		for(int workSet = wss; workSet<=wse; workSet++) { // COMPRT
		//for(int workSet = 0; workSet<=1; workSet++) {
		FSGR_FORIJK1(lev) {

			//if( (RFLAG(lev, i,j,k, workSet) & CFFluid) || (RFLAG(lev, i,j,k, workSet) & CFInter) ) {
			if( 
					(RFLAG(lev,i,j,k, workSet) & CFFluid) || 
					(RFLAG(lev,i,j,k, workSet) & CFInter) ||
					(RFLAG(lev,i,j,k, workSet) & CFGrFromCoarse) || 
					(RFLAG(lev,i,j,k, workSet) & CFGrFromFine) || 
					(RFLAG(lev,i,j,k, workSet) & CFGrNorm) 
					) {
				// these cells have to be scaled...
			} else {
				continue;
			}

			// collide on current set
			LbmFloat rho, ux,uy,uz;
			rho=0.0; ux =  uy = uz = 0.0;
			for(int l=0; l<D::cDfNum; l++) {
				LbmFloat m = QCELL(lev, i, j, k, workSet, l); 
				rho += m;
				ux  += (D::dfDvecX[l]*m);
				uy  += (D::dfDvecY[l]*m); 
				uz  += (D::dfDvecZ[l]*m); 
			} 
#ifndef WIN32
			if (!finite(rho)) {
				errMsg("adaptTimestep","Brute force non-finite rho at"<<PRINT_IJK);  // DEBUG!
				rho = 1.0;
				ux = uy = uz = 0.0;
				QCELL(lev, i, j, k, workSet, dMass) = 1.0;
				QCELL(lev, i, j, k, workSet, dFfrac) = 1.0;
			}
#endif // WIN32

			if( (ux*ux+uy*uy+uz*uz)> (allowMax*allowMax) ) {
				LbmFloat cfac = allowMax/sqrt(ux*ux+uy*uy+uz*uz);
				ux *= cfac;
				uy *= cfac;
				uz *= cfac;
				for(int l=0; l<D::cDfNum; l++) {
					QCELL(lev, i, j, k, workSet, l) = D::getCollideEq(l, rho, ux,uy,uz); }
				rescs++;
				debMsgDirect("B");
			}

		} } 
			//if(rescs>0) { errMsg("adaptTimestep","!!!!! Brute force rescaling was necessary !!!!!!!"); }
			debMsgStd("adaptTimestep",DM_MSG,"Brute force rescale done. level:"<<lev<<" rescs:"<<rescs, 1);
		//TTT mNumProblems += rescs; // add to problem display...
		} // lev,set,ijk

	} // try brute force rescale?

	// time adap done...
}




/******************************************************************************
 * work on lists from updateCellMass to reinit cell flags
 *****************************************************************************/

template<class D>
LbmFloat 
LbmFsgrSolver<D>::getMassdWeight(bool dirForw, int i,int j,int k,int workSet, int l) {
	//return 0.0; // test
	int level = mMaxRefine;
	LbmFloat *ccel = RACPNT(level, i,j,k, workSet);

	LbmFloat nx,ny,nz, nv1,nv2;
	if(RFLAG_NB(level,i,j,k,workSet, dE) &(CFFluid|CFInter)){ nv1 = RAC((ccel+QCELLSTEP ),dFfrac); } else nv1 = 0.0;
	if(RFLAG_NB(level,i,j,k,workSet, dW) &(CFFluid|CFInter)){ nv2 = RAC((ccel-QCELLSTEP ),dFfrac); } else nv2 = 0.0;
	nx = 0.5* (nv2-nv1);
	if(RFLAG_NB(level,i,j,k,workSet, dN) &(CFFluid|CFInter)){ nv1 = RAC((ccel+(mLevel[level].lOffsx*QCELLSTEP)),dFfrac); } else nv1 = 0.0;
	if(RFLAG_NB(level,i,j,k,workSet, dS) &(CFFluid|CFInter)){ nv2 = RAC((ccel-(mLevel[level].lOffsx*QCELLSTEP)),dFfrac); } else nv2 = 0.0;
	ny = 0.5* (nv2-nv1);
#if LBMDIM==3
	if(RFLAG_NB(level,i,j,k,workSet, dT) &(CFFluid|CFInter)){ nv1 = RAC((ccel+(mLevel[level].lOffsy*QCELLSTEP)),dFfrac); } else nv1 = 0.0;
	if(RFLAG_NB(level,i,j,k,workSet, dB) &(CFFluid|CFInter)){ nv2 = RAC((ccel-(mLevel[level].lOffsy*QCELLSTEP)),dFfrac); } else nv2 = 0.0;
	nz = 0.5* (nv2-nv1);
#else //LBMDIM==3
	nz = 0.0;
#endif //LBMDIM==3
	LbmFloat scal = mDvecNrm[l][0]*nx + mDvecNrm[l][1]*ny + mDvecNrm[l][2]*nz;

	LbmFloat ret = 1.0;
	// forward direction, add mass (for filling cells):
	if(dirForw) {
		if(scal<LBM_EPSILON) ret = 0.0;
		else ret = scal;
	} else {
		// backward for emptying
		if(scal>-LBM_EPSILON) ret = 0.0;
		else ret = scal * -1.0;
	}
	//errMsg("massd", PRINT_IJK<<" nv"<<nvel<<" : ret="<<ret ); //xit(1); //VECDEB
	return ret;
}

template<class D>
void LbmFsgrSolver<D>::addToNewInterList( int ni, int nj, int nk ) {
#if FSGR_STRICT_DEBUG==10
	// dangerous, this can change the simulation...
  /*for( vector<LbmPoint>::iterator iter=mListNewInter.begin();
       iter != mListNewInter.end(); iter++ ) {
    if(ni!=iter->x) continue;
    if(nj!=iter->y) continue;
    if(nk!=iter->z) continue;
		// all 3 values match... skip point
		return;
	} */
#endif // FSGR_STRICT_DEBUG==1
	// store point
	LbmPoint newinter;
	newinter.x = ni; newinter.y = nj; newinter.z = nk;
	mListNewInter.push_back(newinter);
}


// WOXDY_N = Weight Order X Dimension Y _ number N
#define WO1D1   ( 1.0/ 2.0)
#define WO1D2   ( 1.0/ 4.0)
#define WO1D3   ( 1.0/ 8.0)

#define WO2D1_1 (-1.0/16.0)
#define WO2D1_9 ( 9.0/16.0)

#define WO2D2_11 (WO2D1_1 * WO2D1_1)
#define WO2D2_19 (WO2D1_9 * WO2D1_1)
#define WO2D2_91 (WO2D1_9 * WO2D1_1)
#define WO2D2_99 (WO2D1_9 * WO2D1_9)

#define WO2D3_111 (WO2D1_1 * WO2D1_1 * WO2D1_1)
#define WO2D3_191 (WO2D1_9 * WO2D1_1 * WO2D1_1)
#define WO2D3_911 (WO2D1_9 * WO2D1_1 * WO2D1_1)
#define WO2D3_991 (WO2D1_9 * WO2D1_9 * WO2D1_1)
#define WO2D3_119 (WO2D1_1 * WO2D1_1 * WO2D1_9)
#define WO2D3_199 (WO2D1_9 * WO2D1_1 * WO2D1_9)
#define WO2D3_919 (WO2D1_9 * WO2D1_1 * WO2D1_9)
#define WO2D3_999 (WO2D1_9 * WO2D1_9 * WO2D1_9)

#if FSGR_STRICT_DEBUG==1
#define ADD_INT_DFSCHECK(alev, ai,aj,ak, at, afac, l) \
				if(	(((1.0-(at))>0.0) && (!(QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr , l) > -1.0 ))) || \
						(((    (at))>0.0) && (!(QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setOther, l) > -1.0 ))) ){ \
					errMsg("INVDFSCHECK", " l"<<(alev)<<" "<<PRINT_VEC((ai),(aj),(ak))<<" fc:"<<RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr )<<" fo:"<<RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther )<<" dfl"<<l ); \
					debugMarkCell((alev), (ai),(aj),(ak));\
					D::mPanic = 1; \
				}
				// end ADD_INT_DFSCHECK
#define ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac) \
				if(	(((1.0-(at))>0.0) && (!(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr )&(CFInter|CFFluid|CFGrCoarseInited) ))) || \
						(((    (at))>0.0) && (!(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther)&(CFInter|CFFluid|CFGrCoarseInited) ))) ){ \
					errMsg("INVFLAGCINTCHECK", " l"<<(alev)<<" at:"<<(at)<<" "<<PRINT_VEC((ai),(aj),(ak))<<\
							" fc:"<<   convertCellFlagType2String(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr  )) <<\
							" fold:"<< convertCellFlagType2String(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther )) ); \
					debugMarkCell((alev), (ai),(aj),(ak));\
					D::mPanic = 1; \
				}
				// end ADD_INT_DFSCHECK
				
				//if(	!(RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) & CFUnused) ){
				//errMsg("INTFLAGUNU", PRINT_VEC(i,j,k)<<" child at "<<PRINT_VEC((ix),(iy),(iz)) );
				//if(iy==15) errMsg("IFFC", PRINT_VEC(i,j,k)<<" child interpolated at "<<PRINT_VEC((ix),(iy),(iz)) );
				//if(((ix)>10)&&(iy>5)&&(iz>5)) { debugMarkCell(lev+1, (ix),(iy),(iz) ); }
#define INTUNUTCHECK(ix,iy,iz) \
				if(	(RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) != (CFFluid|CFGrFromCoarse)) ){\
					errMsg("INTFLAGUNU_CHECK", PRINT_VEC(i,j,k)<<" child not unused at l"<<(lev+1)<<" "<<PRINT_VEC((ix),(iy),(iz))<<" flag: "<<  RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) ); \
					debugMarkCell((lev+1), (ix),(iy),(iz));\
					D::mPanic = 1; \
				}\
				RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) |= CFGrCoarseInited; \
				// INTUNUTCHECK 
#define INTSTRICTCHECK(ix,iy,iz,caseId) \
				if(	QCELL(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr, l) <= 0.0 ){\
					errMsg("INVDFCCELLCHECK", "caseId:"<<caseId<<" "<<PRINT_VEC(i,j,k)<<" child inter at "<<PRINT_VEC((ix),(iy),(iz))<<" invalid df "<<l<<" = "<< QCELL(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr, l) ); \
					debugMarkCell((lev+1), (ix),(iy),(iz));\
					D::mPanic = 1; \
				}\
				// INTSTRICTCHECK

#else// FSGR_STRICT_DEBUG==1
#define ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac) 
#define ADD_INT_DFSCHECK(alev, ai,aj,ak, at, afac, l) 
#define INTSTRICTCHECK(x,y,z,caseId) 
#define INTUNUTCHECK(ix,iy,iz) 
#endif// FSGR_STRICT_DEBUG==1


#if FSGR_STRICT_DEBUG==1
#define INTDEBOUT \
		{ /*LbmFloat rho,ux,uy,uz;*/ \
			rho = ux=uy=uz=0.0; \
			FORDF0{ LbmFloat m = QCELL(lev,i,j,k, dstSet, l); \
				rho += m; ux  += (D::dfDvecX[l]*m); uy  += (D::dfDvecY[l]*m); uz  += (D::dfDvecZ[l]*m);  \
				if(ABS(m)>1.0) { errMsg("interpolateCellFromCoarse", "ICFC_DFCHECK cell  "<<PRINT_IJK<<" m"<<l<<":"<< m ); D::mPanic=1; }\
				/*errMsg("interpolateCellFromCoarse", " cell "<<PRINT_IJK<<" df"<<l<<":"<<m );*/ \
			}  \
			/*if(D::mPanic) { errMsg("interpolateCellFromCoarse", "ICFC_DFOUT cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) ); }*/ \
			if(markNbs) errMsg("interpolateCellFromCoarse", " cell "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) );  \
			/*errMsg("interpolateCellFromCoarse", "ICFC_DFDEBUG cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) ); */\
		} \
		/* both cases are ok to interpolate */	\
		if( (!(RFLAG(lev,i,j,k, dstSet) & CFGrFromCoarse)) &&	\
				(!(RFLAG(lev,i,j,k, dstSet) & CFUnused)) ) {	\
			/* might also have CFGrCoarseInited (shouldnt be a problem here)*/	\
			errMsg("interpolateCellFromCoarse", "CHECK cell not CFGrFromCoarse? "<<PRINT_IJK<<" flag:"<< RFLAG(lev,i,j,k, dstSet)<<" fstr:"<<convertCellFlagType2String(  RFLAG(lev,i,j,k, dstSet) ));	\
			/* FIXME check this warning...? return; this can happen !? */	\
			/*D::mPanic = 1;*/	\
		}	\
		// end INTDEBOUT
#else // FSGR_STRICT_DEBUG==1
#define INTDEBOUT 
#endif // FSGR_STRICT_DEBUG==1

	
// t=0.0 -> only current
// t=0.5 -> mix
// t=1.0 -> only other
#if OPT3D==false 
#define ADD_INT_DFS(alev, ai,aj,ak, at, afac) \
						ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac); \
						FORDF0{ \
							LbmFloat df = ( \
									QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr , l)*(1.0-(at)) + \
									QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setOther, l)*(    (at)) \
									) ; \
							ADD_INT_DFSCHECK(alev, ai,aj,ak, at, afac, l); \
							df *= (afac); \
							rho += df;  \
							ux  += (D::dfDvecX[l]*df);  \
							uy  += (D::dfDvecY[l]*df);   \
							uz  += (D::dfDvecZ[l]*df);   \
							intDf[l] += df; \
						} 
// write interpolated dfs back to cell (correct non-eq. parts)
#define IDF_WRITEBACK_ \
		FORDF0{ \
			LbmFloat eq = D::getCollideEq(l, rho,ux,uy,uz);\
			QCELL(lev,i,j,k, dstSet, l) = (eq+ (intDf[l]-eq)*mDfScaleDown);\
		} \
		/* check that all values are ok */ \
		INTDEBOUT
#define IDF_WRITEBACK \
		LbmFloat omegaDst, omegaSrc;\
		/* smago new */ \
		LbmFloat feq[LBM_DFNUM]; \
		LbmFloat dfScale = mDfScaleDown; \
		FORDF0{ \
			feq[l] = D::getCollideEq(l, rho,ux,uy,uz); \
		} \
		if(mLevel[lev  ].lcsmago>0.0) {\
			LbmFloat Qo = D::getLesNoneqTensorCoeff(intDf,feq); \
			omegaDst  = D::getLesOmega(mLevel[lev+0].omega,mLevel[lev+0].lcsmago,Qo); \
			omegaSrc = D::getLesOmega(mLevel[lev-1].omega,mLevel[lev-1].lcsmago,Qo); \
		} else {\
			omegaDst = mLevel[lev+0].omega; \
			omegaSrc = mLevel[lev-1].omega;\
		} \
		 \
		dfScale   = (mLevel[lev+0].stepsize/mLevel[lev-1].stepsize)* (1.0/omegaDst-1.0)/ (1.0/omegaSrc-1.0);  \
		FORDF0{ \
			/*errMsg("SMAGO"," org"<<mDfScaleDown<<" n"<<dfScale<<" qc"<< QCELL(lev,i,j,k, dstSet, l)<<" idf"<<intDf[l]<<" eq"<<feq[l] ); */ \
			QCELL(lev,i,j,k, dstSet, l) = (feq[l]+ (intDf[l]-feq[l])*dfScale);\
		} \
		/* check that all values are ok */ \
		INTDEBOUT

#else //OPT3D==false 

#define ADDALLVALS \
	addVal = addDfFacT * RAC(addfcel , dC ); \
	                                        intDf[dC ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dN ); \
	             uy+=addVal;               intDf[dN ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dS ); \
	             uy-=addVal;               intDf[dS ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dE ); \
	ux+=addVal;                            intDf[dE ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dW ); \
	ux-=addVal;                            intDf[dW ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dT ); \
	                          uz+=addVal;  intDf[dT ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dB ); \
	                          uz-=addVal;  intDf[dB ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNE); \
	ux+=addVal; uy+=addVal;               intDf[dNE] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNW); \
	ux-=addVal; uy+=addVal;               intDf[dNW] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dSE); \
	ux+=addVal; uy-=addVal;               intDf[dSE] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dSW); \
	ux-=addVal; uy-=addVal;               intDf[dSW] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNT); \
	             uy+=addVal; uz+=addVal;  intDf[dNT] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNB); \
	             uy+=addVal; uz-=addVal;  intDf[dNB] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dST); \
	             uy-=addVal; uz+=addVal;  intDf[dST] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dSB); \
	             uy-=addVal; uz-=addVal;  intDf[dSB] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dET); \
	ux+=addVal;              uz+=addVal;  intDf[dET] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dEB); \
	ux+=addVal;              uz-=addVal;  intDf[dEB] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dWT); \
	ux-=addVal;              uz+=addVal;  intDf[dWT] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dWB); \
	ux-=addVal;              uz-=addVal;  intDf[dWB] += addVal; rho += addVal; 

#define ADD_INT_DFS(alev, ai,aj,ak, at, afac) \
	addDfFacT = at*afac; \
	addfcel = RACPNT((alev), (ai),(aj),(ak),mLevel[(alev)].setOther); \
	ADDALLVALS\
	addDfFacT = (1.0-at)*afac; \
	addfcel = RACPNT((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr); \
	ADDALLVALS

// also ugly...
#define INTDF_C    intDf[dC ]
#define INTDF_N    intDf[dN ]
#define INTDF_S    intDf[dS ]
#define INTDF_E    intDf[dE ]
#define INTDF_W    intDf[dW ]
#define INTDF_T    intDf[dT ]
#define INTDF_B    intDf[dB ]
#define INTDF_NE   intDf[dNE]
#define INTDF_NW   intDf[dNW]
#define INTDF_SE   intDf[dSE]
#define INTDF_SW   intDf[dSW]
#define INTDF_NT   intDf[dNT]
#define INTDF_NB   intDf[dNB]
#define INTDF_ST   intDf[dST]
#define INTDF_SB   intDf[dSB]
#define INTDF_ET   intDf[dET]
#define INTDF_EB   intDf[dEB]
#define INTDF_WT   intDf[dWT]
#define INTDF_WB   intDf[dWB]


// write interpolated dfs back to cell (correct non-eq. parts)
#define IDF_WRITEBACK_LES \
		dstcell = RACPNT(lev, i,j,k,dstSet); \
		usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
		\
		lcsmeq[dC] = EQC ; \
		COLL_CALCULATE_DFEQ(lcsmeq); \
		COLL_CALCULATE_NONEQTENSOR(lev, INTDF_ )\
		COLL_CALCULATE_CSMOMEGAVAL(lev+0, lcsmDstOmega); \
		COLL_CALCULATE_CSMOMEGAVAL(lev-1, lcsmSrcOmega); \
		\
		lcsmdfscale   = (mLevel[lev+0].stepsize/mLevel[lev-1].stepsize)* (1.0/lcsmDstOmega-1.0)/ (1.0/lcsmSrcOmega-1.0);  \
		RAC(dstcell, dC ) = (lcsmeq[dC ] + (intDf[dC ]-lcsmeq[dC ] )*lcsmdfscale);\
		RAC(dstcell, dN ) = (lcsmeq[dN ] + (intDf[dN ]-lcsmeq[dN ] )*lcsmdfscale);\
		RAC(dstcell, dS ) = (lcsmeq[dS ] + (intDf[dS ]-lcsmeq[dS ] )*lcsmdfscale);\
		RAC(dstcell, dE ) = (lcsmeq[dE ] + (intDf[dE ]-lcsmeq[dE ] )*lcsmdfscale);\
		RAC(dstcell, dW ) = (lcsmeq[dW ] + (intDf[dW ]-lcsmeq[dW ] )*lcsmdfscale);\
		RAC(dstcell, dT ) = (lcsmeq[dT ] + (intDf[dT ]-lcsmeq[dT ] )*lcsmdfscale);\
		RAC(dstcell, dB ) = (lcsmeq[dB ] + (intDf[dB ]-lcsmeq[dB ] )*lcsmdfscale);\
		RAC(dstcell, dNE) = (lcsmeq[dNE] + (intDf[dNE]-lcsmeq[dNE] )*lcsmdfscale);\
		RAC(dstcell, dNW) = (lcsmeq[dNW] + (intDf[dNW]-lcsmeq[dNW] )*lcsmdfscale);\
		RAC(dstcell, dSE) = (lcsmeq[dSE] + (intDf[dSE]-lcsmeq[dSE] )*lcsmdfscale);\
		RAC(dstcell, dSW) = (lcsmeq[dSW] + (intDf[dSW]-lcsmeq[dSW] )*lcsmdfscale);\
		RAC(dstcell, dNT) = (lcsmeq[dNT] + (intDf[dNT]-lcsmeq[dNT] )*lcsmdfscale);\
		RAC(dstcell, dNB) = (lcsmeq[dNB] + (intDf[dNB]-lcsmeq[dNB] )*lcsmdfscale);\
		RAC(dstcell, dST) = (lcsmeq[dST] + (intDf[dST]-lcsmeq[dST] )*lcsmdfscale);\
		RAC(dstcell, dSB) = (lcsmeq[dSB] + (intDf[dSB]-lcsmeq[dSB] )*lcsmdfscale);\
		RAC(dstcell, dET) = (lcsmeq[dET] + (intDf[dET]-lcsmeq[dET] )*lcsmdfscale);\
		RAC(dstcell, dEB) = (lcsmeq[dEB] + (intDf[dEB]-lcsmeq[dEB] )*lcsmdfscale);\
		RAC(dstcell, dWT) = (lcsmeq[dWT] + (intDf[dWT]-lcsmeq[dWT] )*lcsmdfscale);\
		RAC(dstcell, dWB) = (lcsmeq[dWB] + (intDf[dWB]-lcsmeq[dWB] )*lcsmdfscale);\
		/* IDF_WRITEBACK optimized */

#define IDF_WRITEBACK_NOLES \
		dstcell = RACPNT(lev, i,j,k,dstSet); \
		usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
		\
		RAC(dstcell, dC ) = (EQC  + (intDf[dC ]-EQC  )*mDfScaleDown);\
		RAC(dstcell, dN ) = (EQN  + (intDf[dN ]-EQN  )*mDfScaleDown);\
		RAC(dstcell, dS ) = (EQS  + (intDf[dS ]-EQS  )*mDfScaleDown);\
		/*old*/ RAC(dstcell, dE ) = (EQE  + (intDf[dE ]-EQE  )*mDfScaleDown);\
		RAC(dstcell, dW ) = (EQW  + (intDf[dW ]-EQW  )*mDfScaleDown);\
		RAC(dstcell, dT ) = (EQT  + (intDf[dT ]-EQT  )*mDfScaleDown);\
		RAC(dstcell, dB ) = (EQB  + (intDf[dB ]-EQB  )*mDfScaleDown);\
		/*old*/ RAC(dstcell, dNE) = (EQNE + (intDf[dNE]-EQNE )*mDfScaleDown);\
		RAC(dstcell, dNW) = (EQNW + (intDf[dNW]-EQNW )*mDfScaleDown);\
		RAC(dstcell, dSE) = (EQSE + (intDf[dSE]-EQSE )*mDfScaleDown);\
		RAC(dstcell, dSW) = (EQSW + (intDf[dSW]-EQSW )*mDfScaleDown);\
		RAC(dstcell, dNT) = (EQNT + (intDf[dNT]-EQNT )*mDfScaleDown);\
		RAC(dstcell, dNB) = (EQNB + (intDf[dNB]-EQNB )*mDfScaleDown);\
		RAC(dstcell, dST) = (EQST + (intDf[dST]-EQST )*mDfScaleDown);\
		RAC(dstcell, dSB) = (EQSB + (intDf[dSB]-EQSB )*mDfScaleDown);\
		RAC(dstcell, dET) = (EQET + (intDf[dET]-EQET )*mDfScaleDown);\
		/*old*/ RAC(dstcell, dEB) = (EQEB + (intDf[dEB]-EQEB )*mDfScaleDown);\
		RAC(dstcell, dWT) = (EQWT + (intDf[dWT]-EQWT )*mDfScaleDown);\
		RAC(dstcell, dWB) = (EQWB + (intDf[dWB]-EQWB )*mDfScaleDown);\
		/* IDF_WRITEBACK optimized */

#if USE_LES==1
#define IDF_WRITEBACK IDF_WRITEBACK_LES
#else 
#define IDF_WRITEBACK IDF_WRITEBACK_NOLES
#endif

#endif// OPT3D==false 

template<class D>
void LbmFsgrSolver<D>::interpolateCellFromCoarse(int lev, int i, int j,int k, int dstSet, LbmFloat t, CellFlagType flagSet, bool markNbs) {
	//errMsg("INV DEBUG REINIT! off!",""); xit(1); // DEBUG quit
	//if(markNbs) errMsg("interpolateCellFromCoarse"," l"<<lev<<" "<<PRINT_VEC(i,j,k)<<" s"<<dstSet<<" t"<<t); //xit(1); // DEBUG quit

	LbmFloat rho=0.0, ux=0.0, uy=0.0, uz=0.0;
	//LbmFloat intDf[LBM_DFNUM];
	// warning only q19 and below!
	LbmFloat intDf[19] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

#if OPT3D==true 
	// for macro add
	LbmFloat addDfFacT, addVal, usqr;
	LbmFloat *addfcel, *dstcell;
	LbmFloat lcsmqadd, lcsmqo, lcsmeq[LBM_DFNUM];
	LbmFloat lcsmDstOmega, lcsmSrcOmega, lcsmdfscale;
#endif // OPT3D==true 

	// SET required nbs to from coarse (this might overwrite flag several times)
	// this is not necessary for interpolateFineFromCoarse
	if(markNbs) {
	FORDF1{ 
		int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
		if(RFLAG(lev,ni,nj,nk,dstSet)&CFUnused) {
			// parents have to be inited!
			interpolateCellFromCoarse(lev, ni, nj, nk, dstSet, t, CFFluid|CFGrFromCoarse, false);
		}
	} }

	// change flag of cell to be interpolated
	RFLAG(lev,i,j,k, dstSet) = flagSet;
	mNumInterdCells++;

	// interpolation lines...
	int betx = i&1;
	int bety = j&1;
	int betz = k&1;
	
	if((!betx) && (!bety) && (!betz)) {
		ADD_INT_DFS(lev-1, i/2  ,j/2  ,k/2  , 0.0, 1.0);

		IDF_WRITEBACK;
		return;
	}

	if(( betx) && (!bety) && (!betz)) {
		//if((i==19)&&(j==14)&&(D::cDimension==2)) { debugMarkCell(lev,i,j,k); debugMarkCell(lev-1, (i/2)-1, (j/2)  , (k/2) ); debugMarkCell(lev-1, (i/2)  , (j/2)  , (k/2) ); debugMarkCell(lev-1, (i/2)+1, (j/2)  , (k/2) ); debugMarkCell(lev-1, (i/2)+2, (j/2)  , (k/2) ); }
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D1);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D1);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)  , (k/2)  , t, WO2D1_1);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)  , (k/2)  , t, WO2D1_9);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)  , (k/2)  , t, WO2D1_9);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)  , (k/2)  , t, WO2D1_1);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	if((!betx) && ( bety) && (!betz)) {
		//if((i==4)&&(j==9)&&(D::cDimension==2)) { debugMarkCell(lev,i,j,k); debugMarkCell(lev-1, (i/2), (j/2)-1  , (k/2) ); debugMarkCell(lev-1, (i/2), (j/2)    , (k/2) ); debugMarkCell(lev-1, (i/2), (j/2)+1  , (k/2) ); debugMarkCell(lev-1, (i/2), (j/2)+2  , (k/2) ); }
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D1);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D1);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, (i/2), (j/2)-1  , (k/2)  , t, WO2D1_1);
		ADD_INT_DFS(lev-1, (i/2), (j/2)    , (k/2)  , t, WO2D1_9);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+1  , (k/2)  , t, WO2D1_9);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+2  , (k/2)  , t, WO2D1_1);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	if((!betx) && (!bety) && ( betz)) {
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D1);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D1);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, i/2  ,j/2  ,k/2-1, t, WO2D1_1);
		ADD_INT_DFS(lev-1, i/2  ,j/2  ,k/2  , t, WO2D1_9);
		ADD_INT_DFS(lev-1, i/2  ,j/2  ,k/2+1, t, WO2D1_9);
		ADD_INT_DFS(lev-1, i/2  ,j/2  ,k/2+2, t, WO2D1_1);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	if(( betx) && ( bety) && (!betz)) {
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)+1,(k/2)  , t, WO1D2);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)-1  , (k/2)  , t, WO2D2_11);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)    , (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+1  , (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+2  , (k/2)  , t, WO2D2_11);

		ADD_INT_DFS(lev-1, (i/2)  , (j/2)-1  , (k/2)  , t, WO2D2_19);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)    , (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+1  , (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+2  , (k/2)  , t, WO2D2_19);

		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)-1  , (k/2)  , t, WO2D2_19);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)    , (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+1  , (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+2  , (k/2)  , t, WO2D2_19);

		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)-1  , (k/2)  , t, WO2D2_11);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)    , (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+1  , (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+2  , (k/2)  , t, WO2D2_11);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	if((!betx) && ( bety) && ( betz)) {
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)+1, t, WO1D2);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, (i/2), (j/2)-1, (k/2)-1, t, WO2D2_11);
		ADD_INT_DFS(lev-1, (i/2), (j/2)-1, (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2), (j/2)-1, (k/2)+1, t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2), (j/2)-1, (k/2)+2, t, WO2D2_11);
                                            
		ADD_INT_DFS(lev-1, (i/2), (j/2)  , (k/2)-1, t, WO2D2_19);
		ADD_INT_DFS(lev-1, (i/2), (j/2)  , (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2), (j/2)  , (k/2)+1, t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2), (j/2)  , (k/2)+2, t, WO2D2_19);
                                            
		ADD_INT_DFS(lev-1, (i/2), (j/2)+1, (k/2)-1, t, WO2D2_19);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+1, (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+1, (k/2)+1, t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+1, (k/2)+2, t, WO2D2_19);
                                            
		ADD_INT_DFS(lev-1, (i/2), (j/2)+2, (k/2)-1, t, WO2D2_11);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+2, (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+2, (k/2)+1, t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2), (j/2)+2, (k/2)+2, t, WO2D2_11);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	if(( betx) && (!bety) && ( betz)) {
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)+1, t, WO1D2);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2), (k/2)-1, t, WO2D2_11);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2), (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2), (k/2)+1, t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2), (k/2)+2, t, WO2D2_11);
                                            
		ADD_INT_DFS(lev-1, (i/2)  , (j/2), (k/2)-1, t, WO2D2_19);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2), (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2), (k/2)+1, t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2), (k/2)+2, t, WO2D2_19);
                                            
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2), (k/2)-1, t, WO2D2_19);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2), (k/2)  , t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2), (k/2)+1, t, WO2D2_99);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2), (k/2)+2, t, WO2D2_19);
                                            
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2), (k/2)-1, t, WO2D2_11);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2), (k/2)  , t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2), (k/2)+1, t, WO2D2_91);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2), (k/2)+2, t, WO2D2_11);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	if(( betx) && ( bety) && ( betz)) {
#if INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)+1, t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)+1,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)+1, t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)+1,(k/2)+1, t, WO1D3);
#else // INTORDER==1
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)-1  , (k/2)-1, t, WO2D3_111);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)    , (k/2)-1, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+1  , (k/2)-1, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+2  , (k/2)-1, t, WO2D3_111);

		ADD_INT_DFS(lev-1, (i/2)  , (j/2)-1  , (k/2)-1, t, WO2D3_191);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)    , (k/2)-1, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+1  , (k/2)-1, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+2  , (k/2)-1, t, WO2D3_191);

		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)-1  , (k/2)-1, t, WO2D3_191);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)    , (k/2)-1, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+1  , (k/2)-1, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+2  , (k/2)-1, t, WO2D3_191);

		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)-1  , (k/2)-1, t, WO2D3_111);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)    , (k/2)-1, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+1  , (k/2)-1, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+2  , (k/2)-1, t, WO2D3_111);
		

		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)-1  , (k/2)  , t, WO2D3_119);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)    , (k/2)  , t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+1  , (k/2)  , t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+2  , (k/2)  , t, WO2D3_119);

		ADD_INT_DFS(lev-1, (i/2)  , (j/2)-1  , (k/2)  , t, WO2D3_199);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)    , (k/2)  , t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+1  , (k/2)  , t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+2  , (k/2)  , t, WO2D3_199);

		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)-1  , (k/2)  , t, WO2D3_199);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)    , (k/2)  , t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+1  , (k/2)  , t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+2  , (k/2)  , t, WO2D3_199);

		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)-1  , (k/2)  , t, WO2D3_119);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)    , (k/2)  , t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+1  , (k/2)  , t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+2  , (k/2)  , t, WO2D3_119);
		

		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)-1  , (k/2)+1, t, WO2D3_119);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)    , (k/2)+1, t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+1  , (k/2)+1, t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+2  , (k/2)+1, t, WO2D3_119);

		ADD_INT_DFS(lev-1, (i/2)  , (j/2)-1  , (k/2)+1, t, WO2D3_199);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)    , (k/2)+1, t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+1  , (k/2)+1, t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+2  , (k/2)+1, t, WO2D3_199);

		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)-1  , (k/2)+1, t, WO2D3_199);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)    , (k/2)+1, t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+1  , (k/2)+1, t, WO2D3_999);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+2  , (k/2)+1, t, WO2D3_199);

		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)-1  , (k/2)+1, t, WO2D3_119);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)    , (k/2)+1, t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+1  , (k/2)+1, t, WO2D3_919);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+2  , (k/2)+1, t, WO2D3_119);
		

		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)-1  , (k/2)+2, t, WO2D3_111);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)    , (k/2)+2, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+1  , (k/2)+2, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)-1, (j/2)+2  , (k/2)+2, t, WO2D3_111);

		ADD_INT_DFS(lev-1, (i/2)  , (j/2)-1  , (k/2)+2, t, WO2D3_191);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)    , (k/2)+2, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+1  , (k/2)+2, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)  , (j/2)+2  , (k/2)+2, t, WO2D3_191);

		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)-1  , (k/2)+2, t, WO2D3_191);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)    , (k/2)+2, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+1  , (k/2)+2, t, WO2D3_991);
		ADD_INT_DFS(lev-1, (i/2)+1, (j/2)+2  , (k/2)+2, t, WO2D3_191);

		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)-1  , (k/2)+2, t, WO2D3_111);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)    , (k/2)+2, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+1  , (k/2)+2, t, WO2D3_911);
		ADD_INT_DFS(lev-1, (i/2)+2, (j/2)+2  , (k/2)+2, t, WO2D3_111);
#endif // INTORDER==1
		IDF_WRITEBACK;
		return;
	}

	D::mPanic=1;
	errFatal("interpolateCellFromCoarse","Invalid!?", SIMWORLD_GENERICERROR);
}

template<class D>
void LbmFsgrSolver<D>::reinitFlags( int workSet )
{
	// OLD mods:
	// add all to intel list?
	// check ffrac for new cells
	// new if cell inits (last loop)
	// vweights handling

#if ELBEEM_BLENDER==1
	const int debugFlagreinit = 0;
#else // ELBEEM_BLENDER==1
	const int debugFlagreinit = 0;
#endif // ELBEEM_BLENDER==1
	
	// some things need to be read/modified on the other set
	int otherSet = (workSet^1);
	// fixed level on which to perform 
	int workLev = mMaxRefine;

  /* modify interface cells from lists */
  /* mark filled interface cells as fluid, or emptied as empty */
	/* count neighbors and distribute excess mass to interface neighbor cells
   * problems arise when there are no interface neighbors anymore
	 * then just distribute to any fluid neighbors...
	 */

	// for symmetry, first init all neighbor cells */
	for( vector<LbmPoint>::iterator iter=mListFull.begin();
       iter != mListFull.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if(debugFlagreinit) errMsg("FULL", PRINT_IJK<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" rho"<< QCELL(workLev, i,j,k, workSet, 0) ); // DEBUG SYMM
    FORDF1 {
			int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      if( RFLAG(workLev, ni,nj,nk, workSet) & CFEmpty ){
				// new and empty interface cell, dont change old flag here!
				addToNewInterList(ni,nj,nk);
				
				// preinit speed, get from average surrounding cells
				// interpolate from non-workset to workset, sets are handled in function
				{
					// WARNING - other i,j,k than filling cell!
					int ei=ni; int ej=nj; int ek=nk;
					LbmFloat avgrho = 0.0;
					LbmFloat avgux = 0.0, avguy = 0.0, avguz = 0.0;
					LbmFloat cellcnt = 0.0;
					LbmFloat avgnbdf[LBM_DFNUM];
					FORDF0M { avgnbdf[m]= 0.0; }

					for(int nbl=1; nbl< D::cDfNum ; ++nbl) {
						if( (RFLAG_NB(workLev,ei,ej,ek,workSet,nbl) & CFFluid) || 
							((!(RFLAG_NB(workLev,ei,ej,ek,workSet,nbl) & CFNoInterpolSrc) ) &&
								(RFLAG_NB(workLev,ei,ej,ek,workSet,nbl) & CFInter) )) { 
							cellcnt += 1.0;
    					for(int rl=0; rl< D::cDfNum ; ++rl) { 
								LbmFloat nbdf =  QCELL_NB(workLev,ei,ej,ek, workSet,nbl, rl);
								avgnbdf[rl] += nbdf;
								avgux  += (D::dfDvecX[rl]*nbdf); 
								avguy  += (D::dfDvecY[rl]*nbdf);  
								avguz  += (D::dfDvecZ[rl]*nbdf);  
								avgrho += nbdf;
							}
						}
					}

					if(cellcnt<=0.0) {
						// no nbs? just use eq.
						//FORDF0 { QCELL(workLev,ei,ej,ek, workSet, l) = D::dfEquil[l]; }
						avgrho = 1.0;
						avgux = avguy = avguz = 0.0;
						//TTT mNumProblems++;
#if ELBEEM_BLENDER!=1
						D::mPanic=1; errFatal("NYI2","cellcnt<=0.0",SIMWORLD_GENERICERROR);
#endif // ELBEEM_BLENDER
					} else {
						// init speed
						avgux /= cellcnt; avguy /= cellcnt; avguz /= cellcnt;
						avgrho /= cellcnt;
						FORDF0M { avgnbdf[m] /= cellcnt; } // CHECK FIXME test?
					}

					// careful with l's...
					FORDF0M { 
						QCELL(workLev,ei,ej,ek, workSet, m) = D::getCollideEq( m,avgrho,  avgux, avguy, avguz ); 
						//QCELL(workLev,ei,ej,ek, workSet, l) = avgnbdf[l]; // CHECK FIXME test?
					}
					//errMsg("FNEW", PRINT_VEC(ei,ej,ek)<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" rho"<<avgrho<<" vel"<<PRINT_VEC(avgux,avguy,avguz) ); // DEBUG SYMM
					QCELL(workLev,ei,ej,ek, workSet, dMass) = 0.0; //?? new
					QCELL(workLev,ei,ej,ek, workSet, dFfrac) = 0.0; //?? new
					//RFLAG(workLev,ei,ej,ek,workSet) = (CellFlagType)(CFInter|CFNoInterpolSrc);
					changeFlag(workLev,ei,ej,ek,workSet, (CFInter|CFNoInterpolSrc));
					if(debugFlagreinit) errMsg("NEWE", PRINT_IJK<<" newif "<<PRINT_VEC(ei,ej,ek)<<" rho"<<avgrho<<" vel("<<avgux<<","<<avguy<<","<<avguz<<") " );
				} 
      }
			/* prevent surrounding interface cells from getting removed as empty cells 
			 * (also cells that are not newly inited) */
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				//RFLAG(workLev,ni,nj,nk, workSet) = (CellFlagType)(RFLAG(workLev,ni,nj,nk, workSet) | CFNoDelete);
				changeFlag(workLev,ni,nj,nk, workSet, (RFLAG(workLev,ni,nj,nk, workSet) | CFNoDelete));
				// also add to list...
				addToNewInterList(ni,nj,nk);
			} // NEW?
    }

		// NEW? no extra loop...
		//RFLAG(workLev,i,j,k, workSet) = CFFluid;
		changeFlag(workLev,i,j,k, workSet,CFFluid);
	}

	/* remove empty interface cells that are not allowed to be removed anyway
	 * this is important, otherwise the dreaded cell-type-flickering can occur! */
  for( vector<LbmPoint>::iterator iter=mListEmpty.begin();
       iter != mListEmpty.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if((RFLAG(workLev,i,j,k, workSet)&(CFInter|CFNoDelete)) == (CFInter|CFNoDelete)) {
			// remove entry
			if(debugFlagreinit) errMsg("EMPT REMOVED!!!", PRINT_IJK<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" rho"<< QCELL(workLev, i,j,k, workSet, 0) ); // DEBUG SYMM
			iter = mListEmpty.erase(iter); 
			iter--; // and continue with next...

			// treat as "new inter"
			addToNewInterList(i,j,k);
		}
	} 


	/* problems arise when adjacent cells empty&fill ->
		 let fill cells+surrounding interface cells have the higher importance */
  for( vector<LbmPoint>::iterator iter=mListEmpty.begin();
       iter != mListEmpty.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if((RFLAG(workLev,i,j,k, workSet)&(CFInter|CFNoDelete)) == (CFInter|CFNoDelete)){ errMsg("A"," ARGHARGRAG "); } // DEBUG
		if(debugFlagreinit) errMsg("EMPT", PRINT_IJK<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" rho"<< QCELL(workLev, i,j,k, workSet, 0) );

		/* set surrounding fluid cells to interface cells */
    FORDF1 {
			int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFFluid){
				// init fluid->interface 
				//RFLAG(workLev,ni,nj,nk, workSet) = (CellFlagType)(CFInter); 
				changeFlag(workLev,ni,nj,nk, workSet, CFInter); 
				/* new mass = current density */
				LbmFloat nbrho = QCELL(workLev,ni,nj,nk, workSet, dC);
    		for(int rl=1; rl< D::cDfNum ; ++rl) { nbrho += QCELL(workLev,ni,nj,nk, workSet, rl); }
				QCELL(workLev,ni,nj,nk, workSet, dMass) =  nbrho; 
				QCELL(workLev,ni,nj,nk, workSet, dFfrac) =  1.0; 

				// store point
				addToNewInterList(ni,nj,nk);
      }
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter){
				// test, also add to list...
				addToNewInterList(ni,nj,nk);
			} // NEW?
    }

		/* for symmetry, set our flag right now */
		//RFLAG(workLev,i,j,k, workSet) = CFEmpty;
		changeFlag(workLev,i,j,k, workSet, CFEmpty);
		// mark cell not be changed mass... - not necessary, not in list anymore anyway!
	} // emptylist


	
	// precompute weights to get rid of order dependancies
	vector<lbmFloatSet> vWeights;
	vWeights.reserve( mListFull.size() + mListEmpty.size() );
	int weightIndex = 0;
  int nbCount = 0;
	LbmFloat nbWeights[LBM_DFNUM];
	LbmFloat nbTotWeights = 0.0;
	for( vector<LbmPoint>::iterator iter=mListFull.begin();
       iter != mListFull.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
    nbCount = 0; nbTotWeights = 0.0;
    FORDF1 {
			int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				nbCount++;
				nbWeights[l] = getMassdWeight(1,i,j,k,workSet,l);
				nbTotWeights += nbWeights[l];
      } else {
				nbWeights[l] = -100.0; // DEBUG;
			}
    }
		if(nbCount>0) { 
			//errMsg("FF  I", PRINT_IJK<<" "<<weightIndex<<" "<<nbTotWeights);
    	vWeights[weightIndex].val[0] = nbTotWeights;
    	FORDF1 { vWeights[weightIndex].val[l] = nbWeights[l]; }
    	vWeights[weightIndex].numNbs = (LbmFloat)nbCount;
		} else { 
    	vWeights[weightIndex].numNbs = 0.0;
		}
		weightIndex++;
	}
  for( vector<LbmPoint>::iterator iter=mListEmpty.begin();
       iter != mListEmpty.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
    nbCount = 0; nbTotWeights = 0.0;
    FORDF1 {
			int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				nbCount++;
				nbWeights[l] = getMassdWeight(0,i,j,k,workSet,l);
				nbTotWeights += nbWeights[l];
      } else {
				nbWeights[l] = -100.0; // DEBUG;
			}
    }
		if(nbCount>0) { 
			//errMsg("EE  I", PRINT_IJK<<" "<<weightIndex<<" "<<nbTotWeights);
    	vWeights[weightIndex].val[0] = nbTotWeights;
    	FORDF1 { vWeights[weightIndex].val[l] = nbWeights[l]; }
    	vWeights[weightIndex].numNbs = (LbmFloat)nbCount;
		} else { 
    	vWeights[weightIndex].numNbs = 0.0;
		}
		weightIndex++;
	} 
	weightIndex = 0;
	

	/* process full list entries, filled cells are done after this loop */
	for( vector<LbmPoint>::iterator iter=mListFull.begin();
       iter != mListFull.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;

		LbmFloat myrho = QCELL(workLev,i,j,k, workSet, dC);
    FORDF1 { myrho += QCELL(workLev,i,j,k, workSet, l); } // QCELL.rho

    LbmFloat massChange = QCELL(workLev,i,j,k, workSet, dMass) - myrho;
    /*int nbCount = 0;
		LbmFloat nbWeights[LBM_DFNUM];
    FORDF1 {
			int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				nbCount++;
				nbWeights[l] = vWeights[weightIndex].val[l];
      } else {
			}
    }*/

		//errMsg("FDIST", PRINT_IJK<<" mss"<<massChange <<" nb"<< nbCount ); // DEBUG SYMM
		if(vWeights[weightIndex].numNbs>0.0) {
			const LbmFloat nbTotWeightsp = vWeights[weightIndex].val[0];
			//errMsg("FF  I", PRINT_IJK<<" "<<weightIndex<<" "<<nbTotWeightsp);
			FORDF1 {
				int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      	if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
					LbmFloat change = -1.0;
					if(nbTotWeightsp>0.0) {
						//change = massChange * ( nbWeights[l]/nbTotWeightsp );
						change = massChange * ( vWeights[weightIndex].val[l]/nbTotWeightsp );
					} else {
						change = (LbmFloat)(massChange/vWeights[weightIndex].numNbs);
					}
					QCELL(workLev,ni,nj,nk, workSet, dMass) += change;
				}
			}
			massChange = 0.0;
		} else {
			// Problem! no interface neighbors
			D::mFixMass += massChange;
			//TTT mNumProblems++;
			//errMsg(" FULL PROBLEM ", PRINT_IJK<<" "<<D::mFixMass);
		}
		weightIndex++;

    // already done? RFLAG(workLev,i,j,k, workSet) = CFFluid;
    QCELL(workLev,i,j,k, workSet, dMass) = myrho; // should be rho... but unused?
    QCELL(workLev,i,j,k, workSet, dFfrac) = 1.0; // should be rho... but unused?
    /*QCELL(workLev,i,j,k, otherSet, dMass) = myrho; // NEW?
    QCELL(workLev,i,j,k, otherSet, dFfrac) = 1.0; // NEW? COMPRT */
  } // fulllist


	/* now, finally handle the empty cells - order is important, has to be after
	 * full cell handling */
  for( vector<LbmPoint>::iterator iter=mListEmpty.begin();
       iter != mListEmpty.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;

    LbmFloat massChange = QCELL(workLev, i,j,k, workSet, dMass);
    /*int nbCount = 0;
		LbmFloat nbWeights[LBM_DFNUM];
    FORDF1 {
			int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				nbCount++;
				nbWeights[l] = vWeights[weightIndex].val[l];
      } else {
				nbWeights[l] = -100.0; // DEBUG;
			}
    }*/

		//errMsg("EDIST", PRINT_IJK<<" mss"<<massChange <<" nb"<< nbCount ); // DEBUG SYMM
		//if(nbCount>0) {
		if(vWeights[weightIndex].numNbs>0.0) {
			const LbmFloat nbTotWeightsp = vWeights[weightIndex].val[0];
			//errMsg("EE  I", PRINT_IJK<<" "<<weightIndex<<" "<<nbTotWeightsp);
			FORDF1 {
				int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
      	if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
					LbmFloat change = -1.0;
					if(nbTotWeightsp>0.0) {
						change = massChange * ( vWeights[weightIndex].val[l]/nbTotWeightsp );
					} else {
						change = (LbmFloat)(massChange/vWeights[weightIndex].numNbs);
					}
					QCELL(workLev, ni,nj,nk, workSet, dMass) += change;
				}
			}
			massChange = 0.0;
		} else {
			// Problem! no interface neighbors
			D::mFixMass += massChange;
			//TTT mNumProblems++;
			//errMsg(" EMPT PROBLEM ", PRINT_IJK<<" "<<D::mFixMass);
		}
		weightIndex++;
		
		// finally... make it empty 
    // already done? RFLAG(workLev,i,j,k, workSet) = CFEmpty;
    QCELL(workLev,i,j,k, workSet, dMass) = 0.0;
    QCELL(workLev,i,j,k, workSet, dFfrac) = 0.0;
	}
  for( vector<LbmPoint>::iterator iter=mListEmpty.begin();
       iter != mListEmpty.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
    //RFLAG(workLev,i,j,k, otherSet) = CFEmpty;
    changeFlag(workLev,i,j,k, otherSet, CFEmpty);
    /*QCELL(workLev,i,j,k, otherSet, dMass) = 0.0;
    QCELL(workLev,i,j,k, otherSet, dFfrac) = 0.0; // COMPRT OFF */
	} 


	// check if some of the new interface cells can be removed again 
	// never happens !!! not necessary
	// calculate ffrac for new IF cells NEW

	// how many are really new interface cells?
	int numNewIf = 0;
  for( vector<LbmPoint>::iterator iter=mListNewInter.begin();
       iter != mListNewInter.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if(!(RFLAG(workLev,i,j,k, workSet)&CFInter)) { 
			continue; 
			// FIXME remove from list?
		}
		numNewIf++;
	}

	// redistribute mass, reinit flags
	float newIfFac = 1.0/(LbmFloat)numNewIf;
  for( vector<LbmPoint>::iterator iter=mListNewInter.begin();
       iter != mListNewInter.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if(!(RFLAG(workLev,i,j,k, workSet)&CFInter)) { 
			//errMsg("???"," "<<PRINT_IJK);
			continue; 
		}

    QCELL(workLev,i,j,k, workSet, dMass) += (D::mFixMass * newIfFac);

		int nbored = 0;
		FORDF1 { nbored |= RFLAG_NB(workLev, i,j,k, workSet,l); }
		if((nbored & CFFluid)==0) { RFLAG(workLev,i,j,k, workSet) |= CFNoNbFluid; }
		if((nbored & CFEmpty)==0) { RFLAG(workLev,i,j,k, workSet) |= CFNoNbEmpty; }

		if(!(RFLAG(workLev,i,j,k, otherSet)&CFInter)) {
			RFLAG(workLev,i,j,k, workSet) = (CellFlagType)(RFLAG(workLev,i,j,k, workSet) | CFNoDelete);
		}
		if(debugFlagreinit) errMsg("NEWIF", PRINT_IJK<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" f"<< RFLAG(workLev,i,j,k, workSet)<<" wl"<<workLev );
	}

	// reinit fill fraction
  for( vector<LbmPoint>::iterator iter=mListNewInter.begin();
       iter != mListNewInter.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if(!(RFLAG(workLev,i,j,k, workSet)&CFInter)) { continue; }

		LbmFloat nrho = 0.0;
		FORDF0 { nrho += QCELL(workLev, i,j,k, workSet, l); }
    QCELL(workLev,i,j,k, workSet, dFfrac) = QCELL(workLev,i,j,k, workSet, dMass)/nrho;
    QCELL(workLev,i,j,k, workSet, dFlux) = FLUX_INIT;
	}

	if(mListNewInter.size()>0){ 
		//errMsg("FixMassDisted"," fm:"<<D::mFixMass<<" nif:"<<mListNewInter.size() );
		D::mFixMass = 0.0; 
	}

	// empty lists for next step
	mListFull.clear();
	mListEmpty.clear();
	mListNewInter.clear();
} // reinitFlags



//! for raytracing
template<class D>
void LbmFsgrSolver<D>::prepareVisualization( void ) {
	int lev = mMaxRefine;
	int workSet = mLevel[lev].setCurr;

	//make same prepareVisualization and getIsoSurface...
#if LBMDIM==2
	// 2d, place in the middle of isofield slice (k=2)
#  define ZKD1 0
	// 2d z offset = 2, lbmGetData adds 1, so use one here
#  define ZKOFF 1
	// reset all values...
	for(int k= 0; k< 5; ++k) 
   for(int j=0;j<mLevel[lev].lSizey-0;j++) 
    for(int i=0;i<mLevel[lev].lSizex-0;i++) {
		*D::mpIso->lbmGetData(i,j,ZKOFF)=0.0;
	}
#else // LBMDIM==2
	// 3d, use normal bounds
#  define ZKD1 1
#  define ZKOFF k
	// reset all values...
	for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
   for(int j=0;j<mLevel[lev].lSizey-0;j++) 
    for(int i=0;i<mLevel[lev].lSizex-0;i++) {
		*D::mpIso->lbmGetData(i,j,ZKOFF)=0.0;
	}
#endif // LBMDIM==2


	
	// add up...
	float val = 0.0;
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) 
   for(int j=1;j<mLevel[lev].lSizey-1;j++) 
    for(int i=1;i<mLevel[lev].lSizex-1;i++) {

		//continue; // OFF DEBUG
		if(RFLAG(lev, i,j,k,workSet)&(CFBnd|CFEmpty)) {
			continue;
		} else
		if( (RFLAG(lev, i,j,k,workSet)&CFInter) && (!(RFLAG(lev, i,j,k,workSet)&CFNoNbEmpty)) ){
			// no empty nb interface cells are treated as full
			val =  (QCELL(lev, i,j,k,workSet, dFfrac)); 
		} else {
			// fluid?
			val = 1.0; ///27.0;
		} // */

		*D::mpIso->lbmGetData( i-1 , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[0] ); 
		*D::mpIso->lbmGetData( i   , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[1] ); 
		*D::mpIso->lbmGetData( i+1 , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[2] ); 
										
		*D::mpIso->lbmGetData( i-1 , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[3] ); 
		*D::mpIso->lbmGetData( i   , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[4] ); 
		*D::mpIso->lbmGetData( i+1 , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[5] ); 
										
		*D::mpIso->lbmGetData( i-1 , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[6] ); 
		*D::mpIso->lbmGetData( i   , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[7] ); 
		*D::mpIso->lbmGetData( i+1 , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[8] ); 
										
										
		*D::mpIso->lbmGetData( i-1 , j-1  ,ZKOFF  ) += ( val * mIsoWeight[9] ); 
		*D::mpIso->lbmGetData( i   , j-1  ,ZKOFF  ) += ( val * mIsoWeight[10] ); 
		*D::mpIso->lbmGetData( i+1 , j-1  ,ZKOFF  ) += ( val * mIsoWeight[11] ); 
																	
		*D::mpIso->lbmGetData( i-1 , j    ,ZKOFF  ) += ( val * mIsoWeight[12] ); 
		*D::mpIso->lbmGetData( i   , j    ,ZKOFF  ) += ( val * mIsoWeight[13] ); 
		*D::mpIso->lbmGetData( i+1 , j    ,ZKOFF  ) += ( val * mIsoWeight[14] ); 
																	
		*D::mpIso->lbmGetData( i-1 , j+1  ,ZKOFF  ) += ( val * mIsoWeight[15] ); 
		*D::mpIso->lbmGetData( i   , j+1  ,ZKOFF  ) += ( val * mIsoWeight[16] ); 
		*D::mpIso->lbmGetData( i+1 , j+1  ,ZKOFF  ) += ( val * mIsoWeight[17] ); 
										
										
		*D::mpIso->lbmGetData( i-1 , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[18] ); 
		*D::mpIso->lbmGetData( i   , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[19] ); 
		*D::mpIso->lbmGetData( i+1 , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[20] ); 
																 
		*D::mpIso->lbmGetData( i-1 , j   ,ZKOFF+ZKD1) += ( val * mIsoWeight[21] ); 
		*D::mpIso->lbmGetData( i   , j   ,ZKOFF+ZKD1)+= ( val * mIsoWeight[22] ); 
		*D::mpIso->lbmGetData( i+1 , j   ,ZKOFF+ZKD1) += ( val * mIsoWeight[23] ); 
																 
		*D::mpIso->lbmGetData( i-1 , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[24] ); 
		*D::mpIso->lbmGetData( i   , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[25] ); 
		*D::mpIso->lbmGetData( i+1 , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[26] ); 
	}

	// update preview, remove 2d?
	if(mOutputSurfacePreview) {
		//int previewSize = mOutputSurfacePreview;
		int pvsx = (int)(mPreviewFactor*D::mSizex);
		int pvsy = (int)(mPreviewFactor*D::mSizey);
		int pvsz = (int)(mPreviewFactor*D::mSizez);
		//float scale = (float)D::mSizex / previewSize;
		LbmFloat scalex = (LbmFloat)D::mSizex/(LbmFloat)pvsx;
		LbmFloat scaley = (LbmFloat)D::mSizey/(LbmFloat)pvsy;
		LbmFloat scalez = (LbmFloat)D::mSizez/(LbmFloat)pvsz;
		for(int k= 0; k< ((D::cDimension==3) ? (pvsz-1):1) ; ++k) 
   		for(int j=0;j< pvsy;j++) 
    		for(int i=0;i< pvsx;i++) {
					*mpPreviewSurface->lbmGetData(i,j,k) = *D::mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley), (int)(k*scalez) );
				}
		// set borders again...
		for(int k= 0; k< ((D::cDimension == 3) ? (pvsz-1):1) ; ++k) {
			for(int j=0;j< pvsy;j++) {
				*mpPreviewSurface->lbmGetData(0,j,k) = *D::mpIso->lbmGetData( 0, (int)(j*scaley), (int)(k*scalez) );
				*mpPreviewSurface->lbmGetData(pvsx-1,j,k) = *D::mpIso->lbmGetData( D::mSizex-1, (int)(j*scaley), (int)(k*scalez) );
			}
			for(int i=0;i< pvsx;i++) {
				*mpPreviewSurface->lbmGetData(i,0,k) = *D::mpIso->lbmGetData( (int)(i*scalex), 0, (int)(k*scalez) );
				*mpPreviewSurface->lbmGetData(i,pvsy-1,k) = *D::mpIso->lbmGetData( (int)(i*scalex), D::mSizey-1, (int)(k*scalez) );
			}
		}
		if(D::cDimension == 3) {
			// only for 3D
			for(int j=0;j<pvsy;j++)
				for(int i=0;i<pvsx;i++) {      
					*mpPreviewSurface->lbmGetData(i,j,0) = *D::mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley) , 0);
					*mpPreviewSurface->lbmGetData(i,j,pvsz-1) = *D::mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley) , D::mSizez-1);
				}
		} // borders done...
	}

	// correction
	return;
}


/*****************************************************************************
 * demo functions
 *****************************************************************************/

template<class D>
float LbmFsgrSolver<D>::getFillFrac(int i, int j, int k) {
	return QCELL(mMaxRefine, i,j,k,mLevel[mMaxRefine].setOther, dFfrac);
}

template<class D>
void LbmFsgrSolver<D>::getIsofieldWeighted(float *iso) {
	//errMsg("XxxX", " "<<(      szx+ISOCORR) );
	float val;
	int szx = mLevel[mMaxRefine].lSizex;
	int szy = mLevel[mMaxRefine].lSizey;
	int szz = mLevel[mMaxRefine].lSizez;
	int oz = (szx+ISOCORR)*(szy+ISOCORR);
	int oy = (szx+ISOCORR);
	//int wcnt = 0;

	// reset all values...
	const LbmFloat initVal = -0.42;
  for(int i=0;i<(szz+ISOCORR)*(szy+ISOCORR)*(szx+ISOCORR);i++) { 
		iso[i] = initVal;
	}

	// add up...
	FSGR_FORIJK1(mMaxRefine) {
		if(RFLAG(mMaxRefine, i,j,k,mLevel[mMaxRefine].setOther)&CFFluid) {
			val = 1.0; ///27.0;
		} else
		//if(RFLAG(mMaxRefine, i,j,k,mLevel[mMaxRefine].setOther)&CFBnd) {
			//continue;
			//val = initVal + 0.05;
			//val = (initVal + 0.05) * -1.0;
		//} else
		if(RFLAG(mMaxRefine, i,j,k,mLevel[mMaxRefine].setOther)&CFInter) {
			val =  (QCELL(mMaxRefine, i,j,k,mLevel[mMaxRefine].setOther, dFfrac)); // (1.0/27.0);
		} else {
			// bnd, empty, etc...
			continue;
		}

		/*
		// */

		const int index =((k)+1)*oz + ((j)+1)*oy + (i)+1;
		iso[ index -oz -oy -1 ] += ( val * mIsoWeight[0] ); 
		iso[ index -oz -oy    ] += ( val * mIsoWeight[1] ); 
		iso[ index -oz -oy +1 ] += ( val * mIsoWeight[2] ); 

		iso[ index -oz     -1 ] += ( val * mIsoWeight[3] ); 
		iso[ index -oz        ] += ( val * mIsoWeight[4] ); 
		iso[ index -oz     +1 ] += ( val * mIsoWeight[5] ); 

		iso[ index -oz +oy -1 ] += ( val * mIsoWeight[6] ); 
		iso[ index -oz +oy    ] += ( val * mIsoWeight[7] ); 
		iso[ index -oz +oy +1 ] += ( val * mIsoWeight[8] ); 


		iso[ index     -oy -1 ] += ( val * mIsoWeight[9] ); 
		iso[ index     -oy    ] += ( val * mIsoWeight[10] ); 
		iso[ index     -oy +1 ] += ( val * mIsoWeight[11] ); 

		iso[ index         -1 ] += ( val * mIsoWeight[12] ); 
		iso[ index            ] += ( val * mIsoWeight[13] ); 
		iso[ index         +1 ] += ( val * mIsoWeight[14] ); 

		iso[ index     +oy -1 ] += ( val * mIsoWeight[15] ); 
		iso[ index     +oy    ] += ( val * mIsoWeight[16] ); 
		iso[ index     +oy +1 ] += ( val * mIsoWeight[17] ); 


		iso[ index +oz -oy -1 ] += ( val * mIsoWeight[18] ); 
		iso[ index +oz -oy    ] += ( val * mIsoWeight[19] ); 
		iso[ index +oz -oy +1 ] += ( val * mIsoWeight[20] ); 

		iso[ index +oz     -1 ] += ( val * mIsoWeight[21] ); 
		iso[ index +oz        ] += ( val * mIsoWeight[22] ); 
		iso[ index +oz     +1 ] += ( val * mIsoWeight[23] ); 

		iso[ index +oz +oy -1 ] += ( val * mIsoWeight[24] ); 
		iso[ index +oz +oy    ] += ( val * mIsoWeight[25] ); 
		iso[ index +oz +oy +1 ] += ( val * mIsoWeight[26] ); 
	}

	return;
}

template<class D>
void LbmFsgrSolver<D>::addDrop(bool active, float mx, float my) {
	mDropping = active;
	mDropX = mx;
	mDropY = my;
}

template<class D>
void LbmFsgrSolver<D>::initDrop(float mx, float my) {
	// invert for convenience
	mx = 1.0-mx;
	int workSet = mLevel[mMaxRefine].setCurr;

	int px = (int)(mLevel[mMaxRefine].lSizex * mx);
	int py = (int)(mLevel[mMaxRefine].lSizey * mDropHeight);
	int pz = (int)(mLevel[mMaxRefine].lSizez * my);
	int rad = (int)(mDropSize*mLevel[mMaxRefine].lSizex) + 1;
	//errMsg("Rad", " "<<rad);

	// check bounds
	const int offset = 1;
	const float forceFill = 1.0;
	if( (px-rad)<=offset     ) px = rad + offset;
	if( (px+rad)>=mLevel[mMaxRefine].lSizex-1 ) px = mLevel[mMaxRefine].lSizex-offset-rad-1;
	if( (py-rad)<=offset     ) py = rad + offset;
	if( (py+rad)>=mLevel[mMaxRefine].lSizey-1 ) py = mLevel[mMaxRefine].lSizey-offset-rad-1;
	if( (pz-rad)<=offset     ) pz = rad + offset;
	if( (pz+rad)>=mLevel[mMaxRefine].lSizez-1 ) pz = mLevel[mMaxRefine].lSizez-offset-rad-1;

	mUpdateFVHeight=true;
	//errMsg("T", " \n\n\n"<<D::mStepCnt<<" \n\n\n");
	if(mDropMode==-1) { return; }

	if(mDropMode==0) {
		// inflow
		if((py-4)<=offset) py = 4+offset;
		//errMsg(" ", " py"<<py<<" "<<mDropHeight );
		for(int k= pz-rad; k<= pz+rad; k++) {
			for(int j= py-1; j<= py+1; j++) {
				for(int i= px-rad; i<= px+rad; i++) {
					float dz = pz-k;
					//float dy = py-j;
					float dx = px-i;
					if( (dx*dx+dz*dz) > (float)(rad*rad) ) continue;
					LbmFloat fill = forceFill; //rad - sqrt(dx*dx+dz*dz);

					if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFFluid)) {
					} else if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFInter)) {
						if(QCELL(mMaxRefine, i,j,k,workSet, dMass) < 0.75) {
							initVelocityCell(mMaxRefine, i,j,k, CFInter, 1.0, fill, mDropSpeed);
						}
					} else if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFEmpty)) {
						initVelocityCell(mMaxRefine, i,j,k, CFInter, 1.0, fill, mDropSpeed);
					} else {
					}

				}
			}
		}

		// stream - continue dropping
		return;

	} else 
	if( (mDropMode==1) || (mDropMode==2) ) {
		mDropping = false;

		// mode 2 - only single drops
		if(mDropMode==2) {
		for(int k= pz-rad-offset; k<= pz+rad+offset; k++) 
			for(int j= py-rad-offset; j<= py+rad+offset; j++) 
				for(int i= px-rad-offset; i<= px+rad+offset; i++) {
					// make sure no fluid cells near...
					if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFBnd)) continue;
					if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFEmpty)) continue;
					if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFInter)) continue;
					return;
				}
		}

		// single drops
		for(int k= pz-rad-offset; k<= pz+rad+offset; k++) 
			for(int j= py-rad-offset; j<= py+rad+offset; j++) 
				for(int i= px-rad-offset; i<= px+rad+offset; i++) {
					if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFBnd)) continue;
					float dz = pz-k;
					float dy = py-j;
					float dx = px-i;
					if( dx*dx+dy*dy+dz*dz > (float)(rad*rad) ) {
						if(mDropMode==1) {
							// overwrite everything...
							initEmptyCell(mMaxRefine, i,j,k, CFEmpty, 0.0, 0.0);
						}
						continue;
					}
					LbmFloat fill = rad - sqrt(dx*dx+dy*dy+dz*dz);
					if(fill > 1.0) fill = 1.0;

					initEmptyCell(mMaxRefine, i,j,k, CFFluid, 1.0, fill);
				} 

		for(int k= pz-rad-offset-1; k<= pz+rad+offset+1; k++) 
			for(int j= py-rad-offset-1; j<= py+rad+offset+1; j++) 
				for(int i= px-rad-offset-1; i<= px+rad+offset+1; i++) {
					if(i<1) continue;
					if(i>=(mLevel[mMaxRefine].lSizex-2) ) continue;
					if(j<1) continue;
					if(j>=(mLevel[mMaxRefine].lSizey-2) ) continue;
					if(k<1) continue;
					if(k>=(mLevel[mMaxRefine].lSizez-2) ) continue;
					if(RFLAG(mMaxRefine, i,j,k,workSet)&(CFBnd)) continue;

					if( (RFLAG(mMaxRefine, i,j,k,workSet )&(CFFluid)) ) {
						bool emptyNb = false;
						FORDF1 {
							int ni=i+D::dfVecX[l], nj=j+D::dfVecY[l], nk=k+D::dfVecZ[l];
							if(RFLAG(mMaxRefine, ni,nj,nk, workSet) & CFEmpty ){
								emptyNb = true;
							}
						}
						if(emptyNb) {
							RFLAG(mMaxRefine, i,j,k,workSet ) = CFInter;
						}
					}
				} 
	} // single drop
}

		//! avg. used cell count stats
template<class D>
void LbmFsgrSolver<D>::printCellStats() {
	debMsgStd("CellStats", DM_NOTIFY, "Min:"<<mMinNoCells<<" Max:"<<mMaxNoCells<<
			" Avg:"<< (int)(mAvgNumUsedCells/(long long int)D::mStepCnt) , 1);
}
template<class D>
int LbmFsgrSolver<D>::checkGfxEndTime() {
	//errMsg("LbmFsgrSolver","GfxEndTime "<<mGfxEndTime<<" "<<mSimulationTime<<" steps:"<<D::mStepCnt);
	//errMsg(" "," e"<<mGfxEndTime<<" s"<<mSimulationTime);
	if((mGfxEndTime>0.0) && (mSimulationTime>mGfxEndTime)) { 
		errMsg("LbmFsgrSolver","GfxEndTime "<<mSimulationTime<<" steps:"<<D::mStepCnt);
		return true;
	}
	return false;
}




/*****************************************************************************
 * move the particles
 * uses updated velocities from mSetOther
 *****************************************************************************/
template<class D>
void LbmFsgrSolver<D>::advanceParticles(ParticleTracer *partt ) { 
	partt = NULL; // remove warning
}


/******************************************************************************
 * reset particle positions to default
 *****************************************************************************/
/*! init particle positions */
template<class D>
int LbmFsgrSolver<D>::initParticles(ParticleTracer *partt) { 
	partt = NULL; // remove warning
	return 0;
}


/*! init particle positions */
template<class D>
void LbmFsgrSolver<D>::recalculateObjectSpeeds() {
	int numobjs = (int)(D::mpGiObjects->size());
	if(numobjs>255) {
		errFatal("LbmFsgrSolver::recalculateObjectSpeeds","More than 256 objects currently not supported...",SIMWORLD_INITERROR);
		return;
	}
	mObjectSpeeds.resize(numobjs+0);
	for(int i=0; i<(int)(numobjs+0); i++) {
		//errMsg("recalculateObjectSpeeds","id"<<i<<" "<<vec2L(D::mpParam->calculateLattVelocityFromRw( vec2P( (*D::mpGiObjects)[i]->getInitialVelocity() )) ));
		mObjectSpeeds[i] = vec2L(D::mpParam->calculateLattVelocityFromRw( vec2P( (*D::mpGiObjects)[i]->getInitialVelocity() )));
	}
}

/*****************************************************************************/
/*! internal quick print function (for debugging) */
/*****************************************************************************/
template<class D>
void 
LbmFsgrSolver<D>::printLbmCell(int level, int i, int j, int k, int set) {
	stdCellId *newcid = new stdCellId;
	newcid->level = level;
	newcid->x = i;
	newcid->y = j;
	newcid->z = k;

	// this function is not called upon clicking, then its from setMouseClick
	::debugPrintNodeInfo< LbmFsgrSolver<D> >( this, newcid, D::mNodeInfoString, set );

	delete newcid;
}
template<class D>
void 
LbmFsgrSolver<D>::debugMarkCellCall(int level, int vi,int vj,int vk) {
	stdCellId *newcid = new stdCellId;
	newcid->level = level;
	newcid->x = vi;
	newcid->y = vj;
	newcid->z = vk;
	addCellToMarkedList( newcid );
}

		
/*****************************************************************************/
// implement CellIterator<UniformFsgrCellIdentifier> interface
/*****************************************************************************/



// values from guiflkt.cpp
extern double guiRoiSX, guiRoiSY, guiRoiSZ, guiRoiEX, guiRoiEY, guiRoiEZ;
extern int guiRoiMaxLev, guiRoiMinLev;
#define CID_SX (int)( (mLevel[cid->level].lSizex-1) * guiRoiSX )
#define CID_SY (int)( (mLevel[cid->level].lSizey-1) * guiRoiSY )
#define CID_SZ (int)( (mLevel[cid->level].lSizez-1) * guiRoiSZ )

#define CID_EX (int)( (mLevel[cid->level].lSizex-1) * guiRoiEX )
#define CID_EY (int)( (mLevel[cid->level].lSizey-1) * guiRoiEY )
#define CID_EZ (int)( (mLevel[cid->level].lSizez-1) * guiRoiEZ )

template<class D>
CellIdentifierInterface* 
LbmFsgrSolver<D>::getFirstCell( ) {
	int level = mMaxRefine;

#if LBMDIM==3
	if(mMaxRefine>0) { level = mMaxRefine-1; } // NO1HIGHESTLEV DEBUG
#endif
	level = guiRoiMaxLev;
	if(level>mMaxRefine) level = mMaxRefine;
	
	//errMsg("LbmFsgrSolver::getFirstCell","Celliteration started...");
	stdCellId *cid = new stdCellId;
	cid->level = level;
	cid->x = CID_SX;
	cid->y = CID_SY;
	cid->z = CID_SZ;
	return cid;
}

template<class D>
typename LbmFsgrSolver<D>::stdCellId* 
LbmFsgrSolver<D>::convertBaseCidToStdCid( CellIdentifierInterface* basecid) {
	//stdCellId *cid = dynamic_cast<stdCellId*>( basecid );
	stdCellId *cid = (stdCellId*)( basecid );
	return cid;
}

template<class D>
void 
LbmFsgrSolver<D>::advanceCell( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	if(cid->getEnd()) return;

	//debugOut(" ADb "<<cid->x<<","<<cid->y<<","<<cid->z<<" e"<<cid->getEnd(), 10);
	cid->x++;
	if(cid->x > CID_EX){ cid->x = CID_SX; cid->y++; 
		if(cid->y > CID_EY){ cid->y = CID_SY; cid->z++; 
			if(cid->z > CID_EZ){ 
				cid->level--;
				cid->x = CID_SX; 
				cid->y = CID_SY; 
				cid->z = CID_SZ; 
				if(cid->level < guiRoiMinLev) {
					cid->level = guiRoiMaxLev;
					cid->setEnd( true );
				}
			}
		}
	}
	//debugOut(" ADa "<<cid->x<<","<<cid->y<<","<<cid->z<<" e"<<cid->getEnd(), 10);
}

template<class D>
bool 
LbmFsgrSolver<D>::noEndCell( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return (!cid->getEnd());
}

template<class D>
void 
LbmFsgrSolver<D>::deleteCellIterator( CellIdentifierInterface** cid ) {
	delete *cid;
	*cid = NULL;
}

template<class D>
CellIdentifierInterface* 
LbmFsgrSolver<D>::getCellAt( ntlVec3Gfx pos ) {
	//int cellok = false;
	pos -= (D::mvGeoStart);

	LbmFloat mmaxsize = mLevel[mMaxRefine].nodeSize;
	for(int level=mMaxRefine; level>=0; level--) { // finest first
	//for(int level=0; level<=mMaxRefine; level++) { // coarsest first
		LbmFloat nsize = mLevel[level].nodeSize;
		int x,y,z;
		//LbmFloat nsize = getCellSize(NULL)[0]*2.0;
		x = (int)((pos[0]-0.5*mmaxsize) / nsize );
		y = (int)((pos[1]-0.5*mmaxsize) / nsize );
		z = (int)((pos[2]-0.5*mmaxsize) / nsize );
		if(D::cDimension==2) z = 0;

		// double check...
		//int level = mMaxRefine;
		if(x<0) continue;
		if(y<0) continue;
		if(z<0) continue;
		if(x>=mLevel[level].lSizex) continue;
		if(y>=mLevel[level].lSizey) continue;
		if(z>=mLevel[level].lSizez) continue;

		/*if( (RFLAG(level, x,y,z, mLevel[level].setCurr)&(CFFluid|CFInter)) ){
			// ok...
		} else {
			// comment out to always retrieve finest cells ( not working )
			continue;
		}
		// O */

		// only return fluid/if cells 
		/*if( (RFLAG(level, x,y,z, mLevel[level].setCurr)&(CFUnused|CFGrFromFine|CFGrFromCoarse)) ){
			continue;
		} // */

		// return fluid/if/border cells
		if( ( (RFLAG(level, x,y,z, mLevel[level].setCurr)&(CFUnused)) ) ||
			  ( (level<mMaxRefine) && (RFLAG(level, x,y,z, mLevel[level].setCurr)&(CFUnused|CFEmpty)) ) ) {
			continue;
		} // */

		stdCellId *newcid = new stdCellId;
		newcid->level = level;
		newcid->x = x;
		newcid->y = y;
		newcid->z = z;
		//errMsg("cellAt",D::mName<<" "<<pos<<" l"<<level<<":"<<x<<","<<y<<","<<z<<" "<<convertCellFlagType2String(RFLAG(level, x,y,z, mLevel[level].setCurr)) );

		return newcid;
	}

	return NULL;
}


// INFO functions

template<class D>
int      
LbmFsgrSolver<D>::getCellSet      ( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return mLevel[cid->level].setCurr;
	//return mLevel[cid->level].setOther;
}

template<class D>
int      
LbmFsgrSolver<D>::getCellLevel    ( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return cid->level;
}

template<class D>
ntlVec3Gfx   
LbmFsgrSolver<D>::getCellOrigin   ( CellIdentifierInterface* basecid) {
	ntlVec3Gfx ret;

	stdCellId *cid = convertBaseCidToStdCid(basecid);
	ntlVec3Gfx cs( mLevel[cid->level].nodeSize );
	if(D::cDimension==2) { cs[2] = 0.0; }

	if(D::cDimension==2) {
		ret =(D::mvGeoStart -(cs*0.5) + ntlVec3Gfx( cid->x *cs[0], cid->y *cs[1], (D::mvGeoEnd[2]-D::mvGeoStart[2])*0.5 )
				+ ntlVec3Gfx(0.0,0.0,cs[1]*-0.25)*cid->level )
			+getCellSize(basecid);
	} else {
		ret =(D::mvGeoStart -(cs*0.5) + ntlVec3Gfx( cid->x *cs[0], cid->y *cs[1], cid->z *cs[2] ))
			+getCellSize(basecid);
	}
	return (ret);
}

template<class D>
ntlVec3Gfx   
LbmFsgrSolver<D>::getCellSize     ( CellIdentifierInterface* basecid) {
	// return half size
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	ntlVec3Gfx retvec( mLevel[cid->level].nodeSize * 0.5 );
	// 2d display as rectangles
	if(D::cDimension==2) { retvec[2] = 0.0; }
	return (retvec);
}

template<class D>
LbmFloat 
LbmFsgrSolver<D>::getCellDensity  ( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);

	LbmFloat rho = 0.0;
	FORDF0 {
		rho += QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
	}
	return ((rho-1.0) * mLevel[cid->level].simCellSize / mLevel[cid->level].stepsize) +1.0; // normal
	//return ((rho-1.0) * D::mpParam->getCellSize() / D::mpParam->getStepTime()) +1.0;
}

template<class D>
LbmVec   
LbmFsgrSolver<D>::getCellVelocity ( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);

	LbmFloat ux,uy,uz;
	ux=uy=uz= 0.0;
	FORDF0 {
		ux += D::dfDvecX[l]* QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
		uy += D::dfDvecY[l]* QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
		uz += D::dfDvecZ[l]* QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
	}
	LbmVec vel(ux,uy,uz);
	// TODO fix...
	return (vel * mLevel[cid->level].simCellSize / mLevel[cid->level].stepsize * D::mDebugVelScale); // normal
}

template<class D>
LbmFloat   
LbmFsgrSolver<D>::getCellDf( CellIdentifierInterface* basecid,int set, int dir) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return QCELL(cid->level, cid->x,cid->y,cid->z, set, dir);
}
template<class D>
LbmFloat   
LbmFsgrSolver<D>::getCellMass( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return QCELL(cid->level, cid->x,cid->y,cid->z, set, dMass);
}
template<class D>
LbmFloat   
LbmFsgrSolver<D>::getCellFill( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&CFInter) return QCELL(cid->level, cid->x,cid->y,cid->z, set, dFfrac);
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&CFFluid) return 1.0;
	return 0.0;
	//return QCELL(cid->level, cid->x,cid->y,cid->z, set, dFfrac);
}
template<class D>
CellFlagType 
LbmFsgrSolver<D>::getCellFlag( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return RFLAG(cid->level, cid->x,cid->y,cid->z, set);
}

template<class D>
LbmFloat 
LbmFsgrSolver<D>::getEquilDf( int l ) {
	return D::dfEquil[l];
}

template<class D>
int 
LbmFsgrSolver<D>::getDfNum( ) {
	return D::cDfNum;
}

#if LBM_USE_GUI==1
//! show simulation info (implement SimulationObject pure virtual func)
template<class D>
void 
LbmFsgrSolver<D>::debugDisplay(fluidDispSettings *set){ 
	lbmDebugDisplay< LbmFsgrSolver<D> >( set, this ); 
}
#endif

/*****************************************************************************/
// strict debugging functions
/*****************************************************************************/
#if FSGR_STRICT_DEBUG==1
#define STRICT_EXIT *((int *)0)=0;

template<class D>
int LbmFsgrSolver<D>::debLBMGI(int level, int ii,int ij,int ik, int is) {
	if(level <  0){ errMsg("LbmStrict::debLBMGI"," invLev- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 
	if(level >  mMaxRefine){ errMsg("LbmStrict::debLBMGI"," invLev+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 

	if(ii<0){ errMsg("LbmStrict"," invX- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ij<0){ errMsg("LbmStrict"," invY- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik<0){ errMsg("LbmStrict"," invZ- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ii>mLevel[level].lSizex-1){ errMsg("LbmStrict"," invX+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ij>mLevel[level].lSizey-1){ errMsg("LbmStrict"," invY+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik>mLevel[level].lSizez-1){ errMsg("LbmStrict"," invZ+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is<0){ errMsg("LbmStrict"," invS- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is>1){ errMsg("LbmStrict"," invS+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	return _LBMGI(level, ii,ij,ik, is);
};

template<class D>
CellFlagType& LbmFsgrSolver<D>::debRFLAG(int level, int xx,int yy,int zz,int set){
	return _RFLAG(level, xx,yy,zz,set);   
};

template<class D>
CellFlagType& LbmFsgrSolver<D>::debRFLAG_NB(int level, int xx,int yy,int zz,int set, int dir) {
	if(dir<0)         { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	// warning might access all spatial nbs
	if(dir>D::cDirNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _RFLAG_NB(level, xx,yy,zz,set, dir);
};

template<class D>
CellFlagType& LbmFsgrSolver<D>::debRFLAG_NBINV(int level, int xx,int yy,int zz,int set, int dir) {
	if(dir<0)         { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	if(dir>D::cDirNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _RFLAG_NBINV(level, xx,yy,zz,set, dir);
};

template<class D>
int LbmFsgrSolver<D>::debLBMQI(int level, int ii,int ij,int ik, int is, int l) {
	if(level <  0){ errMsg("LbmStrict::debLBMQI"," invLev- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 
	if(level >  mMaxRefine){ errMsg("LbmStrict::debLBMQI"," invLev+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 

	if(ii<0){ errMsg("LbmStrict"," invX- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ij<0){ errMsg("LbmStrict"," invY- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik<0){ errMsg("LbmStrict"," invZ- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ii>mLevel[level].lSizex-1){ errMsg("LbmStrict"," invX+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ij>mLevel[level].lSizey-1){ errMsg("LbmStrict"," invY+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik>mLevel[level].lSizez-1){ errMsg("LbmStrict"," invZ+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is<0){ errMsg("LbmStrict"," invS- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is>1){ errMsg("LbmStrict"," invS+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(l<0)        { errMsg("LbmStrict"," invD- "<<" l"<<l); STRICT_EXIT; }
	if(l>D::cDfNum){  // dFfrac is an exception
		if((l != dMass) && (l != dFfrac) && (l != dFlux)){ errMsg("LbmStrict"," invD+ "<<" l"<<l); STRICT_EXIT; } }
#if COMPRESSGRIDS==1
	//if((!D::mInitDone) && (is!=mLevel[level].setCurr)){ STRICT_EXIT; } // COMPRT debug
#endif // COMPRESSGRIDS==1
	return _LBMQI(level, ii,ij,ik, is, l);
};

template<class D>
LbmFloat& LbmFsgrSolver<D>::debQCELL(int level, int xx,int yy,int zz,int set,int l) {
	//errMsg("LbmStrict","debQCELL debug: l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" l"<<l<<" index"<<LBMGI(level, xx,yy,zz,set)); 
	return _QCELL(level, xx,yy,zz,set,l);
};

template<class D>
LbmFloat& LbmFsgrSolver<D>::debQCELL_NB(int level, int xx,int yy,int zz,int set, int dir,int l) {
	if(dir<0)        { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	if(dir>D::cDfNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _QCELL_NB(level, xx,yy,zz,set, dir,l);
};

template<class D>
LbmFloat& LbmFsgrSolver<D>::debQCELL_NBINV(int level, int xx,int yy,int zz,int set, int dir,int l) {
	if(dir<0)        { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	if(dir>D::cDfNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _QCELL_NBINV(level, xx,yy,zz,set, dir,l);
};

template<class D>
LbmFloat* LbmFsgrSolver<D>::debRACPNT(int level,  int ii,int ij,int ik, int is ) {
	return _RACPNT(level, ii,ij,ik, is );
};

template<class D>
LbmFloat& LbmFsgrSolver<D>::debRAC(LbmFloat* s,int l) {
	if(l<0)        { errMsg("LbmStrict"," invD- "<<" l"<<l); STRICT_EXIT; }
	if(l>dTotalNum){ errMsg("LbmStrict"," invD+ "<<" l"<<l); STRICT_EXIT; } 
	//if(l>D::cDfNum){ // dFfrac is an exception 
	//if((l != dMass) && (l != dFfrac) && (l != dFlux)){ errMsg("LbmStrict"," invD+ "<<" l"<<l); STRICT_EXIT; } }
	return _RAC(s,l);
};

#endif // FSGR_STRICT_DEBUG==1

#define LBMFSGRSOLVER_H
#endif


