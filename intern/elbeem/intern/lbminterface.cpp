/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann Interface Class
 * contains stuff to be statically compiled
 * 
 *****************************************************************************/

/* LBM Files */ 
#include "lbmdimensions.h" 
#include "lbminterface.h" 
#include "lbmfunctions.h" 
#include "ntl_scene.h"
#include "ntl_ray.h"
#include "typeslbm.h"


/*****************************************************************************/
//! common variables 

/*****************************************************************************/
/*! class for solver templating - 3D implementation D3Q19 */

	//! how many dimensions?
	const int LbmD3Q19::cDimension = 3;

	// Wi factors for collide step 
	const LbmFloat LbmD3Q19::cCollenZero    = (1.0/3.0);
	const LbmFloat LbmD3Q19::cCollenOne     = (1.0/18.0);
	const LbmFloat LbmD3Q19::cCollenSqrtTwo = (1.0/36.0);

	//! threshold value for filled/emptied cells 
	const LbmFloat LbmD3Q19::cMagicNr2    = 1.0005;
	const LbmFloat LbmD3Q19::cMagicNr2Neg = -0.0005;
	const LbmFloat LbmD3Q19::cMagicNr     = 1.010001;
	const LbmFloat LbmD3Q19::cMagicNrNeg  = -0.010001;

	//! size of a single set of distribution functions 
	const int    LbmD3Q19::cDfNum      = 19;
	//! direction vector contain vecs for all spatial dirs, even if not used for LBM model
	const int    LbmD3Q19::cDirNum     = 27;

	//const string LbmD3Q19::dfString[ cDfNum ] = { 
	const char* LbmD3Q19::dfString[ cDfNum ] = { 
		" C", " N"," S"," E"," W"," T"," B",
		"NE","NW","SE","SW",
		"NT","NB","ST","SB",
		"ET","EB","WT","WB"
	};

	const LbmD3Q19::dfDir LbmD3Q19::dfNorm[ cDfNum ] = { 
		cDirC, cDirN, cDirS, cDirE, cDirW, cDirT, cDirB, 
		cDirNE, cDirNW, cDirSE, cDirSW, 
		cDirNT, cDirNB, cDirST, cDirSB, 
		cDirET, cDirEB, cDirWT, cDirWB
	};

	const LbmD3Q19::dfDir LbmD3Q19::dfInv[ cDfNum ] = { 
		cDirC,  cDirS, cDirN, cDirW, cDirE, cDirB, cDirT,
		cDirSW, cDirSE, cDirNW, cDirNE,
		cDirSB, cDirST, cDirNB, cDirNT, 
		cDirWB, cDirWT, cDirEB, cDirET
	};

	const int LbmD3Q19::dfRefX[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		cDirSE, cDirSW, cDirNE, cDirNW,
		0, 0, 0, 0, 
		cDirEB, cDirET, cDirWB, cDirWT
	};

	const int LbmD3Q19::dfRefY[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		cDirNW, cDirNE, cDirSW, cDirSE,
		cDirNB, cDirNT, cDirSB, cDirST,
		0, 0, 0, 0
	};

	const int LbmD3Q19::dfRefZ[ cDfNum ] = { 
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

	const int LbmD3Q19::dfVecX[ cDirNum ] = { 
		0, 0,0, 1,-1, 0,0,
		1,-1,1,-1,
		0,0,0,0,
		1,1,-1,-1,
		 1,-1, 1,-1,
		 1,-1, 1,-1,
	};
	const int LbmD3Q19::dfVecY[ cDirNum ] = { 
		0, 1,-1, 0,0,0,0,
		1,1,-1,-1,
		1,1,-1,-1,
		0,0,0,0,
		 1, 1,-1,-1,
		 1, 1,-1,-1
	};
	const int LbmD3Q19::dfVecZ[ cDirNum ] = { 
		0, 0,0,0,0,1,-1,
		0,0,0,0,
		1,-1,1,-1,
		1,-1,1,-1,
		 1, 1, 1, 1,
		-1,-1,-1,-1
	};

	const LbmFloat LbmD3Q19::dfDvecX[ cDirNum ] = {
		0, 0,0, 1,-1, 0,0,
		1,-1,1,-1,
		0,0,0,0,
		1,1,-1,-1,
		 1,-1, 1,-1,
		 1,-1, 1,-1
	};
	const LbmFloat LbmD3Q19::dfDvecY[ cDirNum ] = {
		0, 1,-1, 0,0,0,0,
		1,1,-1,-1,
		1,1,-1,-1,
		0,0,0,0,
		 1, 1,-1,-1,
		 1, 1,-1,-1
	};
	const LbmFloat LbmD3Q19::dfDvecZ[ cDirNum ] = {
		0, 0,0,0,0,1,-1,
		0,0,0,0,
		1,-1,1,-1,
		1,-1,1,-1,
		 1, 1, 1, 1,
		-1,-1,-1,-1
	};

	/* principal directions */
	const int LbmD3Q19::princDirX[ 2*LbmD3Q19::cDimension ] = { 
		1,-1, 0,0, 0,0
	};
	const int LbmD3Q19::princDirY[ 2*LbmD3Q19::cDimension ] = { 
		0,0, 1,-1, 0,0
	};
	const int LbmD3Q19::princDirZ[ 2*LbmD3Q19::cDimension ] = { 
		0,0, 0,0, 1,-1
	};

	/*! arrays for les model coefficients, inited in lbmsolver constructor */
	LbmFloat LbmD3Q19::lesCoeffDiag[ (cDimension-1)*(cDimension-1) ][ cDirNum ];
	LbmFloat LbmD3Q19::lesCoeffOffdiag[ cDimension ][ cDirNum ];


	const LbmFloat LbmD3Q19::dfLength[ cDfNum ]= { 
		cCollenZero,
		cCollenOne, cCollenOne, cCollenOne, 
		cCollenOne, cCollenOne, cCollenOne,
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo
	};

	/* precalculated equilibrium dfs, inited in lbmsolver constructor */
	LbmFloat LbmD3Q19::dfEquil[ cDfNum ];

// D3Q19 end



/*****************************************************************************/
/*! class for solver templating - 2D implementation D2Q9 */

		//! how many dimensions?
		const int LbmD2Q9::cDimension = 2;

		//! Wi factors for collide step 
		const LbmFloat LbmD2Q9::cCollenZero    = (4.0/9.0);
		const LbmFloat LbmD2Q9::cCollenOne     = (1.0/9.0);
		const LbmFloat LbmD2Q9::cCollenSqrtTwo = (1.0/36.0);

		//! threshold value for filled/emptied cells 
		const LbmFloat LbmD2Q9::cMagicNr2    = 1.0005;
		const LbmFloat LbmD2Q9::cMagicNr2Neg = -0.0005;
		const LbmFloat LbmD2Q9::cMagicNr     = 1.010001;
		const LbmFloat LbmD2Q9::cMagicNrNeg  = -0.010001;

		//! size of a single set of distribution functions 
		const int LbmD2Q9::cDfNum  = 9;
		const int LbmD2Q9::cDirNum = 9;

	//const string LbmD2Q9::dfString[ cDfNum ] = { 
	const char* LbmD2Q9::dfString[ cDfNum ] = { 
		" C", 
		" N",	" S", " E", " W",
		"NE", "NW", "SE","SW" 
	};

	const LbmD2Q9::dfDir LbmD2Q9::dfNorm[ cDfNum ] = { 
		cDirC, 
		cDirN,  cDirS,  cDirE,  cDirW,
		cDirNE, cDirNW, cDirSE, cDirSW 
	};

	const LbmD2Q9::dfDir LbmD2Q9::dfInv[ cDfNum ] = { 
		cDirC,  
		cDirS,  cDirN,  cDirW,  cDirE,
		cDirSW, cDirSE, cDirNW, cDirNE 
	};

	const int LbmD2Q9::dfRefX[ cDfNum ] = { 
		0,  
		0,  0,  0,  0,
		cDirSE, cDirSW, cDirNE, cDirNW 
	};

	const int LbmD2Q9::dfRefY[ cDfNum ] = { 
		0,  
		0,  0,  0,  0,
		cDirNW, cDirNE, cDirSW, cDirSE 
	};

	const int LbmD2Q9::dfRefZ[ cDfNum ] = { 
		0,  0, 0, 0, 0,
		0, 0, 0, 0
	};

	// Vector Order 2D:
	// 0  1 2  3  4  5  6 7  8
	// 0, 0,0, 1,-1, 1,-1,1,-1 
	// 0, 1,-1, 0,0, 1,1,-1,-1 

	const int LbmD2Q9::dfVecX[ cDirNum ] = { 
		0, 
		0,0, 1,-1,
		1,-1,1,-1 
	};
	const int LbmD2Q9::dfVecY[ cDirNum ] = { 
		0, 
		1,-1, 0,0,
		1,1,-1,-1 
	};
	const int LbmD2Q9::dfVecZ[ cDirNum ] = { 
		0, 0,0,0,0, 0,0,0,0 
	};

	const LbmFloat LbmD2Q9::dfDvecX[ cDirNum ] = {
		0, 
		0,0, 1,-1,
		1,-1,1,-1 
	};
	const LbmFloat LbmD2Q9::dfDvecY[ cDirNum ] = {
		0, 
		1,-1, 0,0,
		1,1,-1,-1 
	};
	const LbmFloat LbmD2Q9::dfDvecZ[ cDirNum ] = {
		0, 0,0,0,0, 0,0,0,0 
	};

	const int LbmD2Q9::princDirX[ 2*LbmD2Q9::cDimension ] = { 
		1,-1, 0,0
	};
	const int LbmD2Q9::princDirY[ 2*LbmD2Q9::cDimension ] = { 
		0,0, 1,-1
	};
	const int LbmD2Q9::princDirZ[ 2*LbmD2Q9::cDimension ] = { 
		0,0, 0,0
	};


	/*! arrays for les model coefficients, inited in lbmsolver constructor */
	LbmFloat LbmD2Q9::lesCoeffDiag[ (cDimension-1)*(cDimension-1) ][ cDirNum ];
	LbmFloat LbmD2Q9::lesCoeffOffdiag[ cDimension ][ cDirNum ];


	const LbmFloat LbmD2Q9::dfLength[ cDfNum ]= { 
		cCollenZero,
		cCollenOne, cCollenOne, cCollenOne, cCollenOne, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo
	};

	/* precalculated equilibrium dfs, inited in lbmsolver constructor */
	LbmFloat LbmD2Q9::dfEquil[ cDfNum ];

// D2Q9 end



/******************************************************************************
 * Interface Constructor
 *****************************************************************************/
LbmSolverInterface::LbmSolverInterface() :
	mPanic( false ),
  mSizex(10), mSizey(10), mSizez(10), 
  mStepCnt( 0 ),
	mFixMass( 0.0 ),
  mOmega( 1.0 ),
  mGravity(0.0),
	mSurfaceTension( 0.0 ), 
  mInitialMass (0.0), 
	mBoundaryEast(  (CellFlagType)(CFBnd) ),mBoundaryWest( (CellFlagType)(CFBnd) ),mBoundaryNorth(  (CellFlagType)(CFBnd) ),
	mBoundarySouth( (CellFlagType)(CFBnd) ),mBoundaryTop(  (CellFlagType)(CFBnd) ),mBoundaryBottom( (CellFlagType)(CFBnd) ),
  mInitDone( false ),
  mInitDensityGradient( false ),
	mpAttrs( NULL ), mpParam( NULL ),
	mNumParticlesLost(0), mNumInvalidDfs(0), mNumFilledCells(0), mNumEmptiedCells(0), mNumUsedCells(0), mMLSUPS(0),
	mDebugVelScale( 1.0 ), mNodeInfoString("+"),
	mRandom( 5123 ),
	mvGeoStart(-1.0), mvGeoEnd(1.0),
	mPerformGeoInit( false ),
	mName("lbm_default") ,
	mpIso( NULL ), mIsoValue(0.49999),
	mSilent(false) , 
	mGeoInitId( 0 ),
	mpGiTree( NULL ),
	mAccurateGeoinit(0),
	mpGiObjects( NULL ), mGiObjInside(), mpGlob( NULL )
{
#if ELBEEM_BLENDER==1
	mSilent = true;
#endif
}


/*******************************************************************************/
/*! parse a boundary flag string */
CellFlagType LbmSolverInterface::readBoundaryFlagInt(string name, int defaultValue, string source,string target, bool needed) {
	string val  = mpAttrs->readString(name, "", source, target, needed);
	if(!strcmp(val.c_str(),"")) {
		// no value given...
		return CFEmpty;
	}
	if(!strcmp(val.c_str(),"bnd_no")) {
		return (CellFlagType)( CFBnd );
	}
	if(!strcmp(val.c_str(),"bnd_free")) {
		return (CellFlagType)( CFBnd );
	}
	if(!strcmp(val.c_str(),"fluid")) {
		/* might be used for some in/out flow cases */
		return (CellFlagType)( CFFluid );
	}
	errorOut("LbmStdSolver::readBoundaryFlagInt error: Invalid value '"<<val<<"' " );
# if LBM_STRICT_DEBUG==1
	exit(1);
# endif
	return defaultValue;
}

/*******************************************************************************/
/*! parse standard attributes */
void LbmSolverInterface::parseStdAttrList() {
	if(!mpAttrs) {
		errorOut("LbmStdSolver::parseAttrList error: mpAttrs pointer not initialized!");
		exit(1); }

	// st currently unused
	//mSurfaceTension  = mpAttrs->readFloat("d_surfacetension",  mSurfaceTension, "LbmStdSolver", "mSurfaceTension", false);
	mBoundaryEast  = readBoundaryFlagInt("boundary_east",  mBoundaryEast, "LbmStdSolver", "mBoundaryEast", false);
	mBoundaryWest  = readBoundaryFlagInt("boundary_west",  mBoundaryWest, "LbmStdSolver", "mBoundaryWest", false);
	mBoundaryNorth = readBoundaryFlagInt("boundary_north", mBoundaryNorth,"LbmStdSolver", "mBoundaryNorth", false);
	mBoundarySouth = readBoundaryFlagInt("boundary_south", mBoundarySouth,"LbmStdSolver", "mBoundarySouth", false);
	mBoundaryTop   = readBoundaryFlagInt("boundary_top",   mBoundaryTop,"LbmStdSolver", "mBoundaryTop", false);
	mBoundaryBottom= readBoundaryFlagInt("boundary_bottom", mBoundaryBottom,"LbmStdSolver", "mBoundaryBottom", false);

	LbmVec sizeVec(mSizex,mSizey,mSizez);
	sizeVec = vec2L( mpAttrs->readVec3d("size",  vec2P(sizeVec), "LbmStdSolver", "sizeVec", false) );
	mSizex = (int)sizeVec[0]; 
	mSizey = (int)sizeVec[1]; 
	mSizez = (int)sizeVec[2];
	mpParam->setSize(mSizex, mSizey, mSizez ); // param needs size in any case

	mInitDensityGradient = mpAttrs->readBool("initdensitygradient", mInitDensityGradient,"LbmStdSolver", "mInitDensityGradient", false);
	mPerformGeoInit = mpAttrs->readBool("geoinit", mPerformGeoInit,"LbmStdSolver", "mPerformGeoInit", false);
	mGeoInitId = mpAttrs->readInt("geoinitid", mGeoInitId,"LbmStdSolver", "mGeoInitId", false);
	mIsoValue = mpAttrs->readFloat("isovalue", mIsoValue, "LbmOptSolver","mIsoValue", false );

	mDebugVelScale = mpAttrs->readFloat("debugvelscale", mDebugVelScale,"LbmStdSolver", "mDebugVelScale", false);
	mNodeInfoString = mpAttrs->readString("nodeinfo", mNodeInfoString, "SimulationLbm","mNodeInfoString", false );
}


/*******************************************************************************/
/*! geometry initialization */
/*******************************************************************************/

/*****************************************************************************/
/*! init tree for certain geometry init */
void LbmSolverInterface::initGeoTree(int id) {
	if(mpGlob == NULL) { errorOut("LbmSolverInterface::initGeoTree error: Requires globals!"); exit(1); }
	mGeoInitId = id;
	ntlScene *scene = mpGlob->getScene();
	mpGiObjects = scene->getObjects();
	mGiObjInside.resize( mpGiObjects->size() );
	mGiObjDistance.resize( mpGiObjects->size() );
	for(size_t i=0; i<mpGiObjects->size(); i++) { 
		if((*mpGiObjects)[i]->getGeoInitIntersect()) mAccurateGeoinit=true;
	}
	debMsgStd("LbmSolverInterface::initGeoTree",DM_MSG,"Accurate geo init: "<<mAccurateGeoinit, 9)

	if(mpGiTree != NULL) delete mpGiTree;
	char treeFlag = (1<<(mGeoInitId+4));
	mpGiTree = new ntlTree( 20, 4, // warning - fixed values for depth & maxtriangles here...
												scene, treeFlag );
}

/*****************************************************************************/
/*! destroy tree etc. when geometry init done */
void LbmSolverInterface::freeGeoTree() {
	if(mpGiTree != NULL) {
		delete mpGiTree;
		mpGiTree = NULL;
	}
}


/*****************************************************************************/
/*! check for a certain flag type at position org */
bool LbmSolverInterface::geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId, gfxReal &distance) {
	// shift ve ctors to avoid rounding errors
	org += ntlVec3Gfx(0.0001);
	ntlVec3Gfx dir = ntlVec3Gfx(0.999999,0.0,0.0);
	OId = -1;
	ntlRay ray(org, dir, 0, 1.0, mpGlob);
	//int insCnt = 0;
	bool done = false;
	bool inside = false;
	//errMsg("III"," start org"<<org<<" dir"<<dir);
	//int insID = ray.getID();
	for(size_t i=0; i<mGiObjInside.size(); i++) { mGiObjInside[i] = 0; }
	// if not inside, return distance to first hit
	gfxReal firstHit=-1.0;
	
	if(mAccurateGeoinit) {
		while(!done) {
			// find first inside intersection
			ntlTriangle *triIns = NULL;
			distance = -1.0;
			ntlVec3Gfx normal(0.0);
			mpGiTree->intersect(ray,distance,normal, triIns, flags, true);
			if(triIns) {
				ntlVec3Gfx norg = ray.getOrigin() + ray.getDirection()*distance;
				LbmFloat orientation = dot(normal, dir);
				OId = triIns->getObjectId();
				if(orientation<=0.0) {
					// outside hit
					normal *= -1.0;
					mGiObjInside[OId]++;
					mGiObjDistance[OId] = -1.0;
					//errMsg("IIO"," oid:"<<OId<<" org"<<org<<" norg"<<norg);
				} else {
					// inside hit
					mGiObjInside[OId]++;
					mGiObjDistance[OId] = distance;
					//errMsg("III"," oid:"<<OId<<" org"<<org<<" norg"<<norg);
				}
				norg += normal * getVecEpsilon();
				ray = ntlRay(norg, dir, 0, 1.0, mpGlob);
				if(firstHit<0.0) firstHit = distance;
				//if((OId<0) && ())
				//insCnt++;
				/*
				// check outside intersect
				LbmFloat orientation = dot(normal, dir);
				if(orientation<=0.0) {
				// do more thorough checks... advance ray
				ntlVec3Gfx norg = ray.getOrigin() + ray.getDirection()*distance;
				norg += normal * (-1.0 * getVecEpsilon());
				ray = ntlRay(norg, dir, 0, 1.0, mpGlob);
				insCnt++;
				errMsg("III"," oid:"<<OId<<" org"<<org<<" norg"<<norg<<" insCnt"<<insCnt);
				} else {
				if(insCnt>0) {
				// we have entered this obj before?
				ntlVec3Gfx norg = ray.getOrigin() + ray.getDirection()*distance;
				norg += normal * (-1.0 * getVecEpsilon());
				ray = ntlRay(norg, dir, 0, 1.0, mpGlob);
				insCnt--;
				errMsg("IIIS"," oid:"<<OId<<" org"<<org<<" norg"<<norg<<" insCnt"<<insCnt);
				} else {
				// first inside intersection -> ok
				OId = triIns->getObjectId();
				done = inside = true;
				}
				}
				*/
			} else {
				// no more intersections... return false
				done = true;
				//if(insCnt%2) inside=true;
			}
		}

		distance = -1.0;
		for(size_t i=0; i<mGiObjInside.size(); i++) {
			//errMsg("CHIII","i"<<i<<" ins="<<mGiObjInside[i]<<" t"<<mGiObjDistance[i]<<" d"<<distance);
			if(((mGiObjInside[i]%2)==1)&&(mGiObjDistance[i]>0.0)) {
				if(distance<0.0) {
					// first intersection -> good
					distance = mGiObjDistance[i];
					OId = i;
					inside = true;
				} else {
					if(distance>mGiObjDistance[i]) {
						// more than one intersection -> use closest one
						distance = mGiObjDistance[i];
						OId = i;
						inside = true;
					}
				}
			}
		}
		if(!inside) {
			distance = firstHit;
		}

		return inside;
	} else {

		// find first inside intersection
		ntlTriangle *triIns = NULL;
		distance = -1.0;
		ntlVec3Gfx normal(0.0);
		mpGiTree->intersect(ray,distance,normal, triIns, flags, true);
		if(triIns) {
			// check outside intersect
			LbmFloat orientation = dot(normal, dir);
			if(orientation<=0.0) return false;

			OId = triIns->getObjectId();
			return true;
		}
		return false;
	}
}

