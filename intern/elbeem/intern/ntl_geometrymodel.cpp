/** \file elbeem/intern/ntl_geometrymodel.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
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
	mTriangles(), mVertices(), mNormals(),
	mcAniVerts(), mcAniNorms(), 
	mcAniTimes(), mAniTimeScale(1.), mAniTimeOffset(0.)
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


/*! is the mesh animated? */
bool ntlGeometryObjModel::getMeshAnimated() { 
	const bool ret = (mcAniVerts.getSize()>1); 
	//errMsg("getMeshAnimated","ret="<<ret<<", size="<<mcAniVerts.getSize() );
	return ret;
}

/*! calculate max extends of (ani) mesh */
void ntlGeometryObjModel::getExtends(ntlVec3Gfx &sstart, ntlVec3Gfx &send) {
	bool ini=false;
	ntlVec3Gfx start(0.),end(0.);
	for(int s=0; s<=(int)mcAniVerts.accessValues().size(); s++) {
		vector<ntlVec3f> *sverts;
		if(mcAniVerts.accessValues().size()>0) {
			if(s==(int)mcAniVerts.accessValues().size()) continue;
			sverts	= &(mcAniVerts.accessValues()[s].mVerts);
		} else sverts = &mVertices;

		for(int i=0; i<(int)sverts->size(); i++) {

			if(!ini) {
				start=(*sverts)[i];
				end=(*sverts)[i];
				//errMsg("getExtends","ini "<<s<<","<<i<<" "<<start<<","<<end);
				ini=true;
			} else {
				for(int j=0; j<3; j++) {
					if(start[j] > (*sverts)[i][j]) { start[j]= (*sverts)[i][j]; }
					if(end[j]   < (*sverts)[i][j]) { end[j]  = (*sverts)[i][j]; }
				}
				//errMsg("getExtends","check "<<s<<","<<i<<" "<<start<<","<<end<<" "<< (*sverts)[i]);
			}

		}
	}
	sstart=start;
	send=end;
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
		errMsg("ntlGeometryObjModel::initialize","Filename not given!");
		return;
	}

	const char *suffix = strrchr(mFilename.c_str(), '.');
	if (suffix) {
		if (!strncasecmp(suffix, ".obj", 4)) {
			errMsg("ntlGeometryObjModel::initialize",".obj files not supported!");
			return;
		} else if (!strncasecmp(suffix, ".gz", 3)) { 
			//mType = 1; // assume its .bobj.gz
		} else if (!strncasecmp(suffix, ".bobj", 5)) { 
			//mType = 1;
		}
	}

	if(getAttributeList()->exists("ani_times") || (!mcAniTimes.isInited()) ) {
		mcAniTimes = mpAttrs->readChannelFloat("ani_times");
	}
	mAniTimeScale = mpAttrs->readFloat("ani_timescale", mAniTimeScale,"ntlGeometryObjModel", "mAniTimeScale", false);
	mAniTimeOffset = mpAttrs->readFloat("ani_timeoffset", mAniTimeOffset,"ntlGeometryObjModel", "mAniTimeOffset", false);

	// continue with standard obj
	if(loadBobjModel(mFilename)==0) mLoaded=1;
	if(!mLoaded) {
		debMsgStd("ntlGeometryObjModel",DM_WARNING,"Unable to load object file '"<<mFilename<<"' !", 0);
	}
	if(getMeshAnimated()) {
		this->mIsAnimated = true;
	}
}

/******************************************************************************
 * init model from given vertex and triangle arrays 
 *****************************************************************************/

