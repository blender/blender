/** \file elbeem/intern/ntl_geometryobject.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * a geometry object
 * all other geometry objects are derived from this one
 *
 *****************************************************************************/


#include "ntl_geometryobject.h"
#include "ntl_world.h"
#include "ntl_matrices.h"

// for FGI
#include "elbeem.h"

#define TRI_UVOFFSET (1./4.)
//#define TRI_UVOFFSET (1./3.)


/*****************************************************************************/
/* Default constructor */
/*****************************************************************************/
ntlGeometryObject::ntlGeometryObject() :
	mIsInitialized(false), mpMaterial( NULL ),
	mMaterialName( "default" ),
	mCastShadows( 1 ), mReceiveShadows( 1 ),
	mGeoInitType( 0 ), 
	mInitialVelocity(0.0), mcInitialVelocity(0.0), mLocalCoordInivel(false),
	mGeoInitIntersect(false),
	mGeoPartSlipValue(0.0),
	mcGeoImpactFactor(1.),
	mVolumeInit(VOLUMEINIT_VOLUME),
	mInitialPos(0.),
	mcTrans(0.), mcRot(0.), mcScale(1.),
	mIsAnimated(false),
	mMovPoints(), mMovNormals(),
	mHaveCachedMov(false),
	mCachedMovPoints(), mCachedMovNormals(),
	mTriangleDivs1(), mTriangleDivs2(),
	mMovPntsInited(-100.0), mMaxMovPnt(-1),
	mcGeoActive(1.),
	mCpsTimeStart(0.), mCpsTimeEnd(1.0), mCpsQuality(10.),
	mcAttrFStr(0.),mcAttrFRad(0.), mcVelFStr(0.), mcVelFRad(0.)
{ 
};


/*****************************************************************************/
/* Default destructor */
/*****************************************************************************/
ntlGeometryObject::~ntlGeometryObject() 
{
}

/*! is the mesh animated? */
bool ntlGeometryObject::getMeshAnimated() {
	// off by default, on for e.g. ntlGeometryObjModel
	return false; 
}

/*! init object anim flag */
bool ntlGeometryObject::checkIsAnimated() {
	if(    (mcTrans.accessValues().size()>1)  // VALIDATE
	    || (mcRot.accessValues().size()>1) 
	    || (mcScale.accessValues().size()>1) 
	    || (mcGeoActive.accessValues().size()>1) 
			// mcGeoImpactFactor only needed when moving
	    || (mcInitialVelocity.accessValues().size()>1) 
		) {
		mIsAnimated = true;
	}

	// fluid objects always have static init!
	if(mGeoInitType==FGI_FLUID) {
		mIsAnimated=false;
	}
	//errMsg("ntlGeometryObject::checkIsAnimated","obj="<<getName()<<" debug: trans:"<<mcTrans.accessValues().size()<<" rot:"<<mcRot.accessValues().size()<<" scale:"<<mcScale.accessValues().size()<<" active:"<<mcGeoActive.accessValues().size()<<" inivel:"<<mcInitialVelocity.accessValues().size()<<". isani?"<<mIsAnimated ); // DEBUG
	return mIsAnimated;
}

