/** \file elbeem/intern/solver_adap.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Adaptivity functions
 *
 *****************************************************************************/

#include "solver_class.h"
#include "solver_relax.h"
#include "particletracer.h"



/*****************************************************************************/
//! coarse step functions
/*****************************************************************************/



void LbmFsgrSolver::coarseCalculateFluxareas(int lev)
{
	FSGR_FORIJK_BOUNDS(lev) {
		if( RFLAG(lev, i,j,k,mLevel[lev].setCurr) & CFFluid) {
			if( RFLAG(lev+1, i*2,j*2,k*2,mLevel[lev+1].setCurr) & CFGrFromCoarse) {
				LbmFloat totArea = mFsgrCellArea[0]; // for l=0
				for(int l=1; l<this->cDirNum; l++) { 
					int ni=(2*i)+this->dfVecX[l], nj=(2*j)+this->dfVecY[l], nk=(2*k)+this->dfVecZ[l];
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
	if(!this->mSilent){ debMsgStd("coarseCalculateFluxareas",DM_MSG,"level "<<lev<<" calculated", 7); }
}
	
void LbmFsgrSolver::coarseAdvance(int lev)
{
	LbmFloat calcCurrentMass = 0.0;
	LbmFloat calcCurrentVolume = 0.0;

	LbmFloat *ccel = NULL;
	LbmFloat *tcel = NULL;
	LbmFloat m[LBM_DFNUM];
	LbmFloat rho, ux, uy, uz, tmp, usqr, lcsmqo;
#if OPT3D==1 
	LbmFloat lcsmqadd, lcsmeq[LBM_DFNUM], lcsmomega;
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
	//if(strstr(this->getName().c_str(),"Debug")){ errMsg("DEBUG","DEBUG!!!!!!!!!!!!!!!!!!!!!!!"); }

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
			FORDF1 { if(RFLAG_NB(lev, i, j, k, SRCS(lev), l) & CFUnused) { invNb = true; } }   
			if(!invNb) {
				// WARNING - this modifies source flag array...
				*pFlagSrc = CFFluid|CFGrNorm;
#if ELBEEM_PLUGIN!=1
				errMsg("coarseAdvance","FC2NRM_CHECK Converted CFGrFromCoarse to Norm at "<<lev<<" "<<PRINT_IJK);
#endif // ELBEEM_PLUGIN!=1
				// move to perform coarsening?
			}
		} // */

#if FSGR_STRICT_DEBUG==1
		*pFlagDst = *pFlagSrc; // always set other set...
#else
		*pFlagDst = (*pFlagSrc & (~CFGrCoarseInited)); // always set other set... , remove coarse inited flag
#endif

		// old INTCFCOARSETEST==1
		if((*pFlagSrc) & CFGrFromCoarse) {  // interpolateFineFromCoarse test!
			if(( this->mStepCnt & (1<<(mMaxRefine-lev)) ) ==1) {
				FORDF0 { RAC(tcel,l) = RAC(ccel,l); }
			} else {
				interpolateCellFromCoarse( lev, i,j,k, TSET(lev), 0.0, CFFluid|CFGrFromCoarse, false);
				this->mNumUsedCells++;
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
				CAUSE_PANIC;
			}
#endif // FSGR_STRICT_DEBUG==1
			this->mNumUsedCells++;

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
	if(!this->mSilent){ errMsg("coarseAdvance","level "<<lev<<" stepped from "<<SRCS(lev)<<" to "<<TSET(lev)); }
	// */

	// update other set
  mLevel[lev].setOther   = mLevel[lev].setCurr;
  mLevel[lev].setCurr   ^= 1;
  mLevel[lev].lsteps++;
  mLevel[lev].lmass   = calcCurrentMass   * mLevel[lev].lcellfactor;
  mLevel[lev].lvolume = calcCurrentVolume * mLevel[lev].lcellfactor;
#if ELBEEM_PLUGIN!=1
  debMsgStd("LbmFsgrSolver::coarseAdvance",DM_NOTIFY, "mass: lev="<<lev<<" m="<<mLevel[lev].lmass<<" c="<<calcCurrentMass<<"  lcf="<< mLevel[lev].lcellfactor, 8 );
  debMsgStd("LbmFsgrSolver::coarseAdvance",DM_NOTIFY, "volume: lev="<<lev<<" v="<<mLevel[lev].lvolume<<" c="<<calcCurrentVolume<<"  lcf="<< mLevel[lev].lcellfactor, 8 );
#endif // ELBEEM_PLUGIN
}

/*****************************************************************************/
//! multi level functions
/*****************************************************************************/


// get dfs from level (lev+1) to (lev) coarse border nodes
void LbmFsgrSolver::coarseRestrictFromFine(int lev)
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

	//restrict
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
	for(int j=1;j<mLevel[lev].lSizey-1;++j) {
	for(int i=1;i<mLevel[lev].lSizex-1;++i) {
		CellFlagType *pFlagSrc = &RFLAG(lev, i,j,k,dstSet);
		if((*pFlagSrc) & (CFFluid)) { 
			if( ((*pFlagSrc) & (CFFluid|CFGrFromFine)) == (CFFluid|CFGrFromFine) ) { 
				// do resctriction
				mNumInterdCells++;
				coarseRestrictCell(lev, i,j,k,srcSet,dstSet);

				this->mNumUsedCells++;
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
	if(!this->mSilent){ errMsg("coarseRestrictFromFine"," from l"<<(lev+1)<<",s"<<mLevel[lev+1].setCurr<<" to l"<<lev<<",s"<<mLevel[lev].setCurr); }
}

bool LbmFsgrSolver::adaptGrid(int lev) {
	if((lev<0) || ((lev+1)>mMaxRefine)) return false;
	bool change = false;
	{ // refinement, PASS 1-3

	//bool nbsok;
	// FIXME remove TIMEINTORDER ?
	LbmFloat interTime = 0.0;
	// update curr from other, as streaming afterwards works on curr
	// thus read only from srcSet, modify other
	const int srcSet = mLevel[lev].setOther;
	const int dstSet = mLevel[lev].setCurr;
	const int srcFineSet = mLevel[lev+1].setCurr;
	const bool debugRefinement = false;

	// use //template functions for 2D/3D
			/*if(strstr(this->getName().c_str(),"Debug"))
			if(lev+1==mMaxRefine) { // mixborder
				for(int l=0;((l<this->cDirNum) && (!removeFromFine)); l++) {  // FARBORD
					int ni=2*i+2*this->dfVecX[l], nj=2*j+2*this->dfVecY[l], nk=2*k+2*this->dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, srcFineSet)&CFBnd) { // NEWREFT
						removeFromFine=true;
					}
				}
			} // FARBORD */
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {

		if(RFLAG(lev, i,j,k, srcSet) & CFGrFromFine) {
			bool removeFromFine = false;
			const CellFlagType notAllowed = (CFInter|CFGrFromFine|CFGrToFine);
			CellFlagType reqType = CFGrNorm;
			if(lev+1==mMaxRefine) reqType = CFNoBndFluid;
			
			if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), srcFineSet) & reqType) &&
			    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), srcFineSet) & (notAllowed)) )  ){ // ok
			} else {
				removeFromFine=true;
			}

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
				if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev,i,j,k); 
				change=true;
				mNumFsgrChanges++;
				for(int l=1; l<this->cDirNum; l++) { 
					int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
					//errMsg("performRefinement","On lev:"<<lev<<" check: "<<PRINT_VEC(ni,nj,nk)<<" set:"<<dstSet<<" = "<<convertCellFlagType2String(RFLAG(lev, ni,nj,nk, srcSet)) );
					if( (  RFLAG(lev, ni,nj,nk, srcSet)&CFFluid      ) &&
							(!(RFLAG(lev, ni,nj,nk, srcSet)&CFGrFromFine)) ) { // dont change status of nb. from fine cells
						// tag as inited for debugging, cell contains fluid DFs anyway
						RFLAG(lev, ni,nj,nk, dstSet) = CFFluid|CFGrFromFine|CFGrCoarseInited;
						//errMsg("performRefinement","On lev:"<<lev<<" set to from fine: "<<PRINT_VEC(ni,nj,nk)<<" set:"<<dstSet);
						//if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev,ni,nj,nk); 
					}
				} // l 

				// FIXME fix fine level?
			}

			// recheck from fine flag
		}
	}}} // TEST
	// PASS 1 */


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
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) { // TEST
  for(int j=1;j<mLevel[lev].lSizey-1;++j) { // TEST
  for(int i=1;i<mLevel[lev].lSizex-1;++i) { // TEST

		// test from coarseAdvance
		// from coarse cells without unused nbs are not necessary...! -> remove

		if(RFLAG(lev, i,j,k, srcSet) & CFGrFromCoarse) {

			// from coarse cells without unused nbs are not necessary...! -> remove
			bool invNb = false;
			bool fluidNb = false;
			for(int l=1; l<this->cDirNum; l++) { 
				if(RFLAG_NB(lev, i, j, k, srcSet, l) & CFUnused) { invNb = true; }
				if(RFLAG_NB(lev, i, j, k, srcSet, l) & (CFGrNorm)) { fluidNb = true; }
			}   
			if(!invNb) {
				// no unused cells around -> calculate normally from now on
				RFLAG(lev, i,j,k, dstSet) = CFFluid|CFGrNorm;
				if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev, i, j, k); 
				change=true;
				mNumFsgrChanges++;
			} // from advance 
			if(!fluidNb) {
				// no fluid cells near -> no transfer necessary
				RFLAG(lev, i,j,k, dstSet) = CFUnused;
				//RFLAG(lev, i,j,k, mLevel[lev].setOther) = CFUnused; // FLAGTEST
				if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev, i, j, k); 
				change=true;
				mNumFsgrChanges++;
			} // from advance 


			// dont allow double transfer
			// this might require fixing the neighborhood
			if(RFLAG(lev+1, 2*i,2*j,2*k, srcFineSet)&(CFGrFromCoarse)) { 
				// dont turn CFGrFromFine above interface cells into CFGrNorm
				//errMsg("performRefinement","Removing CFGrFromCoarse on lev"<<lev<<" " <<PRINT_IJK<<" due to finer from coarse cell " );
				RFLAG(lev, i,j,k, dstSet) = CFFluid|CFGrNorm;
				if(lev>0) RFLAG(lev-1, i/2,j/2,k/2, mLevel[lev-1].setCurr) &= (~CFGrToFine); // TODO add more of these?
				if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev, i, j, k); 
				change=true;
				mNumFsgrChanges++;
				for(int l=1; l<this->cDirNum; l++) { 
					int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
					if(RFLAG(lev, ni,nj,nk, srcSet)&(CFGrNorm)) { //ok
						for(int m=1; m<this->cDirNum; m++) { 
							int mi=  ni +this->dfVecX[m], mj=  nj +this->dfVecY[m], mk=  nk +this->dfVecZ[m];
							if(RFLAG(lev,  mi, mj, mk, srcSet)&CFUnused) {
								// norm cells in neighborhood with unused nbs have to be new border...
								RFLAG(lev, ni,nj,nk, dstSet) = CFFluid|CFGrFromCoarse;
								if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev,ni,nj,nk); 
							}
						}
						// these alreay have valid values...
					}
					else if(RFLAG(lev, ni,nj,nk, srcSet)&(CFUnused)) { //ok
						// this should work because we have a valid neighborhood here for now
						interpolateCellFromCoarse(lev,  ni, nj, nk, dstSet, interTime, CFFluid|CFGrFromCoarse, false);
						if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev,ni,nj,nk); 
						mNumFsgrChanges++;
					}
				} // l 
			} // double transer

		} // from coarse

	} } }
	// PASS 2 */


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
				for(int l=1; l<this->cDirNum; l++) { 
					int bi=(2*i)+this->dfVecX[l], bj=(2*j)+this->dfVecY[l], bk=(2*k)+this->dfVecZ[l];
					if(RFLAG(lev+1,  bi, bj, bk, srcFineSet)&(CFGrFromCoarse)) {
						//errMsg("performRefinement","Removing CFGrFromCoarse on lev"<<(lev+1)<<" "<<PRINT_VEC(bi,bj,bk) );
						RFLAG(lev+1,  bi, bj, bk, srcFineSet) = setf;
						if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev+1,bi,bj,bk); 
					}
					else if(RFLAG(lev+1,  bi, bj, bk, srcFineSet)&(CFUnused      )) { 
						//errMsg("performRefinement","Removing CFUnused on lev"<<(lev+1)<<" "<<PRINT_VEC(bi,bj,bk) );
						interpolateCellFromCoarse(lev+1,  bi, bj, bk, srcFineSet, interTime, setf, false);
						if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev+1,bi,bj,bk); 
						mNumFsgrChanges++;
					}
				}
				for(int l=1; l<this->cDirNum; l++) { 
					int bi=(2*i)+this->dfVecX[l], bj=(2*j)+this->dfVecY[l], bk=(2*k)+this->dfVecZ[l];
					if(   (RFLAG(lev+1,  bi, bj, bk, srcFineSet)&CFFluid       ) &&
							(!(RFLAG(lev+1,  bi, bj, bk, srcFineSet)&CFGrFromCoarse)) ) {
						// all unused nbs now of coarse have to be from coarse
						for(int m=1; m<this->cDirNum; m++) { 
							int mi=  bi +this->dfVecX[m], mj=  bj +this->dfVecY[m], mk=  bk +this->dfVecZ[m];
							if(RFLAG(lev+1,  mi, mj, mk, srcFineSet)&CFUnused) {
								//errMsg("performRefinement","Changing CFUnused on lev"<<(lev+1)<<" "<<PRINT_VEC(mi,mj,mk) );
								interpolateCellFromCoarse(lev+1,  mi, mj, mk, srcFineSet, interTime, CFFluid|CFGrFromCoarse, false);
								if((LBMDIM==2)&&(debugRefinement)) debugMarkCell(lev+1,mi,mj,mk); 
								mNumFsgrChanges++;
							}
						}
						// nbs prepared...
					}
				}
			}
			
		} // convert regions of from fine
	}}} // TEST
	// PASS 3 */

	if(!this->mSilent){ errMsg("performRefinement"," for l"<<lev<<" done ("<<change<<") " ); }
	} // PASS 1-3
	// refinement done

	//LbmFsgrSolver::performCoarsening(int lev) {
	{ // PASS 4,5
	bool nbsok;
	// WARNING
	// now work on modified curr set
	const int srcSet = mLevel[lev].setCurr;
	const int dstlev = lev+1;
	const int dstFineSet = mLevel[dstlev].setCurr;
	const bool debugCoarsening = false;

	// PASS 5 test DEBUG
	/*if(this->mInitDone) {
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {
			if(RFLAG(lev, i,j,k, srcSet) & CFEmpty) {
				// check empty -> from fine conversion
				bool changeToFromFine = false;
				const CellFlagType notAllowed = (CFInter|CFGrFromFine|CFGrToFine);
				CellFlagType reqType = CFGrNorm;
				if(lev+1==mMaxRefine) reqType = CFNoBndFluid;
				if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & reqType) &&
				    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (notAllowed)) )  ){
					changeToFromFine=true; }
				if(changeToFromFine) {
					change = true;
					mNumFsgrChanges++;
					RFLAG(lev, i,j,k, srcSet) = CFFluid|CFGrFromFine;
					if((LBMDIM==2)&&(debugCoarsening)) debugMarkCell(lev,i,j,k); 
					// same as restr from fine func! not necessary ?!
					// coarseRestrictFromFine part 
					coarseRestrictCell(lev, i,j,k,srcSet, dstFineSet);
				}
			} // only check empty cells
	}}} // TEST!
	} // PASS 5 */

	// use //template functions for 2D/3D
					/*if(strstr(this->getName().c_str(),"Debug"))
					if((nbsok)&&(lev+1==mMaxRefine)) { // mixborder
						for(int l=0;((l<this->cDirNum) && (nbsok)); l++) {  // FARBORD
							int ni=2*i+2*this->dfVecX[l], nj=2*j+2*this->dfVecY[l], nk=2*k+2*this->dfVecZ[l];
							if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&CFBnd) { // NEWREFT
								nbsok=false;
							}
						}
					} // FARBORD */
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
				for(int l=0; l<this->cDirNum && nbsok; l++) { 
					int ni=(2*i)+this->dfVecX[l], nj=(2*j)+this->dfVecY[l], nk=(2*k)+this->dfVecZ[l];
					if(   (RFLAG(lev+1, ni,nj,nk, dstFineSet) & reqType) &&
							(!(RFLAG(lev+1, ni,nj,nk, dstFineSet) & (notAllowed)) )  ){
						// ok
					} else {
						nbsok=false;
					}
					// FARBORD
				}
				// dont turn CFGrFromFine above interface cells into CFGrNorm
				// now check nbs on same level
				for(int l=1; l<this->cDirNum && nbsok; l++) { 
					int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
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
					//if(this->mInitDone) errMsg("performCoarsening","CFGrFromFine changed to CFGrNorm at lev"<<lev<<" " <<PRINT_IJK );
					if((LBMDIM==2)&&(debugCoarsening)) debugMarkCell(lev,i,j,k); 

					// only check complete cubes
					for(int dx=-1;dx<=1;dx+=2) {
					for(int dy=-1;dy<=1;dy+=2) {
					for(int dz=-1*(LBMDIM&1);dz<=1*(LBMDIM&1);dz+=2) { // 2d/3d
						// check for norm and from coarse, as the coarse level might just have been refined...
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
							//if(this->mInitDone) errMsg("performCoarsening","CFGrFromFine subcube init center unused set l"<<dstlev<<" at "<<PRINT_VEC(dstx,dsty,dstz) );

							for(int l=1; l<this->cDirNum; l++) { 
								int dstni=dstx+this->dfVecX[l], dstnj=dsty+this->dfVecY[l], dstnk=dstz+this->dfVecZ[l];
								if(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)&(CFFluid)) { 
									RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet) = CFFluid|CFGrFromCoarse;
								}
								if(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)&(CFInter)) { 
									//if(this->mInitDone) errMsg("performCoarsening","CFGrFromFine subcube init CHECK Warning - deleting interface cell...");
									this->mFixMass += QCELL( dstlev, dstni,dstnj,dstnk, dstFineSet, dMass);
									RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet) = CFFluid|CFGrFromCoarse;
								}
							} // l

							// again check nb flags of all surrounding cells to see if any from coarse
							// can be convted to unused
							for(int l=1; l<this->cDirNum; l++) { 
								int dstni=dstx+this->dfVecX[l], dstnj=dsty+this->dfVecY[l], dstnk=dstz+this->dfVecZ[l];
								// have to be at least from coarse here...
								//errMsg("performCoarsening","CFGrFromFine subcube init unused check l"<<dstlev<<" at "<<PRINT_VEC(dstni,dstnj,dstnk)<<" "<< convertCellFlagType2String(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)) );
								if(!(RFLAG(dstlev, dstni,dstnj,dstnk, dstFineSet)&(CFUnused) )) { 
									bool delok = true;
									// careful long range here... check domain bounds?
									for(int m=1; m<this->cDirNum; m++) { 										
										int chkni=dstni+this->dfVecX[m], chknj=dstnj+this->dfVecY[m], chknk=dstnk+this->dfVecZ[m];
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
										if((LBMDIM==2)&&(debugCoarsening)) debugMarkCell(dstlev,dstni,dstnj,dstnk); 
									}
								}
							} // l
							// treat subcube
							//ebugMarkCell(lev,i+dx,j+dy,k+dz); 
							//if(this->mInitDone) errMsg("performCoarsening","CFGrFromFine subcube init, dir:"<<PRINT_VEC(dx,dy,dz) );
						}
					} } }

				}   // ?
			} // convert regions of from fine
	}}} // TEST!
	// PASS 4 */

		// reinit cell area value
		/*if( RFLAG(lev, i,j,k,srcSet) & CFFluid) {
			if( RFLAG(lev+1, i*2,j*2,k*2,dstFineSet) & CFGrFromCoarse) {
				LbmFloat totArea = mFsgrCellArea[0]; // for l=0
				for(int l=1; l<this->cDirNum; l++) { 
					int ni=(2*i)+this->dfVecX[l], nj=(2*j)+this->dfVecY[l], nk=(2*k)+this->dfVecZ[l];
					if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&
							(CFGrFromCoarse|CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
							//(CFUnused|CFEmpty) //? (CFBnd|CFEmpty|CFGrFromCoarse|CFUnused)
							) { 
						//LbmFloat area = 0.25; if(this->dfVecX[l]!=0) area *= 0.5; if(this->dfVecY[l]!=0) area *= 0.5; if(this->dfVecZ[l]!=0) area *= 0.5;
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


	// PASS 5 org
	/*if(strstr(this->getName().c_str(),"Debug"))
	if((changeToFromFine)&&(lev+1==mMaxRefine)) { // mixborder
		for(int l=0;((l<this->cDirNum) && (changeToFromFine)); l++) {  // FARBORD
			int ni=2*i+2*this->dfVecX[l], nj=2*j+2*this->dfVecY[l], nk=2*k+2*this->dfVecZ[l];
			if(RFLAG(lev+1, ni,nj,nk, dstFineSet)&CFBnd) { // NEWREFT
				changeToFromFine=false; }
		} 
	}// FARBORD */
	//if(!this->mInitDone) {
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) {
  for(int j=1;j<mLevel[lev].lSizey-1;++j) {
  for(int i=1;i<mLevel[lev].lSizex-1;++i) {


			if(RFLAG(lev, i,j,k, srcSet) & CFEmpty) {
				// check empty -> from fine conversion
				bool changeToFromFine = false;
				const CellFlagType notAllowed = (CFInter|CFGrFromFine|CFGrToFine);
				CellFlagType reqType = CFGrNorm;
				if(lev+1==mMaxRefine) reqType = CFNoBndFluid;

				if(   (RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & reqType) &&
				    (!(RFLAG(lev+1, (2*i),(2*j),(2*k), dstFineSet) & (notAllowed)) )  ){
					// DEBUG 
					changeToFromFine=true;
				}

				// FARBORD

				if(changeToFromFine) {
					change = true;
					mNumFsgrChanges++;
					RFLAG(lev, i,j,k, srcSet) = CFFluid|CFGrFromFine;
					if((LBMDIM==2)&&(debugCoarsening)) debugMarkCell(lev,i,j,k); 
					// same as restr from fine func! not necessary ?!
					// coarseRestrictFromFine part 
				}
			} // only check empty cells

	}}} // TEST!
	//} // init done
	// PASS 5 */
	} // coarsening, PASS 4,5

	if(!this->mSilent){ errMsg("adaptGrid"," for l"<<lev<<" done " ); }
	return change;
}

