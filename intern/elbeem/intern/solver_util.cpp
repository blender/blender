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
#endif // __APPLE_CC__


#if !defined(__APPLE_CC__) || defined(LBM_FORCEINCLUDE)
/******************************************************************************
 * helper functions
 *****************************************************************************/

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
			/* // flicker-test-fix: no real difference
			if( (!(RFLAG(lev, i,j,k,workSet)&CFNoBndFluid)) && 
						(RFLAG(lev, i,j,k,workSet)&CFNoNbFluid)   &&
						(val<D::mIsoValue) ){ 
					val = D::mIsoValue*1.1; }
			// */
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

	
	/*
  for(int k=0;k<mLevel[mMaxRefine].lSizez-1;k++)
    for(int j=0;j<mLevel[mMaxRefine].lSizey-1;j++) {
			*D::mpIso->lbmGetData(-1,                           j,ZKOFF) = *D::mpIso->lbmGetData( 1,                         j,ZKOFF);
			*D::mpIso->lbmGetData( 0,                           j,ZKOFF) = *D::mpIso->lbmGetData( 1,                         j,ZKOFF);
			*D::mpIso->lbmGetData( mLevel[mMaxRefine].lSizex-1, j,ZKOFF) = *D::mpIso->lbmGetData( mLevel[mMaxRefine].lSizex-2, j,ZKOFF);
			*D::mpIso->lbmGetData( mLevel[mMaxRefine].lSizex-0, j,ZKOFF) = *D::mpIso->lbmGetData( mLevel[mMaxRefine].lSizex-2, j,ZKOFF);
    }

  for(int k=0;k<mLevel[mMaxRefine].lSizez-1;k++)
    for(int i=-1;i<mLevel[mMaxRefine].lSizex+1;i++) {      
			*D::mpIso->lbmGetData( i,-1,                           ZKOFF) = *D::mpIso->lbmGetData( i, 1,                         ZKOFF);
			*D::mpIso->lbmGetData( i, 0,                           ZKOFF) = *D::mpIso->lbmGetData( i, 1,                         ZKOFF);
			*D::mpIso->lbmGetData( i, mLevel[mMaxRefine].lSizey-1, ZKOFF) = *D::mpIso->lbmGetData( i, mLevel[mMaxRefine].lSizey-2, ZKOFF);
			*D::mpIso->lbmGetData( i, mLevel[mMaxRefine].lSizey-0, ZKOFF) = *D::mpIso->lbmGetData( i, mLevel[mMaxRefine].lSizey-2, ZKOFF);
    }

	if(D::cDimension == 3) {
		// only for 3D
		for(int j=-1;j<mLevel[mMaxRefine].lSizey+1;j++)
			for(int i=-1;i<mLevel[mMaxRefine].lSizex+1;i++) {      
				//initEmptyCell(mMaxRefine, i,j,0, domainBoundType, 0.0, BND_FILL); initEmptyCell(mMaxRefine, i,j,mLevel[mMaxRefine].lSizez-1, domainBoundType, 0.0, BND_FILL); 
				*D::mpIso->lbmGetData( i,j,-1                         ) = *D::mpIso->lbmGetData( i,j,1                        );
				*D::mpIso->lbmGetData( i,j, 0                         ) = *D::mpIso->lbmGetData( i,j,1                        );
				*D::mpIso->lbmGetData( i,j,mLevel[mMaxRefine].lSizez-1) = *D::mpIso->lbmGetData( i,j,mLevel[mMaxRefine].lSizez-2);
				*D::mpIso->lbmGetData( i,j,mLevel[mMaxRefine].lSizez-0) = *D::mpIso->lbmGetData( i,j,mLevel[mMaxRefine].lSizez-2);
			}
	}
	// */

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

