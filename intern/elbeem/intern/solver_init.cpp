/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Standard LBM Factory implementation
 *
 *****************************************************************************/


#include "solver_class.h"
#include "solver_relax.h"
// for geo init FGI_ defines
#include "elbeem.h"

// helper for 2d init
#define SWAPYZ(vec) { \
		const LbmFloat tmp = (vec)[2]; \
		(vec)[2] = (vec)[1]; (vec)[1] = tmp; }


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
	LbmFloat LbmFsgrSolver::dfEquil[ dTotalNum ];

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
	LbmFloat LbmFsgrSolver::dfEquil[ dTotalNum ];

// D2Q9 end
#endif  // LBMDIM==2


// required globals
extern bool glob_mpactive;
extern int glob_mpnum, glob_mpindex;


/******************************************************************************
 * Lbm Constructor
 *****************************************************************************/
LbmFsgrSolver::LbmFsgrSolver() :
	//D(),
	mCurrentMass(0.0), mCurrentVolume(0.0),
	mNumProblems(0), 
	mAvgMLSUPS(0.0), mAvgMLSUPSCnt(0.0),
	mpPreviewSurface(NULL), 
	mTimeAdap(true), mForceTimeStepReduce(false),
	mFVHeight(0.0), mFVArea(1.0), mUpdateFVHeight(false),
	mInitSurfaceSmoothing(0), mFsSurfGenSetting(0),
	mTimestepReduceLock(0),
	mTimeSwitchCounts(0), mTimeMaxvelStepCnt(0),
	mSimulationTime(0.0), mLastSimTime(0.0),
	mMinTimestep(0.0), mMaxTimestep(0.0),
	mMaxNoCells(0), mMinNoCells(0), mAvgNumUsedCells(0),
	mObjectSpeeds(), mObjectPartslips(), mObjectMassMovnd(),
	mMOIVertices(), mMOIVerticesOld(), mMOINormals(),
	mIsoWeightMethod(1),
	mMaxRefine(1), 
	mDfScaleUp(-1.0), mDfScaleDown(-1.0),
	mInitialCsmago(0.02), // set to 0.02 for mMaxRefine==0 below and default for fine level, coarser ones are 0.03
	mDebugOmegaRet(0.0),
	mLastOmega(1e10), mLastGravity(1e10),
	mNumInvIfTotal(0), mNumFsgrChanges(0),
	mDisableStandingFluidInit(0),
	mInit2dYZ(false),
	mForceTadapRefine(-1), mCutoff(-1)
{
	mpControl = new LbmControlData();

#if LBM_INCLUDE_TESTSOLVERS==1
	mpTest = new LbmTestdata();
	mMpNum = mMpIndex = 0;
	mOrgSizeX = 0;
	mOrgStartX = 0.;
	mOrgEndX = 0.;
#endif // LBM_INCLUDE_TESTSOLVERS!=1
	mpIso = new IsoSurface( mIsoValue );

  // init equilibrium dist. func 
  LbmFloat rho=1.0;
  FORDF0 {
		dfEquil[l] = this->getCollideEq( l,rho,  0.0, 0.0, 0.0);
  }
	dfEquil[dMass] = 1.;
	dfEquil[dFfrac] = 1.;
	dfEquil[dFlux] = FLUX_INIT;

	// init LES
	int odm = 0;
	for(int m=0; m<LBMDIM; m++) { 
		for(int l=0; l<cDfNum; l++) { 
			this->lesCoeffDiag[m][l] = 
			this->lesCoeffOffdiag[m][l] = 0.0;
		}
	}
	for(int m=0; m<LBMDIM; m++) { 
		for(int n=0; n<LBMDIM; n++) { 
			for(int l=1; l<cDfNum; l++) { 
				LbmFloat em;
				switch(m) {
					case 0: em = dfDvecX[l]; break;
					case 1: em = dfDvecY[l]; break;
					case 2: em = dfDvecZ[l]; break;
					default: em = -1.0; errFatal("SMAGO1","err m="<<m, SIMWORLD_GENERICERROR);
				}
				LbmFloat en;
				switch(n) {
					case 0: en = dfDvecX[l]; break;
					case 1: en = dfDvecY[l]; break;
					case 2: en = dfDvecZ[l]; break;
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
			LbmVec(dfDvecX[dfInv[l]], dfDvecY[dfInv[l]], dfDvecZ[dfInv[l]] ) 
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
	for(int n=0;(n<cDirNum); n++) { mGaussw[n] = 0.0; }
	//for(int n=0;(n<cDirNum); n++) { 
	for(int n=0;(n<cDfNum); n++) { 
		const LbmFloat d = norm(LbmVec(dfVecX[n], dfVecY[n], dfVecZ[n]));
		LbmFloat w = expf( -alpha*d*d ) - expf( -alpha*gw*gw );
		mGaussw[n] = w;
		totGaussw += w;
	}
	for(int n=0;(n<cDirNum); n++) { 
		mGaussw[n] = mGaussw[n]/totGaussw;
	}

}

/*****************************************************************************/
/* Destructor */
/*****************************************************************************/
LbmFsgrSolver::~LbmFsgrSolver()
{
  if(!mInitDone){ debMsgStd("LbmFsgrSolver::LbmFsgrSolver",DM_MSG,"not inited...",0); return; }
#if COMPRESSGRIDS==1
	delete [] mLevel[mMaxRefine].mprsCells[1];
	mLevel[mMaxRefine].mprsCells[0] = mLevel[mMaxRefine].mprsCells[1] = NULL;
#endif // COMPRESSGRIDS==1

	for(int i=0; i<=mMaxRefine; i++) {
		for(int s=0; s<2; s++) {
			if(mLevel[i].mprsCells[s]) delete [] mLevel[i].mprsCells[s];
			if(mLevel[i].mprsFlags[s]) delete [] mLevel[i].mprsFlags[s];
		}
	}
	delete mpIso;
	if(mpPreviewSurface) delete mpPreviewSurface;
	// cleanup done during scene deletion...
	
	if(mpControl) delete mpControl;

	// always output performance estimate
	debMsgStd("LbmFsgrSolver::~LbmFsgrSolver",DM_MSG," Avg. MLSUPS:"<<(mAvgMLSUPS/mAvgMLSUPSCnt), 5);
  if(!mSilent) debMsgStd("LbmFsgrSolver::~LbmFsgrSolver",DM_MSG,"Deleted...",10);
}




/******************************************************************************
 * initilize variables fom attribute list 
 *****************************************************************************/
void LbmFsgrSolver::parseAttrList()
{
	LbmSolverInterface::parseStdAttrList();

	string matIso("default");
	matIso = mpSifAttrs->readString("material_surf", matIso, "SimulationLbm","mpIso->material", false );
	mpIso->setMaterialName( matIso );
	mOutputSurfacePreview = mpSifAttrs->readInt("surfacepreview", mOutputSurfacePreview, "SimulationLbm","mOutputSurfacePreview", false );
	mTimeAdap = mpSifAttrs->readBool("timeadap", mTimeAdap, "SimulationLbm","mTimeAdap", false );
	mDomainBound = mpSifAttrs->readString("domainbound", mDomainBound, "SimulationLbm","mDomainBound", false );
	mDomainPartSlipValue = mpSifAttrs->readFloat("domainpartslip", mDomainPartSlipValue, "SimulationLbm","mDomainPartSlipValue", false );

	mIsoWeightMethod= mpSifAttrs->readInt("isoweightmethod", mIsoWeightMethod, "SimulationLbm","mIsoWeightMethod", false );
	mInitSurfaceSmoothing = mpSifAttrs->readInt("initsurfsmooth", mInitSurfaceSmoothing, "SimulationLbm","mInitSurfaceSmoothing", false );
	mSmoothSurface = mpSifAttrs->readFloat("smoothsurface", mSmoothSurface, "SimulationLbm","mSmoothSurface", false );
	mSmoothNormals = mpSifAttrs->readFloat("smoothnormals", mSmoothNormals, "SimulationLbm","mSmoothNormals", false );
	mFsSurfGenSetting = mpSifAttrs->readInt("fssurfgen", mFsSurfGenSetting, "SimulationLbm","mFsSurfGenSetting", false );

	// refinement
	mMaxRefine = mRefinementDesired;
	mMaxRefine  = mpSifAttrs->readInt("maxrefine",  mMaxRefine ,"LbmFsgrSolver", "mMaxRefine", false);
	if(mMaxRefine<0) mMaxRefine=0;
	if(mMaxRefine>FSGR_MAXNOOFLEVELS) mMaxRefine=FSGR_MAXNOOFLEVELS-1;
	mDisableStandingFluidInit = mpSifAttrs->readInt("disable_stfluidinit", mDisableStandingFluidInit,"LbmFsgrSolver", "mDisableStandingFluidInit", false);
	mInit2dYZ = mpSifAttrs->readBool("init2dyz", mInit2dYZ,"LbmFsgrSolver", "mInit2dYZ", false);
	mForceTadapRefine = mpSifAttrs->readInt("forcetadaprefine", mForceTadapRefine,"LbmFsgrSolver", "mForceTadapRefine", false);

	// demo mode settings
	mFVHeight = mpSifAttrs->readFloat("fvolheight", mFVHeight, "LbmFsgrSolver","mFVHeight", false );
	// FIXME check needed?
	mFVArea   = mpSifAttrs->readFloat("fvolarea", mFVArea, "LbmFsgrSolver","mFArea", false );

	// debugging - skip some time...
	double starttimeskip = 0.;
	starttimeskip = mpSifAttrs->readFloat("forcestarttimeskip", starttimeskip, "LbmFsgrSolver","starttimeskip", false );
	mSimulationTime += starttimeskip;
	if(starttimeskip>0.) debMsgStd("LbmFsgrSolver::parseStdAttrList",DM_NOTIFY,"Used starttimeskip="<<starttimeskip<<", t="<<mSimulationTime, 1);

	mpControl->parseControldataAttrList(mpSifAttrs);

#if LBM_INCLUDE_TESTSOLVERS==1
	mUseTestdata = 0;
	mUseTestdata = mpSifAttrs->readBool("use_testdata", mUseTestdata,"LbmFsgrSolver", "mUseTestdata", false);
	mpTest->parseTestdataAttrList(mpSifAttrs);
#ifdef ELBEEM_PLUGIN
	mUseTestdata=1; // DEBUG
#endif // ELBEEM_PLUGIN

	mMpNum = mpSifAttrs->readInt("mpnum",  mMpNum ,"LbmFsgrSolver", "mMpNum", false);
	mMpIndex = mpSifAttrs->readInt("mpindex",  mMpIndex ,"LbmFsgrSolver", "mMpIndex", false);
	if(glob_mpactive) {
		// used instead...
		mMpNum = glob_mpnum;
		mMpIndex = glob_mpindex;
	} else {
		glob_mpnum = mMpNum;
		glob_mpindex = 0;
	}
	errMsg("LbmFsgrSolver::parseAttrList"," mpactive:"<<glob_mpactive<<", "<<glob_mpindex<<"/"<<glob_mpnum);
	if(mMpNum>0) {
		mUseTestdata=1; // needed in this case...
	}

	errMsg("LbmFsgrSolver::LBM_INCLUDE_TESTSOLVERS","Active, mUseTestdata:"<<mUseTestdata<<" ");
#else // LBM_INCLUDE_TESTSOLVERS!=1
	// not testsolvers, off by default
	mUseTestdata = 0;
	if(mFarFieldSize>=2.) mUseTestdata=1; // equiv. to test solver check
#endif // LBM_INCLUDE_TESTSOLVERS!=1

	mInitialCsmago = mpSifAttrs->readFloat("csmago", mInitialCsmago, "SimulationLbm","mInitialCsmago", false );
	// deprecated!
	float mInitialCsmagoCoarse = 0.0;
	mInitialCsmagoCoarse = mpSifAttrs->readFloat("csmago_coarse", mInitialCsmagoCoarse, "SimulationLbm","mInitialCsmagoCoarse", false );
#if USE_LES==1
#else // USE_LES==1
	debMsgStd("LbmFsgrSolver", DM_WARNING, "LES model switched off!",2);
	mInitialCsmago = 0.0;
#endif // USE_LES==1
}


/******************************************************************************
 * Initialize omegas and forces on all levels (for init/timestep change)
 *****************************************************************************/
void LbmFsgrSolver::initLevelOmegas()
{
	// no explicit settings
	mOmega = mpParam->calculateOmega(mSimulationTime);
	mGravity = vec2L( mpParam->calculateGravity(mSimulationTime) );
	mSurfaceTension = 0.; //mpParam->calculateSurfaceTension(); // unused
	if(mInit2dYZ) { SWAPYZ(mGravity); }

	// check if last init was ok
	LbmFloat gravDelta = norm(mGravity-mLastGravity);
	//errMsg("ChannelAnimDebug","t:"<<mSimulationTime<<" om:"<<mOmega<<" - lom:"<<mLastOmega<<" gv:"<<mGravity<<" - "<<mLastGravity<<" , "<<gravDelta  );
	if((mOmega == mLastOmega) && (gravDelta<=0.0)) return;

	if(mInitialCsmago<=0.0) {
		if(OPT3D==1) {
			errFatal("LbmFsgrSolver::initLevelOmegas","Csmago-LES = 0 not supported for optimized 3D version...",SIMWORLD_INITERROR); 
			return;
		}
	}

	LbmFloat  fineCsmago  = mInitialCsmago;
	LbmFloat coarseCsmago = mInitialCsmago;
	LbmFloat maxFineCsmago1    = 0.026; 
	LbmFloat maxCoarseCsmago1  = 0.029; // try stabilizing
	LbmFloat maxFineCsmago2    = 0.028; 
	LbmFloat maxCoarseCsmago2  = 0.032; // try stabilizing some more
	if((mMaxRefine==1)&&(mInitialCsmago<maxFineCsmago1)) {
		fineCsmago = maxFineCsmago1;
		coarseCsmago = maxCoarseCsmago1;
	}
	if((mMaxRefine>1)&&(mInitialCsmago<maxFineCsmago2)) {
		fineCsmago = maxFineCsmago2;
		coarseCsmago = maxCoarseCsmago2;
	}
		

	// use Tau instead of Omega for calculations
	{ // init base level
		int i = mMaxRefine;
		mLevel[i].omega    = mOmega;
		mLevel[i].timestep = mpParam->getTimestep();
		mLevel[i].lcsmago = fineCsmago; //CSMAGO_INITIAL;
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
		mLevel[i].lcsmago = coarseCsmago;
		mLevel[i].lcsmago_sqr = mLevel[i].lcsmago*mLevel[i].lcsmago;
		mLevel[i].lcnu        = (2.0* (1.0/mLevel[i].omega)-1.0) * (1.0/6.0);
	}
	
	// for lbgk
	mLevel[ mMaxRefine ].gravity = mGravity / mLevel[ mMaxRefine ].omega;
	for(int i=mMaxRefine-1; i>=0; i--) {
		// should be the same on all levels...
		// for lbgk
		mLevel[i].gravity = (mLevel[i+1].gravity * mLevel[i+1].omega) * 2.0 / mLevel[i].omega;
	}

	mLastOmega = mOmega;
	mLastGravity = mGravity;
	// debug? invalidate old values...
	mGravity = -100.0;
	mOmega = -100.0;

	for(int i=0; i<=mMaxRefine; i++) {
		if(!mSilent) {
			errMsg("LbmFsgrSolver", "Level init "<<i<<" - sizes:"<<mLevel[i].lSizex<<","<<mLevel[i].lSizey<<","<<mLevel[i].lSizez<<" offs:"<<mLevel[i].lOffsx<<","<<mLevel[i].lOffsy<<","<<mLevel[i].lOffsz 
					<<" omega:"<<mLevel[i].omega<<" grav:"<<mLevel[i].gravity<< ", "
					<<" cmsagp:"<<mLevel[i].lcsmago<<", "
					<< " ss"<<mLevel[i].timestep<<" ns"<<mLevel[i].nodeSize<<" cs"<<mLevel[i].simCellSize );
		} else {
			if(!mInitDone) {
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

/*! finish the init with config file values (allocate arrays...) */
bool LbmFsgrSolver::initializeSolverMemory()
{
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Init start... "<<mInitDone<<" "<<(void*)this,1);

	// init cppf stage
	if(mCppfStage>0) {
		mSizex *= mCppfStage;
		mSizey *= mCppfStage;
		mSizez *= mCppfStage;
	}
	if(mFsSurfGenSetting==-1) {
		// all on
		mFsSurfGenSetting = 
			 fssgNormal   | fssgNoNorth  | fssgNoSouth  | fssgNoEast   |
			 fssgNoWest   | fssgNoTop    | fssgNoBottom | fssgNoObs   ;
	}

	// size inits to force cubic cells and mult4 level dimensions
	// and make sure we dont allocate too much...
	bool memOk = false;
	int orgSx = mSizex;
	int orgSy = mSizey;
	int orgSz = mSizez;
	double sizeReduction = 1.0;
	double memEstFromFunc = -1.0;
	double memEstFine = -1.0;
	string memreqStr("");	
	bool firstMInit = true;
	int minitTries=0;
	while(!memOk) {
		minitTries++;
		initGridSizes( mSizex, mSizey, mSizez,
				mvGeoStart, mvGeoEnd, mMaxRefine, PARALLEL);

		// MPT
#if LBM_INCLUDE_TESTSOLVERS==1
		if(firstMInit) {
			mrSetup();
		}
#endif // LBM_INCLUDE_TESTSOLVERS==1
		firstMInit=false;

		calculateMemreqEstimate( mSizex, mSizey, mSizez, 
				mMaxRefine, mFarFieldSize, &memEstFromFunc, &memEstFine, &memreqStr );
		
		double memLimit;
		string memLimStr("-");
		if(sizeof(void*)==4) {
			// 32bit system, limit to 2GB
			memLimit = 2.0* 1024.0*1024.0*1024.0;
			memLimStr = string("2GB");
		} else {
			// 64bit, just take 16GB as limit for now...
			memLimit = 16.0* 1024.0*1024.0*1024.0;
			memLimStr = string("16GB");
		}

		// restrict max. chunk of 1 mem block to 1GB for windos
		bool memBlockAllocProblem = false;
		double maxDefaultMemChunk = 2.*1024.*1024.*1024.;
		//std::cerr<<" memEstFine "<< memEstFine <<" maxWin:" <<maxWinMemChunk <<" maxMac:" <<maxMacMemChunk ; // DEBUG
#ifdef WIN32
		double maxWinMemChunk = 1100.*1024.*1024.;
		if(sizeof(void *)==4 && memEstFine>maxWinMemChunk) {
			memBlockAllocProblem = true;
		}
#endif // WIN32
#ifdef __APPLE__
		double maxMacMemChunk = 1200.*1024.*1024.;
		if(memEstFine> maxMacMemChunk) {
			memBlockAllocProblem = true;
		}
#endif // Mac
		if(sizeof(void*)==4 && memEstFine>maxDefaultMemChunk) {
			// max memory chunk for 32bit systems 2gig
			memBlockAllocProblem = true;
		}

		if(memEstFromFunc>memLimit || memBlockAllocProblem) {
			sizeReduction *= 0.9;
			mSizex = (int)(orgSx * sizeReduction);
			mSizey = (int)(orgSy * sizeReduction);
			mSizez = (int)(orgSz * sizeReduction);
			debMsgStd("LbmFsgrSolver::initialize",DM_WARNING,"initGridSizes: memory limit exceeded "<<
					//memEstFromFunc<<"/"<<memLimit<<", "<<
					//memEstFine<<"/"<<maxWinMemChunk<<", "<<
					memreqStr<<"/"<<memLimStr<<", "<<
					"retrying: "<<PRINT_VEC(mSizex,mSizey,mSizez)<<" org:"<<PRINT_VEC(orgSx,orgSy,orgSz)
					, 3 );
		} else {
			memOk = true;
		} 
	}
	
	mPreviewFactor = (LbmFloat)mOutputSurfacePreview / (LbmFloat)mSizex;
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"initGridSizes: Final domain size X:"<<mSizex<<" Y:"<<mSizey<<" Z:"<<mSizez<<
	  ", Domain: "<<mvGeoStart<<":"<<mvGeoEnd<<", "<<(mvGeoEnd-mvGeoStart)<<
	  ", PointerSize: "<< sizeof(void*) << ", IntSize: "<< sizeof(int) <<
	  ", est. Mem.Req.: "<<memreqStr	,2);
	mpParam->setSize(mSizex, mSizey, mSizez);
	if((minitTries>1)&&(glob_mpnum)) { errMsg("LbmFsgrSolver::initialize","Warning!!!!!!!!!!!!!!! Original gridsize changed........."); }

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

	// perform 2D corrections...
	if(LBMDIM == 2) mSizez = 1;

	mpParam->setSimulationMaxSpeed(0.0);
	if(mFVHeight>0.0) mpParam->setFluidVolumeHeight(mFVHeight);
	mpParam->setTadapLevels( mMaxRefine+1 );

	if(mForceTadapRefine>mMaxRefine) {
		mpParam->setTadapLevels( mForceTadapRefine+1 );
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Forcing a t-adap refine level of "<<mForceTadapRefine, 6);
	}

	if(!mpParam->calculateAllMissingValues(mSimulationTime, false)) {
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
	mLevel[mMaxRefine].lSizex = mSizex;
	mLevel[mMaxRefine].lSizey = mSizey;
	mLevel[mMaxRefine].lSizez = mSizez;
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
	mLevel[ mMaxRefine ].nodeSize = ((mvGeoEnd[0]-mvGeoStart[0]) / (LbmFloat)(mSizex));
	mLevel[ mMaxRefine ].simCellSize = mpParam->getCellSize();
	mLevel[ mMaxRefine ].lcellfactor = 1.0;
	LONGINT rcellSize = ((mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez) *dTotalNum);

#if COMPRESSGRIDS==0
	mLevel[ mMaxRefine ].mprsCells[0] = new LbmFloat[ rcellSize +4 ];
	mLevel[ mMaxRefine ].mprsCells[1] = new LbmFloat[ rcellSize +4 ];
	ownMemCheck += 2 * sizeof(LbmFloat) * (rcellSize+4);
#else // COMPRESSGRIDS==0
	LONGINT compressOffset = (mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*dTotalNum*2);
	// D int tmp = ( (rcellSize +compressOffset +4)/(1024*1024) )*4;
	// D printf("Debug MEMMMM excee: %d\n", tmp);
	mLevel[ mMaxRefine ].mprsCells[1] = new LbmFloat[ rcellSize +compressOffset +4 ];
	mLevel[ mMaxRefine ].mprsCells[0] = mLevel[ mMaxRefine ].mprsCells[1]+compressOffset;
	ownMemCheck += sizeof(LbmFloat) * (rcellSize +compressOffset +4);
#endif // COMPRESSGRIDS==0

	if(!mLevel[ mMaxRefine ].mprsCells[1] || !mLevel[ mMaxRefine ].mprsCells[0]) {
		errFatal("LbmFsgrSolver::initialize","Fatal: Couldnt allocate memory (1)! Aborting...",SIMWORLD_INITERROR);
		return false;
	}

	// +4 for safety ?
	mLevel[ mMaxRefine ].mprsFlags[0] = new CellFlagType[ rcellSize/dTotalNum +4 ];
	mLevel[ mMaxRefine ].mprsFlags[1] = new CellFlagType[ rcellSize/dTotalNum +4 ];
	ownMemCheck += 2 * sizeof(CellFlagType) * (rcellSize/dTotalNum +4);
	if(!mLevel[ mMaxRefine ].mprsFlags[1] || !mLevel[ mMaxRefine ].mprsFlags[0]) {
		errFatal("LbmFsgrSolver::initialize","Fatal: Couldnt allocate memory (2)! Aborting...",SIMWORLD_INITERROR);

#if COMPRESSGRIDS==0
		delete[] mLevel[ mMaxRefine ].mprsCells[0];
		delete[] mLevel[ mMaxRefine ].mprsCells[1];
#else // COMPRESSGRIDS==0
		delete[] mLevel[ mMaxRefine ].mprsCells[1];
#endif // COMPRESSGRIDS==0
		return false;
	}

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

	// isosurface memory, use orig res values
	if(mFarFieldSize>0.) {
		ownMemCheck += (double)( (3*sizeof(int)+sizeof(float)) * ((mSizex+2)*(mSizey+2)*(mSizez+2)) );
	} else {
		// ignore 3 int slices...
		ownMemCheck += (double)( (              sizeof(float)) * ((mSizex+2)*(mSizey+2)*(mSizez+2)) );
	}

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
	mMinTimestep = mpParam->getTimestep();
	mMaxTimestep = mpParam->getTimestep();

	// init isosurf
	mpIso->setIsolevel( mIsoValue );
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata) {
		mpTest->setMaterialName( mpIso->getMaterialName() );
		delete mpIso;
		mpIso = mpTest;
		if(mpTest->mFarfMode>0) { // 3d off
			mpTest->setIsolevel(-100.0);
		} else {
			mpTest->setIsolevel( mIsoValue );
		}
	}
#endif // LBM_INCLUDE_TESTSOLVERS!=1
	// approximate feature size with mesh resolution
	float featureSize = mLevel[ mMaxRefine ].nodeSize*0.5;
	// smooth vars defined in solver_interface, set by simulation object
	// reset for invalid values...
	if((mSmoothSurface<0.)||(mSmoothSurface>50.)) mSmoothSurface = 1.;
	if((mSmoothNormals<0.)||(mSmoothNormals>50.)) mSmoothNormals = 1.;
	mpIso->setSmoothSurface( mSmoothSurface * featureSize );
	mpIso->setSmoothNormals( mSmoothNormals * featureSize );

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

	LbmVec isostart = vec2L(mvGeoStart);
	LbmVec isoend   = vec2L(mvGeoEnd);
	int twodOff = 0; // 2d slices
	if(LBMDIM==2) {
		LbmFloat sn,se;
		sn = isostart[2]+(isoend[2]-isostart[2])*0.5 - ((isoend[0]-isostart[0]) / (LbmFloat)(mSizex+1.0))*0.5;
		se = isostart[2]+(isoend[2]-isostart[2])*0.5 + ((isoend[0]-isostart[0]) / (LbmFloat)(mSizex+1.0))*0.5;
		isostart[2] = sn;
		isoend[2] = se;
		twodOff = 2;
	}
	int isosx = mSizex+2;
	int isosy = mSizey+2;
	int isosz = mSizez+2+twodOff;

	// MPT
#if LBM_INCLUDE_TESTSOLVERS==1
	//if( strstr( this->getName().c_str(), "mpfluid1" ) != NULL) {
	if( (mMpNum>0) && (mMpIndex==0) ) {
		//? mpindex==0
		// restore original value for node0
		isosx       = mOrgSizeX + 2;
		isostart[0] = mOrgStartX;
		isoend[0]   = mOrgEndX;
	}
	errMsg("LbmFsgrSolver::initialize", "MPT: gcon "<<mMpNum<<","<<mMpIndex<<" src"<< PRINT_VEC(mpTest->mGCMin.mSrcx,mpTest->mGCMin.mSrcy,mpTest->mGCMin.mSrcz)<<" dst"
			<< PRINT_VEC(mpTest->mGCMin.mDstx,mpTest->mGCMin.mDsty,mpTest->mGCMin.mDstz)<<" consize"
			<< PRINT_VEC(mpTest->mGCMin.mConSizex,mpTest->mGCMin.mConSizey,mpTest->mGCMin.mConSizez)<<" ");
	errMsg("LbmFsgrSolver::initialize", "MPT: gcon "<<mMpNum<<","<<mMpIndex<<" src"<< PRINT_VEC(mpTest->mGCMax.mSrcx,mpTest->mGCMax.mSrcy,mpTest->mGCMax.mSrcz)<<" dst"
			<< PRINT_VEC(mpTest->mGCMax.mDstx,mpTest->mGCMax.mDsty,mpTest->mGCMax.mDstz)<<" consize"
			<< PRINT_VEC(mpTest->mGCMax.mConSizex,mpTest->mGCMax.mConSizey,mpTest->mGCMax.mConSizez)<<" ");
#endif // LBM_INCLUDE_TESTSOLVERS==1

	errMsg(" SETISO ", "iso "<<isostart<<" - "<<isoend<<" "<<(((isoend[0]-isostart[0]) / (LbmFloat)(mSizex+1.0))*0.5)<<" "<<(LbmFloat)(mSizex+1.0) );
	errMsg("LbmFsgrSolver::initialize", "MPT: geo "<< mvGeoStart<<","<<mvGeoEnd<<
			" grid:"<<PRINT_VEC(mSizex,mSizey,mSizez)<<",iso:"<< PRINT_VEC(isosx,isosy,isosz) );
	mpIso->setStart( vec2G(isostart) );
	mpIso->setEnd(   vec2G(isoend) );
	LbmVec isodist = isoend-isostart;

	int isosubs = mIsoSubdivs;
	if(mFarFieldSize>1.) {
		errMsg("LbmFsgrSolver::initialize","Warning - resetting isosubdivs, using fulledge!");
		isosubs = 1;
		mpIso->setUseFulledgeArrays(true);
	}
	mpIso->setSubdivs(isosubs);

	mpIso->initializeIsosurface( isosx,isosy,isosz, vec2G(isodist) );

	// reset iso field
	for(int ak=0;ak<isosz;ak++) 
		for(int aj=0;aj<isosy;aj++) 
			for(int ai=0;ai<isosx;ai++) { *mpIso->getData(ai,aj,ak) = 0.0; }


  /* init array (set all invalid first) */
	preinitGrids();
	for(int lev=0; lev<=mMaxRefine; lev++) {
		FSGR_FORIJK_BOUNDS(lev) {
			RFLAG(lev,i,j,k,0) = RFLAG(lev,i,j,k,0) = 0; // reset for changeFlag usage
			if(!mAllfluid) {
				initEmptyCell(lev, i,j,k, CFEmpty, -1.0, -1.0); 
			} else {
				initEmptyCell(lev, i,j,k, CFFluid, 1.0, 1.0); 
			}
		}
	}


	if(LBMDIM==2) {
		if(mOutputSurfacePreview) {
			errMsg("LbmFsgrSolver::init","No preview in 2D allowed!");
			mOutputSurfacePreview = 0; }
	}
	if((glob_mpactive) && (glob_mpindex>0)) {
		mOutputSurfacePreview = 0;
	}

#if LBM_USE_GUI==1
	if(mOutputSurfacePreview) {
		errMsg("LbmFsgrSolver::init","No preview in GUI mode... mOutputSurfacePreview=0");
		mOutputSurfacePreview = 0; }
#endif // LBM_USE_GUI==1
	if(mOutputSurfacePreview) {
		// same as normal one, but use reduced size
		mpPreviewSurface = new IsoSurface( mIsoValue );
		mpPreviewSurface->setMaterialName( mpPreviewSurface->getMaterialName() );
		mpPreviewSurface->setIsolevel( mIsoValue );
		// usually dont display for rendering
		mpPreviewSurface->setVisible( false );

		mpPreviewSurface->setStart( vec2G(isostart) );
		mpPreviewSurface->setEnd(   vec2G(isoend) );
		LbmVec pisodist = isoend-isostart;
		LbmFloat pfac = mPreviewFactor;
		mpPreviewSurface->initializeIsosurface( (int)(pfac*mSizex)+2, (int)(pfac*mSizey)+2, (int)(pfac*mSizez)+2, vec2G(pisodist) );
		//mpPreviewSurface->setName( getName() + "preview" );
		mpPreviewSurface->setName( "preview" );
	
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Preview with sizes "<<(pfac*mSizex)<<","<<(pfac*mSizey)<<","<<(pfac*mSizez)<<" enabled",10);
	}

	// init defaults
	mAvgNumUsedCells = 0;
	mFixMass= 0.0;
	return true;
}

/*! init solver arrays */
bool LbmFsgrSolver::initializeSolverGrids() {
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
	if(mDomainBound.find(string("free")) != string::npos) {
		domainBoundType = CFBnd | CFBndFreeslip;
  	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Domain Boundary Type: FreeSlip, value:"<<mDomainBound,10);
	} else if(mDomainBound.find(string("part")) != string::npos) {
		domainBoundType = CFBnd | CFBndPartslip; // part slip type
  	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Domain Boundary Type: PartSlip ("<<mDomainPartSlipValue<<"), value:"<<mDomainBound,10);
	} else { 
		domainBoundType = CFBnd | CFBndNoslip;
  	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Domain Boundary Type: NoSlip, value:"<<mDomainBound,10);
	}

	// use ar[numobjs] as entry for domain (used e.g. for mDomainPartSlipValue in mObjectPartslips)
	int domainobj = (int)(mpGiObjects->size());
	domainBoundType |= (domainobj<<24);
	//for(int i=0; i<(int)(domainobj+0); i++) {
		//errMsg("GEOIN","i"<<i<<" "<<(*mpGiObjects)[i]->getName());
		//if((*mpGiObjects)[i] == mpIso) { //check...
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
	// vortt
#if LBM_INCLUDE_TESTSOLVERS==1
	if(( strstr( this->getName().c_str(), "vorttfluid" ) != NULL) && (LBMDIM==2)) {
		errMsg("VORTT","init");
		int level=mMaxRefine;
		int cx = mLevel[level].lSizex/2;
		int cyo = mLevel[level].lSizey/2;
		int sx = mLevel[level].lSizex/8;
		int sy = mLevel[level].lSizey/8;
		LbmFloat rho = 1.;
		LbmFloat rhomass = 1.;
		LbmFloat uFactor = 0.15;
		LbmFloat vdist = 1.0;

		int cy1=cyo-(int)(vdist*sy);
		int cy2=cyo+(int)(vdist*sy);

		//for(int j=cy-sy;j<cy+sy;j++) for(int i=cx-sx;i<cx+sx;i++) {      
		for(int j=1;j<mLevel[level].lSizey-1;j++)
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				LbmFloat d1 = norm(LbmVec(cx,cy1,0.)-LbmVec(i,j,0));
				LbmFloat d2 = norm(LbmVec(cx,cy2,0.)-LbmVec(i,j,0));
				bool in1 = (d1<=(LbmFloat)(sx));
				bool in2 = (d2<=(LbmFloat)(sx));
				LbmVec uvec(0.);
			  LbmVec v1 = getNormalized( cross( LbmVec(cx,cy1,0.)-LbmVec(i,j,0), LbmVec(0.,0.,1.)) )*  uFactor;
			  LbmVec v2 = getNormalized( cross( LbmVec(cx,cy2,0.)-LbmVec(i,j,0), LbmVec(0.,0.,1.)) )*  uFactor;
				LbmFloat w1=1., w2=1.;
				if(!in1) w1=(LbmFloat)(sx)/(1.5*d1);
				if(!in2) w2=(LbmFloat)(sx)/(1.5*d2);
				if(!in1) w1=0.; if(!in2) w2=0.; // sharp falloff
			  uvec += v1*w1;
			  uvec += v2*w2;
				initVelocityCell(level, i,j,0, CFFluid, rho, rhomass, uvec );
				//errMsg("VORTT","init uvec"<<uvec);
			}

	}
#endif // LBM_INCLUDE_TESTSOLVERS==1

	//if(getGlobalBakeState()<0) { CAUSE_PANIC; errMsg("LbmFsgrSolver::initialize","Got abort signal1, causing panic, aborting..."); return false; }

	// prepare interface cells
	initFreeSurfaces();
	initStandingFluidGradient();

	// perform first step to init initial mass
	mInitialMass = 0.0;
	int inmCellCnt = 0;
	FSGR_FORIJK1(mMaxRefine) {
		if( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr)& CFFluid) {
			LbmFloat fluidRho = QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, 0); 
			FORDF1 { fluidRho += QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, l); }
			mInitialMass += fluidRho;
			inmCellCnt ++;
		} else if( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr)& CFInter) {
			mInitialMass += QCELL(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, dMass);
			inmCellCnt ++;
		}
	}
	mCurrentVolume = mCurrentMass = mInitialMass;

	ParamVec cspv = mpParam->calculateCellSize();
	if(LBMDIM==2) cspv[2] = 1.0;
	inmCellCnt = 1;
	double nrmMass = (double)mInitialMass / (double)(inmCellCnt) *cspv[0]*cspv[1]*cspv[2] * 1000.0;
	debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Initial Mass:"<<mInitialMass<<" normalized:"<<nrmMass, 3);
	mInitialMass = 0.0; // reset, and use actual value after first step

	//mStartSymm = false;
#if ELBEEM_PLUGIN!=1
	if((LBMDIM==2)&&(mSizex<200)) {
		if(!checkSymmetry("init")) {
			errMsg("LbmFsgrSolver::initialize","Unsymmetric init...");
		} else {
			errMsg("LbmFsgrSolver::initialize","Symmetric init!");
		}
	}
#endif // ELBEEM_PLUGIN!=1
	return true;
}


/*! prepare actual simulation start, setup viz etc */
bool LbmFsgrSolver::initializeSolverPostinit() {
	// coarsen region
	myTime_t fsgrtstart = getTime(); 
	for(int lev=mMaxRefine-1; lev>=0; lev--) {
		debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Coarsening level "<<lev<<".",8);
		adaptGrid(lev);
		coarseRestrictFromFine(lev);
		adaptGrid(lev);
		coarseRestrictFromFine(lev);
	}
	markedClearList();
	myTime_t fsgrtend = getTime(); 
	if(!mSilent){ debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"FSGR init done ("<< getTimeString(fsgrtend-fsgrtstart)<<"), changes:"<<mNumFsgrChanges , 10 ); }
	mNumFsgrChanges = 0;

	for(int l=0; l<cDirNum; l++) { 
		LbmFloat area = 0.5 * 0.5 *0.5;
		if(LBMDIM==2) area = 0.5 * 0.5;

		if(dfVecX[l]!=0) area *= 0.5;
		if(dfVecY[l]!=0) area *= 0.5;
		if(dfVecZ[l]!=0) area *= 0.5;
		mFsgrCellArea[l] = area;
	} // l

	// make sure both sets are ok
	// copy from other to curr
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		RFLAG(lev, i,j,k,mLevel[lev].setOther) = RFLAG(lev, i,j,k,mLevel[lev].setCurr);
	} } // first copy flags */


	// old mpPreviewSurface init
	//if(getGlobalBakeState()<0) { CAUSE_PANIC; errMsg("LbmFsgrSolver::initialize","Got abort signal2, causing panic, aborting..."); return false; }
	// make sure fill fracs are right for first surface generation
	stepMain();

	// prepare once...
	mpIso->setParticles(mpParticles, mPartDropMassSub);
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Iso Settings, subdivs="<<mpIso->getSubdivs()<<", partsize="<<mPartDropMassSub, 9);
	prepareVisualization();
	// copy again for stats counting
	for(int lev=0; lev<=mMaxRefine; lev++) {
	FSGR_FORIJK_BOUNDS(lev) {
		RFLAG(lev, i,j,k,mLevel[lev].setOther) = RFLAG(lev, i,j,k,mLevel[lev].setCurr);
	} } // first copy flags */


	// now really done...
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"SurfaceGen: SmsOrg("<<mSmoothSurface<<","<<mSmoothNormals<< /*","<<featureSize<<*/ "), Iso("<<mpIso->getSmoothSurface()<<","<<mpIso->getSmoothNormals()<<") ",10);
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Init done ... ",10);
	mInitDone = 1;

	// init fluid control
	initCpdata();

