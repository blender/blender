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
			} else {
				performRefinement(lev);
				performCoarsening(lev);
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
#ifndef USE_MSVC6FIXES
		int avgcls = (int)(mAvgNumUsedCells/(long long int)D::mStepCnt);
#else
		int avgcls = (int)(mAvgNumUsedCells/(_int64)D::mStepCnt);
#endif
		debMsgDirect( 
			"mlsups(curr:"<<D::mMLSUPS<<
			" avg:"<<(mAvgMLSUPS/mAvgMLSUPSCnt)<<"), "<< sepStr<<
			" totcls:"<<(D::mNumUsedCells)<< sepStr<<
			" avgcls:"<< avgcls<< sepStr<<
			" intd:"<<mNumInterdCells<< sepStr<<
			" invif:"<<mNumInvIfCells<< sepStr<<
			" invift:"<<mNumInvIfTotal<< sepStr<<
			" fsgrcs:"<<mNumFsgrChanges<< sepStr<<
			" filled:"<<D::mNumFilledCells<<", emptied:"<<D::mNumEmptiedCells<< sepStr<<
			" mMxv:"<<mMxvx<<","<<mMxvy<<","<<mMxvz<<", tscnts:"<<mTimeSwitchCounts<< sepStr<<
			" probs:"<<mNumProblems<< sepStr<<
			" simt:"<<mSimulationTime<< sepStr<<
			" for '"<<D::mName<<"' " );

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
	/*if((D::mStepCnt%100)==1){
		std::ostringstream name;
		name <<"fill_" << D::mStepCnt <<".dump";
		FILE *file = fopen(name.str().c_str(),"w");
		for(int k= getForZMinBnd(); k< getForZMaxBnd(mMaxRefine); ++k)  {
		 for(int j=0;j<mLevel[mMaxRefine].lSizey-0;j++)  {
			for(int i=0;i<mLevel[mMaxRefine].lSizex-0;i++) {
				float val = QCELL(mMaxRefine,i,j,k, mLevel[mMaxRefine].setCurr,dFfrac);
				//fwrite( &val, sizeof(val), 1, file); // binary
				fprintf(file, "%f ",val); // text
				//errMsg("W", PRINT_IJK<<" val:"<<val);
			}
			fprintf(file, "\n"); // text
		 }
			fprintf(file, "\n"); // text
		}
		fclose(file);
	} // */

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
	LbmFloat calcCurrentMass = 0.0;
	LbmFloat calcCurrentVolume = 0.0;
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
#if OPT3D==1 
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
				rho = fluidRho;
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
			//? LbmFloat ofrho=RAC(ccel,0); for(int l=1; l<D::cDfNum; l++) { ofrho += RAC(ccel,l); }
//FST errMsg("TTTfl","at "<<PRINT_IJK<<", rho"<<rho );
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
		if(!(nbored&CFBnd)) {
			newFlag |= CFNoBndFluid;
		}

		// store own dfs and mass
		mass = RAC(ccel,dMass);

		// WARNING - only interface cells arrive here!
		// read distribution funtions of adjacent cells = stream step
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
			if( NBFLAG(l)&(CFFluid|CFBnd) ) { // NEWTEST! FIXME check!!!!!!!!!!!!!!!!!!
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

			//FST errMsg("TTTIF","at "<<PRINT_IJK<<",l"<<l<<" m"<<mass<<",c"<<change<<"        nbflag"<<convertCellFlagType2String(NBFLAG(l))<<" nbdf"<<nbdf(l)<<" mydf"<<MYDF(l)<<"         isflix"<<((NBFLAG(l)&(CFFluid|CFBnd) )!=0) );
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
#if OPT3D==0
			oldRho=RAC(ccel,0);
			oldUx = oldUy = oldUz = 0.0;
			for(int l=1; l<D::cDfNum; l++) {
				oldRho += RAC(ccel,l);
				oldUx  += (D::dfDvecX[l]*RAC(ccel,l));
				oldUy  += (D::dfDvecY[l]*RAC(ccel,l)); 
				oldUz  += (D::dfDvecZ[l]*RAC(ccel,l)); 
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
				m[ D::dfInv[l] ] = 
					D::getCollideEq(l, REFERENCE_PRESSURE, oldUx,oldUy,oldUz) + 
					D::getCollideEq(D::dfInv[l], REFERENCE_PRESSURE, oldUx,oldUy,oldUz) 
					- MYDF( l );
			}
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
#if FSGR_LISTTRICK==1
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
//FST errMsg("M","i "<<mass);
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
#if OPT3D==1 
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
#if OPT3D==1 
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
#if OPT3D==1 
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

#				if OPT3D==0
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
#				endif // OPT3D==0

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
	// FIXME remove TIMEINTORDER ?
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

	LbmFloat levOldOmega[FSGR_MAXNOOFLEVELS];
	LbmFloat levOldStepsize[FSGR_MAXNOOFLEVELS];
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

template<class D>
void LbmFsgrSolver<D>::interpolateCellFromCoarse(int lev, int i, int j,int k, int dstSet, LbmFloat t, CellFlagType flagSet, bool markNbs) {
	LbmFloat rho=0.0, ux=0.0, uy=0.0, uz=0.0;
	LbmFloat intDf[19] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

#if OPT3D==1 
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
	}
	else if(( betx) && (!bety) && (!betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D1);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D1);
	}
	else if((!betx) && ( bety) && (!betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D1);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D1);
	}
	else if((!betx) && (!bety) && ( betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D1);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D1);
	}
	else if(( betx) && ( bety) && (!betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)+1,(k/2)  , t, WO1D2);
	}
	else if((!betx) && ( bety) && ( betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)+1, t, WO1D2);
	}
	else if(( betx) && (!bety) && ( betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D2);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)+1, t, WO1D2);
	}
	else if(( betx) && ( bety) && ( betz)) {
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)  ,(k/2)+1, t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)  ,(k/2)+1, t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)+1,(k/2)  , t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)  ,(j/2)+1,(k/2)+1, t, WO1D3);
		ADD_INT_DFS(lev-1, (i/2)+1,(j/2)+1,(k/2)+1, t, WO1D3);
	}
	else {
		D::mPanic=1;
		errFatal("interpolateCellFromCoarse","Invalid!?", SIMWORLD_GENERICERROR);	
	}

	IDF_WRITEBACK;
	return;
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



//! lbm factory functions
LbmSolverInterface* createSolver() {
#if LBMDIM==2
	return new LbmFsgrSolver< LbmBGK2D >();
#endif // LBMDIM==2
#if LBMDIM==3
	return new LbmFsgrSolver< LbmBGK3D >();
#endif // LBMDIM==3
	return NULL;
}

#if LBMDIM==2
template class LbmFsgrSolver< LbmBGK2D >;
#endif // LBMDIM==2
#if LBMDIM==3
template class LbmFsgrSolver< LbmBGK3D >;
#endif // LBMDIM==3
