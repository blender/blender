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
#include "ntl_renderglobals.h"

// for FGI
#include "ntl_scene.h"



/*****************************************************************************/
/* Default constructor */
/*****************************************************************************/
ntlGeometryObject::ntlGeometryObject() :
	mpMaterial( NULL ),
	mMaterialName( "default" ),
	mCastShadows( 1 ),
	mReceiveShadows( 1 ),
	mGeoInitId( -1 ), mGeoInitType( 0 ), 
	mInitialVelocity(0.0),
	mGeoInitIntersect(false)
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
			errFatal("ntlGeometryObject::initialize","Unkown 'geoinittype' value: '"<< ginitStr <<"' ", SIMWORLD_INITERROR);
			return;
		}
	}

	int geoActive = mpAttrs->readInt("geoinitactive", 1,"ntlGeometryObject", "mGeoInitId", false);
	if(!geoActive) {
		// disable geo init again...
		mGeoInitId = -1;
	}
	mInitialVelocity = vec2G( mpAttrs->readVec3d("initial_velocity", vec2D(mInitialVelocity),"ntlGeometryObject", "mInitialVelocity", false));
	debMsgStd("ntlGeometryObject::initialize",DM_MSG,"GeoObj '"<<this->getName()<<"': gid="<<mGeoInitId<<" gtype="<<mGeoInitType<<","<<ginitStr<<
			" gvel="<<mInitialVelocity<<" gisect="<<mGeoInitIntersect, 10); // debug

	// override cfg types
	mVisible = mpAttrs->readBool("visible", mVisible,"ntlGeometryObject", "mVisible", false);
	mReceiveShadows = mpAttrs->readBool("recv_shad", mReceiveShadows,"ntlGeometryObject", "mReceiveShadows", false);
	mCastShadows = mpAttrs->readBool("cast_shad", mCastShadows,"ntlGeometryObject", "mCastShadows", false);
	
	// init material
	searchMaterial( glob->getMaterials() );
}


/*****************************************************************************/
/* Search the material for this object from the material list */
/*****************************************************************************/
void ntlGeometryObject::searchMaterial(vector<ntlMaterial *> *mat)
{
	//errorOut("my: "<<mMaterialName); // DEBUG
	/* search the list... */
	int i=0;
	for (vector<ntlMaterial*>::iterator iter = mat->begin();
         iter != mat->end(); iter++) {
		//if(strcmp(mMaterialName, (*iter)->getName()) == 0) { // DEBUG
		if( mMaterialName == (*iter)->getName() ) {
			//warnMsg("ntlGeometryObject::searchMaterial","for obj '"<<getName()<<"' found - '"<<(*iter)->getName()<<"' "<<i); // DEBUG
			mpMaterial = (*iter);
			return;
		}
		i++;
	}
	errFatal("ntlGeometryObject::searchMaterial","Unknown material '"<<mMaterialName<<"' ! ", SIMWORLD_INITERROR);
	return;
}



