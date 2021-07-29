/** \file elbeem/intern/solver_control.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - the visual lattice boltzmann freesurface simulator
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * testing extensions
 *
 *****************************************************************************/


#ifndef LBM_TESTCLASS_H
#define LBM_TESTCLASS_H

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

//class IsoSurface;
class ParticleObject;
class ControlParticles;
class ControlForces;

//#define NUMGRIDS 2
//#define MAXNUMSWS 10

// farfield modes
#define FARF_3DONLY -1
#define FARF_BOTH    0
#define FARF_SWEONLY 1
// dont reuse 3d vars/init
#define FARF_SEPSWE  2

// relaxation macros for solver_relax.h

// WARNING has to match controlparts.h
#define CPF_ENTRIES     12
#define CPF_FORCE       0
#define CPF_VELWEIGHT   3
#define CPF_VELOCITY    4
#define CPF_FORCEWEIGHT 7
#define CPF_MINCPDIST   8
#define CPF_MINCPDIR    9

#include "controlparticles.h"

#include "ntl_geometrymodel.h"
			 
// get force entry, set=0 is unused anyway
#define LBMGET_FORCE(lev, i,j,k)  mpControl->mCpForces[lev][ (LBMGI(lev,i,j,k,0)) ]

// debug mods off...
// same as in src/solver_relax.h!
#define __PRECOLLIDE_MODS(rho,ux,uy,uz, grav) \
	ux += (grav)[0]; \
	uy += (grav)[1]; \
	uz += (grav)[2]; 

//void testMaxdmod(int i, int j,int k, LbmFloat &ux,LbmFloat &uy,LbmFloat &uz,ControlForces &ff);
#if LBMDIM==3
#define MAXDGRAV \
			if(myforce->forceMaxd[0]*ux+myforce->forceMaxd[1]*uy<LBM_EPSILON) { \
				ux = v2w*myforce->forceVel[0]+ v2wi*ux;  \
				uy = v2w*myforce->forceVel[1]+ v2wi*uy; }  \
			/* movement inverse to g? */ \
			if((uz>LBM_EPSILON)&&(uz>myforce->forceVel[2])) { \
				uz = v2w*myforce->forceVel[2]+ v2wi*uz; } 
#else // LBMDIM==3
#define MAXDGRAV \
			if(myforce->forceMaxd[0]*ux<LBM_EPSILON) { \
				ux = v2w*myforce->forceVel[0]+ v2wi*ux; } \
			/* movement inverse to g? */ \
			if((uy>LBM_EPSILON)&&(uy>myforce->forceVel[1])) { \
				uy = v2w*myforce->forceVel[1]+ v2wi*uy; } 
#endif // LBMDIM==3

