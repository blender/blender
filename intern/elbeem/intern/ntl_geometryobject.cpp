/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
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



/*****************************************************************************/
/* Default constructor */
/*****************************************************************************/
ntlGeometryObject::ntlGeometryObject() :
	mIsInitialized(false), mpMaterial( NULL ),
	mMaterialName( "default" ),
	mCastShadows( 1 ),
	mReceiveShadows( 1 ),
	mGeoInitId( -1 ), mGeoInitType( 0 ), 
	mInitialVelocity(0.0), mcInitialVelocity(0.0), mLocalCoordInivel(false),
	mGeoInitIntersect(false),
	mGeoPartSlipValue(0.0),
	mOnlyThinInit(false),
	mInitialPos(0.),
	mcTrans(0.), mcRot(0.), mcScale(1.),
	mIsAnimated(false),
	mMovPoints(), mMovNormals(),
	mHaveCachedMov(false),
	mCachedMovPoints(), mCachedMovNormals(),
	mMovPntsInited(-100.0), mMaxMovPnt(-1),
	mcGeoActive(1.)
{ 
};


/*****************************************************************************/
/* Default destructor */
/*****************************************************************************/
ntlGeometryObject::~ntlGeometryObject() 
{
}

/*****************************************************************************/
/* Init attributes etc. of this object */
/*****************************************************************************/
#define GEOINIT_STRINGS  9
static char *initStringStrs[GEOINIT_STRINGS] = {
	"fluid",
	"bnd_no","bnd_noslip",
	"bnd_free","bnd_freeslip",
	"bnd_part","bnd_partslip",
	"inflow", "outflow"
};
static int initStringTypes[GEOINIT_STRINGS] = {
	FGI_FLUID,
	FGI_BNDNO, FGI_BNDNO,
	FGI_BNDFREE, FGI_BNDFREE,
	FGI_BNDPART, FGI_BNDPART,
	FGI_MBNDINFLOW, FGI_MBNDOUTFLOW
};
void ntlGeometryObject::initialize(ntlRenderGlobals *glob) 
{
	//debugOut("ntlGeometryObject::initialize: '"<<getName()<<"' ", 10);
	// initialize only once...
	if(mIsInitialized) return;
	
	// init material, always necessary
	searchMaterial( glob->getMaterials() );
	
	mGeoInitId = mpAttrs->readInt("geoinitid", mGeoInitId,"ntlGeometryObject", "mGeoInitId", false);
	mGeoInitIntersect = mpAttrs->readInt("geoinit_intersect", mGeoInitIntersect,"ntlGeometryObject", "mGeoInitIntersect", false);
	string ginitStr = mpAttrs->readString("geoinittype", "", "ntlGeometryObject", "mGeoInitType", false);
	if(mGeoInitId>=0) {
		bool gotit = false;
		for(int i=0; i<GEOINIT_STRINGS; i++) {
			if(ginitStr== initStringStrs[i]) {
				gotit = true;
				mGeoInitType = initStringTypes[i];
			}
		}

		if(!gotit) {
			errFatal("ntlGeometryObject::initialize","Obj '"<<mName<<"', Unkown 'geoinittype' value: '"<< ginitStr <<"' ", SIMWORLD_INITERROR);
			return;
		}
	}

	int geoActive = mpAttrs->readInt("geoinitactive", 1,"ntlGeometryObject", "mGeoInitId", false);
	if(!geoActive) {
		// disable geo init again...
		mGeoInitId = -1;
	}
	mInitialVelocity  = vec2G( mpAttrs->readVec3d("initial_velocity", vec2D(mInitialVelocity),"ntlGeometryObject", "mInitialVelocity", false));
	if(getAttributeList()->exists("initial_velocity") || (!mcInitialVelocity.isInited()) ) {
		mcInitialVelocity = mpAttrs->readChannelVec3f("initial_velocity");
	}
	// always use channel
	if(!mcInitialVelocity.isInited()) { mcInitialVelocity = AnimChannel<ntlVec3Gfx>(mInitialVelocity); }
	mLocalCoordInivel = mpAttrs->readBool("geoinit_localinivel", mLocalCoordInivel,"ntlGeometryObject", "mLocalCoordInivel", false);

	mGeoPartSlipValue = mpAttrs->readFloat("geoinit_partslip", mGeoPartSlipValue,"ntlGeometryObject", "mGeoPartSlipValue", false);
	mOnlyThinInit     = mpAttrs->readBool("geoinit_onlythin", mOnlyThinInit,"ntlGeometryObject", "mOnlyThinInit", false);

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
	geoactive = mpAttrs->readFloat("geoactive", geoactive,"ntlGeometryObject", "geoactive", false);
	if(getAttributeList()->exists("geoactive") || (!mcGeoActive.isInited()) ) {
		mcGeoActive = mpAttrs->readChannelFloat("geoactive");
	}
	// always use channel
	if(!mcGeoActive.isInited()) { mcGeoActive = AnimChannel<double>(geoactive); }

	if(    (mcTrans.accessValues().size()>1)  // VALIDATE
	    || (mcRot.accessValues().size()>1) 
	    || (mcScale.accessValues().size()>1) 
	    || (mcGeoActive.accessValues().size()>1) 
	    || (mcInitialVelocity.accessValues().size()>1) 
		) {
		mIsAnimated = true;
	}

	mIsInitialized = true;
	debMsgStd("ntlGeometryObject::initialize",DM_MSG,"GeoObj '"<<this->getName()<<"': visible="<<this->mVisible<<" gid="<<mGeoInitId<<" gtype="<<mGeoInitType<<","<<ginitStr<<
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
		if( (getMaterial()->getMirror()>0.0) ||  
				(getMaterial()->getTransparence()>0.0) ||  
				(getMaterial()->getFresnel()>0.0) ) { 
			flag |= TRI_MAKECAUSTICS; } 
		else { 
			flag |= TRI_NOCAUSTICS; } 
		
		/* init geo init id */
		int geoiId = getGeoInitId(); 
		if((geoiId > 0) && (!mOnlyThinInit) && (!mIsAnimated)) { 
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
		valsd.clear(); time.clear(); elbeemSimplifyChannelFloat(val,&nvals); \
		for(int i=0; i<(nvals); i++) { \
			valsd.push_back( (val)[i*2+0] ); \
			time.push_back( (val)[i*2+1] ); \
		} \
		(dst) = AnimChannel< double >(valsd,time); 

void ntlGeometryObject::initChannels(
		int nTrans, float *trans, int nRot, float *rot, int nScale, float *scale,
		int nAct, float *act, int nIvel, float *ivel
		) {
	const bool debugInitc=true;
	if(debugInitc) { debMsgStd("ntlGeometryObject::initChannels",DM_MSG,"nt:"<<nTrans<<" nr:"<<nRot<<" ns:"<<nScale, 10); 
	                 debMsgStd("ntlGeometryObject::initChannels",DM_MSG,"na:"<<nAct<<" niv:"<<nIvel<<" ", 10); }
	vector<ntlVec3Gfx> vals;
	vector<double> valsd;
	vector<double> time;
	if((trans)&&(nTrans>0)) {  ADD_CHANNEL_VEC(mcTrans, nTrans, trans); }
	if((rot)&&(nRot>0)) {      ADD_CHANNEL_VEC(mcRot, nRot, rot); }
	if((scale)&&(nScale>0)) {  ADD_CHANNEL_VEC(mcScale, nScale, scale); }
	if((act)&&(nAct>0)) {      ADD_CHANNEL_FLOAT(mcGeoActive, nAct, act); }
	if((ivel)&&(nIvel>0)) {    ADD_CHANNEL_VEC(mcInitialVelocity, nIvel, ivel); }

	if(    (mcTrans.accessValues().size()>1)  // VALIDATE
	    || (mcRot.accessValues().size()>1) 
	    || (mcScale.accessValues().size()>1) 
	    || (mcGeoActive.accessValues().size()>1) 
	    || (mcInitialVelocity.accessValues().size()>1) 
		) {
		mIsAnimated = true;
	}
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
		ntlVec3Gfx pos = mcTrans.get(t);
		ntlVec3Gfx scale = mcScale.get(t);
		ntlVec3Gfx rot = mcRot.get(t);
		ntlMat4Gfx rotMat;
		rotMat.initRotationXYZ(rot[0],rot[1],rot[2]);
		pos += mInitialPos;
		//errMsg("ntlGeometryObject::applyTransformation","obj="<<getName()<<" t"<<pos<<" r"<<rot<<" s"<<scale);
		for(int i=vstart; i<vend; i++) {
			(*verts)[i] *= scale;
			(*verts)[i] = rotMat * (*verts)[i];
			(*verts)[i] += pos;
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

/*! Prepare points for moving objects */
void ntlGeometryObject::initMovingPoints(gfxReal featureSize) {
	if(mMovPntsInited==featureSize) return;
	const bool debugMoinit=false;

	vector<ntlTriangle> triangles; 
	vector<ntlVec3Gfx> vertices; 
	vector<ntlVec3Gfx> normals; 
	int objectId = 1;
	this->getTriangles(&triangles,&vertices,&normals,objectId);
	
	mMovPoints.clear(); //= vertices;
	mMovNormals.clear(); //= normals;
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

	// debug: count points to init
	if(debugMoinit) {
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
						const gfxReal uf = (gfxReal)(u+0.25) / (gfxReal)(divs1+0.0);
						const gfxReal vf = (gfxReal)(v+0.25) / (gfxReal)(divs2+0.0);
						if(uf+vf>1.0) continue;
						countp+=2;
					}
				}
			}
		}
		errMsg("ntlGeometryObject::initMovingPoints","Object "<<getName()<<" requires:"<<countp*2);
	}

	bool discardInflowBack = false;
	if( (mGeoInitType==FGI_MBNDINFLOW) && (mcInitialVelocity.accessValues().size()<1) ) discardInflowBack = true;
	discardInflowBack = false; // DEBUG disable for now


	// init std points
	for(size_t i=0; i<vertices.size(); i++) {
		ntlVec3Gfx p = vertices[ i ];
		ntlVec3Gfx n = normals[ i ];
		// discard inflow backsides
		//if( (mGeoInitType==FGI_MBNDINFLOW) && (!mIsAnimated)) {
		if(discardInflowBack) { //if( (mGeoInitType==FGI_MBNDINFLOW) && (!mIsAnimated)) {
			if(dot(mInitialVelocity,n)<0.0) continue;
		}
		mMovPoints.push_back(p);
		mMovNormals.push_back(n);
	}
	// init points & refine...
	for(size_t i=0; i<triangles.size(); i++) {
		ntlVec3Gfx p0 = vertices[ triangles[i].getPoints()[0] ];
		ntlVec3Gfx side1 = vertices[ triangles[i].getPoints()[1] ] - p0;
		ntlVec3Gfx side2 = vertices[ triangles[i].getPoints()[2] ] - p0;
		int divs1=0, divs2=0;
		if(normNoSqrt(side1) > fsTri*fsTri) { divs1 = (int)(norm(side1)/fsTri); }
		if(normNoSqrt(side2) > fsTri*fsTri) { divs2 = (int)(norm(side2)/fsTri); }
		/* if( (i!=6) &&
				(i!=6) ) { divs1=divs2=0; } // DEBUG */
		if(divs1+divs2 > 0) {
			for(int u=0; u<=divs1; u++) {
				for(int v=0; v<=divs2; v++) {
					const gfxReal uf = (gfxReal)(u+0.25) / (gfxReal)(divs1+0.0);
					const gfxReal vf = (gfxReal)(v+0.25) / (gfxReal)(divs2+0.0);
					if(uf+vf>1.0) continue;
					ntlVec3Gfx p = vertices[ triangles[i].getPoints()[0] ] * (1.0-uf-vf)+
						vertices[ triangles[i].getPoints()[1] ]*uf +
						vertices[ triangles[i].getPoints()[2] ]*vf;
					ntlVec3Gfx n = normals[ triangles[i].getPoints()[0] ] * (1.0-uf-vf)+
						normals[ triangles[i].getPoints()[1] ]*uf +
						normals[ triangles[i].getPoints()[2] ]*vf;
					normalize(n);
					//if(mGeoInitType==FGI_MBNDINFLOW) {
					// discard inflow backsides
					if(discardInflowBack) { //if( (mGeoInitType==FGI_MBNDINFLOW) && (!mIsAnimated)) {
						if(dot(mInitialVelocity,n)<0.0) continue;
					}

					mMovPoints.push_back(p);
					mMovNormals.push_back(n);
				}
			}
		}
	}

	// duplicate insides
	size_t mpsize = mMovPoints.size();
	for(size_t i=0; i<mpsize; i++) {
		//normalize(normals[i]);
		//errMsg("TTAT"," moved:"<<(mMovPoints[i] - mMovPoints[i]*featureSize)<<" org"<<mMovPoints[i]<<" norm"<<mMovPoints[i]<<" fs"<<featureSize);
		mMovPoints.push_back(mMovPoints[i] - mMovNormals[i]*0.5*featureSize);
		mMovNormals.push_back(mMovNormals[i]);
	}

	// find max point
	mMaxMovPnt = 0;
	gfxReal dist = normNoSqrt(mMovPoints[0]);
	for(size_t i=0; i<mpsize; i++) {
		if(normNoSqrt(mMovPoints[i])>dist) {
			mMaxMovPnt = i;
			dist = normNoSqrt(mMovPoints[0]);
		}
	}

	if(    (mcTrans.accessValues().size()>1)  // VALIDATE
	    || (mcRot.accessValues().size()>1) 
	    || (mcScale.accessValues().size()>1) 
		) {
		// also do trafo...
	} else {
		mCachedMovPoints = mMovPoints;
		mCachedMovNormals = mMovNormals;
		applyTransformation(0., &mCachedMovPoints, &mCachedMovNormals, 0, mCachedMovPoints.size(), true);
		mHaveCachedMov = true;
		debMsgStd("ntlGeometryObject::initMovingPoints",DM_MSG,"Object "<<getName()<<" cached points ", 7);
	}

	mMovPntsInited = featureSize;
	debMsgStd("ntlGeometryObject::initMovingPoints",DM_MSG,"Object "<<getName()<<" inited v:"<<vertices.size()<<"->"<<mMovPoints.size() , 5);
}

/*! Prepare points for moving objects */
void ntlGeometryObject::getMovingPoints(vector<ntlVec3Gfx> &ret, vector<ntlVec3Gfx> *norms) {
	if(mHaveCachedMov) {
		ret = mCachedMovPoints;
		if(norms) { *norms = mCachedMovNormals; }
		errMsg ("ntlGeometryObject::getMovingPoints","Object "<<getName()<<" used cached points ");
		return;
	}

	ret = mMovPoints;
	if(norms) { *norms = mMovNormals; }
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
	errMsg("ntlGeometryObject::calculateMaxVel","t1="<<t1<<" t2="<<t2<<" p1="<<verts1[0]<<" p2="<<verts2[0]<<" v="<<vel);
	return vel;
}

/*! get translation at time t*/
ntlVec3Gfx ntlGeometryObject::getTranslation(double t) {
	ntlVec3Gfx pos = mcTrans.get(t);
	return pos;
}
/*! get active flag time t*/
float ntlGeometryObject::getGeoActive(double t) {
	//float act = mcGeoActive.getConstant(t);
	float act = mcGeoActive.get(t); // if <= 0.0 -> off
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
	

