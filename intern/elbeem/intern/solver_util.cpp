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


/******************************************************************************
 * helper functions
 *****************************************************************************/

//! for raytracing
void LbmFsgrSolver::prepareVisualization( void ) {
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
		*this->mpIso->lbmGetData(i,j,ZKOFF)=0.0;
	}
#else // LBMDIM==2
	// 3d, use normal bounds
#  define ZKD1 1
#  define ZKOFF k
	// reset all values...
	for(int k= getForZMinBnd(); k< getForZMaxBnd(lev); ++k) 
   for(int j=0;j<mLevel[lev].lSizey-0;j++) 
    for(int i=0;i<mLevel[lev].lSizex-0;i++) {
		*this->mpIso->lbmGetData(i,j,ZKOFF)=0.0;
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
						(val<this->mIsoValue) ){ 
					val = this->mIsoValue*1.1; }
			// */
		} else {
			// fluid?
			val = 1.0; ///27.0;
		} // */

		*this->mpIso->lbmGetData( i-1 , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[0] ); 
		*this->mpIso->lbmGetData( i   , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[1] ); 
		*this->mpIso->lbmGetData( i+1 , j-1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[2] ); 
										
		*this->mpIso->lbmGetData( i-1 , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[3] ); 
		*this->mpIso->lbmGetData( i   , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[4] ); 
		*this->mpIso->lbmGetData( i+1 , j   ,ZKOFF-ZKD1) += ( val * mIsoWeight[5] ); 
										
		*this->mpIso->lbmGetData( i-1 , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[6] ); 
		*this->mpIso->lbmGetData( i   , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[7] ); 
		*this->mpIso->lbmGetData( i+1 , j+1 ,ZKOFF-ZKD1) += ( val * mIsoWeight[8] ); 
										
										
		*this->mpIso->lbmGetData( i-1 , j-1  ,ZKOFF  ) += ( val * mIsoWeight[9] ); 
		*this->mpIso->lbmGetData( i   , j-1  ,ZKOFF  ) += ( val * mIsoWeight[10] ); 
		*this->mpIso->lbmGetData( i+1 , j-1  ,ZKOFF  ) += ( val * mIsoWeight[11] ); 
																	
		*this->mpIso->lbmGetData( i-1 , j    ,ZKOFF  ) += ( val * mIsoWeight[12] ); 
		*this->mpIso->lbmGetData( i   , j    ,ZKOFF  ) += ( val * mIsoWeight[13] ); 
		*this->mpIso->lbmGetData( i+1 , j    ,ZKOFF  ) += ( val * mIsoWeight[14] ); 
																	
		*this->mpIso->lbmGetData( i-1 , j+1  ,ZKOFF  ) += ( val * mIsoWeight[15] ); 
		*this->mpIso->lbmGetData( i   , j+1  ,ZKOFF  ) += ( val * mIsoWeight[16] ); 
		*this->mpIso->lbmGetData( i+1 , j+1  ,ZKOFF  ) += ( val * mIsoWeight[17] ); 
										
										
		*this->mpIso->lbmGetData( i-1 , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[18] ); 
		*this->mpIso->lbmGetData( i   , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[19] ); 
		*this->mpIso->lbmGetData( i+1 , j-1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[20] ); 
																 
		*this->mpIso->lbmGetData( i-1 , j   ,ZKOFF+ZKD1) += ( val * mIsoWeight[21] ); 
		*this->mpIso->lbmGetData( i   , j   ,ZKOFF+ZKD1)+= ( val * mIsoWeight[22] ); 
		*this->mpIso->lbmGetData( i+1 , j   ,ZKOFF+ZKD1) += ( val * mIsoWeight[23] ); 
																 
		*this->mpIso->lbmGetData( i-1 , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[24] ); 
		*this->mpIso->lbmGetData( i   , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[25] ); 
		*this->mpIso->lbmGetData( i+1 , j+1 ,ZKOFF+ZKD1) += ( val * mIsoWeight[26] ); 
	}


	// update preview, remove 2d?
	if((this->mOutputSurfacePreview)&&(LBMDIM==3)) {
		int pvsx = (int)(this->mPreviewFactor*this->mSizex);
		int pvsy = (int)(this->mPreviewFactor*this->mSizey);
		int pvsz = (int)(this->mPreviewFactor*this->mSizez);
		//float scale = (float)this->mSizex / previewSize;
		LbmFloat scalex = (LbmFloat)this->mSizex/(LbmFloat)pvsx;
		LbmFloat scaley = (LbmFloat)this->mSizey/(LbmFloat)pvsy;
		LbmFloat scalez = (LbmFloat)this->mSizez/(LbmFloat)pvsz;
		for(int k= 0; k< (pvsz-1); ++k) 
   		for(int j=0;j< pvsy;j++) 
    		for(int i=0;i< pvsx;i++) {
					*mpPreviewSurface->lbmGetData(i,j,k) = *this->mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley), (int)(k*scalez) );
				}
		// set borders again...
		for(int k= 0; k< (pvsz-1); ++k) {
			for(int j=0;j< pvsy;j++) {
				*mpPreviewSurface->lbmGetData(0,j,k) = *this->mpIso->lbmGetData( 0, (int)(j*scaley), (int)(k*scalez) );
				*mpPreviewSurface->lbmGetData(pvsx-1,j,k) = *this->mpIso->lbmGetData( this->mSizex-1, (int)(j*scaley), (int)(k*scalez) );
			}
			for(int i=0;i< pvsx;i++) {
				*mpPreviewSurface->lbmGetData(i,0,k) = *this->mpIso->lbmGetData( (int)(i*scalex), 0, (int)(k*scalez) );
				*mpPreviewSurface->lbmGetData(i,pvsy-1,k) = *this->mpIso->lbmGetData( (int)(i*scalex), this->mSizey-1, (int)(k*scalez) );
			}
		}
		for(int j=0;j<pvsy;j++)
			for(int i=0;i<pvsx;i++) {      
				*mpPreviewSurface->lbmGetData(i,j,0) = *this->mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley) , 0);
				*mpPreviewSurface->lbmGetData(i,j,pvsz-1) = *this->mpIso->lbmGetData( (int)(i*scalex), (int)(j*scaley) , this->mSizez-1);
			}

		if(mFarFieldSize>=2.) {
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

	// correction
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
	for(int i=0; i<(int)(numobjs+0); i++) {
		mObjectPartslips[i] = (LbmFloat)(*this->mpGiObjects)[i]->getGeoPartSlipValue();
		if(debugRecalc) errMsg("recalculateObjectSpeeds","id"<<i<<" parts "<< mObjectPartslips[i] );
	}

	mObjectPartslips[numobjs] = this->mDomainPartSlipValue;
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
  partt->setSimEnd  ( ntlVec3Gfx(this->mSizex,   this->mSizey,   getForZMaxBnd(mMaxRefine)) );
  
  while( (num<partt->getNumInitialParticles()) && (tries<100*partt->getNumInitialParticles()) ) {
    LbmFloat x,y,z;
    //x = 0.0+(( (LbmFloat)(this->mSizex-1) )     * (rand()/(RAND_MAX+1.0)) );
    //y = 0.0+(( (LbmFloat)(this->mSizey-1) )     * (rand()/(RAND_MAX+1.0)) );
    //z = 0.0+(( (LbmFloat) getForZMax1(mMaxRefine) )* (rand()/(RAND_MAX+1.0)) );
    x = 1.0+(( (LbmFloat)(this->mSizex-3.) )          * (rand()/(RAND_MAX+1.0)) );
    y = 1.0+(( (LbmFloat)(this->mSizey-3.) )          * (rand()/(RAND_MAX+1.0)) );
    z = 1.0+(( (LbmFloat) getForZMax1(mMaxRefine)-2. )* (rand()/(RAND_MAX+1.0)) );
    int i = (int)(x+0.5);
    int j = (int)(y+0.5);
    int k = (int)(z+0.5);
    if(LBMDIM==2) {
      k = 0;
      z = 0.5; // place in the middle of domain
    }

    //if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) ||
        //TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFInter ) ) { // only fluid cells?
    if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) 
        //&& TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFNoNbFluid ) 
				) { // inner fluid only 
			bool cellOk = true;
			FORDF1 { if(!(RFLAG_NB(mMaxRefine,i,j,k,workSet, l) & CFFluid)) cellOk = false; }
			if(!cellOk) continue;
      // in fluid...
      partt->addParticle(x,y,z);
			partt->getLast()->setStatus(PART_IN);
			partt->getLast()->setType(PART_TRACER);
			partt->getLast()->setSize(1.0);
      num++;
    }
    tries++;
  }


	// DEBUG TEST
