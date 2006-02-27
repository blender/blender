/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A simple box object
 *
 *****************************************************************************/

#include "ntl_geometrymodel.h"
#include "ntl_ray.h"
#include "ntl_world.h"
#include "zlib.h"

#ifdef WIN32
#ifndef strncasecmp
#define strncasecmp(a,b,c) strcmp(a,b)
#endif
#endif // WIN32


/******************************************************************************
 * Default Constructor 
 *****************************************************************************/
ntlGeometryObjModel::ntlGeometryObjModel( void ) :
	ntlGeometryObject(),
	mvStart( 0.0 ), mvEnd( 1.0 ),
	mLoaded( false ),
	mTriangles(), mVertices(), mNormals()
{
}

/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlGeometryObjModel::~ntlGeometryObjModel()
{
	if(!mLoaded) {
		errMsg("ntlGeometryObjModel","delete obj...");
	}
}


/*****************************************************************************/
/* Init attributes etc. of this object */
/*****************************************************************************/
void ntlGeometryObjModel::initialize(ntlRenderGlobals *glob) 
{
	// perhaps the model is already inited from initModel below?
	if(mLoaded==1) {
		// init default material
		searchMaterial( glob->getMaterials() );
		return;
	}
	
	ntlGeometryObject::initialize(glob);
	mFilename = mpAttrs->readString("filename", mFilename,"ntlGeometryObjModel", "mFilename", true);

	if(mFilename == "") {
		errMsg("ntlGeometryObjModel::getTriangles","Filename not given!");
		return;
	}

	const char *suffix = strrchr(mFilename.c_str(), '.');
	if (suffix) {
		if (!strncasecmp(suffix, ".obj", 4)) {
			errMsg("ntlGeometryObjModel::getTriangles",".obj files not supported!");
			return;
		} else if (!strncasecmp(suffix, ".gz", 3)) { 
			//mType = 1; // assume its .bobj.gz
		} else if (!strncasecmp(suffix, ".bobj", 5)) { 
			//mType = 1;
		}
	}

	// continue with standard obj
	if(loadBobjModel(mFilename)==0) mLoaded=1;
	if(!mLoaded) {
		debMsgStd("ntlGeometryObjModel",DM_WARNING,"Unable to load object file '"<<mFilename<<"' !", 0);
	}
}

/******************************************************************************
 * init model from given vertex and triangle arrays 
 *****************************************************************************/

int ntlGeometryObjModel::initModel(int numVertices, float *vertices, int numTriangles, int *triangles)
{
	mVertices.clear();
	mVertices.resize( numVertices );
	mNormals.resize( numVertices );
	for(int i=0; i<numVertices; i++) {
		mVertices[i] = ntlVec3Gfx(vertices[i*3+0],vertices[i*3+1],vertices[i*3+2]);
		mNormals[i] = ntlVec3Gfx(1.0); // unused, set to !=0.0
	}

	mTriangles.clear();
	mTriangles.resize( 3*numTriangles );
	for(int i=0; i<numTriangles; i++) {
		mTriangles[3*i+0] = triangles[i*3+0];
		mTriangles[3*i+1] = triangles[i*3+1];
		mTriangles[3*i+2] = triangles[i*3+2];
	}

	// inited, no need to parse attribs etc.
	mLoaded = 1;
	return 0;
}


/******************************************************************************
 * load model from .obj file
 *****************************************************************************/