/*! calculate speeds of fluid objects (or inflow) */
template<class D>
void LbmFsgrSolver<D>::recalculateObjectSpeeds() {
	int numobjs = (int)(D::mpGiObjects->size());
	// note - (numobjs + 1) is entry for domain settings
	if(numobjs>255-1) {
		errFatal("LbmFsgrSolver::recalculateObjectSpeeds","More than 256 objects currently not supported...",SIMWORLD_INITERROR);
		return;
	}
	mObjectSpeeds.resize(numobjs+1);
	for(int i=0; i<(int)(numobjs+0); i++) {
		mObjectSpeeds[i] = vec2L(D::mpParam->calculateLattVelocityFromRw( vec2P( (*D::mpGiObjects)[i]->getInitialVelocity() )));
		//errMsg("recalculateObjectSpeeds","id"<<i<<" set to "<< mObjectSpeeds[i]<<", unscaled:"<< (*D::mpGiObjects)[i]->getInitialVelocity() );
	}

	// also reinit part slip values here
	mObjectPartslips.resize(numobjs+1);
	for(int i=0; i<(int)(numobjs+0); i++) {
		mObjectPartslips[i] = (LbmFloat)(*D::mpGiObjects)[i]->getGeoPartSlipValue();
	}
	//errMsg("GEOIN"," dm set "<<mDomainPartSlipValue);
	mObjectPartslips[numobjs] = mDomainPartSlipValue;
}



/*****************************************************************************/
/*! debug object display */
/*****************************************************************************/
template<class D>
vector<ntlGeometryObject*> LbmFsgrSolver<D>::getDebugObjects() { 
	vector<ntlGeometryObject*> debo; 
	if(mOutputSurfacePreview) {
		debo.push_back( mpPreviewSurface );
	}
#ifndef ELBEEM_BLENDER
	if(mUseTestdata) debo.push_back( mpTest );
#endif // ELBEEM_BLENDER
	return debo; 
}

/******************************************************************************
 * particle handling
 *****************************************************************************/

/*! init particle positions */
template<class D>
int LbmFsgrSolver<D>::initParticles(ParticleTracer *partt) { 
#ifdef ELBEEM_BLENDER
	partt = NULL; // remove warning
#else // ELBEEM_BLENDER
  int workSet = mLevel[mMaxRefine].setCurr;
  int tries = 0;
  int num = 0;

  //partt->setSimEnd  ( ntlVec3Gfx(D::mSizex-1, D::mSizey-1, getForZMax1()) );
  partt->setSimEnd  ( ntlVec3Gfx(D::mSizex,   D::mSizey,   D::getForZMaxBnd()) );
  partt->setSimStart( ntlVec3Gfx(0.0) );
  
  while( (num<partt->getNumParticles()) && (tries<100*partt->getNumParticles()) ) {
    double x,y,z;
    x = 0.0+(( (float)(D::mSizex-1) )     * (rand()/(RAND_MAX+1.0)) );
    y = 0.0+(( (float)(D::mSizey-1) )     * (rand()/(RAND_MAX+1.0)) );
    z = 0.0+(( (float) D::getForZMax1()  )* (rand()/(RAND_MAX+1.0)) );
    int i = (int)(x-0.5);
    int j = (int)(y-0.5);
    int k = (int)(z-0.5);
    if(D::cDimension==2) {
      k = 0;
      z = 0.5; // place in the middle of domain
    }

    if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) ||
        TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) ) { // only fluid cells?
      // in fluid...
      partt->addParticle(x,y,z);
      num++;
    }
    tries++;
  }
  debMsgStd("LbmTestSolver::initParticles",DM_MSG,"Added "<<num<<" particles ", 10);
  if(num != partt->getNumParticles()) return 1;
#endif // ELBEEM_BLENDER

	return 0;
}

