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
#include "particletracer.h"

// MPT
#include "ntl_world.h"
#include "simulation_object.h"

#include <stdlib.h>
#include <zlib.h>
#ifndef sqrtf
#define sqrtf sqrt
#endif

/******************************************************************************
 * helper functions
 *****************************************************************************/

// try to enhance surface?
#define SURFACE_ENH 2

extern bool glob_mpactive;
extern bool glob_mpnum;
extern bool glob_mpindex;

//! for raytracing
void LbmFsgrSolver::prepareVisualization( void ) {
	int lev = mMaxRefine;
	int workSet = mLevel[lev].setCurr;

	int mainGravDir=6; // if normalizing fails, we asume z-direction gravity
	LbmFloat mainGravLen = 0.;
	FORDF1{
		LbmFloat thisGravLen = dot(LbmVec(dfVecX[l],dfVecY[l],dfVecZ[l]), mLevel[mMaxRefine].gravity );	
		if(thisGravLen>mainGravLen) {
			mainGravLen = thisGravLen;
			mainGravDir = l;
		}
	}

#if LBMDIM==2
	// 2d, place in the middle of isofield slice (k=2)
#  define ZKD1 0
	// 2d z offset = 2, lbmGetData adds 1, so use one here
#  define ZKOFF 1
	// reset all values...
	for(int k= 0; k< 5; ++k) 
   for(int j=0;j<mLevel[lev].lSizey-0;j++) 
    for(int i=0;i<mLevel[lev].lSizex-0;i++) {
		*mpIso->lbmGetData(i,j,ZKOFF)=0.0;
	}
#else // LBMDIM==2
	// 3d, use normal bounds
#  define ZKD1 1
#  define ZKOFF k
	// reset all values...
	for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
   for(int j=0;j<mLevel[lev].lSizey-0;j++) 
    for(int i=0;i<mLevel[lev].lSizex-0;i++) {
		*mpIso->lbmGetData(i,j,ZKOFF)=0.0;
	}
#endif // LBMDIM==2

	// MPT, ignore
	if((glob_mpactive) && (glob_mpnum>1) && (glob_mpindex==0)) {
		mpIso->resetAll(0.);
	}


	LbmFloat minval = mIsoValue*1.05; // / mIsoWeight[13]; 
	// add up...
	float val = 0.0;
	for(int k= getForZMin1(); k< getForZMax1(lev); ++k) 
   for(int j=1;j<mLevel[lev].lSizey-1;j++) 
    for(int i=1;i<mLevel[lev].lSizex-1;i++) {
			const CellFlagType cflag = RFLAG(lev, i,j,k,workSet);
			//if(cflag&(CFBnd|CFEmpty)) {

#if SURFACE_ENH==0

			// no enhancements...
			if( (cflag&(CFFluid|CFUnused)) ) {
				val = 1.;
			} else if( (cflag&CFInter) ) {
				val = (QCELL(lev, i,j,k,workSet, dFfrac)); 
			} else {
				continue;
			}

#else // SURFACE_ENH!=1
			if(cflag&CFBnd) {
				// treated in second loop
				continue;
			} else if(cflag&CFUnused) {
				val = 1.;
			} else if( (cflag&CFFluid) && (cflag&CFNoBndFluid)) {
				// optimized fluid
				val = 1.;
			} else if( (cflag&(CFEmpty|CFInter|CFFluid)) ) {
				int noslipbnd = 0;
				int intercnt = 0;
				FORDF1 { 
					const CellFlagType nbflag = RFLAG_NB(lev, i,j,k, workSet,l);
					if(nbflag&CFInter){ intercnt++; }

					// check all directions otherwise we get bugs with splashes on obstacles
					if(l!=mainGravDir) continue; // only check bnd along main grav. dir
					//if((nbflag&CFBnd)&&(nbflag&CFBndNoslip)){ noslipbnd=1; }
					if((nbflag&CFBnd)){ noslipbnd=1; }
				}

				if(cflag&CFEmpty) {
					// special empty treatment
					if((noslipbnd)&&(intercnt>6)) {
						*mpIso->lbmGetData(i,j,ZKOFF) += minval;
					} else if((noslipbnd)&&(intercnt>0)) {
						// necessary?
						*mpIso->lbmGetData(i,j,ZKOFF) += mIsoValue*0.9;
					} else {
						// nothing to do...
					}

					continue;
				} else if(cflag&(CFNoNbEmpty|CFFluid)) {
					// no empty nb interface cells are treated as full
					val=1.0;
				} else {
					val = (QCELL(lev, i,j,k,workSet, dFfrac)); 
				}
				
				if(noslipbnd) {
					if(val<minval) val = minval; 
					*mpIso->lbmGetData(i,j,ZKOFF) += minval-( val * mIsoWeight[13] ); 
				}
			} else { // all others, unused?
				continue;
			} 
#endif // SURFACE_ENH>0

			*mpIso->lbmGetData( i-1 , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[0] ); 
			*mpIso->lbmGetData( i   , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[1] ); 
			*mpIso->lbmGetData( i+1 , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[2] ); 

			*mpIso->lbmGetData( i-1 , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[3] ); 
			*mpIso->lbmGetData( i   , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[4] ); 
			*mpIso->lbmGetData( i+1 , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[5] ); 

			*mpIso->lbmGetData( i-1 , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[6] ); 
			*mpIso->lbmGetData( i   , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[7] ); 
			*mpIso->lbmGetData( i+1 , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[8] ); 


			*mpIso->lbmGetData( i-1 , j-1  ,ZKOFF  ) += ( val * mIsoWeight[9] ); 
			*mpIso->lbmGetData( i   , j-1  ,ZKOFF  ) += ( val * mIsoWeight[10] ); 
			*mpIso->lbmGetData( i+1 , j-1  ,ZKOFF  ) += ( val * mIsoWeight[11] ); 

			*mpIso->lbmGetData( i-1 , j    ,ZKOFF  ) += ( val * mIsoWeight[12] ); 
			*mpIso->lbmGetData( i   , j    ,ZKOFF  ) += ( val * mIsoWeight[13] ); 
			*mpIso->lbmGetData( i+1 , j    ,ZKOFF  ) += ( val * mIsoWeight[14] ); 

			*mpIso->lbmGetData( i-1 , j+1  ,ZKOFF  ) += ( val * mIsoWeight[15] ); 
			*mpIso->lbmGetData( i   , j+1  ,ZKOFF  ) += ( val * mIsoWeight[16] ); 
			*mpIso->lbmGetData( i+1 , j+1  ,ZKOFF  ) += ( val * mIsoWeight[17] ); 


			*mpIso->lbmGetData( i-1 , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[18] ); 
			*mpIso->lbmGetData( i   , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[19] ); 
			*mpIso->lbmGetData( i+1 , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[20] ); 

			*mpIso->lbmGetData( i-1 , j   ,ZKOFF+ZKD1) += ( val * mIsoWeight[21] ); 
			*mpIso->lbmGetData( i   , j   ,ZKOFF+ZKD1)+= ( val * mIsoWeight[22] ); 
			*mpIso->lbmGetData( i+1 , j   ,ZKOFF+ZKD1) += ( val * mIsoWeight[23] ); 

			*mpIso->lbmGetData( i-1 , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[24] ); 
			*mpIso->lbmGetData( i   , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[25] ); 
			*mpIso->lbmGetData( i+1 , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[26] ); 
	}

	// TEST!?
#if SURFACE_ENH>=2

	if(mFsSurfGenSetting&fssgNoObs) {
		for(int k= getForZMin1(); k< getForZMax1(lev); ++k) 
		 for(int j=1;j<mLevel[lev].lSizey-1;j++) 
			for(int i=1;i<mLevel[lev].lSizex-1;i++) {
				const CellFlagType cflag = RFLAG(lev, i,j,k,workSet);
				if(cflag&(CFBnd)) {
					CellFlagType nbored=0;
					LbmFloat avgfill=0.,avgfcnt=0.;
					FORDF1 { 
						const int ni = i+dfVecX[l];
						const int nj = j+dfVecY[l];
						const int nk = ZKOFF+dfVecZ[l];

						const CellFlagType nbflag = RFLAG(lev, ni,nj,nk, workSet);
						nbored |= nbflag;
						if(nbflag&CFInter) {
							avgfill += QCELL(lev, ni,nj,nk, workSet,dFfrac); avgfcnt += 1.;
						} else if(nbflag&CFFluid) {
							avgfill += 1.; avgfcnt += 1.;
						} else if(nbflag&CFEmpty) {
							avgfill += 0.; avgfcnt += 1.;
						}

						//if( (ni<0) || (nj<0) || (nk<0) 
						 //|| (ni>=mLevel[mMaxRefine].lSizex) 
						 //|| (nj>=mLevel[mMaxRefine].lSizey) 
						 //|| (nk>=mLevel[mMaxRefine].lSizez) ) continue;
					}

					if(nbored&CFInter) {
						if(avgfcnt>0.) avgfill/=avgfcnt;
						*mpIso->lbmGetData(i,j,ZKOFF) = avgfill; continue;
					} 
					else if(nbored&CFFluid) {
						*mpIso->lbmGetData(i,j,ZKOFF) = 1.; continue;
					}

				}
			}

		// move surface towards inner "row" of obstacle
		// cells if necessary (all obs cells without fluid/inter
		// nbs (=iso==0) next to obstacles...)
		for(int k= getForZMin1(); k< getForZMax1(lev); ++k) 
			for(int j=1;j<mLevel[lev].lSizey-1;j++) 
				for(int i=1;i<mLevel[lev].lSizex-1;i++) {
					const CellFlagType cflag = RFLAG(lev, i,j,k,workSet);
					if( (cflag&(CFBnd)) && (*mpIso->lbmGetData(i,j,ZKOFF)==0.)) {
						int bndnbcnt=0;
						FORDF1 { 
							const int ni = i+dfVecX[l];
							const int nj = j+dfVecY[l];
							const int nk = ZKOFF+dfVecZ[l];
							const CellFlagType nbflag = RFLAG(lev, ni,nj,nk, workSet);
							if(nbflag&CFBnd) bndnbcnt++;
						}
						if(bndnbcnt>0) *mpIso->lbmGetData(i,j,ZKOFF)=mIsoValue*0.95; 
					}
				}
	}
	// */

	if(mFsSurfGenSetting&fssgNoNorth) 
		for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
			for(int j=0;j<mLevel[lev].lSizey-0;j++) {
				*mpIso->lbmGetData(0,                   j,ZKOFF) = *mpIso->lbmGetData(1,                   j,ZKOFF);
			}
	if(mFsSurfGenSetting&fssgNoEast) 
		for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
			for(int i=0;i<mLevel[lev].lSizex-0;i++) {
				*mpIso->lbmGetData(i,0,                   ZKOFF) = *mpIso->lbmGetData(i,1,                   ZKOFF);
			}
	if(mFsSurfGenSetting&fssgNoSouth) 
		for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
			for(int j=0;j<mLevel[lev].lSizey-0;j++) {
				*mpIso->lbmGetData(mLevel[lev].lSizex-1,j,ZKOFF) = *mpIso->lbmGetData(mLevel[lev].lSizex-2,j,ZKOFF);
			}
	if(mFsSurfGenSetting&fssgNoWest) 
		for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
			for(int i=0;i<mLevel[lev].lSizex-0;i++) {
				*mpIso->lbmGetData(i,mLevel[lev].lSizey-1,ZKOFF) = *mpIso->lbmGetData(i,mLevel[lev].lSizey-2,ZKOFF);
			}
	if(LBMDIM>2) {
		if(mFsSurfGenSetting&fssgNoBottom) 
			for(int j=0;j<mLevel[lev].lSizey-0;j++) 
				for(int i=0;i<mLevel[lev].lSizex-0;i++) {
					*mpIso->lbmGetData(i,j,0                   ) = *mpIso->lbmGetData(i,j,1                   );
				} 
		if(mFsSurfGenSetting&fssgNoTop) 
			for(int j=0;j<mLevel[lev].lSizey-0;j++) 
				for(int i=0;i<mLevel[lev].lSizex-0;i++) {
					*mpIso->lbmGetData(i,j,mLevel[lev].lSizez-1) = *mpIso->lbmGetData(i,j,mLevel[lev].lSizez-2);
				} 
	}
#endif // SURFACE_ENH>=2


	// update preview, remove 2d?
	if((mOutputSurfacePreview)&&(LBMDIM==3)) {
		int pvsx = (int)(mPreviewFactor*mSizex);
		int pvsy = (int)(mPreviewFactor*mSizey);
		int pvsz = (int)(mPreviewFactor*mSizez);
		//float scale = (float)mSizex / previewSize;
		LbmFloat scalex = (LbmFloat)mSizex/(LbmFloat)pvsx;
		LbmFloat scaley = (LbmFloat)mSizey/(LbmFloat)pvsy;
		LbmFloat scalez = (LbmFloat)mSizez/(LbmFloat)pvsz;
		for(int k= 0; k< (pvsz-1); ++k) 
   		for(int j=0;j< pvsy;j++) 
    		for(int i=0;i< pvsx;i++) {
					*mpPreviewSurface->lbmGetData(i,j,k) = *mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley), (int)(k*scalez) );
				}
		// set borders again...
		for(int k= 0; k< (pvsz-1); ++k) {
			for(int j=0;j< pvsy;j++) {
				*mpPreviewSurface->lbmGetData(0,j,k) = *mpIso->lbmGetData( 0, (int)(j*scaley), (int)(k*scalez) );
				*mpPreviewSurface->lbmGetData(pvsx-1,j,k) = *mpIso->lbmGetData( mSizex-1, (int)(j*scaley), (int)(k*scalez) );
			}
			for(int i=0;i< pvsx;i++) {
				*mpPreviewSurface->lbmGetData(i,0,k) = *mpIso->lbmGetData( (int)(i*scalex), 0, (int)(k*scalez) );
				*mpPreviewSurface->lbmGetData(i,pvsy-1,k) = *mpIso->lbmGetData( (int)(i*scalex), mSizey-1, (int)(k*scalez) );
			}
		}
		for(int j=0;j<pvsy;j++)
			for(int i=0;i<pvsx;i++) {      
				*mpPreviewSurface->lbmGetData(i,j,0) = *mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley) , 0);
				*mpPreviewSurface->lbmGetData(i,j,pvsz-1) = *mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley) , mSizez-1);
			}

		if(mFarFieldSize>=1.2) {
			// also remove preview border
			for(int k= 0; k< (pvsz-1); ++k) {
				for(int j=0;j< pvsy;j++) {
					*mpPreviewSurface->lbmGetData(0,j,k) = 
					*mpPreviewSurface->lbmGetData(1,j,k) =  
					*mpPreviewSurface->lbmGetData(2,j,k);
					*mpPreviewSurface->lbmGetData(pvsx-1,j,k) = 
					*mpPreviewSurface->lbmGetData(pvsx-2,j,k) = 
					*mpPreviewSurface->lbmGetData(pvsx-3,j,k);
					//0.0;
				}
				for(int i=0;i< pvsx;i++) {
					*mpPreviewSurface->lbmGetData(i,0,k) = 
					*mpPreviewSurface->lbmGetData(i,1,k) = 
					*mpPreviewSurface->lbmGetData(i,2,k);
					*mpPreviewSurface->lbmGetData(i,pvsy-1,k) = 
					*mpPreviewSurface->lbmGetData(i,pvsy-2,k) = 
					*mpPreviewSurface->lbmGetData(i,pvsy-3,k);
					//0.0;
				}
			}
			for(int j=0;j<pvsy;j++)
				for(int i=0;i<pvsx;i++) {      
					*mpPreviewSurface->lbmGetData(i,j,0) = 
					*mpPreviewSurface->lbmGetData(i,j,1) = 
					*mpPreviewSurface->lbmGetData(i,j,2);
					*mpPreviewSurface->lbmGetData(i,j,pvsz-1) = 
					*mpPreviewSurface->lbmGetData(i,j,pvsz-2) = 
					*mpPreviewSurface->lbmGetData(i,j,pvsz-3);
					//0.0;
			}
		}
	}

	// MPT
	#if LBM_INCLUDE_TESTSOLVERS==1
	mrIsoExchange();
	#endif // LBM_INCLUDE_TESTSOLVERS==1

	return;
}

