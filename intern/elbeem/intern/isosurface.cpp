/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2005 Nils Thuerey
 *
 * Marching Cubes surface mesh generation
 *
 *****************************************************************************/

#include "isosurface.h"
#include "mcubes_tables.h"
#include "ntl_scene.h"

#include <algorithm>
#include <stdio.h>



/******************************************************************************
 * Constructor
 *****************************************************************************/
IsoSurface::IsoSurface(double iso, double blend) :
	ntlGeometryObject(),
	mSizex(-1), mSizey(-1), mSizez(-1),
  mIsoValue( iso ), 
	mBlendVal( blend ),
	mPoints(), 
	mpEdgeVerticesX(NULL), mpEdgeVerticesY(NULL), mpEdgeVerticesZ(NULL),
	mIndices(),

  mStart(0.0), mEnd(0.0), mDomainExtent(0.0),
  mInitDone(false),
	mSmoothSurface(0.0), mSmoothNormals(0.0),
	mAcrossEdge(), mAdjacentFaces()
{
}


/******************************************************************************
 * The real init...
 *****************************************************************************/
void IsoSurface::initializeIsosurface(int setx, int sety, int setz, ntlVec3Gfx extent) 
{
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
  mpEdgeVerticesX = new int[nodes];
  mpEdgeVerticesY = new int[nodes];
  mpEdgeVerticesZ = new int[nodes];
  for(int i=0;i<nodes;i++) { mpEdgeVerticesX[i] = mpEdgeVerticesY[i] = mpEdgeVerticesZ[i] = -1; }
  
	// marching cubes are ready 
	mInitDone = true;
	unsigned long int memCnt = (3*sizeof(int)*nodes + sizeof(float)*nodes);
	double memd = memCnt;
	char *sizeStr = "";
	const double sfac = 1000.0;
	if(memd>sfac){ memd /= sfac; sizeStr="KB"; }
	if(memd>sfac){ memd /= sfac; sizeStr="MB"; }
	if(memd>sfac){ memd /= sfac; sizeStr="GB"; }
	if(memd>sfac){ memd /= sfac; sizeStr="TB"; }

	debMsgStd("IsoSurface::initializeIsosurface",DM_MSG,"Inited "<<PRINT_VEC(setx,sety,setz)<<" alloced:"<< memd<<" "<<sizeStr<<"." ,10);
}




/******************************************************************************
 * Destructor
 *****************************************************************************/