#if LBM_INCLUDE_TESTSOLVERS==1
	if(mUseTestdata) { 
		const bool partDebug=false;
		if(mpTest->mDebugvalue2!=0.0){ errMsg("LbmTestdata"," part init "<<mpTest->mDebugvalue2); }
		if(mpTest->mDebugvalue2==-12.0){ 
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
		if(mpTest->mDebugvalue2==-11.0){ 
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
		if(mpTest->mDebugvalue2==-10.0){ 
			const int lev = mMaxRefine;
			const int sx = mLevel[lev].lSizex;
			const int sy = mLevel[lev].lSizey;
			//for(int j=-(int)(sy*0.25);j<-(int)(sy*0.25)+2;++j) { for(int i=-(int)(sx*0.25);i<-(int)(sy*0.25)+2;++i) {
			//for(int j=-(int)(sy*1.25);j<(int)(2.25*sy);++j) { for(int i=-(int)(sx*1.25);i<(int)(2.25*sx);++i) {
			for(int j=-(int)(sy*0.5);j<(int)(1.5*sy);++j) { for(int i=-(int)(sx*0.5);i<(int)(1.5*sx);++i) {
			//for(int j=-(int)(sy*0.2);j<(int)(0.2*sy);++j) { for(int i= (int)(sx*0.5);i<= (int)(0.51*sx);++i) {
					LbmFloat x,y,z;
					x = 0.0+(LbmFloat)(i);
					y = 0.0+(LbmFloat)(j);
					//z = mLevel[lev].lSizez/10.0*2.5 - 1.0;
					z = mLevel[lev].lSizez/20.0*7.5 - 1.0;
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

#define P_CHANGETYPE(p, newtype) \
		p->setLifeTime(0); \
    /* errMsg("PIT","U pit"<<(p)->getId()<<" pos:"<< (p)->getPos()<<" status:"<<convertFlags2String((p)->getFlags())<<" to "<< (newtype) ); */ \
		p->setType(newtype); 

void LbmFsgrSolver::advanceParticles() { 
  int workSet = mLevel[mMaxRefine].setCurr;
	LbmFloat vx=0.0,vy=0.0,vz=0.0;
	LbmFloat rho, df[27]; //feq[27];
#define DEL_PART { \
	/*errMsg("PIT","DEL AT "<< __LINE__<<" type:"<<p->getType()<<"  ");  */ \
	p->setActive( false ); \
	continue; }

	myTime_t parttstart = getTime(); 
	const LbmFloat cellsize = this->mpParam->getCellSize();
	const LbmFloat timestep = this->mpParam->getTimestep();
	//const LbmFloat viscAir = 1.79 * 1e-5; // RW2L kin. viscosity, mu
	const LbmFloat viscWater = 1.0 * 1e-6; // RW2L kin. viscosity, mu
	const LbmFloat rhoAir = 1.2;  // [kg m^-3] RW2L
	const LbmFloat rhoWater = 1000.0; // RW2L
	const LbmFloat minDropSize = 0.0005; // [m], = 2mm  RW2L
	const LbmVec   velAir(0.); // [m / s]

	const LbmFloat r1 = 0.005;  // r max
	const LbmFloat r2 = 0.0005; // r min
	const LbmFloat v1 = 9.0; // v max
	const LbmFloat v2 = 2.0; // v min
	const LbmVec rwgrav = vec2L( this->mpParam->getGravity(mSimulationTime) );
	const bool useff = (mFarFieldSize>2.); // if(mpTest->mDebugvalue1>0.0){ 

	// TODO use timestep size
	//bool isIn,isOut,isInZ;
	//const int cutval = 1+mCutoff/2; // TODO FIXME add half border!
	//const int cutval = mCutoff/2; // TODO FIXME add half border!
	//const int cutval = 0; // TODO FIXME add half border!
	const int cutval = mCutoff; // use full border!?
	int actCnt=0;
	if(this->mStepCnt%50==49) { mpParticles->cleanup(); }
  for(vector<ParticleObject>::iterator pit= mpParticles->getParticlesBegin();
      pit!= mpParticles->getParticlesEnd(); pit++) {
    //errMsg("PIT"," pit"<<(*pit)->getId()<<" pos:"<< (*pit).getPos()<<" status:"<<convertFlags2String((*pit).getFlags())<<" vel:"<< (*pit).getVel()  );
    //errMsg("PIT"," pit pos:"<< (*pit).getPos()<<" vel:"<< (*pit).getVel()<<" status:"<<convertFlags2String((*pit).getFlags()) <<" " <<mpParticles->getStart()<<" "<<mpParticles->getEnd() );
		//int flag = (*pit).getFlags();
    if( (*pit).getActive()==false ) continue;
    int i,j,k;
		ParticleObject *p = &(*pit);
		p->setLifeTime(p->getLifeTime()+1);

		// nearest neighbor, particle positions don't include empty bounds
		ntlVec3Gfx pos = p->getPos();
		i= (int)(pos[0]+0.5);
		j= (int)(pos[1]+0.5);
		k= (int)(pos[2]+0.5);
		if(LBMDIM==2) { k = 0; }

		// only testdata handling, all for sws
#if LBM_INCLUDE_TESTSOLVERS==1
		if(useff) {
			p->setStatus(PART_OUT);
			mpTest->handleParticle(p, i,j,k); continue;
		} 
#endif // LBM_INCLUDE_TESTSOLVERS==1

		// FIXME , add k tests again, remove per type ones...
		if(p->getStatus()&PART_IN) { // IN
			if( (i<cutval)||(i>this->mSizex-1-cutval)||
					(j<cutval)||(j>this->mSizey-1-cutval)
					//||(k<cutval)||(k>this->mSizez-1-cutval) 
					) {
				if(!useff) { DEL_PART;
				} else { 
					p->setStatus(PART_OUT); 
					/* del? */ //if((rand()/(RAND_MAX+1.0))<0.5) DEL_PART;
				}
			} 
		} else { // OUT rough check
			// check in again?
			if( (i>=cutval)&&(i<=this->mSizex-1-cutval)&&
					(j>=cutval)&&(j<=this->mSizey-1-cutval)
					//&&(k>=cutval)&&(k<=this->mSizez-1-cutval) 
					) {
				p->setStatus(PART_IN);
				/* del? */ //if((rand()/(RAND_MAX+1.0))<0.5) DEL_PART;
			}
		}
		//p->setStatus(PART_OUT);// DEBUG always out!

		if( (p->getType()==PART_BUBBLE) ||
		    (p->getType()==PART_TRACER) ) {

			// no interpol
			rho = vx = vy = vz = 0.0;
			if(p->getStatus()&PART_IN) { // IN
				if(k>=cutval) {
					if(k>this->mSizez-1-cutval) DEL_PART; 
					FORDF0{
						LbmFloat cdf = QCELL(mMaxRefine, i,j,k, workSet, l);
						df[l] = cdf;
						rho += cdf; 
						vx  += (this->dfDvecX[l]*cdf); 
						vy  += (this->dfDvecY[l]*cdf);  
						vz  += (this->dfDvecZ[l]*cdf);  
					}

					// remove gravity influence
					const LbmFloat lesomega = mLevel[mMaxRefine].omega; // no les
					vx -= mLevel[mMaxRefine].gravity[0] * lesomega*0.5;
					vy -= mLevel[mMaxRefine].gravity[1] * lesomega*0.5;
					vz -= mLevel[mMaxRefine].gravity[2] * lesomega*0.5;

					if( RFLAG(mMaxRefine, i,j,k, workSet)&(CFFluid) ) {
						// still ok
					} else { // OUT
						// out of bounds, deactivate...
						// FIXME make fsgr treatment
						if(p->getType()==PART_BUBBLE) { P_CHANGETYPE(p, PART_FLOAT ); continue; }
					}
				} else {
					// below 3d region, just rise
				}
			} else { // OUT
#if LBM_INCLUDE_TESTSOLVERS==1
				if(useff) { mpTest->handleParticle(p, i,j,k); }
				else DEL_PART;
#else // LBM_INCLUDE_TESTSOLVERS==1
				DEL_PART;
#endif // LBM_INCLUDE_TESTSOLVERS==1
				// TODO use x,y vel...?
			}

			ntlVec3Gfx v = p->getVel(); // dampen...
			if( (useff)&& (p->getType()==PART_BUBBLE) ) {
				// test rise
				//O vz = p->getVel()[2]-0.5*mLevel[mMaxRefine].gravity[2];

				LbmFloat radius = p->getSize() * minDropSize;
				LbmVec   velPart = vec2L(p->getVel()) *cellsize/timestep; // L2RW, lattice velocity
				LbmVec   velWater = LbmVec(vx,vy,vz) *cellsize/timestep;// L2RW, fluid velocity
				LbmVec   velRel = velWater - velPart;
				LbmFloat velRelNorm = norm(velRel);
				// TODO calculate values in lattice units, compute CD?!??!
				LbmFloat pvolume = rhoAir * 4.0/3.0 * M_PI* radius*radius*radius; // volume: 4/3 pi r^3
				//const LbmFloat cd = 

				LbmVec fb = -rwgrav* pvolume *rhoWater;
				LbmVec fd = velRel*6.0*M_PI*radius* (1e-3); //viscWater;
				LbmVec change = (fb+fd) *10.0*timestep  *(timestep/cellsize);
				//LbmVec change = (fb+fd) *timestep / (pvolume*rhoAir)  *(timestep/cellsize);
				//actCnt++; // should be after active test
				if(actCnt<0) {
					errMsg("\nPIT","BTEST1   vol="<<pvolume<<" radius="<<radius<<" vn="<<velRelNorm<<" velPart="<<velPart<<" velRel"<<velRel);
					errMsg("PIT","BTEST2        cellsize="<<cellsize<<" timestep="<<timestep<<" viscW="<<viscWater<<" ss/mb="<<(timestep/(pvolume*rhoAir)));
					errMsg("PIT","BTEST2        grav="<<rwgrav<<"  " );
					errMsg("PIT","BTEST2        change="<<(change)<<" fb="<<(fb)<<" fd="<<(fd)<<" ");
					errMsg("PIT","BTEST2        change="<<norm(change)<<" fb="<<norm(fb)<<" fd="<<norm(fd)<<" ");
#if LOOPTEST==1
					errMsg("PIT","BTEST2        n="<<n<<" "); // LOOPTEST! DEBUG
#endif // LOOPTEST==1
					errMsg("PIT","\n");
				}
					
				//v += change;
				//v += ntlVec3Gfx(vx,vy,vz);
				LbmVec fd2 = (LbmVec(vx,vy,vz)-vec2L(p->getVel())) * 6.0*M_PI*radius* (1e-3); //viscWater;
				LbmFloat w = 0.99;
				vz = (1.0-w)*vz + w*(p->getVel()[2]-0.5*(p->getSize()/5.0)*mLevel[mMaxRefine].gravity[2]);
				v = ntlVec3Gfx(vx,vy,vz)+vec2G(fd2);
			} else if(p->getType()==PART_TRACER) {
				v = ntlVec3Gfx(vx,vy,vz);
				if( RFLAG(mMaxRefine, i,j,k, workSet)&(CFFluid) ) {
					// ok
				} else {
					const int lev = mMaxRefine;
					LbmFloat nx,ny,nz, nv1,nv2;
			//mynbfac = QCELL_NB(lev, i,j,k,SRCS(lev),l, dFlux) / QCELL(lev, i,j,k,SRCS(lev), dFlux);
					if(RFLAG_NB(lev,i,j,k,workSet, dE) &(CFFluid|CFInter)){ nv1 = QCELL_NB(lev,i,j,k,workSet,dE,dFfrac); } else nv1 = 0.0;
					if(RFLAG_NB(lev,i,j,k,workSet, dW) &(CFFluid|CFInter)){ nv2 = QCELL_NB(lev,i,j,k,workSet,dW,dFfrac); } else nv2 = 0.0;
					nx = 0.5* (nv2-nv1);
					if(RFLAG_NB(lev,i,j,k,workSet, dN) &(CFFluid|CFInter)){ nv1 = QCELL_NB(lev,i,j,k,workSet,dN,dFfrac); } else nv1 = 0.0;
					if(RFLAG_NB(lev,i,j,k,workSet, dS) &(CFFluid|CFInter)){ nv2 = QCELL_NB(lev,i,j,k,workSet,dS,dFfrac); } else nv2 = 0.0;
					ny = 0.5* (nv2-nv1);
#if LBMDIM==3
					if(RFLAG_NB(lev,i,j,k,workSet, dT) &(CFFluid|CFInter)){ nv1 = QCELL_NB(lev,i,j,k,workSet,dT,dFfrac); } else nv1 = 0.0;
					if(RFLAG_NB(lev,i,j,k,workSet, dB) &(CFFluid|CFInter)){ nv2 = QCELL_NB(lev,i,j,k,workSet,dB,dFfrac); } else nv2 = 0.0;
					nz = 0.5* (nv2-nv1);
#else //LBMDIM==3
					nz = 0.0;
#endif //LBMDIM==3

					v = (ntlVec3Gfx(nx,ny,nz)) * -0.025;
				}
			}

			p->setVel( v );
			p->advanceVel();
			//errMsg("PPPP"," pos"<<p->getPos()<<" "<<p->getVel() );
		} 

		// drop handling
		else if(p->getType()==PART_DROP) {
			ntlVec3Gfx v = p->getVel(); // dampen...

			if(1) {
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

				//actCnt++; // should be after active test
				if(actCnt<0) {
					errMsg("\nPIT","NTEST1   mb="<<mb<<" radius="<<radius<<" vn="<<velRelNorm<<" velPart="<<velPart<<" velRel"<<velRel<<" pgetVel="<<p->getVel() );
					//errMsg("PIT","NTEST2        cellsize="<<cellsize<<" timestep="<<timestep<<" viscAir="<<viscAir<<" ss/mb="<<(timestep/mb));
					//errMsg("PIT","NTEST2        grav="<<rwgrav<<" mb="<<mb<<" "<<" cd="<<cd );
					//errMsg("PIT","NTEST2        change="<<norm(change)<<" fg="<<norm(fg)<<" fd="<<norm(fd)<<" ");
				}

				v += vec2G(change);
				p->setVel(v); 
				// NEW
			} else {
				p->setVel( v * 0.999 ); // dampen...
				p->setVel( v ); // DEBUG!
				p->addToVel( vec2G(mLevel[mMaxRefine].gravity) );\
			} // OLD
			p->advanceVel();

			if(p->getStatus()&PART_IN) { // IN
				if(k<cutval) { DEL_PART; continue; }
				if(k<=this->mSizez-1-cutval){ 
					//if( RFLAG(mMaxRefine, i,j,k, workSet)& (CFEmpty|CFInter)) {
					if( RFLAG(mMaxRefine, i,j,k, workSet)& (CFEmpty|CFInter|CFBnd)) {
						// still ok
					// shipt3 } else if( RFLAG(mMaxRefine, i,j,k, workSet) & (CFFluid|CFInter) ){
					} else if( RFLAG(mMaxRefine, i,j,k, workSet) & (CFFluid) ){
						// FIXME make fsgr treatment
						//if(p->getLifeTime()>50) { 
						P_CHANGETYPE(p, PART_FLOAT ); continue; 
						// jitter in cell to prevent stacking when hitting a steep surface
						LbmVec posi = p->getPos(); posi[0] += (rand()/(RAND_MAX+1.0))-0.5;
						posi[1] += (rand()/(RAND_MAX+1.0))-0.5; p->setPos(posi);
						//} else DEL_PART;
					} else {
						DEL_PART;
						this->mNumParticlesLost++;
					}
				}
			} else { // OUT
#if LBM_INCLUDE_TESTSOLVERS==1
				if(useff) { mpTest->handleParticle(p, i,j,k); }
				else{ DEL_PART; }
#else // LBM_INCLUDE_TESTSOLVERS==1
				{ DEL_PART; }
#endif // LBM_INCLUDE_TESTSOLVERS==1
			}

		} // air particle

		// inter particle
		else if(p->getType()==PART_INTER) {
			if(p->getStatus()&PART_IN) { // IN
				if((k<cutval)||(k>this->mSizez-1-cutval)) {
					// undecided particle above or below... remove?
					DEL_PART; 
				}

				if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFInter ) ) {
					// still ok
				} else if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) ) {
    			//errMsg("PIT","NEWBUB pit "<< (*pit).getPos()<<" status:"<<convertFlags2String((*pit).getFlags())  );

					//P_CHANGETYPE(p, PART_BUBBLE ); continue;
					// currently bubbles off! DEBUG!
					//DEL_PART; // DEBUG bubbles off for siggraph
					P_CHANGETYPE(p, PART_FLOAT ); continue;
				} else if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFEmpty ) ) {
    			//errMsg("PIT","NEWDROP pit "<< (*pit).getPos()<<" status:"<<convertFlags2String((*pit).getFlags())  );
					//? if(p->getLifeTime()>50) {
					// only use drops that really flew for a while...?
					//? } else DEL_PART;						
					//if( (i<=cutval)||(i>=this->mSizex-1-cutval)||
							//(j<=cutval)||(j>=this->mSizey-1-cutval)) {
					//} else 
					//if(p->getLifeTime()>10) {
					P_CHANGETYPE(p, PART_DROP ); continue;
					//} else DEL_PART;						
					
				}
			} else { // OUT
				// undecided particle outside... remove?
				DEL_PART; 
				//P_CHANGETYPE(p, PART_FLOAT ); continue;
			}
		}

		// float particle
		else if(p->getType()==PART_FLOAT) {
			//  test - delte on boundary!?
			//if( (i<=cutval)||(i>=this->mSizex-1-cutval)|| (j<=cutval)||(j>=this->mSizey-1-cutval)) { DEL_PART; } // DEBUG TEST

			LbmFloat prob = 1.0;
#if LBM_INCLUDE_TESTSOLVERS==1
			// vanishing
			prob = (rand()/(RAND_MAX+1.0));
			// increse del prob. up to max height by given factor
			const int fhCheckh = (int)(mpTest->mFluidHeight*1.25);
			const LbmFloat fhProbh = 25.;
			if((useff)&&(k>fhCheckh)) {
				LbmFloat fac = (LbmFloat)(k-fhCheckh)/(LbmFloat)(fhProbh*(mLevel[mMaxRefine].lSizez-fhCheckh));
				prob /= fac; 
			}
			if(prob<mLevel[mMaxRefine].timestep*0.1) DEL_PART;
			// forced vanishing
			//? if(k>this->mSizez*3/4) {	if(prob<3.0*mLevel[mMaxRefine].timestep*0.1) DEL_PART;}

#else // LBM_INCLUDE_TESTSOLVERS==1
#endif // LBM_INCLUDE_TESTSOLVERS==1

			if(p->getStatus()&PART_IN) { // IN
				if((k<cutval)||(k>this->mSizez-1-cutval)) DEL_PART; 
				//ntlVec3Gfx v = getVelocityAt(i,j,k);
				rho = vx = vy = vz = 0.0;

				// define from particletracer.h
#if MOVE_FLOATS==1
				const int DEPTH_AVG=3; // only average interface vels
				int ccnt=0;
				for(int kk=0;kk<DEPTH_AVG;kk+=1) {
				//for(int kk=1;kk<DEPTH_AVG;kk+=1) {
					if((k-kk)<1) continue;
					//if(RFLAG(mMaxRefine, i,j,k, workSet)&(CFFluid|CFInter)) {} else continue;
					if(RFLAG(mMaxRefine, i,j,k, workSet)&(CFInter)) {} else continue;
					ccnt++;
					FORDF0{
						LbmFloat cdf = QCELL(mMaxRefine, i,j,k-kk, workSet, l);
						df[l] = cdf;
						//rho += cdf; 
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
				vx += (rand()/(RAND_MAX+1.0))*FLOAT_JITTER-(FLOAT_JITTER*0.5);
				vy += (rand()/(RAND_MAX+1.0))*FLOAT_JITTER-(FLOAT_JITTER*0.5);

				bool delfloat = false;
				if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFFluid ) ) {
					// in fluid cell
					if((1) && (k<this->mSizez-3) && 
							(
							  TESTFLAG( RFLAG(mMaxRefine, i,j,k+1, workSet), CFInter ) ||
							  TESTFLAG( RFLAG(mMaxRefine, i,j,k+2, workSet), CFInter ) 
							) ) {
						vz = p->getVel()[2]-0.5*mLevel[mMaxRefine].gravity[2];
						if(vz<0.0) vz=0.0;
					} else delfloat=true;
					// keep below obstacles
					if((delfloat) && (TESTFLAG( RFLAG(mMaxRefine, i,j,k+1, workSet), CFBnd )) ) {
						delfloat=false; vz=0.0;
					}
				} else if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFInter ) ) {
					// keep in interface , one grid cell offset is added in part. gen
				} else { //if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFEmpty ) ) { // shipt?, was all else before
					// check if above inter, remove otherwise
					if((1) && (k>2) && (
								TESTFLAG( RFLAG(mMaxRefine, i,j,k-1, workSet), CFInter ) ||
								TESTFLAG( RFLAG(mMaxRefine, i,j,k-2, workSet), CFInter ) 
								) ) {
						vz = p->getVel()[2]+0.5*mLevel[mMaxRefine].gravity[2];
						if(vz>0.0) vz=0.0;
					} else delfloat=true; // */
				}
				if(delfloat) DEL_PART;
				/*
				// move down from empty
				else if( TESTFLAG( RFLAG(mMaxRefine, i,j,k, workSet), CFEmpty ) ) {
					vz = p->getVel()[2]+0.5*mLevel[mMaxRefine].gravity[2];
					if(vz>0.0) vz=0.0;
					//DEL_PART; // ????
				} else  {	 DEL_PART; } // */
				//vz = 0.0; // DEBUG

				p->setVel( vec2G( ntlVec3Gfx(vx,vy,vz) ) ); //?
				//p->setVel( vec2G(v)*0.75 + p->getVel()*0.25 ); //?
				p->advanceVel();
				//errMsg("PIT","IN pit "<< (*pit).getPos()<<" status:"<<convertFlags2String((*pit).getFlags())  );
			} else {
#if LBM_INCLUDE_TESTSOLVERS==1
				if(useff) { mpTest->handleParticle(p, i,j,k); }
				else DEL_PART; 
#else // LBM_INCLUDE_TESTSOLVERS==1
				DEL_PART; 
#endif // LBM_INCLUDE_TESTSOLVERS==1
				//errMsg("PIT","OUT pit "<< (*pit).getPos()<<" status:"<<convertFlags2String((*pit).getFlags())  );
			}
				
			// additional bnd jitter
			if((1) && (useff) && (p->getLifeTime()<3)) {
				// use half butoff border 1/8
				int maxdw = (int)(mLevel[mMaxRefine].lSizex*0.125*0.5);
				if(maxdw<3) maxdw=3;
#define FLOAT_JITTER_BND (FLOAT_JITTER*2.0)
#define FLOAT_JITTBNDRAND(x) ((rand()/(RAND_MAX+1.0))*FLOAT_JITTER_BND*(1.-(x/(LbmFloat)maxdw))-(FLOAT_JITTER_BND*(1.-(x)/(LbmFloat)maxdw)*0.5)) 
				//if(ABS(i-(               cutval))<maxdw) { p->advance(  (rand()/(RAND_MAX+1.0))*FLOAT_JITTER_BND-(FLOAT_JITTER_BND*0.5)   ,0.,0.); }
				//if(ABS(i-(this->mSizex-1-cutval))<maxdw) { p->advance(  (rand()/(RAND_MAX+1.0))*FLOAT_JITTER_BND-(FLOAT_JITTER_BND*0.5)   ,0.,0.); }
				if((j>=0)&&(j<=this->mSizey-1)) {
					if(ABS(i-(               cutval))<maxdw) { p->advance(  FLOAT_JITTBNDRAND( ABS(i-(               cutval))), 0.,0.); }
					if(ABS(i-(this->mSizex-1-cutval))<maxdw) { p->advance(  FLOAT_JITTBNDRAND( ABS(i-(this->mSizex-1-cutval))), 0.,0.); }
				}
//#undef FLOAT_JITTER_BND
#undef FLOAT_JITTBNDRAND
				//if( (i<cutval)||(i>this->mSizex-1-cutval)|| //(j<cutval)||(j>this->mSizey-1-cutval)
			}
		} 
		
		// unknown particle type	
		else {
			errMsg("LbmFsgrSolver::advanceParticles","PIT pit invalid type!? "<<p->getStatus() );
		}
  }
	myTime_t parttend = getTime(); 
	debMsgStd("LbmFsgrSolver::advanceParticles",DM_MSG,"Time for particle update:"<< getTimeString(parttend-parttstart)<<" "<<mpParticles->getNumParticles() , 10 );
}

