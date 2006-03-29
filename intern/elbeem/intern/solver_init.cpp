/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004,2005 Nils Thuerey
 *
 * Standard LBM Factory implementation
 *
 *****************************************************************************/


#include "solver_class.h"
#include "solver_relax.h"
// for geo init FGI_ defines
#include "elbeem.h"

/*****************************************************************************/
//! common variables 

/*****************************************************************************/
/*! 3D implementation D3Q19 */
#if LBMDIM==3

	//! how many dimensions?
	const int LbmFsgrSolver::cDimension = 3;

	// Wi factors for collide step 
	const LbmFloat LbmFsgrSolver::cCollenZero    = (1.0/3.0);
	const LbmFloat LbmFsgrSolver::cCollenOne     = (1.0/18.0);
	const LbmFloat LbmFsgrSolver::cCollenSqrtTwo = (1.0/36.0);

	//! threshold value for filled/emptied cells 
	const LbmFloat LbmFsgrSolver::cMagicNr2    = 1.0005;
	const LbmFloat LbmFsgrSolver::cMagicNr2Neg = -0.0005;
	const LbmFloat LbmFsgrSolver::cMagicNr     = 1.010001;
	const LbmFloat LbmFsgrSolver::cMagicNrNeg  = -0.010001;

	//! size of a single set of distribution functions 
	const int    LbmFsgrSolver::cDfNum      = 19;
	//! direction vector contain vecs for all spatial dirs, even if not used for LBM model
	const int    LbmFsgrSolver::cDirNum     = 27;

	//const string LbmFsgrSolver::dfString[ cDfNum ] = { 
	const char* LbmFsgrSolver::dfString[ cDfNum ] = { 
		" C", " N"," S"," E"," W"," T"," B",
		"NE","NW","SE","SW",
		"NT","NB","ST","SB",
		"ET","EB","WT","WB"
	};

	const int LbmFsgrSolver::dfNorm[ cDfNum ] = { 
		cDirC, cDirN, cDirS, cDirE, cDirW, cDirT, cDirB, 
		cDirNE, cDirNW, cDirSE, cDirSW, 
		cDirNT, cDirNB, cDirST, cDirSB, 
		cDirET, cDirEB, cDirWT, cDirWB
	};

	const int LbmFsgrSolver::dfInv[ cDfNum ] = { 
		cDirC,  cDirS, cDirN, cDirW, cDirE, cDirB, cDirT,
		cDirSW, cDirSE, cDirNW, cDirNE,
		cDirSB, cDirST, cDirNB, cDirNT, 
		cDirWB, cDirWT, cDirEB, cDirET
	};

	const int LbmFsgrSolver::dfRefX[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		cDirSE, cDirSW, cDirNE, cDirNW,
		0, 0, 0, 0, 
		cDirEB, cDirET, cDirWB, cDirWT
	};

	const int LbmFsgrSolver::dfRefY[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		cDirNW, cDirNE, cDirSW, cDirSE,
		cDirNB, cDirNT, cDirSB, cDirST,
		0, 0, 0, 0
	};

	const int LbmFsgrSolver::dfRefZ[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 
		cDirST, cDirSB, cDirNT, cDirNB,
		cDirWT, cDirWB, cDirET, cDirEB
	};

	// Vector Order 3D:
	//  0   1  2   3  4   5  6       7  8  9 10  11 12 13 14  15 16 17 18     19 20 21 22  23 24 25 26
	//  0,  0, 0,  1,-1,  0, 0,      1,-1, 1,-1,  0, 0, 0, 0,  1, 1,-1,-1,     1,-1, 1,-1,  1,-1, 1,-1
	//  0,  1,-1,  0, 0,  0, 0,      1, 1,-1,-1,  1, 1,-1,-1,  0, 0, 0, 0,     1, 1,-1,-1,  1, 1,-1,-1
	//  0,  0, 0,  0, 0,  1,-1,      0, 0, 0, 0,  1,-1, 1,-1,  1,-1, 1,-1,     1, 1, 1, 1, -1,-1,-1,-1

	const int LbmFsgrSolver::dfVecX[ cDirNum ] = { 
		0, 0,0, 1,-1, 0,0,
		1,-1,1,-1,
		0,0,0,0,
		1,1,-1,-1,
		 1,-1, 1,-1,
		 1,-1, 1,-1,
	};
	const int LbmFsgrSolver::dfVecY[ cDirNum ] = { 
		0, 1,-1, 0,0,0,0,
		1,1,-1,-1,
		1,1,-1,-1,
		0,0,0,0,
		 1, 1,-1,-1,
		 1, 1,-1,-1
	};
	const int LbmFsgrSolver::dfVecZ[ cDirNum ] = { 
		0, 0,0,0,0,1,-1,
		0,0,0,0,
		1,-1,1,-1,
		1,-1,1,-1,
		 1, 1, 1, 1,
		-1,-1,-1,-1
	};

	const LbmFloat LbmFsgrSolver::dfDvecX[ cDirNum ] = {
		0, 0,0, 1,-1, 0,0,
		1,-1,1,-1,
		0,0,0,0,
		1,1,-1,-1,
		 1,-1, 1,-1,
		 1,-1, 1,-1
	};
	const LbmFloat LbmFsgrSolver::dfDvecY[ cDirNum ] = {
		0, 1,-1, 0,0,0,0,
		1,1,-1,-1,
		1,1,-1,-1,
		0,0,0,0,
		 1, 1,-1,-1,
		 1, 1,-1,-1
	};
	const LbmFloat LbmFsgrSolver::dfDvecZ[ cDirNum ] = {
		0, 0,0,0,0,1,-1,
		0,0,0,0,
		1,-1,1,-1,
		1,-1,1,-1,
		 1, 1, 1, 1,
		-1,-1,-1,-1
	};

	/* principal directions */
	const int LbmFsgrSolver::princDirX[ 2*LbmFsgrSolver::cDimension ] = { 
		1,-1, 0,0, 0,0
	};
	const int LbmFsgrSolver::princDirY[ 2*LbmFsgrSolver::cDimension ] = { 
		0,0, 1,-1, 0,0
	};
	const int LbmFsgrSolver::princDirZ[ 2*LbmFsgrSolver::cDimension ] = { 
		0,0, 0,0, 1,-1
	};

	/*! arrays for les model coefficients, inited in lbmsolver constructor */
	LbmFloat LbmFsgrSolver::lesCoeffDiag[ (cDimension-1)*(cDimension-1) ][ cDirNum ];
	LbmFloat LbmFsgrSolver::lesCoeffOffdiag[ cDimension ][ cDirNum ];


	const LbmFloat LbmFsgrSolver::dfLength[ cDfNum ]= { 
		cCollenZero,
		cCollenOne, cCollenOne, cCollenOne, 
		cCollenOne, cCollenOne, cCollenOne,
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo
	};

	/* precalculated equilibrium dfs, inited in lbmsolver constructor */
	LbmFloat LbmFsgrSolver::dfEquil[ cDfNum ];

#else // end LBMDIM==3 , LBMDIM==2

/*****************************************************************************/
/*! 2D implementation D2Q9 */

		//! how many dimensions?
		const int LbmFsgrSolver::cDimension = 2;

		//! Wi factors for collide step 
		const LbmFloat LbmFsgrSolver::cCollenZero    = (4.0/9.0);
		const LbmFloat LbmFsgrSolver::cCollenOne     = (1.0/9.0);
		const LbmFloat LbmFsgrSolver::cCollenSqrtTwo = (1.0/36.0);

		//! threshold value for filled/emptied cells 
		const LbmFloat LbmFsgrSolver::cMagicNr2    = 1.0005;
		const LbmFloat LbmFsgrSolver::cMagicNr2Neg = -0.0005;
		const LbmFloat LbmFsgrSolver::cMagicNr     = 1.010001;
		const LbmFloat LbmFsgrSolver::cMagicNrNeg  = -0.010001;

		//! size of a single set of distribution functions 
		const int LbmFsgrSolver::cDfNum  = 9;
		const int LbmFsgrSolver::cDirNum = 9;

	//const string LbmFsgrSolver::dfString[ cDfNum ] = { 
	const char* LbmFsgrSolver::dfString[ cDfNum ] = { 
		" C", 
		" N",	" S", " E", " W",
		"NE", "NW", "SE","SW" 
	};

	const int LbmFsgrSolver::dfNorm[ cDfNum ] = { 
		cDirC, 
		cDirN,  cDirS,  cDirE,  cDirW,
		cDirNE, cDirNW, cDirSE, cDirSW 
	};

	const int LbmFsgrSolver::dfInv[ cDfNum ] = { 
		cDirC,  
		cDirS,  cDirN,  cDirW,  cDirE,
		cDirSW, cDirSE, cDirNW, cDirNE 
	};

	const int LbmFsgrSolver::dfRefX[ cDfNum ] = { 
		0,  
		0,  0,  0,  0,
		cDirSE, cDirSW, cDirNE, cDirNW 
	};

	const int LbmFsgrSolver::dfRefY[ cDfNum ] = { 
		0,  
		0,  0,  0,  0,
		cDirNW, cDirNE, cDirSW, cDirSE 
	};

	const int LbmFsgrSolver::dfRefZ[ cDfNum ] = { 
		0,  0, 0, 0, 0,
		0, 0, 0, 0
	};

	// Vector Order 2D:
	// 0  1 2  3  4  5  6 7  8
	// 0, 0,0, 1,-1, 1,-1,1,-1 
	// 0, 1,-1, 0,0, 1,1,-1,-1 

	const int LbmFsgrSolver::dfVecX[ cDirNum ] = { 
		0, 
		0,0, 1,-1,
		1,-1,1,-1 
	};
	const int LbmFsgrSolver::dfVecY[ cDirNum ] = { 
		0, 
		1,-1, 0,0,
		1,1,-1,-1 
	};
	const int LbmFsgrSolver::dfVecZ[ cDirNum ] = { 
		0, 0,0,0,0, 0,0,0,0 
	};

	const LbmFloat LbmFsgrSolver::dfDvecX[ cDirNum ] = {
		0, 
		0,0, 1,-1,
		1,-1,1,-1 
	};
	const LbmFloat LbmFsgrSolver::dfDvecY[ cDirNum ] = {
		0, 
		1,-1, 0,0,
		1,1,-1,-1 
	};
	const LbmFloat LbmFsgrSolver::dfDvecZ[ cDirNum ] = {
		0, 0,0,0,0, 0,0,0,0 
	};

	const int LbmFsgrSolver::princDirX[ 2*LbmFsgrSolver::cDimension ] = { 
		1,-1, 0,0
	};
	const int LbmFsgrSolver::princDirY[ 2*LbmFsgrSolver::cDimension ] = { 
		0,0, 1,-1
	};
	const int LbmFsgrSolver::princDirZ[ 2*LbmFsgrSolver::cDimension ] = { 
		0,0, 0,0
	};


	/*! arrays for les model coefficients, inited in lbmsolver constructor */
	LbmFloat LbmFsgrSolver::lesCoeffDiag[ (cDimension-1)*(cDimension-1) ][ cDirNum ];
	LbmFloat LbmFsgrSolver::lesCoeffOffdiag[ cDimension ][ cDirNum ];


	const LbmFloat LbmFsgrSolver::dfLength[ cDfNum ]= { 
		cCollenZero,
		cCollenOne, cCollenOne, cCollenOne, cCollenOne, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo
	};

	/* precalculated equilibrium dfs, inited in lbmsolver constructor */
	LbmFloat LbmFsgrSolver::dfEquil[ cDfNum ];