/*****************************************************************************/
//! cell restriction and prolongation
/*****************************************************************************/

void LbmFsgrSolver::coarseRestrictCell(int lev, int i,int j,int k, int srcSet, int dstSet)
{
	LbmFloat *ccel = RACPNT(lev+1, 2*i,2*j,2*k,srcSet);
	LbmFloat *tcel = RACPNT(lev  , i,j,k      ,dstSet);

	LbmFloat rho=0.0, ux=0.0, uy=0.0, uz=0.0;			
	//LbmFloat *ccel = NULL;
	//LbmFloat *tcel = NULL;
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

#				if OPT3D==0
	// add up weighted dfs
	FORDF0{ df[l] = 0.0;}
	for(int n=0;(n<this->cDirNum); n++) { 
		int ni=2*i+1*this->dfVecX[n], nj=2*j+1*this->dfVecY[n], nk=2*k+1*this->dfVecZ[n];
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
		ux  += (this->dfDvecX[l]*cdf); 
		uy  += (this->dfDvecY[l]*cdf);  
		uz  += (this->dfDvecZ[l]*cdf);  
	}

	FORDF0{ feq[l] = this->getCollideEq(l, rho,ux,uy,uz); }
	if(mLevel[lev  ].lcsmago>0.0) {
		const LbmFloat Qo = this->getLesNoneqTensorCoeff(df,feq);
		omegaDst  = this->getLesOmega(mLevel[lev  ].omega,mLevel[lev  ].lcsmago,Qo);
		omegaSrc = this->getLesOmega(mLevel[lev+1].omega,mLevel[lev+1].lcsmago,Qo);
	} else {
		omegaDst = mLevel[lev+0].omega; /* NEWSMAGOT*/ 
		omegaSrc = mLevel[lev+1].omega;
	}
	dfScale   = (mLevel[lev  ].timestep/mLevel[lev+1].timestep)* (1.0/omegaDst-1.0)/ (1.0/omegaSrc-1.0); // yu
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
	for(int n=1;(n<this->cDirNum); n++) { 
		ccel = RACPNT(lev+1,  2*i+1*this->dfVecX[n], 2*j+1*this->dfVecY[n], 2*k+1*this->dfVecZ[n]  ,srcSet);
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
	lcsmdfscale   = (mLevel[lev+0].timestep/mLevel[lev+1].timestep)* (1.0/lcsmDstOmega-1.0)/ (1.0/lcsmSrcOmega-1.0);  \
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
}

void LbmFsgrSolver::interpolateCellFromCoarse(int lev, int i, int j,int k, int dstSet, LbmFloat t, CellFlagType flagSet, bool markNbs) {
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
		int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
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
		CAUSE_PANIC;
		errFatal("interpolateCellFromCoarse","Invalid!?", SIMWORLD_GENERICERROR);	
	}

	IDF_WRITEBACK;
	return;
}