/*****************************************************************************/
/*! get max. velocity of all objects to initialize as fluid regions */
ntlVec3Gfx LbmSolverInterface::getGeoMaxInitialVelocity() {
	ntlScene *scene = mpGlob->getScene();
	mpGiObjects = scene->getObjects();
	ntlVec3Gfx max(0.0);
	
	for(int i=0; i< (int)mpGiObjects->size(); i++) {
		if( (*mpGiObjects)[i]->getGeoInitType() & FGI_FLUID ){
			ntlVec3Gfx ovel = (*mpGiObjects)[i]->getInitialVelocity();
			if( normNoSqrt(ovel) > normNoSqrt(max) ) { max = ovel; } 
			//errMsg("IVT","i"<<i<<" "<< (*mpGiObjects)[i]->getInitialVelocity() ); // DEBUG
		}
	}
	//errMsg("IVT","max "<<" "<< max ); // DEBUG
	// unused !? mGiInsideCnt.resize( mpGiObjects->size() );

	return max;
}


/*******************************************************************************/
/*! "traditional" initialization */
/*******************************************************************************/


/*****************************************************************************/
// handle generic test cases (currently only reset geo init)
bool LbmSolverInterface::initGenericTestCases() {
	bool initTypeFound = false;
	LbmSolverInterface::CellIdentifier cid = getFirstCell();
	// deprecated! - only check for invalid cells...

	// this is the default init - check if the geometry flag init didnt
	initTypeFound = true;

	while(noEndCell(cid)) {
		// check node
		if( (getCellFlag(cid,0)==CFInvalid) || (getCellFlag(cid,1)==CFInvalid) ) {
			warnMsg("LbmSolverInterface::initGenericTestCases","GeoInit produced invalid Flag at "<<cid->getAsString()<<"!" );
		}
		advanceCell( cid );
	}

	deleteCellIterator( &cid );
	return initTypeFound;
}