// debug modifications of collide vars (testing)
// requires: lev,i,j,k
#define PRECOLLIDE_MODS(rho,ux,uy,uz, grav) \
	LbmFloat attforce = 1.; \
	if(this->mTForceStrength>0.) { \
		ControlForces* myforce = &LBMGET_FORCE(lev,i,j,k); \
		const LbmFloat vf = myforce->weightAtt;\
		const LbmFloat vw = myforce->weightVel;\
		if(vf!=0.) { attforce = MAX(0., 1.-vf);  /* TODO FIXME? use ABS(vf) for repulsion force? */ \
			ux += myforce->forceAtt[0]; \
			uy += myforce->forceAtt[1]; \
			uz += myforce->forceAtt[2]; \
			\
		} else if(( myforce->maxDistance>0.) && ( myforce->maxDistance<CPF_MAXDINIT)) {\
			const LbmFloat v2w = mpControl->mCons[0]->mCparts->getInfluenceMaxdist() * \
				(myforce->maxDistance-mpControl->mCons[0]->mCparts->getRadiusMinMaxd()) / (mpControl->mCons[0]->mCparts->getRadiusMaxd()-mpControl->mCons[0]->mCparts->getRadiusMinMaxd()); \
			const LbmFloat v2wi = 1.-v2w; \
			if(v2w>0.){ MAXDGRAV; \
				/* errMsg("ERRMDTT","at "<<PRINT_IJK<<" maxd="<<myforce->maxDistance<<", newu"<<PRINT_VEC(ux,uy,uz)<<", org"<<PRINT_VEC(oux,ouy,ouz)<<", fv"<<myforce->forceVel<<" " );  */ \
			}\
		} \
		if(vw>0.) { \
			const LbmFloat vwi = 1.-vw;\
			const LbmFloat vwd = mpControl->mDiffVelCon;\
			ux += vw*(myforce->forceVel[0]-myforce->compAv[0] + vwd*(myforce->compAv[0]-ux) ); \
			uy += vw*(myforce->forceVel[1]-myforce->compAv[1] + vwd*(myforce->compAv[1]-uy) ); \
			uz += vw*(myforce->forceVel[2]-myforce->compAv[2] + vwd*(myforce->compAv[2]-uz) ); \
			/*  TODO test!? modify smooth vel by influence of force for each lbm step, to account for force update only each N steps */ \
			myforce->compAv = (myforce->forceVel*vw+ myforce->compAv*vwi); \
		} \
	} \
	ux += (grav)[0]*attforce; \
	uy += (grav)[1]*attforce; \
	uz += (grav)[2]*attforce; \
 	/* end PRECOLLIDE_MODS */

#define TEST_IF_CHECK \
		if((!iffilled)&&(LBMGET_FORCE(lev,i,j,k).weightAtt!=0.)) { \
			errMsg("TESTIFFILL"," at "<<PRINT_IJK<<" "<<mass<<" "<<rho); \
			iffilled = true; \
			if(mass<rho*1.0) mass = rho*1.0; myfrac = 1.0; \
		}


// a single set of control particles and params
class LbmControlSet {
	public:
		LbmControlSet();
		~LbmControlSet();
		void initCparts();

		// control particles
		ControlParticles *mCparts; 
		// control particle overall motion (for easier manual generation)
		ControlParticles *mCpmotion;
		// cp data file
		string mContrPartFile;
		string mCpmotionFile;
		// cp debug displau
		LbmFloat mDebugCpscale, mDebugVelScale, mDebugCompavScale, mDebugAttScale, mDebugMaxdScale, mDebugAvgVelScale;

		// params
		AnimChannel<float> mcForceAtt;
		AnimChannel<float> mcForceVel;
		AnimChannel<float> mcForceMaxd;

		AnimChannel<float> mcRadiusAtt;
		AnimChannel<float> mcRadiusVel;
		AnimChannel<float> mcRadiusMind;
		AnimChannel<float> mcRadiusMaxd;

		AnimChannel<ntlVec3f> mcCpScale;
		AnimChannel<ntlVec3f> mcCpOffset;

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:LbmControlSet")
#endif
};
		


// main control data storage
class LbmControlData 
{
	public:
		LbmControlData();
		virtual ~LbmControlData();

		// control data

		// contorl params
		void parseControldataAttrList(AttributeList *attr);

		// control strength, set for solver interface
		LbmFloat mSetForceStrength;
		// cp vars
		std::vector<LbmControlSet*> mCons;
		// update interval
		int mCpUpdateInterval;
		// output
		string mCpOutfile;
		// control particle precomputed influence
		std::vector< std::vector<ControlForces> > mCpForces;
		std::vector<ControlForces> mCpKernel;
		std::vector<ControlForces> mMdKernel;
		// activate differential velcon
		LbmFloat mDiffVelCon;

		// cp debug displau
		LbmFloat mDebugCpscale, mDebugVelScale, mDebugCompavScale, mDebugAttScale, mDebugMaxdScale, mDebugAvgVelScale;

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:LbmControlData ")
#endif
};

#endif // LBM_TESTCLASS_H