#if LBM_INCLUDE_TESTSOLVERS==1
	initTestdata();
#endif // ELBEEM_PLUGIN!=1
	// not inited? dont use...
	if(mCutoff<0) mCutoff=0;

	initParticles();
	return true;
}



// macros for mov obj init
#if LBMDIM==2

#define POS2GRID_CHECK(vec,n) \
				monTotal++;\
				int k=(int)( ((vec)[n][2]-iniPos[2])/dvec[2] +0.0); \
				if(k!=0) continue; \
				const int i=(int)( ((vec)[n][0]-iniPos[0])/dvec[0] +0.0); \
				if(i<=0) continue; \
				if(i>=mLevel[level].lSizex-1) continue; \
				const int j=(int)( ((vec)[n][1]-iniPos[1])/dvec[1] +0.0); \
				if(j<=0) continue; \
				if(j>=mLevel[level].lSizey-1) continue;  \

#else // LBMDIM -> 3
#define POS2GRID_CHECK(vec,n) \
				monTotal++;\
				const int i=(int)( ((vec)[n][0]-iniPos[0])/dvec[0] +0.0); \
				if(i<=0) continue; \
				if(i>=mLevel[level].lSizex-1) continue; \
				const int j=(int)( ((vec)[n][1]-iniPos[1])/dvec[1] +0.0); \
				if(j<=0) continue; \
				if(j>=mLevel[level].lSizey-1) continue; \
				const int k=(int)( ((vec)[n][2]-iniPos[2])/dvec[2] +0.0); \
				if(k<=0) continue; \
				if(k>=mLevel[level].lSizez-1) continue; \

