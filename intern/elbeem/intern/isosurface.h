/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Marching Cubes "displayer"
 *
 *****************************************************************************/

#ifndef ISOSURFACE_H

#include "ntl_geometryobject.h"
#include "ntl_bsptree.h"

#define ISO_STRICT_DEBUG 0
#define ISOSTRICT_EXIT *((int *)0)=0;

/* access some 3d array */
#define ISOLEVEL_INDEX(ii,ij,ik) ((mSizex*mSizey*(ik))+(mSizex*(ij))+((ii)))

/* struct for a small cube in the scalar field */
typedef struct {
  ntlVec3Gfx   pos[8];
  double  value[8];
  int i,j,k;
} IsoLevelCube;


typedef struct {
  ntlVec3Gfx v; // vertex
  ntlVec3Gfx n; // vertex normal
} IsoLevelVertex;

//! class to triangulate a scalar field, e.g. for
// the fluid surface, templated by scalar field access object 
class IsoSurface : 
	public ntlGeometryObject //, public S
{

	public:

		/*! Constructor */
		IsoSurface(double iso, double blend);
		/*! Destructor */
		~IsoSurface();

		/*! Init ararys etc. */
		virtual void initializeIsosurface(int setx, int sety, int setz, ntlVec3Gfx extent);

		/*! triangulate the scalar field given by pointer*/
		void triangulate( void );

	protected:

		/* variables ... */

		//! size
		int mSizex, mSizey, mSizez;

		//! data pointer
		float *mpData;

		//! Level of the iso surface 
		double mIsoValue;

		//! blending distance for marching cubes 
		double mBlendVal;

		//! Store all the triangles vertices 
		vector<IsoLevelVertex> mPoints;

		//! Store indices of calculated points along the cubie edges 
		int *mpEdgeVerticesX;
		int *mpEdgeVerticesY;
		int *mpEdgeVerticesZ;


		//! vector for all the triangles (stored as 3 indices) 
		vector<unsigned int> mIndices;

		//! start and end vectors for the triangulation region to create triangles in 
		ntlVec3Gfx mStart, mEnd;

		//! normalized domain extent from parametrizer/visualizer 
		ntlVec3Gfx mDomainExtent;

		//! initialized? 
		bool mInitDone;

		//! amount of surface smoothing
		float mSmoothSurface;
		//! amount of normal smoothing
		float mSmoothNormals;
		
		//! grid data
		vector<int> mAcrossEdge;
		vector< vector<int> > mAdjacentFaces;

		vector<unsigned> flags;
		unsigned flag_curr;
		vector<ntlVec3Gfx> cornerareas;
		vector<float> pointareas;
		vector< vector<int> > neighbors;

	public:
		// miscelleanous access functions 

		//! set geometry start (for renderer) 
		void setStart(ntlVec3Gfx set) { mStart = set; };
		ntlVec3Gfx getStart() { return mStart; };
		//! set geometry end (for renderer) 
		void setEnd(ntlVec3Gfx set) { mEnd = set; };
		ntlVec3Gfx getEnd() { return mEnd; };
		//! set iso level value for surface reconstruction 
		inline void setIsolevel(double set) { mIsoValue = set; };
		//! set loop subdiv num
		inline void setSmoothSurface(float set) { mSmoothSurface = set; };
		inline void setSmoothNormals(float set) { mSmoothNormals = set; };

		// geometry object functions 
		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId );

		//! for easy GUI detection get start of axis aligned bounding box, return NULL of no BB 
		virtual inline ntlVec3Gfx *getBBStart() 	{ return &mStart; }
		virtual inline ntlVec3Gfx *getBBEnd() 		{ return &mEnd; }

		//! access data array
		inline float* getData(){ return mpData; }
		inline float* getData(int ii, int jj, int kk){ 
#if ISO_STRICT_DEBUG==1
			if(ii<0){ errMsg("IsoStrict"," invX- |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(jj<0){ errMsg("IsoStrict"," invY- |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(kk<0){ errMsg("IsoStrict"," invZ- |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(ii>mSizex-1){ errMsg("IsoStrict"," invX+ |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(jj>mSizey-1){ errMsg("IsoStrict"," invY+ |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(kk>mSizez-1){ errMsg("IsoStrict"," invZ+ |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			return mpData + ISOLEVEL_INDEX(ii, jj, kk); 
#else //ISO_STRICT_DEBUG==1
			return mpData + ISOLEVEL_INDEX(ii, jj, kk); 
#endif
		}
		inline float* lbmGetData(int ii, int jj, int kk){ 
#if ISO_STRICT_DEBUG==1
			ii++; jj++; kk++;
			if(ii<0){ errMsg("IsoStrict"," invX- |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(jj<0){ errMsg("IsoStrict"," invY- |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(kk<0){ errMsg("IsoStrict"," invZ- |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(ii>mSizex-1){ errMsg("IsoStrict"," invX+ |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(jj>mSizey-1){ errMsg("IsoStrict"," invY+ |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			if(kk>mSizez-1){ errMsg("IsoStrict"," invZ+ |"<<ii<<","<<jj<<","<<kk); ISOSTRICT_EXIT; }
			return mpData + ISOLEVEL_INDEX(ii, jj, kk); 
#else //ISO_STRICT_DEBUG==1
			return mpData + ISOLEVEL_INDEX(ii+1,jj+1,kk+1); 
#endif
		}

		//! OpenGL viz "interface"
		unsigned int getIsoVertexCount() {
			return mPoints.size();
		}
		unsigned int getIsoIndexCount() {
			return mIndices.size();
		}
		char* getIsoVertexArray() {
			return (char *) &(mPoints[0]);
		}
		unsigned int *getIsoIndexArray() {
			return &(mIndices[0]);
		}

	protected:

		//! computer normal
		inline ntlVec3Gfx getNormal(int i, int j,int k);

		void subdivide();
		void smoothSurface(float val);
		void smoothNormals(float val);
		void diffuseVertexField(ntlVec3Gfx *field, const int pointerScale, int v, float invsigma2, ntlVec3Gfx &flt);
};


#define ISOSURFACE_H
#endif


