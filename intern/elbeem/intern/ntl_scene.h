/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Scene object, that contains and manages all geometry objects
 *
 *****************************************************************************/
#ifndef NTL_SCENE_HH
#define NTL_SCENE_HH

#include <sstream>
#include "ntl_vector3dim.h"
#include "ntl_material.h"
#include "ntl_geometryclass.h"
#include "ntl_triangle.h"
#include "ntl_bsptree.h"
class ntlRay;
class ntlGeometryObject;

/*! fluid geometry init types */
#define FGI_FLAGSTART   16
#define FGI_FLUID			  (1<<(FGI_FLAGSTART+ 0))
#define FGI_NO_FLUID	  (1<<(FGI_FLAGSTART+ 1))
#define FGI_BNDNO			  (1<<(FGI_FLAGSTART+ 2))
#define FGI_BNDFREE		  (1<<(FGI_FLAGSTART+ 3))
#define FGI_BNDPART		  (1<<(FGI_FLAGSTART+ 4))
#define FGI_NO_BND		  (1<<(FGI_FLAGSTART+ 5))
#define FGI_MBNDINFLOW	(1<<(FGI_FLAGSTART+ 6))
#define FGI_MBNDOUTFLOW	(1<<(FGI_FLAGSTART+ 7))

#define FGI_ALLBOUNDS ( FGI_BNDNO | FGI_BNDFREE | FGI_BNDPART | FGI_MBNDINFLOW | FGI_MBNDOUTFLOW )


//! convenience macro for adding triangles
#define sceneAddTriangle(p1,p2,p3, pn1,pn2,pn3, trin, smooth)   {\
	\
	ntlTriangle tri;\
	int tempVert;\
  \
	if(normals->size() != vertices->size()) {\
		errFatal("getTriangles","For '"<<mName<<"': Vertices and normals sizes to not match!!!",SIMWORLD_GENERICERROR);\
	} else {\
  \
	vertices->push_back( p1 ); \
	normals->push_back( pn1 ); \
	tempVert = normals->size()-1;\
	tri.getPoints()[0] = tempVert;\
  \
	vertices->push_back( p2 ); \
	normals->push_back( pn2 ); \
	tempVert = normals->size()-1;\
	tri.getPoints()[1] = tempVert;\
  \
	vertices->push_back( p3 ); \
	normals->push_back( pn3 ); \
	tempVert = normals->size()-1;\
	tri.getPoints()[2] = tempVert;\
  \
	\
	/* init flags */\
	int flag = 0; \
	if(getVisible()){ flag |= TRI_GEOMETRY; }\
	if(getCastShadows() ) { \
		flag |= TRI_CASTSHADOWS; } \
	if( (getMaterial()->getMirror()>0.0) ||  \
			(getMaterial()->getTransparence()>0.0) ||  \
			(getMaterial()->getFresnel()>0.0) ) { \
		flag |= TRI_MAKECAUSTICS; } \
	else { \
		flag |= TRI_NOCAUSTICS; } \
	\
	/* init geo init id */\
	int geoiId = getGeoInitId(); \
	if(geoiId > 0) { \
		flag |= (1<< (geoiId+4)); \
		flag |= mGeoInitType; \
	} \
	\
	tri.setFlags( flag );\
	\
	/* triangle normal missing */\
	tri.setNormal( trin );\
	tri.setSmoothNormals( smooth );\
	tri.setObjectId( objectId );\
	triangles->push_back( tri ); \
	} /* normals check*/ \
	}\



class ntlScene
{
public:
  /* CONSTRUCTORS */
  /*! Default constructor */
  ntlScene( ntlRenderGlobals *glob );
  /*! Default destructor */
   ~ntlScene();

	/*! Add an object to the scene */
	inline void addGeoClass(ntlGeometryClass *geo) { mGeos.push_back( geo ); }

	/*! Acces a certain object */
	inline ntlGeometryObject *getObject(int id) { 
		if(!mSceneBuilt) { errMsg("ntlScene::getObject","Scene not inited!"); return NULL; }
		return mObjects[id]; }

	/*! Acces object array */
	inline vector<ntlGeometryObject*> *getObjects() { 
		if(!mSceneBuilt) { errMsg("ntlScene::getObjects[]","Scene not inited!"); return NULL; }
		return &mObjects; }

	/*! Acces geo class array */
	inline vector<ntlGeometryClass*> *getGeoClasses() { 
		if(!mSceneBuilt) { errMsg("ntlScene::getGeoClasses[]","Scene not inited!"); return NULL; }
		return &mGeos; }

	/*! draw scene with opengl */
	//void draw();
	
	/*! Build the scene arrays */
	void buildScene( void );
	
	//! Prepare the scene triangles and maps for raytracing
	void prepareScene( void );
	//! Do some memory cleaning, when frame is finished
	void cleanupScene( void );

	/*! Intersect a ray with the scene triangles */
	void intersectScene(const ntlRay &r, gfxReal &distance, ntlVec3Gfx &normal, ntlTriangle *&tri, int flags) const;

	/*! return a vertex */
	ntlVec3Gfx getVertex(int index) { return mVertices[index]; } 

	// for tree generation 
	/*! return pointer to vertices vector */
	vector<ntlVec3Gfx> *getVertexPointer( void ) { return &mVertices; }
	/*! return pointer to vertices vector */
	vector<ntlVec3Gfx> *getVertexNormalPointer( void ) { return &mVertNormals; }
	/*! return pointer to vertices vector */
	vector<ntlTriangle> *getTrianglePointer( void ) { return &mTriangles; }

private:

	/*! Global settings */
	ntlRenderGlobals *mpGlob;

  /*! List of geometry classes */
  vector<ntlGeometryClass *> mGeos;

  /*! List of geometry objects */
  vector<ntlGeometryObject *> mObjects;

  /*! List of triangles */
  vector<ntlTriangle> mTriangles;
  /*! List of vertices */
  vector<ntlVec3Gfx>  mVertices;
  /*! List of normals */
  vector<ntlVec3Gfx>  mVertNormals;
  /*! List of triangle normals */
  vector<ntlVec3Gfx>  mTriangleNormals;

	/*! Tree to store quickly intersect triangles */
	ntlTree *mpTree;

	/*! id of dislpay list for raytracer stuff */
	int mDisplayListId;

	/*! was the scene successfully built? only then getObject(i) requests are valid */
	bool mSceneBuilt;

	/*! shader/obj initializations are only done on first init */
	bool mFirstInitDone;

};


#endif