/*! calculate speeds of fluid objects (or inflow) */
void LbmFsgrSolver::recalculateObjectSpeeds() {
	const bool debugRecalc = false;
	int numobjs = (int)(this->mpGiObjects->size());
	// note - (numobjs + 1) is entry for domain settings

	if(debugRecalc) errMsg("recalculateObjectSpeeds","start, #obj:"<<numobjs);
	if(numobjs>255-1) {
		errFatal("LbmFsgrSolver::recalculateObjectSpeeds","More than 256 objects currently not supported...",SIMWORLD_INITERROR);
		return;
	}
	mObjectSpeeds.resize(numobjs+1);
	for(int i=0; i<(int)(numobjs+0); i++) {
		mObjectSpeeds[i] = vec2L(this->mpParam->calculateLattVelocityFromRw( vec2P( (*this->mpGiObjects)[i]->getInitialVelocity(mSimulationTime) )));
		if(debugRecalc) errMsg("recalculateObjectSpeeds","id"<<i<<" set to "<< mObjectSpeeds[i]<<", unscaled:"<< (*this->mpGiObjects)[i]->getInitialVelocity(mSimulationTime) );
	}

	// also reinit part slip values here
	mObjectPartslips.resize(numobjs+1);
	for(int i=0; i<=(int)(numobjs+0); i++) {
		if(i<numobjs) {
			mObjectPartslips[i] = (LbmFloat)(*this->mpGiObjects)[i]->getGeoPartSlipValue();
		} else {
			// domain setting
			mObjectPartslips[i] = this->mDomainPartSlipValue;
		}
		LbmFloat set = mObjectPartslips[i];

		// as in setInfluenceVelocity
		const LbmFloat dt = mLevel[mMaxRefine].timestep;
		const LbmFloat dtInter = 0.01;
		//LbmFloat facFv = 1.-set;
		// mLevel[mMaxRefine].timestep
		LbmFloat facNv = (LbmFloat)( 1.-pow( (double)(set), (double)(dt/dtInter)) );
		errMsg("mObjectPartslips","id:"<<i<<"/"<<numobjs<<"  ts:"<<dt<< " its:"<<(dt/dtInter) <<" set"<<set<<" nv"<<facNv<<" test:"<<
			 pow( (double)(1.-facNv),(double)(dtInter/dt))	);
		mObjectPartslips[i] = facNv;

		if(debugRecalc) errMsg("recalculateObjectSpeeds","id"<<i<<" parts "<< mObjectPartslips[i] );
	}

	if(debugRecalc) errMsg("recalculateObjectSpeeds","done, domain:"<<mObjectPartslips[numobjs]<<" n"<<numobjs);
}



/*****************************************************************************/
/*! debug object display */
/*****************************************************************************/
vector<ntlGeometryObject*> LbmFsgrSolver::getDebugObjects() { 
	vector<ntlGeometryObject*> debo; 
	if(this->mOutputSurfacePreview) {
		debo.push_back( mpPreviewSurface );
	}
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata) {
		vector<ntlGeometryObject*> tdebo; 
		tdebo = mpTest->getDebugObjects();
		for(size_t i=0; i<tdebo.size(); i++) debo.push_back( tdebo[i] );
	}
#endif // ELBEEM_PLUGIN
	return debo; 
}

/******************************************************************************
 * particle handling
 *****************************************************************************/