// D2Q9 end
#endif  // LBMDIM==2


/******************************************************************************
 * Lbm Constructor
 *****************************************************************************/
LbmFsgrSolver::LbmFsgrSolver() :
	//D(),
	mCurrentMass(0.0), mCurrentVolume(0.0),
	mNumProblems(0), 
	mAvgMLSUPS(0.0), mAvgMLSUPSCnt(0.0),
	mpPreviewSurface(NULL), 
	mTimeAdap(true), 
	mFVHeight(0.0), mFVArea(1.0), mUpdateFVHeight(false),
	mInitSurfaceSmoothing(0),
	mTimestepReduceLock(0),
	mTimeSwitchCounts(0), mTimeMaxvelStepCnt(0),
	mSimulationTime(0.0), mLastSimTime(0.0),
	mMinTimestep(0.0), mMaxTimestep(0.0),
	mMaxNoCells(0), mMinNoCells(0), mAvgNumUsedCells(0),
	mDropMode(1), mDropSize(0.15), mDropSpeed(0.0),
	mObjectSpeeds(), mObjectPartslips(),
	mIsoWeightMethod(1),
	mMaxRefine(1), 
	mDfScaleUp(-1.0), mDfScaleDown(-1.0),
	mInitialCsmago(0.04), mDebugOmegaRet(0.0),
	mLastOmega(1e10), mLastGravity(1e10),
	mNumInvIfTotal(0), mNumFsgrChanges(0),
	mDisableStandingFluidInit(0),
	mForceTadapRefine(-1), mCutoff(-1)
{
  // not much to do here... 
#if LBM_INCLUDE_TESTSOLVERS==1
	mpTest = new LbmTestdata();
#endif // ELBEEM_PLUGIN!=1
	this->mpIso = new IsoSurface( this->mIsoValue );

  // init equilibrium dist. func 
  LbmFloat rho=1.0;
  FORDF0 {
		this->dfEquil[l] = this->getCollideEq( l,rho,  0.0, 0.0, 0.0);
  }

	// init LES
	int odm = 0;
	for(int m=0; m<LBMDIM; m++) { 
		for(int l=0; l<this->cDfNum; l++) { 
			this->lesCoeffDiag[m][l] = 
			this->lesCoeffOffdiag[m][l] = 0.0;
		}
	}
	for(int m=0; m<LBMDIM; m++) { 
		for(int n=0; n<LBMDIM; n++) { 
			for(int l=1; l<this->cDfNum; l++) { 
				LbmFloat em;
				switch(m) {
					case 0: em = this->dfDvecX[l]; break;
					case 1: em = this->dfDvecY[l]; break;
					case 2: em = this->dfDvecZ[l]; break;
					default: em = -1.0; errFatal("SMAGO1","err m="<<m, SIMWORLD_GENERICERROR);
				}
				LbmFloat en;
				switch(n) {
					case 0: en = this->dfDvecX[l]; break;
					case 1: en = this->dfDvecY[l]; break;
					case 2: en = this->dfDvecZ[l]; break;
					default: en = -1.0; errFatal("SMAGO2","err n="<<n, SIMWORLD_GENERICERROR);
				}
				const LbmFloat coeff = em*en;
				if(m==n) {
					this->lesCoeffDiag[m][l] = coeff;
				} else {
					if(m>n) {
						this->lesCoeffOffdiag[odm][l] = coeff;
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
			LbmVec(this->dfDvecX[this->dfInv[l]], this->dfDvecY[this->dfInv[l]], this->dfDvecZ[this->dfInv[l]] ) 
			) * -1.0; 
	}

	// calculate gauss weights for restriction
	//LbmFloat mGaussw[27];
	LbmFloat totGaussw = 0.0;
	const LbmFloat alpha = 1.0;
	const LbmFloat gw = sqrt(2.0*LBMDIM);
#if ELBEEM_PLUGIN!=1
	errMsg("coarseRestrictFromFine", "TCRFF_DFDEBUG2 test df/dir num!");
#endif
	for(int n=0;(n<this->cDirNum); n++) { mGaussw[n] = 0.0; }
	//for(int n=0;(n<this->cDirNum); n++) { 
	for(int n=0;(n<this->cDfNum); n++) { 
		const LbmFloat d = norm(LbmVec(this->dfVecX[n], this->dfVecY[n], this->dfVecZ[n]));
		LbmFloat w = expf( -alpha*d*d ) - expf( -alpha*gw*gw );
		mGaussw[n] = w;
		totGaussw += w;
	}
	for(int n=0;(n<this->cDirNum); n++) { 
		mGaussw[n] = mGaussw[n]/totGaussw;
	}

}

/*****************************************************************************/
/* Destructor */
/*****************************************************************************/
LbmFsgrSolver::~LbmFsgrSolver()
{
  if(!this->mInitDone){ debugOut("LbmFsgrSolver::LbmFsgrSolver : not inited...",0); return; }
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
	delete this->mpIso;
	if(mpPreviewSurface) delete mpPreviewSurface;

#if LBM_INCLUDE_TESTSOLVERS==1
	// cleanup done during scene deletion...
#endif // ELBEEM_PLUGIN!=1

	// always output performance estimate
	debMsgStd("LbmFsgrSolver::~LbmFsgrSolver",DM_MSG," Avg. MLSUPS:"<<(mAvgMLSUPS/mAvgMLSUPSCnt), 5);
  if(!this->mSilent) debMsgStd("LbmFsgrSolver::~LbmFsgrSolver",DM_MSG,"Deleted...",10);
}




/******************************************************************************
 * initilize variables fom attribute list 
 *****************************************************************************/
void LbmFsgrSolver::parseAttrList()
{
	LbmSolverInterface::parseStdAttrList();

	string matIso("default");
	matIso = this->mpAttrs->readString("material_surf", matIso, "SimulationLbm","mpIso->material", false );
	this->mpIso->setMaterialName( matIso );
	this->mOutputSurfacePreview = this->mpAttrs->readInt("surfacepreview", this->mOutputSurfacePreview, "SimulationLbm","this->mOutputSurfacePreview", false );
	mTimeAdap = this->mpAttrs->readBool("timeadap", mTimeAdap, "SimulationLbm","mTimeAdap", false );
	this->mDomainBound = this->mpAttrs->readString("domainbound", this->mDomainBound, "SimulationLbm","mDomainBound", false );
	this->mDomainPartSlipValue = this->mpAttrs->readFloat("domainpartslip", this->mDomainPartSlipValue, "SimulationLbm","mDomainPartSlipValue", false );

	mIsoWeightMethod= this->mpAttrs->readInt("isoweightmethod", mIsoWeightMethod, "SimulationLbm","mIsoWeightMethod", false );
	mInitSurfaceSmoothing = this->mpAttrs->readInt("initsurfsmooth", mInitSurfaceSmoothing, "SimulationLbm","mInitSurfaceSmoothing", false );
	this->mSmoothSurface = this->mpAttrs->readFloat("smoothsurface", this->mSmoothSurface, "SimulationLbm","mSmoothSurface", false );
	this->mSmoothNormals = this->mpAttrs->readFloat("smoothnormals", this->mSmoothNormals, "SimulationLbm","mSmoothNormals", false );

	mInitialCsmago = this->mpAttrs->readFloat("csmago", mInitialCsmago, "SimulationLbm","mInitialCsmago", false );
	// deprecated!
	float mInitialCsmagoCoarse = 0.0;
	mInitialCsmagoCoarse = this->mpAttrs->readFloat("csmago_coarse", mInitialCsmagoCoarse, "SimulationLbm","mInitialCsmagoCoarse", false );
#if USE_LES==1
#else // USE_LES==1
	debMsgStd("LbmFsgrSolver", DM_WARNING, "LES model switched off!",2);
	mInitialCsmago = 0.0;
#endif // USE_LES==1

	// refinement
	mMaxRefine = this->mRefinementDesired;
	mMaxRefine  = this->mpAttrs->readInt("maxrefine",  mMaxRefine ,"LbmFsgrSolver", "mMaxRefine", false);
	if(mMaxRefine<0) mMaxRefine=0;
	if(mMaxRefine>FSGR_MAXNOOFLEVELS) mMaxRefine=FSGR_MAXNOOFLEVELS-1;
	mDisableStandingFluidInit = this->mpAttrs->readInt("disable_stfluidinit", mDisableStandingFluidInit,"LbmFsgrSolver", "mDisableStandingFluidInit", false);
	mForceTadapRefine = this->mpAttrs->readInt("forcetadaprefine", mForceTadapRefine,"LbmFsgrSolver", "mForceTadapRefine", false);

	// demo mode settings
	mFVHeight = this->mpAttrs->readFloat("fvolheight", mFVHeight, "LbmFsgrSolver","mFVHeight", false );
	// FIXME check needed?
	mFVArea   = this->mpAttrs->readFloat("fvolarea", mFVArea, "LbmFsgrSolver","mFArea", false );

#if LBM_INCLUDE_TESTSOLVERS==1
	mUseTestdata = 0;
	mUseTestdata = this->mpAttrs->readBool("use_testdata", mUseTestdata,"LbmFsgrSolver", "mUseTestdata", false);
	mpTest->parseTestdataAttrList(this->mpAttrs);
#ifdef ELBEEM_PLUGIN
	mUseTestdata=1; // DEBUG
#endif // ELBEEM_PLUGIN
	errMsg("LbmFsgrSolver::LBM_INCLUDE_TESTSOLVERS","Active, mUseTestdata:"<<mUseTestdata<<" ");
#else // LBM_INCLUDE_TESTSOLVERS!=1
	// off by default
	mUseTestdata = 0;
	if(mFarFieldSize>=2.) mUseTestdata=1; // equiv. to test solver check
	if(mUseTestdata) { mMaxRefine=0; } // force fsgr off
#endif // LBM_INCLUDE_TESTSOLVERS!=1
}


/******************************************************************************
 * Initialize omegas and forces on all levels (for init/timestep change)
 *****************************************************************************/
void LbmFsgrSolver::initLevelOmegas()
{
	// no explicit settings
	this->mOmega = this->mpParam->calculateOmega(mSimulationTime);
	this->mGravity = vec2L( this->mpParam->calculateGravity(mSimulationTime) );
	this->mSurfaceTension = 0.; //this->mpParam->calculateSurfaceTension(); // unused

	// check if last init was ok
	LbmFloat gravDelta = norm(this->mGravity-mLastGravity);
	//errMsg("ChannelAnimDebug","t:"<<mSimulationTime<<" om:"<<this->mOmega<<" - lom:"<<mLastOmega<<" gv:"<<this->mGravity<<" - "<<mLastGravity<<" , "<<gravDelta  );
	if((this->mOmega == mLastOmega) && (gravDelta<=0.0)) return;

	if(mInitialCsmago<=0.0) {
		if(OPT3D==1) {
			errFatal("LbmFsgrSolver::initLevelOmegas","Csmago-LES = 0 not supported for optimized 3D version...",SIMWORLD_INITERROR); 
			return;
		}
	}

	// use Tau instead of Omega for calculations
	{ // init base level
		int i = mMaxRefine;
		mLevel[i].omega    = this->mOmega;
		mLevel[i].timestep = this->mpParam->getTimestep();
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
		mLevel[i].timestep    = 2.0 * mLevel[i+1].timestep;
		mLevel[i].lcsmago = mInitialCsmago;
		mLevel[i].lcsmago_sqr = mLevel[i].lcsmago*mLevel[i].lcsmago;
		mLevel[i].lcnu        = (2.0* (1.0/mLevel[i].omega)-1.0) * (1.0/6.0);
	}
	
	// for lbgk
	mLevel[ mMaxRefine ].gravity = this->mGravity / mLevel[ mMaxRefine ].omega;
	for(int i=mMaxRefine-1; i>=0; i--) {
		// should be the same on all levels...
		// for lbgk
		mLevel[i].gravity = (mLevel[i+1].gravity * mLevel[i+1].omega) * 2.0 / mLevel[i].omega;
	}

	mLastOmega = this->mOmega;
	mLastGravity = this->mGravity;
	// debug? invalidate old values...
	this->mGravity = -100.0;
	this->mOmega = -100.0;

	for(int i=0; i<=mMaxRefine; i++) {
		if(!this->mSilent) {
			errMsg("LbmFsgrSolver", "Level init "<<i<<" - sizes:"<<mLevel[i].lSizex<<","<<mLevel[i].lSizey<<","<<mLevel[i].lSizez<<" offs:"<<mLevel[i].lOffsx<<","<<mLevel[i].lOffsy<<","<<mLevel[i].lOffsz 
					<<" omega:"<<mLevel[i].omega<<" grav:"<<mLevel[i].gravity<< ", "
					<<" cmsagp:"<<mLevel[i].lcsmago<<", "
					<< " ss"<<mLevel[i].timestep<<" ns"<<mLevel[i].nodeSize<<" cs"<<mLevel[i].simCellSize );
		} else {
			if(!this->mInitDone) {
				debMsgStd("LbmFsgrSolver", DM_MSG, "Level init "<<i<<" - sizes:"<<mLevel[i].lSizex<<","<<mLevel[i].lSizey<<","<<mLevel[i].lSizez<<" "
						<<"omega:"<<mLevel[i].omega<<" grav:"<<mLevel[i].gravity , 5);
			}
		}
	}
	if(mMaxRefine>0) {
		mDfScaleUp   = (mLevel[0  ].timestep/mLevel[0+1].timestep)* (1.0/mLevel[0  ].omega-1.0)/ (1.0/mLevel[0+1].omega-1.0); // yu
		mDfScaleDown = (mLevel[0+1].timestep/mLevel[0  ].timestep)* (1.0/mLevel[0+1].omega-1.0)/ (1.0/mLevel[0  ].omega-1.0); // yu
	}
}


/******************************************************************************
 * Init Solver (values should be read from config file)
 *****************************************************************************/
bool LbmFsgrSolver::initializeSolver()
{
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Init start... "<<this->mInitDone<<" "<<(void*)this,1);

	// init cppf stage
	if(mCppfStage>0) {
		this->mSizex *= mCppfStage;
		this->mSizey *= mCppfStage;
		this->mSizez *= mCppfStage;
	}

	// size inits to force cubic cells and mult4 level dimensions
	// and make sure we dont allocate too much...
	bool memOk = false;
	int orgSx = this->mSizex;
	int orgSy = this->mSizey;
	int orgSz = this->mSizez;
	double sizeReduction = 1.0;
	double memEstFromFunc = -1.0;
	string memreqStr("");	
	while(!memOk) {
		initGridSizes( this->mSizex, this->mSizey, this->mSizez,
				this->mvGeoStart, this->mvGeoEnd, mMaxRefine, PARALLEL);
		calculateMemreqEstimate( this->mSizex, this->mSizey, this->mSizez, mMaxRefine, &memEstFromFunc, &memreqStr );
		
		double memLimit;
		if(sizeof(int)==4) {
			// 32bit system, limit to 2GB
			memLimit = 2.0* 1024.0*1024.0*1024.0;
		} else {
			// 64bit, just take 16GB as limit for now...
			memLimit = 16.0* 1024.0*1024.0*1024.0;
		}
		if(memEstFromFunc>memLimit) {
			sizeReduction *= 0.9;
			this->mSizex = (int)(orgSx * sizeReduction);
			this->mSizey = (int)(orgSy * sizeReduction);
			this->mSizez = (int)(orgSz * sizeReduction);
			debMsgStd("LbmFsgrSolver::initialize",DM_WARNING,"initGridSizes: memory limit exceeded "<<memEstFromFunc<<"/"<<memLimit<<", retrying: "
					<<this->mSizex<<" Y:"<<this->mSizey<<" Z:"<<this->mSizez, 3 );
		} else {
			memOk = true;
		} 
	}
	
	this->mPreviewFactor = (LbmFloat)this->mOutputSurfacePreview / (LbmFloat)this->mSizex;
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"initGridSizes: Final domain size X:"<<this->mSizex<<" Y:"<<this->mSizey<<" Z:"<<this->mSizez<<
	  ", Domain: "<<this->mvGeoStart<<":"<<this->mvGeoEnd<<", "<<(this->mvGeoEnd-this->mvGeoStart)<<
	  ", est. Mem.Req.: "<<memreqStr	,2);
	this->mpParam->setSize(this->mSizex, this->mSizey, this->mSizez);

  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Definitions: "
		<<"LBM_EPSILON="<<LBM_EPSILON  <<" "
		<<"FSGR_STRICT_DEBUG="<<FSGR_STRICT_DEBUG <<" "
		<<"OPT3D="<<OPT3D <<" "
		<<"COMPRESSGRIDS="<<COMPRESSGRIDS<<" "
		<<"MASS_INVALID="<<MASS_INVALID <<" "
		<<"FSGR_LISTTRICK="<<FSGR_LISTTRICK <<" "
		<<"FSGR_LISTTTHRESHEMPTY="<<FSGR_LISTTTHRESHEMPTY <<" "
		<<"FSGR_LISTTTHRESHFULL="<<FSGR_LISTTTHRESHFULL <<" "
		<<"FSGR_MAGICNR="<<FSGR_MAGICNR <<" " 
		<<"USE_LES="<<USE_LES <<" " 
		,10);
#if ELBEEM_PLUGIN!=1
#endif // ELBEEM_PLUGIN!=1

	// perform 2D corrections...
	if(LBMDIM == 2) this->mSizez = 1;

	this->mpParam->setSimulationMaxSpeed(0.0);
	if(mFVHeight>0.0) this->mpParam->setFluidVolumeHeight(mFVHeight);
	this->mpParam->setTadapLevels( mMaxRefine+1 );

	if(mForceTadapRefine>mMaxRefine) {
		this->mpParam->setTadapLevels( mForceTadapRefine+1 );
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Forcing a t-adap refine level of "<<mForceTadapRefine, 6);
	}

	if(!this->mpParam->calculateAllMissingValues(mSimulationTime, false)) {
		errFatal("LbmFsgrSolver::initialize","Fatal: failed to init parameters! Aborting...",SIMWORLD_INITERROR);
		return false;
	}


	// init vectors
	for(int i=0; i<=mMaxRefine; i++) {
		mLevel[i].id = i;
		mLevel[i].nodeSize = 0.0; 
		mLevel[i].simCellSize = 0.0; 
		mLevel[i].omega = 0.0; 
		mLevel[i].time = 0.0; 
		mLevel[i].timestep = 1.0;
		mLevel[i].gravity = LbmVec(0.0); 
		mLevel[i].mprsCells[0] = NULL;
		mLevel[i].mprsCells[1] = NULL;
		mLevel[i].mprsFlags[0] = NULL;
		mLevel[i].mprsFlags[1] = NULL;

		mLevel[i].avgOmega = 0.0; 
		mLevel[i].avgOmegaCnt = 0.0;
	}

	// init sizes
	mLevel[mMaxRefine].lSizex = this->mSizex;
	mLevel[mMaxRefine].lSizey = this->mSizey;
	mLevel[mMaxRefine].lSizez = this->mSizez;
	for(int i=mMaxRefine-1; i>=0; i--) {
		mLevel[i].lSizex = mLevel[i+1].lSizex/2;
		mLevel[i].lSizey = mLevel[i+1].lSizey/2;
		mLevel[i].lSizez = mLevel[i+1].lSizez/2;
	}

	// safety check
	if(sizeof(CellFlagType) != CellFlagTypeSize) {
		errFatal("LbmFsgrSolver::initialize","Fatal Error: CellFlagType has wrong size! Is:"<<sizeof(CellFlagType)<<", should be:"<<CellFlagTypeSize, SIMWORLD_GENERICERROR);
		return false;
	}

	double ownMemCheck = 0.0;
	mLevel[ mMaxRefine ].nodeSize = ((this->mvGeoEnd[0]-this->mvGeoStart[0]) / (LbmFloat)(this->mSizex));
	mLevel[ mMaxRefine ].simCellSize = this->mpParam->getCellSize();
	mLevel[ mMaxRefine ].lcellfactor = 1.0;
	LONGINT rcellSize = ((mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez) *dTotalNum);
	// +4 for safety ?
	mLevel[ mMaxRefine ].mprsFlags[0] = new CellFlagType[ rcellSize/dTotalNum +4 ];
	mLevel[ mMaxRefine ].mprsFlags[1] = new CellFlagType[ rcellSize/dTotalNum +4 ];
	ownMemCheck += 2 * sizeof(CellFlagType) * (rcellSize/dTotalNum +4);

#if COMPRESSGRIDS==0
	mLevel[ mMaxRefine ].mprsCells[0] = new LbmFloat[ rcellSize +4 ];
	mLevel[ mMaxRefine ].mprsCells[1] = new LbmFloat[ rcellSize +4 ];
	ownMemCheck += 2 * sizeof(LbmFloat) * (rcellSize+4);
#else // COMPRESSGRIDS==0
	LONGINT compressOffset = (mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*dTotalNum*2);
	mLevel[ mMaxRefine ].mprsCells[1] = new LbmFloat[ rcellSize +compressOffset +4 ];
	mLevel[ mMaxRefine ].mprsCells[0] = mLevel[ mMaxRefine ].mprsCells[1]+compressOffset;
	ownMemCheck += sizeof(LbmFloat) * (rcellSize +compressOffset +4);
#endif // COMPRESSGRIDS==0

	LbmFloat lcfdimFac = 8.0;
	if(LBMDIM==2) lcfdimFac = 4.0;
	for(int i=mMaxRefine-1; i>=0; i--) {
		mLevel[i].nodeSize = 2.0 * mLevel[i+1].nodeSize;
		mLevel[i].simCellSize = 2.0 * mLevel[i+1].simCellSize;
		mLevel[i].lcellfactor = mLevel[i+1].lcellfactor * lcfdimFac;

		if(LBMDIM==2){ mLevel[i].lSizez = 1; } // 2D
		rcellSize = ((mLevel[i].lSizex*mLevel[i].lSizey*mLevel[i].lSizez) *dTotalNum);
		mLevel[i].mprsFlags[0] = new CellFlagType[ rcellSize/dTotalNum +4 ];
		mLevel[i].mprsFlags[1] = new CellFlagType[ rcellSize/dTotalNum +4 ];
		ownMemCheck += 2 * sizeof(CellFlagType) * (rcellSize/dTotalNum +4);
		mLevel[i].mprsCells[0] = new LbmFloat[ rcellSize +4 ];
		mLevel[i].mprsCells[1] = new LbmFloat[ rcellSize +4 ];
		ownMemCheck += 2 * sizeof(LbmFloat) * (rcellSize+4);
	}

	// isosurface memory
	ownMemCheck += (3*sizeof(int)+sizeof(float)) * ((this->mSizex+2)*(this->mSizey+2)*(this->mSizez+2));
	// sanity check
#if ELBEEM_PLUGIN!=1
	if(ABS(1.0-ownMemCheck/memEstFromFunc)>0.01) {
		errMsg("LbmFsgrSolver::initialize","Sanity Error - memory estimate is off! real:"<<ownMemCheck<<" vs. estimate:"<<memEstFromFunc );
	}
#endif // ELBEEM_PLUGIN!=1
	
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
	mMinTimestep = this->mpParam->getTimestep();
	mMaxTimestep = this->mpParam->getTimestep();

	// init isosurf
	this->mpIso->setIsolevel( this->mIsoValue );
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata) {
		mpTest->setMaterialName( this->mpIso->getMaterialName() );
		delete this->mpIso;
		this->mpIso = mpTest;
		if(mpTest->mDebugvalue1>0.0) { // 3d off
			mpTest->setIsolevel(-100.0);
		} else {
			mpTest->setIsolevel( this->mIsoValue );
		}
	}
#endif // ELBEEM_PLUGIN!=1
	// approximate feature size with mesh resolution
	float featureSize = mLevel[ mMaxRefine ].nodeSize*0.5;
	// smooth vars defined in solver_interface, set by simulation object
	// reset for invalid values...
	if((this->mSmoothSurface<0.)||(this->mSmoothSurface>50.)) this->mSmoothSurface = 1.;
	if((this->mSmoothNormals<0.)||(this->mSmoothNormals>50.)) this->mSmoothNormals = 1.;
	this->mpIso->setSmoothSurface( this->mSmoothSurface * featureSize );
	this->mpIso->setSmoothNormals( this->mSmoothNormals * featureSize );

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

	LbmVec isostart = vec2L(this->mvGeoStart);
	LbmVec isoend   = vec2L(this->mvGeoEnd);
	int twodOff = 0; // 2d slices
	if(LBMDIM==2) {
		LbmFloat sn,se;
		sn = isostart[2]+(isoend[2]-isostart[2])*0.5 - ((isoend[0]-isostart[0]) / (LbmFloat)(this->mSizex+1.0))*0.5;
		se = isostart[2]+(isoend[2]-isostart[2])*0.5 + ((isoend[0]-isostart[0]) / (LbmFloat)(this->mSizex+1.0))*0.5;
		isostart[2] = sn;
		isoend[2] = se;
		twodOff = 2;
	}
	//errMsg(" SETISO ", " "<<isostart<<" - "<<isoend<<" "<<(((isoend[0]-isostart[0]) / (LbmFloat)(this->mSizex+1.0))*0.5)<<" "<<(LbmFloat)(this->mSizex+1.0)<<" " );
	this->mpIso->setStart( vec2G(isostart) );
	this->mpIso->setEnd(   vec2G(isoend) );
	LbmVec isodist = isoend-isostart;
	this->mpIso->initializeIsosurface( this->mSizex+2, this->mSizey+2, this->mSizez+2+twodOff, vec2G(isodist) );
	for(int ak=0;ak<this->mSizez+2+twodOff;ak++) 
		for(int aj=0;aj<this->mSizey+2;aj++) 
			for(int ai=0;ai<this->mSizex+2;ai++) { *this->mpIso->getData(ai,aj,ak) = 0.0; }

  /* init array (set all invalid first) */
	for(int lev=0; lev<=mMaxRefine; lev++) {
		FSGR_FORIJK_BOUNDS(lev) {
			RFLAG(lev,i,j,k,0) = RFLAG(lev,i,j,k,0) = 0; // reset for changeFlag usage
			if(!this->mAllfluid) {
				initEmptyCell(lev, i,j,k, CFEmpty, -1.0, -1.0); 
			} else {
				initEmptyCell(lev, i,j,k, CFFluid, 1.0, 1.0); 
			}
		}
	}

	// init defaults
	mAvgNumUsedCells = 0;
	this->mFixMass= 0.0;

  /* init boundaries */
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Boundary init...",10);
	// init obstacles, and reinit time step size 
	initGeometryFlags();
	mLastSimTime = -1.0;
	// TODO check for invalid cells? nitGenericTestCases();
	
	// new - init noslip 1 everywhere...
	// half fill boundary cells?

	CellFlagType domainBoundType = CFInvalid;
	// TODO use normal object types instad...
	if(this->mDomainBound.find(string("free")) != string::npos) {
		domainBoundType = CFBnd | CFBndFreeslip;
  	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Domain Boundary Type: FreeSlip, value:"<<this->mDomainBound,10);
	} else if(this->mDomainBound.find(string("part")) != string::npos) {
		domainBoundType = CFBnd | CFBndPartslip; // part slip type
  	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Domain Boundary Type: PartSlip ("<<this->mDomainPartSlipValue<<"), value:"<<this->mDomainBound,10);
	} else { 
		domainBoundType = CFBnd | CFBndNoslip;
  	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Domain Boundary Type: NoSlip, value:"<<this->mDomainBound,10);
	}

	// use ar[numobjs] as entry for domain (used e.g. for mDomainPartSlipValue in mObjectPartslips)
	int domainobj = (int)(this->mpGiObjects->size());
	domainBoundType |= (domainobj<<24);
	//for(int i=0; i<(int)(domainobj+0); i++) {
		//errMsg("GEOIN","i"<<i<<" "<<(*this->mpGiObjects)[i]->getName());
		//if((*this->mpGiObjects)[i] == this->mpIso) { //check...
		//} 
	//}
	//errMsg("GEOIN"," dm "<<(domainBoundType>>24));

  for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
			initEmptyCell(mMaxRefine, i,0,k, domainBoundType, 0.0, BND_FILL); 
			initEmptyCell(mMaxRefine, i,mLevel[mMaxRefine].lSizey-1,k, domainBoundType, 0.0, BND_FILL); 
    }

  for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int j=0;j<mLevel[mMaxRefine].lSizey;j++) {
			initEmptyCell(mMaxRefine, 0,j,k, domainBoundType, 0.0, BND_FILL); 
			initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-1,j,k, domainBoundType, 0.0, BND_FILL); 
			// DEBUG BORDER!
			//initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-2,j,k, domainBoundType, 0.0, BND_FILL); 
    }

	if(LBMDIM == 3) {
		// only for 3D
		for(int j=0;j<mLevel[mMaxRefine].lSizey;j++)
			for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
				initEmptyCell(mMaxRefine, i,j,0, domainBoundType, 0.0, BND_FILL); 
				initEmptyCell(mMaxRefine, i,j,mLevel[mMaxRefine].lSizez-1, domainBoundType, 0.0, BND_FILL); 
			}
	}

	// TEST!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11
  /*for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int j=0;j<mLevel[mMaxRefine].lSizey;j++) {
			initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-2,j,k, domainBoundType, 0.0, BND_FILL); 
    }
  for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
    for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
			initEmptyCell(mMaxRefine, i,1,k, domainBoundType, 0.0, BND_FILL); 
    }
	// */

	/*for(int ii=0; ii<(int)po w_change?(2.0,mMaxRefine)-1; ii++) {
		errMsg("BNDTESTSYMM","set "<<mLevel[mMaxRefine].lSizex-2-ii );
		for(int k=0;k<mLevel[mMaxRefine].lSizez;k++)
			for(int j=0;j<mLevel[mMaxRefine].lSizey;j++) {
				initEmptyCell(mMaxRefine, mLevel[mMaxRefine].lSizex-2-ii,j,k, domainBoundType, 0.0, BND_FILL);  // SYMM!? 2D?
			}
		for(int j=0;j<mLevel[mMaxRefine].lSizey;j++)
			for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {      
				initEmptyCell(mMaxRefine, i,j,mLevel[mMaxRefine].lSizez-2-ii, domainBoundType, 0.0, BND_FILL);   // SYMM!? 3D?
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

	ParamVec cspv = this->mpParam->calculateCellSize();
	if(LBMDIM==2) cspv[2] = 1.0;
	inmCellCnt = 1;
	double nrmMass = (double)mInitialMass / (double)(inmCellCnt) *cspv[0]*cspv[1]*cspv[2] * 1000.0;
	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Initial Mass:"<<mInitialMass<<" normalized:"<<nrmMass, 3);
	mInitialMass = 0.0; // reset, and use actual value after first step

	//mStartSymm = false;
#if ELBEEM_PLUGIN!=1
	if((LBMDIM==2)&&(this->mSizex<200)) {
		if(!checkSymmetry("init")) {
			errMsg("LbmFsgrSolver::initialize","Unsymmetric init...");
		} else {
			errMsg("LbmFsgrSolver::initialize","Symmetric init!");
		}
	}
#endif // ELBEEM_PLUGIN!=1
	

	// ----------------------------------------------------------------------
	// coarsen region
	myTime_t fsgrtstart = getTime(); 
	for(int lev=mMaxRefine-1; lev>=0; lev--) {
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Coarsening level "<<lev<<".",8);
		adaptGrid(lev);
		coarseRestrictFromFine(lev);
		adaptGrid(lev);
		coarseRestrictFromFine(lev);
	}
	this->markedClearList();
	myTime_t fsgrtend = getTime(); 
	if(!this->mSilent){ debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"FSGR init done ("<< getTimeString(fsgrtend-fsgrtstart)<<"), changes:"<<mNumFsgrChanges , 10 ); }
	mNumFsgrChanges = 0;

	for(int l=0; l<this->cDirNum; l++) { 
		LbmFloat area = 0.5 * 0.5 *0.5;
		if(LBMDIM==2) area = 0.5 * 0.5;

		if(this->dfVecX[l]!=0) area *= 0.5;
		if(this->dfVecY[l]!=0) area *= 0.5;
		if(this->dfVecZ[l]!=0) area *= 0.5;
		mFsgrCellArea[l] = area;
	} // l

	// make sure both sets are ok
	// copy from other to curr
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		RFLAG(lev, i,j,k,mLevel[lev].setOther) = RFLAG(lev, i,j,k,mLevel[lev].setCurr);
	} } // first copy flags */


	
	if(LBMDIM==2) {
		if(this->mOutputSurfacePreview) {
			errMsg("LbmFsgrSolver::init","No preview in 2D allowed!");
			this->mOutputSurfacePreview = 0; }
	}
