/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Particle Viewer/Tracer
 *
 *****************************************************************************/

#include <stdio.h>
//#include "../libs/my_gl.h"
//#include "../libs/my_glu.h"

/* own lib's */
#include "particletracer.h"
#include "ntl_matrices.h"
#include "ntl_ray.h"
#include "ntl_scene.h"



/******************************************************************************
 * Standard constructor
 *****************************************************************************/
ParticleTracer::ParticleTracer() :
	ntlGeometryObject(),
	mParts(1),
	mNumParticles(0), mTrailLength(1), mTrailInterval(1),mTrailIntervalCounter(0),
	mPartSize(0.01), mTrailScale(1.0),
	mStart(-1.0), mEnd(1.0),
	mSimStart(-1.0), mSimEnd(1.0),
	mPartScale(1.0) , mPartHeadDist( 0.5 ), mPartTailDist( -4.5 ), mPartSegments( 4 ),
	mValueScale(0),
	mValueCutoffTop(0.0), mValueCutoffBottom(0.0)
{
};

/*****************************************************************************/
//! parse settings from attributes (dont use own list!)
/*****************************************************************************/
void ParticleTracer::parseAttrList(AttributeList *att) 
{
	AttributeList *tempAtt = mpAttrs;
	mpAttrs = att;
	mNumParticles = mpAttrs->readInt("particles",mNumParticles, "ParticleTracer","mNumParticles", false);
	mTrailLength  = mpAttrs->readInt("traillength",mTrailLength, "ParticleTracer","mTrailLength", false);
	mTrailInterval= mpAttrs->readInt("trailinterval",mTrailInterval, "ParticleTracer","mTrailInterval", false);

	mPartScale    = mpAttrs->readFloat("part_scale",mPartScale, "ParticleTracer","mPartScale", false);
	mPartHeadDist = mpAttrs->readFloat("part_headdist",mPartHeadDist, "ParticleTracer","mPartHeadDist", false);
	mPartTailDist = mpAttrs->readFloat("part_taildist",mPartTailDist, "ParticleTracer","mPartTailDist", false);
	mPartSegments = mpAttrs->readInt  ("part_segments",mPartSegments, "ParticleTracer","mPartSegments", false);
	mValueScale   = mpAttrs->readInt  ("part_valscale",mValueScale, "ParticleTracer","mValueScale", false);
	mValueCutoffTop = mpAttrs->readFloat("part_valcutofftop",mValueCutoffTop, "ParticleTracer","mValueCutoffTop", false);
	mValueCutoffBottom = mpAttrs->readFloat("part_valcutoffbottom",mValueCutoffBottom, "ParticleTracer","mValueCutoffBottom", false);

	mTrailScale   = mpAttrs->readFloat("trail_scale",mTrailScale, "ParticleTracer","mTrailScale", false);

	string matPart;
	matPart = mpAttrs->readString("material_part", "default", "ParticleTracer","material", false);
	setMaterialName( matPart );
	// trail length has to be at least one, if anything should be displayed
	if((mNumParticles>0)&&(mTrailLength<2)) mTrailLength = 2;

	// restore old list
	mpAttrs = tempAtt;
	mParts.resize(mTrailLength*mTrailInterval);
}

/******************************************************************************
 * draw the particle array
 *****************************************************************************/
void ParticleTracer::draw()
{
}


/******************************************************************************
 * set the number of timesteps to trace
 *****************************************************************************/
void ParticleTracer::setTimesteps(int steps)
{
	steps=0; // remove warning...
} 


/******************************************************************************
 * add a particle at this position
 *****************************************************************************/
void ParticleTracer::addParticle(double x, double y, double z)
{
	ntlVec3Gfx p(x,y,z);
	ParticleObject part( p );
	//mParts[0].push_back( part );
	// TODO handle other arrays?
	//part.setActive( false );
	for(size_t l=0; l<mParts.size(); l++) {
		// add deactivated particles to other arrays
		mParts[l].push_back( part );
		// deactivate further particles
		if(l>1) {
			//mParts[l][ mParts.size()-1 ].setActive( false );
		}
	}
}



/******************************************************************************
 * save particle positions before adding a new timestep
 * copy "one index up", newest has to remain unmodified, it will be
 * advanced after the next smiulation step
 *****************************************************************************/
void ParticleTracer::savePreviousPositions()
{
	//debugOut(" PARTS SIZE "<<mParts.size() ,10);
	if(mTrailIntervalCounter==0) {
	//errMsg("spp"," PARTS SIZE "<<mParts.size() );
		for(size_t l=mParts.size()-1; l>0; l--) {
			if( mParts[l].size() != mParts[l-1].size() ) {
				errFatal("ParticleTracer::savePreviousPositions","Invalid array sizes ["<<l<<"]="<<mParts[l].size()<<
						" ["<<(l+1)<<"]="<<mParts[l+1].size() <<" , total "<< mParts.size() , SIMWORLD_GENERICERROR);
				return;
			}

			for(size_t i=0; i<mParts[l].size(); i++) {
				mParts[l][i] = mParts[l-1][i];
			}

		}
	} 
	mTrailIntervalCounter++;
	if(mTrailIntervalCounter>=mTrailInterval) mTrailIntervalCounter = 0;
}




/******************************************************************************
 * Get triangles for rendering
 *****************************************************************************/