/*! init particle positions */
int LbmFsgrSolver::initParticles() { 
  int workSet = mLevel[mMaxRefine].setCurr;
  int tries = 0;
  int num = 0;
	ParticleTracer *partt = mpParticles;

  partt->setStart( this->mvGeoStart + ntlVec3Gfx(mLevel[mMaxRefine].nodeSize*0.5) );
  partt->setEnd  ( this->mvGeoEnd   + ntlVec3Gfx(mLevel[mMaxRefine].nodeSize*0.5) );

  partt->setSimStart( ntlVec3Gfx(0.0) );
  partt->setSimEnd  ( ntlVec3Gfx(mSizex,   mSizey,   getForZMaxBnd(mMaxRefine)) );
  
  while( (num<partt->getNumInitialParticles()) && (tries<100*partt->getNumInitialParticles()) ) {
    LbmFloat x,y,z,t;
    x = 1.0+(( (LbmFloat)(mSizex-3.) )          * (rand()/(RAND_MAX+1.0)) );
    y = 1.0+(( (LbmFloat)(mSizey-3.) )          * (rand()/(RAND_MAX+1.0)) );
    z = 1.0+(( (LbmFloat) getForZMax1(mMaxRefine)-2. )* (rand()/(RAND_MAX+1.0)) );
    int i = (int)(x+0.5);
    int j = (int)(y+0.5);
    int k = (int)(z+0.5);
    if(LBMDIM==2) {
      k = 0; z = 0.5; // place in the middle of domain
    }

    //if( RFLAG(mMaxRefine, i,j,k, workSet)& (CFFluid) ) 
    //&& ( RFLAG(mMaxRefine, i,j,k, workSet)& CFNoNbFluid ) 
    //if( RFLAG(mMaxRefine, i,j,k, workSet) & (CFFluid|CFInter|CFMbndInflow) ) { 
    if( RFLAG(mMaxRefine, i,j,k, workSet) & (CFNoBndFluid|CFUnused) ) { 
			bool cellOk = true;
			//? FORDF1 { if(!(RFLAG_NB(mMaxRefine,i,j,k,workSet, l) & CFFluid)) cellOk = false; }
			if(!cellOk) continue;
      // in fluid...
      partt->addParticle(x,y,z);
			partt->getLast()->setStatus(PART_IN);
			partt->getLast()->setType(PART_TRACER);

			partt->getLast()->setSize(1.);
			// randomize size
			partt->getLast()->setSize(0.5 + (rand()/(RAND_MAX+1.0)));

			if( ( partt->getInitStart()>0.)
					&& ( partt->getInitEnd()>0.)
					&& ( partt->getInitEnd()>partt->getInitStart() )) {
    		t = partt->getInitStart()+ (partt->getInitEnd()-partt->getInitStart())*(rand()/(RAND_MAX+1.0));
				partt->getLast()->setLifeTime( -t );
			}
      num++;
    }
    tries++;
  } // */

	/*FSGR_FORIJK1(mMaxRefine) {
		if( (RFLAG(mMaxRefine,i,j,k, workSet) & (CFNoBndFluid)) ) {
    	LbmFloat rndn = (rand()/(RAND_MAX+1.0));
			if(rndn>0.0) {
				ntlVec3Gfx pos( (LbmFloat)(i)-0.5, (LbmFloat)(j)-0.5, (LbmFloat)(k)-0.5 );
				if(LBMDIM==2) { pos[2]=0.5; }
				partt->addParticle( pos[0],pos[1],pos[2] );
				partt->getLast()->setStatus(PART_IN);
				partt->getLast()->setType(PART_TRACER);
				partt->getLast()->setSize(1.0);
			}
		}
	} // */


	// DEBUG TEST
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata) { 
		const bool partDebug=false;
		if(mpTest->mPartTestcase==0){ errMsg("LbmTestdata"," part init "<<mpTest->mPartTestcase); }
		if(mpTest->mPartTestcase==-12){ 
			const int lev = mMaxRefine;
			for(int i=5;i<15;i++) {
				LbmFloat x,y,z;
				y = 0.5+(LbmFloat)(i);
				x = mLevel[lev].lSizex/20.0*10.0;
				z = mLevel[lev].lSizez/20.0*2.0;
				partt->addParticle(x,y,z);
				partt->getLast()->setStatus(PART_IN);
				partt->getLast()->setType(PART_BUBBLE);
				partt->getLast()->setSize(  (-4.0+(LbmFloat)i)/1.0  );
				if(partDebug) errMsg("PARTTT","SET "<<PRINT_VEC(x,y,z)<<" p"<<partt->getLast()->getPos() <<" s"<<partt->getLast()->getSize() );
			}
		}
		if(mpTest->mPartTestcase==-11){ 
			const int lev = mMaxRefine;
			for(int i=5;i<15;i++) {
				LbmFloat x,y,z;
				y = 10.5+(LbmFloat)(i);
				x = mLevel[lev].lSizex/20.0*10.0;
				z = mLevel[lev].lSizez/20.0*40.0;
				partt->addParticle(x,y,z);
				partt->getLast()->setStatus(PART_IN);
				partt->getLast()->setType(PART_DROP);
				partt->getLast()->setSize(  (-4.0+(LbmFloat)i)/1.0  );
				if(partDebug) errMsg("PARTTT","SET "<<PRINT_VEC(x,y,z)<<" p"<<partt->getLast()->getPos() <<" s"<<partt->getLast()->getSize() );
			}
		}
		// place floats on rectangular region FLOAT_JITTER_BND
		if(mpTest->mPartTestcase==-10){ 
			const int lev = mMaxRefine;
			const int sx = mLevel[lev].lSizex;
			const int sy = mLevel[lev].lSizey;
			//for(int j=-(int)(sy*0.25);j<-(int)(sy*0.25)+2;++j) { for(int i=-(int)(sx*0.25);i<-(int)(sy*0.25)+2;++i) {
			//for(int j=-(int)(sy*1.25);j<(int)(2.25*sy);++j) { for(int i=-(int)(sx*1.25);i<(int)(2.25*sx);++i) {
			for(int j=-(int)(sy*0.3);j<(int)(1.3*sy);++j) { for(int i=-(int)(sx*0.3);i<(int)(1.3*sx);++i) {
			//for(int j=-(int)(sy*0.2);j<(int)(0.2*sy);++j) { for(int i= (int)(sx*0.5);i<= (int)(0.51*sx);++i) {
					LbmFloat x,y,z;
					x = 0.0+(LbmFloat)(i);
					y = 0.0+(LbmFloat)(j);
					//z = mLevel[lev].lSizez/10.0*2.5 - 1.0;
					z = mLevel[lev].lSizez/20.0*9.5 - 1.0;
					//z = mLevel[lev].lSizez/20.0*4.5 - 1.0;
					partt->addParticle(x,y,z);
					//if( (i>0)&&(i<sx) && (j>0)&&(j<sy) ) { partt->getLast()->setStatus(PART_IN); } else { partt->getLast()->setStatus(PART_OUT); }
					partt->getLast()->setStatus(PART_IN);
					partt->getLast()->setType(PART_FLOAT);
					partt->getLast()->setSize( 15.0 );
					if(partDebug) errMsg("PARTTT","SET "<<PRINT_VEC(x,y,z)<<" p"<<partt->getLast()->getPos() <<" s"<<partt->getLast()->getSize() );
			 }
		}	}
	} 
	// DEBUG TEST
#endif // LBM_INCLUDE_TESTSOLVERS

	
  debMsgStd("LbmFsgrSolver::initParticles",DM_MSG,"Added "<<num<<" particles, genProb:"<<this->mPartGenProb<<", tries:"<<tries, 10);
  if(num != partt->getNumParticles()) return 1;

	return 0;
}

// helper function for particle debugging
/*static string getParticleStatusString(int state) {
	std::ostringstream out;
	if(state&PART_DROP)   out << "dropp ";
	if(state&PART_TRACER) out << "tracr ";
	if(state&PART_BUBBLE) out << "bubbl ";
	if(state&PART_FLOAT)  out << "float ";
	if(state&PART_INTER)  out << "inter ";

	if(state&PART_IN)   out << "inn ";
	if(state&PART_OUT)  out << "out ";
	if(state&PART_INACTIVE)  out << "INACT ";
	if(state&PART_OUTFLUID)  out << "outfluid ";
	return out.str();
} // */

#define P_CHANGETYPE(p, newtype) \
		p->setLifeTime(0.); \
    /* errMsg("PIT","U pit"<<(p)->getId()<<" pos:"<< (p)->getPos()<<" status:"<<convertFlags2String((p)->getFlags())<<" to "<< (newtype) ); */ \
		p->setType(newtype); 

// tracer defines
#define TRACE_JITTER 0.025
#define TRACE_RAND (rand()/(RAND_MAX+1.0))*TRACE_JITTER-(TRACE_JITTER*0.5)
#define FFGET_NORM(var,dl) \
							if(RFLAG_NB(lev,i,j,k,workSet, dl) &(CFInter)){ (var) = QCELL_NB(lev,i,j,k,workSet,dl,dFfrac); } \
							else if(RFLAG_NB(lev,i,j,k,workSet, dl) &(CFFluid|CFUnused)){ (var) = 1.; } else (var) = 0.0;

// float jitter
#define FLOAT_JITTER_BND (FLOAT_JITTER*2.0)
#define FLOAT_JITTBNDRAND(x) ((rand()/(RAND_MAX+1.0))*FLOAT_JITTER_BND*(1.-(x/(LbmFloat)maxdw))-(FLOAT_JITTER_BND*(1.-(x)/(LbmFloat)maxdw)*0.5)) 

#define DEL_PART { \
	/*errMsg("PIT","DEL AT "<< __LINE__<<" type:"<<p->getType()<<"  ");  */ \
	p->setActive( false ); \
	continue; }

