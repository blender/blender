/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A simple box object
 *
 *****************************************************************************/

#include "ntl_geometrybox.h"
#include "ntl_ray.h"
#include "ntl_scene.h"


/******************************************************************************
 * Default Constructor 
 *****************************************************************************/
ntlGeometryBox::ntlGeometryBox( void ) :
	ntlGeometryObject(),
	mvStart( 0.0 ),
	mvEnd( 1.0 ),
	mRefinement(0)
{
}

/******************************************************************************
 * Init Constructor 
 *****************************************************************************/
/*ntlGeometryBox::ntlGeometryBox( ntlVec3Gfx start, ntlVec3Gfx end ) :
	ntlGeometryObject(),
	mvStart( start ),
	mvEnd( end ),
	mRefinement(0)
{
}*/

/*****************************************************************************/
/* Init refinement attribute */
/*****************************************************************************/
void ntlGeometryBox::initialize(ntlRenderGlobals *glob) {
	ntlGeometryObject::initialize(glob);
	//READATTR(ntlGeometryBox, mRefinement, refine, Int, false);
	mRefinement = mpAttrs->readInt("refine", mRefinement,"ntlGeometryBox", "mRefinement", false);

	checkBoundingBox(mvStart,mvEnd, "ntlGeometryBox::initialize");
}


/******************************************************************************
 * 
 *****************************************************************************/