IsoSurface::~IsoSurface( void )
{

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

  // get grid spacing
  gsx = (mEnd[0]-mStart[0])/(double)(mSizex-1.0);
  gsy = (mEnd[1]-mStart[1])/(double)(mSizey-1.0);
  gsz = (mEnd[2]-mStart[2])/(double)(mSizez-1.0);

  // clean up previous frame
	mIndices.clear();
	mPoints.clear();

	// reset edge vertices
  for(int i=0;i<(mSizex*mSizey*mSizez);i++) {
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

  // let the cubes march 
	pz = mStart[2]-gsz;
	for(int k=0;k<(mSizez-2);k++) {
		pz += gsz;
		py = mStart[1]-gsy;
		for(int j=0;j<(mSizey-2);j++) {
      py += gsy;
			px = mStart[0]-gsx;
			for(int i=0;i<(mSizex-2);i++) {
   			px += gsx;
				int baseIn = ISOLEVEL_INDEX( i+0, j+0, k+0);

				value[0] = *getData(i  ,j  ,k  );
				value[1] = *getData(i+1,j  ,k  );
				value[2] = *getData(i+1,j+1,k  );
				value[3] = *getData(i  ,j+1,k  );
				value[4] = *getData(i  ,j  ,k+1);
				value[5] = *getData(i+1,j  ,k+1);
				value[6] = *getData(i+1,j+1,k+1);
				value[7] = *getData(i  ,j+1,k+1);
				//errMsg("ISOT2D"," at "<<PRINT_IJK<<" "
						//<<" v0="<<value[0] <<" v1="<<value[1] <<" v2="<<value[2] <<" v3="<<value[3]
						//<<" v4="<<value[4] <<" v5="<<value[5] <<" v6="<<value[6] <<" v7="<<value[7] );

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
				eVert[ 0] = &mpEdgeVerticesX[ baseIn ];
				eVert[ 1] = &mpEdgeVerticesY[ baseIn + 1 ];
				eVert[ 2] = &mpEdgeVerticesX[ ISOLEVEL_INDEX( i+0, j+1, k+0) ];
				eVert[ 3] = &mpEdgeVerticesY[ baseIn ];

				eVert[ 4] = &mpEdgeVerticesX[ ISOLEVEL_INDEX( i+0, j+0, k+1) ];
				eVert[ 5] = &mpEdgeVerticesY[ ISOLEVEL_INDEX( i+1, j+0, k+1) ];
				eVert[ 6] = &mpEdgeVerticesX[ ISOLEVEL_INDEX( i+0, j+1, k+1) ];
				eVert[ 7] = &mpEdgeVerticesY[ ISOLEVEL_INDEX( i+0, j+0, k+1) ];

				eVert[ 8] = &mpEdgeVerticesZ[ baseIn ];
				eVert[ 9] = &mpEdgeVerticesZ[ ISOLEVEL_INDEX( i+1, j+0, k+0) ];
				eVert[10] = &mpEdgeVerticesZ[ ISOLEVEL_INDEX( i+1, j+1, k+0) ];
				eVert[11] = &mpEdgeVerticesZ[ ISOLEVEL_INDEX( i+0, j+1, k+0) ];

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
							//double mu;            // interpolation value
							//ntlVec3Gfx p;         // new point

							// choose if point should be calculated by interpolation,
							// or "Karolin" method 

							//double deltaVal = ABS(valp2-valp1);
							//if(deltaVal >-10.0) {
							// standard interpolation
							//vertInfo[e].type = 0; 
							/*if (ABS(mIsoValue-valp1) < 0.000000001) {
								mu = 0.0;
							} else {
								if (ABS(mIsoValue-valp2) < 0.000000001) {
									mu = 1.0;
								} else {
									mu = (mIsoValue - valp1) / (valp2 - valp1);
								}
							} */
							/*} else {
								errorOut(" ? ");
							// use fill grade (=karo) 
							vertInfo[e].type = 1; 
							if(valp1 < valp2) { mu = 1.0- (valp1 + valp2 - mIsoValue);
							} else { mu = 0.0+ (valp1 + valp2 - mIsoValue); } 
							} */

							//const float mu = (mIsoValue - valp1) / (valp2 - valp1);
							float mu;
							if(valp1 < valp2) {
								mu = 1.0 - 1.0*(valp1 + valp2 - mIsoValue);
							} else {
								mu = 0.0 + 1.0*(valp1 + valp2 - mIsoValue);
							}

							float isov2 = mIsoValue;
							isov2 = (valp1+valp2)*0.5;
							mu = (isov2 - valp1) / (valp2 - valp1);
							mu = (isov2) / (valp2 - valp1);

							mu = (mIsoValue - valp1) / (valp2 - valp1);
							//mu *= mu;



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


				// Create the triangles... 
				for(int e=0; mcTriTable[cubeIndex][e]!=-1; e+=3) {
					//errMsg("MC","tri "<<mIndices.size() <<" "<< triIndices[ mcTriTable[cubeIndex][e+0] ]<<" "<< triIndices[ mcTriTable[cubeIndex][e+1] ]<<" "<< triIndices[ mcTriTable[cubeIndex][e+2] ] );
					mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+0] ] );
					mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+1] ] );
					mIndices.push_back( triIndices[ mcTriTable[cubeIndex][e+2] ] );
				}

				
      }
    }

  } // k
  

  // precalculate normals using an approximation of the scalar field gradient 
	for(int ni=0;ni<(int)mPoints.size();ni++) {
		// use triangle normals?, this seems better for File-IsoSurf
		normalize( mPoints[ni].n );
	}

	if(mSmoothSurface>0.0) {
		// not needed for post normal smoothing? 
		// if(mSmoothNormals<=0.0) { smoothNormals(mSmoothSurface*0.5); }
		smoothSurface(mSmoothSurface);
	}
	if(mSmoothNormals>0.0) {
		smoothNormals(mSmoothNormals);
	}
	myTime_t tritimeend = getTime(); 
	debMsgStd("IsoSurface::triangulate",DM_MSG,"took "<< ((tritimeend-tritimestart)/(double)1000.0)<<"s, ss="<<mSmoothSurface<<" sm="<<mSmoothNormals , 10 );
}


	


/******************************************************************************
 * Get triangles for rendering
 *****************************************************************************/
