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
void ntlGeometryObject::initialize(ntlRenderGlobals *glob) 
{
	//debugOut("ntlGeometryObject::initialize: '"<<getName()<<"' ", 10);
	
	mGeoInitId = mpAttrs->readInt("geoinitid", mGeoInitId,"ntlGeometryObject", "mGeoInitId", false);
	mGeoInitIntersect = mpAttrs->readInt("geoinit_intersect", mGeoInitIntersect,"ntlGeometryObject", "mGeoInitIntersect", false);
	if(mGeoInitId>=0) {
		string initStr = mpAttrs->readString("geoinittype", "", "ntlGeometryObject", "mGeoInitType", false);
		if(initStr== "fluid") {
			mGeoInitType = FGI_FLUID;
		} else 
		if((initStr== "bnd_no") || (initStr=="bnd_noslip")) {
			mGeoInitType = FGI_BNDNO;
		} else 
		if((initStr== "bnd_free") || (initStr=="bnd_freeslip")) {
			mGeoInitType = FGI_BNDFREE;
		} else 
		if((initStr== "acc") || (initStr=="accelerator")) {
			mGeoInitType = FGI_ACC;
			ntlVec3d force = mpAttrs->readVec3d("geoinitforce", ntlVec3d(0.0), "ntlGeometryObject", "mGeoInitForce", true);
			errMsg("ntlGeometryObject::initialize","Deprectated acc object used!"); exit(1);
		} else 
		if((initStr== "set") || (initStr=="speedset")) {
			mGeoInitType = FGI_SPEEDSET;
			ntlVec3d force = mpAttrs->readVec3d("geoinitforce", ntlVec3d(0.0), "ntlGeometryObject", "mGeoInitForce", true);
			errMsg("ntlGeometryObject::initialize","Deprectated speedset object used!"); exit(1);
		} else 
		// not so nice - define refinement types...
		if(initStr== "p1") {
			mGeoInitType = FGI_REFP1;
		} else 
		if(initStr== "p2") {
			mGeoInitType = FGI_REFP2;
		} else 
		if(initStr== "p3") {
			mGeoInitType = FGI_REFP3;
		// nothing found
		} else {
			errorOut("ntlGeometryObject::initialize error: Unkown 'geoinittype' value: '"<< initStr <<"' ");
			exit(1);
		}
	}

	int geoActive = mpAttrs->readInt("geoinitactive", 1,"ntlGeometryObject", "mGeoInitId", false);
	if(!geoActive) {
		// disable geo init again...
		mGeoInitId = -1;
	}
	mInitialVelocity = vec2G( mpAttrs->readVec3d("initial_velocity", vec2D(mInitialVelocity),"ntlGeometryObject", "mInitialVelocity", false));

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
	errMsg("ntlGeometryObject::searchMaterial","Unknown material '"<<mMaterialName<<"' ! ");
	exit(1);
}