void 
ntlGeometryBox::getTriangles( vector<ntlTriangle> *triangles, 
															vector<ntlVec3Gfx> *vertices, 
															vector<ntlVec3Gfx> *normals, int objectId )
{
	int doBack = 1;
	int doFront = 1;
	int doTop = 1;
	int doBottom = 1;
	int doLeft = 1;
	int doRight = 1;
	
	/*int mRefinement = 0;
	string refineAttr("refine");
	if(mpAttrs->exists(refineAttr)) {
		mRefinement = mpAttrs->find(refineAttr)->getAsInt(); 
		//debugOut("GeoBox Ref set to '"<< mpAttrs->find(refineAttr)->getCompleteString() <<"' " , 3); 
		mpAttrs->find(refineAttr)->setUsed(true);
	}*/

	if(mRefinement==0) {
	gfxReal s0 = mvStart[0];
	gfxReal s1 = mvStart[1];
	gfxReal s2 = mvStart[2];
	gfxReal e0 = mvEnd[0];
	gfxReal e1 = mvEnd[1];
	gfxReal e2 = mvEnd[2];
	ntlVec3Gfx p1,p2,p3;
	ntlVec3Gfx n1,n2,n3;

		/* front plane */
		if(doFront) {
			n1 = n2 = n3 = ntlVec3Gfx( 0.0, 0.0, -1.0 );
			p1 = ntlVec3Gfx( s0, s1, s2 );
			p3 = ntlVec3Gfx( e0, s1, s2 );
			p2 = ntlVec3Gfx( s0, e1, s2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
			p1 = ntlVec3Gfx( e0, e1, s2 );
			p3 = ntlVec3Gfx( s0, e1, s2 );
			p2 = ntlVec3Gfx( e0, s1, s2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
		}

		/* back plane k */
		if(doBack) {
			n1 = n2 = n3 = ntlVec3Gfx( 0.0, 0.0, 1.0 );
			p1 = ntlVec3Gfx( s0, s1, e2 );
			p3 = ntlVec3Gfx( s0, e1, e2 );
			p2 = ntlVec3Gfx( e0, s1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
			p1 = ntlVec3Gfx( s0, e1, e2 );
			p3 = ntlVec3Gfx( e0, e1, e2 );
			p2 = ntlVec3Gfx( e0, s1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
		}

		/* bottom plane k */
		if(doBottom) {
			n1 = n2 = n3 = ntlVec3Gfx( 0.0, -1.0, 0.0 );
			p1 = ntlVec3Gfx( e0, s1, s2 );
			p3 = ntlVec3Gfx( s0, s1, s2 );
			p2 = ntlVec3Gfx( s0, s1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
			p1 = ntlVec3Gfx( s0, s1, e2 );
			p3 = ntlVec3Gfx( e0, s1, e2 );
			p2 = ntlVec3Gfx( e0, s1, s2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
		}

		/* top plane k */
		if(doTop) {
			n1 = n2 = n3 = ntlVec3Gfx( 0.0, 1.0, 0.0 );
			p1 = ntlVec3Gfx( e0, e1, e2 );
			p2 = ntlVec3Gfx( e0, e1, s2 );
			p3 = ntlVec3Gfx( s0, e1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
			p1 = ntlVec3Gfx( s0, e1, s2 );
			p2 = ntlVec3Gfx( s0, e1, e2 );
			p3 = ntlVec3Gfx( e0, e1, s2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
		}

		/* left plane k */
		if(doLeft) {
			n1 = n2 = n3 = ntlVec3Gfx( -1.0, 0.0, 0.0 );
			p1 = ntlVec3Gfx( s0, s1, e2 );
			p3 = ntlVec3Gfx( s0, s1, s2 );
			p2 = ntlVec3Gfx( s0, e1, s2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
			p1 = ntlVec3Gfx( s0, e1, s2 );
			p3 = ntlVec3Gfx( s0, e1, e2 );
			p2 = ntlVec3Gfx( s0, s1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
		}

		/* right plane k */
		if(doRight) {
			n1 = n2 = n3 = ntlVec3Gfx( 1.0, 0.0, 0.0 );
			p1 = ntlVec3Gfx( e0, e1, e2 );
			p3 = ntlVec3Gfx( e0, e1, s2 );
			p2 = ntlVec3Gfx( e0, s1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
			p1 = ntlVec3Gfx( e0, e1, s2 );
			p3 = ntlVec3Gfx( e0, s1, s2 );
			p2 = ntlVec3Gfx( e0, s1, e2 );
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
		}

	} else {
		// refined box
		gfxReal S0 = mvStart[0];
		gfxReal S1 = mvStart[1];
		gfxReal S2 = mvStart[2];
		gfxReal v0 = (mvEnd[0]-mvStart[0])/(gfxReal)(mRefinement+1);
		gfxReal v1 = (mvEnd[1]-mvStart[1])/(gfxReal)(mRefinement+1);
		gfxReal v2 = (mvEnd[2]-mvStart[2])/(gfxReal)(mRefinement+1);
		ntlVec3Gfx p1,p2,p3;
		ntlVec3Gfx n1,n2,n3;

		for(int i=0; i<=mRefinement; i++) 
			for(int j=0; j<=mRefinement; j++) {
				gfxReal s0 = S0 + i*v0;
				gfxReal s1 = S1 + j*v1;
				gfxReal s2 = S2;
				gfxReal e0 = S0 + (i+1.0)*v0;
				gfxReal e1 = S1 + (j+1.0)*v1;
				/* front plane */
				if(doFront) {
					n1 = n2 = n3 = ntlVec3Gfx( 0.0, 0.0, -1.0 );
					p1 = ntlVec3Gfx( s0, s1, s2 );
					p3 = ntlVec3Gfx( e0, s1, s2 );
					p2 = ntlVec3Gfx( s0, e1, s2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
					p1 = ntlVec3Gfx( e0, e1, s2 );
					p3 = ntlVec3Gfx( s0, e1, s2 );
					p2 = ntlVec3Gfx( e0, s1, s2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
				}
			} // i,j
		for(int i=0; i<=mRefinement; i++) 
			for(int j=0; j<=mRefinement; j++) {
				gfxReal s0 = S0 + i*v0;
				gfxReal s1 = S1 + j*v1;
				gfxReal e0 = S0 + (i+1.0)*v0;
				gfxReal e1 = S1 + (j+1.0)*v1;
				gfxReal e2 = S2 + (mRefinement+1.0)*v2;
				/* back plane k */
				if(doBack) {
					n1 = n2 = n3 = ntlVec3Gfx( 0.0, 0.0, 1.0 );
					p1 = ntlVec3Gfx( s0, s1, e2 );
					p3 = ntlVec3Gfx( s0, e1, e2 );
					p2 = ntlVec3Gfx( e0, s1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
					p1 = ntlVec3Gfx( s0, e1, e2 );
					p3 = ntlVec3Gfx( e0, e1, e2 );
					p2 = ntlVec3Gfx( e0, s1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
				}
			}

		for(int i=0; i<=mRefinement; i++) 
			for(int j=0; j<=mRefinement; j++) {

				gfxReal s0 = S0 + i*v0;
				gfxReal s1 = S1;
				gfxReal s2 = S2 + j*v2;
				gfxReal e0 = S0 + (i+1.0)*v0;
				gfxReal e2 = S2 + (j+1.0)*v2;
				/* bottom plane k */
				if(doBottom) {
					n1 = n2 = n3 = ntlVec3Gfx( 0.0, -1.0, 0.0 );
					p1 = ntlVec3Gfx( e0, s1, s2 );
					p3 = ntlVec3Gfx( s0, s1, s2 );
					p2 = ntlVec3Gfx( s0, s1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
					p1 = ntlVec3Gfx( s0, s1, e2 );
					p3 = ntlVec3Gfx( e0, s1, e2 );
					p2 = ntlVec3Gfx( e0, s1, s2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
				}
			}

		for(int i=0; i<=mRefinement; i++) 
			for(int j=0; j<=mRefinement; j++) {

				gfxReal s0 = S0 + i*v0;
				gfxReal s2 = S2 + j*v2;
				gfxReal e0 = S0 + (i+1.0)*v0;
				gfxReal e1 = S1 + (mRefinement+1.0)*v1;
				gfxReal e2 = S2 + (j+1.0)*v2;
				/* top plane k */
				if(doTop) {
					n1 = n2 = n3 = ntlVec3Gfx( 0.0, 1.0, 0.0 );
					p1 = ntlVec3Gfx( e0, e1, e2 );
					p2 = ntlVec3Gfx( e0, e1, s2 );
					p3 = ntlVec3Gfx( s0, e1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
					p1 = ntlVec3Gfx( s0, e1, s2 );
					p2 = ntlVec3Gfx( s0, e1, e2 );
					p3 = ntlVec3Gfx( e0, e1, s2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
				}
			}
		
		for(int i=0; i<=mRefinement; i++) 
			for(int j=0; j<=mRefinement; j++) {
				gfxReal s0 = S0;
				gfxReal s1 = S1 + i*v1;
				gfxReal s2 = S2 + j*v2;
				gfxReal e1 = S1 + (i+1.0)*v1;
				gfxReal e2 = S2 + (j+1.0)*v2;
				/* left plane k */
				if(doLeft) {
					n1 = n2 = n3 = ntlVec3Gfx( -1.0, 0.0, 0.0 );
					p1 = ntlVec3Gfx( s0, s1, e2 );
					p3 = ntlVec3Gfx( s0, s1, s2 );
					p2 = ntlVec3Gfx( s0, e1, s2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
					p1 = ntlVec3Gfx( s0, e1, s2 );
					p3 = ntlVec3Gfx( s0, e1, e2 );
					p2 = ntlVec3Gfx( s0, s1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
				}
			}

		for(int i=0; i<=mRefinement; i++) 
			for(int j=0; j<=mRefinement; j++) {
				gfxReal s1 = S1 + i*v1;
				gfxReal s2 = S2 + j*v2;
				gfxReal e0 = S0 + (mRefinement+1.0)*v0;
				gfxReal e1 = S1 + (i+1.0)*v1;
				gfxReal e2 = S2 + (j+1.0)*v2;
				/* right plane k */
				if(doRight) {
					n1 = n2 = n3 = ntlVec3Gfx( 1.0, 0.0, 0.0 );
					p1 = ntlVec3Gfx( e0, e1, e2 );
					p3 = ntlVec3Gfx( e0, e1, s2 );
					p2 = ntlVec3Gfx( e0, s1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
					p1 = ntlVec3Gfx( e0, e1, s2 );
					p3 = ntlVec3Gfx( e0, s1, s2 );
					p2 = ntlVec3Gfx( e0, s1, e2 );
					sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );
				}
			}

	} // do ref

}