/*******************************************************************************/
/*! cell iteration functions */
/*******************************************************************************/




/*****************************************************************************/
//! add cell to mMarkedCells list
void 
LbmSolverInterface::addCellToMarkedList( CellIdentifierInterface *cid ) {
	for(size_t i=0; i<mMarkedCells.size(); i++) {
		// check if cids alreay in
		if( mMarkedCells[i]->equal(cid) ) return;
		mMarkedCells[i]->setEnd(false);
	}
	mMarkedCells.push_back( cid );
	cid->setEnd(true);
}

/*****************************************************************************/
//! marked cell iteration methods
CellIdentifierInterface* 
LbmSolverInterface::markedGetFirstCell( ) {
	/*MarkedCellIdentifier *newcid = new MarkedCellIdentifier();
	if(mMarkedCells.size() < 1){ 
		newcid->setEnd( true );
	}	else {
		newcid->mpCell = mMarkedCells[0];
	}
	return newcid;*/
	return NULL;
}

void 
LbmSolverInterface::markedAdvanceCell( CellIdentifierInterface* basecid ) {
	if(!basecid) return;
	basecid->setEnd( true );
	/*MarkedCellIdentifier *cid = dynamic_cast<MarkedCellIdentifier*>( basecid );
	CellIdentifierInterface *cid = basecid;
	cid->mIndex++;
	if(cid->mIndex >= (int)mMarkedCells.size()) {
		cid->setEnd( true );
	}
	cid->mpCell = mMarkedCells[ cid->mIndex ];
	*/
}