void LbmFsgrSolver::advanceParticles() { 
  const int level = mMaxRefine;
  const int workSet = mLevel[level].setCurr;
	LbmFloat vx=0.0,vy=0.0,vz=0.0;
	//int debugOutCounter=0; // debug output counter

	myTime_t parttstart = getTime(); 
	const LbmFloat cellsize = this->mpParam->getCellSize();
	const LbmFloat timestep = this->mpParam->getTimestep();
	//const LbmFloat viscAir = 1.79 * 1e-5; // RW2L kin. viscosity, mu
	//const LbmFloat viscWater = 1.0 * 1e-6; // RW2L kin. viscosity, mu
	const LbmFloat rhoAir = 1.2;  // [kg m^-3] RW2L
	const LbmFloat rhoWater = 1000.0; // RW2L
	const LbmFloat minDropSize = 0.0005; // [m], = 2mm  RW2L
	const LbmVec   velAir(0.); // [m / s]

	const LbmFloat r1 = 0.005;  // r max
	const LbmFloat r2 = 0.0005; // r min
	const LbmFloat v1 = 9.0; // v max
	const LbmFloat v2 = 2.0; // v min
	const LbmVec rwgrav = vec2L( this->mpParam->getGravity(mSimulationTime) );
	const bool useff = (mFarFieldSize>1.2); // if(mpTest->mFarfMode>0){ 

	// TODO scale bubble/part damping dep. on timestep, also drop bnd rev damping
	const int cutval = mCutoff; // use full border!?
	if(this->mStepCnt%50==49) { mpParticles->cleanup(); }
  for(vector<ParticleObject>::iterator pit= mpParticles->getParticlesBegin();
      pit!= mpParticles->getParticlesEnd(); pit++) {
    //if((*pit).getPos()[2]>10.) errMsg("PIT"," pit"<<(*pit).getId()<<" pos:"<< (*pit).getPos()<<" status:["<<getParticleStatusString((*pit).getFlags())<<"] vel:"<< (*pit).getVel()  );
    if( (*pit).getActive()==false ) continue;
		// skip until reached
		ParticleObject *p = &(*pit);
		if(p->getLifeTime()<0.){ 
			if(p->getLifeTime() < -mSimulationTime) continue; 
			else p->setLifeTime(-mLevel[level].timestep); // zero for following update
		}
    int i,j,k;
		p->setLifeTime(p->getLifeTime()+mLevel[level].timestep);

		// nearest neighbor, particle positions don't include empty bounds
		ntlVec3Gfx pos = p->getPos();
		i= (int)pos[0]; j= (int)pos[1]; k= (int)pos[2];// no offset necessary
		if(LBMDIM==2) { k = 0; }

		// only testdata handling, all for sws
#if LBM_INCLUDE_TESTSOLVERS==1
		if(useff && (mpTest->mFarfMode>0)) {
			p->setStatus(PART_OUT);
			mpTest->handleParticle(p, i,j,k); continue;
		} 
#endif // LBM_INCLUDE_TESTSOLVERS==1

		// in out tests
		if(p->getStatus()&PART_IN) { // IN
			if( (i<cutval)||(i>mSizex-1-cutval)||
					(j<cutval)||(j>mSizey-1-cutval)
					//||(k<cutval)||(k>mSizez-1-cutval) 
					) {
				if(!useff) { DEL_PART;
				} else { 
					p->setStatus(PART_OUT); 
				}
			} 
		} else { // OUT rough check
			// check in again?
			if( (i>=cutval)&&(i<=mSizex-1-cutval)&&
					(j>=cutval)&&(j<=mSizey-1-cutval)
					) {
				p->setStatus(PART_IN);
			}
		}

		if( (p->getType()==PART_BUBBLE) ||
		    (p->getType()==PART_TRACER) ) {

			// no interpol
			vx = vy = vz = 0.0;
			if(p->getStatus()&PART_IN) { // IN
				if(k>=cutval) {
					if(k>mSizez-1-cutval) DEL_PART; 

					if( RFLAG(level, i,j,k, workSet)&(CFFluid|CFUnused) ) {
						// still ok
						int partLev = level;
						int si=i, sj=j, sk=k;
						while(partLev>0 && RFLAG(partLev, si,sj,sk, workSet)&(CFUnused)) {
							partLev--;
							si/=2;
							sj/=2;
							sk/=2;
						}
						// get velocity from fluid cell
						if( RFLAG(partLev, si,sj,sk, workSet)&(CFFluid) ) {
							LbmFloat *ccel = RACPNT(partLev, si,sj,sk, workSet);
							FORDF1{
								LbmFloat cdf = RAC(ccel, l);
								// TODO update below
								vx  += (this->dfDvecX[l]*cdf); 
								vy  += (this->dfDvecY[l]*cdf);  
								vz  += (this->dfDvecZ[l]*cdf);  
							}
							// remove gravity influence
							const LbmFloat lesomega = mLevel[level].omega; // no les
							vx -= mLevel[level].gravity[0] * lesomega*0.5;
							vy -= mLevel[level].gravity[1] * lesomega*0.5;
							vz -= mLevel[level].gravity[2] * lesomega*0.5;
						} // fluid vel

					} else { // OUT
						// out of bounds, deactivate...
						// FIXME make fsgr treatment
						if(p->getType()==PART_BUBBLE) { P_CHANGETYPE(p, PART_FLOAT ); continue; }
					}
				} else {
					// below 3d region, just rise
				}
			} else { // OUT
#				if LBM_INCLUDE_TESTSOLVERS==1
				if(useff) { mpTest->handleParticle(p, i,j,k); }
				else DEL_PART;
#				else // LBM_INCLUDE_TESTSOLVERS==1
				DEL_PART;
#				endif // LBM_INCLUDE_TESTSOLVERS==1
				// TODO use x,y vel...?
			}

			ntlVec3Gfx v = p->getVel(); // dampen...
			if( (useff)&& (p->getType()==PART_BUBBLE) ) {
				// test rise

				if(mPartUsePhysModel) {
					LbmFloat radius = p->getSize() * minDropSize;
					LbmVec   velPart = vec2L(p->getVel()) *cellsize/timestep; // L2RW, lattice velocity
					LbmVec   velWater = LbmVec(vx,vy,vz) *cellsize/timestep;// L2RW, fluid velocity
					LbmVec   velRel = velWater - velPart;
					//LbmFloat velRelNorm = norm(velRel);
					LbmFloat pvolume = rhoAir * 4.0/3.0 * M_PI* radius*radius*radius; // volume: 4/3 pi r^3

					LbmVec fb = -rwgrav* pvolume *rhoWater;
					LbmVec fd = velRel*6.0*M_PI*radius* (1e-3); //viscWater;
					LbmVec change = (fb+fd) *10.0*timestep  *(timestep/cellsize);
					/*if(debugOutCounter<0) {
						errMsg("PIT","BTEST1   vol="<<pvolume<<" radius="<<radius<<" vn="<<velRelNorm<<" velPart="<<velPart<<" velRel"<<velRel);
						errMsg("PIT","BTEST2        cellsize="<<cellsize<<" timestep="<<timestep<<" viscW="<<viscWater<<" ss/mb="<<(timestep/(pvolume*rhoAir)));
						errMsg("PIT","BTEST2        grav="<<rwgrav<<"  " );
						errMsg("PIT","BTEST2        change="<<(change)<<" fb="<<(fb)<<" fd="<<(fd)<<" ");
						errMsg("PIT","BTEST2        change="<<norm(change)<<" fb="<<norm(fb)<<" fd="<<norm(fd)<<" ");
					} // DEBUG */
						
					LbmVec fd2 = (LbmVec(vx,vy,vz)-vec2L(p->getVel())) * 6.0*M_PI*radius* (1e-3); //viscWater;
					LbmFloat w = 0.99;
					vz = (1.0-w)*vz + w*(p->getVel()[2]-0.5*(p->getSize()/5.0)*mLevel[level].gravity[2]);
					v = ntlVec3Gfx(vx,vy,vz)+vec2G(fd2);
					p->setVel( v );
				} else {
					// non phys, half old, half fluid, use slightly slower acc
					v = v*0.5 + ntlVec3Gfx(vx,vy,vz)* 0.5-vec2G(mLevel[level].gravity)*0.5;
					p->setVel( v * 0.99 );
				}
				p->advanceVel();

			} else if(p->getType()==PART_TRACER) {
				v = ntlVec3Gfx(vx,vy,vz);
				CellFlagType fflag = RFLAG(level, i,j,k, workSet);

				if(fflag&(CFFluid|CFInter) ) { p->setInFluid(true);
				} else { p->setInFluid(false); }

				if( (( fflag&CFFluid ) && ( fflag&CFNoBndFluid )) ||
						(( fflag&CFInter ) && (!(fflag&CFNoNbFluid)) ) ) {
					// only real fluid
#					if LBMDIM==3
					p->advance( TRACE_RAND,TRACE_RAND,TRACE_RAND);
#					else
					p->advance( TRACE_RAND,TRACE_RAND, 0.);
#					endif

				} else {
					// move inwards along normal, make sure normal is valid first
					// todo use class funcs!
					const int lev = level;
					LbmFloat nx=0.,ny=0.,nz=0., nv1,nv2;
					bool nonorm = false;
					if(i<=0)              { nx = -1.; nonorm = true; }
					if(i>=mSizex-1) { nx =  1.; nonorm = true; }
					if(j<=0)              { ny = -1.; nonorm = true; }
					if(j>=mSizey-1) { ny =  1.; nonorm = true; }
#					if LBMDIM==3
					if(k<=0)              { nz = -1.; nonorm = true; }
					if(k>=mSizez-1) { nz =  1.; nonorm = true; }
#					endif // LBMDIM==3
					if(!nonorm) {
						FFGET_NORM(nv1,dE); FFGET_NORM(nv2,dW);
						nx = 0.5* (nv2-nv1);
						FFGET_NORM(nv1,dN); FFGET_NORM(nv2,dS);
						ny = 0.5* (nv2-nv1);
#						if LBMDIM==3
						FFGET_NORM(nv1,dT); FFGET_NORM(nv2,dB);
						nz = 0.5* (nv2-nv1);
#						else // LBMDIM==3
						nz = 0.;
#						endif // LBMDIM==3
					} else {
						v = p->getVel() + vec2G(mLevel[level].gravity);
					}
					p->advanceVec( (ntlVec3Gfx(nx,ny,nz)) * -0.1 ); // + vec2G(mLevel[level].gravity);
				}
			}

			p->setVel( v );
			p->advanceVel();
		} 

		// drop handling
		else if(p->getType()==PART_DROP) {
			ntlVec3Gfx v = p->getVel(); // dampen...

			if(mPartUsePhysModel) {
				LbmFloat radius = p->getSize() * minDropSize;
				LbmVec   velPart = vec2L(p->getVel()) *cellsize /timestep; // * cellsize / timestep; // L2RW, lattice velocity
				LbmVec   velRel = velAir - velPart;
				//LbmVec   velRelLat = velRel /cellsize*timestep; // L2RW
				LbmFloat velRelNorm = norm(velRel);
				// TODO calculate values in lattice units, compute CD?!??!
				LbmFloat mb = rhoWater * 4.0/3.0 * M_PI* radius*radius*radius; // mass: 4/3 pi r^3 rho
				const LbmFloat rw = (r1-radius)/(r1-r2);
				const LbmFloat rmax = (0.5 + 0.5*rw);
				const LbmFloat vmax = (v2 + (v1-v2)* (1.0-rw) );
				const LbmFloat cd = (rmax) * (velRelNorm)/(vmax);

				LbmVec fg = rwgrav * mb;//  * (1.0-rhoAir/rhoWater);
				LbmVec fd = velRel* velRelNorm* cd*M_PI *rhoAir *0.5 *radius*radius;
				LbmVec change = (fg+   fd ) *timestep / mb  *(timestep/cellsize);
				//if(k>0) { errMsg("\nPIT","NTEST1   mb="<<mb<<" radius="<<radius<<" vn="<<velRelNorm<<" velPart="<<velPart<<" velRel"<<velRel<<" pgetVel="<<p->getVel() ); }

				v += vec2G(change);
				p->setVel(v); 
				// NEW
			} else {
				p->setVel( v ); 
				int gravk = (int)(p->getPos()[2]+mLevel[level].gravity[2]);
				if(gravk>=0 && gravk<mSizez && RFLAG(level, i,j,gravk, workSet)&CFBnd) {
					// dont add for "resting" parts
					v[2] = 0.;
					p->setVel( v*0.9 ); // restdamping
				} else {
					p->addToVel( vec2G(mLevel[level].gravity) );
				}
			} // OLD
			p->advanceVel();

			if(p->getStatus()&PART_IN) { // IN
				if(k<cutval) { DEL_PART; continue; }
				if(k<=mSizez-1-cutval){ 
					CellFlagType pflag = RFLAG(level, i,j,k, workSet);
					//errMsg("PIT move"," at "<<PRINT_IJK<<" flag"<<convertCellFlagType2String(pflag) );
					if(pflag & (CFBnd)) {
						handleObstacleParticle(p);
						continue;
					} else if(pflag & (CFEmpty)) {
						// still ok
					} else if((pflag & CFInter) 
					          //&&(!(RFLAG(level, i,j,k, workSet)& CFNoNbFluid)) 
										) {
						// add to no nb fluid i.f.'s, so skip if interface with fluid nb
					} else if(pflag  & (CFFluid|CFUnused|CFInter) ){ // interface cells ignored here due to previous check!
						// add dropmass again, (these are only interf. with nonbfl.)
						int oi= (int)(pos[0]-1.25*v[0]+0.5);
						int oj= (int)(pos[1]-1.25*v[1]+0.5);
						int ok= (int)(pos[2]-1.25*v[2]+0.5);
						const LbmFloat size = p->getSize();
						const LbmFloat dropmass = ParticleObject::getMass(mPartDropMassSub*size);
						bool orgcellok = false;
						if( (oi<0)||(oi>mSizex-1)||
						    (oj<0)||(oj>mSizey-1)||
						    (ok<0)||(ok>mSizez-1) ) {
							// org cell not ok!
						} else if( RFLAG(level, oi,oj,ok, workSet) & (CFInter) ){
							orgcellok = true;
						} else {
							// search upward for interface
							oi=i; oj=j; ok=k;
							for(int kk=0; kk<5 && ok<=mSizez-2; kk++) {
								ok++; // check sizez-2 due to this increment!
								if( RFLAG(level, oi,oj,ok, workSet) & (CFInter) ){
									kk = 5; orgcellok = true;
								}
							}
						}

						//errMsg("PTIMPULSE"," new v"<<v<<" at "<<PRINT_VEC(oi,oj,ok)<<" , was "<<PRINT_VEC(i,j,k)<<" ok "<<orgcellok );
						if(orgcellok) {
							QCELL(level, oi,oj,ok, workSet, dMass) += dropmass;
							QCELL(level, oi,oj,ok, workSet, dFfrac) += dropmass; // assume rho=1?

							if(RFLAG(level, oi,oj,ok, workSet) & CFNoBndFluid){
							// check speed, perhaps normalize
							gfxReal vlensqr = normNoSqrt(v);
							if(vlensqr > 0.166*0.166) {
								v *= 1./sqrtf((float)vlensqr)*0.166;
							}
							// compute cell velocity
							LbmFloat *tcel = RACPNT(level, oi,oj,ok, workSet);
							LbmFloat velUx=0., velUy=0., velUz=0.;
							FORDF0 { 
								velUx  += (this->dfDvecX[l]*RAC(tcel,l));
								velUy  += (this->dfDvecY[l]*RAC(tcel,l)); 
								velUz  += (this->dfDvecZ[l]*RAC(tcel,l)); 
							}
							// add impulse
							/*
							LbmFloat cellVelSqr = velUx*velUx+ velUy*velUy+ velUz*velUz;
							//errMsg("PTIMPULSE"," new v"<<v<<" cvs"<<cellVelSqr<<"="<<sqrt(cellVelSqr));
							if(cellVelSqr< 0.166*0.166) {
								FORDF1 { 
									const LbmFloat add = 3. * dropmass * this->dfLength[l]*(v[0]*this->dfDvecX[l]+v[1]*this->dfDvecY[l]+v[2]*this->dfDvecZ[l]);
									RAC(tcel,l) += add;
								} } // */
							} // only add impulse away from obstacles! 
						} // orgcellok

						// FIXME make fsgr treatment
						P_CHANGETYPE(p, PART_FLOAT ); continue; 
						// jitter in cell to prevent stacking when hitting a steep surface
						ntlVec3Gfx cpos = p->getPos(); 
						cpos[0] += (rand()/(RAND_MAX+1.0))-0.5;
						cpos[1] += (rand()/(RAND_MAX+1.0))-0.5; 
						cpos[2] += (rand()/(RAND_MAX+1.0))-0.5; 
						p->setPos(cpos);
					} else {
						DEL_PART;
						this->mNumParticlesLost++;
					}
				}
			} else { // OUT
#				if LBM_INCLUDE_TESTSOLVERS==1
				if(useff) { mpTest->handleParticle(p, i,j,k); }
				else{ DEL_PART; }
#				else // LBM_INCLUDE_TESTSOLVERS==1
				DEL_PART; 
#				endif // LBM_INCLUDE_TESTSOLVERS==1
			}

		} // air particle

		// inter particle
		else if(p->getType()==PART_INTER) {
			// unused!?
			if(p->getStatus()&PART_IN) { // IN
				if((k<cutval)||(k>mSizez-1-cutval)) {
					// undecided particle above or below... remove?
					DEL_PART; 
				}

				CellFlagType pflag = RFLAG(level, i,j,k, workSet);
				if(pflag& CFInter ) {
					// still ok
				} else if(pflag& (CFFluid|CFUnused) ) {
					P_CHANGETYPE(p, PART_FLOAT ); continue;
				} else if(pflag& CFEmpty ) {
					P_CHANGETYPE(p, PART_DROP ); continue;
				} else if(pflag& CFBnd ) {
					P_CHANGETYPE(p, PART_FLOAT ); continue;
				}
			} else { // OUT
				// undecided particle outside... remove?
				DEL_PART; 
			}
		}

		// float particle
		else if(p->getType()==PART_FLOAT) {

			if(p->getStatus()&PART_IN) { // IN
				if(k<cutval) DEL_PART; 
				// not valid for mass... 
				vx = vy = vz = 0.0;

				// define from particletracer.h
#if MOVE_FLOATS==1
				const int DEPTH_AVG=3; // only average interface vels
				int ccnt=0;
				for(int kk=0;kk<DEPTH_AVG;kk+=1) {
					if((k-kk)<1) continue;
					if(RFLAG(level, i,j,k, workSet)&(CFInter)) {} else continue;
					ccnt++;
					FORDF1{
						LbmFloat cdf = QCELL(level, i,j,k-kk, workSet, l);
						vx  += (this->dfDvecX[l]*cdf); 
						vy  += (this->dfDvecY[l]*cdf);  
						vz  += (this->dfDvecZ[l]*cdf);  
					}
				}
				if(ccnt) {
				// use halved surface velocity (todo, use omega instead)
				vx /=(LbmFloat)(ccnt * 2.0); // half xy speed! value2
				vy /=(LbmFloat)(ccnt * 2.0);
				vz /=(LbmFloat)(ccnt); }
#else // MOVE_FLOATS==1
				vx=vy=0.; //p->setVel(ntlVec3Gfx(0.) ); // static_float
#endif // MOVE_FLOATS==1
				vx += (rand()/(RAND_MAX+1.0))*(FLOAT_JITTER*0.2)-(FLOAT_JITTER*0.2*0.5);
				vy += (rand()/(RAND_MAX+1.0))*(FLOAT_JITTER*0.2)-(FLOAT_JITTER*0.2*0.5);

				//bool delfloat = false;
				if( ( RFLAG(level, i,j,k, workSet)& (CFFluid|CFUnused) ) ) {
					// in fluid cell
					vz = p->getVel()[2]-1.0*mLevel[level].gravity[2]; // simply rise...
					if(vz<0.) vz=0.;
				} else if( ( RFLAG(level, i,j,k, workSet)& CFBnd ) ) {
					// force downwards movement, move below obstacle...
					//vz = p->getVel()[2]+1.0*mLevel[level].gravity[2]; // fall...
					//if(vz>0.) vz=0.;
					DEL_PART; 
				} else if( ( RFLAG(level, i,j,k, workSet)& CFInter ) ) {
					// keep in interface , one grid cell offset is added in part. gen
				} else { // all else...
					if( ( RFLAG(level, i,j,k-1, workSet)& (CFFluid|CFInter) ) ) {
						vz = p->getVel()[2]+2.0*mLevel[level].gravity[2]; // fall...
						if(vz>0.) vz=0.; }
					else { DEL_PART; }
				}

				p->setVel( vec2G( ntlVec3Gfx(vx,vy,vz) ) ); //?
				p->advanceVel();
			} else {
#if LBM_INCLUDE_TESTSOLVERS==1
				if(useff) { mpTest->handleParticle(p, i,j,k); }
				else DEL_PART; 
#else // LBM_INCLUDE_TESTSOLVERS==1
				DEL_PART; 
#endif // LBM_INCLUDE_TESTSOLVERS==1
			}
				
			// additional bnd jitter
			if((0) && (useff) && (p->getLifeTime()<3.*mLevel[level].timestep)) {
				// use half butoff border 1/8
				int maxdw = (int)(mLevel[level].lSizex*0.125*0.5);
				if(maxdw<3) maxdw=3;
				if((j>=0)&&(j<=mSizey-1)) {
					if(ABS(i-(               cutval))<maxdw) { p->advance(  FLOAT_JITTBNDRAND( ABS(i-(               cutval))), 0.,0.); }
					if(ABS(i-(mSizex-1-cutval))<maxdw) { p->advance(  FLOAT_JITTBNDRAND( ABS(i-(mSizex-1-cutval))), 0.,0.); }
				}
			}
		}  // PART_FLOAT
		
		// unknown particle type	
		else {
			errMsg("LbmFsgrSolver::advanceParticles","PIT pit invalid type!? "<<p->getStatus() );
		}
  }
	myTime_t parttend = getTime(); 
	debMsgStd("LbmFsgrSolver::advanceParticles",DM_MSG,"Time for particle update:"<< getTimeString(parttend-parttstart)<<", #particles:"<<mpParticles->getNumParticles() , 10 );
}

