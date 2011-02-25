/** \file elbeem/intern/isosurface.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Marching Cubes surface mesh generation
 *
 *****************************************************************************/

#include "isosurface.h"
#include "mcubes_tables.h"
#include "particletracer.h"
#include <algorithm>
#include <stdio.h>

#ifdef sun
#include "ieeefp.h"
#endif

// just use default rounding for platforms where its not available
#ifndef round
#define round(x) (x)
#endif

/******************************************************************************
 * Constructor
 *****************************************************************************/
IsoSurface::IsoSurface(double iso) :
	ntlGeometryObject(),
	mSizex(-1), mSizey(-1), mSizez(-1),
	mpData(NULL),
  mIsoValue( iso ), 
	mPoints(), 
	mUseFullEdgeArrays(false),
	mpEdgeVerticesX(NULL), mpEdgeVerticesY(NULL), mpEdgeVerticesZ(NULL),
	mEdgeArSize(-1),
	mIndices(),

  mStart(0.0), mEnd(0.0), mDomainExtent(0.0),
  mInitDone(false),
	mSmoothSurface(0.0), mSmoothNormals(0.0),
	mAcrossEdge(), mAdjacentFaces(),
	mCutoff(-1), mCutArray(NULL), // off by default
	mpIsoParts(NULL), mPartSize(0.), mSubdivs(0),
	mFlagCnt(1),
	mSCrad1(0.), mSCrad2(0.), mSCcenter(0.)
{
}


/******************************************************************************
 * The real init...
 *****************************************************************************/
void IsoSurface::initializeIsosurface(int setx, int sety, int setz, ntlVec3Gfx extent) 
{
	// range 1-10 (max due to subd array in triangulate)
	if(mSubdivs<1) mSubdivs=1;
	if(mSubdivs>10) mSubdivs=10;

	// init solver and size
	mSizex = setx;
	mSizey = sety;
	if(setz == 1) {// 2D, create thin 2D surface
		setz = 5; 
	}
	mSizez = setz;
	mDomainExtent = extent;

	/* check triangulation size (for raytraing) */
	if( ( mStart[0] >= mEnd[0] ) && ( mStart[1] >= mEnd[1] ) && ( mStart[2] >= mEnd[2] ) ){
		// extent was not set, use normalized one from parametrizer
		mStart = ntlVec3Gfx(0.0) - extent*0.5;
		mEnd = ntlVec3Gfx(0.0) + extent*0.5;
	}

  // init 
	mIndices.clear();
  mPoints.clear();

	int nodes = mSizez*mSizey*mSizex;
  mpData = new float[nodes];
  for(int i=0;i<nodes;i++) { mpData[i] = 0.0; }

  // allocate edge arrays  (last slices are never used...)
	int initsize = -1;
	if(mUseFullEdgeArrays) {
		mEdgeArSize = nodes;
		mpEdgeVerticesX = new int[nodes];
		mpEdgeVerticesY = new int[nodes];
		mpEdgeVerticesZ = new int[nodes];
		initsize = 3*nodes;
	} else {
		int sliceNodes = 2*mSizex*mSizey*mSubdivs*mSubdivs;
		mEdgeArSize = sliceNodes;
		mpEdgeVerticesX = new int[sliceNodes];
		mpEdgeVerticesY = new int[sliceNodes];
		mpEdgeVerticesZ = new int[sliceNodes];
		initsize = 3*sliceNodes;
	}
  for(int i=0;i<mEdgeArSize;i++) { mpEdgeVerticesX[i] = mpEdgeVerticesY[i] = mpEdgeVerticesZ[i] = -1; }
	// WARNING - make sure this is consistent with calculateMemreqEstimate
  
	// marching cubes are ready 
	mInitDone = true;
	debMsgStd("IsoSurface::initializeIsosurface",DM_MSG,"Inited, edgenodes:"<<initsize<<" subdivs:"<<mSubdivs<<" fulledg:"<<mUseFullEdgeArrays , 10);
}



/*! Reset all values */
void IsoSurface::resetAll(gfxReal val) {
	int nodes = mSizez*mSizey*mSizex;
  for(int i=0;i<nodes;i++) { mpData[i] = val; }
}


/******************************************************************************
 * Destructor
 *****************************************************************************/
IsoSurface::~IsoSurface( void )
{
	if(mpData) delete [] mpData;
	if(mpEdgeVerticesX) delete [] mpEdgeVerticesX;
	if(mpEdgeVerticesY) delete [] mpEdgeVerticesY;
	if(mpEdgeVerticesZ) delete [] mpEdgeVerticesZ;
}





/******************************************************************************
 * triangulate the scalar field given by pointer
 *****************************************************************************/