bool 
LbmSolverInterface::markedNoEndCell( CellIdentifierInterface* cid ) {
	if(!cid) return false;
	return(! cid->getEnd() );
}

void LbmSolverInterface::markedClearList() {
	// FIXME free cids?
	mMarkedCells.clear();
}

/*******************************************************************************/
/*! string helper functions */
/*******************************************************************************/



// 32k
std::string convertSingleFlag2String(CellFlagType cflag) {
	CellFlagType flag = cflag;
	if(flag == CFUnused         ) return string("cCFUnused");
	if(flag == CFEmpty          ) return string("cCFEmpty");      
	if(flag == CFBnd            ) return string("cCFBnd");        
	if(flag == CFNoInterpolSrc  ) return string("cCFNoInterpolSrc");
	if(flag == CFFluid          ) return string("cCFFluid");      
	if(flag == CFInter          ) return string("cCFInter");      
	if(flag == CFNoNbFluid      ) return string("cCFNoNbFluid");  
	if(flag == CFNoNbEmpty      ) return string("cCFNoNbEmpty");  
	if(flag == CFNoDelete       ) return string("cCFNoDelete");   
	if(flag == CFNoBndFluid     ) return string("cCFNoBndFluid"); 
	if(flag == CFBndMARK        ) return string("cCFBndMARK");     
	if(flag == CFGrNorm         ) return string("cCFGrNorm");     
	if(flag == CFGrFromFine     ) return string("cCFGrFromFine"); 
	if(flag == CFGrFromCoarse   ) return string("cCFGrFromCoarse");
	if(flag == CFGrCoarseInited ) return string("cCFGrCoarseInited");
	if(flag == CFInvalid        ) return string("cfINVALID");

	std::ostringstream mult;
	int val = 0;
	if(flag != 0) {
		while(! (flag&1) ) {
			flag = flag>>1;
			val++;
		}
	} else {
		val = -1;
	}
	mult << "cfUNKNOWN_" << val <<"_TYPE";
	return mult.str();
}
	
//! helper function to convert flag to string (for debuggin)
std::string convertCellFlagType2String( CellFlagType cflag ) {
	int flag = cflag;

	const int jmax = sizeof(CellFlagType)*8;
	bool somefound = false;
	std::ostringstream mult;
	mult << "[";
	for(int j=0; j<jmax ; j++) {
		if(flag& (1<<j)) {
			if(somefound) mult << "|";
			mult << convertSingleFlag2String( (CellFlagType)(1<<j) ); // this call should always be _non_-recursive
			somefound = true;
		}
	};
	mult << "]";

	// return concatenated string
	if(somefound) return mult.str();

	// empty?
	return string("[emptyCFT]");
}

