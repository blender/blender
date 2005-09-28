/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Scene object, that contains and manages all geometry objects
 *
 *****************************************************************************/

#include "utilities.h"
#include "ntl_scene.h"
#include "ntl_geometryobject.h"
#include "ntl_geometryshader.h"


/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlScene::ntlScene( ntlRenderGlobals *glob ) :
	mpGlob( glob ),
	mpTree( NULL ),
	mDisplayListId( -1 ), 
	mSceneBuilt( false ), mFirstInitDone( false )
{
}


/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlScene::~ntlScene()
{
	cleanupScene();

	// cleanup lists
	for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin();
			iter != mGeos.end(); iter++) {
		delete (*iter);
	}
	for (vector<ntlLightObject*>::iterator iter = mpGlob->getLightList()->begin();
			 iter != mpGlob->getLightList()->end(); iter++) {
		delete (*iter);
	}
	for (vector<ntlMaterial*>::iterator iter = mpGlob->getMaterials()->begin();
			 iter != mpGlob->getMaterials()->end(); iter++) {
		delete (*iter);
	}
}


/******************************************************************************
 * Build the scene arrays (obj, tris etc.)
 *****************************************************************************/
void ntlScene::buildScene( void )
{
	const bool buildInfo=false;
	mObjects.clear();
	/* init geometry array, first all standard objects */
	for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin();
			iter != mGeos.end(); iter++) {
		bool geoinit = false;
		int tid = (*iter)->getTypeId();
		if(tid & GEOCLASSTID_OBJECT) {
			ntlGeometryObject *geoobj = (ntlGeometryObject*)(*iter);
			geoinit = true;
			mObjects.push_back( geoobj );
			if(buildInfo) debMsgStd("ntlScene::BuildScene",DM_MSG,"added GeoObj "<<geoobj->getName(), 5 );
		}
		//if(geoshad) {
		if(tid & GEOCLASSTID_SHADER) {
			ntlGeometryShader *geoshad = (ntlGeometryShader*)(*iter);
			geoinit = true;
			if(!mFirstInitDone) {
				// only on first init
				geoshad->initializeShader();
			}
			for (vector<ntlGeometryObject*>::iterator siter = geoshad->getObjectsBegin();
					siter != geoshad->getObjectsEnd();
					siter++) {
				if(buildInfo) debMsgStd("ntlScene::BuildScene",DM_MSG,"added shader geometry "<<(*siter)->getName(), 5 );
				mObjects.push_back( (*siter) );
			}
		}

		if(!geoinit) {
			errFatal("ntlScene::BuildScene","Invalid geometry class!", SIMWORLD_INITERROR);
			return;
		}
	}

	// collect triangles
	mTriangles.clear();
	mVertices.clear();
	mVertNormals.clear();

	/* for test mode deactivate transparencies etc. */
	if( mpGlob->getTestMode() ) {
		debugOut("ntlScene::buildScene : Test Mode activated!", 2);
		// assign random colors to dark materials
		int matCounter = 0;
		ntlColor stdCols[] = { ntlColor(0,0,1.0), ntlColor(0,1.0,0), ntlColor(1.0,0.7,0) , ntlColor(0.7,0,0.6) };
		int stdColNum = 4;
		for (vector<ntlMaterial*>::iterator iter = mpGlob->getMaterials()->begin();
					 iter != mpGlob->getMaterials()->end(); iter++) {
			(*iter)->setTransparence(0.0);
			(*iter)->setMirror(0.0);
			(*iter)->setFresnel(false);
			// too dark?
			if( norm((*iter)->getDiffuseRefl()) <0.01) {
				(*iter)->setDiffuseRefl( stdCols[matCounter] );
				matCounter ++;
				matCounter = matCounter%stdColNum;
			}
		}

		// restrict output file size to 400
		float downscale = 1.0;
		if(mpGlob->getResX() > 400){ downscale = 400.0/(float)mpGlob->getResX(); }
		if(mpGlob->getResY() > 400){ 
			float downscale2 = 400.0/(float)mpGlob->getResY(); 
			if(downscale2<downscale) downscale=downscale2;
		}
		mpGlob->setResX( (int)(mpGlob->getResX() * downscale) );
		mpGlob->setResY( (int)(mpGlob->getResY() * downscale) );

	}

	/* collect triangles from objects */
	int idCnt = 0;          // give IDs to objects
  for (vector<ntlGeometryObject*>::iterator iter = mObjects.begin();
       iter != mObjects.end();
       iter++) {
		/* only add visible objects */
		(*iter)->initialize( mpGlob );
		(*iter)->getTriangles(&mTriangles, &mVertices, &mVertNormals, idCnt);
		idCnt ++;
	}


	/* calculate triangle normals, and initialize flags */
  for (vector<ntlTriangle>::iterator iter = mTriangles.begin();
       iter != mTriangles.end();
       iter++) {

		// calculate normal from triangle points
		ntlVec3Gfx normal = 
			cross( (ntlVec3Gfx)( (mVertices[(*iter).getPoints()[2]] - mVertices[(*iter).getPoints()[0]]) *-1.0),  // BLITZ minus sign right??
			(ntlVec3Gfx)(mVertices[(*iter).getPoints()[1]] - mVertices[(*iter).getPoints()[0]]) );
		normalize(normal);
		(*iter).setNormal( normal );
	}



	// scene geometry built 
	mSceneBuilt = true;

	// init shaders that require complete geometry
	if(!mFirstInitDone) {
		// only on first init
		for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin();
				iter != mGeos.end(); iter++) {
			if( (*iter)->getTypeId() & GEOCLASSTID_SHADER ) {
				ntlGeometryShader *geoshad = (ntlGeometryShader*)(*iter);
				geoshad->postGeoConstrInit( mpGlob );
			}
		}
		mFirstInitDone = true;
	}

	// check unused attributes (for classes and objects!)
  for (vector<ntlGeometryObject*>::iterator iter = mObjects.begin(); iter != mObjects.end(); iter++) {
		if((*iter)->getAttributeList()->checkUnusedParams()) {
			(*iter)->getAttributeList()->print(); // DEBUG
			errFatal("ntlScene::buildScene","Unused params for object '"<< (*iter)->getName() <<"' !", SIMWORLD_INITERROR );
			return;
		}
	}
	for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin(); iter != mGeos.end(); iter++) { 
		if((*iter)->getAttributeList()->checkUnusedParams()) {
			(*iter)->getAttributeList()->print(); // DEBUG
			errFatal("ntlScene::buildScene","Unused params for object '"<< (*iter)->getName() <<"' !", SIMWORLD_INITERROR );
			return;
		}
	}

}