void IsoSurface::triangulate( void )
{
  double gsx,gsy,gsz; // grid spacing in x,y,z direction
  double px,py,pz;    // current position in grid in x,y,z direction
  IsoLevelCube cubie;    // struct for a small subcube
	myTime_t tritimestart = getTime(); 

	if(!mpData) {
		errFatal("IsoSurface::triangulate","no LBM object, and no scalar field...!",SIMWORLD_INITERROR);
		return;
	}

  // get grid spacing (-2 to have same spacing as sim)
  gsx = (mEnd[0]-mStart[0])/(double)(mSizex-2.0);
  gsy = (mEnd[1]-mStart[1])/(double)(mSizey-2.0);
  gsz = (mEnd[2]-mStart[2])/(double)(mSizez-2.0);

  // clean up previous frame
	mIndices.clear();
	mPoints.clear();

	// reset edge vertices
  for(int i=0;i<mEdgeArSize;i++) {
		mpEdgeVerticesX[i] = -1;
		mpEdgeVerticesY[i] = -1;
		mpEdgeVerticesZ[i] = -1;
	}

	ntlVec3Gfx pos[8];
	float value[8];
	int cubeIndex;      // index entry of the cube 
	int triIndices[12]; // vertex indices 
	int *eVert[12];
	IsoLevelVertex ilv;

	// edges between which points?
	const int mcEdges[24] = { 
		0,1,  1,2,  2,3,  3,0,
		4,5,  5,6,  6,7,  7,4,
		0,4,  1,5,  2,6,  3,7 };

	const int cubieOffsetX[8] = {
		0,1,1,0,  0,1,1,0 };
	const int cubieOffsetY[8] = {
		0,0,1,1,  0,0,1,1 };
	const int cubieOffsetZ[8] = {
		0,0,0,0,  1,1,1,1 };

	const int coAdd=2;
  // let the cubes march 
	if(mSubdivs<=1) {

		pz = mStart[2]-gsz*0.5;
		for(int k=1;k<(mSizez-2);k++) {
			pz += gsz;
			py = mStart[1]-gsy*0.5;
			for(int j=1;j<(mSizey-2);j++) {
				py += gsy;
				px = mStart[0]-gsx*0.5;
				for(int i=1;i<(mSizex-2);i++) {
					px += gsx;

					value[0] = *getData(i  ,j  ,k  );
					value[1] = *getData(i+1,j  ,k  );
					value[2] = *getData(i+1,j+1,k  );
					value[3] = *getData(i  ,j+1,k  );
					value[4] = *getData(i  ,j  ,k+1);
					value[5] = *getData(i+1,j  ,k+1);
					value[6] = *getData(i+1,j+1,k+1);
					value[7] = *getData(i  ,j+1,k+1);

					// check intersections of isosurface with edges, and calculate cubie index
					cubeIndex = 0;
					if (value[0] < mIsoValue) cubeIndex |= 1;
					if (value[1] < mIsoValue) cubeIndex |= 2;
					if (value[2] < mIsoValue) cubeIndex |= 4;
					if (value[3] < mIsoValue) cubeIndex |= 8;
					if (value[4] < mIsoValue) cubeIndex |= 16;
					if (value[5] < mIsoValue) cubeIndex |= 32;
					if (value[6] < mIsoValue) cubeIndex |= 64;
					if (value[7] < mIsoValue) cubeIndex |= 128;

					// No triangles to generate?
					if (mcEdgeTable[cubeIndex] == 0) {
						continue;
					}

					// where to look up if this point already exists
					int edgek = 0;
					if(mUseFullEdgeArrays) edgek=k;
					const int baseIn = ISOLEVEL_INDEX( i+0, j+0, edgek+0);
					eVert[ 0] = &mpEdgeVerticesX[ baseIn ];
					eVert[ 1] = &mpEdgeVerticesY[ baseIn + 1 ];
					eVert[ 2] = &mpEdgeVerticesX[ ISOLEVEL_INDEX( i+0, j+1, edgek+0) ];
					eVert[ 3] = &mpEdgeVerticesY[ baseIn ];

					eVert[ 4] = &mpEdgeVerticesX[ ISOLEVEL_INDEX( i+0, j+0, edgek+1) ];
					eVert[ 5] = &mpEdgeVerticesY[ ISOLEVEL_INDEX( i+1, j+0, edgek+1) ];
					eVert[ 6] = &mpEdgeVerticesX[ ISOLEVEL_INDEX( i+0, j+1, edgek+1) ];
					eVert[ 7] = &mpEdgeVerticesY[ ISOLEVEL_INDEX( i+0, j+0, edgek+1) ];

					eVert[ 8] = &mpEdgeVerticesZ[ baseIn ];
					eVert[ 9] = &mpEdgeVerticesZ[ ISOLEVEL_INDEX( i+1, j+0, edgek+0) ];
					eVert[10] = &mpEdgeVerticesZ[ ISOLEVEL_INDEX( i+1, j+1, edgek+0) ];
					eVert[11] = &mpEdgeVerticesZ[ ISOLEVEL_INDEX( i+0, j+1, edgek+0) ];

					// grid positions
					pos[0] = ntlVec3Gfx(px    ,py    ,pz);
					pos[1] = ntlVec3Gfx(px+gsx,py    ,pz);
					pos[2] = ntlVec3Gfx(px+gsx,py+gsy,pz);
					pos[3] = ntlVec3Gfx(px    ,py+gsy,pz);
					pos[4] = ntlVec3Gfx(px    ,py    ,pz+gsz);
					pos[5] = ntlVec3Gfx(px+gsx,py    ,pz+gsz);
					pos[6] = ntlVec3Gfx(px+gsx,py+gsy,pz+gsz);
					pos[7] = ntlVec3Gfx(px    ,py+gsy,pz+gsz);

					// check all edges
					for(int e=0;e<12;e++) {
						if (mcEdgeTable[cubeIndex] & (1<<e)) {
							// is the vertex already calculated?
							if(*eVert[ e ] < 0) {
								// interpolate edge
								const int e1 = mcEdges[e*2  ];
								const int e2 = mcEdges[e*2+1];
								const ntlVec3Gfx p1 = pos[ e1  ];    // scalar field pos 1
								const ntlVec3Gfx p2 = pos[ e2  ];    // scalar field pos 2
								const float valp1  = value[ e1  ];  // scalar field val 1
								const float valp2  = value[ e2  ];  // scalar field val 2
								const float mu = (mIsoValue - valp1) / (valp2 - valp1);

								// init isolevel vertex
								ilv.v = p1 + (p2-p1)*mu;
								ilv.n = getNormal( i+cubieOffsetX[e1], j+cubieOffsetY[e1], k+cubieOffsetZ[e1]) * (1.0-mu) +
												getNormal( i+cubieOffsetX[e2], j+cubieOffsetY[e2], k+cubieOffsetZ[e2]) * (    mu) ;
								mPoints.push_back( ilv );

								triIndices[e] = (mPoints.size()-1);
								// store vertex 
								*eVert[ e ] = triIndices[e];
							}	else {
								// retrieve  from vert array
								triIndices[e] = *eVert[ e ];
							}
						} // along all edges 
					}

					if( (i<coAdd+mCutoff) || (j<coAdd+mCutoff) ||
							((mCutoff>0) && (k<coAdd)) ||// bottom layer
							(i>mSizex-2-coAdd-mCutoff) ||
							(j>mSizey-2-coAdd-mCutoff) ) {
						if(mCutArray) {
							if(k < mCutArray[j*this->mSizex+i]) continue;
						} else { continue; }
					}

					// Create the triangles... 
					for(int e=0; mcTriTable[cubeIndex][e]!=-1; e+=3) {
						mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+0] ] );
						mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+1] ] );
						mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+2] ] );
					}
					
				}//i
			}// j

			// copy edge arrays
			if(!mUseFullEdgeArrays) {
			for(int j=0;j<(mSizey-0);j++) 
				for(int i=0;i<(mSizex-0);i++) {
					//int edgek = 0;
					const int dst = ISOLEVEL_INDEX( i+0, j+0, 0);
					const int src = ISOLEVEL_INDEX( i+0, j+0, 1);
					mpEdgeVerticesX[ dst ] = mpEdgeVerticesX[ src ];
					mpEdgeVerticesY[ dst ] = mpEdgeVerticesY[ src ];
					mpEdgeVerticesZ[ dst ] = mpEdgeVerticesZ[ src ];
					mpEdgeVerticesX[ src ]=-1;
					mpEdgeVerticesY[ src ]=-1;
					mpEdgeVerticesZ[ src ]=-1;
				}
			} // */

		} // k

  	// precalculate normals using an approximation of the scalar field gradient 
		for(int ni=0;ni<(int)mPoints.size();ni++) { normalize( mPoints[ni].n ); }

	} else { // subdivs

#define EDGEAR_INDEX(Ai,Aj,Ak, Bi,Bj) ((mSizex*mSizey*mSubdivs*mSubdivs*(Ak))+\
		(mSizex*mSubdivs*((Aj)*mSubdivs+(Bj)))+((Ai)*mSubdivs)+(Bi))