#define MPTADAP_INTERV 4

/*****************************************************************************/
/*! change the  size of the LBM time step */
/*****************************************************************************/
void LbmFsgrSolver::adaptTimestep() {
	LbmFloat massTOld=0.0, massTNew=0.0;
	LbmFloat volTOld=0.0, volTNew=0.0;

	bool rescale = false;  // do any rescale at all?
	LbmFloat scaleFac = -1.0; // timestep scaling
	if(mPanic) return;

	LbmFloat levOldOmega[FSGR_MAXNOOFLEVELS];
	LbmFloat levOldStepsize[FSGR_MAXNOOFLEVELS];
	for(int lev=mMaxRefine; lev>=0 ; lev--) {
		levOldOmega[lev] = mLevel[lev].omega;
		levOldStepsize[lev] = mLevel[lev].timestep;
	}

	const LbmFloat reduceFac = 0.8;          // modify time step by 20%, TODO? do multiple times for large changes?
	LbmFloat diffPercent = 0.05; // dont scale if less than 5%
	LbmFloat allowMax = mpParam->getTadapMaxSpeed();  // maximum allowed velocity
	LbmFloat nextmax = mpParam->getSimulationMaxSpeed() + norm(mLevel[mMaxRefine].gravity);

	// sync nextmax
#if LBM_INCLUDE_TESTSOLVERS==1
	if(glob_mpactive) {
		if(mLevel[mMaxRefine].lsteps % MPTADAP_INTERV != MPTADAP_INTERV-1) {
			debMsgStd("LbmFsgrSolver::TAdp",DM_MSG, "mpact:"<<glob_mpactive<<","<<glob_mpindex<<"/"<<glob_mpnum<<" step:"<<mLevel[mMaxRefine].lsteps<<" skipping tadap...",8);
			return;
		}
		nextmax = mrInitTadap(nextmax);
	}
#endif // LBM_INCLUDE_TESTSOLVERS==1

	LbmFloat newdt = mpParam->getTimestep(); // newtr
	if(nextmax > allowMax/reduceFac) {
		mTimeMaxvelStepCnt++; }
	else { mTimeMaxvelStepCnt=0; }
	
	// emergency, or 10 steps with high vel
	if((mTimeMaxvelStepCnt>5) || (nextmax> (1.0/3.0)) || (mForceTimeStepReduce) ) {
		newdt = mpParam->getTimestep() * reduceFac;
	} else {
		if(nextmax<allowMax*reduceFac) {
			newdt = mpParam->getTimestep() / reduceFac;
		}
	} // newtr
	//errMsg("LbmFsgrSolver::adaptTimestep","nextmax="<<nextmax<<" allowMax="<<allowMax<<" fac="<<reduceFac<<" simmaxv="<< mpParam->getSimulationMaxSpeed() );

	bool minCutoff = false;
	LbmFloat desireddt = newdt;
	if(newdt>mpParam->getMaxTimestep()){ newdt = mpParam->getMaxTimestep(); }
	if(newdt<mpParam->getMinTimestep()){ 
		newdt = mpParam->getMinTimestep(); 
		if(nextmax>allowMax/reduceFac){	minCutoff=true; } // only if really large vels...
	}

	LbmFloat dtdiff = fabs(newdt - mpParam->getTimestep());
	if(!mSilent) {
		debMsgStd("LbmFsgrSolver::TAdp",DM_MSG, "new"<<newdt
			<<" max"<<mpParam->getMaxTimestep()<<" min"<<mpParam->getMinTimestep()<<" diff"<<dtdiff
			<<" simt:"<<mSimulationTime<<" minsteps:"<<(mSimulationTime/mMaxTimestep)<<" maxsteps:"<<(mSimulationTime/mMinTimestep)<<
			" olddt"<<levOldStepsize[mMaxRefine]<<" redlock"<<mTimestepReduceLock 
		 	, 10); }

	// in range, and more than X% change?
	//if( newdt <  mpParam->getTimestep() ) // DEBUG
	LbmFloat rhoAvg = mCurrentMass/mCurrentVolume;
	if( (newdt<=mpParam->getMaxTimestep()) && (newdt>=mpParam->getMinTimestep()) 
			&& (dtdiff>(mpParam->getTimestep()*diffPercent)) ) {
		if((newdt>levOldStepsize[mMaxRefine])&&(mTimestepReduceLock)) {
			// wait some more...
			//debMsgNnl("LbmFsgrSolver::TAdp",DM_NOTIFY," Delayed... "<<mTimestepReduceLock<<" ",10);
			debMsgDirect("D");
		} else {
			mpParam->setDesiredTimestep( newdt );
			rescale = true;
			if(!mSilent) {
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"\n\n\n\n",10);
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"Timestep changing: new="<<newdt<<" old="<<mpParam->getTimestep()
						<<" maxSpeed:"<<mpParam->getSimulationMaxSpeed()<<" next:"<<nextmax<<" step:"<<mStepCnt, 10 );
				//debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"Timestep changing: "<< "rhoAvg="<<rhoAvg<<" cMass="<<mCurrentMass<<" cVol="<<mCurrentVolume,10);
			}
		} // really change dt
	}

	if(mTimestepReduceLock>0) mTimestepReduceLock--;

	
	// forced back and forth switchting (for testing)
	/*const int tadtogInter = 100;
	const double tadtogSwitch = 0.66;
	errMsg("TIMESWITCHTOGGLETEST","warning enabled "<< tadtogSwitch<<","<<tadtogSwitch<<" !!!!!!!!!!!!!!!!!!!");
	if( ((mStepCnt% tadtogInter)== (tadtogInter/4*1)-1) ||
	    ((mStepCnt% tadtogInter)== (tadtogInter/4*2)-1) ){
		rescale = true; minCutoff = false;
		newdt = tadtogSwitch * mpParam->getTimestep();
		mpParam->setDesiredTimestep( newdt );
	} else 
	if( ((mStepCnt% tadtogInter)== (tadtogInter/4*3)-1) ||
	    ((mStepCnt% tadtogInter)== (tadtogInter/4*4)-1) ){
		rescale = true; minCutoff = false;
		newdt = mpParam->getTimestep()/tadtogSwitch ;
		mpParam->setDesiredTimestep( newdt );
	} else {
		rescale = false; minCutoff = false;
	}
	// */

	// test mass rescale

	scaleFac = newdt/mpParam->getTimestep();
	if(rescale) {
		// perform acutal rescaling...
		mTimeMaxvelStepCnt=0; 
		mForceTimeStepReduce = false;

		// FIXME - approximate by averaging, take gravity direction here?
		//mTimestepReduceLock = 4*(mLevel[mMaxRefine].lSizey+mLevel[mMaxRefine].lSizez+mLevel[mMaxRefine].lSizex)/3;
		// use z as gravity direction
		mTimestepReduceLock = 4*mLevel[mMaxRefine].lSizez;

		mTimeSwitchCounts++;
		mpParam->calculateAllMissingValues( mSimulationTime, mSilent );
		recalculateObjectSpeeds();
		// calc omega, force for all levels
		mLastOmega=1e10; mLastGravity=1e10;
		initLevelOmegas();
		if(mpParam->getTimestep()<mMinTimestep) mMinTimestep = mpParam->getTimestep();
		if(mpParam->getTimestep()>mMaxTimestep) mMaxTimestep = mpParam->getTimestep();

		// this might be called during init, before we have any particles
		if(mpParticles) { mpParticles->adaptPartTimestep(scaleFac); }
#if LBM_INCLUDE_TESTSOLVERS==1
		if((mUseTestdata)&&(mpTest)) { 
			mpTest->adaptTimestep(scaleFac, mLevel[mMaxRefine].omega, mLevel[mMaxRefine].timestep, vec2L( mpParam->calculateGravity(mSimulationTime)) ); 
			mpTest->mGrav3d = mLevel[mMaxRefine].gravity;
		}
#endif // LBM_INCLUDE_TESTSOLVERS!=1
	
		for(int lev=mMaxRefine; lev>=0 ; lev--) {
			LbmFloat newSteptime = mLevel[lev].timestep;
			LbmFloat dfScaleFac = (newSteptime/1.0)/(levOldStepsize[lev]/levOldOmega[lev]);

			if(!mSilent) {
				debMsgStd("LbmFsgrSolver::TAdp",DM_NOTIFY,"Level: "<<lev<<" Timestep chlevel: "<<
						" scaleFac="<<dfScaleFac<<" newDt="<<newSteptime<<" newOmega="<<mLevel[lev].omega,10);
			}
			if(lev!=mMaxRefine) coarseCalculateFluxareas(lev);

			int wss = 0, wse = 1;
			// only change currset (necessary for compressed grids, better for non-compr.gr.)
			wss = wse = mLevel[lev].setCurr;
			for(int workSet = wss; workSet<=wse; workSet++) { // COMPRT
					// warning - check sets for higher levels...?
				FSGR_FORIJK1(lev) {
					if( (RFLAG(lev,i,j,k, workSet) & CFBndMoving) ) {
						/*
						// paranoid check - shouldnt be necessary!
						if(QCELL(lev, i, j, k, workSet, dFlux)!=mSimulationTime) {
							errMsg("TTTT","found invalid bnd cell.... removing at "<<PRINT_IJK);
							RFLAG(lev,i,j,k, workSet) = CFInter;
							// init empty zero vel  interface cell...
							initVelocityCell(lev, i,j,k, CFInter, 1.0, 0.01, LbmVec(0.) );
						} else {// */
							for(int l=0; l<cDfNum; l++) {
								QCELL(lev, i, j, k, workSet, l) = QCELL(lev, i, j, k, workSet, l)* scaleFac; 
							}
						//} //  ok
						continue;
					}
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
					for(int l=0; l<cDfNum; l++) {
						LbmFloat m = QCELL(lev, i, j, k, workSet, l); 
						rho += m;
						ux  += (dfDvecX[l]*m);
						uy  += (dfDvecY[l]*m); 
						uz  += (dfDvecZ[l]*m); 
					} 
					rhoOld = rho;
					velOld = LbmVec(ux,uy,uz);

					LbmFloat rhoNew = (rhoOld-rhoAvg)*scaleFac +rhoAvg;
					LbmVec velNew = velOld * scaleFac;

					LbmFloat df[LBM_DFNUM];
					LbmFloat feqOld[LBM_DFNUM];
					LbmFloat feqNew[LBM_DFNUM];
					for(int l=0; l<cDfNum; l++) {
						feqOld[l] = getCollideEq(l,rhoOld, velOld[0],velOld[1],velOld[2] );
						feqNew[l] = getCollideEq(l,rhoNew, velNew[0],velNew[1],velNew[2] );
						df[l] = QCELL(lev, i,j,k,workSet, l);
					}
					const LbmFloat Qo = getLesNoneqTensorCoeff(df,feqOld);
					const LbmFloat oldOmega = getLesOmega(levOldOmega[lev], mLevel[lev].lcsmago,Qo);
					const LbmFloat newOmega = getLesOmega(mLevel[lev].omega,mLevel[lev].lcsmago,Qo);
					//newOmega = mLevel[lev].omega; // FIXME debug test

					//LbmFloat dfScaleFac = (newSteptime/1.0)/(levOldStepsize[lev]/levOldOmega[lev]);
					const LbmFloat dfScale = (newSteptime/newOmega)/(levOldStepsize[lev]/oldOmega);
					//dfScale = dfScaleFac/newOmega;
					
					for(int l=0; l<cDfNum; l++) {
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

		if(!mSilent) {
			debMsgStd("LbmFsgrSolver::step",DM_MSG,"REINIT DONE "<<mStepCnt<<
					" no"<<mTimeSwitchCounts<<" maxdt"<<mMaxTimestep<<
					" mindt"<<mMinTimestep<<" currdt"<<mLevel[mMaxRefine].timestep, 10);
			debMsgStd("LbmFsgrSolver::step",DM_MSG,"REINIT DONE  masst:"<<massTNew<<","<<massTOld<<" org:"<<mCurrentMass<<"; "<<
					" volt:"<<volTNew<<","<<volTOld<<" org:"<<mCurrentVolume, 10);
		} else {
			debMsgStd("\nLbmOptSolver::step",DM_MSG,"Timestep changed by "<< (newdt/levOldStepsize[mMaxRefine]) <<" newDt:"<<newdt
					<<", oldDt:"<<levOldStepsize[mMaxRefine]<<" newOmega:"<<mOmega<<" gStar:"<<mpParam->getCurrentGStar()<<", step:"<<mStepCnt , 10);
		}
	} // rescale?
	//NEWDEBCHECK("tt2");
	
	//errMsg("adaptTimestep","Warning - brute force rescale off!"); minCutoff = false; // DEBUG
	if(minCutoff) {
		errMsg("adaptTimestep","Warning - performing Brute-Force rescale... (sim:"<<mName<<" step:"<<mStepCnt<<" newdt="<<desireddt<<" mindt="<<mpParam->getMinTimestep()<<") " );
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
			for(int l=0; l<cDfNum; l++) {
				LbmFloat m = QCELL(lev, i, j, k, workSet, l); 
				rho += m;
				ux  += (dfDvecX[l]*m);
				uy  += (dfDvecY[l]*m); 
				uz  += (dfDvecZ[l]*m); 
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
				for(int l=0; l<cDfNum; l++) {
					QCELL(lev, i, j, k, workSet, l) = getCollideEq(l, rho, ux,uy,uz); }
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