int ntlGeometryObjModel::initModel(int numVertices, float *vertices, int numTriangles, int *triangles,
		int channelSize, float *channelVertices)
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
	int triangleErrs=0;
	for(int i=0; i<numTriangles; i++) {
		for(int j=0;j<3;j++) {
			mTriangles[3*i+j] = triangles[i*3+j];
			if(mTriangles[3*i+j]<0) { mTriangles[3*i+j]=0; triangleErrs++; }
			if(mTriangles[3*i+j]>=numVertices) { mTriangles[3*i+j]=0; triangleErrs++; }
		}
	}
	if(triangleErrs>0) {
		errMsg("ntlGeometryObjModel::initModel","Triangle errors occurred ("<<triangleErrs<<")!");
	}

	//fprintf(stderr,"initModel DEBUG %d \n",channelSize);
	debMsgStd("ntlGeometryObjModel::initModel",DM_MSG, "Csize:"<<channelSize<<", Cvert:"<<(long)(channelVertices) ,10);
	if(channelVertices && (channelSize>0)) {
		vector<ntlSetVec3f> aniverts;
		vector<ntlSetVec3f> aninorms;
		vector<double> anitimes;
		aniverts.clear();
		aninorms.clear();
		anitimes.clear();
		for(int frame=0; frame<channelSize; frame++) {
			ntlSetVec3f averts; averts.mVerts.clear();
			ntlSetVec3f anorms; anorms.mVerts.clear();
			int setsize = (3*numVertices+1);

			ntlVec3Gfx p(0.),n(1.);
			for(int i=0; i<numVertices; i++) {
				for(int j=0; j<3; j++) p[j] = channelVertices[frame*setsize+ 3*i +j];
				averts.mVerts.push_back(p);
				anorms.mVerts.push_back(p);
				//debMsgStd("ntlGeometryObjModel::initModel",DM_MSG, "Frame:"<<frame<<",i:"<<i<<" "<<p,10);
			}
			if( ((int)averts.mVerts.size()==numVertices) && 
  				((int)anorms.mVerts.size()==numVertices) ) {
				aniverts.push_back(averts);
				aninorms.push_back(anorms);
				double time = (double)channelVertices[frame*setsize+ setsize-1];
				anitimes.push_back(time);
			} else {
				errMsg("ntlGeometryObjModel::initModel","Invalid mesh, obj="<<this->getName()<<" frame="<<frame<<" verts="<<averts.mVerts.size()<<"/"<<numVertices<<". Skipping...");
			}
			//debMsgStd("ntlGeometryObjModel::initModel",DM_MSG, "Frame:"<<frame<<" at t="<<time,10);
		}

		mcAniVerts = AnimChannel<ntlSetVec3f>(aniverts,anitimes);
		mcAniNorms = AnimChannel<ntlSetVec3f>(aninorms,anitimes);
		debMsgStd("ntlGeometryObjModel::initModel",DM_MSG, "Ani sets inited: "<< mcAniVerts.accessValues().size() <<","<<mcAniNorms.accessValues().size() <<" ", 1 );
	}
	if(getMeshAnimated()) {
		this->mIsAnimated = true;
	}

	// inited, no need to parse attribs etc.
	mLoaded = 1;
	return 0;
}

/*! init triangle divisions */
void ntlGeometryObjModel::calcTriangleDivs(vector<ntlVec3Gfx> &verts, vector<ntlTriangle> &tris, gfxReal fsTri) {
	// warning - copied from geomobj calc!
	errMsg("ntlGeometryObjModel","calcTriangleDivs special!");
	mTriangleDivs1.resize( tris.size() );
	mTriangleDivs2.resize( tris.size() );
	mTriangleDivs3.resize( tris.size() );
	for(size_t i=0; i<tris.size(); i++) {
		ntlVec3Gfx p0 = verts[ tris[i].getPoints()[0] ];
		ntlVec3Gfx p1 = verts[ tris[i].getPoints()[1] ];
		ntlVec3Gfx p2 = verts[ tris[i].getPoints()[2] ];
		ntlVec3Gfx side1 = p1 - p0;
		ntlVec3Gfx side2 = p2 - p0;
		ntlVec3Gfx side3 = p1 - p2;
		int divs1=0, divs2=0, divs3=0;
		if(normNoSqrt(side1) > fsTri*fsTri) { divs1 = (int)(norm(side1)/fsTri); }
		if(normNoSqrt(side2) > fsTri*fsTri) { divs2 = (int)(norm(side2)/fsTri); }
		//if(normNoSqrt(side3) > fsTri*fsTri) { divs3 = (int)(norm(side3)/fsTri); }

		// special handling
		// warning, requires objmodel triangle treatment (no verts dups)
		if(getMeshAnimated()) {
			vector<ntlSetVec3f> &sverts = mcAniVerts.accessValues();
			for(int s=0; s<(int)sverts.size(); s++) {
				p0 = sverts[s].mVerts[ tris[i].getPoints()[0] ];
				p1 = sverts[s].mVerts[ tris[i].getPoints()[1] ];
				p2 = sverts[s].mVerts[ tris[i].getPoints()[2] ];
				side1 = p1 - p0; side2 = p2 - p0; side3 = p1 - p2;
				int tdivs1=0, tdivs2=0, tdivs3=0;
				if(normNoSqrt(side1) > fsTri*fsTri) { tdivs1 = (int)(norm(side1)/fsTri); }
				if(normNoSqrt(side2) > fsTri*fsTri) { tdivs2 = (int)(norm(side2)/fsTri); }
				if(tdivs1>divs1) divs1=tdivs1;
				if(tdivs2>divs2) divs2=tdivs2;
				if(tdivs3>divs3) divs3=tdivs3;
			}
		} // */
		mTriangleDivs1[i] = divs1;
		mTriangleDivs2[i] = divs2;
		mTriangleDivs3[i] = divs3;
	}
}