template<class D>
void LbmFsgrSolver<D>::advanceParticles(ParticleTracer *partt ) { 
#ifdef ELBEEM_BLENDER
	partt = NULL; // remove warning
#else // ELBEEM_BLENDER
  int workSet = mLevel[mMaxRefine].setCurr;
	LbmFloat vx=0.0,vy=0.0,vz=0.0;
	LbmFloat rho, df[27]; //feq[27];

  for(vector<ParticleObject>::iterator p= partt->getParticlesBegin();
      p!= partt->getParticlesEnd(); p++) {
    //errorOut(" p "<< (*p).getPos() );
    if( (*p).getActive()==false ) continue;
    int i,j,k;

    // nearest neighbor, particle positions don't include empty bounds
    ntlVec3Gfx pos = (*p).getPos();
    i= (int)(pos[0]+0.5);
    j= (int)(pos[1]+0.5);
    k= (int)(pos[2]+0.5);
    if(D::cDimension==2) {
      k = 0;
    }

    if( (i<0)||(i>D::mSizex-1)||
        (j<0)||(j>D::mSizey-1)||
        (k<0)||(k>D::mSizez-1) ) {
      (*p).setActive( false );
      continue;
    }

    // no interpol
    rho = vx = vy = vz = 0.0;
		FORDF0{
			LbmFloat cdf = QCELL(mMaxRefine, i,j,k, workSet, l);
			df[l] = cdf;
			rho += cdf; 
			vx  += (D::dfDvecX[l]*cdf); 
			vy  += (D::dfDvecY[l]*cdf);  
			vz  += (D::dfDvecZ[l]*cdf);  
		}

    // remove gravity influence
		//FORDF0{ feq[l] = D::getCollideEq(l, rho,vx,vy,vz); }
		//const LbmFloat Qo = D::getLesNoneqTensorCoeff(df,feq);
		//const LbmFloat lesomega = D::getLesOmega(mLevel[mMaxRefine].omega,mLevel[mMaxRefine].lcsmago,Qo);
		const LbmFloat lesomega = mLevel[mMaxRefine].omega; // no les
    vx -= mLevel[mMaxRefine].gravity[0] * lesomega*0.5;
    vy -= mLevel[mMaxRefine].gravity[1] * lesomega*0.5;
    vz -= mLevel[mMaxRefine].gravity[2] * lesomega*0.5;

    if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) ||
        TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFInter ) ) {
      // still ok
    } else {
      // out of bounds, deactivate...
			// FIXME had fsgr treatment
      (*p).setActive( false );
      continue;
      D::mNumParticlesLost++;
    }

    (*p).advance( vx,vy,vz );
  }
#endif // ELBEEM_BLENDER
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
	debugPrintNodeInfo( newcid, set );
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
	//lbmDebugDisplay< LbmFsgrSolver<D> >( set, this ); 
	lbmDebugDisplay( set ); 
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


/******************************************************************************
 * GUI&debugging functions
 *****************************************************************************/


#if LBM_USE_GUI==1
#define USE_GLUTILITIES
#include "../gui/gui_utilities.h"