#endif // LBMDIM 

// calculate object velocity from vert arrays in objvel vec
#define OBJVEL_CALC \
				LbmVec objvel = vec2L((mMOIVertices[n]-mMOIVerticesOld[n]) /dvec); { \
				const LbmFloat usqr = (objvel[0]*objvel[0]+objvel[1]*objvel[1]+objvel[2]*objvel[2])*1.5; \
				USQRMAXCHECK(usqr, objvel[0],objvel[1],objvel[2], mMaxVlen, mMxvx,mMxvy,mMxvz); \
				if(usqr>maxusqr) { \
					/* cutoff at maxVelVal */ \
					for(int jj=0; jj<3; jj++) { \
						if(objvel[jj]>0.) objvel[jj] =  maxVelVal;  \
						if(objvel[jj]<0.) objvel[jj] = -maxVelVal; \
					} \
				} } \
				if(ntype&(CFBndFreeslip)) { \
					const LbmFloat dp=dot(objvel, vec2L((*pNormals)[n]) ); \
					const LbmVec oldov=objvel; /*DEBUG*/ \
					objvel = vec2L((*pNormals)[n]) *dp; \
					/* if((j==24)&&(n%5==2)) errMsg("FSBT","n"<<n<<" v"<<objvel<<" nn"<<(*pNormals)[n]<<" dp"<<dp<<" oldov"<<oldov ); */ \
				} \
				else if(ntype&(CFBndPartslip)) { \
					const LbmFloat dp=dot(objvel, vec2L((*pNormals)[n]) ); \
					const LbmVec oldov=objvel; /*DEBUG*/ \
					/* if((j==24)&&(n%5==2)) errMsg("FSBT","n"<<n<<" v"<<objvel<<" nn"<<(*pNormals)[n]<<" dp"<<dp<<" oldov"<<oldov ); */ \
					const LbmFloat partv = mObjectPartslips[OId]; \
					/*errMsg("PARTSLIP_DEBUG","l="<<l<<" ccel="<<RAC(ccel, dfInv[l] )<<" partv="<<partv<<",id="<<(int)(mnbf>>24)<<" newval="<<newval ); / part slip debug */ \
					/* m[l] = (RAC(ccel, dfInv[l] ) ) * partv + newval * (1.-partv); part slip */ \
					objvel = objvel*partv + vec2L((*pNormals)[n]) *dp*(1.-partv); \
				}

