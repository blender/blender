/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A simple sphere object
 *
 *****************************************************************************/

#include "ntl_geometrysphere.h"
#include "ntl_ray.h"
#include "ntl_scene.h"


/******************************************************************************
 * Default Constructor 
 *****************************************************************************/
ntlGeometrySphere::ntlGeometrySphere() :
	ntlGeometryObject(),
	mvCenter( 0.0 ),
	mRadius( 1.0 ),
	mRefPolar(5), mRefAzim(5)
{
}


/*****************************************************************************/
/* Init attributes */
/*****************************************************************************/
void ntlGeometrySphere::initialize(ntlRenderGlobals *glob) {
	ntlGeometryObject::initialize(glob);

	mvCenter    = vec2G(mpAttrs->readVec3d("center", vec2D(mvCenter)   ,"ntlGeometrySphere", "mvCenter", false));
	mRadius     = mpAttrs->readFloat("radius", mRadius    ,"ntlGeometrySphere", "mRadius", false);
	mRefPolar   = mpAttrs->readInt  ("refpolar", mRefPolar,"ntlGeometrySphere", "mRefPolar", false);
	mRefAzim    = mpAttrs->readInt  ("refazim",  mRefAzim ,"ntlGeometrySphere", "mRefAzim", false);
	if(mRefPolar<1) mRefPolar = 1;
	if(mRefAzim<1) mRefAzim = 1;
	mRefAzim *= 4;

	mvBBStart = mvCenter - ntlVec3Gfx(mRadius);
	mvBBEnd   = mvCenter + ntlVec3Gfx(mRadius);
}


/******************************************************************************
 * 
 *****************************************************************************/

ntlVec3Gfx getSphereCoord(gfxReal radius, gfxReal phi, gfxReal theta) {
	return ntlVec3Gfx(
			radius * cos(theta) * sin(phi),
			radius * sin(theta) * sin(phi),
			radius * cos(phi)
			);
};

void 
ntlGeometrySphere::getTriangles( vector<ntlTriangle> *triangles, 
															vector<ntlVec3Gfx> *vertices, 
															vector<ntlVec3Gfx> *normals, int objectId )
{

	gfxReal phiD   = 0.5* M_PI/ (gfxReal)mRefPolar;
	gfxReal thetaD = 2.0* M_PI/ (gfxReal)mRefAzim;
	gfxReal phi   = 0.0;
	for(int i=0; i<mRefPolar; i++) {
		gfxReal theta = 0.0;
		for(int j=0; j<mRefAzim; j++) {
			ntlVec3Gfx p1,p2,p3;
			ntlVec3Gfx n1,n2,n3;

			p1 = getSphereCoord(mRadius, phi     , theta );
			p2 = getSphereCoord(mRadius, phi+phiD, theta );
			p3 = getSphereCoord(mRadius, phi+phiD, theta+thetaD );
			n1 = getNormalized(p1);
			n2 = getNormalized(p2);
			n3 = getNormalized(p3);
			//n3 = n2 = n1;
			p1 += mvCenter;
			p2 += mvCenter;
			p3 += mvCenter;
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );

			n1[2] *= -1.0;
			n2[2] *= -1.0;
			n3[2] *= -1.0;
			p1[2] -= mvCenter[2];
			p2[2] -= mvCenter[2];
			p3[2] -= mvCenter[2];
			p1[2] *= -1.0;
			p2[2] *= -1.0;
			p3[2] *= -1.0;
			p1[2] += mvCenter[2];
			p2[2] += mvCenter[2];
			p3[2] += mvCenter[2];
			sceneAddTriangle( p1,p3,p2,  n1,n3,n2,  ntlVec3Gfx(0.0), 1 );

			p1 = getSphereCoord(mRadius, phi     , theta );
			p3 = getSphereCoord(mRadius, phi     , theta+thetaD );
			p2 = getSphereCoord(mRadius, phi+phiD, theta+thetaD );
			n1 = getNormalized(p1);
			n2 = getNormalized(p2);
			n3 = getNormalized(p3);
			//n3 = n2 = n1;
			p1 += mvCenter;
			p2 += mvCenter;
			p3 += mvCenter;
			sceneAddTriangle( p1,p2,p3,  n1,n2,n3,  ntlVec3Gfx(0.0), 1 );

			n1[2] *= -1.0;
			n2[2] *= -1.0;
			n3[2] *= -1.0;
			p1[2] -= mvCenter[2];
			p2[2] -= mvCenter[2];
			p3[2] -= mvCenter[2];
			p1[2] *= -1.0;
			p2[2] *= -1.0;
			p3[2] *= -1.0;
			p1[2] += mvCenter[2];
			p2[2] += mvCenter[2];
			p3[2] += mvCenter[2];
			sceneAddTriangle( p1,p3,p2,  n1,n3,n2,  ntlVec3Gfx(0.0), 1 );
			
			theta += thetaD;
		}
		phi += phiD;
	}
	
	int doBack = 0;
	int doFront = 0;
	int doTop = 0;
	int doBottom = 0;
	int doLeft = 0;
	int doRight = 0;
	ntlVec3Gfx mvStart = mvBBStart;
	ntlVec3Gfx mvEnd   = mvBBEnd;
	
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


}