#define ISOTRILININT(fi,fj,fk) ( \
				(1.-(fi))*(1.-(fj))*(1.-(fk))*orgval[0] + \
				(   (fi))*(1.-(fj))*(1.-(fk))*orgval[1] + \
				(   (fi))*(   (fj))*(1.-(fk))*orgval[2] + \
				(1.-(fi))*(   (fj))*(1.-(fk))*orgval[3] + \
				(1.-(fi))*(1.-(fj))*(   (fk))*orgval[4] + \
				(   (fi))*(1.-(fj))*(   (fk))*orgval[5] + \
				(   (fi))*(   (fj))*(   (fk))*orgval[6] + \
				(1.-(fi))*(   (fj))*(   (fk))*orgval[7] )

		// use subdivisions
		gfxReal subdfac = 1./(gfxReal)(mSubdivs);
		gfxReal orgGsx = gsx;
		gfxReal orgGsy = gsy;
		gfxReal orgGsz = gsz;
		gsx *= subdfac;
		gsy *= subdfac;
		gsz *= subdfac;
		if(mUseFullEdgeArrays) {
			errMsg("IsoSurface::triangulate","Disabling mUseFullEdgeArrays!");
		}

		// subdiv local arrays
		gfxReal orgval[8];
		gfxReal subdAr[2][11][11]; // max 10 subdivs!
		ParticleObject* *arppnt = new ParticleObject*[mSizez*mSizey*mSizex];

		// construct pointers
		// part test
		int pInUse = 0;
		int pUsedTest = 0;
		// reset particles
		// reset list array
		for(int k=0;k<(mSizez);k++) 
			for(int j=0;j<(mSizey);j++) 
				for(int i=0;i<(mSizex);i++) {
					arppnt[ISOLEVEL_INDEX(i,j,k)] = NULL;
				}
		if(mpIsoParts) {
			for(vector<ParticleObject>::iterator pit= mpIsoParts->getParticlesBegin();
					pit!= mpIsoParts->getParticlesEnd(); pit++) {
				if( (*pit).getActive()==false ) continue;
				if( (*pit).getType()!=PART_DROP) continue;
				(*pit).setNext(NULL);
			}
			// build per node lists
			for(vector<ParticleObject>::iterator pit= mpIsoParts->getParticlesBegin();
					pit!= mpIsoParts->getParticlesEnd(); pit++) {
				if( (*pit).getActive()==false ) continue;
				if( (*pit).getType()!=PART_DROP) continue;
				// check lifetime ignored here
				ParticleObject *p = &(*pit);
				const ntlVec3Gfx ppos = p->getPos();
				const int pi= (int)round(ppos[0])+0; 
				const int pj= (int)round(ppos[1])+0; 
				int       pk= (int)round(ppos[2])+0;// no offset necessary
				// 2d should be handled by solver. if(LBMDIM==2) { pk = 0; }

				if(pi<0) continue;
				if(pj<0) continue;
				if(pk<0) continue;
				if(pi>mSizex-1) continue;
				if(pj>mSizey-1) continue;
				if(pk>mSizez-1) continue;
				ParticleObject* &pnt = arppnt[ISOLEVEL_INDEX(pi,pj,pk)]; 
				if(pnt) {
					// append
					ParticleObject* listpnt = pnt;
					while(listpnt) {
						if(!listpnt->getNext()) {
							listpnt->setNext(p); listpnt = NULL;
						} else {
							listpnt = listpnt->getNext();
						}
					}
				} else {
					// start new list
					pnt = p;
				}
				pInUse++;
			}
		} // mpIsoParts

		debMsgStd("IsoSurface::triangulate",DM_MSG,"Starting. Parts in use:"<<pInUse<<", Subdivs:"<<mSubdivs, 9);
		pz = mStart[2]-(double)(0.*gsz)-0.5*orgGsz;
		for(int ok=1;ok<(mSizez-2)*mSubdivs;ok++) {
			pz += gsz;
			const int k = ok/mSubdivs;
			if(k<=0) continue; // skip zero plane
			for(int j=1;j<(mSizey-2);j++) {
				for(int i=1;i<(mSizex-2);i++) {

					orgval[0] = *getData(i  ,j  ,k  );
					orgval[1] = *getData(i+1,j  ,k  );
					orgval[2] = *getData(i+1,j+1,k  ); // with subdivs
					orgval[3] = *getData(i  ,j+1,k  );
					orgval[4] = *getData(i  ,j  ,k+1);
					orgval[5] = *getData(i+1,j  ,k+1);
					orgval[6] = *getData(i+1,j+1,k+1); // with subdivs
					orgval[7] = *getData(i  ,j+1,k+1);

					// prebuild subsampled array slice
					const int sdkOffset = ok-k*mSubdivs; 
					for(int sdk=0; sdk<2; sdk++) 
						for(int sdj=0; sdj<mSubdivs+1; sdj++) 
							for(int sdi=0; sdi<mSubdivs+1; sdi++) {
								subdAr[sdk][sdj][sdi] = ISOTRILININT(sdi*subdfac, sdj*subdfac, (sdkOffset+sdk)*subdfac);
							}

					const int poDistOffset=2;
					for(int pok=-poDistOffset; pok<1+poDistOffset; pok++) {
						if(k+pok<0) continue;
						if(k+pok>=mSizez-1) continue;
					for(int poj=-poDistOffset; poj<1+poDistOffset; poj++) {
						if(j+poj<0) continue;
						if(j+poj>=mSizey-1) continue;
					for(int poi=-poDistOffset; poi<1+poDistOffset; poi++) {
						if(i+poi<0) continue;
						if(i+poi>=mSizex-1) continue; 
						ParticleObject *p;
						p = arppnt[ISOLEVEL_INDEX(i+poi,j+poj,k+pok)];
						while(p) { // */
					/*
					for(vector<ParticleObject>::iterator pit= mpIsoParts->getParticlesBegin();
							pit!= mpIsoParts->getParticlesEnd(); pit++) { { { {
						// debug test! , full list slow!
						if(( (*pit).getActive()==false ) || ( (*pit).getType()!=PART_DROP)) continue;
						ParticleObject *p;
						p = &(*pit); // */

							pUsedTest++;
							ntlVec3Gfx ppos = p->getPos();
							const int spi= (int)round( (ppos[0]+1.-(gfxReal)i) *(gfxReal)mSubdivs-1.5); 
							const int spj= (int)round( (ppos[1]+1.-(gfxReal)j) *(gfxReal)mSubdivs-1.5); 
							const int spk= (int)round( (ppos[2]+1.-(gfxReal)k) *(gfxReal)mSubdivs-1.5)-sdkOffset; // why -2?
							// 2d should be handled by solver. if(LBMDIM==2) { spk = 0; }

							gfxReal pfLen = p->getSize()*1.5*mPartSize;  // test, was 1.1
							const gfxReal minPfLen = subdfac*0.8;
							if(pfLen<minPfLen) pfLen = minPfLen;
							//errMsg("ISOPPP"," at "<<PRINT_IJK<<"  pp"<<ppos<<"  sp"<<PRINT_VEC(spi,spj,spk)<<" pflen"<<pfLen );
							//errMsg("ISOPPP"," subdfac="<<subdfac<<" size"<<p->getSize()<<" ps"<<mPartSize );
							const int icellpsize = (int)(1.*pfLen*(gfxReal)mSubdivs)+1;
							for(int swk=-icellpsize; swk<=icellpsize; swk++) {
								if(spk+swk<         0) { continue; }
								if(spk+swk>         1) { continue; } // */
							for(int swj=-icellpsize; swj<=icellpsize; swj++) {
								if(spj+swj<         0) { continue; }
								if(spj+swj>mSubdivs+0) { continue; } // */
							for(int swi=-icellpsize; swi<=icellpsize; swi++) {
								if(spi+swi<         0) { continue; } 
								if(spi+swi>mSubdivs+0) { continue; } // */
								ntlVec3Gfx cellp = ntlVec3Gfx(
										(1.5+(gfxReal)(spi+swi))           *subdfac + (gfxReal)(i-1),
										(1.5+(gfxReal)(spj+swj))           *subdfac + (gfxReal)(j-1),
										(1.5+(gfxReal)(spk+swk)+sdkOffset) *subdfac + (gfxReal)(k-1)
										);
								//if(swi==0 && swj==0 && swk==0) subdAr[spk][spj][spi] = 1.; // DEBUG
								// clip domain boundaries again 
								if(cellp[0]<1.) { continue; } 
								if(cellp[1]<1.) { continue; } 
								if(cellp[2]<1.) { continue; } 
								if(cellp[0]>(gfxReal)mSizex-3.) { continue; } 
								if(cellp[1]>(gfxReal)mSizey-3.) { continue; } 
								if(cellp[2]>(gfxReal)mSizez-3.) { continue; } 
								gfxReal len = norm(cellp-ppos);
								gfxReal isoadd = 0.; 
								const gfxReal baseIsoVal = mIsoValue*1.1;
								if(len<pfLen) { 
									isoadd = baseIsoVal*1.;
								} else { 
									// falloff linear with pfLen (kernel size=2pfLen
									isoadd = baseIsoVal*(1. - (len-pfLen)/(pfLen)); 
								}
								if(isoadd<0.) { continue; }
								//errMsg("ISOPPP"," at "<<PRINT_IJK<<" sp"<<PRINT_VEC(spi+swi,spj+swj,spk+swk)<<" cellp"<<cellp<<" pp"<<ppos << " l"<< len<< " add"<< isoadd);
								const gfxReal arval = subdAr[spk+swk][spj+swj][spi+swi];
								if(arval>1.) { continue; }
								subdAr[spk+swk][spj+swj][spi+swi] = arval + isoadd;
							} } }

							p = p->getNext();
						}
					} } } // poDist loops */

					py = mStart[1]+(((double)j-0.5)*orgGsy)-gsy;
					for(int sj=0;sj<mSubdivs;sj++) {
						py += gsy;
						px = mStart[0]+(((double)i-0.5)*orgGsx)-gsx;
						for(int si=0;si<mSubdivs;si++) {
							px += gsx;
							value[0] = subdAr[0+0][sj+0][si+0]; 
							value[1] = subdAr[0+0][sj+0][si+1]; 
							value[2] = subdAr[0+0][sj+1][si+1]; 
							value[3] = subdAr[0+0][sj+1][si+0]; 
							value[4] = subdAr[0+1][sj+0][si+0]; 
							value[5] = subdAr[0+1][sj+0][si+1]; 
							value[6] = subdAr[0+1][sj+1][si+1]; 
							value[7] = subdAr[0+1][sj+1][si+0]; 

							// check intersections of isosurface with edges, and calculate cubie index
							cubeIndex = 0;
							if (value[0] < mIsoValue) cubeIndex |= 1;
							if (value[1] < mIsoValue) cubeIndex |= 2; // with subdivs
							if (value[2] < mIsoValue) cubeIndex |= 4;
							if (value[3] < mIsoValue) cubeIndex |= 8;
							if (value[4] < mIsoValue) cubeIndex |= 16;
							if (value[5] < mIsoValue) cubeIndex |= 32; // with subdivs
							if (value[6] < mIsoValue) cubeIndex |= 64;
							if (value[7] < mIsoValue) cubeIndex |= 128;

							if (mcEdgeTable[cubeIndex] >  0) {

							// where to look up if this point already exists
							const int edgek = 0;
							const int baseIn = EDGEAR_INDEX( i+0, j+0, edgek+0, si,sj);
							eVert[ 0] = &mpEdgeVerticesX[ baseIn ];
							eVert[ 1] = &mpEdgeVerticesY[ baseIn + 1 ];
							eVert[ 2] = &mpEdgeVerticesX[ EDGEAR_INDEX( i, j, edgek+0, si+0,sj+1) ];
							eVert[ 3] = &mpEdgeVerticesY[ baseIn ];                             
																																									
							eVert[ 4] = &mpEdgeVerticesX[ EDGEAR_INDEX( i, j, edgek+1, si+0,sj+0) ];
							eVert[ 5] = &mpEdgeVerticesY[ EDGEAR_INDEX( i, j, edgek+1, si+1,sj+0) ]; // with subdivs
							eVert[ 6] = &mpEdgeVerticesX[ EDGEAR_INDEX( i, j, edgek+1, si+0,sj+1) ];
							eVert[ 7] = &mpEdgeVerticesY[ EDGEAR_INDEX( i, j, edgek+1, si+0,sj+0) ];
																																									
							eVert[ 8] = &mpEdgeVerticesZ[ baseIn ];                             
							eVert[ 9] = &mpEdgeVerticesZ[ EDGEAR_INDEX( i, j, edgek+0, si+1,sj+0) ]; // with subdivs
							eVert[10] = &mpEdgeVerticesZ[ EDGEAR_INDEX( i, j, edgek+0, si+1,sj+1) ];
							eVert[11] = &mpEdgeVerticesZ[ EDGEAR_INDEX( i, j, edgek+0, si+0,sj+1) ];

							// grid positions
							pos[0] = ntlVec3Gfx(px    ,py    ,pz);
							pos[1] = ntlVec3Gfx(px+gsx,py    ,pz);
							pos[2] = ntlVec3Gfx(px+gsx,py+gsy,pz); // with subdivs
							pos[3] = ntlVec3Gfx(px    ,py+gsy,pz);
							pos[4] = ntlVec3Gfx(px    ,py    ,pz+gsz);
							pos[5] = ntlVec3Gfx(px+gsx,py    ,pz+gsz);
							pos[6] = ntlVec3Gfx(px+gsx,py+gsy,pz+gsz); // with subdivs
							pos[7] = ntlVec3Gfx(px    ,py+gsy,pz+gsz);

							// check all edges
							for(int e=0;e<12;e++) {
								if (mcEdgeTable[cubeIndex] & (1<<e)) {
									// is the vertex already calculated?
									if(*eVert[ e ] < 0) {
										// interpolate edge
										const int e1 = mcEdges[e*2  ];
										const int e2 = mcEdges[e*2+1];
										const ntlVec3Gfx p1 = pos[ e1  ];   // scalar field pos 1
										const ntlVec3Gfx p2 = pos[ e2  ];   // scalar field pos 2
										const float valp1  = value[ e1  ];  // scalar field val 1
										const float valp2  = value[ e2  ];  // scalar field val 2
										const float mu = (mIsoValue - valp1) / (valp2 - valp1);

										// init isolevel vertex
										ilv.v = p1 + (p2-p1)*mu; // with subdivs
										mPoints.push_back( ilv );
										triIndices[e] = (mPoints.size()-1);
										// store vertex 
										*eVert[ e ] = triIndices[e]; 
									}	else {
										// retrieve  from vert array
										triIndices[e] = *eVert[ e ];
									}
								} // along all edges 
							}
							// removed cutoff treatment...

							// Create the triangles... 
							for(int e=0; mcTriTable[cubeIndex][e]!=-1; e+=3) {
								mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+0] ] );
								mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+1] ] ); // with subdivs
								mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+2] ] );
								//errMsg("TTT"," i1"<<mIndices[mIndices.size()-3]<<" "<< " i2"<<mIndices[mIndices.size()-2]<<" "<< " i3"<<mIndices[mIndices.size()-1]<<" "<< mIndices.size() );
							}

							} // triangles in edge table?
							
						}//si
					}// sj

				}//i
			}// j

			// copy edge arrays
			for(int j=0;j<(mSizey-0)*mSubdivs;j++) 
				for(int i=0;i<(mSizex-0)*mSubdivs;i++) {
					//int edgek = 0;
					const int dst = EDGEAR_INDEX( 0, 0, 0, i,j);
					const int src = EDGEAR_INDEX( 0, 0, 1, i,j);
					mpEdgeVerticesX[ dst ] = mpEdgeVerticesX[ src ];
					mpEdgeVerticesY[ dst ] = mpEdgeVerticesY[ src ]; // with subdivs
					mpEdgeVerticesZ[ dst ] = mpEdgeVerticesZ[ src ];
					mpEdgeVerticesX[ src ]=-1;
					mpEdgeVerticesY[ src ]=-1; // with subdivs
					mpEdgeVerticesZ[ src ]=-1;
				}
			// */

		} // ok, k subdiv loop

		//delete [] subdAr;
		delete [] arppnt;
		computeNormals();
	} // with subdivs

	// perform smoothing
	float smoSubdfac = 1.;
	if(mSubdivs>0) {
		//smoSubdfac = 1./(float)(mSubdivs);
		smoSubdfac = pow(0.55,(double)mSubdivs); // slightly stronger
	}
	if(mSmoothSurface>0. || mSmoothNormals>0.) debMsgStd("IsoSurface::triangulate",DM_MSG,"Smoothing...",10);
	if(mSmoothSurface>0.0) { 
		smoothSurface(mSmoothSurface*smoSubdfac, (mSmoothNormals<=0.0) ); 
	}
	if(mSmoothNormals>0.0) { 
		smoothNormals(mSmoothNormals*smoSubdfac); 
	}

	myTime_t tritimeend = getTime(); 
	debMsgStd("IsoSurface::triangulate",DM_MSG,"took "<< getTimeString(tritimeend-tritimestart)<<", S("<<mSmoothSurface<<","<<mSmoothNormals<<"),"<<
			" verts:"<<mPoints.size()<<" tris:"<<(mIndices.size()/3)<<" subdivs:"<<mSubdivs
		 , 10 );
	if(mpIsoParts) debMsgStd("IsoSurface::triangulate",DM_MSG,"parts:"<<mpIsoParts->getNumParticles(), 10);
}


	