/*****************************************************************************/
/* Init attributes etc. of this object */
/*****************************************************************************/
#define GEOINIT_STRINGS  10
static const char *initStringStrs[GEOINIT_STRINGS] = {
	"fluid",
	"bnd_no","bnd_noslip",
	"bnd_free","bnd_freeslip",
	"bnd_part","bnd_partslip",
	"inflow", "outflow", "control",
};
static int initStringTypes[GEOINIT_STRINGS] = {
	FGI_FLUID,
	FGI_BNDNO, FGI_BNDNO,
	FGI_BNDFREE, FGI_BNDFREE,
	FGI_BNDPART, FGI_BNDPART,
	FGI_MBNDINFLOW, FGI_MBNDOUTFLOW, 
	FGI_CONTROL
};
void ntlGeometryObject::initialize(ntlRenderGlobals *glob) 
{
	//debugOut("ntlGeometryObject::initialize: '"<<getName()<<"' ", 10);
	// initialize only once...
	if(mIsInitialized) return;
	
	// init material, always necessary
	searchMaterial( glob->getMaterials() );
	
	this->mGeoInitId = mpAttrs->readInt("geoinitid", this->mGeoInitId,"ntlGeometryObject", "mGeoInitId", false);
	mGeoInitIntersect = mpAttrs->readInt("geoinit_intersect", mGeoInitIntersect,"ntlGeometryObject", "mGeoInitIntersect", false);
	string ginitStr = mpAttrs->readString("geoinittype", "", "ntlGeometryObject", "mGeoInitType", false);
	if(this->mGeoInitId>=0) {
		bool gotit = false;
		for(int i=0; i<GEOINIT_STRINGS; i++) {
			if(ginitStr== initStringStrs[i]) {
				gotit = true;
				mGeoInitType = initStringTypes[i];
			}
		}

		if(!gotit) {
			errFatal("ntlGeometryObject::initialize","Obj '"<<mName<<"', Unknown 'geoinittype' value: '"<< ginitStr <<"' ", SIMWORLD_INITERROR);
			return;
		}
	}

	int geoActive = mpAttrs->readInt("geoinitactive", 1,"ntlGeometryObject", "geoActive", false);
	if(!geoActive) {
		// disable geo init again...
		this->mGeoInitId = -1;
	}
	mInitialVelocity  = vec2G( mpAttrs->readVec3d("initial_velocity", vec2D(mInitialVelocity),"ntlGeometryObject", "mInitialVelocity", false));
	if(getAttributeList()->exists("initial_velocity") || (!mcInitialVelocity.isInited()) ) {
		mcInitialVelocity = mpAttrs->readChannelVec3f("initial_velocity");
	}
	// always use channel
	if(!mcInitialVelocity.isInited()) { mcInitialVelocity = AnimChannel<ntlVec3Gfx>(mInitialVelocity); }
	mLocalCoordInivel = mpAttrs->readBool("geoinit_localinivel", mLocalCoordInivel,"ntlGeometryObject", "mLocalCoordInivel", false);

	mGeoPartSlipValue = mpAttrs->readFloat("geoinit_partslip", mGeoPartSlipValue,"ntlGeometryObject", "mGeoPartSlipValue", false);
	bool mOnlyThinInit = false; // deprecated!
	mOnlyThinInit     = mpAttrs->readBool("geoinit_onlythin", mOnlyThinInit,"ntlGeometryObject", "mOnlyThinInit", false);
	if(mOnlyThinInit) mVolumeInit = VOLUMEINIT_SHELL;
	mVolumeInit     = mpAttrs->readInt("geoinit_volumeinit", mVolumeInit,"ntlGeometryObject", "mVolumeInit", false);
	if((mVolumeInit<VOLUMEINIT_VOLUME)||(mVolumeInit>VOLUMEINIT_BOTH)) mVolumeInit = VOLUMEINIT_VOLUME;

	// moving obs correction factor
	float impactfactor=1.;
	impactfactor = (float)mpAttrs->readFloat("impactfactor", impactfactor,"ntlGeometryObject", "impactfactor", false);
	if(getAttributeList()->exists("impactfactor") || (!mcGeoImpactFactor.isInited()) ) {
		mcGeoImpactFactor = mpAttrs->readChannelSinglePrecFloat("impactfactor");
	}

	// override cfg types
	mVisible = mpAttrs->readBool("visible", mVisible,"ntlGeometryObject", "mVisible", false);
	mReceiveShadows = mpAttrs->readBool("recv_shad", mReceiveShadows,"ntlGeometryObject", "mReceiveShadows", false);
	mCastShadows = mpAttrs->readBool("cast_shad", mCastShadows,"ntlGeometryObject", "mCastShadows", false);

	// read mesh animation channels
	ntlVec3d translation(0.0);
	translation = mpAttrs->readVec3d("translation", translation,"ntlGeometryObject", "translation", false);
	if(getAttributeList()->exists("translation") || (!mcTrans.isInited()) ) {
		mcTrans = mpAttrs->readChannelVec3f("translation");
	}
	ntlVec3d rotation(0.0);
	rotation = mpAttrs->readVec3d("rotation", rotation,"ntlGeometryObject", "rotation", false);
	if(getAttributeList()->exists("rotation") || (!mcRot.isInited()) ) {
		mcRot = mpAttrs->readChannelVec3f("rotation");
	}
	ntlVec3d scale(1.0);
	scale = mpAttrs->readVec3d("scale", scale,"ntlGeometryObject", "scale", false);
	if(getAttributeList()->exists("scale") || (!mcScale.isInited()) ) {
		mcScale = mpAttrs->readChannelVec3f("scale");
	}

	float geoactive=1.;
	geoactive = (float)mpAttrs->readFloat("geoactive", geoactive,"ntlGeometryObject", "geoactive", false);
	if(getAttributeList()->exists("geoactive") || (!mcGeoActive.isInited()) ) {
		mcGeoActive = mpAttrs->readChannelSinglePrecFloat("geoactive");
	}
	// always use channel
	if(!mcGeoActive.isInited()) { mcGeoActive = AnimChannel<float>(geoactive); }

	checkIsAnimated();

	mIsInitialized = true;
	debMsgStd("ntlGeometryObject::initialize",DM_MSG,"GeoObj '"<<this->getName()<<"': visible="<<this->mVisible<<" gid="<<this->mGeoInitId<<" gtype="<<mGeoInitType<<","<<ginitStr<<
			" gvel="<<mInitialVelocity<<" gisect="<<mGeoInitIntersect, 10); // debug
}

/*! notify object that dump is in progress (e.g. for particles) */
// default action - do nothing...
void ntlGeometryObject::notifyOfDump(int dumtp, int frameNr,char *frameNrStr,string outfilename, double simtime) {
  bool debugOut=false;
	if(debugOut) debMsgStd("ntlGeometryObject::notifyOfDump",DM_MSG," dt:"<<dumtp<<" obj:"<<this->getName()<<" frame:"<<frameNrStr<<","<<frameNr<<",t"<<simtime<<" to "<<outfilename, 10); // DEBUG
}