/******************************************************************************
 * load model from .obj file
 *****************************************************************************/

int ntlGeometryObjModel::loadBobjModel(string filename)
{
	bool haveAniSets=false;
	vector<ntlSetVec3f> aniverts;
	vector<ntlSetVec3f> aninorms;
	vector<double> anitimes;

	const bool debugPrint=false;
	const bool debugPrintFull=false;
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
		if(debugPrintFull) errMsg("FULLV"," "<<i<<" "<< mVertices[i] );
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
		if(debugPrintFull) errMsg("FULLN"," "<<i<<" "<< mNormals[i] );
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

	// try to load animated mesh
	aniverts.clear();
	aninorms.clear();
	anitimes.clear();
	while(1) {
		//ntlVec3Gfx check;
		float x[3];
		float frameTime=0.;
		int bytesRead = 0;
		int numNorms2=-1, numVerts2=-1;
		//for(int j=0; j<3; j++) {
			//x[j] = 0.;
			//bytesRead += gzread(gzf, &(x[j]), sizeof(float) ); 
		//}
		//check = ntlVec3Gfx(x[0],x[1],x[2]);
		//if(debugPrint) errMsg("ANI_NV1"," "<<check<<" "<<" bytes:"<<bytesRead );
		bytesRead += gzread(gzf, &frameTime, sizeof(frameTime) );
		//if(bytesRead!=3*sizeof(float)) {
		if(bytesRead!=sizeof(float)) {
			debMsgStd("ntlGeometryObjModel::loadBobjModel",DM_MSG, "File '"<<filename<<"' end of gzfile. ", 10 );
			if(anitimes.size()>0) {
				// finally init channels and stop reading file
				mcAniVerts = AnimChannel<ntlSetVec3f>(aniverts,anitimes);
				mcAniNorms = AnimChannel<ntlSetVec3f>(aninorms,anitimes);
			}
			goto gzreaddone;
		}
		bytesRead += gzread(gzf, &numVerts2, sizeof(numVerts2) );
		haveAniSets=true;
		// continue to read new set
		vector<ntlVec3Gfx> vertset;
		vector<ntlVec3Gfx> normset;
		vertset.resize(numVerts);
		normset.resize(numVerts);
		//vertset[0] = check;
		if(debugPrintFull) errMsg("FUL1V"," "<<0<<" "<< vertset[0] );

		for(int i=0; i<numVerts; i++) { // start at one!
			for(int j=0; j<3; j++) {
				bytesRead += gzread(gzf, &(x[j]), sizeof( (x[j]) ) ); 
			}
			vertset[i] = ntlVec3Gfx(x[0],x[1],x[2]);
			if(debugPrintFull) errMsg("FUL2V"," "<<i<<" "<< vertset[i] );
		}
		if(debugPrint) errMsg("ANI_VV"," "<<numVerts<<" "<< vertset.size()<<" bytes:"<<bytesRead );

		bytesRead += gzread(gzf, &numNorms2, sizeof(numNorms2) );
		for(int i=0; i<numVerts; i++) {
			for(int j=0; j<3; j++) {
				bytesRead += gzread(gzf, &(x[j]), sizeof( (x[j]) ) ); 
			}
			normset[i] = ntlVec3Gfx(x[0],x[1],x[2]);
			if(debugPrintFull) errMsg("FUL2N"," "<<i<<" "<< normset[i] );
		}
		if(debugPrint) errMsg("ANI_NV"," "<<numVerts<<","<<numVerts2<<","<<numNorms2<<","<< normset.size()<<" bytes:"<<bytesRead );

		// set ok
		if(bytesRead== (int)( (numVerts*2*3+1) *sizeof(float)+2*sizeof(int) ) ) {
			if(aniverts.size()==0) {
				// TODO, ignore first mesh?
				double anitime = (double)(frameTime-1.); // start offset!? anitimes.size();
				// get for current frame entry
				if(mcAniTimes.getSize()>1)  anitime = mcAniTimes.get(anitime); 
				anitime = anitime*mAniTimeScale+mAniTimeOffset;

				anitimes.push_back( anitime );
				aniverts.push_back( ntlSetVec3f(mVertices) );
				aninorms.push_back( ntlSetVec3f(mNormals) );
				if(debugPrint) errMsg("ANI_NV","new set "<<mVertices.size()<<","<< mNormals.size()<<" time:"<<anitime );
			}
			double anitime = (double)(frameTime); //anitimes.size();
			// get for current frame entry
			if(mcAniTimes.getSize()>1)  anitime = mcAniTimes.get(anitime); 
			anitime = anitime*mAniTimeScale+mAniTimeOffset;

			anitimes.push_back( anitime );
			aniverts.push_back( ntlSetVec3f(vertset) );
			aninorms.push_back( ntlSetVec3f(normset) );
			if(debugPrint) errMsg("ANI_NV","new set "<<vertset.size()<<","<< normset.size()<<" time:"<<anitime );
		} else {
			errMsg("ntlGeometryObjModel::loadBobjModel","Malformed ani set! Aborting... ("<<bytesRead<<") ");
			goto gzreaddone;
		}
	} // anim sets */

gzreaddone:

	if(haveAniSets) { 
		debMsgStd("ntlGeometryObjModel::loadBobjModel",DM_MSG, "File '"<<filename<<"' ani sets loaded: "<< mcAniVerts.accessValues().size() <<","<<mcAniNorms.accessValues().size() <<" ", 1 );
	}
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
ntlGeometryObjModel::getTriangles(double t, vector<ntlTriangle> *triangles, 
															vector<ntlVec3Gfx> *vertices, 
															vector<ntlVec3Gfx> *normals, int objectId )
{
	if(!mLoaded) { // invalid type...
		return;
	}
	if(mcAniVerts.getSize()>1) { mVertices = mcAniVerts.get(t).mVerts; }
	if(mcAniNorms.getSize()>1) { mNormals  = mcAniNorms.get(t).mVerts; }

	int startvert = vertices->size();
	vertices->resize( vertices->size() + mVertices.size() );
	normals->resize( normals->size() + mVertices.size() );
	for(int i=0; i<(int)mVertices.size(); i++) {
		(*vertices)[startvert+i] = mVertices[i];
		(*normals)[startvert+i] = mNormals[i];
	}

	triangles->reserve(triangles->size() + mTriangles.size()/3 );
	for(int i=0; i<(int)mTriangles.size(); i+=3) {
		int trip[3];
		trip[0] = startvert+mTriangles[i+0];
		trip[1] = startvert+mTriangles[i+1];
		trip[2] = startvert+mTriangles[i+2];

		//sceneAddTriangle( 
				//mVertices[trip[0]], mVertices[trip[1]], mVertices[trip[2]], 
				//mNormals[trip[0]], mNormals[trip[1]], mNormals[trip[2]], 
				//ntlVec3Gfx(0.0), 1 , triangles,vertices,normals ); /* normal unused */
		sceneAddTriangleNoVert( trip, ntlVec3Gfx(0.0), 1 , triangles ); /* normal unused */
	}
	objectId = -1; // remove warning
	// bobj
	return;
}