void IsoSurface::getTriangles( vector<ntlTriangle> *triangles, 
													 vector<ntlVec3Gfx> *vertices, 
													 vector<ntlVec3Gfx> *normals, int objectId )
{
	if(!mInitDone) {
		debugOut("IsoSurface::getTriangles warning: Not initialized! ", 10);
		return;
	}

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
		if( (getMaterial()->getMirror()>0.0) ||  
				(getMaterial()->getTransparence()>0.0) ||  
				(getMaterial()->getFresnel()>0.0) ) { 
			flag |= TRI_MAKECAUSTICS; } 
		else { 
			flag |= TRI_NOCAUSTICS; } 

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
 * Surface improvement
 * makes use of trimesh2 library
 * http://www.cs.princeton.edu/gfx/proj/trimesh2/
 *
 * Copyright (c) 2004 Szymon Rusinkiewicz.
 * see COPYING_trimesh2
 * 
 *****************************************************************************/

// Diffuse a vector field at 1 vertex, weighted by
// a gaussian of width 1/sqrt(invsigma2)
void IsoSurface::diffuseVertexField(ntlVec3Gfx *field, const int pointerScale, int v, float invsigma2, ntlVec3Gfx &flt)
{
	flt = ntlVec3Gfx(0.0);
	flt += *(field+pointerScale*v) *pointareas[v];
	float sum_w = pointareas[v];
	const ntlVec3Gfx &nv = mPoints[v].n;

	unsigned &flag = flag_curr;
	flag++;
	flags[v] = flag;
	vector<int> boundary = neighbors[v];
	while (!boundary.empty()) {
		const int bbn = boundary.back();
		boundary.pop_back();
		if (flags[bbn] == flag) continue;
		flags[bbn] = flag;

		// gaussian weight of width 1/sqrt(invsigma2)
		const float d2 = invsigma2 * normNoSqrt(mPoints[bbn].v - mPoints[v].v);
		if(d2 >= 9.0f) continue; // 25 also possible  , slower
		//float w = (d2 >=  9.0f) ? 0.0f : exp(-0.5f*d2);
		float w = exp(-0.5f*d2);
		if(dot(nv, mPoints[bbn].n) <= 0.0f) continue; // faster than before d2 calc?

		// Downweight things pointing in different directions
		w *= dot(nv , mPoints[bbn].n);
		// Surface area "belonging" to each point
		w *= pointareas[bbn];
		// Accumulate weight times field at neighbor
		flt += *(field+pointerScale*bbn)*w;

		sum_w += w;
		for (int i = 0; i < (int)neighbors[bbn].size(); i++) {
			int nn = neighbors[bbn][i];
			if (flags[nn] == flag) continue;
			boundary.push_back(nn);
		}
	}
	flt /= sum_w;
}

void IsoSurface::smoothSurface(float sigma)
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
		diffuseVertexField( &mPoints[0].v, 2,
				   i, invsigma2, dflt[i]);
		// Just keep the displacement
		dflt[i] -= mPoints[i].v;
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
		diffuseVertexField( &dflt[0], 1,
				   i, invsigma2, dflt2[i]);
	}

	// Update vertex positions
	for (int i = 0; i < nv; i++) {
		mPoints[i].v += dflt[i] - dflt2[i]; // second Laplacian
	}

	// when normals smoothing off, this cleans up quite well
	// costs ca. 50% additional time though
	float nsFac = 1.5f;
	{ float ninvsigma2 = 1.0f / (nsFac*nsFac*sigma*sigma);
		for (int i = 0; i < nv; i++) {
			diffuseVertexField( &mPoints[0].n, 2, i, ninvsigma2, dflt[i]);
			normalize(dflt[i]);
		}
		for (int i = 0; i < nv; i++) {
			mPoints[i].n = dflt[i];
		} 
	} // smoothNormals copy */

	//errMsg("SMSURF","done v:"<<sigma); // DEBUG
}

void IsoSurface::smoothNormals(float sigma)
{
	{ // need neighbor
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
		diffuseVertexField( &mPoints[0].n, 2, i, invsigma2, nflt[i]);
		normalize(nflt[i]);
	}

	for (int i = 0; i < nv; i++) {
		mPoints[i].n = nflt[i];
	}

	//errMsg("SMNRMLS","done v:"<<sigma); // DEBUG
}


