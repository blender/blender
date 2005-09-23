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
#include "ntl_scene.h"
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


/* defines */
#define T(x) model->triangles[(x)]

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
				ntlVec3Gfx(0.0), 1 ); /* normal unused */
	}
	// bobj
	return;
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

	int wri;
	int gotbytes = -1;
	gotbytes = gzread(gzf, &wri, sizeof(wri) );
	if(gotbytes != sizeof(int)){ errMsg("Reading GZ_BOBJ"," Invalid readNV size "<< wri); goto gzreaderror; }
	if(sizeof(wri)!=4) {  // paranoia check
		errMsg("Reading GZ_BOBJ"," Invalid int size "<< wri); 
		goto gzreaderror;
	}
	if(wri<0 || wri>1e9) {
		errMsg("Reading GZ_BOBJ"," invalid num vertices "<< wri);
		goto gzreaderror;
	}
	mVertices.clear();
	mVertices.resize( wri );
	for(int i=0; i<wri; i++) {
		float x[3];
		for(int j=0; j<3; j++) {
			gotbytes = gzread(gzf, &(x[j]), sizeof( (x[j]) ) ); 
			if(gotbytes != sizeof(float)){ errMsg("Reading GZ_BOBJ"," Invalid readV size "<< wri); goto gzreaderror; } // CHECK
		}
		mVertices[i] = ntlVec3Gfx(x[0],x[1],x[2]);
	}
	if(debugPrint) errMsg("NV"," "<<wri<<" "<< mVertices.size() );

	// should be the same as Vertices.size
	gotbytes = gzread(gzf, &wri, sizeof(wri) );
	if(gotbytes != sizeof(int)){ errMsg("Reading GZ_BOBJ","Invalid readNN size "<< wri); goto gzreaderror; }
	if(wri<0 || wri>1e9) {
		errMsg("Reading GZ_BOBJ","invalid num normals "<< wri);
		goto gzreaderror;
	}
	mNormals.clear();
	mNormals.resize( wri );
	for(int i=0; i<wri; i++) {
		float n[3];
		for(int j=0; j<3; j++) {
			gotbytes = gzread(gzf, &(n[j]), sizeof( (n[j]) ) ); 
			if(gotbytes != sizeof(float)){ errMsg("Reading GZ_BOBJ","Invalid readN size "<< wri); goto gzreaderror; }
		}
		mNormals[i] = ntlVec3Gfx(n[0],n[1],n[2]);
	}
	if(debugPrint) errMsg("NN"," "<<wri<<" "<< mNormals.size() );

	gotbytes = gzread(gzf, &wri, sizeof(wri) );
	if(gotbytes != sizeof(int)){ errMsg("Reading GZ_BOBJ","Invalid readNT size "<< wri); goto gzreaderror; }
	if(wri<0 || wri>1e9) {
		errMsg("Reading GZ_BOBJ","invalid num normals "<< wri);
		goto gzreaderror;
	}
	mTriangles.resize( 3*wri );
	for(int i=0; i<wri; i++) {
		int tri[3];
		for(int j=0; j<3; j++) {
			gotbytes = gzread(gzf, &(tri[j]), sizeof( (tri[j]) ) ); 
			if(gotbytes != sizeof(int)){ errMsg("Reading GZ_BOBJ","Invalid readT size "<< wri); goto gzreaderror; }
		}
		mTriangles[3*i+0] = tri[0];
		mTriangles[3*i+1] = tri[1];
		mTriangles[3*i+2] = tri[2];
	}
	if(debugPrint) errMsg("NT"," "<<wri<<" "<< mTriangles.size() );

	debMsgStd("ntlGeometryObjModel::loadBobjModel",DM_MSG, "File '"<<filename<<"' loaded, #Vertices: "<<mVertices.size()<<", #Normals: "<<mNormals.size()<<", #Triangles: "<<(mTriangles.size()/3)<<" ", 1 );

	gzclose( gzf );
	return 0;
gzreaderror:
	gzclose( gzf );
	errFatal("ntlGeometryObjModel::loadBobjModel","Reading GZ_BOBJ, Unable to load '"<< filename <<"', exiting...\n", SIMWORLD_INITERROR );
	return 1;
}