#if LBM_USE_GUI==1
	if(this->mOutputSurfacePreview) {
		errMsg("LbmFsgrSolver::init","No preview in GUI mode... mOutputSurfacePreview=0");
		this->mOutputSurfacePreview = 0; }
#endif // LBM_USE_GUI==1
	
	if(this->mOutputSurfacePreview) {

		// same as normal one, but use reduced size
		mpPreviewSurface = new IsoSurface( this->mIsoValue );
		mpPreviewSurface->setMaterialName( mpPreviewSurface->getMaterialName() );
		mpPreviewSurface->setIsolevel( this->mIsoValue );
		// usually dont display for rendering
		mpPreviewSurface->setVisible( false );

		mpPreviewSurface->setStart( vec2G(isostart) );
		mpPreviewSurface->setEnd(   vec2G(isoend) );
		LbmVec pisodist = isoend-isostart;
		LbmFloat pfac = this->mPreviewFactor;
		mpPreviewSurface->initializeIsosurface( (int)(pfac*this->mSizex)+2, (int)(pfac*this->mSizey)+2, (int)(pfac*this->mSizez)+2, vec2G(pisodist) );
		//mpPreviewSurface->setName( this->getName() + "preview" );
		mpPreviewSurface->setName( "preview" );
	
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Preview with sizes "<<(pfac*this->mSizex)<<","<<(pfac*this->mSizey)<<","<<(pfac*this->mSizez)<<" enabled",10);
	}

	// make sure fill fracs are right for first surface generation
	stepMain();

	// prepare once...
	prepareVisualization();
	// copy again for stats counting
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		RFLAG(lev, i,j,k,mLevel[lev].setOther) = RFLAG(lev, i,j,k,mLevel[lev].setCurr);
	} } // first copy flags */


	// now really done...
  debugOut("LbmFsgrSolver::initialize : SurfaceGen: SmsOrg("<<this->mSmoothSurface<<","<<this->mSmoothNormals<<","<<featureSize<<"), Iso("<<this->mpIso->getSmoothSurface()<<","<<this->mpIso->getSmoothNormals()<<") ",10);
  debugOut("LbmFsgrSolver::initialize : Init done ... ",10);
	this->mInitDone = 1;