/*****************************************************************************/
/* Search the material for this object from the material list */
/*****************************************************************************/
void ntlGeometryObject::searchMaterial(vector<ntlMaterial *> *mat)
{
	/* search the list... */
	int i=0;
	for (vector<ntlMaterial*>::iterator iter = mat->begin();
         iter != mat->end(); iter++) {
		if( mMaterialName == (*iter)->getName() ) {
			//warnMsg("ntlGeometryObject::searchMaterial","for obj '"<<getName()<<"' found - '"<<(*iter)->getName()<<"' "<<i); // DEBUG
			mpMaterial = (*iter);
			return;
		}
		i++;
	}
	errFatal("ntlGeometryObject::searchMaterial","Unknown material '"<<mMaterialName<<"' ! ", SIMWORLD_INITERROR);
	mpMaterial = new ntlMaterial();
	return;
}

/******************************************************************************
 * static add triangle function
 *****************************************************************************/
void ntlGeometryObject::sceneAddTriangle(
		ntlVec3Gfx  p1,ntlVec3Gfx  p2,ntlVec3Gfx  p3,
		ntlVec3Gfx pn1,ntlVec3Gfx pn2,ntlVec3Gfx pn3,
		ntlVec3Gfx trin, bool smooth,
		vector<ntlTriangle> *triangles,
		vector<ntlVec3Gfx>  *vertices,
		vector<ntlVec3Gfx>  *normals) {
	ntlTriangle tri;
	int tempVert;
  
	if(normals->size() != vertices->size()) {
		errFatal("ntlGeometryObject::sceneAddTriangle","For '"<<this->mName<<"': Vertices and normals sizes to not match!!!",SIMWORLD_GENERICERROR);

	} else {
		
		vertices->push_back( p1 ); 
		normals->push_back( pn1 ); 
		tempVert = normals->size()-1;
		tri.getPoints()[0] = tempVert;
		
		vertices->push_back( p2 ); 
		normals->push_back( pn2 ); 
		tempVert = normals->size()-1;
		tri.getPoints()[1] = tempVert;
		
		vertices->push_back( p3 ); 
		normals->push_back( pn3 ); 
		tempVert = normals->size()-1;
		tri.getPoints()[2] = tempVert;
		
		
		/* init flags from ntl_ray.h */
		int flag = 0; 
		if(getVisible()){ flag |= TRI_GEOMETRY; }
		if(getCastShadows() ) { 
			flag |= TRI_CASTSHADOWS; } 
		
		/* init geo init id */
		int geoiId = getGeoInitId(); 
		//if((geoiId > 0) && (mVolumeInit&VOLUMEINIT_VOLUME) && (!mIsAnimated)) { 
		if((geoiId > 0) && (mVolumeInit&VOLUMEINIT_VOLUME)) { 
			flag |= (1<< (geoiId+4)); 
			flag |= mGeoInitType; 
		} 
		/*errMsg("ntlScene::addTriangle","DEBUG flag="<<convertFlags2String(flag) ); */ 
		tri.setFlags( flag );
		
		/* triangle normal missing */
		tri.setNormal( trin );
		tri.setSmoothNormals( smooth );
		tri.setObjectId( this->mObjectId );
		triangles->push_back( tri ); 
	} /* normals check*/ 
}

void ntlGeometryObject::sceneAddTriangleNoVert(int *trips,
		ntlVec3Gfx trin, bool smooth,
		vector<ntlTriangle> *triangles) {
	ntlTriangle tri;
		
	tri.getPoints()[0] = trips[0];
	tri.getPoints()[1] = trips[1];
	tri.getPoints()[2] = trips[2];

	// same as normal sceneAddTriangle

	/* init flags from ntl_ray.h */
	int flag = 0; 
	if(getVisible()){ flag |= TRI_GEOMETRY; }
	if(getCastShadows() ) { 
		flag |= TRI_CASTSHADOWS; } 

	/* init geo init id */
	int geoiId = getGeoInitId(); 
	if((geoiId > 0) && (mVolumeInit&VOLUMEINIT_VOLUME)) { 
		flag |= (1<< (geoiId+4)); 
		flag |= mGeoInitType; 
	} 
	/*errMsg("ntlScene::addTriangle","DEBUG flag="<<convertFlags2String(flag) ); */ 
	tri.setFlags( flag );

	/* triangle normal missing */
	tri.setNormal( trin );
	tri.setSmoothNormals( smooth );
	tri.setObjectId( this->mObjectId );
	triangles->push_back( tri ); 
}


/******************************************************************************/
/* Init channels from float arrays (for elbeem API) */
/******************************************************************************/

#define ADD_CHANNEL_VEC(dst,nvals,val) \
		vals.clear(); time.clear(); elbeemSimplifyChannelVec3(val,&nvals); \
		for(int i=0; i<(nvals); i++) { \
			vals.push_back(ntlVec3Gfx((val)[i*4+0], (val)[i*4+1],(val)[i*4+2] )); \
			time.push_back( (val)[i*4+3] ); \
		} \
		(dst) = AnimChannel< ntlVec3Gfx >(vals,time); 

#define ADD_CHANNEL_FLOAT(dst,nvals,val) \
		valsfloat.clear(); time.clear(); elbeemSimplifyChannelFloat(val,&nvals); \
		for(int i=0; i<(nvals); i++) { \
			valsfloat.push_back( (val)[i*2+0] ); \
			time.push_back( (val)[i*2+1] ); \
		} \
		(dst) = AnimChannel< float >(valsfloat,time); 