//! display a single node
template<class D>
void LbmFsgrSolver<D>::debugDisplayNode(fluidDispSettings *dispset, CellIdentifierInterface* cell ) {
	//debugOut(" DD: "<<cell->getAsString() , 10);
	ntlVec3Gfx org      = this->getCellOrigin( cell );
	ntlVec3Gfx halfsize = this->getCellSize( cell );
	int    set      = this->getCellSet( cell );
	//debugOut(" DD: "<<cell->getAsString()<<" "<< (dispset->type) , 10);

	bool     showcell = true;
	int      linewidth = 1;
	ntlColor col(0.5);
	LbmFloat cscale = dispset->scale;

#define DRAWDISPCUBE(col,scale) \
	{	glLineWidth( linewidth ); \
	  glColor3f( (col)[0], (col)[1], (col)[2]); \
		ntlVec3Gfx s = org-(halfsize * (scale)); \
		ntlVec3Gfx e = org+(halfsize * (scale)); \
		drawCubeWire( s,e ); }

	switch(dispset->type) {
		case FLUIDDISPNothing: {
				showcell = false;
			} break;
		case FLUIDDISPCelltypes: {
				CellFlagType flag = this->getCellFlag(cell, set );
				cscale = 0.5;

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
				CellFlagType flag = this->getCellFlag( cell,set );
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
	DRAWDISPCUBE(col, cscale);
}

//! debug display function
//  D has to implement the CellIterator interface
template<class D>
void LbmFsgrSolver<D>::lbmDebugDisplay(fluidDispSettings *dispset) {
	//je nach solver...?
	if(!dispset->on) return;
	glDisable( GL_LIGHTING ); // dont light lines

	typename D::CellIdentifier cid = this->getFirstCell();
	for(; this->noEndCell( cid );
	      this->advanceCell( cid ) ) {
		this->debugDisplayNode(dispset, cid );
	}
	delete cid;

	glEnable( GL_LIGHTING ); // dont light lines
}

//! debug display function
//  D has to implement the CellIterator interface
template<class D>
void LbmFsgrSolver<D>::lbmMarkedCellDisplay() {
	fluidDispSettings dispset;
	// trick - display marked cells as grid displa -> white, big
	dispset.type = FLUIDDISPGrid;
	dispset.on = true;
	glDisable( GL_LIGHTING ); // dont light lines
	
	typename D::CellIdentifier cid = this->markedGetFirstCell();
	while(cid) {
		this->debugDisplayNode(&dispset, cid );
		cid = this->markedAdvanceCell();
	}
	delete cid;

	glEnable( GL_LIGHTING ); // dont light lines
}

#endif // LBM_USE_GUI==1

//! display a single node
template<class D>
void LbmFsgrSolver<D>::debugPrintNodeInfo(CellIdentifierInterface* cell, int forceSet) {
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
			for(int l=0; l<this->getDfNum(); l++) { // FIXME ??
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
	}
}

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
//template<class LBM_INSTANTIATE> void LbmFsgrSolver<LBM_INSTANTIATE>::prepareVisualization( void );
//template<class LBM_INSTANTIATE> vector<ntlGeometryObject*> LbmFsgrSolver<LBM_INSTANTIATE>::getDebugObjects();
//template<class LBM_INSTANTIATE> int  LbmFsgrSolver<LBM_INSTANTIATE>::initParticles(ParticleTracer *partt );
//template<class LBM_INSTANTIATE> void LbmFsgrSolver<LBM_INSTANTIATE>::advanceParticles(ParticleTracer *partt );

// instantiate whole celliterator interface
//template<class LBM_INSTANTIATE> CellIdentifierInterface* LbmFsgrSolver<LBM_INSTANTIATE>::getFirstCell( );
//template<class LBM_INSTANTIATE> void LbmFsgrSolver<LBM_INSTANTIATE>::advanceCell( CellIdentifierInterface* );
//template<class LBM_INSTANTIATE> bool LbmFsgrSolver<LBM_INSTANTIATE>::noEndCell( CellIdentifierInterface* );
//template<class LBM_INSTANTIATE> void LbmFsgrSolver<LBM_INSTANTIATE>::deleteCellIterator( CellIdentifierInterface** );
//template<class LBM_INSTANTIATE> CellIdentifierInterface* LbmFsgrSolver<LBM_INSTANTIATE>::getCellAt( ntlVec3Gfx pos );
//template<class LBM_INSTANTIATE> int        LbmFsgrSolver<LBM_INSTANTIATE>::getCellSet      ( CellIdentifierInterface* );
//template<class LBM_INSTANTIATE> ntlVec3Gfx LbmFsgrSolver<LBM_INSTANTIATE>::getCellOrigin   ( CellIdentifierInterface* );
//template<class LBM_INSTANTIATE> ntlVec3Gfx LbmFsgrSolver<LBM_INSTANTIATE>::getCellSize     ( CellIdentifierInterface* );
//template<class LBM_INSTANTIATE> int        LbmFsgrSolver<LBM_INSTANTIATE>::getCellLevel    ( CellIdentifierInterface* );
//template<class LBM_INSTANTIATE> LbmFloat   LbmFsgrSolver<LBM_INSTANTIATE>::getCellDensity  ( CellIdentifierInterface* ,int set);
//template<class LBM_INSTANTIATE> LbmVec     LbmFsgrSolver<LBM_INSTANTIATE>::getCellVelocity ( CellIdentifierInterface* ,int set);
//template<class LBM_INSTANTIATE> LbmFloat   LbmFsgrSolver<LBM_INSTANTIATE>::getCellDf       ( CellIdentifierInterface* ,int set, int dir);
//template<class LBM_INSTANTIATE> LbmFloat   LbmFsgrSolver<LBM_INSTANTIATE>::getCellMass     ( CellIdentifierInterface* ,int set);
//template<class LBM_INSTANTIATE> LbmFloat   LbmFsgrSolver<LBM_INSTANTIATE>::getCellFill     ( CellIdentifierInterface* ,int set);
//template<class LBM_INSTANTIATE> CellFlagType LbmFsgrSolver<LBM_INSTANTIATE>::getCellFlag   ( CellIdentifierInterface* ,int set);
//template<class LBM_INSTANTIATE> LbmFloat   LbmFsgrSolver<LBM_INSTANTIATE>::getEquilDf      ( int );
//template<class LBM_INSTANTIATE> int        LbmFsgrSolver<LBM_INSTANTIATE>::getDfNum        ( );

