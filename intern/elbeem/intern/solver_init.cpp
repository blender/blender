/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004,2005 Nils Thuerey
 *
 * Standard LBM Factory implementation
 *
 *****************************************************************************/

#ifndef __APPLE_CC__
#include "solver_class.h"
#include "solver_relax.h"
#endif // __APPLE_CC__

#if !defined(__APPLE_CC__) || defined(LBM_FORCEINCLUDE)

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
	mSmoothSurface(0.0), mSmoothNormals(0.0),
	mTimeAdap(false), mDomainBound("noslip"), mDomainPartSlipValue(0.1),
	mOutputSurfacePreview(0), mPreviewFactor(0.25),
	mFVHeight(0.0), mFVArea(1.0), mUpdateFVHeight(false),
	mInitSurfaceSmoothing(0),
	mTimestepReduceLock(0),
	mTimeSwitchCounts(0), 
	mSimulationTime(0.0),
	mMinStepTime(0.0), mMaxStepTime(0.0),
	mMaxNoCells(0), mMinNoCells(0), mAvgNumUsedCells(0),
	mDropMode(1), mDropSize(0.15), mDropSpeed(0.0),
	mObjectSpeeds(), mObjectPartslips(),
	mIsoWeightMethod(2),
	mMaxRefine(1), 
	mDfScaleUp(-1.0), mDfScaleDown(-1.0),
	mInitialCsmago(0.04), mDebugOmegaRet(0.0),
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

	//addDrop(false,0,0);
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

#if ELBEEM_BLENDER!=1
	destroyTestdata();
#endif // ELBEEM_BLENDER!=1

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
	mDomainBound = D::mpAttrs->readString("domainbound", mDomainBound, "SimulationLbm","mDomainBound", false );
	mDomainPartSlipValue = D::mpAttrs->readFloat("domainpartslip", mDomainPartSlipValue, "SimulationLbm","mDomainPartSlipValue", false );

	mIsoWeightMethod= D::mpAttrs->readInt("isoweightmethod", mIsoWeightMethod, "SimulationLbm","mIsoWeightMethod", false );
	mInitSurfaceSmoothing = D::mpAttrs->readInt("initsurfsmooth", mInitSurfaceSmoothing, "SimulationLbm","mInitSurfaceSmoothing", false );
	mSmoothSurface = D::mpAttrs->readFloat("smoothsurface", mSmoothSurface, "SimulationLbm","mSmoothSurface", false );
	mSmoothNormals = D::mpAttrs->readFloat("smoothnormals", mSmoothNormals, "SimulationLbm","mSmoothNormals", false );

	mInitialCsmago = D::mpAttrs->readFloat("csmago", mInitialCsmago, "SimulationLbm","mInitialCsmago", false );
	// deprecated!
	float mInitialCsmagoCoarse = 0.0;
	mInitialCsmagoCoarse = D::mpAttrs->readFloat("csmago_coarse", mInitialCsmagoCoarse, "SimulationLbm","mInitialCsmagoCoarse", false );
#if USE_LES==1
#else // USE_LES==1
	debMsgStd("LbmFsgrSolver", DM_WARNING, "LES model switched off!",2);
	mInitialCsmago = 0.0;