#if LBM_INCLUDE_TESTSOLVERS==1
	initTestdata();
#endif // ELBEEM_PLUGIN!=1
	// not inited? dont use...
	if(mCutoff<0) mCutoff=0;

	initParticles();
	return true;
}




/*****************************************************************************/
//! init moving obstacles for next sim step sim 
/*****************************************************************************/
void LbmFsgrSolver::initMovingObstacles(bool staticInit) {

	myTime_t monstart = getTime();
	// new test
	const int level = mMaxRefine;
	const int workSet = mLevel[level].setCurr;
	LbmFloat sourceTime = mSimulationTime; // should be equal to mLastSimTime!
	// for debugging - check targetTime check during DEFAULT STREAM
	LbmFloat targetTime = mSimulationTime + this->mpParam->getTimestep();
	if(mLastSimTime == targetTime) {
		debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_WARNING,"Called for same time! (t="<<mSimulationTime<<" , targett="<<targetTime<<")", 1);
		return;
	}
	//debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_WARNING,"time: "<<mSimulationTime<<" lasttt:"<<mLastSimTime,10);
	//if(mSimulationTime!=mLastSimTime) errMsg("LbmFsgrSolver::initMovingObstacles","time: "<<mSimulationTime<<" lasttt:"<<mLastSimTime);

	LbmFloat rhomass = 0.0;
	CellFlagType otype = CFInvalid; // verify type of last step, might be non moving obs!
	CellFlagType ntype = CFInvalid;
	// WARNING - copied from geo init!
	int numobjs = (int)(this->mpGiObjects->size());
	ntlVec3Gfx dvec = ntlVec3Gfx(mLevel[level].nodeSize); //dvec*1.0;
	// 2d display as rectangles
	ntlVec3Gfx iniPos(0.0);
	if(LBMDIM==2) {
		dvec[2] = 1.0; 
		iniPos = (this->mvGeoStart + ntlVec3Gfx( 0.0, 0.0, (this->mvGeoEnd[2]-this->mvGeoStart[2])*0.5 ))-(dvec*0.0);
	} else {
		iniPos = (this->mvGeoStart + ntlVec3Gfx( 0.0 ))-(dvec*0.0);
		//iniPos[2] = this->mvGeoStart[2] + dvec[2]*getForZMin1();
	}
	
	// stats
	int monPoints=0, monObsts=0, monFluids=0, monTotal=0, monTrafo=0;
	CellFlagType nbflag[LBM_DFNUM]; 
	int nbored;
  vector<ntlVec3Gfx>  vertices;
  vector<ntlVec3Gfx>  verticesOld;
	int i,j,k; // grid pos init
	LbmFloat ux,uy,uz, rho;
	for(int OId=0; OId<numobjs; OId++) {
		ntlGeometryObject *obj = (*this->mpGiObjects)[OId];
		if( (!staticInit) && (!obj->getIsAnimated()) ) continue;
		if( ( staticInit) && ( obj->getIsAnimated()) ) continue;

		if(obj->getGeoInitType()&FGI_ALLBOUNDS) {

			otype = ntype = CFInvalid;
			switch(obj->getGeoInitType()) {
				case FGI_BNDPART: 
				case FGI_BNDFREE: 
					errMsg("LbmFsgrSolver::initMovingObstacles","Warning - moving free/part slip objects NYI "<<obj->getName() );
				case FGI_BNDNO: 
					rhomass = BND_FILL;
					otype = ntype = CFBnd|CFBndNoslip;
					break;
				case FGI_MBNDINFLOW: 
					otype = ntype = CFMbndInflow; 
					break;
				case FGI_MBNDOUTFLOW: 
					otype = ntype = CFMbndOutflow; 
					break;
			}
			int wasActive = ((obj->getGeoActive(sourceTime)>0.)? 1:0);
			int active =    ((obj->getGeoActive(targetTime)>0.)? 1:0);
			//errMsg("GEOACTT"," obj "<<obj->getName()<<" a:"<<active<<","<<wasActive<<"  s"<<sourceTime<<" t"<<targetTime <<" v"<<mObjectSpeeds[OId] );
			// skip inactive in/out flows
			if((!active) && (otype&(CFMbndOutflow|CFMbndInflow)) ) continue;

			// copied from  recalculateObjectSpeeds
			mObjectSpeeds[OId] = vec2L(this->mpParam->calculateLattVelocityFromRw( vec2P( (*this->mpGiObjects)[OId]->getInitialVelocity(mSimulationTime) )));
			debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG,"id"<<OId<<" "<<obj->getName()<<" inivel set to "<< mObjectSpeeds[OId]<<", unscaled:"<< (*this->mpGiObjects)[OId]->getInitialVelocity(mSimulationTime) ,10 );

			vertices.clear();
			obj->getMovingPoints(vertices);
			verticesOld = vertices;
			// WARNING - assumes mSimulationTime is global!?
			obj->applyTransformation(targetTime, &vertices,NULL, 0, vertices.size(), false );
			monTrafo += vertices.size();

			// correct flags from last position, but extrapolate
			// velocity to next timestep
			obj->applyTransformation(sourceTime, &verticesOld,NULL, 0, verticesOld.size(), false );
			monTrafo += verticesOld.size();

			// object types
			if(ntype&CFBnd){

				// check if object is moving at all
				if(obj->getIsAnimated()) {
					ntlVec3Gfx objMaxVel = obj->calculateMaxVel(sourceTime,targetTime);
					// FIXME?
					if(normNoSqrt(objMaxVel)>0.0) { ntype |= CFBndMoving; }
					// get old type - CHECK FIXME , timestep could have changed - cause trouble?
					ntlVec3Gfx oldobjMaxVel = obj->calculateMaxVel(sourceTime - this->mpParam->getTimestep(),sourceTime);
					if(normNoSqrt(oldobjMaxVel)>0.0) { otype |= CFBndMoving; }
				}

#if LBMDIM==2
#define CHECKIJK \
				if(i<=0) continue; \
				if(j<=0) continue; \
				if(i>=mLevel[level].lSizex-2) continue; \
				if(j>=mLevel[level].lSizey-2) continue; 
#define POS2GRID(vec,n) \
				monTotal++;\
				i=(int)( ((vec)[n][0]-iniPos[0])/dvec[0] +0.0); \
				j=(int)( ((vec)[n][1]-iniPos[1])/dvec[1] +0.0); \
				k=0;
#else // LBMDIM -> 3
#define CHECKIJK \
				if(i<=0) continue; \
				if(j<=0) continue; \
				if(k<=0) continue; \
				if(i>=mLevel[level].lSizex-2) continue; \
				if(j>=mLevel[level].lSizey-2) continue; \
				if(k>=mLevel[level].lSizez-2) continue; 
#define POS2GRID(vec,n) \
				monTotal++;\
				i=(int)( ((vec)[n][0]-iniPos[0])/dvec[0] +0.0); \
				j=(int)( ((vec)[n][1]-iniPos[1])/dvec[1] +0.0); \
				k=(int)( ((vec)[n][2]-iniPos[2])/dvec[2] +0.0); 
#endif // LBMDIM 

				// first pass - set new obs. cells
				if(active) {
				for(size_t n=0; n<vertices.size(); n++) {
					//errMsg("AAABB","OId"<<OId<<" n"<<n<<" -> "<<PRINT_IJK);
					POS2GRID(vertices,n);
					CHECKIJK;
					//if(i==30 && j==14) { errMsg("AAABB","OId"<<OId<<" n"<<n<<" -> "<<PRINT_IJK<<" "); }
					if(QCELL(level, i,j,k, workSet, dFlux)==targetTime) continue;
					monPoints++;
					LbmVec u= vec2L((vertices[n]-verticesOld[n]) /dvec); // * timeFac;
					RFLAG(level, i,j,k, workSet) = ntype;
					FORDF1 {
						CellFlagType flag = RFLAG_NB(level, i,j,k,workSet,l);
						if(flag&(CFFluid|CFInter)) {
							flag &= (~CFNoBndFluid); // remove CFNoBndFluid flag
							RFLAG_NB(level, i,j,k,workSet,l) &= flag; 
						}
					}
					LbmFloat *dstCell = RACPNT(level, i,j,k,workSet);
					RAC(dstCell,0) = 0.0;
					if(ntype&CFBndMoving) {
						//if(i==30 && j==14) { errMsg("AAABB","OId"<<OId<<" move "<<u); }
						// movement?
						FORDF1 {
							// TODO optimize, test rho calc necessary?
							// calculate destination density
							LbmFloat *dfs = &QCELL_NB(level, i,j,k,workSet,l,dC);
							rho = RAC(dfs,dC);
							for(int nl=0; nl<this->cDfNum; nl++) rho += RAC(dfs,nl);
							//rho = 1.0;
							const LbmFloat factor = 2.0*this->dfLength[l]*rho* 3.0;
							ux = this->dfDvecX[l]*u[0];
							uy = this->dfDvecY[l]*u[1];
							uz = this->dfDvecZ[l]*u[2];
							const LbmFloat usqr = (ux*ux+uy*uy+uz*uz)*1.5;
							USQRMAXCHECK(usqr,ux,uy,uz, mMaxVlen, mMxvx,mMxvy,mMxvz);
							//ux=uy=uz=0.0; // DEBUG
							RAC(dstCell,l) = factor*(ux+uy+uz);
						}
					} else {
						FORDF1 { RAC(dstCell,l) = 0.0; }
					}
					//errMsg("AAABB","OId"<<OId<<" n"<<n<<" -> "<<PRINT_IJK" u"<<u<<" ul"<<PRINT_VEC(ux,uy,uz) );
					RAC(dstCell, dFlux) = targetTime;
					monObsts++;
				} } // bnd, is active?

				// second pass, remove old ones
				if(wasActive) {
				for(size_t n=0; n<verticesOld.size(); n++) {
					POS2GRID(verticesOld,n);
					CHECKIJK;
					monPoints++;
					if((RFLAG(level, i,j,k, workSet) == otype) &&
					   (QCELL(level, i,j,k, workSet, dFlux) != targetTime)) {
						//? unused ntlVec3Gfx u= (vertices[n]-verticesOld[n]);
						// from mainloop
						nbored = 0;
#if OPT3D==0
						FORDF1 {
							nbflag[l] = RFLAG_NB(level, i,j,k,workSet,l);
							nbored |= nbflag[l];
						} 
#else
						const CellFlagType *pFlagCheck = &RFLAG(level, i,j,k,workSet); // omp
						nbflag[dSB] = *(pFlagCheck + (-mLevel[level].lOffsy+-mLevel[level].lOffsx)); nbored |= nbflag[dSB];
						nbflag[dWB] = *(pFlagCheck + (-mLevel[level].lOffsy+-1)); nbored |= nbflag[dWB];
						nbflag[ dB] = *(pFlagCheck + (-mLevel[level].lOffsy)); nbored |= nbflag[dB];
						nbflag[dEB] = *(pFlagCheck + (-mLevel[level].lOffsy+ 1)); nbored |= nbflag[dEB];
						nbflag[dNB] = *(pFlagCheck + (-mLevel[level].lOffsy+ mLevel[level].lOffsx)); nbored |= nbflag[dNB];

						nbflag[dSW] = *(pFlagCheck + (-mLevel[level].lOffsx+-1)); nbored |= nbflag[dSW];
						nbflag[ dS] = *(pFlagCheck + (-mLevel[level].lOffsx)); nbored |= nbflag[dS];
						nbflag[dSE] = *(pFlagCheck + (-mLevel[level].lOffsx+ 1)); nbored |= nbflag[dSE];

						nbflag[ dW] = *(pFlagCheck + (-1)); nbored |= nbflag[dW];
						nbflag[ dE] = *(pFlagCheck + ( 1)); nbored |= nbflag[dE];

						nbflag[dNW] = *(pFlagCheck + ( mLevel[level].lOffsx+-1)); nbored |= nbflag[dNW];
						nbflag[ dN] = *(pFlagCheck + ( mLevel[level].lOffsx)); nbored |= nbflag[dN];
						nbflag[dNE] = *(pFlagCheck + ( mLevel[level].lOffsx+ 1)); nbored |= nbflag[dNE];

						nbflag[dST] = *(pFlagCheck + ( mLevel[level].lOffsy+-mLevel[level].lOffsx)); nbored |= nbflag[dST];
						nbflag[dWT] = *(pFlagCheck + ( mLevel[level].lOffsy+-1)); nbored |= nbflag[dWT];
						nbflag[ dT] = *(pFlagCheck + ( mLevel[level].lOffsy)); nbored |= nbflag[dT];
						nbflag[dET] = *(pFlagCheck + ( mLevel[level].lOffsy+ 1)); nbored |= nbflag[dET];
						nbflag[dNT] = *(pFlagCheck + ( mLevel[level].lOffsy+ mLevel[level].lOffsx)); nbored |= nbflag[dNT];
						// */
#endif
						CellFlagType settype = CFInvalid;
						LbmFloat avgrho=0.0, avgux=0.0, avguy=0.0, avguz=0.0; 
						if(nbored&CFFluid) {
							if(nbored&CFInter) {
								settype = CFInter|CFNoInterpolSrc; rhomass = 0.001;
								interpolateCellValues(level,i,j,k, workSet, avgrho,avgux,avguy,avguz);
							} else {
								// ? dangerous...? settype = CFFluid|CFNoInterpolSrc; rhomass = 1.0;
								settype = CFInter|CFNoInterpolSrc; rhomass = 1.0;
								interpolateCellValues(level,i,j,k, workSet, avgrho,avgux,avguy,avguz);
							}
						} else {
							settype = CFEmpty; rhomass = 0.0;
						}
						//settype = CFBnd|CFBndNoslip; rhomass = 0.0;
						//avgux=avguy=avguz=0.0; avgrho=1.0;
						LbmVec speed(avgux,avguy,avguz);
						initVelocityCell(level, i,j,k, settype, avgrho, rhomass, speed );
						monFluids++;
					} // flag & simtime
				}
			} } // bnd, active

			else if(ntype&CFMbndInflow){
				// inflow pass - add new fluid cells
				// this is slightly different for standing inflows,
				// as the fluid is forced to the desired velocity inside as 
				// well...
				const LbmFloat iniRho = 1.0;
				const LbmVec vel(mObjectSpeeds[OId]);
				const LbmFloat usqr = (vel[0]*vel[0]+vel[1]*vel[1]+vel[2]*vel[2])*1.5;
				USQRMAXCHECK(usqr,vel[0],vel[1],vel[2], mMaxVlen, mMxvx,mMxvy,mMxvz);
	errMsg("LbmFsgrSolver::initMovingObstacles","id"<<OId<<" "<<obj->getName()<<" inflow "<<staticInit<<" "<<vertices.size() );
				
				for(size_t n=0; n<vertices.size(); n++) {
					POS2GRID(vertices,n);
					CHECKIJK;
					// TODO - also reinit interface cells !?
					if(RFLAG(level, i,j,k, workSet)!=CFEmpty) { 
						// test prevent particle gen for inflow inter cells
						if(RFLAG(level, i,j,k, workSet)&(CFInter)) { RFLAG(level, i,j,k, workSet) &= (~CFNoBndFluid); } // remove CFNoBndFluid flag
						continue; }
					monFluids++;

					// TODO add OPT3D treatment
					LbmFloat *tcel = RACPNT(level, i,j,k,workSet);
					FORDF0 { RAC(tcel, l) = this->getCollideEq(l, iniRho,vel[0],vel[1],vel[2]); }
					RAC(tcel, dMass) = RAC(tcel, dFfrac) = iniRho;
					RAC(tcel, dFlux) = FLUX_INIT;
					CellFlagType setFlag = CFInter;
					changeFlag(level, i,j,k, workSet, setFlag);
					mInitialMass += iniRho;
				}
				// second static init pass
				if(staticInit) {
					CellFlagType set2Flag = CFMbndInflow|(OId<<24);
					for(size_t n=0; n<vertices.size(); n++) {
						POS2GRID(vertices,n);
						CHECKIJK;
						if(RFLAG(level, i,j,k, workSet)&(CFMbndInflow|CFMbndOutflow)){ continue; }
						if(RFLAG(level, i,j,k, workSet)&(CFEmpty)) {
							changeFlag(level, i,j,k, workSet, set2Flag);
						} else if(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter)) {
							changeFlag(level, i,j,k, workSet, RFLAG(level, i,j,k, workSet)|set2Flag);
						}
					}
				} // second static inflow pass

			} // inflow

			//else if(ntype&CFMbndOutflow){
			else if(ntype&CFMbndOutflow){
				const LbmFloat iniRho = 0.0;
				for(size_t n=0; n<vertices.size(); n++) {
					POS2GRID(vertices,n);
					CHECKIJK;
					// FIXME check fluid/inter cells for non-static!?
					if(!(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter))) {
						if((staticInit)&&(RFLAG(level, i,j,k, workSet)==CFEmpty)) {
							changeFlag(level, i,j,k, workSet, CFMbndOutflow); }
						continue;
					}
					monFluids++;
					// interface cell - might be double empty? FIXME check?

					// remove fluid cells, shouldnt be here anyway
					LbmFloat *tcel = RACPNT(level, i,j,k,workSet);
					LbmFloat fluidRho = RAC(tcel,0); FORDF1 { fluidRho += RAC(tcel,l); }
					mInitialMass -= fluidRho;
					RAC(tcel, dMass) = RAC(tcel, dFfrac) = iniRho;
					RAC(tcel, dFlux) = FLUX_INIT;
					CellFlagType setFlag = CFInter;
					changeFlag(level, i,j,k, workSet, setFlag);

					// same as ifemptied for if below
					LbmPoint emptyp;
					emptyp.x = i; emptyp.y = j; emptyp.z = k;
					mListEmpty.push_back( emptyp );
					//calcCellsEmptied++;
				} // points
				// second static init pass
				if(staticInit) {
					CellFlagType set2Flag = CFMbndOutflow|(OId<<24);
					for(size_t n=0; n<vertices.size(); n++) {
						POS2GRID(vertices,n);
						CHECKIJK;
						if(RFLAG(level, i,j,k, workSet)&(CFMbndInflow|CFMbndOutflow)){ continue; }
						if(RFLAG(level, i,j,k, workSet)&(CFEmpty)) {
							changeFlag(level, i,j,k, workSet, set2Flag);
						} else if(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter)) {
							changeFlag(level, i,j,k, workSet, RFLAG(level, i,j,k, workSet)|set2Flag);
						}
					}
				} // second static outflow pass
			} // outflow
		}
		//vector<ntlGeometryObject*> *mpGiObjects;
		//Obj->getGeoInitType() ){
	}


	/* { // DEBUG check
		int workSet = mLevel[level].setCurr;
		FSGR_FORIJK1(level) {
			if( (RFLAG(level,i,j,k, workSet) & CFBndMoving) ) {
				if(QCELL(level, i,j,k, workSet, dFlux)!=targetTime) {
					errMsg("lastt"," old val!? at "<<PRINT_IJK<<" t="<<QCELL(level, i,j,k, workSet, dFlux)<<" target="<<targetTime);
				}
			}
		}
	} // DEBUG */
	