/******************************************************************************
 * Get triangles for rendering
 *****************************************************************************/
void IsoSurface::getTriangles(double t, vector<ntlTriangle> *triangles, 
													 vector<ntlVec3Gfx> *vertices, 
													 vector<ntlVec3Gfx> *normals, int objectId )
{
	if(!mInitDone) {
		debugOut("IsoSurface::getTriangles warning: Not initialized! ", 10);
		return;
	}
	t = 0.;
	//return; // DEBUG

  /* triangulate field */
  triangulate();
	//errMsg("TRIS"," "<<mIndices.size() );

	// new output with vertice reuse
	int iniVertIndex = (*vertices).size();
	int iniNormIndex = (*normals).size();
	if(iniVertIndex != iniNormIndex) {
		errFatal("getTriangles Error","For '"<<mName<<"': Vertices and normal array sizes to not match!!!",SIMWORLD_GENERICERROR);
		return; 
	}
	//errMsg("NM"," ivi"<<iniVertIndex<<" ini"<<iniNormIndex<<" vs"<<vertices->size()<<" ns"<<normals->size()<<" ts"<<triangles->size() );
	//errMsg("NM"," ovs"<<mVertices.size()<<" ons"<<mVertNormals.size()<<" ots"<<mIndices.size() );

  for(int i=0;i<(int)mPoints.size();i++) {
		vertices->push_back( mPoints[i].v );
	}
  for(int i=0;i<(int)mPoints.size();i++) {
		normals->push_back( mPoints[i].n );
	}

	//errMsg("N2"," ivi"<<iniVertIndex<<" ini"<<iniNormIndex<<" vs"<<vertices->size()<<" ns"<<normals->size()<<" ts"<<triangles->size() );
	//errMsg("N2"," ovs"<<mVertices.size()<<" ons"<<mVertNormals.size()<<" ots"<<mIndices.size() );

  for(int i=0;i<(int)mIndices.size();i+=3) {
		const int smooth = 1;
    int t1 = mIndices[i];
    int t2 = mIndices[i+1];
		int t3 = mIndices[i+2];
		//errMsg("NM"," tri"<<t1<<" "<<t2<<" "<<t3 );

		ntlTriangle tri;

		tri.getPoints()[0] = t1+iniVertIndex;
		tri.getPoints()[1] = t2+iniVertIndex;
		tri.getPoints()[2] = t3+iniVertIndex;

		/* init flags */
		int flag = 0; 
		if(getVisible()){ flag |= TRI_GEOMETRY; }
		if(getCastShadows() ) { 
			flag |= TRI_CASTSHADOWS; } 

		/* init geo init id */
		int geoiId = getGeoInitId(); 
		if(geoiId > 0) { 
			flag |= (1<< (geoiId+4)); 
			flag |= mGeoInitType; 
		} 

		tri.setFlags( flag );

		/* triangle normal missing */
		tri.setNormal( ntlVec3Gfx(0.0) );
		tri.setSmoothNormals( smooth );
		tri.setObjectId( objectId );
		triangles->push_back( tri ); 
	}
	//errMsg("N3"," ivi"<<iniVertIndex<<" ini"<<iniNormIndex<<" vs"<<vertices->size()<<" ns"<<normals->size()<<" ts"<<triangles->size() );
	return;
}