void ParticleTracer::getTriangles( vector<ntlTriangle> *triangles, 
													 vector<ntlVec3Gfx> *vertices, 
													 vector<ntlVec3Gfx> *normals, int objectId )
{
	int tris = 0;
	gfxReal partNormSize = 0.01 * mPartScale;
	ntlVec3Gfx pScale = ntlVec3Gfx(
			(mEnd[0]-mStart[0])/(mSimEnd[0]-mSimStart[0]),
			(mEnd[1]-mStart[1])/(mSimEnd[1]-mSimStart[1]),
			(mEnd[2]-mStart[2])/(mSimEnd[2]-mSimStart[2])
			);
	//errMsg(" PS ", " S "<<pScale );
	ntlVec3Gfx org = mStart;
	int segments = mPartSegments;

	int lnewst = mTrailLength-1;
	int loldst = mTrailLength-2;
	// trails gehen nicht so richtig mit der
	// richtung der partikel...
	//for(int l=0; l<mTrailLength-2; l++) {
	//int lnewst = l+1;
	//int loldst = l;

	for(size_t i=0; i<mParts[lnewst].size(); i++) {

		//mParts[0][i].setActive(true);

		if( mParts[lnewst][i].getActive()==false ) continue;
		if( mParts[loldst][i].getActive()==false ) continue;

		ntlVec3Gfx pnew = mParts[lnewst][i].getPos();
		ntlVec3Gfx pold = mParts[loldst][i].getPos();
		ntlVec3Gfx pdir = pnew - pold;
		gfxReal plen = normalize( pdir );
		if( plen < 1e-05) pdir = ntlVec3Gfx(-1.0 ,0.0 ,0.0);
		ntlVec3Gfx p = org + pnew*pScale;
		gfxReal partsize = 0.0;
		//errMsg("pp"," "<<l<<" i"<<i<<" new"<<pnew<<" old"<<pold );
		
		// value length scaling?
		if(mValueScale==1) {
			partsize = mPartScale * plen;
		} else if(mValueScale==2) {
			// cut off scaling
			if(plen > mValueCutoffTop) continue;
			if(plen < mValueCutoffBottom) continue;
			partsize = mPartScale * plen;
		} else {
			partsize = mPartScale; // no length scaling
		}

		ntlVec3Gfx pstart( mPartHeadDist *partsize, 0.0, 0.0 );
		ntlVec3Gfx pend  ( mPartTailDist *partsize, 0.0, 0.0 );
		gfxReal phi = 0.0;
		gfxReal phiD = 2.0*M_PI / (gfxReal)segments;

		ntlMat4Gfx cvmat; 
		cvmat.initId();
		pdir *= -1.0;
		ntlVec3Gfx cv1 = pdir;
		ntlVec3Gfx cv2 = ntlVec3Gfx(pdir[1], -pdir[0], 0.0);
		ntlVec3Gfx cv3 = cross( cv1, cv2);
		for(int l=0; l<3; l++) {
			cvmat.value[l][0] = cv1[l];
			cvmat.value[l][1] = cv2[l];
			cvmat.value[l][2] = cv3[l];
		}
		pstart = (cvmat * pstart);
		pend = (cvmat * pend);

		for(int s=0; s<segments; s++) {
			ntlVec3Gfx p1( 0.0 );
			ntlVec3Gfx p2( 0.0 );

			gfxReal radscale = partNormSize;
			radscale = (partsize+partNormSize)*0.5;
			p1[1] += cos(phi) * radscale;
			p1[2] += sin(phi) * radscale;
			p2[1] += cos(phi + phiD) * radscale;
			p2[2] += sin(phi + phiD) * radscale;
			ntlVec3Gfx n1 = ntlVec3Gfx( 0.0, cos(phi), sin(phi) );
			ntlVec3Gfx n2 = ntlVec3Gfx( 0.0, cos(phi + phiD), sin(phi + phiD) );
			ntlVec3Gfx ns = n1*0.5 + n2*0.5;

			p1 = (cvmat * p1);
			p2 = (cvmat * p2);

			sceneAddTriangle( p+pstart, p+p1, p+p2, 
					ns,n1,n2, ntlVec3Gfx(0.0), 1 ); 
			sceneAddTriangle( p+pend  , p+p2, p+p1, 
					ns,n2,n1, ntlVec3Gfx(0.0), 1 ); 

			phi += phiD;
			tris += 2;
		}
	}

	//} // trail
	return; // DEBUG


	// add trails
	//double tScale = 0.01 * mPartScale * mTrailScale;
	double trails = 0.01 * mPartScale * mTrailScale;
	//for(int l=0; l<mParts.size()-1; l++) {
	for(int l=0; l<mTrailLength-2; l++) {
		for(size_t i=0; i<mParts[0].size(); i++) {
			int tl1 = l*mTrailInterval;
			int tl2 = (l+1)*mTrailInterval;
			if( mParts[tl1][i].getActive()==false ) continue;
			if( mParts[tl2][i].getActive()==false ) continue;
			ntlVec3Gfx p1 = org+mParts[tl1][i].getPos()*pScale;
			ntlVec3Gfx p2 = org+mParts[tl2][i].getPos()*pScale;
			ntlVec3Gfx n = ntlVec3Gfx(0,0,-1);
			sceneAddTriangle( p1+ntlVec3Gfx(0,trails,0), p1+ntlVec3Gfx(0,-trails,0), p2, 
					n,n,n, ntlVec3Gfx(0.0), 1 ); 
			sceneAddTriangle( p2, p1+ntlVec3Gfx(0,-trails,0), p1+ntlVec3Gfx(0,trails,0), 
					n,n,n, ntlVec3Gfx(0.0), 1 ); 
			tris += 2;
		}
	}
	debugOut("ParticleTracer::getTriangles "<<mName<<" : Triangulated "<< (mParts[0].size()) <<" particles (triangles: "<<tris<<") ", 10);
	//debugOut(" s"<<mStart<<" e"<<mEnd<<" ss"<<mSimStart<<" se"<<mSimEnd , 10);

}