#undef CHECKIJK 
#undef POS2GRID
	myTime_t monend = getTime();
	errMsg("LbmFsgrSolver::initMovingObstacles","Total: "<<monTotal<<" Points :"<<monPoints<<" ObstInits:"<<monObsts<<" FlInits:"<<monFluids<<" Trafos:"<<monTotal<<", took "<<getTimeString(monend-monstart));
	mLastSimTime = targetTime;
}


/*****************************************************************************/
/*! perform geometry init (if switched on) */
/*****************************************************************************/
extern int globGeoInitDebug; //solver_interface
bool LbmFsgrSolver::initGeometryFlags() {
	int level = mMaxRefine;
	myTime_t geotimestart = getTime(); 
	ntlGeometryObject *pObj;
	// getCellSize (due to forced cubes, use x values)
	//ntlVec3Gfx dvec; //( (this->mvGeoEnd[0]-this->mvGeoStart[0])/ ((LbmFloat)this->mSizex*2.0));
	//bool thinHit = false;
	// real cell size from now on...
	//dvec *= 2.0; 
	//ntlVec3Gfx nodesize = ntlVec3Gfx(mLevel[level].nodeSize); //dvec*1.0;
	//dvec = nodesize;
	ntlVec3Gfx dvec = ntlVec3Gfx(mLevel[level].nodeSize); //dvec*1.0;
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Performing geometry init ("<< this->mGeoInitId <<") v"<<dvec,3);
	// WARNING - copied to movobj init!

	/* init object velocities, this has always to be called for init */
	this->initGeoTree(this->mGeoInitId);
	if(this->mAllfluid) { 
		this->freeGeoTree();
		return true; }

	// make sure moving obstacles are inited correctly
	// preinit moving obj points
	int numobjs = (int)(this->mpGiObjects->size());
	for(int o=0; o<numobjs; o++) {
		ntlGeometryObject *obj = (*this->mpGiObjects)[o];
		if(
				((obj->getGeoInitType()&FGI_ALLBOUNDS) && (obj->getIsAnimated())) ||
				(obj->getOnlyThinInit()) ) {
			obj->initMovingPoints(mLevel[mMaxRefine].nodeSize);
		}
	}

	// max speed init
	ntlVec3Gfx maxMovobjVelRw = this->getGeoMaxMovementVelocity( mSimulationTime, this->mpParam->getTimestep() );
	ntlVec3Gfx maxMovobjVel = vec2G( this->mpParam->calculateLattVelocityFromRw( vec2P( maxMovobjVelRw )) );
	this->mpParam->setSimulationMaxSpeed( norm(maxMovobjVel) + norm(mLevel[level].gravity) );
	LbmFloat allowMax = this->mpParam->getTadapMaxSpeed();  // maximum allowed velocity
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Maximum Velocity from geo init="<< maxMovobjVel <<" from mov. obj.="<<maxMovobjVelRw<<" , allowed Max="<<allowMax ,5);
	if(this->mpParam->getSimulationMaxSpeed() > allowMax) {
		// similar to adaptTimestep();
		LbmFloat nextmax = this->mpParam->getSimulationMaxSpeed();
		LbmFloat newdt = this->mpParam->getTimestep() * (allowMax / nextmax); // newtr
		debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Performing reparametrization, newdt="<< newdt<<" prevdt="<< this->mpParam->getTimestep() <<" ",5);
		this->mpParam->setDesiredTimestep( newdt );
		this->mpParam->calculateAllMissingValues( mSimulationTime, this->mSilent );
		//maxIniVel = vec2G( this->mpParam->calculateLattVelocityFromRw( vec2P(this->getGeoMaxInitialVelocity()) ));
		maxMovobjVel = vec2G( this->mpParam->calculateLattVelocityFromRw( vec2P(this->getGeoMaxMovementVelocity(
		                      mSimulationTime, this->mpParam->getTimestep() )) ));
		debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"New maximum Velocity from geo init="<< maxMovobjVel,5);
	}
	recalculateObjectSpeeds();
	// */

	// init obstacles for first time step (requires obj speeds)
	initMovingObstacles(true);

	/* set interface cells */
	ntlVec3Gfx pos,iniPos; // position of current cell
	LbmFloat rhomass = 0.0;
	CellFlagType ntype = CFInvalid;
	int savedNodes = 0;
	int OId = -1;
	gfxReal distance;

	// 2d display as rectangles
	if(LBMDIM==2) {
		dvec[2] = 0.0; 
		iniPos =(this->mvGeoStart + ntlVec3Gfx( 0.0, 0.0, (this->mvGeoEnd[2]-this->mvGeoStart[2])*0.5 ))+(dvec*0.5);
	} else {
		iniPos =(this->mvGeoStart + ntlVec3Gfx( 0.0 ))+(dvec*0.5);
	}


	// first init boundary conditions
	// invalid cells are set to empty afterwards