#endif // USE_LES==1

	// refinement
	mMaxRefine  = D::mpAttrs->readInt("maxrefine",  mMaxRefine ,"LbmFsgrSolver", "mMaxRefine", true);
	if(mMaxRefine<0) mMaxRefine=0;
	if(mMaxRefine>FSGR_MAXNOOFLEVELS) mMaxRefine=FSGR_MAXNOOFLEVELS-1;
	mDisableStandingFluidInit = D::mpAttrs->readInt("disable_stfluidinit", mDisableStandingFluidInit,"LbmFsgrSolver", "mDisableStandingFluidInit", false);
	mForceTadapRefine = D::mpAttrs->readInt("forcetadaprefine", mForceTadapRefine,"LbmFsgrSolver", "mForceTadapRefine", false);

	// demo mode settings
	mFVHeight = D::mpAttrs->readFloat("fvolheight", mFVHeight, "SimulationLbm","mFVHeight", false );
	// FIXME check needed?
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
		mLevel[i].lcsmago = mInitialCsmago;
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
	//if(mMaxRefine >= FSGR_MAXNOOFLEVELS) { errFatal("LbmFsgrSolver::initializeLbmGridref"," error: Too many levels!", SIMWORLD_INITERROR); return false; }
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
  debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Boundary init...",10);


	// use the density init?
	initGeometryFlags();
	D::initGenericTestCases();
	
	// new - init noslip 1 everywhere...
	// half fill boundary cells?

	CellFlagType domainBoundType = CFInvalid;
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
	int domainobj = (int)(D::mpGiObjects->size());
	domainBoundType |= (domainobj<<24);
	//for(int i=0; i<(int)(domainobj+0); i++) {
		//errMsg("GEOIN","i"<<i<<" "<<(*D::mpGiObjects)[i]->getName());
		//if((*D::mpGiObjects)[i] == D::mpIso) {
			//check...
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

	if(D::cDimension == 3) {
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

	/*for(int ii=0; ii<(int)pow(2.0,mMaxRefine)-1; ii++) {
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


	
	if(D::cDimension==2) {
		if(mOutputSurfacePreview) {
			errMsg("LbmFsgrSolver::init","No preview in 2D allowed!");
			mOutputSurfacePreview = 0; }
	}
	if(mOutputSurfacePreview) {

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
	bool thinHit = false;
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
				//if(D::geoInitCheckPointInside( GETPOS(i,j,k) , ntlVec3Gfx(1.0,0.0,0.0), FGI_ALLBOUNDS, OId, distance, dvec[0]*0.5, thinHit, true)) {
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
					case FGI_BNDNO: 
						rhomass = BND_FILL;
						ntype = CFBnd|CFBndNoslip; break;
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
				// *

			} 
		} 
	} // zmax
	// */

	/*
	for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
		for(int i=1;i<mLevel[level].lSizex-1;i++) {
			for(int j=1;j<mLevel[level].lSizey-1;j++) {
			//errMsg("INIT0","at "<<PRINT_IJK<<" p="<<GETPOS(i,j,k)<<" j"<<j<<" "<<mLevel[level].lSizey);
				//if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;
				CellFlagType ntype = CFInvalid;
			//errMsg("INIT1","at "<<PRINT_IJK<<" p="<<GETPOS(i,j,k));
				//if(D::geoInitCheckPointInside( GETPOS(i,j,k) , ntlVec3Gfx(0.0,1.0,0.0), FGI_ALLBOUNDS, OId, distance, dvec[1]*0.5)) {
				if(D::geoInitCheckPointInside( GETPOS(i,j,k) , ntlVec3Gfx(0.0,1.0,0.0), FGI_ALLBOUNDS, OId, distance, dvec[1]*0.5, thinHit, true)) {
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
					if((ntype & CFMbndInflow) || (ntype & CFMbndOutflow) ) {
						ntype |= (OId<<24);
					}
					initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
				}
				//errMsg("INITT","at "<<PRINT_IJK<<" t="<<ntype<<" d="<<distance);

				// walk along x until hit for following inits
				if(distance<=-1.0) { distance = 100.0; }
				if(distance>=0.0) {
					gfxReal dcnt=dvec[1];
					while(( dcnt< distance-dvec[1] )&&(j+1<mLevel[level].lSizey-1)) {
						dcnt += dvec[1]; j++;
						//if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;
						savedNodes++;
						if(ntype != CFInvalid) {
							// rho,mass,OId are still inited from above
							initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
						}
					}
				} 

			} 
		} 
	} // zmax, j
	// */
	
	
	/*
	for(int j=1;j<mLevel[level].lSizey-1;j++) {
		for(int i=1;i<mLevel[level].lSizex-1;i++) {
			for(int k= getForZMin1(); k< getForZMax1(level); ++k) {
				//if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;
				CellFlagType ntype = CFInvalid;
				if(D::geoInitCheckPointInside( GETPOS(i,j,k) , ntlVec3Gfx(0.0,0.0,1.0), FGI_ALLBOUNDS, OId, distance, dvec[2]*0.5, thinHit, true)) {
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
					if((ntype & CFMbndInflow) || (ntype & CFMbndOutflow) ) {
						ntype |= (OId<<24);
					}
					initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
				}
				errMsg("INITZ","at "<<PRINT_IJK<<" t="<<ntype<<" d="<<distance);

				// walk along x until hit for following inits
				if(distance<=-1.0) { distance = 100.0; }
				if(distance>=0.0) {
					gfxReal dcnt=dvec[2];
					while(( dcnt< distance-dvec[2] )&&(k+1<mLevel[level].lSizez-1)) {
						dcnt += dvec[2]; k++;
						//if(!(RFLAG(level, i,j,k, mLevel[level].setCurr)==CFEmpty)) continue;
						savedNodes++;
						if(ntype != CFInvalid) {
							// rho,mass,OId are still inited from above
							initVelocityCell(level, i,j,k, ntype, rhomass, rhomass, mObjectSpeeds[OId] );
						}
					}
				} // *

			} 
		} 
	} // zmax, k
	// */
	thinHit = false;


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

#if ELBEEM_BLENDER!=1
	initTestdata();
#endif // ELBEEM_BLENDER!=1
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
#if OPT3D==1 
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
		//D::mInitDone = 0;  // GRAVTEST
		// */

		myTime_t timeend = getTime();
		debMsgDirect(" done, "<<((timeend-timestart)/(double)1000.0)<<"s \n");
#undef  NBFLAG
	}
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

#endif // !defined(__APPLE_CC__) || defined(LBM_FORCEINCLUDE)


/******************************************************************************
 * instantiation
 *****************************************************************************/

#ifndef __APPLE_CC__

#if LBMDIM==2
#define LBM_INSTANTIATE LbmBGK2D
#endif // LBMDIM==2
#if LBMDIM==3
#define LBM_INSTANTIATE LbmBGK3D
#endif // LBMDIM==3

template class LbmFsgrSolver< LBM_INSTANTIATE >;

#endif // __APPLE_CC__

// the intel compiler is too smart - so the virtual functions called from other cpp
// files have to be instantiated explcitly (otherwise this will cause undefined
// references to "non virtual thunks") ... still not working, though
//template<class LBM_INSTANTIATE> LbmFsgrSolver<LBM_INSTANTIATE>::~LbmFsgrSolver();
//template<class LBM_INSTANTIATE> void LbmFsgrSolver<LBM_INSTANTIATE>::parseAttrList();
//template<class LBM_INSTANTIATE> bool LbmFsgrSolver<LBM_INSTANTIATE>::initialize( ntlTree* /*tree*/, vector<ntlGeometryObject*>* /*objects*/ );