void LbmFsgrSolver::notifySolverOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename) {
	int workSet = mLevel[mMaxRefine].setCurr;
	std::ostringstream name;

	// debug - raw dump of ffrac values, as text!
	if(mDumpRawText) { 
		name << outfilename<< frameNrStr <<".dump";
		FILE *file = fopen(name.str().c_str(),"w");
		if(file) {

			for(int k= getForZMinBnd(); k< getForZMaxBnd(mMaxRefine); ++k)  {
				for(int j=0;j<mLevel[mMaxRefine].lSizey-0;j++)  {
					for(int i=0;i<mLevel[mMaxRefine].lSizex-0;i++) {
						float val = 0.;
						if(RFLAG(mMaxRefine, i,j,k, workSet) & CFInter) {
							val = QCELL(mMaxRefine,i,j,k, mLevel[mMaxRefine].setCurr,dFfrac);
							if(val<0.) val=0.;
							if(val>1.) val=1.;
						}
						if(RFLAG(mMaxRefine, i,j,k, workSet) & CFFluid) val = 1.;
						fprintf(file, "%f ",val); // text
						//errMsg("W", PRINT_IJK<<" val:"<<val);
					}
					fprintf(file, "\n"); // text
				}
				fprintf(file, "\n"); // text
			}
			fclose(file);

		} // file
	} // */

	if(mDumpRawBinary) {
		if(!mDumpRawBinaryZip) {
			// unzipped, only fill
			name << outfilename<< frameNrStr <<".bdump";
			FILE *file = fopen(name.str().c_str(),"w");
			if(file) {
				for(int k= getForZMinBnd(); k< getForZMaxBnd(mMaxRefine); ++k)  {
					for(int j=0;j<mLevel[mMaxRefine].lSizey-0;j++)  {
						for(int i=0;i<mLevel[mMaxRefine].lSizex-0;i++) {
							float val = 0.;
							if(RFLAG(mMaxRefine, i,j,k, workSet) & CFInter) {
								val = QCELL(mMaxRefine,i,j,k, mLevel[mMaxRefine].setCurr,dFfrac);
								if(val<0.) val=0.;
								if(val>1.) val=1.;
							}
							if(RFLAG(mMaxRefine, i,j,k, workSet) & CFFluid) val = 1.;
							fwrite( &val, sizeof(val), 1, file); // binary
						}
					}
				}
				fclose(file);
			} // file
		} // unzipped
		else {
			// zipped, use iso values
			prepareVisualization();
			name << outfilename<< frameNrStr <<".bdump.gz";
			gzFile gzf = gzopen(name.str().c_str(),"wb9");
			if(gzf) {
				// write size
				int s;
				s=mSizex;	gzwrite(gzf, &s, sizeof(s));
				s=mSizey;	gzwrite(gzf, &s, sizeof(s));
				s=mSizez;	gzwrite(gzf, &s, sizeof(s));

				// write isovalues
				for(int k= getForZMinBnd(); k< getForZMaxBnd(mMaxRefine); ++k)  {
					for(int j=0;j<mLevel[mMaxRefine].lSizey;j++)  {
						for(int i=0;i<mLevel[mMaxRefine].lSizex;i++) {
							float val = 0.;
							val = *mpIso->lbmGetData( i,j,k );
							gzwrite(gzf, &val, sizeof(val));
						}
					}
				}
				gzclose(gzf);
			} // gzf
		} // zip
	} // bin dump

	dumptype = 0; frameNr = 0; // get rid of warning
}

