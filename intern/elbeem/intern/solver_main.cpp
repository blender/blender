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
#include "particletracer.h"

/*****************************************************************************/
/*! perform a single LBM step */
/*****************************************************************************/


void LbmFsgrSolver::step() { 
	initLevelOmegas();
	stepMain(); 
}

void LbmFsgrSolver::stepMain()
{
	this->markedClearList(); // DMC clearMarkedCellsList

	// safety check, counter reset
	this->mNumUsedCells = 0;
	mNumInterdCells = 0;
	mNumInvIfCells = 0;

  //debugOutNnl("LbmFsgrSolver::step : "<<this->mStepCnt, 10);
  if(!this->mSilent){ debMsgStd("LbmFsgrSolver::step", DM_MSG, this->mName<<" cnt:"<<this->mStepCnt<<" t:"<<mSimulationTime, 10); }
	//debMsgDirect(  "LbmFsgrSolver::step : "<<this->mStepCnt<<" ");
	myTime_t timestart = getTime();
	//myTime_t timestart = 0;
	//if(mStartSymm) { checkSymmetry("step1"); } // DEBUG 

	// time adapt
	mMaxVlen = mMxvz = mMxvy = mMxvx = 0.0;

	// init moving bc's, can change mMaxVlen
	initMovingObstacles(false);
#if LBM_INCLUDE_TESTSOLVERS==1
	handleCpdata();
#endif

	// important - keep for tadap
	LbmFloat lastMass = mCurrentMass;
	mCurrentMass = this->mFixMass; // reset here for next step
	mCurrentVolume = 0.0;
	
	//change to single step advance!
	int levsteps = 0;
	int dsbits = this->mStepCnt ^ (this->mStepCnt-1);
	//errMsg("S"," step:"<<this->mStepCnt<<" s-1:"<<(this->mStepCnt-1)<<" xf:"<<convertCellFlagType2String(dsbits));
	for(int lev=0; lev<=mMaxRefine; lev++) {
		//if(! (this->mStepCnt&(1<<lev)) ) {
		if( dsbits & (1<<(mMaxRefine-lev)) ) {
			//errMsg("S"," l"<<lev);

			if(lev==mMaxRefine) {
				// always advance fine level...
				fineAdvance();
			} else {
				adaptGrid(lev);
				coarseRestrictFromFine(lev);
				coarseAdvance(lev);
			}
#if FSGR_OMEGA_DEBUG==1
			errMsg("LbmFsgrSolver::step","LES stats l="<<lev<<" omega="<<mLevel[lev].omega<<" avgOmega="<< (mLevel[lev].avgOmega/mLevel[lev].avgOmegaCnt) );
			mLevel[lev].avgOmega = 0.0; mLevel[lev].avgOmegaCnt = 0.0;
#endif // FSGR_OMEGA_DEBUG==1
			levsteps++;
		}
		mCurrentMass   += mLevel[lev].lmass;
		mCurrentVolume += mLevel[lev].lvolume;
	}

  // prepare next step
	this->mStepCnt++;


	// some dbugging output follows
	// calculate MLSUPS
	myTime_t timeend = getTime();

	this->mNumUsedCells += mNumInterdCells; // count both types for MLSUPS
	mAvgNumUsedCells += this->mNumUsedCells;
	this->mMLSUPS = (this->mNumUsedCells / ((timeend-timestart)/(double)1000.0) ) / (1000000);
	if(this->mMLSUPS>10000){ this->mMLSUPS = -1; }
	else { mAvgMLSUPS += this->mMLSUPS; mAvgMLSUPSCnt += 1.0; } // track average mlsups
	
	LbmFloat totMLSUPS = ( ((mLevel[mMaxRefine].lSizex-2)*(mLevel[mMaxRefine].lSizey-2)*(getForZMax1(mMaxRefine)-getForZMin1())) / ((timeend-timestart)/(double)1000.0) ) / (1000000);
	if(totMLSUPS>10000) totMLSUPS = -1;
	mNumInvIfTotal += mNumInvIfCells; // debug

  // do some formatting 
  if(!this->mSilent){ 
		string sepStr(""); // DEBUG
		int avgcls = (int)(mAvgNumUsedCells/(LONGINT)this->mStepCnt);
  	debMsgStd("LbmFsgrSolver::step", DM_MSG, this->mName<<" cnt:"<<this->mStepCnt<<" t:"<<mSimulationTime<<
		//debMsgDirect( 
			" mlsups(curr:"<<this->mMLSUPS<<
			" avg:"<<(mAvgMLSUPS/mAvgMLSUPSCnt)<<"), "<< sepStr<<
			" totcls:"<<(this->mNumUsedCells)<< sepStr<<
			" avgcls:"<< avgcls<< sepStr<<
			" intd:"<<mNumInterdCells<< sepStr<<
			" invif:"<<mNumInvIfCells<< sepStr<<
			" invift:"<<mNumInvIfTotal<< sepStr<<
			" fsgrcs:"<<mNumFsgrChanges<< sepStr<<
			" filled:"<<this->mNumFilledCells<<", emptied:"<<this->mNumEmptiedCells<< sepStr<<
			" mMxv:"<<mMxvx<<","<<mMxvy<<","<<mMxvz<<", tscnts:"<<mTimeSwitchCounts<< sepStr<<
			" RWmxv:"<<ntlVec3Gfx(mMxvx,mMxvy,mMxvz)*(mLevel[mMaxRefine].simCellSize / mLevel[mMaxRefine].timestep)<<" "<< /* realworld vel output */
			" probs:"<<mNumProblems<< sepStr<<
			" simt:"<<mSimulationTime<< sepStr<<
			" for '"<<this->mName<<"' " , 10);
	} else { debMsgDirect("."); }

	if(this->mStepCnt==1) {
		mMinNoCells = mMaxNoCells = this->mNumUsedCells;
	} else {
		if(this->mNumUsedCells>mMaxNoCells) mMaxNoCells = this->mNumUsedCells;
		if(this->mNumUsedCells<mMinNoCells) mMinNoCells = this->mNumUsedCells;
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
					<<" step:"<<this->mStepCnt<<" levstep:"<<mLevel[0].lsteps<<" msc:"<<mscount<<" "
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

#if LBM_INCLUDE_TESTSOLVERS==1
	if((mUseTestdata)&&(this->mInitDone)) { handleTestdata(); }
#endif

	// advance positions with current grid
	advanceParticles();
	if(mpParticles) {
		mpParticles->checkDumpTextPositions(mSimulationTime);
		mpParticles->checkTrails(mSimulationTime);
	}

	// one of the last things to do - adapt timestep
	// was in fineAdvance before... 
	if(mTimeAdap) {
		adaptTimestep();
	} // time adaptivity


#ifndef WIN32
	// good indicator for instabilities
	if( (!finite(mMxvx)) || (!finite(mMxvy)) || (!finite(mMxvz)) ) { CAUSE_PANIC; }
	if( (!finite(mCurrentMass)) || (!finite(mCurrentVolume)) ) { CAUSE_PANIC; }
#endif // WIN32

	// output total step time
	timeend = getTime();
	debMsgStd("LbmFsgrSolver::stepMain",DM_MSG,"step:"<<this->mStepCnt
			<<": dccd="<< mCurrentMass<<",d"<<(lastMass-mCurrentMass)<<"/"<<mCurrentVolume<<"(fix="<<this->mFixMass<<",ini="<<mInitialMass<<"), "
			<<" totst:"<<getTimeString(timeend-timestart), 3);
	// nicer output
	debMsgDirect(std::endl); 

 //#endif // ELBEEM_PLUGIN!=1
}

#define NEWDEBCHECK(str) \
	if(!this->mPanic){ FSGR_FORIJK_BOUNDS(mMaxRefine) { \
		if(RFLAG(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr)&(CFFluid|CFInter)) { \
		for(int l=0;l<dTotalNum;l++) { \
			if(!finite(QCELL(mMaxRefine,i,j,k,mLevel[mMaxRefine].setCurr,l))) { errMsg("NNOFIN"," "<<str<<" at "<<PRINT_IJK<<" l"<<l<<" "); }\
		}/*for*/ \
		}/*if*/ \
	} }

void LbmFsgrSolver::fineAdvance()
{
	// do the real thing...
	//NEWDEBCHECK("t1");
	mainLoop( mMaxRefine );
	if(mUpdateFVHeight) {
		// warning assume -Y gravity...
		mFVHeight = mCurrentMass*mFVArea/((LbmFloat)(mLevel[mMaxRefine].lSizex*mLevel[mMaxRefine].lSizez));
		if(mFVHeight<1.0) mFVHeight = 1.0;
		this->mpParam->setFluidVolumeHeight(mFVHeight);
	}

	// advance time before timestep change
	mSimulationTime += this->mpParam->getTimestep();
	// time adaptivity
	this->mpParam->setSimulationMaxSpeed( sqrt(mMaxVlen / 1.5) );
	//if(mStartSymm) { checkSymmetry("step2"); } // DEBUG 
	if(!this->mSilent){ errMsg("fineAdvance"," stepped from "<<mLevel[mMaxRefine].setCurr<<" to "<<mLevel[mMaxRefine].setOther<<" step"<< (mLevel[mMaxRefine].lsteps) ); }

	// update other set
  mLevel[mMaxRefine].setOther   = mLevel[mMaxRefine].setCurr;
  mLevel[mMaxRefine].setCurr   ^= 1;
  mLevel[mMaxRefine].lsteps++;

	// flag init... (work on current set, to simplify flag checks)
	reinitFlags( mLevel[mMaxRefine].setCurr );
	if(!this->mSilent){ errMsg("fineAdvance"," flags reinit on set "<< mLevel[mMaxRefine].setCurr ); }
}


/*****************************************************************************/
//! fine step function
/*****************************************************************************/


// access to own dfs during step (may be changed to local array)
#define MYDF(l) RAC(ccel, l)

void 
LbmFsgrSolver::mainLoop(int lev)
{
	// loops over _only inner_ cells  -----------------------------------------------------------------------------------
	LbmFloat calcCurrentMass = 0.0;
	LbmFloat calcCurrentVolume = 0.0;
	int      calcCellsFilled = this->mNumFilledCells;
	int      calcCellsEmptied = this->mNumEmptiedCells;
	int      calcNumUsedCells = this->mNumUsedCells;
	const int cutMin  = 1;
	const int cutConst = mCutoff+2;

#	if LBM_INCLUDE_TESTSOLVERS==1
	// 3d region off... quit
	if((mUseTestdata)&&(mpTest->mDebugvalue1>0.0)) { return; }
#endif // ELBEEM_PLUGIN!=1
	//printLbmCell(lev, 6,6,16, mLevel[lev].setCurr ); // DEBUG
	
#if PARALLEL==1
#include "paraloop.h"
#else // PARALLEL==1
  { // main loop region
	int kstart=getForZMin1(), kend=getForZMax1(mMaxRefine);
	//{ errMsg("LbmFsgrSolver::mainLoop","Err MAINADVANCE0 ks:"<< kstart<<" ke:"<<kend<<" dim:"<<LBMDIMcDimension<<" mlsz:"<< mLevel[mMaxRefine].lSizez<<" zmax1:"<<getForZMax1(mMaxRefine) ); } // DEBUG
#define PERFORM_USQRMAXCHECK USQRMAXCHECK(usqr,ux,uy,uz, mMaxVlen, mMxvx,mMxvy,mMxvz);
#define LIST_EMPTY(x) mListEmpty.push_back( x );
#define LIST_FULL(x)  mListFull.push_back( x );
#endif // PARALLEL==1

	// local to loop
	CellFlagType nbflag[LBM_DFNUM]; 
	LbmFloat *ccel = NULL;
	LbmFloat *tcel = NULL;
	int oldFlag;
	int newFlag;
	int nbored;
	LbmFloat m[LBM_DFNUM];
	LbmFloat rho, ux, uy, uz, tmp, usqr;
	LbmFloat mass, change, lcsmqo=0.0;
	usqr = tmp = 0.0; 
#if OPT3D==1 
	LbmFloat lcsmqadd, lcsmeq[LBM_DFNUM], lcsmomega;
#endif // OPT3D==true 

	
	// ifempty cell conversion flags
	bool iffilled, ifemptied;
	LbmFloat nbfracs[LBM_DFNUM]; // ffracs of neighbors
	int recons[LBM_DFNUM];   // reconstruct this DF?
	int numRecons;           // how many are reconstructed?

	// slow surf regions smooth (if below)
	const LbmFloat smoothStrength = 0.0; //0.01;
	const LbmFloat sssUsqrLimit = 1.5 * 0.03*0.03;
	const LbmFloat sssUsqrLimitInv = 1.0/sssUsqrLimit;
	
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

	// ---
	// now stream etc.

	//{ errMsg("LbmFsgrSolver::mainLoop","Err MAINADVANCE0 ks:"<< kstart<<" ke:"<<kend<<" dim:"<<LBMDIM<<" mlsz:"<<mLevel[mMaxRefine].lSizez ); } // DEBUG

	// use //template functions for 2D/3D
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
  //const int id = 0, Nthrds = 1;
	const int jstart = 0;
	const int jend   = mLevel[mMaxRefine].lSizey;
//#endif // PARALLEL==1
#else // PARALLEL==1
	PARA_INITIALIZE();
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
			CAUSE_PANIC;
		}	
		if( (&QCELL(lev, i,j,k,mLevel[lev].setCurr,0) != ccel) || 
		    (&QCELL(lev, i,j,k,mLevel[lev].setOther,0) != tcel) ) {
			errMsg("LbmFsgrSolver::mainLoop","Err cellp "<<PRINT_IJK<<"="<<
          (int)(&QCELL(lev, i,j,k,mLevel[lev].setCurr,0))<<","<<(int)(&QCELL(lev, i,j,k,mLevel[lev].setOther,0))<<" but is "<<
          (int)(ccel)<<","<<(int)(tcel)<<" "
					); 
			CAUSE_PANIC;
		}	
#endif
		oldFlag = *pFlagSrc;
		//DEBUG if(LBMDIM==2) { errMsg("LbmFsgrSolver::mainLoop","Err flagp "<<PRINT_IJK<<"="<< RFLAG(lev, i,j,k,mLevel[lev].setCurr)<<" "); }
		// stream from current set to other, then collide and store
		
		// old INTCFCOARSETEST==1
		if( (oldFlag & (CFGrFromCoarse)) ) {  // interpolateFineFromCoarse test!
			if(( this->mStepCnt & (1<<(mMaxRefine-lev)) ) ==1) {
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
				FORDF0 { RAC(tcel, l) = this->getCollideEq(l, iniRho,vel[0],vel[1],vel[2]); }
				RAC(tcel, dMass) = RAC(tcel, dFfrac) = iniRho;
				RAC(tcel, dFlux) = FLUX_INIT;
				changeFlag(lev, i,j,k, TSET(lev), CFInter);
				calcCurrentMass += iniRho; calcCurrentVolume += 1.0; calcNumUsedCells++;
				mInitialMass += iniRho;
				//errMsg("INFLOW_DEBUG","ini at "<<PRINT_IJK<<" v="<<vel<<" inirho="<<iniRho);
				// dont treat cell until next step
				continue;
			} 
		} 
		else  // these are exclusive
		if(oldFlag & (CFMbndOutflow)) {
			int isnotValid = oldFlag & (CFFluid);
			if(isnotValid) {
				// remove fluid cells, shouldnt be here anyway
				LbmFloat fluidRho = m[0]; FORDF1 { fluidRho += m[l]; }
				mInitialMass -= fluidRho;
				const LbmFloat iniRho = 0.0;
				RAC(tcel, dMass) = RAC(tcel, dFfrac) = iniRho;
				RAC(tcel, dFlux) = FLUX_INIT;
				changeFlag(lev, i,j,k, TSET(lev), CFInter);

				// same as ifemptied for if below
				LbmPoint oemptyp; oemptyp.flag = 0;
				oemptyp.x = i; oemptyp.y = j; oemptyp.z = k;
#if PARALLEL==1
				//calcListEmpty[id].push_back( oemptyp );
#else // PARALLEL==1
				//mListEmpty.push_back( oemptyp );
#endif // PARALLEL==1
				LIST_EMPTY(oemptyp);
				calcCellsEmptied++;
				continue;
			}
		}

		if(oldFlag & (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)) { 
			*pFlagDst = oldFlag;
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
		
#if OPT3D==0
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
				// force velocity for inflow, necessary to have constant direction of flow
				// FIXME , test also set interface cells?
				const int OId = oldFlag>>24;
				//? DEFAULT_STREAM;
				//const LbmFloat fluidRho = 1.0;
				// for submerged inflows, streaming would have to be performed...
				LbmFloat fluidRho = m[0]; FORDF1 { fluidRho += m[l]; }
				const LbmVec vel(mObjectSpeeds[OId]);
				ux=vel[0], uy=vel[1], uz=vel[2]; 
				usqr = 1.5 * (ux*ux + uy*uy + uz*uz);
				FORDF0 { RAC(tcel, l) = this->getCollideEq(l, fluidRho,ux,uy,uz); }
				rho = fluidRho;
				//errMsg("INFLOW_DEBUG","std at "<<PRINT_IJK<<" v="<<vel<<" rho="<<rho);
			} else {
				if(nbored&CFBnd) {
					DEFAULT_STREAM;
					//ux = [0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; 
					DEFAULT_COLLIDEG(mLevel[lev].gravity);
					oldFlag &= (~CFNoBndFluid);
				} else {
					// do standard stream/collide
					OPTIMIZED_STREAMCOLLIDE;
					oldFlag |= CFNoBndFluid;
				} 
			}

			PERFORM_USQRMAXCHECK;
			// "normal" fluid cells
			RAC(tcel,dFfrac) = 1.0; 
			*pFlagDst = (CellFlagType)oldFlag; // newFlag;
			calcCurrentMass += rho; 
			calcCurrentVolume += 1.0;
			continue;
		}
		
		newFlag  = oldFlag;
		// make sure here: always check which flags to really unset...!
		newFlag = newFlag & (~( 
					CFNoNbFluid|CFNoNbEmpty| CFNoDelete 
					| CFNoInterpolSrc
					| CFNoBndFluid
					));
		if(!(nbored&CFBndNoslip)) { //NEWSURFT NEWSURFTNOS
			newFlag |= CFNoBndFluid;
		}
		/*if(!(nbored&CFBnd)) { //NEWSURFT NEWSURFTNOS
			// explicitly check for noslip neighbors
			bool hasnoslipnb = false;
			FORDF1 { if((nbflag[l]&CFBnd)&&(nbflag[l]&CFBndNoslip)) hasnoslipnb=true; }
			if(!hasnoslipnb) newFlag |= CFNoBndFluid; 
		} // */

		// store own dfs and mass
		mass = RAC(ccel,dMass);

		// WARNING - only interface cells arrive here!
		// read distribution funtions of adjacent cells = stream step
		DEFAULT_STREAM;

		if((nbored & CFFluid)==0) { newFlag |= CFNoNbFluid; mNumInvIfCells++; }
		if((nbored & CFEmpty)==0) { newFlag |= CFNoNbEmpty; mNumInvIfCells++; }

		// calculate mass exchange for interface cells 
		LbmFloat myfrac = RAC(ccel,dFfrac);
		if(myfrac<0.) myfrac=0.; // NEWSURFT
#		define nbdf(l) m[ this->dfInv[(l)] ]

		// update mass 
		// only do boundaries for fluid cells, and interface cells without
		// any fluid neighbors (assume that interface cells _with_ fluid
		// neighbors are affected enough by these) 
		// which Df's have to be reconstructed? 
		// for fluid cells - just the f_i difference from streaming to empty cells  ----
		numRecons = 0;
		bool onlyBndnb = ((!(oldFlag&CFNoBndFluid))&&(oldFlag&CFNoNbFluid)&&(nbored&CFBndNoslip));
	//onlyBndnb = false; // DEBUG test off

		FORDF1 { // dfl loop
			recons[l] = 0;
			nbfracs[l] = 0.0;
			// finally, "normal" interface cells ----
			if( nbflag[l]&(CFFluid|CFBnd) ) { // NEWTEST! FIXME check!!!!!!!!!!!!!!!!!!
				change = nbdf(l) - MYDF(l);
			}
			// interface cells - distuingish cells that shouldn't fill/empty 
			else if( nbflag[l] & CFInter ) {
				
				LbmFloat mynbfac,nbnbfac;
				// NEW TEST t1
				// t2 -> off
				if((oldFlag&CFNoBndFluid)&&(nbflag[l]&CFNoBndFluid)) {
					mynbfac = QCELL_NB(lev, i,j,k,SRCS(lev),l, dFlux) / QCELL(lev, i,j,k,SRCS(lev), dFlux);
					nbnbfac = 1.0/mynbfac;
					onlyBndnb = false;
				} else {
					mynbfac = nbnbfac = 1.0; // switch calc flux off
					goto changeDefault;  // NEWSURFT
					//change = 0.; goto changeDone;  // NEWSURFT
				}
				//mynbfac = nbnbfac = 1.0; // switch calc flux off t3

				// perform interface case handling
				if ((oldFlag|nbflag[l])&(CFNoNbFluid|CFNoNbEmpty)) {
				switch (oldFlag&(CFNoNbFluid|CFNoNbEmpty)) {
					case 0: 
						// we are a normal cell so... 
						switch (nbflag[l]&(CFNoNbFluid|CFNoNbEmpty)) {
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
						switch (nbflag[l]&(CFNoNbFluid|CFNoNbEmpty)) {
							case 0: 
							case CFNoNbEmpty: 
								// we have no fluid nb's -> just empty
								change = - mynbfac*MYDF(l) ; goto changeDone; 
						}
						break;

					case CFNoNbEmpty: 
						// we dont have empty nb's so...
						switch (nbflag[l]&(CFNoNbFluid|CFNoNbEmpty)) {
							case 0: 
							case CFNoNbFluid: 
								// we have no empty nb's -> just fill
								change = nbnbfac*nbdf(l); goto changeDone; 
						}
						break;
				}} // inter-inter exchange

			changeDefault: ;
				// just do normal mass exchange...
				change = ( nbnbfac*nbdf(l) - mynbfac*MYDF(l) ) ;
			changeDone: ;
				nbfracs[l] = QCELL_NB(lev, i,j,k, SRCS(lev),l, dFfrac);
				if(nbfracs[l]<0.) nbfracs[l] = 0.; // NEWSURFT
				change *=  (myfrac + nbfracs[l]) * 0.5;
			} // the other cell is interface

			// last alternative - reconstruction in this direction
			else {
				// empty + bnd case
				recons[l] = 1; 
				numRecons++;
				change = 0.0; 
			}

			// modify mass at SRCS
			mass += change;
		} // l
		// normal interface, no if empty/fluid

		LbmFloat nv1,nv2;
		LbmFloat nx,ny,nz;

		if(nbflag[dE] &(CFFluid|CFInter)){ nv1 = RAC((ccel+QCELLSTEP ),dFfrac); } else nv1 = 0.0;
		if(nbflag[dW] &(CFFluid|CFInter)){ nv2 = RAC((ccel-QCELLSTEP ),dFfrac); } else nv2 = 0.0;
		nx = 0.5* (nv2-nv1);
		if(nbflag[dN] &(CFFluid|CFInter)){ nv1 = RAC((ccel+(mLevel[lev].lOffsx*QCELLSTEP)),dFfrac); } else nv1 = 0.0;
		if(nbflag[dS] &(CFFluid|CFInter)){ nv2 = RAC((ccel-(mLevel[lev].lOffsx*QCELLSTEP)),dFfrac); } else nv2 = 0.0;
		ny = 0.5* (nv2-nv1);
#if LBMDIM==3
		if(nbflag[dT] &(CFFluid|CFInter)){ nv1 = RAC((ccel+(mLevel[lev].lOffsy*QCELLSTEP)),dFfrac); } else nv1 = 0.0;
		if(nbflag[dB] &(CFFluid|CFInter)){ nv2 = RAC((ccel-(mLevel[lev].lOffsy*QCELLSTEP)),dFfrac); } else nv2 = 0.0;
		nz = 0.5* (nv2-nv1);
#else // LBMDIM==3
		nz = 0.0;
#endif // LBMDIM==3

		if( (ABS(nx)+ABS(ny)+ABS(nz)) > LBM_EPSILON) {
			// normal ok and usable...
			FORDF1 {
				if( (this->dfDvecX[l]*nx + this->dfDvecY[l]*ny + this->dfDvecZ[l]*nz)  // dot Dvec,norml
						> LBM_EPSILON) {
					recons[l] = 2; 
					numRecons++;
				} 
			}
		}

		// calculate macroscopic cell values
		LbmFloat oldUx, oldUy, oldUz;
		LbmFloat oldRho; // OLD rho = ccel->rho;
#if OPT3D==0
			oldRho=RAC(ccel,0);
			oldUx = oldUy = oldUz = 0.0;
			for(int l=1; l<this->cDfNum; l++) {
				oldRho += RAC(ccel,l);
				oldUx  += (this->dfDvecX[l]*RAC(ccel,l));
				oldUy  += (this->dfDvecY[l]*RAC(ccel,l)); 
				oldUz  += (this->dfDvecZ[l]*RAC(ccel,l)); 
			} 
#else // OPT3D==0
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
#if OPT3D==0
		// NOW - construct dist funcs from empty cells
		FORDF1 {
			if(recons[ l ]) {
				m[ this->dfInv[l] ] = 
					this->getCollideEq(l, REFERENCE_PRESSURE, oldUx,oldUy,oldUz) + 
					this->getCollideEq(this->dfInv[l], REFERENCE_PRESSURE, oldUx,oldUy,oldUz) 
					- MYDF( l );
			}
		}
		usqr = 1.5 * (oldUx*oldUx + oldUy*oldUy + oldUz*oldUz); // needed later on
#else
		ux=oldUx, uy=oldUy, uz=oldUz;  // no local vars, only for usqr
		rho = REFERENCE_PRESSURE;
		usqr = 1.5 * (ux*ux + uy*uy + uz*uz); // needed later on
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


		// inflow bc handling
		if(oldFlag & (CFMbndInflow)) {
			// fill if cells in inflow region
			if(myfrac<0.5) { 
				mass += 0.25; 
				mInitialMass += 0.25;
			}
			const int OId = oldFlag>>24;
			const LbmVec vel(mObjectSpeeds[OId]);
			ux=vel[0], uy=vel[1], uz=vel[2]; 
			//? usqr = 1.5 * (ux*ux + uy*uy + uz*uz);
			//FORDF0 { RAC(tcel, l) = this->getCollideEq(l, fluidRho,ux,uy,uz); } rho = fluidRho;
			rho = REFERENCE_PRESSURE;
			FORDF0 { RAC(tcel, l) = this->getCollideEq(l, rho,ux,uy,uz); }
			//errMsg("INFLOW_DEBUG","if at "<<PRINT_IJK<<" v="<<vel<<" rho="<<rho);
		}  else { 
			// NEWSURFT, todo optimize!
			if(onlyBndnb) { //if(usqr<0.001*0.001) {
				rho = ux = uy = uz = 0.;
				FORDF0{ 
					rho += m[l]; 
					ux  += (this->dfDvecX[l]*m[l]); 
					uy  += (this->dfDvecY[l]*m[l]);  
					uz  += (this->dfDvecZ[l]*m[l]);  
				}
				FORDF0 { RAC(tcel, l) = this->getCollideEq(l, rho,ux,uy,uz); }
			} else {// NEWSURFT */
				// normal collide
				// mass streaming done... do normal collide
				LbmVec grav = mLevel[lev].gravity*mass;
				DEFAULT_COLLIDEG(grav);
				PERFORM_USQRMAXCHECK;
				// rho init from default collide necessary for fill/empty check below
			} // test
		}

		// testing..., particle generation
		// also check oldFlag for CFNoNbFluid, test
		// for inflow no pargen test // NOBUBBB!
		if((this->mInitDone) //&&(mUseTestdata) 
				//&& (!((oldFlag|newFlag)&CFNoNbEmpty)) 
				&& (!((oldFlag|newFlag)&CFNoDelete)) 
				&& (this->mPartGenProb>0.0)) {
			bool doAdd = true;
			bool bndOk=true;
			//if( (i<1+mCutoff)||(i>this->mSizex-1-mCutoff)||
					//(j<1+mCutoff)||(j>this->mSizey-1-mCutoff)||
					//(k<1+mCutoff)||(k>this->mSizez-1-mCutoff) ) { bndOk=false; }
			if( (i<cutMin)||(i>this->mSizex-cutMin)||
					(j<cutMin)||(j>this->mSizey-cutMin)||
					(k<cutMin)||(k>this->mSizez-cutMin) ) { bndOk=false; }
			if(!bndOk) doAdd=false;
			
			LbmFloat realWorldFac = (mLevel[lev].simCellSize / mLevel[lev].timestep);
			LbmFloat rux = (ux * realWorldFac);
			LbmFloat ruy = (uy * realWorldFac);
			LbmFloat ruz = (uz * realWorldFac);
			LbmFloat rl = norm(ntlVec3Gfx(rux,ruy,ruz));
			// WHMOD

			LbmFloat prob = (rand()/(RAND_MAX+1.0));
			LbmFloat basethresh = this->mPartGenProb*lcsmqo*rl;

			// reduce probability in outer region
			//if( (i<cutConst)||(i>this->mSizex-cutConst) ){ prob *= 0.5; }
			//if( (j<cutConst)||(j>this->mSizey-cutConst) ){ prob *= 0.5; }
			//if( (k<cutConst)||(k>this->mSizez-cutConst) ){ prob *= 0.5; }
			// NEW TEST 040627
			//if( (i<cutConst)||(i>this->mSizex-cutConst) ){ doAdd=false; }
			//if( (j<cutConst)||(j>this->mSizey-cutConst) ){ doAdd=false; }
			//if( (k<cutConst)||(k>this->mSizez-cutConst) ){ doAdd=false; }
			const int pibord = mLevel[mMaxRefine].lSizex/2-cutConst;
			const int pjbord = mLevel[mMaxRefine].lSizey/2-cutConst;
			LbmFloat pifac = 1.-(LbmFloat)(ABS(i-pibord)) / (LbmFloat)(pibord);
			LbmFloat pjfac = 1.-(LbmFloat)(ABS(j-pjbord)) / (LbmFloat)(pjbord);
			if(pifac<0.) pifac=0.;
			if(pjfac<0.) pjfac=0.;
			//const LbmFloat pkfac = 1.-(LbmFloat)(ABS(k-mLevel[mMaxRefine].lSizez/2))/(LbmFloat)(mLevel[mMaxRefine].lSizez/2);
			//errMsg("PROBTTT"," at "<<PRINT_IJK<<" prob"<<prob<<" pifac"<<pifac<<" pjfac"<<pjfac<<" "<<(basethresh*rl*pifac*pjfac));
			//prob *= pifac*pjfac; //*pkfac;

//#define RWVEL_THRESH 1.0
#define RWVEL_THRESH 1.5
#define RWVEL_WINDTHRESH (RWVEL_THRESH*0.5)
			//if( (prob< (basethresh*rl)) && (lcsmqo>0.0095) && (rl>RWVEL_THRESH) ) {
			if( (prob< (basethresh*rl*pifac*pjfac)) && (lcsmqo>0.0095) && (rl>RWVEL_THRESH) ) {
				// add
			} else {
				doAdd = false; // dont...
			}

//#define SLOWDOWNREGION (2*mCutoff)
#if LBMDIM==3
			// normal
#define SLOWDOWNREGION (this->mSizez/4)
#else // LBMDIM==2
			// off
#define SLOWDOWNREGION 10 
#endif // LBMDIM==2
#define P_LCSMQO 0.01

			// "wind" disturbance
			// use realworld relative velocity here instead?
			if( (doAdd && 
					((rl>RWVEL_WINDTHRESH) && (lcsmqo<P_LCSMQO)) )// normal checks
					||(k>this->mSizez-SLOWDOWNREGION)   ) {
				LbmFloat nuz = uz;
				//? LbmFloat jdf; // = 0.05 * (rand()/(RAND_MAX+1.0));
				//? if(rl>RWVEL_WINDTHRESH) jdf *= (rl-RWVEL_WINDTHRESH);
				if(k>this->mSizez-SLOWDOWNREGION) {
					// special case
					LbmFloat zfac = (LbmFloat)( k-(this->mSizez-SLOWDOWNREGION) );
					zfac /= (LbmFloat)(SLOWDOWNREGION);
					nuz += (1.0) * zfac; // check max speed? OFF?
					//errMsg("TOPT"," at "<<PRINT_IJK<<" zfac"<<zfac);
				} else {
					// normal probability
					//? LbmFloat fac = P_LCSMQO-lcsmqo;
					//? jdf *= fac;
				}
				FORDF1 {
					const LbmFloat jdf = 0.05 * (rand()/(RAND_MAX+1.0));
					// TODO  use wind velocity?
					if(jdf>0.025) {
					const LbmFloat add =  this->dfLength[l]*(-ux*this->dfDvecX[l]-uy*this->dfDvecY[l]-nuz*this->dfDvecZ[l])*jdf;
					RAC(tcel,l) += add; }
				}
				//errMsg("TOPDOWNCORR"," jdf:"<<jdf<<" rl"<<rl<<" vel "<<PRINT_VEC(ux,uy,nuz)<<" rwv"<<PRINT_VEC(rux,ruy,ruz) );
				//errMsg("TOPDOWNCORR"," jdf:"<<jdf<<" rl"<<rl<<" vel "<<norm(LbmVec(ux,uy,nuz))<<" rwv"<<norm(LbmVec(rux,ruy,ruz)) );
			} // wind disturbance

			if(usqr<0.0001) doAdd=false;   // TODO check!?
			// if outside, and 20% above sea level, delete, TODO really check level?
			//if((!bndOk)&&((LbmFloat)k>pTest->mFluidHeight*1.5)) { doAdd=true; } // FORCEDISSOLVE
			//if(this->mStepCnt>700) errMsg("DFJITT"," at "<<PRINT_IJK<<"rwl:"<<rl<<"  usqr:"<<usqr <<" qo:"<<lcsmqo<<" add="<<doAdd );

			if( (doAdd)  ) { // ADD DROP
				LbmFloat len = norm(LbmVec(ux,uy,uz));
				// WHMOD
				//for(int s=0; s<10; s++) { // multiple parts
				for(int s=0; s<1; s++) { // one part!
				//LbmFloat prob = this->mPartGenProb * 0.02* (rand()/(RAND_MAX+1.0));
				const LbmFloat posjitter = 1.0;
				const LbmFloat posjitteroffs = posjitter*-0.5;
				LbmFloat jpx = posjitteroffs+ posjitter* (rand()/(RAND_MAX+1.0));
				LbmFloat jpy = posjitteroffs+ posjitter* (rand()/(RAND_MAX+1.0));
				LbmFloat jpz = posjitteroffs+ posjitter* (rand()/(RAND_MAX+1.0));

				const LbmFloat jitterstr = 0.1;
				const LbmFloat jitteroffs = jitterstr*-0.5;
				LbmFloat jx = jitteroffs+ jitterstr* (rand()/(RAND_MAX+1.0));
				LbmFloat jy = jitteroffs+ jitterstr* (rand()/(RAND_MAX+1.0));
				LbmFloat jz = jitteroffs+ jitterstr* (rand()/(RAND_MAX+1.0));

				// average normal & velocity 
				// -> mostly along velocity dir, many into surface
				LbmVec pv = (LbmVec(nx+jx,ny+jy,nz+jz)*0.75 + getNormalized(LbmVec(ux,uy,uz)) )*0.35; 
				normalize(pv);

				LbmFloat srci = i+0.5+jpx; // TEST? + (pv[0]*1.41);
				LbmFloat srcj = j+0.5+jpy; // TEST? + (pv[1]*1.41);
				LbmFloat srck = k+0.5+jpz; // TEST? + (pv[2]*1.41);
				int type=0;
				//if((s%3)!=2) {} else { type=PART_FLOAT; }
				//type = PART_DROP;
				type = PART_INTER;
				// drop
				/*srci += (pv[0]*1.41);
				srcj += (pv[1]*1.41);
				srck += (pv[2]*1.41);
				if(!(RFLAG(lev, (int)(srci),(int)(srcj),(int)(srck),SRCS(lev)) &CFEmpty)) continue; // only add in good direction */

				pv *= len;
				LbmFloat size = 1.0+ 9.0* (rand()/(RAND_MAX+1.0));

				mpParticles->addParticle(srci, srcj, srck); //i+0.5+jpx,j+0.5+jpy,k+0.5+jpz);
				mpParticles->getLast()->setVel(pv[0],pv[1],pv[2]);
				//? mpParticles->getLast()->advanceVel(); // advance a bit outwards
				mpParticles->getLast()->setStatus(PART_IN);
				mpParticles->getLast()->setType(type);
				//if((s%3)==2) mpParticles->getLast()->setType(PART_FLOAT);
				mpParticles->getLast()->setSize(size);
				//errMsg("NEWPART"," at "<<PRINT_IJK<<"   u="<<PRINT_VEC(ux,uy,uz) <<" RWu="<<PRINT_VEC(rux,ruy,ruz)<<" add"<<doAdd<<" pvel="<<pv );
				//errMsg("NEWPART"," at "<<PRINT_IJK<<"   u="<<norm(LbmVec(ux,uy,uz)) <<" RWu="<<norm(LbmVec(rux,ruy,ruz))<<" add"<<doAdd<<" pvel="<<norm(pv) );
				//if(!bndOk){ mass -= val2fac*size*0.02; } // FORCEDISSOLVE
				//const LbmFloat val2fac = 1.0; //? TODO N test? /(this->mPartGenProb); // FIXME remove factor!
				//mass -= val2fac*size*0.0015; // NTEST!
				//mass -= val2fac*size*0.001; // NTEST!
				mass -= size*0.0020; // NTEST!
#if LBMDIM==2
				mpParticles->getLast()->setVel(pv[0],pv[1],0.0);
				mpParticles->getLast()->setPos(ntlVec3Gfx(srci,srcj,0.5));
#endif // LBMDIM==2
    		//errMsg("PIT","NEW pit"<<mpParticles->getLast()->getId()<<" pos:"<< mpParticles->getLast()->getPos()<<" status:"<<convertFlags2String(mpParticles->getLast()->getFlags())<<" vel:"<< mpParticles->getLast()->getVel()  );
				} // multiple parts
			} // doAdd
		} // */


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
#if FSGR_LISTTRICK==1
		if((oldFlag&CFNoNbEmpty)&&(newFlag&CFNoNbEmpty)) { TEST_IF_CHECK; }
		if(newFlag&CFNoBndFluid) { // test NEW TEST
			if(!iffilled) {
				// remove cells independent from amount of change...
				if( (oldFlag & CFNoNbEmpty)&&(newFlag & CFNoNbEmpty)&&
						( (mass>(rho*FSGR_LISTTTHRESHFULL))  || ((nbored&CFInter)==0)  )) { 
					//if((nbored&CFInter)==0){ errMsg("NBORED!CFINTER","filled "<<PRINT_IJK); };
					iffilled = 1; 
				} 
			}
			if(!ifemptied) {
				if( (oldFlag & CFNoNbFluid)&&(newFlag & CFNoNbFluid)&&
						( (mass<(rho*FSGR_LISTTTHRESHEMPTY)) || ((nbored&CFInter)==0)  )) { 
					//if((nbored&CFInter)==0){ errMsg("NBORED!CFINTER","emptied "<<PRINT_IJK); };
					ifemptied = 1; 
				} 
			}
		} // nobndfluid only */
#endif
		//iffilled = ifemptied = 0; // DEBUG!!!!!!!!!!!!!!!
		

		// now that all dfs are known, handle last changes
		if(iffilled) {
			LbmPoint filledp; filledp.flag=0;
			if(!(newFlag&CFNoBndFluid)) filledp.flag |= 1;  // NEWSURFT
			filledp.x = i; filledp.y = j; filledp.z = k;
#if PARALLEL==1
			//calcListFull[id].push_back( filledp );
#else // PARALLEL==1
			//mListFull.push_back( filledp );
#endif // PARALLEL==1
			LIST_FULL(filledp);
			//this->mNumFilledCells++; // DEBUG
			calcCellsFilled++;
		}
		else if(ifemptied) {
			LbmPoint emptyp; emptyp.flag=0;
			if(!(newFlag&CFNoBndFluid)) emptyp.flag |= 1; //  NEWSURFT
			emptyp.x = i; emptyp.y = j; emptyp.z = k;
#if PARALLEL==1
			//calcListEmpty[id].push_back( emptyp );
#else // PARALLEL==1
			//mListEmpty.push_back( emptyp );
#endif // PARALLEL==1
			LIST_EMPTY(emptyp);
			//this->mNumEmptiedCells++; // DEBUG
			calcCellsEmptied++;
		} 
		// dont cutoff values -> better cell conversions
		RAC(tcel,dFfrac)   = (mass/rho);

		// init new flux value
		float flux = FLUX_INIT; // dxqn on
		if(newFlag&CFNoBndFluid) {
			//flux = 50.0; // extreme on
			for(int nn=1; nn<this->cDfNum; nn++) { 
				if(nbflag[nn] & (CFFluid|CFInter|CFBnd)) { flux += this->dfLength[nn]; }
			}
			// optical hack - smooth slow moving
			// surface regions
			if(usqr< sssUsqrLimit) {
			for(int nn=1; nn<this->cDfNum; nn++) { 
				if(nbfracs[nn]!=0.0) {
					LbmFloat curSmooth = (sssUsqrLimit-usqr)*sssUsqrLimitInv;
					if(curSmooth>1.0) curSmooth=1.0;
					flux *= (1.0+ smoothStrength*curSmooth * (nbfracs[nn]-myfrac)) ;
				}
			} }
			// NEW TEST */
		}
		// flux = FLUX_INIT; // calc flux off
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
	//frintf(stderr," (id=%d k=%d) ",id,k);
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
	this->mNumFilledCells  = calcCellsFilled;
	this->mNumEmptiedCells = calcCellsEmptied;
	this->mNumUsedCells = calcNumUsedCells;
#if PARALLEL==1
	PARA_FINISH();
#endif // PARALLEL==1

	// check other vars...?
}



/******************************************************************************
 * work on lists from updateCellMass to reinit cell flags
 *****************************************************************************/

LbmFloat LbmFsgrSolver::getMassdWeight(bool dirForw, int i,int j,int k,int workSet, int l) {
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

void LbmFsgrSolver::addToNewInterList( int ni, int nj, int nk ) {
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
	LbmPoint newinter; newinter.flag = 0;
	newinter.x = ni; newinter.y = nj; newinter.z = nk;
	mListNewInter.push_back(newinter);
}

void LbmFsgrSolver::reinitFlags( int workSet ) { 
	// reinitCellFlags OLD mods:
	// add all to intel list?
	// check ffrac for new cells
	// new if cell inits (last loop)
	// vweights handling

	const int debugFlagreinit = 0;
	
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
			int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
			//if((LBMDIM>2)&&( (ni<=0) || (nj<=0) || (nk<=0) || (ni>=mLevel[workLev].lSizex-1) || (nj>=mLevel[workLev].lSizey-1) || (nk>=mLevel[workLev].lSizez-1) )) {
			if( (ni<=0) || (nj<=0) || 
				  (ni>=mLevel[workLev].lSizex-1) ||
				  (nj>=mLevel[workLev].lSizey-1) 
#					if LBMDIM==3
				  || (nk<=0) || (nk>=mLevel[workLev].lSizez-1) 
#					endif // LBMDIM==1
				 ) {
				continue; } // new bc, dont treat cells on boundary NEWBC
      if( RFLAG(workLev, ni,nj,nk, workSet) & CFEmpty ){
				
				// preinit speed, get from average surrounding cells
				// interpolate from non-workset to workset, sets are handled in function

				// new and empty interface cell, dont change old flag here!
				addToNewInterList(ni,nj,nk);

				LbmFloat avgrho = 0.0;
				LbmFloat avgux = 0.0, avguy = 0.0, avguz = 0.0;
				interpolateCellValues(workLev,ni,nj,nk,workSet, avgrho,avgux,avguy,avguz);

				// careful with l's...
				FORDF0M { 
					QCELL(workLev,ni,nj,nk, workSet, m) = this->getCollideEq( m,avgrho,  avgux, avguy, avguz ); 
					//QCELL(workLev,ni,nj,nk, workSet, l) = avgnbdf[l]; // CHECK FIXME test?
				}
				//errMsg("FNEW", PRINT_VEC(ni,nj,nk)<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" rho"<<avgrho<<" vel"<<PRINT_VEC(avgux,avguy,avguz) ); // DEBUG SYMM
				QCELL(workLev,ni,nj,nk, workSet, dMass) = 0.0; //?? new
				QCELL(workLev,ni,nj,nk, workSet, dFfrac) = 0.0; //?? new
				//RFLAG(workLev,ni,nj,nk,workSet) = (CellFlagType)(CFInter|CFNoInterpolSrc);
				changeFlag(workLev,ni,nj,nk,workSet, (CFInter|CFNoInterpolSrc));
				if(debugFlagreinit) errMsg("NEWE", PRINT_IJK<<" newif "<<PRINT_VEC(ni,nj,nk)<<" rho"<<avgrho<<" vel("<<avgux<<","<<avguy<<","<<avguz<<") " );
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
  for(int n=0; n<(int)mListEmpty.size(); n++) {
    int i=mListEmpty[n].x, j=mListEmpty[n].y, k=mListEmpty[n].z;
		if((RFLAG(workLev,i,j,k, workSet)&(CFInter|CFNoDelete)) == (CFInter|CFNoDelete)) {
			// treat as "new inter"
			addToNewInterList(i,j,k);
			// remove entry
			if(debugFlagreinit) errMsg("EMPT REMOVED!!!", PRINT_IJK<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" rho"<< QCELL(workLev, i,j,k, workSet, 0) ); // DEBUG SYMM
			if(n<(int)mListEmpty.size()-1) mListEmpty[n] = mListEmpty[mListEmpty.size()-1]; 
			mListEmpty.pop_back();
			n--; 
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
			int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFFluid){
				// init fluid->interface 
				//RFLAG(workLev,ni,nj,nk, workSet) = (CellFlagType)(CFInter); 
				changeFlag(workLev,ni,nj,nk, workSet, CFInter); 
				/* new mass = current density */
				LbmFloat nbrho = QCELL(workLev,ni,nj,nk, workSet, dC);
    		for(int rl=1; rl< this->cDfNum ; ++rl) { nbrho += QCELL(workLev,ni,nj,nk, workSet, rl); }
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
		changeFlag(workLev,i,j,k, workSet, CFEmpty);
		// mark cell not be changed mass... - not necessary, not in list anymore anyway!
	} // emptylist


	
	// precompute weights to get rid of order dependancies
	vector<lbmFloatSet> vWeights;
	vWeights.resize( mListFull.size() + mListEmpty.size() );
	int weightIndex = 0;
  int nbCount = 0;
	LbmFloat nbWeights[LBM_DFNUM];
	LbmFloat nbTotWeights = 0.0;
	for( vector<LbmPoint>::iterator iter=mListFull.begin();
       iter != mListFull.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
    nbCount = 0; nbTotWeights = 0.0;
    FORDF1 {
			int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				nbCount++;
				if(iter->flag&1) nbWeights[l] = 1.; // NEWSURFT
				else nbWeights[l] = getMassdWeight(1,i,j,k,workSet,l); // NEWSURFT
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
			int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
      if( RFLAG(workLev,ni,nj,nk, workSet) & CFInter) {
				nbCount++;
				if(iter->flag&1) nbWeights[l] = 1.; // NEWSURFT
				else nbWeights[l] = getMassdWeight(0,i,j,k,workSet,l); // NEWSURFT
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
		if(vWeights[weightIndex].numNbs>0.0) {
			const LbmFloat nbTotWeightsp = vWeights[weightIndex].val[0];
			//errMsg("FF  I", PRINT_IJK<<" "<<weightIndex<<" "<<nbTotWeightsp);
			FORDF1 {
				int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
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
			this->mFixMass += massChange;
			//TTT mNumProblems++;
			//errMsg(" FULL PROBLEM ", PRINT_IJK<<" "<<this->mFixMass);
		}
		weightIndex++;

    // already done? RFLAG(workLev,i,j,k, workSet) = CFFluid;
    QCELL(workLev,i,j,k, workSet, dMass) = myrho; // should be rho... but unused?
    QCELL(workLev,i,j,k, workSet, dFfrac) = 1.0; // should be rho... but unused?
  } // fulllist


	/* now, finally handle the empty cells - order is important, has to be after
	 * full cell handling */
  for( vector<LbmPoint>::iterator iter=mListEmpty.begin();
       iter != mListEmpty.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;

    LbmFloat massChange = QCELL(workLev, i,j,k, workSet, dMass);
		if(vWeights[weightIndex].numNbs>0.0) {
			const LbmFloat nbTotWeightsp = vWeights[weightIndex].val[0];
			//errMsg("EE  I", PRINT_IJK<<" "<<weightIndex<<" "<<nbTotWeightsp);
			FORDF1 {
				int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
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
			this->mFixMass += massChange;
			//TTT mNumProblems++;
			//errMsg(" EMPT PROBLEM ", PRINT_IJK<<" "<<this->mFixMass);
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
    changeFlag(workLev,i,j,k, otherSet, CFEmpty);
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
	if(debugFlagreinit) errMsg("NEWIF", "total:"<<mListNewInter.size());
	float newIfFac = 1.0/(LbmFloat)numNewIf;
  for( vector<LbmPoint>::iterator iter=mListNewInter.begin();
       iter != mListNewInter.end(); iter++ ) {
    int i=iter->x, j=iter->y, k=iter->z;
		if((i<=0) || (j<=0) || 
			 (i>=mLevel[workLev].lSizex-1) ||
			 (j>=mLevel[workLev].lSizey-1) ||
			 ((LBMDIM==3) && ((k<=0) || (k>=mLevel[workLev].lSizez-1) ) )
			 ) {
			continue; } // new bc, dont treat cells on boundary NEWBC
		if(!(RFLAG(workLev,i,j,k, workSet)&CFInter)) { 
			//errMsg("???"," "<<PRINT_IJK);
			continue; 
		} // */

    QCELL(workLev,i,j,k, workSet, dMass) += (this->mFixMass * newIfFac);
		
		int nbored = 0;
		FORDF1 { nbored |= RFLAG_NB(workLev, i,j,k, workSet,l); }
		if(!(nbored & CFBndNoslip)) { RFLAG(workLev,i,j,k, workSet) |= CFNoBndFluid; }
		if(!(nbored & CFFluid))     { RFLAG(workLev,i,j,k, workSet) |= CFNoNbFluid; }
		if(!(nbored & CFEmpty))     { RFLAG(workLev,i,j,k, workSet) |= CFNoNbEmpty; }

		if(!(RFLAG(workLev,i,j,k, otherSet)&CFInter)) {
			RFLAG(workLev,i,j,k, workSet) = (CellFlagType)(RFLAG(workLev,i,j,k, workSet) | CFNoDelete);
		}
		if(debugFlagreinit) errMsg("NEWIF", PRINT_IJK<<" mss"<<QCELL(workLev, i,j,k, workSet, dMass) <<" f"<< convertCellFlagType2String(RFLAG(workLev,i,j,k, workSet))<<" wl"<<workLev );
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
		//errMsg("FixMassDisted"," fm:"<<this->mFixMass<<" nif:"<<mListNewInter.size() );
		this->mFixMass = 0.0; 
	}

	// empty lists for next step
	mListFull.clear();
	mListEmpty.clear();
	mListNewInter.clear();
} // reinitFlags