#define GETPOS(i,j,k) \
						ntlVec3Gfx( iniPos[0]+ dvec[0]*(gfxReal)(i), \
						            iniPos[1]+ dvec[1]*(gfxReal)(j), \
						            iniPos[2]+ dvec[2]*(gfxReal)(k) )
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int j=1;j<mLevel[level].lSizey-1;j++) {
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				ntype = CFInvalid;
				
				const bool inside = this->geoInitCheckPointInside( GETPOS(i,j,k) , FGI_ALLBOUNDS, OId, distance);
				// = this->geoInitCheckPointInside( GETPOS(i,j,k) , ntlVec3Gfx(1.0,0.0,0.0), FGI_ALLBOUNDS, OId, distance, dvec[0]*0.5, thinHit, true);
				//if(this->geoInitCheckPointInside( GETPOS(i,j,k) , FGI_ALLBOUNDS, OId, distance)) {
				if(inside) {
					pObj = (*this->mpGiObjects)[OId];
					switch( pObj->getGeoInitType() ){
					case FGI_MBNDINFLOW:  
					  if(! pObj->getIsAnimated() ) {
							rhomass = 1.0;
							ntype = CFFluid | CFMbndInflow;
						} else {
							ntype = CFInvalid;
						}
						break;
					case FGI_MBNDOUTFLOW: 
					  if(! pObj->getIsAnimated() ) {
							rhomass = 0.0;
							ntype = CFEmpty|CFMbndOutflow; 
						} else {
							ntype = CFInvalid;
						}
						break;
					case FGI_BNDNO: 
						rhomass = BND_FILL;
						ntype = CFBnd|CFBndNoslip; 
						break;
					case FGI_BNDFREE: 
						rhomass = BND_FILL;
						ntype = CFBnd|CFBndFreeslip; break;
					default: // warn here?
						rhomass = BND_FILL;
						ntype = CFBnd|CFBndNoslip; break;
					}
				}
				if(ntype != CFInvalid) {
					// initDefaultCell
					if((ntype & CFMbndInflow) || (ntype & CFMbndOutflow) ) { }
					ntype |= (OId<<24); // NEWTEST2
					initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
				}

				// walk along x until hit for following inits
				if(distance<=-1.0) { distance = 100.0; } // FIXME dangerous
				if(distance>=0.0) {
					gfxReal dcnt=dvec[0];
					while(( dcnt< distance-dvec[0] )&&(i+1<mLevel[level].lSizex-1)) {
						dcnt += dvec[0]; i++;
						savedNodes++;
						if(ntype != CFInvalid) {
							// rho,mass,OId are still inited from above
							initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
						}
					}
				} 
				// */

			} 
		} 
	} // zmax
	// */

	// now init fluid layer
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int j=1;j<mLevel[level].lSizey-1;j++) {
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;
				ntype = CFInvalid;
				int inits = 0;
				//if((i==1) && (j==31) && (k==48)) globGeoInitDebug=1;
				//else globGeoInitDebug=0;
				const bool inside = this->geoInitCheckPointInside( GETPOS(i,j,k) , FGI_FLUID, OId, distance);
				if(inside) {
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
				if(distance>=0.0) {
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

	// reset invalid to empty again
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int j=1;j<mLevel[level].lSizey-1;j++) {
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				if(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFInvalid) {
					RFLAG(level, i,j,k, mLevel[level].setOther) = 
					RFLAG(level, i,j,k, mLevel[level].setCurr) = CFEmpty;
				}
			}
		}
	}

	this->freeGeoTree();
	myTime_t geotimeend = getTime(); 
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Geometry init done ("<< getTimeString(geotimeend-geotimestart)<<","<<savedNodes<<") " , 10 ); 
	//errMsg(" SAVED "," "<<savedNodes<<" of "<<(mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez));
	return true;
}