/*! move a particle at a boundary */
void LbmFsgrSolver::handleObstacleParticle(ParticleObject *p) {
	//if(normNoSqrt(v)<=0.) continue; // skip stuck
	/*
		 p->setVel( v * -1. ); // revert
		 p->advanceVel(); // move back twice...
		 if( RFLAG(mMaxRefine, i,j,k, workSet)& (CFBndNoslip)) {
		 p->setVel( v * -0.5 ); // revert & dampen
		 }
		 p->advanceVel();  
	// */
	// TODO mark/remove stuck parts!?

	const int level = mMaxRefine;
	const int workSet = mLevel[level].setCurr;
	LbmVec v = vec2L( p->getVel() );
	if(normNoSqrt(v)<=0.) { 
		p->setVel(vec2G(mLevel[level].gravity)); 
	}

	CellFlagType pflag = CFBnd;
	ntlVec3Gfx posOrg(p->getPos());
	ntlVec3Gfx npos(0.);
	int ni=1,nj=1,nk=1;
	int tries = 0;

	// try to undo movement
	p->advanceVec( (p->getVel()-vec2G(mLevel[level].gravity)) * -2.);  

	npos = p->getPos(); ni= (int)npos[0]; 
	nj= (int)npos[1]; nk= (int)npos[2];
	if(LBMDIM==2) { nk = 0; }
	//errMsg("BOUNDCPAR"," t"<<PRINT_VEC(ni,nj,nk)<<" v"<<v<<" p"<<npos);

	// delete out of domain
	if(!checkDomainBounds(level,ni,nj,nk)) {
		//errMsg("BOUNDCPAR"," DEL! ");
		p->setActive( false ); 
		return;
	}
	pflag =  RFLAG(level, ni,nj,nk, workSet);
	
	// try to force particle out of boundary
	bool haveNorm = false;
	LbmVec bnormal;
	if(pflag&CFBnd) {
		npos = posOrg; ni= (int)npos[0]; 
		nj= (int)npos[1]; nk= (int)npos[2];
		if(LBMDIM==2) { nk = 0; }

		computeObstacleSurfaceNormalAcc(ni,nj,nk, &bnormal[0]);
		haveNorm = true;
		normalize(bnormal);
		bnormal *= 0.25;

		tries = 1;
		while(pflag&CFBnd && tries<=5) {
			// use increasing step sizes
			p->advanceVec( vec2G( bnormal *0.5 *(gfxReal)tries ) );  
			npos = p->getPos();
			ni= (int)npos[0]; 
			nj= (int)npos[1]; 
			nk= (int)npos[2];

			// delete out of domain
			if(!checkDomainBounds(level,ni,nj,nk)) {
				//errMsg("BOUNDCPAR"," DEL! ");
				p->setActive( false ); 
				return;
			}
			pflag =  RFLAG(level, ni,nj,nk, workSet);
			tries++;
		}

		// really stuck, delete...
		if(pflag&CFBnd) {
			p->setActive( false ); 
			return;
		}
	}

	// not in bound anymore!
	if(!haveNorm) {
		CellFlagType *bflag = &RFLAG(level, ni,nj,nk, workSet);
		LbmFloat     *bcell = RACPNT(level, ni,nj,nk, workSet);
		computeObstacleSurfaceNormal(bcell,bflag, &bnormal[0]);
	}
	normalize(bnormal);
	LbmVec normComp = bnormal * dot(vec2L(v),bnormal);
	//errMsg("BOUNDCPAR","bnormal"<<bnormal<<" normComp"<<normComp<<" newv"<<(v-normComp) );
	v = (v-normComp)*0.9; // only move tangential
	v *= 0.9; // restdamping , todo use timestep
	p->setVel(vec2G(v));
	p->advanceVel();
}

/*****************************************************************************/
/*! internal quick print function (for debugging) */
/*****************************************************************************/
void 
LbmFsgrSolver::printLbmCell(int level, int i, int j, int k, int set) {
	stdCellId *newcid = new stdCellId;
	newcid->level = level;
	newcid->x = i;
	newcid->y = j;
	newcid->z = k;

	// this function is not called upon clicking, then its from setMouseClick
	debugPrintNodeInfo( newcid, set );
	delete newcid;
}
void 
LbmFsgrSolver::debugMarkCellCall(int level, int vi,int vj,int vk) {
	stdCellId *newcid = new stdCellId;
	newcid->level = level;
	newcid->x = vi;
	newcid->y = vj;
	newcid->z = vk;
	this->addCellToMarkedList( newcid );
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

CellIdentifierInterface* 
LbmFsgrSolver::getFirstCell( ) {
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

LbmFsgrSolver::stdCellId* 
LbmFsgrSolver::convertBaseCidToStdCid( CellIdentifierInterface* basecid) {
	//stdCellId *cid = dynamic_cast<stdCellId*>( basecid );
	stdCellId *cid = (stdCellId*)( basecid );
	return cid;
}

void LbmFsgrSolver::advanceCell( CellIdentifierInterface* basecid) {
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

bool LbmFsgrSolver::noEndCell( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return (!cid->getEnd());
}

void LbmFsgrSolver::deleteCellIterator( CellIdentifierInterface** cid ) {
	delete *cid;
	*cid = NULL;
}

CellIdentifierInterface* LbmFsgrSolver::getCellAt( ntlVec3Gfx pos ) {
	//int cellok = false;
	pos -= (this->mvGeoStart);

	LbmFloat mmaxsize = mLevel[mMaxRefine].nodeSize;
	for(int level=mMaxRefine; level>=0; level--) { // finest first
	//for(int level=0; level<=mMaxRefine; level++) { // coarsest first
		LbmFloat nsize = mLevel[level].nodeSize;
		int x,y,z;
		// CHECK +- maxsize?
		x = (int)((pos[0]+0.5*mmaxsize) / nsize );
		y = (int)((pos[1]+0.5*mmaxsize) / nsize );
		z = (int)((pos[2]+0.5*mmaxsize) / nsize );
		if(LBMDIM==2) z = 0;

		// double check...
		if(x<0) continue;
		if(y<0) continue;
		if(z<0) continue;
		if(x>=mLevel[level].lSizex) continue;
		if(y>=mLevel[level].lSizey) continue;
		if(z>=mLevel[level].lSizez) continue;

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
		//errMsg("cellAt",this->mName<<" "<<pos<<" l"<<level<<":"<<x<<","<<y<<","<<z<<" "<<convertCellFlagType2String(RFLAG(level, x,y,z, mLevel[level].setCurr)) );
		return newcid;
	}

	return NULL;
}


// INFO functions

int      LbmFsgrSolver::getCellSet      ( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return mLevel[cid->level].setCurr;
	//return mLevel[cid->level].setOther;
}

int      LbmFsgrSolver::getCellLevel    ( CellIdentifierInterface* basecid) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return cid->level;
}

ntlVec3Gfx   LbmFsgrSolver::getCellOrigin   ( CellIdentifierInterface* basecid) {
	ntlVec3Gfx ret;

	stdCellId *cid = convertBaseCidToStdCid(basecid);
	ntlVec3Gfx cs( mLevel[cid->level].nodeSize );
	if(LBMDIM==2) { cs[2] = 0.0; }

	if(LBMDIM==2) {
		ret =(this->mvGeoStart + ntlVec3Gfx( cid->x *cs[0], cid->y *cs[1], (this->mvGeoEnd[2]-this->mvGeoStart[2])*0.5 )
				+ ntlVec3Gfx(0.0,0.0,cs[1]*-0.25)*cid->level )
			+getCellSize(basecid);
	} else {
		ret =(this->mvGeoStart + ntlVec3Gfx( cid->x *cs[0], cid->y *cs[1], cid->z *cs[2] ))
			+getCellSize(basecid);
	}
	return (ret);
}

ntlVec3Gfx   LbmFsgrSolver::getCellSize     ( CellIdentifierInterface* basecid) {
	// return half size
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	ntlVec3Gfx retvec( mLevel[cid->level].nodeSize * 0.5 );
	// 2d display as rectangles
	if(LBMDIM==2) { retvec[2] = 0.0; }
	return (retvec);
}

LbmFloat LbmFsgrSolver::getCellDensity  ( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);

	// skip non-fluid cells
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&(CFFluid|CFInter)) {
		// ok go on...
	} else {
		return 0.;
	}

	LbmFloat rho = 0.0;
	FORDF0 { rho += QCELL(cid->level, cid->x,cid->y,cid->z, set, l); } // ORG
	return ((rho-1.0) * mLevel[cid->level].simCellSize / mLevel[cid->level].timestep) +1.0; // ORG
	/*if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&CFInter) { // test
		LbmFloat ux,uy,uz;
		ux=uy=uz= 0.0;
		int lev = cid->level;
		LbmFloat df[27], feqOld[27];
		FORDF0 {
			rho += QCELL(lev, cid->x,cid->y,cid->z, set, l);
			ux += this->dfDvecX[l]* QCELL(lev, cid->x,cid->y,cid->z, set, l);
			uy += this->dfDvecY[l]* QCELL(lev, cid->x,cid->y,cid->z, set, l);
			uz += this->dfDvecZ[l]* QCELL(lev, cid->x,cid->y,cid->z, set, l);
			df[l] = QCELL(lev, cid->x,cid->y,cid->z, set, l);
		}
		FORDF0 {
			feqOld[l] = getCollideEq(l, rho,ux,uy,uz); 
		}
		// debugging mods
		//const LbmFloat Qo = this->getLesNoneqTensorCoeff(df,feqOld);
		//const LbmFloat modOmega = this->getLesOmega(mLevel[lev].omega, mLevel[lev].lcsmago,Qo);
		//rho = (2.0-modOmega) *25.0;
		//rho = Qo*100.0;
		//if(cid->x==24){ errMsg("MODOMT"," at "<<PRINT_VEC(cid->x,cid->y,cid->z)<<" = "<<rho<<" "<<Qo); }
		//else{ rho=0.0; }
	} // test 
	return rho; // test */
}