inline ntlVec3Gfx IsoSurface::getNormal(int i, int j,int k) {
	// WARNING - this requires a security boundary layer... 
	ntlVec3Gfx ret(0.0);
	ret[0] = *getData(i-1,j  ,k  ) - 
	         *getData(i+1,j  ,k  );
	ret[1] = *getData(i  ,j-1,k  ) - 
	         *getData(i  ,j+1,k  );
	ret[2] = *getData(i  ,j  ,k-1  ) - 
	         *getData(i  ,j  ,k+1  );
	return ret;
}




/******************************************************************************
 * 
 * Surface improvement, inspired by trimesh2 library
 * (http://www.cs.princeton.edu/gfx/proj/trimesh2/)
 * 
 *****************************************************************************/

void IsoSurface::setSmoothRad(float radi1, float radi2, ntlVec3Gfx mscc) {
	mSCrad1 = radi1*radi1;
	mSCrad2 = radi2*radi2;
	mSCcenter = mscc;
}

// compute normals for all generated triangles
void IsoSurface::computeNormals() {
  for(int i=0;i<(int)mPoints.size();i++) {
		mPoints[i].n = ntlVec3Gfx(0.);
	}

  for(int i=0;i<(int)mIndices.size();i+=3) {
    const int t1 = mIndices[i];
    const int t2 = mIndices[i+1];
		const int t3 = mIndices[i+2];
		const ntlVec3Gfx p1 = mPoints[t1].v;
		const ntlVec3Gfx p2 = mPoints[t2].v;
		const ntlVec3Gfx p3 = mPoints[t3].v;
		const ntlVec3Gfx n1=p1-p2;
		const ntlVec3Gfx n2=p2-p3;
		const ntlVec3Gfx n3=p3-p1;
		const gfxReal len1 = normNoSqrt(n1);
		const gfxReal len2 = normNoSqrt(n2);
		const gfxReal len3 = normNoSqrt(n3);
		const ntlVec3Gfx norm = cross(n1,n2);
		mPoints[t1].n += norm * (1./(len1*len3));
		mPoints[t2].n += norm * (1./(len1*len2));
		mPoints[t3].n += norm * (1./(len2*len3));
	}

  for(int i=0;i<(int)mPoints.size();i++) {
		normalize(mPoints[i].n);
	}
}