void ntlGeometryObject::initChannels(
		int nTrans, float *trans, int nRot, float *rot, int nScale, float *scale,
		int nAct, float *act, int nIvel, float *ivel,
		int nAttrFStr, float *attrFStr,
		int nAttrFRad, float *attrFRad,
		int nVelFStr, float *velFStr,
		int nVelFRad, float *velFRad
		) {
	const bool debugInitc=true;
	if(debugInitc) { debMsgStd("ntlGeometryObject::initChannels",DM_MSG,"nt:"<<nTrans<<" nr:"<<nRot<<" ns:"<<nScale, 10); 
	                 debMsgStd("ntlGeometryObject::initChannels",DM_MSG,"na:"<<nAct<<" niv:"<<nIvel<<" ", 10); }
	vector<ntlVec3Gfx> vals;
	vector<float>  valsfloat;
	vector<double> time;
	if((trans)&&(nTrans>0)) {  ADD_CHANNEL_VEC(mcTrans, nTrans, trans); }
	if((rot)&&(nRot>0)) {      ADD_CHANNEL_VEC(mcRot, nRot, rot); }
	if((scale)&&(nScale>0)) {  ADD_CHANNEL_VEC(mcScale, nScale, scale); }
	if((act)&&(nAct>0)) {      ADD_CHANNEL_FLOAT(mcGeoActive, nAct, act); }
	if((ivel)&&(nIvel>0)) {    ADD_CHANNEL_VEC(mcInitialVelocity, nIvel, ivel); }
	
	/* fluid control channels */
	if((attrFStr)&&(nAttrFStr>0)) { ADD_CHANNEL_FLOAT(mcAttrFStr, nAttrFStr, attrFStr); }
	if((attrFRad)&&(nAttrFRad>0)) { ADD_CHANNEL_FLOAT(mcAttrFRad, nAttrFRad, attrFRad); }
	if((velFStr)&&(nVelFStr>0)) {   ADD_CHANNEL_FLOAT(mcVelFStr, nAct, velFStr); }
	if((velFRad)&&(nVelFRad>0)) {   ADD_CHANNEL_FLOAT(mcVelFRad, nVelFRad, velFRad); }

	checkIsAnimated();
	
	if(debugInitc) { 
		debMsgStd("ntlGeometryObject::initChannels",DM_MSG,getName()<<
				" nt:"<<mcTrans.accessValues().size()<<" nr:"<<mcRot.accessValues().size()<<
				" ns:"<<mcScale.accessValues().size()<<" isAnim:"<<mIsAnimated, 10); }

	if(debugInitc) {
		std::ostringstream ostr;
		ostr << "trans: ";
		for(size_t i=0; i<mcTrans.accessValues().size(); i++) {
			ostr<<" "<<mcTrans.accessValues()[i]<<"@"<<mcTrans.accessTimes()[i]<<" ";
		} ostr<<";   ";
		ostr<<"rot: ";
		for(size_t i=0; i<mcRot.accessValues().size(); i++) {
			ostr<<" "<<mcRot.accessValues()[i]<<"@"<<mcRot.accessTimes()[i]<<" ";
		} ostr<<";   ";
		ostr<<"scale: ";
		for(size_t i=0; i<mcScale.accessValues().size(); i++) {
			ostr<<" "<<mcScale.accessValues()[i]<<"@"<<mcScale.accessTimes()[i]<<" ";
		} ostr<<";   ";
		ostr<<"act: ";
		for(size_t i=0; i<mcGeoActive.accessValues().size(); i++) {
			ostr<<" "<<mcGeoActive.accessValues()[i]<<"@"<<mcGeoActive.accessTimes()[i]<<" ";
		} ostr<<";   ";
		ostr<<"ivel: ";
		for(size_t i=0; i<mcInitialVelocity.accessValues().size(); i++) {
			ostr<<" "<<mcInitialVelocity.accessValues()[i]<<"@"<<mcInitialVelocity.accessTimes()[i]<<" ";
		} ostr<<";   ";
		debMsgStd("ntlGeometryObject::initChannels",DM_MSG,"Inited "<<ostr.str(),10);
	}
}
#undef ADD_CHANNEL


/*****************************************************************************/
/* apply object translation at time t*/
/*****************************************************************************/
void ntlGeometryObject::applyTransformation(double t, vector<ntlVec3Gfx> *verts, vector<ntlVec3Gfx> *norms, int vstart, int vend, int forceTrafo) {
	if(    (mcTrans.accessValues().size()>1)  // VALIDATE
	    || (mcRot.accessValues().size()>1) 
	    || (mcScale.accessValues().size()>1) 
	    || (forceTrafo)
	    || (!mHaveCachedMov)
		) {
		// transformation is animated, continue
		ntlVec3Gfx pos = getTranslation(t); 
		ntlVec3Gfx scale = mcScale.get(t);
		ntlVec3Gfx rot = mcRot.get(t);
		ntlMat4Gfx rotMat;
		rotMat.initRotationXYZ(rot[0],rot[1],rot[2]);
		pos += mInitialPos;
		errMsg("ntlGeometryObject::applyTransformation","obj="<<getName()<<" t"<<pos<<" r"<<rot<<" s"<<scale);
		for(int i=vstart; i<vend; i++) {
			(*verts)[i] *= scale;
			(*verts)[i] = rotMat * (*verts)[i];
			(*verts)[i] += pos;
			//if(i<10) errMsg("ntlGeometryObject::applyTransformation"," v"<<i<<"/"<<vend<<"="<<(*verts)[i]);
		}
		if(norms) {
			for(int i=vstart; i<vend; i++) {
				(*norms)[i] = rotMat * (*norms)[i];
			}
		}
	} else {
		// not animated, cached points were already returned
		errMsg ("ntlGeometryObject::applyTransformation","Object "<<getName()<<" used cached points ");
	}
}