int ntlGeometryObjModel::loadBobjModel(string filename)
{
	const bool debugPrint=false;
	gzFile gzf;
	gzf = gzopen(filename.c_str(), "rb");
	if (!gzf) {
		errFatal("ntlGeometryObjModel::loadBobjModel","Reading GZ_BOBJ, Unable to open '"<< filename <<"'...\n", SIMWORLD_INITERROR );
		return 1;
	}

	int numVerts;
	if(sizeof(numVerts)!=4) {  // paranoia check
		errMsg("Reading GZ_BOBJ"," Invalid int size, check compiler settings: int has to be 4 byte long"); 
		goto gzreaderror;
	}
	gzread(gzf, &numVerts, sizeof(numVerts) );
	if(numVerts<0 || numVerts>1e9) {
		errMsg("Reading GZ_BOBJ"," invalid num vertices "<< numVerts);
		goto gzreaderror;
	}
	mVertices.clear();
	mVertices.resize( numVerts );
	for(int i=0; i<numVerts; i++) {
		float x[3];
		for(int j=0; j<3; j++) {
			gzread(gzf, &(x[j]), sizeof( (x[j]) ) ); 
		}
		mVertices[i] = ntlVec3Gfx(x[0],x[1],x[2]);
	}
	if(debugPrint) errMsg("NV"," "<<numVerts<<" "<< mVertices.size() );

	// should be the same as Vertices.size
	gzread(gzf, &numVerts, sizeof(numVerts) );
	if(numVerts<0 || numVerts>1e9) {
		errMsg("Reading GZ_BOBJ","invalid num normals "<< numVerts);
		goto gzreaderror;
	}
	mNormals.clear();
	mNormals.resize( numVerts );
	for(int i=0; i<numVerts; i++) {
		float n[3];
		for(int j=0; j<3; j++) {
			gzread(gzf, &(n[j]), sizeof( (n[j]) ) ); 
		}
		mNormals[i] = ntlVec3Gfx(n[0],n[1],n[2]);
	}
	if(debugPrint) errMsg("NN"," "<<numVerts<<" "<< mNormals.size() );

	int numTris;
	gzread(gzf, &numTris, sizeof(numTris) );
	if(numTris<0 || numTris>1e9) {
		errMsg("Reading GZ_BOBJ","invalid num normals "<< numTris);
		goto gzreaderror;
	}
	mTriangles.resize( 3*numTris );
	for(int i=0; i<numTris; i++) {
		int tri[3];
		for(int j=0; j<3; j++) {
			gzread(gzf, &(tri[j]), sizeof( (tri[j]) ) ); 
		}
		mTriangles[3*i+0] = tri[0];
		mTriangles[3*i+1] = tri[1];
		mTriangles[3*i+2] = tri[2];
	}
	if(debugPrint) errMsg("NT"," "<<numTris<<" "<< mTriangles.size() );

	debMsgStd("ntlGeometryObjModel::loadBobjModel",DM_MSG, "File '"<<filename<<"' loaded, #Vertices: "<<mVertices.size()<<", #Normals: "<<mNormals.size()<<", #Triangles: "<<(mTriangles.size()/3)<<" ", 1 );

	gzclose( gzf );
	return 0;
gzreaderror:
	mTriangles.clear();
	mVertices.clear();
	mNormals.clear();
	gzclose( gzf );
	errFatal("ntlGeometryObjModel::loadBobjModel","Reading GZ_BOBJ, Unable to load '"<< filename <<"', exiting...\n", SIMWORLD_INITERROR );
	return 1;
}


/******************************************************************************
 * 
 *****************************************************************************/
void 
ntlGeometryObjModel::getTriangles( vector<ntlTriangle> *triangles, 
															vector<ntlVec3Gfx> *vertices, 
															vector<ntlVec3Gfx> *normals, int objectId )
{
	if(!mLoaded) { // invalid type...
		return;
	}

	for(int i=0; i<(int)mTriangles.size(); i+=3) {
		int trip[3];
		trip[0] = mTriangles[i+0];
		trip[1] = mTriangles[i+1];
		trip[2] = mTriangles[i+2];

		sceneAddTriangle( 
				mVertices[trip[0]], mVertices[trip[1]], mVertices[trip[2]], 
				mNormals[trip[0]], mNormals[trip[1]], mNormals[trip[2]], 
				ntlVec3Gfx(0.0), 1 , triangles,vertices,normals ); /* normal unused */
	}
	objectId = -1; // remove warning
	// bobj
	return;
}