/*****************************************************************************/
/* init part for all freesurface testcases */
void LbmFsgrSolver::initFreeSurfaces() {
	double interfaceFill = 0.45;   // filling level of interface cells
	//interfaceFill = 1.0; // DEUG!! GEOMTEST!!!!!!!!!!!!

	// set interface cells 
	FSGR_FORIJK1(mMaxRefine) {

		if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr), CFFluid )) {
			int initInter = 0; // check for neighboring empty cells 
			FORDF1 {
				if( TESTFLAG( RFLAG_NBINV(mMaxRefine, i, j, k,  mLevel[mMaxRefine].setCurr,l), CFEmpty ) ) {
					initInter = 1;
				}
			}
			if(initInter) {
				QCELL(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr, dMass) = interfaceFill;
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
	} // */

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
		debMsgStd("Surface Smoothing init", DM_MSG, "Performing "<<(mInitSurfaceSmoothing)<<" smoothing timestep ",10);
#if COMPRESSGRIDS==1
		//errFatal("NYI","COMPRESSGRIDS mInitSurfaceSmoothing",SIMWORLD_INITERROR); return;
#endif // COMPRESSGRIDS==0
	}
	for(int s=0; s<mInitSurfaceSmoothing; s++) {
		//SGR_FORIJK1(mMaxRefine) {

		int kstart=getForZMin1(), kend=getForZMax1(mMaxRefine);
		int lev = mMaxRefine;
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
#endif // COMPRESSGRIDS==0
		for(int j=1;j<mLevel[lev].lSizey-1;++j) {
		for(int i=1;i<mLevel[lev].lSizex-1;++i) {
			if( TESTFLAG( RFLAG(lev, i,j,k, mLevel[lev].setCurr), CFInter) ) {
				LbmFloat mass = 0.0;
				//LbmFloat nbdiv;
				//FORDF0 {
				for(int l=0;(l<this->cDirNum); l++) { 
					int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
					if( RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr) & CFFluid ){
						mass += 1.0;
					}
					if( RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr) & CFInter ){
						mass += QCELL(lev, ni,nj,nk, mLevel[lev].setCurr, dMass);
					}
					//nbdiv+=1.0;
				}

				//errMsg(" I ", PRINT_IJK<<" m"<<mass );
				QCELL(lev, i,j,k, mLevel[lev].setOther, dMass) = (mass/ ((LbmFloat)this->cDirNum) );
				QCELL(lev, i,j,k, mLevel[lev].setOther, dFfrac) = QCELL(lev, i,j,k, mLevel[lev].setOther, dMass);
			}
		}}}

		mLevel[lev].setOther = mLevel[lev].setCurr;
		mLevel[lev].setCurr ^= 1;
	}
	// copy back...?

}

