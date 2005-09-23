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
//class LbmSolver3D;



/* access some 3d array */
//#define ISOLEVEL_INDEX(ii,ij,ik) ((S::mSizex*S::mSizey*(ik))+(S::mSizex*(ij))+((ii)))
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
//template<class S>
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

		//! no. of refinement steps
		int mLoopSubdivs;
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
		//! set geometry end (for renderer) 
		void setEnd(ntlVec3Gfx set) { mEnd = set; };
		//! set iso level value for surface reconstruction 
		inline void setIsolevel(double set) { mIsoValue = set; };
		//! set loop subdiv num
		inline void setLoopSubdivs(int set) { mLoopSubdivs = set; };
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
		inline float* getData(int i, int j, int k){ return mpData + ISOLEVEL_INDEX(i,j,k); }
		inline float* lbmGetData(int i, int j, int k){ return mpData + ISOLEVEL_INDEX(i+1,j+1,k+1); }

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



class TriMesh {
public:
	// Types
	struct Face {
		int v[3];

		Face() {}
		Face(const int &v0, const int &v1, const int &v2)
			{ v[0] = v0; v[1] = v1; v[2] = v2; }
		Face(const int *v_)
			{ v[0] = v_[0]; v[1] = v_[1]; v[2] = v_[2]; }
		int &operator[] (int i) { return v[i]; }
		const int &operator[] (int i) const { return v[i]; }
		operator const int * () const { return &(v[0]); }
		operator const int * () { return &(v[0]); }
		operator int * () { return &(v[0]); }
		int indexof(int v_) const
		{
			return (v[0] == v_) ? 0 :
			       (v[1] == v_) ? 1 :
			       (v[2] == v_) ? 2 : -1;
		}
	};

	struct BBox {
		public:	
		BBox() {};
		ntlVec3Gfx min, max;
		ntlVec3Gfx center() const { return (min+max)*0.5f; }
		ntlVec3Gfx size() const { return max - min; }
	};

	struct BSphere {
		ntlVec3Gfx center;
		float r;
	};

	// Enums
	enum tstrip_rep { TSTRIP_LENGTH, TSTRIP_TERM };

	// The basics: vertices and faces
	vector<ntlVec3Gfx> vertices;
	vector<Face> faces;

	// Triangle strips
	vector<int> tstrips;

	// Other per-vertex properties
	//vector<Color> colors;
	vector<float> confidences;
	vector<unsigned> flags;
	unsigned flag_curr;

	// Computed per-vertex properties
	vector<ntlVec3Gfx> normals;
	vector<ntlVec3Gfx> pdir1, pdir2;
	vector<float> curv1, curv2;
	//vector< Vec<4,float> > dcurv;
	vector<ntlVec3Gfx> cornerareas;
	vector<float> pointareas;

	// Bounding structures
	BBox bbox;
	BSphere bsphere;

	// Connectivity structures:
	//  For each vertex, all neighboring vertices
	vector< vector<int> > neighbors;
	//  For each vertex, all neighboring faces
	vector< vector<int> > adjacentfaces;
	//  For each face, the three faces attached to its edges
	//  (for example, across_edge[3][2] is the number of the face
	//   that's touching the edge opposite vertex 2 of face 3)
	vector<Face> across_edge;

	// Compute all this stuff...
	void need_tstrips();
	void convert_strips(tstrip_rep rep);
	void need_faces();
	void need_normals();
	void need_pointareas();
	void need_curvatures();
	void need_dcurv();
	void need_bbox();
	void need_bsphere();
	void need_neighbors();
	void need_adjacentfaces();
	void need_across_edge();

	// Input and output
	static TriMesh *read(const char *filename);
	void write(const char *filename);

	// Statistics
	// XXX - Add stuff here
	float feature_size();

	// Useful queries
	// XXX - Add stuff here
	bool is_bdy(int v)
	{
		if (neighbors.empty()) need_neighbors();
		if (adjacentfaces.empty()) need_adjacentfaces();
		return neighbors[v].size() != adjacentfaces[v].size();
	}

	// Debugging printout, controllable by a "verbose"ness parameter
	static int verbose;
	static void set_verbose(int);
	static int dprintf(const char *format, ...);
};

#define ISOSURFACE_H
#endif