/******************************************************************************
 * Prepare the scene triangles and maps for raytracing
 *****************************************************************************/
void ntlScene::prepareScene( void )
{
	/* init triangles... */
	buildScene();
	// what for currently not used ???
	if(mpTree != NULL) delete mpTree;
	mpTree = new ntlTree( mpGlob->getTreeMaxDepth(), mpGlob->getTreeMaxTriangles(), 
												this, TRI_GEOMETRY );

	//debMsgStd("ntlScene::prepareScene",DM_MSG,"Stats - tris:"<< (int)mTriangles.size()<<" verts:"<<mVertices.size()<<" vnorms:"<<mVertNormals.size(), 5 );
}
/******************************************************************************
 * Do some memory cleaning, when frame is finished
 *****************************************************************************/
void ntlScene::cleanupScene( void )
{
	mObjects.clear();
	mTriangles.clear();
	mVertices.clear();
	mVertNormals.clear();

	if(mpTree != NULL) delete mpTree;
	mpTree = NULL;
}


/******************************************************************************
 * Intersect a ray with the scene triangles
 *****************************************************************************/
void ntlScene::intersectScene(const ntlRay &r, gfxReal &distance, ntlVec3Gfx &normal, ntlTriangle *&tri,int flags) const
{
	distance = -1.0;
  mpGlob->setCounterSceneInter( mpGlob->getCounterSceneInter()+1 );
	mpTree->intersect(r, distance, normal, tri, flags, false);
}








		