#define TTT \


/*****************************************************************************/
//! init moving obstacles for next sim step sim 
/*****************************************************************************/
void LbmFsgrSolver::initMovingObstacles(bool staticInit) {
	myTime_t monstart = getTime();

	// movobj init
	const int level = mMaxRefine;
	const int workSet = mLevel[level].setCurr;
	const int otherSet = mLevel[level].setOther;
	LbmFloat sourceTime = mSimulationTime; // should be equal to mLastSimTime!
	// for debugging - check targetTime check during DEFAULT STREAM
	LbmFloat targetTime = mSimulationTime + mpParam->getTimestep();
	if(mLastSimTime == targetTime) {
		debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_WARNING,"Called for same time! (t="<<mSimulationTime<<" , targett="<<targetTime<<")", 1);
		return;
	}
	//debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_WARNING,"time: "<<mSimulationTime<<" lasttt:"<<mLastSimTime,10);
	//if(mSimulationTime!=mLastSimTime) errMsg("LbmFsgrSolver::initMovingObstacles","time: "<<mSimulationTime<<" lasttt:"<<mLastSimTime);

	const LbmFloat maxVelVal = 0.1666;
	const LbmFloat maxusqr = maxVelVal*maxVelVal*3. *1.5;

	LbmFloat rhomass = 0.0;
	CellFlagType otype = CFInvalid; // verify type of last step, might be non moving obs!
	CellFlagType ntype = CFInvalid;
	// WARNING - copied from geo init!
	int numobjs = (int)(mpGiObjects->size());
	ntlVec3Gfx dvec = ntlVec3Gfx(mLevel[level].nodeSize); //dvec*1.0;
	// 2d display as rectangles
	ntlVec3Gfx iniPos(0.0);
	if(LBMDIM==2) {
		dvec[2] = 1.0; 
		iniPos = (mvGeoStart + ntlVec3Gfx( 0.0, 0.0, (mvGeoEnd[2]-mvGeoStart[2])*0.5 ))-(dvec*0.0);
	} else {
		iniPos = (mvGeoStart + ntlVec3Gfx( 0.0 ))-(dvec*0.0);
	}
	
	if( (int)mObjectMassMovnd.size() < numobjs) {
		for(int i=mObjectMassMovnd.size(); i<numobjs; i++) {
			mObjectMassMovnd.push_back(0.);
		}
	}
	
	// stats
	int monPoints=0, monObsts=0, monFluids=0, monTotal=0, monTrafo=0;
	int nbored;
	for(int OId=0; OId<numobjs; OId++) {
		ntlGeometryObject *obj = (*mpGiObjects)[OId];
		bool skip = false;
		if(obj->getGeoInitId() != mLbmInitId) skip=true;
		if( (!staticInit) && (!obj->getIsAnimated()) ) skip=true;
		if( ( staticInit) && ( obj->getIsAnimated()) ) skip=true;
		if(skip) continue;
		debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG," obj "<<obj->getName()<<" skip:"<<skip<<", static:"<<staticInit<<" anim:"<<obj->getIsAnimated()<<" gid:"<<obj->getGeoInitId()<<" simgid:"<<mLbmInitId, 10);

		if( (obj->getGeoInitType()&FGI_ALLBOUNDS) || 
				(obj->getGeoInitType()&FGI_FLUID) && staticInit ) {

			otype = ntype = CFInvalid;
			switch(obj->getGeoInitType()) {
				/* case FGI_BNDPART: // old, use noslip for moving part/free objs
				case FGI_BNDFREE: 
					if(!staticInit) {
						errMsg("LbmFsgrSolver::initMovingObstacles","Warning - moving free/part slip objects NYI "<<obj->getName() );
						otype = ntype = CFBnd|CFBndNoslip;
					} else {
						if(obj->getGeoInitType()==FGI_BNDPART) otype = ntype = CFBnd|CFBndPartslip;
						if(obj->getGeoInitType()==FGI_BNDFREE) otype = ntype = CFBnd|CFBndFreeslip;
					}
					break; 
					// off */
				case FGI_BNDPART: rhomass = BND_FILL;
					otype = ntype = CFBnd|CFBndPartslip|(OId<<24);
					break;
				case FGI_BNDFREE: rhomass = BND_FILL;
					otype = ntype = CFBnd|CFBndFreeslip|(OId<<24);
					break;
					// off */
				case FGI_BNDNO:   rhomass = BND_FILL;
					otype = ntype = CFBnd|CFBndNoslip|(OId<<24);
					break;
				case FGI_FLUID: 
					otype = ntype = CFFluid; 
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
			if(ntype==CFInvalid){ errMsg("LbmFsgrSolver::initMovingObstacles","Invalid obj type "<<obj->getGeoInitType()); continue; }
			if((!active) && (otype&(CFMbndOutflow|CFMbndInflow)) ) continue;

			// copied from  recalculateObjectSpeeds
			mObjectSpeeds[OId] = vec2L(mpParam->calculateLattVelocityFromRw( vec2P( (*mpGiObjects)[OId]->getInitialVelocity(mSimulationTime) )));
			debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG,"id"<<OId<<" "<<obj->getName()<<" inivel set to "<< mObjectSpeeds[OId]<<", unscaled:"<< (*mpGiObjects)[OId]->getInitialVelocity(mSimulationTime) ,10 );

			//vector<ntlVec3Gfx> tNormals;
			vector<ntlVec3Gfx> *pNormals = NULL;
			mMOINormals.clear();
			if(ntype&(CFBndFreeslip|CFBndPartslip)) { pNormals = &mMOINormals; }

			mMOIVertices.clear();
			if(obj->getMeshAnimated()) { 
				// do two full update
				// TODO tNormals handling!?
				mMOIVerticesOld.clear();
				obj->initMovingPointsAnim(sourceTime,mMOIVerticesOld, targetTime, mMOIVertices, pNormals,  mLevel[mMaxRefine].nodeSize, mvGeoStart, mvGeoEnd);
				monTrafo += mMOIVerticesOld.size();
				obj->applyTransformation(sourceTime, &mMOIVerticesOld,pNormals, 0, mMOIVerticesOld.size(), false );
				monTrafo += mMOIVertices.size();
				obj->applyTransformation(targetTime, &mMOIVertices,NULL /* no old normals needed */, 0, mMOIVertices.size(), false );
			} else {
				// only do transform update
				obj->getMovingPoints(mMOIVertices,pNormals);
				mMOIVerticesOld = mMOIVertices;
				// WARNING - assumes mSimulationTime is global!?
				obj->applyTransformation(targetTime, &mMOIVertices,pNormals, 0, mMOIVertices.size(), false );
				monTrafo += mMOIVertices.size();

				// correct flags from last position, but extrapolate
				// velocity to next timestep
				obj->applyTransformation(sourceTime, &mMOIVerticesOld, NULL /* no old normals needed */, 0, mMOIVerticesOld.size(), false );
				monTrafo += mMOIVerticesOld.size();
			}

			// object types
			if(ntype&CFBnd){

				// check if object is moving at all
				if(obj->getIsAnimated()) {
					ntlVec3Gfx objMaxVel = obj->calculateMaxVel(sourceTime,targetTime);
					// FIXME?
					if(normNoSqrt(objMaxVel)>0.0) { ntype |= CFBndMoving; }
					// get old type - CHECK FIXME , timestep could have changed - cause trouble?
					ntlVec3Gfx oldobjMaxVel = obj->calculateMaxVel(sourceTime - mpParam->getTimestep(),sourceTime);
					if(normNoSqrt(oldobjMaxVel)>0.0) { otype |= CFBndMoving; }
				}
				if(obj->getMeshAnimated()) { ntype |= CFBndMoving; otype |= CFBndMoving; }
				CellFlagType rflagnb[27];
				LbmFloat massCheck = 0.;
				int massReinits=0;				
				bool fillCells = (mObjectMassMovnd[OId]<=-1.);
				LbmFloat impactCorrFactor = obj->getGeoImpactFactor(targetTime);
				

				// first pass - set new obs. cells
				if(active) {
					for(size_t n=0; n<mMOIVertices.size(); n++) {
						//errMsg("initMovingObstacles_Debug","OId"<<OId<<" n"<<n<<" -> "<<PRINT_IJK);
						POS2GRID_CHECK(mMOIVertices,n);
						//{ errMsg("initMovingObstacles_Debug","OId"<<OId<<" n"<<n<<" -> "<<PRINT_IJK<<", t="<<targetTime); }
						if(QCELL(level, i,j,k, workSet, dFlux)==targetTime) continue;
						monPoints++;
						
						// check mass
						if(RFLAG(level, i,j,k, workSet)&(CFFluid)) {
							FORDF0 { massCheck -= QCELL(level, i,j,k, workSet, l); }
							massReinits++;
						}
						else if(RFLAG(level, i,j,k, workSet)&(CFInter)) {
							massCheck -= QCELL(level, i,j,k, workSet, dMass);
							massReinits++;
						}

						RFLAG(level, i,j,k, workSet) = ntype;
						FORDF1 {
							//CellFlagType flag = RFLAG_NB(level, i,j,k,workSet,l);
							rflagnb[l] = RFLAG_NB(level, i,j,k,workSet,l);
							if(rflagnb[l]&(CFFluid|CFInter)) {
								rflagnb[l] &= (~CFNoBndFluid); // remove CFNoBndFluid flag
								RFLAG_NB(level, i,j,k,workSet,l) &= rflagnb[l]; 
							}
						}
						LbmFloat *dstCell = RACPNT(level, i,j,k,workSet);
						RAC(dstCell,0) = 0.0;
						if(ntype&CFBndMoving) {
							OBJVEL_CALC;
							
							// compute fluid acceleration
							FORDF1 {
								if(rflagnb[l]&(CFFluid|CFInter)) { 
									const LbmFloat ux = dfDvecX[l]*objvel[0];
									const LbmFloat uy = dfDvecY[l]*objvel[1];
									const LbmFloat uz = dfDvecZ[l]*objvel[2];

									LbmFloat factor = 2. * dfLength[l] * 3.0 * (ux+uy+uz); // 
									if(ntype&(CFBndFreeslip|CFBndPartslip)) {
										// missing, diag mass checks...
										//if(l<=LBMDIM*2) factor *= 1.4142;
										factor *= 2.0; // TODO, optimize
									} else {
										factor *= 1.2; // TODO, optimize
									}
									factor *= impactCorrFactor; // use manual tweaking channel
									RAC(dstCell,l) = factor;
									massCheck += factor;
								} else {
									RAC(dstCell,l) = 0.;
								}
							}

#if NEWDIRVELMOTEST==1
							FORDF1 { RAC(dstCell,l) = 0.; }
							RAC(dstCell,dMass)  = objvel[0];
							RAC(dstCell,dFfrac) = objvel[1];
							RAC(dstCell,dC)     = objvel[2];
#endif // NEWDIRVELMOTEST==1
						} else {
							FORDF1 { RAC(dstCell,l) = 0.0; }
						}
						RAC(dstCell, dFlux) = targetTime;
						//errMsg("initMovingObstacles_Debug","OId"<<OId<<" n"<<n<<" -> "<<PRINT_IJK" dflt="<<RAC(dstCell, dFlux) );
						monObsts++;
					} // points
				} // bnd, is active?

				// second pass, remove old ones
				// warning - initEmptyCell et. al dont overwrite OId or persist flags...
				if(wasActive) {
					for(size_t n=0; n<mMOIVerticesOld.size(); n++) {
						POS2GRID_CHECK(mMOIVerticesOld,n);
						monPoints++;
						if((RFLAG(level, i,j,k, workSet) == otype) &&
							 (QCELL(level, i,j,k, workSet, dFlux) != targetTime)) {
							// from mainloop
							nbored = 0;
							// TODO: optimize for OPT3D==0
							FORDF1 {
								//rflagnb[l] = RFLAG_NB(level, i,j,k,workSet,l);
								rflagnb[l] = RFLAG_NB(level, i,j,k,otherSet,l); // test use other set to not have loop dependance
								nbored |= rflagnb[l];
							} 
							CellFlagType settype = CFInvalid;
							if(nbored&CFFluid) {
								settype = CFInter|CFNoInterpolSrc; 
								rhomass = 1.5;
								if(!fillCells) rhomass = 0.;

								OBJVEL_CALC;
								if(!(nbored&CFEmpty)) { settype=CFFluid|CFNoInterpolSrc; rhomass=1.; }

								// new interpolate values
								LbmFloat avgrho = 0.0;
								LbmFloat avgux = 0.0, avguy = 0.0, avguz = 0.0;
								interpolateCellValues(level,i,j,k,workSet, avgrho,avgux,avguy,avguz);
								initVelocityCell(level, i,j,k, settype, avgrho, rhomass, LbmVec(avgux,avguy,avguz) );
								//errMsg("NMOCIT"," at "<<PRINT_IJK<<" "<<avgrho<<" "<<norm(LbmVec(avgux,avguy,avguz))<<" "<<LbmVec(avgux,avguy,avguz) );
								massCheck += rhomass;
							} else {
								settype = CFEmpty; rhomass = 0.0;
								initEmptyCell(level, i,j,k, settype, 1., rhomass );
							}
							monFluids++;
							massReinits++;
						} // flag & simtime
					}
				}  // wasactive

				// only compute mass transfer when init is done
				if(mInitDone) {
					errMsg("initMov","dccd\n\nMassd test "<<obj->getName()<<" dccd massCheck="<<massCheck<<" massReinits"<<massReinits<<
						" fillCells"<<fillCells<<" massmovbnd:"<<mObjectMassMovnd[OId]<<" inim:"<<mInitialMass ); 
					mObjectMassMovnd[OId] += massCheck;
				}
			} // bnd, active

			else if(ntype&CFFluid){
				// second static init pass
				if(staticInit) {
					//debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG," obj "<<obj->getName()<<" verts"<<mMOIVertices.size() ,9);
					CellFlagType setflflag = CFFluid; //|(OId<<24);
					for(size_t n=0; n<mMOIVertices.size(); n++) {
						POS2GRID_CHECK(mMOIVertices,n);
						if(RFLAG(level, i,j,k, workSet)&(CFMbndInflow|CFMbndOutflow)){ continue; }
						if(RFLAG(level, i,j,k, workSet)&(CFEmpty)) {
							//changeFlag(level, i,j,k, workSet, setflflag);
							initVelocityCell(level, i,j,k, setflflag, 1., 1., mObjectSpeeds[OId] );
						} 
						//else if(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter)) { changeFlag(level, i,j,k, workSet, RFLAG(level, i,j,k, workSet)|set2Flag); }
					}
				} // second static inflow pass
			} // fluid

			else if(ntype&CFMbndInflow){
				// inflow pass - add new fluid cells
				// this is slightly different for standing inflows,
				// as the fluid is forced to the desired velocity inside as 
				// well...
				const LbmFloat iniRho = 1.0;
				const LbmVec vel(mObjectSpeeds[OId]);
				const LbmFloat usqr = (vel[0]*vel[0]+vel[1]*vel[1]+vel[2]*vel[2])*1.5;
				USQRMAXCHECK(usqr,vel[0],vel[1],vel[2], mMaxVlen, mMxvx,mMxvy,mMxvz);
				//errMsg("LbmFsgrSolver::initMovingObstacles","id"<<OId<<" "<<obj->getName()<<" inflow "<<staticInit<<" "<<mMOIVertices.size() );
				
				for(size_t n=0; n<mMOIVertices.size(); n++) {
					POS2GRID_CHECK(mMOIVertices,n);
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
					for(size_t n=0; n<mMOIVertices.size(); n++) {
						POS2GRID_CHECK(mMOIVertices,n);
						if(RFLAG(level, i,j,k, workSet)&(CFMbndInflow|CFMbndOutflow)){ continue; }
						if(RFLAG(level, i,j,k, workSet)&(CFEmpty)) {
							forceChangeFlag(level, i,j,k, workSet, set2Flag);
						} else if(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter)) {
							forceChangeFlag(level, i,j,k, workSet, 
									(RFLAG(level, i,j,k, workSet)&CFNoPersistMask)|set2Flag);
						}
					}
				} // second static inflow pass

			} // inflow

			else if(ntype&CFMbndOutflow){
				const LbmFloat iniRho = 0.0;
				for(size_t n=0; n<mMOIVertices.size(); n++) {
					POS2GRID_CHECK(mMOIVertices,n);
					// FIXME check fluid/inter cells for non-static!?
					if(!(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter))) {
						if((staticInit)&&(RFLAG(level, i,j,k, workSet)==CFEmpty)) {
							forceChangeFlag(level, i,j,k, workSet, CFMbndOutflow); }
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
					for(size_t n=0; n<mMOIVertices.size(); n++) {
						POS2GRID_CHECK(mMOIVertices,n);
						if(RFLAG(level, i,j,k, workSet)&(CFMbndInflow|CFMbndOutflow)){ continue; }
						if(RFLAG(level, i,j,k, workSet)&(CFEmpty)) {
							forceChangeFlag(level, i,j,k, workSet, set2Flag);
						} else if(RFLAG(level, i,j,k, workSet)&(CFFluid|CFInter)) {
							forceChangeFlag(level, i,j,k, workSet, 
									(RFLAG(level, i,j,k, workSet)&CFNoPersistMask)|set2Flag);
						}
					}
				} // second static outflow pass
			} // outflow

		} // allbound check
	} // OId


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
	