/*! init triangle divisions */
void ntlGeometryObject::calcTriangleDivs(vector<ntlVec3Gfx> &verts, vector<ntlTriangle> &tris, gfxReal fsTri) {
	mTriangleDivs1.resize( tris.size() );
	mTriangleDivs2.resize( tris.size() );

	//fsTri *= 2.; // DEBUG! , wrong init!

	for(size_t i=0; i<tris.size(); i++) {
		const ntlVec3Gfx p0 = verts[ tris[i].getPoints()[0] ];
		const ntlVec3Gfx p1 = verts[ tris[i].getPoints()[1] ];
		const ntlVec3Gfx p2 = verts[ tris[i].getPoints()[2] ];
		const ntlVec3Gfx side1 = p1 - p0;
		const ntlVec3Gfx side2 = p2 - p0;
		int divs1=0, divs2=0;
		if(normNoSqrt(side1) > fsTri*fsTri) { divs1 = (int)(norm(side1)/fsTri); }
		if(normNoSqrt(side2) > fsTri*fsTri) { divs2 = (int)(norm(side2)/fsTri); }

		mTriangleDivs1[i] = divs1;
		mTriangleDivs2[i] = divs2;
	}
}

/*! Prepare points for moving objects */
void ntlGeometryObject::initMovingPoints(double time, gfxReal featureSize) {
	if((mMovPntsInited==featureSize)&&(!getMeshAnimated())) return;
	const bool debugMoinit=false;

	vector<ntlTriangle> triangles; 
	vector<ntlVec3Gfx> vertices; 
	vector<ntlVec3Gfx> vnormals; 
	int objectId = 1;
	this->getTriangles(time, &triangles,&vertices,&vnormals,objectId);
	
	mMovPoints.clear();
	mMovNormals.clear();
	if(debugMoinit) errMsg("ntlGeometryObject::initMovingPoints","Object "<<getName()<<" has v:"<<vertices.size()<<" t:"<<triangles.size() );
	// no points?
	if(vertices.size()<1) {
		mMaxMovPnt=-1;
		return; 
	}
	ntlVec3f maxscale = channelFindMaxVf(mcScale);
	float maxpart = ABS(maxscale[0]);
	if(ABS(maxscale[1])>maxpart) maxpart = ABS(maxscale[1]);
	if(ABS(maxscale[2])>maxpart) maxpart = ABS(maxscale[2]);
	float scaleFac = 1.0/(maxpart);
	// TODO - better reinit from time to time?
	const gfxReal fsTri = featureSize*0.5 *scaleFac;
	if(debugMoinit) errMsg("ntlGeometryObject::initMovingPoints","maxscale:"<<maxpart<<" featureSize:"<<featureSize<<" fsTri:"<<fsTri );

	if(mTriangleDivs1.size()!=triangles.size()) {
		calcTriangleDivs(vertices,triangles,fsTri);
	}

	// debug: count points to init
	/*if(debugMoinit) {
		errMsg("ntlGeometryObject::initMovingPoints","Object "<<getName()<<" estimating...");
		int countp=vertices.size()*2;
		for(size_t i=0; i<triangles.size(); i++) {
			ntlVec3Gfx p0 = vertices[ triangles[i].getPoints()[0] ];
			ntlVec3Gfx side1 = vertices[ triangles[i].getPoints()[1] ] - p0;
			ntlVec3Gfx side2 = vertices[ triangles[i].getPoints()[2] ] - p0;
			int divs1=0, divs2=0;
			if(normNoSqrt(side1) > fsTri*fsTri) { divs1 = (int)(norm(side1)/fsTri); }
			if(normNoSqrt(side2) > fsTri*fsTri) { divs2 = (int)(norm(side2)/fsTri); }
			errMsg("ntlGeometryObject::initMovingPoints","tri:"<<i<<" p:"<<p0<<" s1:"<<side1<<" s2:"<<side2<<" -> "<<divs1<<","<<divs2 );
			if(divs1+divs2 > 0) {
				for(int u=0; u<=divs1; u++) {
					for(int v=0; v<=divs2; v++) {
						const gfxReal uf = (gfxReal)(u+TRI_UVOFFSET) / (gfxReal)(divs1+0.0);
						const gfxReal vf = (gfxReal)(v+TRI_UVOFFSET) / (gfxReal)(divs2+0.0);
						if(uf+vf>1.0) continue;
						countp+=2;
					}
				}
			}
		}
		errMsg("ntlGeometryObject::initMovingPoints","Object "<<getName()<<" requires:"<<countp*2);
	} // */

	bool discardInflowBack = false;
	if( (mGeoInitType==FGI_MBNDINFLOW) && (mcInitialVelocity.accessValues().size()<1) ) discardInflowBack = true;
	discardInflowBack = false; // DEBUG disable for now


	// init std points
	for(size_t i=0; i<vertices.size(); i++) {
		ntlVec3Gfx p = vertices[ i ];
		ntlVec3Gfx n = vnormals[ i ];
		// discard inflow backsides
		//if( (mGeoInitType==FGI_MBNDINFLOW) && (!mIsAnimated)) {
		if(discardInflowBack) { //if( (mGeoInitType==FGI_MBNDINFLOW) && (!mIsAnimated)) {
			if(dot(mInitialVelocity,n)<0.0) continue;
		}
		mMovPoints.push_back(p);
		mMovNormals.push_back(n);
		if(debugMoinit) errMsg("ntlGeometryObject::initMovingPoints","std"<<i<<" p"<<p<<" n"<<n<<" ");
	}
	// init points & refine...
	for(size_t i=0; i<triangles.size(); i++) {
		int *trips = triangles[i].getPoints();
		const ntlVec3Gfx p0 = vertices[ trips[0] ];
		const ntlVec3Gfx side1 = vertices[ trips[1] ] - p0;
		const ntlVec3Gfx side2 = vertices[ trips[2] ] - p0;
		int divs1=mTriangleDivs1[i], divs2=mTriangleDivs2[i];
		
		const ntlVec3Gfx trinormOrg = getNormalized(cross(side1,side2));
		const ntlVec3Gfx trinorm = trinormOrg*0.25*featureSize;
		if(discardInflowBack) { 
			if(dot(mInitialVelocity,trinorm)<0.0) continue;
		}
		if(debugMoinit) errMsg("ntlGeometryObject::initMovingPoints","Tri1 "<<vertices[trips[0]]<<","<<vertices[trips[1]]<<","<<vertices[trips[2]]<<" "<<divs1<<","<<divs2 );
		if(divs1+divs2 > 0) {
			for(int u=0; u<=divs1; u++) {
				for(int v=0; v<=divs2; v++) {
					const gfxReal uf = (gfxReal)(u+TRI_UVOFFSET) / (gfxReal)(divs1+0.0);
					const gfxReal vf = (gfxReal)(v+TRI_UVOFFSET) / (gfxReal)(divs2+0.0);
					if(uf+vf>1.0) continue;
					ntlVec3Gfx p = 
						vertices[ trips[0] ] * (1.0-uf-vf)+
						vertices[ trips[1] ] * uf +
						vertices[ trips[2] ] * vf;
					//ntlVec3Gfx n = vnormals[ 
						//trips[0] ] * (1.0-uf-vf)+
						//vnormals[ trips[1] ]*uf +
						//vnormals[ trips[2] ]*vf;
					//normalize(n);
					// discard inflow backsides

					mMovPoints.push_back(p + trinorm); // NEW!?
					mMovPoints.push_back(p - trinorm);
					mMovNormals.push_back(trinormOrg);
					mMovNormals.push_back(trinormOrg); 
					//errMsg("TRINORM","p"<<p<<" n"<<n<<" trin"<<trinorm);
				}
			}
		}
	}

	// find max point
	mMaxMovPnt = 0;
	gfxReal dist = normNoSqrt(mMovPoints[0]);
	for(size_t i=0; i<mMovPoints.size(); i++) {
		if(normNoSqrt(mMovPoints[i])>dist) {
			mMaxMovPnt = i;
			dist = normNoSqrt(mMovPoints[0]);
		}
	}

	if(    (this->getMeshAnimated())
      || (mcTrans.accessValues().size()>1)  // VALIDATE
	    || (mcRot.accessValues().size()>1) 
	    || (mcScale.accessValues().size()>1) 
		) {
		// also do trafo...
	} else {
		mCachedMovPoints = mMovPoints;
		mCachedMovNormals = mMovNormals;
		//applyTransformation(time, &mCachedMovPoints, &mCachedMovNormals, 0, mCachedMovPoints.size(), true);
		applyTransformation(time, &mCachedMovPoints, NULL, 0, mCachedMovPoints.size(), true);
		mHaveCachedMov = true;
		debMsgStd("ntlGeometryObject::initMovingPoints",DM_MSG,"Object "<<getName()<<" cached points ", 7);
	}

	mMovPntsInited = featureSize;
	debMsgStd("ntlGeometryObject::initMovingPoints",DM_MSG,"Object "<<getName()<<" inited v:"<<vertices.size()<<"->"<<mMovPoints.size() , 5);
}
/*! Prepare points for animated objects,
 * init both sets, never used cached points 
 * discardInflowBack ignore */