void LbmFsgrSolver::notifySolverOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename) {
	// debug - raw dump of ffrac values
	if(getenv("ELBEEM_RAWDEBUGDUMP")) {
		int workSet = mLevel[mMaxRefine].setCurr;
		std::ostringstream name;
		//name <<"fill_" << this->mStepCnt <<".dump";
		name << outfilename<< frameNrStr <<".dump";
		FILE *file = fopen(name.str().c_str(),"w");
		if(file) {

			for(int k= getForZMinBnd(); k< getForZMaxBnd(mMaxRefine); ++k)  {
				for(int j=0;j<mLevel[mMaxRefine].lSizey-0;j++)  {
					for(int i=0;i<mLevel[mMaxRefine].lSizex-0;i++) {
						float val = 0.;
						if(RFLAG(mMaxRefine, i,j,k, workSet) & CFInter) val = QCELL(mMaxRefine,i,j,k, mLevel[mMaxRefine].setCurr,dFfrac);
						if(RFLAG(mMaxRefine, i,j,k, workSet) & CFFluid) val = 1.;
						//fwrite( &val, sizeof(val), 1, file); // binary
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

	LbmFloat rho = 0.0;
	//FORDF0 { rho += QCELL(cid->level, cid->x,cid->y,cid->z, set, l); } // ORG
	//return ((rho-1.0) * mLevel[cid->level].simCellSize / mLevel[cid->level].timestep) +1.0; // ORG
	if(RFLAG(cid->level, cid->x,cid->y,cid->z, set)&CFInter) { // test
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
	return rho; // test
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
	LbmFloat nsize = mLevel[level].nodeSize;
	const int x = (int)((-this->mvGeoStart[0]+xp-0.5*nsize) / nsize );
	const int y = (int)((-this->mvGeoStart[1]+yp-0.5*nsize) / nsize );
	int       z = (int)((-this->mvGeoStart[2]+zp-0.5*nsize) / nsize );
	if(LBMDIM==2) z=0;
	//errMsg("DUMPVEL","p"<<PRINT_VEC(xp,yp,zp)<<" at "<<PRINT_VEC(x,y,z)<<" max"<<PRINT_VEC(mLevel[level].lSizex,mLevel[level].lSizey,mLevel[level].lSizez) );

	// return fluid/if/border cells
	// search neighborhood, do smoothing
	FORDF0{ 
		int i=x+this->dfVecX[l];
		int j=y+this->dfVecY[l];
		int k=z+this->dfVecZ[l];

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

	if(ii<0){ errMsg("LbmStrict"," invX- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ij<0){ errMsg("LbmStrict"," invY- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ik<0){ errMsg("LbmStrict"," invZ- l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ii>mLevel[level].lSizex-1){ errMsg("LbmStrict"," invX+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
	if(ij>mLevel[level].lSizey-1){ errMsg("LbmStrict"," invY+ l"<<level<<"|"<<ii<<","<<ij<<","<<ik<<" s"<<is); STRICT_EXIT; }
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
	if(mUseTestdata){ mpTest->testDebugDisplay(dispset); }
#endif // LBM_INCLUDE_TESTSOLVERS==1
	if(dispset<=FLUIDDISPNothing) return;
	//if(!dispset->on) return;
	glDisable( GL_LIGHTING ); // dont light lines

#if LBM_INCLUDE_TESTSOLVERS==1
	if((!mUseTestdata)|| (mUseTestdata)&&(mpTest->mDebugvalue1<=0.0)) {
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