LbmVec   LbmFsgrSolver::getCellVelocity ( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);

	// skip non-fluid cells
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&(CFFluid|CFInter)) {
		// ok go on...
	} else {
		return LbmVec(0.0);
	}

	LbmFloat ux,uy,uz;
	ux=uy=uz= 0.0;
	FORDF0 {
		ux += this->dfDvecX[l]* QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
		uy += this->dfDvecY[l]* QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
		uz += this->dfDvecZ[l]* QCELL(cid->level, cid->x,cid->y,cid->z, set, l);
	}
	LbmVec vel(ux,uy,uz);
	// TODO fix...
	return (vel * mLevel[cid->level].simCellSize / mLevel[cid->level].timestep * this->mDebugVelScale); // normal
}

LbmFloat   LbmFsgrSolver::getCellDf( CellIdentifierInterface* basecid,int set, int dir) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return QCELL(cid->level, cid->x,cid->y,cid->z, set, dir);
}
LbmFloat   LbmFsgrSolver::getCellMass( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return QCELL(cid->level, cid->x,cid->y,cid->z, set, dMass);
}
LbmFloat   LbmFsgrSolver::getCellFill( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&CFInter) return QCELL(cid->level, cid->x,cid->y,cid->z, set, dFfrac);
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&CFFluid) return 1.0;
	return 0.0;
	//return QCELL(cid->level, cid->x,cid->y,cid->z, set, dFfrac);
}
CellFlagType LbmFsgrSolver::getCellFlag( CellIdentifierInterface* basecid,int set) {
	stdCellId *cid = convertBaseCidToStdCid(basecid);
	return RFLAG(cid->level, cid->x,cid->y,cid->z, set);
}

LbmFloat LbmFsgrSolver::getEquilDf( int l ) {
	return this->dfEquil[l];
}


ntlVec3Gfx LbmFsgrSolver::getVelocityAt   (float xp, float yp, float zp) {
	ntlVec3Gfx avgvel(0.0);
	LbmFloat   avgnum = 0.;

	// taken from getCellAt!
	const int level = mMaxRefine;
	const int workSet = mLevel[level].setCurr;
	const LbmFloat nsize = mLevel[level].nodeSize;
	const int x = (int)((-this->mvGeoStart[0]+xp-0.5*nsize) / nsize );
	const int y = (int)((-this->mvGeoStart[1]+yp-0.5*nsize) / nsize );
	int       z = (int)((-this->mvGeoStart[2]+zp-0.5*nsize) / nsize );
	if(LBMDIM==2) z=0;
	//errMsg("DUMPVEL","p"<<PRINT_VEC(xp,yp,zp)<<" at "<<PRINT_VEC(x,y,z)<<" max"<<PRINT_VEC(mLevel[level].lSizex,mLevel[level].lSizey,mLevel[level].lSizez) );

	// return fluid/if/border cells
	// search neighborhood, do smoothing
	FORDF0{ 
		const int i = x+this->dfVecX[l];
		const int j = y+this->dfVecY[l];
		const int k = z+this->dfVecZ[l];

		if( (i<0) || (j<0) || (k<0) 
		 || (i>=mLevel[level].lSizex) 
		 || (j>=mLevel[level].lSizey) 
		 || (k>=mLevel[level].lSizez) ) continue;

		if( (RFLAG(level, i,j,k, mLevel[level].setCurr)&(CFFluid|CFInter)) ) {
			ntlVec3Gfx vel(0.0);
			LbmFloat *ccel = RACPNT(level, i,j,k ,workSet); // omp
			for(int n=1; n<this->cDfNum; n++) {
				vel[0]  += (this->dfDvecX[n]*RAC(ccel,n));
				vel[1]  += (this->dfDvecY[n]*RAC(ccel,n)); 
				vel[2]  += (this->dfDvecZ[n]*RAC(ccel,n)); 
			} 

			avgvel += vel;
			avgnum += 1.0;
			if(l==0) { // center slightly more weight
				avgvel += vel; avgnum += 1.0;
			}
		} // */
	}

	if(avgnum>0.) {
		ntlVec3Gfx retv = avgvel / avgnum;
		retv *= nsize/mLevel[level].timestep;
		// scale for current animation settings (frame time)
		retv *= mpParam->getCurrentAniFrameTime();
		//errMsg("DUMPVEL","t"<<mSimulationTime<<" at "<<PRINT_VEC(xp,yp,zp)<<" ret:"<<retv<<", avgv:"<<avgvel<<" n"<<avgnum<<" nsize"<<nsize<<" ts"<<mLevel[level].timestep<<" fr"<<mpParam->getCurrentAniFrameTime() );
		return retv;
	}
	// no cells here...?
	//errMsg("DUMPVEL"," at "<<PRINT_VEC(xp,yp,zp)<<" v"<<avgvel<<" n"<<avgnum<<" no vel !?");
	return ntlVec3Gfx(0.);
}

#if LBM_USE_GUI==1
//! show simulation info (implement SimulationObject pure virtual func)
void 
LbmFsgrSolver::debugDisplay(int set){ 
	//lbmDebugDisplay< LbmFsgrSolver >( set, this ); 
	lbmDebugDisplay( set ); 
}
#endif

/*****************************************************************************/
// strict debugging functions
/*****************************************************************************/
#if FSGR_STRICT_DEBUG==1
#define STRICT_EXIT *((int *)0)=0;