/*****************************************************************************/
/* init part for all freesurface testcases */
void LbmFsgrSolver::initStandingFluidGradient() {
	// ----------------------------------------------------------------------
	// standing fluid preinit
	const int debugStandingPreinit = 0;
	int haveStandingFluid = 0;

#define STANDFLAGCHECK(iindex) \
				if( ( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFInter)) ) || \
						( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFEmpty)) ) ){  \
					if((iindex)>1) { haveStandingFluid=(iindex); } \
					j = mLevel[mMaxRefine].lSizey; i=mLevel[mMaxRefine].lSizex; k=getForZMaxBnd(); \
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
		if( ( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFInter)) ) || 
		    ( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFBndMoving)) ) || 
				( (RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr) & (CFEmpty)) ) ) {  
			int fluidHeight = (ABS(gravIndex[maxGravComp] - gravIMin[maxGravComp]));
			if(debugStandingPreinit) errMsg("Standing fp","fh="<<fluidHeight<<" gmax="<<gravIMax[maxGravComp]<<" gi="<<gravIndex[maxGravComp] );
			if(fluidHeight>1) {
				haveStandingFluid = fluidHeight; //gravIndex[maxGravComp]; 
				gravIMax[maxGravComp] = gravIndex[maxGravComp] + gravDir[maxGravComp];
			}
			gravAbort = true; continue; 
		} 
	} // GRAVLOOP
	// _PRINTGDIRS;

	LbmFloat fluidHeight;
	fluidHeight = (LbmFloat)(ABS(gravIMax[maxGravComp]-gravIMin[maxGravComp]));
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata) mpTest->mFluidHeight = (int)fluidHeight;
#endif // ELBEEM_PLUGIN!=1
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
	for(int k=getForZMinBnd();k<getForZMaxBnd(mMaxRefine);++k) {
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
		LbmFloat lcsmqo;
#if OPT3D==1 
		LbmFloat lcsmqadd, lcsmeq[LBM_DFNUM], lcsmomega;
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
		debMsgStd("Standing fluid preinit", DM_MSG, "Density gradient inited (max-rho:"<<
			(1.0+ (fluidHeight) * (mLevel[lev].gravity[maxGravComp])* (-3.0/1.0)*(mLevel[lev].omega)) <<", h:"<< fluidHeight<<") ", 8);
		
#if ELBEEM_PLUGIN!=1 && LBMDIM==3
		/*int lowj = 0;
		for(int k=1;k<mLevel[lev].lSizez-1;++k) {
		for(int i=1;i<mLevel[lev].lSizex-1;++i) {
			LbmFloat rho = 1.0+ (fluidHeight) * (mLevel[lev].gravity[maxGravComp])* (-3.0/1.0)*(mLevel[lev].omega); 
			RFLAG(lev, i,lowj,k, rhoworkSet^1) =
			RFLAG(lev, i,lowj,k, rhoworkSet) = CFFluid;
			FORDF0 { QCELL(lev, i,lowj,k, rhoworkSet, l) = this->dfEquil[l]*rho; }
			QCELL(lev, i,lowj,k, rhoworkSet, dMass) = rho;
		} } // */
#endif 

		int preinitSteps = (haveStandingFluid* ((mLevel[lev].lSizey+mLevel[lev].lSizez+mLevel[lev].lSizex)/3) );
		preinitSteps = (haveStandingFluid>>2); // not much use...?
		//preinitSteps = 4; // DEBUG!!!!
		//this->mInitDone = 1; // GRAVTEST
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
		int kstart=getForZMinBnd(), kend=getForZMaxBnd(mMaxRefine);
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
		//this->mInitDone = 0;  // GRAVTEST
		// */

		myTime_t timeend = getTime();
		debMsgDirect(" done, "<<getTimeString(timeend-timestart)<<" \n");
#undef  NBFLAG
	}
}



bool LbmFsgrSolver::checkSymmetry(string idstring)
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
				if(LBMDIM==2) {
					if(msgs<maxMsgs) { msgs++;
						errMsg("EFLAG", PRINT_IJK<<"s"<<s<<" flag "<<RFLAG(lev, i,j,k,s)<<" , at "<<PRINT_VEC(inb,j,k)<<"s"<<s<<" flag "<<RFLAG(lev, inb,j,k,s) );
					}
				}
				if(markCells){ debugMarkCell(lev, i,j,k); debugMarkCell(lev, inb,j,k); }
				symm = false;
			}
			if( LBM_FLOATNEQ(QCELL(lev, i,j,k,s, dMass), QCELL(lev, inb,j,k,s, dMass)) ) { erro = true;
				if(LBMDIM==2) {
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
				if(LBMDIM==2) {
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
		//if(LBMDIM==2) this->mPanic = true; 
		//return false;
	} else {
		errMsg("SymCheck OK!", idstring<<" rho maxdiv:"<< maxdiv );
	}
	// all ok...
	return symm;
}// */


void 
LbmFsgrSolver::interpolateCellValues(
		int level,int ei,int ej,int ek,int workSet, 
		LbmFloat &retrho, LbmFloat &retux, LbmFloat &retuy, LbmFloat &retuz) 
{
	LbmFloat avgrho = 0.0;
	LbmFloat avgux = 0.0, avguy = 0.0, avguz = 0.0;
	LbmFloat cellcnt = 0.0;
	LbmFloat avgnbdf[LBM_DFNUM];
	FORDF0M { avgnbdf[m]= 0.0; }

	for(int nbl=1; nbl< this->cDfNum ; ++nbl) {
		if( (RFLAG_NB(level,ei,ej,ek,workSet,nbl) & CFFluid) || 
				((!(RFLAG_NB(level,ei,ej,ek,workSet,nbl) & CFNoInterpolSrc) ) &&
				 (RFLAG_NB(level,ei,ej,ek,workSet,nbl) & CFInter) )) { 
			cellcnt += 1.0;
			for(int rl=0; rl< this->cDfNum ; ++rl) { 
				LbmFloat nbdf =  QCELL_NB(level,ei,ej,ek, workSet,nbl, rl);
				avgnbdf[rl] += nbdf;
				avgux  += (this->dfDvecX[rl]*nbdf); 
				avguy  += (this->dfDvecY[rl]*nbdf);  
				avguz  += (this->dfDvecZ[rl]*nbdf);  
				avgrho += nbdf;
			}
		}
	}

	if(cellcnt<=0.0) {
		// no nbs? just use eq.
		//FORDF0 { QCELL(level,ei,ej,ek, workSet, l) = this->dfEquil[l]; }
		avgrho = 1.0;
		avgux = avguy = avguz = 0.0;
		//TTT mNumProblems++;
#if ELBEEM_PLUGIN!=1
		//this->mPanic=1; 
		// this can happen for worst case moving obj scenarios...
		errMsg("LbmFsgrSolver::interpolateCellValues","Cellcnt<=0.0 at "<<PRINT_VEC(ei,ej,ek)); //,SIMWORLD_GENERICERROR);
#endif // ELBEEM_PLUGIN
	} else {
		// init speed
		avgux /= cellcnt; avguy /= cellcnt; avguz /= cellcnt;
		avgrho /= cellcnt;
		FORDF0M { avgnbdf[m] /= cellcnt; } // CHECK FIXME test?
	}

	retrho = avgrho;
	retux = avgux;
	retuy = avguy;
	retuz = avguz;
} // interpolateCellValues


