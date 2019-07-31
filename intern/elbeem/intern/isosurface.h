/** \file
 * \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Marching Cubes "displayer"
 *
 *****************************************************************************/

#ifndef ISOSURFACE_H

#include "ntl_geometryobject.h"
#include "ntl_bsptree.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#define ISO_STRICT_DEBUG 0
#define ISOSTRICT_EXIT *((int *)0)=0;

/* access some 3d array */
#define ISOLEVEL_INDEX(ii,ij,ik) ((mSizex*mSizey*(ik))+(mSizex*(ij))+((ii)))

class ParticleTracer;

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
		IsoSurface(double iso);
		/*! Destructor */
		virtual ~IsoSurface();

		/*! Init ararys etc. */
		virtual void initializeIsosurface(int setx, int sety, int setz, ntlVec3Gfx extent);

		/*! Reset all values */
		void resetAll(gfxReal val);

		/*! triangulate the scalar field given by pointer*/
		void triangulate( void );

		/*! set particle pointer */
		void setParticles(ParticleTracer *pnt,float psize){ mpIsoParts = pnt; mPartSize=psize; };
		/*! set # of subdivisions, this has to be done before init! */
		void setSubdivs(int s) { 
			if(mInitDone) errFatal("IsoSurface::setSubdivs","Changing subdivs after init!", SIMWORLD_INITERROR);
			if(s<1) s=1;
			if(s>10) s=10;
			mSubdivs = s;
		}
		int  getSubdivs() { return mSubdivs;}
		/*! set full edge settings, this has to be done before init! */
		void setUseFulledgeArrays(bool set) { 
			if(mInitDone) errFatal("IsoSurface::setUseFulledgeArrays","Changing usefulledge after init!", SIMWORLD_INITERROR);
			mUseFullEdgeArrays = set;}

	protected:

		/* variables ... */

		//! size
		int mSizex, mSizey, mSizez;

		//! data pointer
		float *mpData;

		//! Level of the iso surface 
		double mIsoValue;

		//! Store all the triangles vertices 
		vector<IsoLevelVertex> mPoints;

		//! use full arrays? (not for farfield)
		bool mUseFullEdgeArrays;
		//! Store indices of calculated points along the cubie edges 
		int *mpEdgeVerticesX;
		int *mpEdgeVerticesY;
		int *mpEdgeVerticesZ;
		int mEdgeArSize;


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

		//! cutoff border area
		int mCutoff;
		//! cutoff height values
		int *mCutArray;
		//! particle pointer
		ParticleTracer *mpIsoParts;
		//! particle size
		float mPartSize;
		//! no of subdivisions
		int mSubdivs;
		
		//! trimesh vars
		vector<int> flags;
		int mFlagCnt;
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
		inline float getSmoothSurface() { return mSmoothSurface; }
		inline float getSmoothNormals() { return mSmoothNormals; }

		// geometry object functions 
		virtual void getTriangles(double t, vector<ntlTriangle> *triangles, 
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
		//! set cut off border
		inline void setCutoff(int set) { mCutoff = set; };
		//! set cut off border
		inline void setCutArray(int *set) { mCutArray = set; };

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
		
		// surface smoothing functions
		void setSmoothRad(float radi1, float radi2, ntlVec3Gfx mscc);
		void smoothSurface(float val, bool smoothNorm);
		void smoothNormals(float val);
		void computeNormals();

	protected:

		//! compute normal
		inline ntlVec3Gfx getNormal(int i, int j,int k);
		//! smoothing helper function
		bool diffuseVertexField(ntlVec3Gfx *field, int pointerScale, int v, float invsigma2, ntlVec3Gfx &flt);
		vector<int> mDboundary;
		float mSCrad1, mSCrad2;
		ntlVec3Gfx mSCcenter;

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:IsoSurface")
#endif
};


#define ISOSURFACE_H
#endif