// Diffuse a vector field at 1 vertex, weighted by
// a gaussian of width 1/sqrt(invsigma2)
bool IsoSurface::diffuseVertexField(ntlVec3Gfx *field, const int pointerScale, int src, float invsigma2, ntlVec3Gfx &target)
{
	if((neighbors[src].size()<1) || (pointareas[src]<=0.0)) return 0;
	const ntlVec3Gfx srcp = mPoints[src].v;
	const ntlVec3Gfx srcn = mPoints[src].n;
	if(mSCrad1>0.0 && mSCrad2>0.0) {
		ntlVec3Gfx dp = mSCcenter-srcp; dp[2] = 0.0; // only xy-plane
		float rd = normNoSqrt(dp);
		if(rd > mSCrad2) {
			return 0;
		} else if(rd > mSCrad1) {
			// optimize?
			float org = 1.0/sqrt(invsigma2);
			org *= (1.0- (rd-mSCrad1) / (mSCrad2-mSCrad1));
			invsigma2 = 1.0/(org*org);
			//errMsg("TRi","p"<<srcp<<" rd:"<<rd<<" r1:"<<mSCrad1<<" r2:"<<mSCrad2<<" org:"<<org<<" is:"<<invsigma2);
		} else {
		}
	}
	target = ntlVec3Gfx(0.0);
	target += *(field+pointerScale*src) *pointareas[src];
	float smstrSum = pointareas[src];

	int flag = mFlagCnt; 
	mFlagCnt++;
	flags[src] = flag;
	mDboundary = neighbors[src];
	while (!mDboundary.empty()) {
		const int bbn = mDboundary.back();
		mDboundary.pop_back();
		if(flags[bbn]==flag) continue;
		flags[bbn] = flag;

		// normal check
		const float nvdot = dot(srcn, mPoints[bbn].n); // faster than before d2 calc?
		if(nvdot <= 0.0f) continue;

		// gaussian weight of width 1/sqrt(invsigma2)
		const float d2 = invsigma2 * normNoSqrt(mPoints[bbn].v - srcp);
		if(d2 >= 9.0f) continue;

		// aggressive smoothing factor
		float smstr = nvdot * pointareas[bbn];
		// Accumulate weight times field at neighbor
		target += *(field+pointerScale*bbn)*smstr;
		smstrSum += smstr;

		for(int i = 0; i < (int)neighbors[bbn].size(); i++) {
			const int nn = neighbors[bbn][i];
			if (flags[nn] == flag) continue;
			mDboundary.push_back(nn);
		}
	}
	target /= smstrSum;
	return 1;
}

	
// perform smoothing of the surface (and possible normals)
void IsoSurface::smoothSurface(float sigma, bool normSmooth)
{
	int nv = mPoints.size();
	if ((int)flags.size() != nv) flags.resize(nv);
	int nf = mIndices.size()/3;

	{ // need neighbors
		vector<int> numneighbors(mPoints.size());
		int i;
		for (i = 0; i < (int)mIndices.size()/3; i++) {
			numneighbors[mIndices[i*3+0]]++;
			numneighbors[mIndices[i*3+1]]++;
			numneighbors[mIndices[i*3+2]]++;
		}

		neighbors.clear();
		neighbors.resize(mPoints.size());
		for (i = 0; i < (int)mPoints.size(); i++) {
			neighbors[i].clear();
			neighbors[i].reserve(numneighbors[i]+2); // Slop for boundaries
		}

		for (i = 0; i < (int)mIndices.size()/3; i++) {
			for (int j = 0; j < 3; j++) {
				vector<int> &me = neighbors[ mIndices[i*3+j]];
				int n1 =  mIndices[i*3+((j+1)%3)];
				int n2 =  mIndices[i*3+((j+2)%3)];
				if (std::find(me.begin(), me.end(), n1) == me.end())
					me.push_back(n1);
				if (std::find(me.begin(), me.end(), n2) == me.end())
					me.push_back(n2);
			}
		}
	} // need neighbor

	{ // need pointarea
		pointareas.clear();
		pointareas.resize(nv);
		cornerareas.clear();
		cornerareas.resize(nf);

		for (int i = 0; i < nf; i++) {
			// Edges
			ntlVec3Gfx e[3] = { 
				mPoints[mIndices[i*3+2]].v - mPoints[mIndices[i*3+1]].v,
				mPoints[mIndices[i*3+0]].v - mPoints[mIndices[i*3+2]].v,
				mPoints[mIndices[i*3+1]].v - mPoints[mIndices[i*3+0]].v };

			// Compute corner weights
			float area = 0.5f * norm( cross(e[0], e[1]));
			float l2[3] = { normNoSqrt(e[0]), normNoSqrt(e[1]), normNoSqrt(e[2]) };
			float ew[3] = { l2[0] * (l2[1] + l2[2] - l2[0]),
					l2[1] * (l2[2] + l2[0] - l2[1]),
					l2[2] * (l2[0] + l2[1] - l2[2]) };
			if (ew[0] <= 0.0f) {
				cornerareas[i][1] = -0.25f * l2[2] * area /
								dot(e[0] , e[2]);
				cornerareas[i][2] = -0.25f * l2[1] * area /
								dot(e[0] , e[1]);
				cornerareas[i][0] = area - cornerareas[i][1] -
								cornerareas[i][2];
			} else if (ew[1] <= 0.0f) {
				cornerareas[i][2] = -0.25f * l2[0] * area /
								dot(e[1] , e[0]);
				cornerareas[i][0] = -0.25f * l2[2] * area /
								dot(e[1] , e[2]);
				cornerareas[i][1] = area - cornerareas[i][2] -
								cornerareas[i][0];
			} else if (ew[2] <= 0.0f) {
				cornerareas[i][0] = -0.25f * l2[1] * area /
								dot(e[2] , e[1]);
				cornerareas[i][1] = -0.25f * l2[0] * area /
								dot(e[2] , e[0]);
				cornerareas[i][2] = area - cornerareas[i][0] -
								cornerareas[i][1];
			} else {
				float ewscale = 0.5f * area / (ew[0] + ew[1] + ew[2]);
				for (int j = 0; j < 3; j++)
					cornerareas[i][j] = ewscale * (ew[(j+1)%3] +
											 ew[(j+2)%3]);
			}

			// NT important, check this...
#ifndef WIN32
			if(! finite(cornerareas[i][0]) ) cornerareas[i][0]=1e-6;
			if(! finite(cornerareas[i][1]) ) cornerareas[i][1]=1e-6;
			if(! finite(cornerareas[i][2]) ) cornerareas[i][2]=1e-6;
#else // WIN32
			// FIXME check as well...
			if(! (cornerareas[i][0]>=0.0) ) cornerareas[i][0]=1e-6;
			if(! (cornerareas[i][1]>=0.0) ) cornerareas[i][1]=1e-6;
			if(! (cornerareas[i][2]>=0.0) ) cornerareas[i][2]=1e-6;
#endif // WIN32

			pointareas[mIndices[i*3+0]] += cornerareas[i][0];
			pointareas[mIndices[i*3+1]] += cornerareas[i][1];
			pointareas[mIndices[i*3+2]] += cornerareas[i][2];
		}

	} // need pointarea
 	// */

	float invsigma2 = 1.0f / (sigma*sigma);

	vector<ntlVec3Gfx> dflt(nv);
	for (int i = 0; i < nv; i++) {
		if(diffuseVertexField( &mPoints[0].v, 2,
				   i, invsigma2, dflt[i])) {
			// Just keep the displacement
			dflt[i] -= mPoints[i].v;
		} else { dflt[i] = 0.0; } //?mPoints[i].v; }
	}

	// Slightly better small-neighborhood approximation
	for (int i = 0; i < nf; i++) {
		ntlVec3Gfx c = mPoints[mIndices[i*3+0]].v +
			  mPoints[mIndices[i*3+1]].v +
			  mPoints[mIndices[i*3+2]].v;
		c /= 3.0f;
		for (int j = 0; j < 3; j++) {
			int v = mIndices[i*3+j];
			ntlVec3Gfx d =(c - mPoints[v].v) * 0.5f;
			dflt[v] += d * (cornerareas[i][j] /
				   pointareas[mIndices[i*3+j]] *
				   exp(-0.5f * invsigma2 * normNoSqrt(d)) );
		}
	}

	// Filter displacement field
	vector<ntlVec3Gfx> dflt2(nv);
	for (int i = 0; i < nv; i++) {
		if(diffuseVertexField( &dflt[0], 1,
				   i, invsigma2, dflt2[i])) { }
		else { /*mPoints[i].v=0.0;*/ dflt2[i] = 0.0; }//dflt2[i]; }
	}

	// Update vertex positions
	for (int i = 0; i < nv; i++) {
		mPoints[i].v += dflt[i] - dflt2[i]; // second Laplacian
	}

	// when normals smoothing off, this cleans up quite well
	// costs ca. 50% additional time though
	float nsFac = 1.5f;
	if(normSmooth) { float ninvsigma2 = 1.0f / (nsFac*nsFac*sigma*sigma);
		for (int i = 0; i < nv; i++) {
			if( diffuseVertexField( &mPoints[0].n, 2, i, ninvsigma2, dflt[i]) ) {
				normalize(dflt[i]);
			} else {
				dflt[i] = mPoints[i].n;
			}
		}
		for (int i = 0; i < nv; i++) {
			mPoints[i].n = dflt[i];
		} 
	} // smoothNormals copy */

	//errMsg("SMSURF","done v:"<<sigma); // DEBUG
}

