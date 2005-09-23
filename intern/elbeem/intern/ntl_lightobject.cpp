/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a light object
 *
 *****************************************************************************/


#include "ntl_lightobject.h"
#include "ntl_ray.h"
#include "ntl_scene.h"
#include "ntl_renderglobals.h"


/******************************************************************************
 * Default Constructor
 *****************************************************************************/
ntlLightObject::ntlLightObject(ntlRenderGlobals *glob) :
	mpGlob( glob ),
	mActive( 1 ),
	mCastShadows( 1 ),
	mcColor( ntlColor(1.0) ),
	mvPosition( ntlVec3Gfx(0.0) )
{
	// nothing to do...
}


/******************************************************************************
 * Constructor with parameters 
 *****************************************************************************/
ntlLightObject::ntlLightObject(ntlRenderGlobals *glob, const ntlColor& col) :
	mpGlob( glob ),
	mActive( 1 ),
	mCastShadows( 1 ),
	mcColor( col )
{
	// nothing to do...
}



/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlLightObject::~ntlLightObject()
{
	// nothing to do...
}



/******************************************************************************
 * Determine color contribution of a lightsource (Phong model)
 * Specular part is returned in seperate parameter and added later
 *****************************************************************************/
const ntlColor
ntlLightObject::getShadedColor(const ntlRay &reflectedRay, const ntlVec3Gfx lightDir,
															 ntlMaterial *surf, ntlColor &highlight) const
{
  gfxReal ldot = dot(lightDir, reflectedRay.getNormal()); /* equals cos( angle(L,N) ) */
  ntlColor reflected_color = ntlColor(0.0);  /* adds up to total reflected color */
	if(mpGlob->getDebugOut() > 5) errorOut("Lighting dir:"<<lightDir<<"  norm:"<<reflectedRay.getNormal()<<"  "<<ldot );

  /* lambertian reflection model */
  if (ldot > 0.0) {
		//ldot *= -1.0;
    reflected_color += surf->getDiffuseRefl() * (getColor() * ldot );

    /* specular part */
    /* specular reflection only makes sense, when the light is facing the surface,
       as the highlight is supposed to be a reflection of the lightsource, it cannot
       be reflected on surfaces with ldot<=0, as this means the arc between light 
       and normal is more than 90 degrees. If this isn't done, ugly moiree patterns appear
       in the highlights, and refractions have strange patterns due to highlights on the
       inside of the surface */
    gfxReal spec = dot(reflectedRay.getDirection(), lightDir); // equals cos( angle(R,L) )
    if((spec > 0.0) && (surf->getSpecular()>0)) {
      spec = pow( spec, surf->getSpecExponent() ); /* phong exponent */
      highlight += getColor() * surf->getSpecular() * spec;
			//errorOut( " "<< surf->getName() <<" S "<<highlight<<" "<<spec<<" "<<surf->getSpecular()<<" "<<surf->getSpecExponent() );
    }

  }

  return ntlColor(reflected_color);
}


// omni light implementation


/******************************************************************************
 *! prepare shadow maps if necessary 
 *****************************************************************************/
void ntlLightObject::prepare( bool doCaustics )
{
	doCaustics = false; // unused
	if(!mActive) { return; }
}


/******************************************************************************
 * Illuminate the given point on an object
 *****************************************************************************/
ntlColor ntlLightObject::illuminatePoint(ntlRay &reflectedRay, ntlGeometryObject *closest,
																			 ntlColor &highlight )
{
	/* is this light active? */
	if(!mActive) { return ntlColor(0.0); }

	gfxReal visibility = 1.0;   // how much of light is visible
	ntlVec3Gfx intersectionPos = reflectedRay.getOrigin();
	ntlColor current_color = ntlColor(0.0);
	ntlMaterial *clossurf = closest->getMaterial();

	ntlVec3Gfx lightDir = (mvPosition - intersectionPos);
	gfxReal lightDirNorm = normalize(lightDir);

	// where is the lightsource ?
	ntlRay rayOfLight(intersectionPos, lightDir, 0, 1.0, mpGlob );
	
	if( (mCastShadows)&&(closest->getReceiveShadows()) ) {
		ntlTriangle *tri;
		ntlVec3Gfx triNormal;
		gfxReal trit;
		mpGlob->getScene()->intersectScene(rayOfLight, trit, triNormal, tri, TRI_CASTSHADOWS);
		if(( trit>0 )&&( trit<lightDirNorm )) visibility = 0.0;
		if(mpGlob->getDebugOut() > 5) errorOut("Omni lighting with "<<visibility );
	}
	
	/* is light partly visible ? */
	if (visibility>0.0) {
		ntlColor highTemp(0.0); // temporary highlight color to multiply highTemp with offFac
		current_color = getShadedColor(reflectedRay, lightDir, clossurf, highTemp) * visibility;
		highlight += highTemp * visibility;
		if(mpGlob->getDebugOut() > 5) errorOut("Omni lighting color "<<current_color );
	}
	return current_color;
}