void ntlGeometryObject::initMovingPointsAnim(
		 double srctime, vector<ntlVec3Gfx> &srcmovPoints,
		 double dsttime, vector<ntlVec3Gfx> &dstmovPoints,
		 vector<ntlVec3Gfx> *dstmovNormals,
		 gfxReal featureSize,
		 ntlVec3Gfx geostart, ntlVec3Gfx geoend
		 ) {
	const bool debugMoinit=false;

	vector<ntlTriangle> srctriangles; 
	vector<ntlVec3Gfx> srcvertices; 
	vector<ntlVec3Gfx> unused_normals; 
	vector<ntlTriangle> dsttriangles; 
	vector<ntlVec3Gfx> dstvertices; 
	vector<ntlVec3Gfx> dstnormals; 
	int objectId = 1;
	// TODO optimize? , get rid of normals?
	unused_normals.clear();
	this->getTriangles(srctime, &srctriangles,&srcvertices,&unused_normals,objectId);
	unused_normals.clear();
	this->getTriangles(dsttime, &dsttriangles,&dstvertices,&dstnormals,objectId);
	
	srcmovPoints.clear();
	dstmovPoints.clear();
	if(debugMoinit) errMsg("ntlGeometryObject::initMovingPointsAnim","Object "<<getName()<<" has srcv:"<<srcvertices.size()<<" srct:"<<srctriangles.size() );
	if(debugMoinit) errMsg("ntlGeometryObject::initMovingPointsAnim","Object "<<getName()<<" has dstv:"<<dstvertices.size()<<" dstt:"<<dsttriangles.size() );
	// no points?
	if(srcvertices.size()<1) {
		mMaxMovPnt=-1;
		return; 
	}
	if((srctriangles.size() != dsttriangles.size()) ||
	   (srcvertices.size() != dstvertices.size()) ) {
		errMsg("ntlGeometryObject::initMovingPointsAnim","Invalid triangle numbers! Aborting...");
		return;
	}
	ntlVec3f maxscale = channelFindMaxVf(mcScale);
	float maxpart = ABS(maxscale[0]);
	if(ABS(maxscale[1])>maxpart) maxpart = ABS(maxscale[1]);
	if(ABS(maxscale[2])>maxpart) maxpart = ABS(maxscale[2]);
	float scaleFac = 1.0/(maxpart);
	// TODO - better reinit from time to time?
	const gfxReal fsTri = featureSize*0.5 *scaleFac;
	if(debugMoinit) errMsg("ntlGeometryObject::initMovingPointsAnim","maxscale:"<<maxpart<<" featureSize:"<<featureSize<<" fsTri:"<<fsTri );

	if(mTriangleDivs1.size()!=srctriangles.size()) {
		calcTriangleDivs(srcvertices,srctriangles,fsTri);
	}


	// init std points
	for(size_t i=0; i<srcvertices.size(); i++) {
		srcmovPoints.push_back(srcvertices[i]);
		//srcmovNormals.push_back(srcnormals[i]);
	}
	for(size_t i=0; i<dstvertices.size(); i++) {
		dstmovPoints.push_back(dstvertices[i]);
		if(dstmovNormals) (*dstmovNormals).push_back(dstnormals[i]);
	}
	if(debugMoinit) errMsg("ntlGeometryObject::initMovingPointsAnim","stats src:"<<srcmovPoints.size()<<" dst:"<<dstmovPoints.size()<<" " );
	// init points & refine...
	for(size_t i=0; i<srctriangles.size(); i++) {
		const int divs1=mTriangleDivs1[i];
		const int	divs2=mTriangleDivs2[i];
		if(divs1+divs2 > 0) {
			int *srctrips = srctriangles[i].getPoints();
			int *dsttrips = dsttriangles[i].getPoints();
			const ntlVec3Gfx srcp0 =    srcvertices[ srctrips[0] ];
			const ntlVec3Gfx srcside1 = srcvertices[ srctrips[1] ] - srcp0;
			const ntlVec3Gfx srcside2 = srcvertices[ srctrips[2] ] - srcp0;
			const ntlVec3Gfx dstp0 =    dstvertices[ dsttrips[0] ];
			const ntlVec3Gfx dstside1 = dstvertices[ dsttrips[1] ] - dstp0;
			const ntlVec3Gfx dstside2 = dstvertices[ dsttrips[2] ] - dstp0;
			const ntlVec3Gfx src_trinorm    = getNormalized(cross(srcside1,srcside2))*0.25*featureSize;
			const ntlVec3Gfx dst_trinormOrg = getNormalized(cross(dstside1,dstside2));
			const ntlVec3Gfx dst_trinorm    = dst_trinormOrg                         *0.25*featureSize;
			//errMsg("ntlGeometryObject::initMovingPointsAnim","Tri1 "<<srcvertices[srctrips[0]]<<","<<srcvertices[srctrips[1]]<<","<<srcvertices[srctrips[2]]<<" "<<divs1<<","<<divs2 );
			for(int u=0; u<=divs1; u++) {
				for(int v=0; v<=divs2; v++) {
					const gfxReal uf = (gfxReal)(u+TRI_UVOFFSET) / (gfxReal)(divs1+0.0);
					const gfxReal vf = (gfxReal)(v+TRI_UVOFFSET) / (gfxReal)(divs2+0.0);
					if(uf+vf>1.0) continue;
					ntlVec3Gfx srcp = 
						srcvertices[ srctrips[0] ] * (1.0-uf-vf)+
						srcvertices[ srctrips[1] ] * uf +
						srcvertices[ srctrips[2] ] * vf;
					ntlVec3Gfx dstp = 
						dstvertices[ dsttrips[0] ] * (1.0-uf-vf)+
						dstvertices[ dsttrips[1] ] * uf +
						dstvertices[ dsttrips[2] ] * vf;

					// cutoffDomain
					if((srcp[0]<geostart[0]) && (dstp[0]<geostart[0])) continue;
					if((srcp[1]<geostart[1]) && (dstp[1]<geostart[1])) continue;
					if((srcp[2]<geostart[2]) && (dstp[2]<geostart[2])) continue;
					if((srcp[0]>geoend[0]  ) && (dstp[0]>geoend[0]  )) continue;
					if((srcp[1]>geoend[1]  ) && (dstp[1]>geoend[1]  )) continue;
					if((srcp[2]>geoend[2]  ) && (dstp[2]>geoend[2]  )) continue;
					
					srcmovPoints.push_back(srcp+src_trinorm); // SURFENHTEST
					srcmovPoints.push_back(srcp-src_trinorm);

					dstmovPoints.push_back(dstp+dst_trinorm); // SURFENHTEST
					dstmovPoints.push_back(dstp-dst_trinorm);
					if(dstmovNormals) {
						(*dstmovNormals).push_back(dst_trinormOrg);
						(*dstmovNormals).push_back(dst_trinormOrg); }
				}
			}
		}
	}

	// find max point not necessary
	debMsgStd("ntlGeometryObject::initMovingPointsAnim",DM_MSG,"Object "<<getName()<<" inited v:"<<srcvertices.size()<<"->"<<srcmovPoints.size()<<","<<dstmovPoints.size() , 5);
}