// only smoothen the normals
void IsoSurface::smoothNormals(float sigma) {
	// reuse from smoothSurface
	if(neighbors.size() != mPoints.size()) { 
		// need neighbor
		vector<int> numneighbors(mPoints.size());
		int i;
		for (i = 0; i < (int)mIndices.size()/3; i++) {
			numneighbors[mIndices[i*3+0]]++;
			numneighbors[mIndices[i*3+1]]++;
			numneighbors[mIndices[i*3+2]]++;
		}

		neighbors.clear();
		neighbors.resize(mPoints.size());
		for (i = 0; i < (int)mPoints.size(); i++) {
			neighbors[i].clear();
			neighbors[i].reserve(numneighbors[i]+2); // Slop for boundaries
		}

		for (i = 0; i < (int)mIndices.size()/3; i++) {
			for (int j = 0; j < 3; j++) {
				vector<int> &me = neighbors[ mIndices[i*3+j]];
				int n1 =  mIndices[i*3+((j+1)%3)];
				int n2 =  mIndices[i*3+((j+2)%3)];
				if (std::find(me.begin(), me.end(), n1) == me.end())
					me.push_back(n1);
				if (std::find(me.begin(), me.end(), n2) == me.end())
					me.push_back(n2);
			}
		}
	} // need neighbor

	{ // need pointarea
		int nf = mIndices.size()/3, nv = mPoints.size();
		pointareas.clear();
		pointareas.resize(nv);
		cornerareas.clear();
		cornerareas.resize(nf);

		for (int i = 0; i < nf; i++) {
			// Edges
			ntlVec3Gfx e[3] = { 
				mPoints[mIndices[i*3+2]].v - mPoints[mIndices[i*3+1]].v,
				mPoints[mIndices[i*3+0]].v - mPoints[mIndices[i*3+2]].v,
				mPoints[mIndices[i*3+1]].v - mPoints[mIndices[i*3+0]].v };

			// Compute corner weights
			float area = 0.5f * norm( cross(e[0], e[1]));
			float l2[3] = { normNoSqrt(e[0]), normNoSqrt(e[1]), normNoSqrt(e[2]) };
			float ew[3] = { l2[0] * (l2[1] + l2[2] - l2[0]),
					l2[1] * (l2[2] + l2[0] - l2[1]),
					l2[2] * (l2[0] + l2[1] - l2[2]) };
			if (ew[0] <= 0.0f) {
				cornerareas[i][1] = -0.25f * l2[2] * area /
								dot(e[0] , e[2]);
				cornerareas[i][2] = -0.25f * l2[1] * area /
								dot(e[0] , e[1]);
				cornerareas[i][0] = area - cornerareas[i][1] -
								cornerareas[i][2];
			} else if (ew[1] <= 0.0f) {
				cornerareas[i][2] = -0.25f * l2[0] * area /
								dot(e[1] , e[0]);
				cornerareas[i][0] = -0.25f * l2[2] * area /
								dot(e[1] , e[2]);
				cornerareas[i][1] = area - cornerareas[i][2] -
								cornerareas[i][0];
			} else if (ew[2] <= 0.0f) {
				cornerareas[i][0] = -0.25f * l2[1] * area /
								dot(e[2] , e[1]);
				cornerareas[i][1] = -0.25f * l2[0] * area /
								dot(e[2] , e[0]);
				cornerareas[i][2] = area - cornerareas[i][0] -
								cornerareas[i][1];
			} else {
				float ewscale = 0.5f * area / (ew[0] + ew[1] + ew[2]);
				for (int j = 0; j < 3; j++)
					cornerareas[i][j] = ewscale * (ew[(j+1)%3] +
											 ew[(j+2)%3]);
			}

			// NT important, check this...
#ifndef WIN32
			if(! finite(cornerareas[i][0]) ) cornerareas[i][0]=1e-6;
			if(! finite(cornerareas[i][1]) ) cornerareas[i][1]=1e-6;
			if(! finite(cornerareas[i][2]) ) cornerareas[i][2]=1e-6;
#else // WIN32
			// FIXME check as well...
			if(! (cornerareas[i][0]>=0.0) ) cornerareas[i][0]=1e-6;
			if(! (cornerareas[i][1]>=0.0) ) cornerareas[i][1]=1e-6;
			if(! (cornerareas[i][2]>=0.0) ) cornerareas[i][2]=1e-6;
#endif // WIN32

			pointareas[mIndices[i*3+0]] += cornerareas[i][0];
			pointareas[mIndices[i*3+1]] += cornerareas[i][1];
			pointareas[mIndices[i*3+2]] += cornerareas[i][2];
		}

	} // need pointarea

	int nv = mPoints.size();
	if ((int)flags.size() != nv) flags.resize(nv);
	float invsigma2 = 1.0f / (sigma*sigma);

	vector<ntlVec3Gfx> nflt(nv);
	for (int i = 0; i < nv; i++) {
		if(diffuseVertexField( &mPoints[0].n, 2, i, invsigma2, nflt[i])) {
			normalize(nflt[i]);
		} else { nflt[i]=mPoints[i].n; }
	}

	// copy results
	for (int i = 0; i < nv; i++) { mPoints[i].n = nflt[i]; }
}