#undef POS2GRID_CHECK
	myTime_t monend = getTime();
	if(monend-monstart>0) debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG,"Total: "<<monTotal<<" Points :"<<monPoints<<" ObstInits:"<<monObsts<<" FlInits:"<<monFluids<<" Trafos:"<<monTotal<<", took "<<getTimeString(monend-monstart), 7);
	mLastSimTime = targetTime;
}


// geoinit position

#define GETPOS(i,j,k)  \
	ntlVec3Gfx ggpos = \
		ntlVec3Gfx( iniPos[0]+ dvec[0]*(gfxReal)(i), \
			          iniPos[1]+ dvec[1]*(gfxReal)(j), \
			          iniPos[2]+ dvec[2]*(gfxReal)(k) ); \
  if((LBMDIM==2)&&(mInit2dYZ)) { SWAPYZ(ggpos); }

/*****************************************************************************/
/*! perform geometry init (if switched on) */
/*****************************************************************************/
extern int globGeoInitDebug; //solver_interface
bool LbmFsgrSolver::initGeometryFlags() {
	int level = mMaxRefine;
	myTime_t geotimestart = getTime(); 
	ntlGeometryObject *pObj;
	ntlVec3Gfx dvec = ntlVec3Gfx(mLevel[level].nodeSize); //dvec*1.0;
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Performing geometry init ("<< mLbmInitId <<") v"<<dvec,3);
	// WARNING - copied to movobj init!

	/* init object velocities, this has always to be called for init */
	initGeoTree();
	if(mAllfluid) { 
		freeGeoTree();
		return true; }

	// make sure moving obstacles are inited correctly
	// preinit moving obj points
	int numobjs = (int)(mpGiObjects->size());
	for(int o=0; o<numobjs; o++) {
		ntlGeometryObject *obj = (*mpGiObjects)[o];
		//debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG," obj "<<obj->getName()<<" type "<<obj->getGeoInitType()<<" anim"<<obj->getIsAnimated()<<" "<<obj->getVolumeInit() ,9);
		if(
				((obj->getGeoInitType()&FGI_ALLBOUNDS) && (obj->getIsAnimated())) ||
				(obj->getVolumeInit()&VOLUMEINIT_SHELL) ) {
			if(!obj->getMeshAnimated()) { 
				debMsgStd("LbmFsgrSolver::initMovingObstacles",DM_MSG," obj "<<obj->getName()<<" type "<<obj->getGeoInitType()<<" anim"<<obj->getIsAnimated()<<" "<<obj->getVolumeInit() ,9);
				obj->initMovingPoints(mSimulationTime, mLevel[mMaxRefine].nodeSize);
			}
		}
	}

	// max speed init
	ntlVec3Gfx maxMovobjVelRw = getGeoMaxMovementVelocity( mSimulationTime, mpParam->getTimestep() );
	ntlVec3Gfx maxMovobjVel = vec2G( mpParam->calculateLattVelocityFromRw( vec2P( maxMovobjVelRw )) );
	mpParam->setSimulationMaxSpeed( norm(maxMovobjVel) + norm(mLevel[level].gravity) );
	LbmFloat allowMax = mpParam->getTadapMaxSpeed();  // maximum allowed velocity
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Maximum Velocity from geo init="<< maxMovobjVel <<" from mov. obj.="<<maxMovobjVelRw<<" , allowed Max="<<allowMax ,5);
	if(mpParam->getSimulationMaxSpeed() > allowMax) {
		// similar to adaptTimestep();
		LbmFloat nextmax = mpParam->getSimulationMaxSpeed();
		LbmFloat newdt = mpParam->getTimestep() * (allowMax / nextmax); // newtr
		debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Performing reparametrization, newdt="<< newdt<<" prevdt="<< mpParam->getTimestep() <<" ",5);
		mpParam->setDesiredTimestep( newdt );
		mpParam->calculateAllMissingValues( mSimulationTime, mSilent );
		maxMovobjVel = vec2G( mpParam->calculateLattVelocityFromRw( vec2P(getGeoMaxMovementVelocity(
		                      mSimulationTime, mpParam->getTimestep() )) ));
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
		iniPos =(mvGeoStart + ntlVec3Gfx( 0.0, 0.0, (mvGeoEnd[2]-mvGeoStart[2])*0.5 ))+(dvec*0.5);
		//if(mInit2dYZ) { SWAPYZ(mGravity); for(int lev=0; lev<=mMaxRefine; lev++){ SWAPYZ( mLevel[lev].gravity ); } }
	} else {
		iniPos =(mvGeoStart + ntlVec3Gfx( 0.0 ))+(dvec*0.5);
	}


	// first init boundary conditions
	// invalid cells are set to empty afterwards
	// TODO use floop macros!?
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int j=1;j<mLevel[level].lSizey-1;j++) {
			for(int i=1;i<mLevel[level].lSizex-1;i++) {
				ntype = CFInvalid;
				
				GETPOS(i,j,k);
				const bool inside = geoInitCheckPointInside( ggpos, FGI_ALLBOUNDS, OId, distance);
				if(inside) {
					pObj = (*mpGiObjects)[OId];
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
					case FGI_BNDPART: 
						rhomass = BND_FILL;
						ntype = CFBnd|CFBndPartslip; break;
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
				GETPOS(i,j,k);
				const bool inside = geoInitCheckPointInside( ggpos, FGI_FLUID, OId, distance);
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

	freeGeoTree();
	myTime_t geotimeend = getTime(); 
	debMsgStd("LbmFsgrSolver::initGeometryFlags",DM_MSG,"Geometry init done ("<< getTimeString(geotimeend-geotimestart)<<","<<savedNodes<<") " , 10 ); 
	//errMsg(" SAVED "," "<<savedNodes<<" of "<<(mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizey*mLevel[mMaxRefine].lSizez));
	return true;
}

#undef GETPOS

/*****************************************************************************/
/* init part for all freesurface testcases */
void LbmFsgrSolver::initFreeSurfaces() {
	double interfaceFill = 0.45;   // filling level of interface cells
	//interfaceFill = 1.0; // DEUG!! GEOMTEST!!!!!!!!!!!!

	// set interface cells 
	FSGR_FORIJK1(mMaxRefine) {
		if( ( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr)& CFFluid )) {
			FORDF1 {
				int ni=i+dfVecX[l], nj=j+dfVecY[l], nk=k+dfVecZ[l];
				if( ( RFLAG(mMaxRefine, ni, nj, nk,  mLevel[mMaxRefine].setCurr)& CFEmpty ) ) {
					LbmFloat arho=0., aux=0., auy=0., auz=0.;
					interpolateCellValues(mMaxRefine, ni,nj,nk, mLevel[mMaxRefine].setCurr, arho,aux,auy,auz);
					//errMsg("TINI"," "<<PRINT_VEC(ni,nj,nk)<<" v"<<LbmVec(aux,auy,auz) );
					// unnecessary? initEmptyCell(mMaxRefine, ni,nj,nk, CFInter, arho, interfaceFill);
					initVelocityCell(mMaxRefine, ni,nj,nk, CFInter, arho, interfaceFill, LbmVec(aux,auy,auz) );
				}
			}
		}
	}

	// remove invalid interface cells 
	FSGR_FORIJK1(mMaxRefine) {
		// remove invalid interface cells 
		if( ( RFLAG(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr)& CFInter) ) {
			int delit = 0;
			int NBs = 0; // neighbor flags or'ed 
			int noEmptyNB = 1;

			FORDF1 {
				if( ( RFLAG_NBINV(mMaxRefine, i, j, k, mLevel[mMaxRefine].setCurr,l )& CFEmpty ) ) {
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
				//initEmptyCell(mMaxRefine, i,j,k, CFFluid, 1.0, 1.0);
				LbmFloat arho=0., aux=0., auy=0., auz=0.;
				interpolateCellValues(mMaxRefine, i,j,k, mLevel[mMaxRefine].setCurr, arho,aux,auy,auz);
				initVelocityCell(mMaxRefine, i,j,k, CFFluid, arho,1., LbmVec(aux,auy,auz) );
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
			if( ( RFLAG(lev, i,j,k, mLevel[lev].setCurr)& CFInter) ) {
				LbmFloat mass = 0.0;
				//LbmFloat nbdiv;
				//FORDF0 {
				for(int l=0;(l<cDirNum); l++) { 
					int ni=i+dfVecX[l], nj=j+dfVecY[l], nk=k+dfVecZ[l];
					if( RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr) & CFFluid ){
						mass += 1.0;
					}
					if( RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr) & CFInter ){
						mass += QCELL(lev, ni,nj,nk, mLevel[lev].setCurr, dMass);
					}
					//nbdiv+=1.0;
				}

				//errMsg(" I ", PRINT_IJK<<" m"<<mass );
				QCELL(lev, i,j,k, mLevel[lev].setOther, dMass) = (mass/ ((LbmFloat)cDirNum) );
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
		
		int preinitSteps = (haveStandingFluid* ((mLevel[lev].lSizey+mLevel[lev].lSizez+mLevel[lev].lSizex)/3) );
		preinitSteps = (haveStandingFluid>>2); // not much use...?
		//preinitSteps = 0;
		debMsgStd("Standing fluid preinit", DM_MSG, "Performing "<<preinitSteps<<" prerelaxations ",10);
		for(int s=0; s<preinitSteps; s++) {
			// in solver main cpp
			standingFluidPreinit();
		}

		myTime_t timeend = getTime();
		debMsgStd("Standing fluid preinit", DM_MSG, " done, "<<getTimeString(timeend-timestart), 9);
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
		//if(LBMDIM==2) mPanic = true; 
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

	for(int nbl=1; nbl< cDfNum ; ++nbl) {
		if(RFLAG_NB(level,ei,ej,ek,workSet,nbl) & CFNoInterpolSrc) continue;
		if( (RFLAG_NB(level,ei,ej,ek,workSet,nbl) & (CFFluid|CFInter)) ){
				//((!(RFLAG_NB(level,ei,ej,ek,workSet,nbl) & CFNoInterpolSrc) ) &&
				 //(RFLAG_NB(level,ei,ej,ek,workSet,nbl) & CFInter) ) { 
			cellcnt += 1.0;
			for(int rl=0; rl< cDfNum ; ++rl) { 
				LbmFloat nbdf =  QCELL_NB(level,ei,ej,ek, workSet,nbl, rl);
				avgux  += (dfDvecX[rl]*nbdf); 
				avguy  += (dfDvecY[rl]*nbdf);  
				avguz  += (dfDvecZ[rl]*nbdf);  
				avgrho += nbdf;
			}
		}
	}

	if(cellcnt<=0.0) {
		// no nbs? just use eq.
		avgrho = 1.0;
		avgux = avguy = avguz = 0.0;
		//TTT mNumProblems++;
#if ELBEEM_PLUGIN!=1
		//mPanic=1; 
		// this can happen for worst case moving obj scenarios...
		errMsg("LbmFsgrSolver::interpolateCellValues","Cellcnt<=0.0 at "<<PRINT_VEC(ei,ej,ek));
#endif // ELBEEM_PLUGIN
	} else {
		// init speed
		avgux /= cellcnt; avguy /= cellcnt; avguz /= cellcnt;
		avgrho /= cellcnt;
	}

	retrho = avgrho;
	retux = avgux;
	retuy = avguy;
	retuz = avguz;
} // interpolateCellValues


/******************************************************************************
 * instantiation
 *****************************************************************************/

//! lbm factory functions
LbmSolverInterface* createSolver() { return new LbmFsgrSolver(); }