/*! Prepare points for moving objects */
void ntlGeometryObject::getMovingPoints(vector<ntlVec3Gfx> &ret, vector<ntlVec3Gfx> *norms) {
	if(mHaveCachedMov) {
		ret = mCachedMovPoints;
		if(norms) { 
			*norms = mCachedMovNormals; 
			//errMsg("ntlGeometryObject","getMovingPoints - Normals currently unused!");
		}
		//errMsg ("ntlGeometryObject::getMovingPoints","Object "<<getName()<<" used cached points "); // DEBUG
		return;
	}

	ret = mMovPoints;
	if(norms) { 
		//errMsg("ntlGeometryObject","getMovingPoints - Normals currently unused!");
		*norms = mMovNormals; 
	}
}


/*! Calculate max. velocity on object from t1 to t2 */
ntlVec3Gfx ntlGeometryObject::calculateMaxVel(double t1, double t2) {
	ntlVec3Gfx vel(0.);
	if(mMaxMovPnt<0) return vel;
		
	vector<ntlVec3Gfx> verts1,verts2;
	verts1.push_back(mMovPoints[mMaxMovPnt]);
	verts2 = verts1;
	applyTransformation(t1,&verts1,NULL, 0,verts1.size(), true);
	applyTransformation(t2,&verts2,NULL, 0,verts2.size(), true);

	vel = (verts2[0]-verts1[0]); // /(t2-t1);
	//errMsg("ntlGeometryObject::calculateMaxVel","t1="<<t1<<" t2="<<t2<<" p1="<<verts1[0]<<" p2="<<verts2[0]<<" v="<<vel);
	return vel;
}