int LbmFsgrSolver::debLBMGI(int level, int ii,int ij,int ik, int is) {
	if(level <  0){ errMsg("LbmStrict::debLBMGI"," invLev- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 
	if(level >  mMaxRefine){ errMsg("LbmStrict::debLBMGI"," invLev+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 

	if((ii==-1)&&(ij==0)) {
		// special case for main loop, ok
	} else {
		if(ii<0){ errMsg("LbmStrict"," invX- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
		if(ij<0){ errMsg("LbmStrict"," invY- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
		if(ii>mLevel[level].lSizex-1){ errMsg("LbmStrict"," invX+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
		if(ij>mLevel[level].lSizey-1){ errMsg("LbmStrict"," invY+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	}
	if(ik<0){ errMsg("LbmStrict"," invZ- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik>mLevel[level].lSizez-1){ errMsg("LbmStrict"," invZ+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is<0){ errMsg("LbmStrict"," invS- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is>1){ errMsg("LbmStrict"," invS+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	return _LBMGI(level, ii,ij,ik, is);
};

CellFlagType& LbmFsgrSolver::debRFLAG(int level, int xx,int yy,int zz,int set){
	return _RFLAG(level, xx,yy,zz,set);   
};

CellFlagType& LbmFsgrSolver::debRFLAG_NB(int level, int xx,int yy,int zz,int set, int dir) {
	if(dir<0)         { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	// warning might access all spatial nbs
	if(dir>this->cDirNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _RFLAG_NB(level, xx,yy,zz,set, dir);
};

CellFlagType& LbmFsgrSolver::debRFLAG_NBINV(int level, int xx,int yy,int zz,int set, int dir) {
	if(dir<0)         { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	if(dir>this->cDirNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _RFLAG_NBINV(level, xx,yy,zz,set, dir);
};

int LbmFsgrSolver::debLBMQI(int level, int ii,int ij,int ik, int is, int l) {
	if(level <  0){ errMsg("LbmStrict::debLBMQI"," invLev- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 
	if(level >  mMaxRefine){ errMsg("LbmStrict::debLBMQI"," invLev+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; } 

	if((ii==-1)&&(ij==0)) {
		// special case for main loop, ok
	} else {
		if(ii<0){ errMsg("LbmStrict"," invX- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
		if(ij<0){ errMsg("LbmStrict"," invY- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
		if(ii>mLevel[level].lSizex-1){ errMsg("LbmStrict"," invX+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
		if(ij>mLevel[level].lSizey-1){ errMsg("LbmStrict"," invY+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	}
	if(ik<0){ errMsg("LbmStrict"," invZ- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik>mLevel[level].lSizez-1){ errMsg("LbmStrict"," invZ+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is<0){ errMsg("LbmStrict"," invS- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(is>1){ errMsg("LbmStrict"," invS+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(l<0)        { errMsg("LbmStrict"," invD- "<<" l"<<l); STRICT_EXIT; }
	if(l>this->cDfNum){  // dFfrac is an exception
		if((l != dMass) && (l != dFfrac) && (l != dFlux)){ errMsg("LbmStrict"," invD+ "<<" l"<<l); STRICT_EXIT; } }
#if COMPRESSGRIDS==1
	//if((!this->mInitDone) && (is!=mLevel[level].setCurr)){ STRICT_EXIT; } // COMPRT debug
#endif // COMPRESSGRIDS==1
	return _LBMQI(level, ii,ij,ik, is, l);
};

LbmFloat& LbmFsgrSolver::debQCELL(int level, int xx,int yy,int zz,int set,int l) {
	//errMsg("LbmStrict","debQCELL debug: l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" l"<<l<<" index"<<LBMGI(level, xx,yy,zz,set)); 
	return _QCELL(level, xx,yy,zz,set,l);
};

LbmFloat& LbmFsgrSolver::debQCELL_NB(int level, int xx,int yy,int zz,int set, int dir,int l) {
	if(dir<0)        { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	if(dir>this->cDfNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _QCELL_NB(level, xx,yy,zz,set, dir,l);
};

LbmFloat& LbmFsgrSolver::debQCELL_NBINV(int level, int xx,int yy,int zz,int set, int dir,int l) {
	if(dir<0)        { errMsg("LbmStrict"," invD- l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	if(dir>this->cDfNum){ errMsg("LbmStrict"," invD+ l"<<level<<"|"<<xx<<","<<yy<<","<<zz<<" s"<<set<<" d"<<dir); STRICT_EXIT; }
	return _QCELL_NBINV(level, xx,yy,zz,set, dir,l);
};

LbmFloat* LbmFsgrSolver::debRACPNT(int level,  int ii,int ij,int ik, int is ) {
	return _RACPNT(level, ii,ij,ik, is );
};

LbmFloat& LbmFsgrSolver::debRAC(LbmFloat* s,int l) {
	if(l<0)        { errMsg("LbmStrict"," invD- "<<" l"<<l); STRICT_EXIT; }
	if(l>dTotalNum){ errMsg("LbmStrict"," invD+ "<<" l"<<l); STRICT_EXIT; } 
	//if(l>this->cDfNum){ // dFfrac is an exception 
	//if((l != dMass) && (l != dFfrac) && (l != dFlux)){ errMsg("LbmStrict"," invD+ "<<" l"<<l); STRICT_EXIT; } }
	return _RAC(s,l);
};

#endif // FSGR_STRICT_DEBUG==1


/******************************************************************************
 * GUI&debugging functions
 *****************************************************************************/


#if LBM_USE_GUI==1
#define USE_GLUTILITIES
#include "../gui/gui_utilities.h"

//! display a single node
void LbmFsgrSolver::debugDisplayNode(int dispset, CellIdentifierInterface* cell ) {
	//debugOut(" DD: "<<cell->getAsString() , 10);
	ntlVec3Gfx org      = this->getCellOrigin( cell );
	ntlVec3Gfx halfsize = this->getCellSize( cell );
	int    set      = this->getCellSet( cell );
	//debugOut(" DD: "<<cell->getAsString()<<" "<< (dispset->type) , 10);

	bool     showcell = true;
	int      linewidth = 1;
	ntlColor col(0.5);
	LbmFloat cscale = 1.0; //dispset->scale;

#define DRAWDISPCUBE(col,scale) \
	{	glLineWidth( linewidth ); \
	  glColor3f( (col)[0], (col)[1], (col)[2]); \
		ntlVec3Gfx s = org-(halfsize * (scale)); \
		ntlVec3Gfx e = org+(halfsize * (scale)); \
		drawCubeWire( s,e ); }

	CellFlagType flag = this->getCellFlag(cell, set );
	// always check types
	if(flag& CFInvalid  ) { if(!guiShowInvalid  ) return; }
	if(flag& CFUnused   ) { if(!guiShowInvalid  ) return; }
	if(flag& CFEmpty    ) { if(!guiShowEmpty    ) return; }
	if(flag& CFInter    ) { if(!guiShowInterface) return; }
	if(flag& CFNoDelete ) { if(!guiShowNoDelete ) return; }
	if(flag& CFBnd      ) { if(!guiShowBnd      ) return; }

	// only dismiss one of these types 
	if(flag& CFGrFromCoarse)  { if(!guiShowCoarseInner  ) return; } // inner not really interesting
	else
	if(flag& CFGrFromFine) { if(!guiShowCoarseBorder ) return; }
	else
	if(flag& CFFluid    )    { if(!guiShowFluid    ) return; }

	switch(dispset) {
		case FLUIDDISPNothing: {
				showcell = false;
			} break;
		case FLUIDDISPCelltypes: {
				cscale = 0.5;

				if(flag& CFNoDelete) { // debug, mark nodel cells
					ntlColor ccol(0.7,0.0,0.0);
					DRAWDISPCUBE(ccol, 0.1);
				}
				if(flag& CFPersistMask) { // mark persistent flags
					ntlColor ccol(0.5);
					DRAWDISPCUBE(ccol, 0.125);
				}
				if(flag& CFNoBndFluid) { // mark persistent flags
					ntlColor ccol(0,0,1);
					DRAWDISPCUBE(ccol, 0.075);
				}

				if(flag& CFInvalid) {
					cscale = 0.50;
					col = ntlColor(0.0,0,0.0);
				}
				else if(flag& CFBnd) {
					cscale = 0.59;
					col = ntlColor(0.4);
				}

				else if(flag& CFInter) {
					cscale = 0.55;
					col = ntlColor(0,1,1);

				} else if(flag& CFGrFromCoarse) {
					// draw as - with marker
					ntlColor col2(0.0,1.0,0.3);
					DRAWDISPCUBE(col2, 0.1);
					cscale = 0.5;
					showcell=false; // DEBUG
				}
				else if(flag& CFFluid) {
					cscale = 0.5;
					if(flag& CFGrToFine) {
						ntlColor col2(0.5,0.0,0.5);
						DRAWDISPCUBE(col2, 0.1);
						col = ntlColor(0,0,1);
					}
					if(flag& CFGrFromFine) {
						ntlColor col2(1.0,1.0,0.0);
						DRAWDISPCUBE(col2, 0.1);
						col = ntlColor(0,0,1);
					} else if(flag& CFGrFromCoarse) {
						// draw as fluid with marker
						ntlColor col2(0.0,1.0,0.3);
						DRAWDISPCUBE(col2, 0.1);
						col = ntlColor(0,0,1);
					} else {
						col = ntlColor(0,0,1);
					}
				}
				else if(flag& CFEmpty) {
					showcell=false;
				}

			} break;
		case FLUIDDISPVelocities: {
				// dont use cube display
				LbmVec vel = this->getCellVelocity( cell, set );
				glBegin(GL_LINES);
				glColor3f( 0.0,0.0,0.0 );
				glVertex3f( org[0], org[1], org[2] );
				org += vec2G(vel * 10.0 * cscale);
				glColor3f( 1.0,1.0,1.0 );
				glVertex3f( org[0], org[1], org[2] );
				glEnd();
				showcell = false;
			} break;
		case FLUIDDISPCellfills: {
				cscale = 0.5;
				if(flag& CFFluid) {
					cscale = 0.75;
					col = ntlColor(0,0,0.5);
				}
				else if(flag& CFInter) {
					cscale = 0.75 * this->getCellMass(cell,set);
					col = ntlColor(0,1,1);
				}
				else {
					showcell=false;
				}

					if( ABS(this->getCellMass(cell,set)) < 10.0 ) {
						cscale = 0.75 * this->getCellMass(cell,set);
					} else {
						showcell = false;
					}
					if(cscale>0.0) {
						col = ntlColor(0,1,1);
					} else {
						col = ntlColor(1,1,0);
					}
			// TODO
			} break;
		case FLUIDDISPDensity: {
				LbmFloat rho = this->getCellDensity(cell,set);
				cscale = rho*rho * 0.25;
				col = ntlColor( MIN(0.5+cscale,1.0) , MIN(0.0+cscale,1.0), MIN(0.0+cscale,1.0) );
				cscale *= 2.0;
			} break;
		case FLUIDDISPGrid: {
				cscale = 0.59;
				col = ntlColor(1.0);
			} break;
		default: {
				cscale = 0.5;
				col = ntlColor(1.0,0.0,0.0);
			} break;
	}

	if(!showcell) return;
	if(cscale==0.0) return; // dont draw zero values
	DRAWDISPCUBE(col, cscale);
}

//! debug display function
//  D has to implement the CellIterator interface
void LbmFsgrSolver::lbmDebugDisplay(int dispset) {
	// DEBUG always display testdata
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata){ 
		cpDebugDisplay(dispset); 
		mpTest->testDebugDisplay(dispset); 
	}
#endif // LBM_INCLUDE_TESTSOLVERS==1
	if(dispset<=FLUIDDISPNothing) return;
	//if(!dispset->on) return;
	glDisable( GL_LIGHTING ); // dont light lines

#if LBM_INCLUDE_TESTSOLVERS==1
	if((!mUseTestdata)|| (mUseTestdata)&&(mpTest->mFarfMode<=0)) {
#endif // LBM_INCLUDE_TESTSOLVERS==1

	LbmFsgrSolver::CellIdentifier cid = this->getFirstCell();
	for(; this->noEndCell( cid );
	      this->advanceCell( cid ) ) {
		this->debugDisplayNode(dispset, cid );
	}
	delete cid;

#if LBM_INCLUDE_TESTSOLVERS==1
	} // 3d check
#endif // LBM_INCLUDE_TESTSOLVERS==1

	glEnable( GL_LIGHTING ); // dont light lines
}

//! debug display function
//  D has to implement the CellIterator interface
void LbmFsgrSolver::lbmMarkedCellDisplay() {
	//fluidDispSettings dispset;
	// trick - display marked cells as grid displa -> white, big
	int dispset = FLUIDDISPGrid;
	glDisable( GL_LIGHTING ); // dont light lines
	
	LbmFsgrSolver::CellIdentifier cid = this->markedGetFirstCell();
	while(cid) {
		this->debugDisplayNode(dispset, cid );
		cid = this->markedAdvanceCell();
	}
	delete cid;

	glEnable( GL_LIGHTING ); // dont light lines
}

#endif // LBM_USE_GUI==1

//! display a single node
void LbmFsgrSolver::debugPrintNodeInfo(CellIdentifierInterface* cell, int forceSet) {
		//string printInfo,
		// force printing of one set? default = -1 = off
  bool printDF     = false;
  bool printRho    = false;
  bool printVel    = false;
  bool printFlag   = false;
  bool printGeom   = false;
  bool printMass=false;
	bool printBothSets = false;
	string printInfo = this->getNodeInfoString();

	for(size_t i=0; i<printInfo.length()-0; i++) {
		char what = printInfo[i];
		switch(what) {
			case '+': // all on
								printDF = true; printRho = true; printVel = true; printFlag = true; printGeom = true; printMass = true ;
								printBothSets = true; break;
			case '-': // all off
								printDF = false; printRho = false; printVel = false; printFlag = false; printGeom = false; printMass = false; 
								printBothSets = false; break;
			case 'd': printDF = true; break;
			case 'r': printRho = true; break;
			case 'v': printVel = true; break;
			case 'f': printFlag = true; break;
			case 'g': printGeom = true; break;
			case 'm': printMass = true; break;
			case 's': printBothSets = true; break;
			default: 
				errFatal("debugPrintNodeInfo","Invalid node info id "<<what,SIMWORLD_GENERICERROR); return;
		}
	}

	ntlVec3Gfx org      = this->getCellOrigin( cell );
	ntlVec3Gfx halfsize = this->getCellSize( cell );
	int    set      = this->getCellSet( cell );
	debMsgStd("debugPrintNodeInfo",DM_NOTIFY, "Printing cell info '"<<printInfo<<"' for node: "<<cell->getAsString()<<" from "<<this->getName()<<" currSet:"<<set , 1);
	if(printGeom) debMsgStd("                  ",DM_MSG, "Org:"<<org<<" Halfsize:"<<halfsize<<" ", 1);

	int setmax = 2;
	if(!printBothSets) setmax = 1;
	if(forceSet>=0) setmax = 1;

	for(int s=0; s<setmax; s++) {
		int workset = set;
		if(s==1){ workset = (set^1); }		
		if(forceSet>=0) workset = forceSet;
		debMsgStd("                  ",DM_MSG, "Printing set:"<<workset<<" orgSet:"<<set, 1);
		
		if(printDF) {
			for(int l=0; l<LBM_DFNUM; l++) { // FIXME ??
				debMsgStd("                  ",DM_MSG, "  Df"<<l<<": "<<this->getCellDf(cell,workset,l), 1);
			}
		}
		if(printRho) {
			debMsgStd("                  ",DM_MSG, "  Rho: "<<this->getCellDensity(cell,workset), 1);
		}
		if(printVel) {
			debMsgStd("                  ",DM_MSG, "  Vel: "<<this->getCellVelocity(cell,workset), 1);
		}
		if(printFlag) {
			CellFlagType flag = this->getCellFlag(cell,workset);
			debMsgStd("                  ",DM_MSG, "  Flg: "<< flag<<" "<<convertFlags2String( flag ) <<" "<<convertCellFlagType2String( flag ), 1);
		}
		if(printMass) {
			debMsgStd("                  ",DM_MSG, "  Mss: "<<this->getCellMass(cell,workset), 1);
		}
		// dirty... TODO fixme
		debMsgStd("                  ",DM_MSG, "  Flx: "<<this->getCellDf(cell,workset,dFlux), 1);
	}
}