/*! get translation at time t*/
ntlVec3Gfx ntlGeometryObject::getTranslation(double t) {
	ntlVec3Gfx pos = mcTrans.get(t);
  // DEBUG CP_FORCECIRCLEINIT 1
	/*if( 
			(mName.compare(string("0__ts1"))==0) ||
			(mName.compare(string("1__ts1"))==0) ||
			(mName.compare(string("2__ts1"))==0) ||
			(mName.compare(string("3__ts1"))==0) ||
			(mName.compare(string("4__ts1"))==0) ||
			(mName.compare(string("5__ts1"))==0) ||
			(mName.compare(string("6__ts1"))==0) ||
			(mName.compare(string("7__ts1"))==0) ||
			(mName.compare(string("8__ts1"))==0) ||
			(mName.compare(string("9__ts1"))==0) 
			) { int j=mName[0]-'0';
			ntlVec3Gfx ppos(0.); { // DEBUG
			const float tscale=10.;
			const float tprevo = 0.33;
			const ntlVec3Gfx   toff(50,50,0);
			const ntlVec3Gfx oscale(30,30,0);
			ppos[0] =  cos(tscale* t - tprevo*(float)j + M_PI -0.1) * oscale[0] + toff[0];
			ppos[1] = -sin(tscale* t - tprevo*(float)j + M_PI -0.1) * oscale[1] + toff[1];
			ppos[2] =                               toff[2]; } // DEBUG
			pos = ppos;
			pos[2] = 0.15;
	}
  // DEBUG CP_FORCECIRCLEINIT 1 */
	return pos;
}
/*! get active flag time t*/
float ntlGeometryObject::getGeoActive(double t) {
	float act = (mcGeoActive.get(t) >= 1.) ? 1.0 : 0.0; 
	return act;
}

void ntlGeometryObject::setInitialVelocity(ntlVec3Gfx set) { 
	mInitialVelocity=set; 
	mcInitialVelocity = AnimChannel<ntlVec3Gfx>(set); 
}
ntlVec3Gfx ntlGeometryObject::getInitialVelocity(double t) { 
	ntlVec3Gfx v =  mcInitialVelocity.get(t); //return mInitialVelocity; 
	if(!mLocalCoordInivel) return v;

	ntlVec3Gfx rot = mcRot.get(t);
	ntlMat4Gfx rotMat;
	rotMat.initRotationXYZ(rot[0],rot[1],rot[2]);
	v = rotMat * v;
	return v;
}
	

