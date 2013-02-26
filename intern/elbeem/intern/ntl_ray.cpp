/** \file elbeem/intern/ntl_ray.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * main renderer class
 *
 *****************************************************************************/


#include "utilities.h"
#include "ntl_ray.h"
#include "ntl_world.h"
#include "ntl_geometryobject.h"
#include "ntl_geometryshader.h"


/* Minimum value for refl/refr to be traced */
#define RAY_THRESHOLD 0.001

#if GFX_PRECISION==1
// float values
//! Minimal contribution for rays to be traced on
#define RAY_MINCONTRIB (1e-04)

#else 
// double values
//! Minimal contribution for rays to be traced on
#define RAY_MINCONTRIB (1e-05)

#endif 





/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlRay::ntlRay( void )
  : mOrigin(0.0)
  , mDirection(0.0)
  , mvNormal(0.0)
  , mDepth(0)
  , mpGlob(NULL)
  , mIsRefracted(0)
{
  errFatal("ntlRay::ntlRay()","Don't use uninitialized rays !", SIMWORLD_GENERICERROR);
	return;
}


/******************************************************************************
 * Copy - Constructor
 *****************************************************************************/
ntlRay::ntlRay( const ntlRay &r )
{
  // copy it! initialization is not enough!
  mOrigin    = r.mOrigin;
  mDirection = r.mDirection;
  mvNormal   = r.mvNormal;
  mDepth     = r.mDepth;
  mIsRefracted  = r.mIsRefracted;
  mIsReflected  = r.mIsReflected;
	mContribution = r.mContribution;
  mpGlob        = r.mpGlob;

	// get new ID
	if(mpGlob) {
		mID           = mpGlob->getCounterRays()+1;
		mpGlob->setCounterRays( mpGlob->getCounterRays()+1 );
	} else {
		mID = 0;
	}
}


/******************************************************************************
 * Constructor with explicit parameters and global render object
 *****************************************************************************/
ntlRay::ntlRay(const ntlVec3Gfx &o, const ntlVec3Gfx &d, unsigned int i, gfxReal contrib, ntlRenderGlobals *glob)
  : mOrigin( o )
  , mDirection( d )
  , mvNormal(0.0)
  , mDepth( i )
	, mContribution( contrib )
  , mpGlob( glob )
  , mIsRefracted( 0 )
	, mIsReflected( 0 )
{
	// get new ID
	if(mpGlob) {
		mID           = mpGlob->getCounterRays()+1;
		mpGlob->setCounterRays( mpGlob->getCounterRays()+1 );
	} else {
		mID = 0;
	}
}



/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlRay::~ntlRay()
{
  /* nothing to do... */
}



/******************************************************************************
 * AABB
 *****************************************************************************/
/* for AABB intersect */
#define NUMDIM 3
#define RIGHT  0
#define LEFT   1
#define MIDDLE 2

//! intersect ray with AABB
#ifndef ELBEEM_PLUGIN
void ntlRay::intersectFrontAABB(ntlVec3Gfx mStart, ntlVec3Gfx mEnd, gfxReal &t, ntlVec3Gfx &retnormal,ntlVec3Gfx &retcoord) const
{
  char   inside = true;   /* inside box? */
  char   hit    = false;  /* ray hits box? */
  int    whichPlane;      /* intersection plane */
  gfxReal candPlane[NUMDIM];  /* candidate plane */
  gfxReal quadrant[NUMDIM];   /* quadrants */
  gfxReal maxT[NUMDIM];       /* max intersection T for planes */
  ntlVec3Gfx  coord;           /* intersection point */
  ntlVec3Gfx  dir = mDirection;
  ntlVec3Gfx  origin = mOrigin;
  ntlVec3Gfx  normal(0.0, 0.0, 0.0);

  t = GFX_REAL_MAX;

  /* check intersection planes for AABB */
  for(int i=0;i<NUMDIM;i++) {
    if(origin[i] < mStart[i]) {
      quadrant[i] = LEFT;
      candPlane [i] = mStart[i];
      inside = false;
    } else if(origin[i] > mEnd[i]) {
      quadrant[i] = RIGHT;
      candPlane[i] = mEnd[i];
      inside = false;
    } else {
      quadrant[i] = MIDDLE;
    }
  }

  /* inside AABB? */
  if(!inside) {
    /* get t distances to planes */
    /* treat too small direction components as paralell */
    for(int i=0;i<NUMDIM;i++) {
      if((quadrant[i] != MIDDLE) && (fabs(dir[i]) > getVecEpsilon()) ) {
				maxT[i] = (candPlane[i] - origin[i]) / dir[i];
      } else {
				maxT[i] = -1;
      }
    }
    
    /* largest max t */
    whichPlane = 0;
    for(int i=1;i<NUMDIM;i++) {
      if(maxT[whichPlane] < maxT[i]) whichPlane = i;
    }
    
    /* check final candidate */
    hit  = true;
    if(maxT[whichPlane] >= 0.0) {

      for(int i=0;i<NUMDIM;i++) {
				if(whichPlane != i) {
					coord[i] = origin[i] + maxT[whichPlane] * dir[i];
					if( (coord[i] < mStart[i]-getVecEpsilon() ) || 
							(coord[i] > mEnd[i]  +getVecEpsilon() )  ) {
						/* no hit... */
						hit  = false;
					} 
				}
				else {
					coord[i] = candPlane[i];
				}      
      }

      /* AABB hit... */
      if( hit ) {
				t = maxT[whichPlane];	
				if(quadrant[whichPlane]==RIGHT) normal[whichPlane] = 1.0;
				else normal[whichPlane] = -1.0;
      }
    }
    

  } else {
    /* inside AABB... */
    t  = 0.0;
    coord = origin;
    return;
  }

  if(t == GFX_REAL_MAX) t = -1.0;
  retnormal = normal;
  retcoord = coord;
}

//! intersect ray with AABB
void ntlRay::intersectBackAABB(ntlVec3Gfx mStart, ntlVec3Gfx mEnd, gfxReal &t, ntlVec3Gfx &retnormal,ntlVec3Gfx &retcoord) const
{
  char   hit    = false;  /* ray hits box? */
  int    whichPlane;      /* intersection plane */
  gfxReal candPlane[NUMDIM]; /* candidate plane */
  gfxReal quadrant[NUMDIM];  /* quadrants */
  gfxReal maxT[NUMDIM];    /* max intersection T for planes */
  ntlVec3Gfx  coord;           /* intersection point */
  ntlVec3Gfx  dir = mDirection;
  ntlVec3Gfx  origin = mOrigin;
  ntlVec3Gfx  normal(0.0, 0.0, 0.0);

  t = GFX_REAL_MAX;
  for(int i=0;i<NUMDIM;i++) {
    if(origin[i] < mStart[i]) {
      quadrant[i] = LEFT;
      candPlane [i] = mEnd[i];
    } else if(origin[i] > mEnd[i]) {
      quadrant[i] = RIGHT;
      candPlane[i] = mStart[i];
    } else {
      if(dir[i] > 0) {
				quadrant[i] = LEFT;
				candPlane [i] = mEnd[i];
      } else 
				if(dir[i] < 0) {
					quadrant[i] = RIGHT;
					candPlane[i] = mStart[i];
				} else {
					quadrant[i] = MIDDLE;
				}
    }
  }


	/* get t distances to planes */
	/* treat too small direction components as paralell */
	for(int i=0;i<NUMDIM;i++) {
		if((quadrant[i] != MIDDLE) && (fabs(dir[i]) > getVecEpsilon()) ) {
			maxT[i] = (candPlane[i] - origin[i]) / dir[i];
		} else {
			maxT[i] = GFX_REAL_MAX;
		}
	}
    
	/* largest max t */
	whichPlane = 0;
	for(int i=1;i<NUMDIM;i++) {
		if(maxT[whichPlane] > maxT[i]) whichPlane = i;
	}
    
	/* check final candidate */
	hit  = true;
	if(maxT[whichPlane] != GFX_REAL_MAX) {

		for(int i=0;i<NUMDIM;i++) {
			if(whichPlane != i) {
				coord[i] = origin[i] + maxT[whichPlane] * dir[i];
				if( (coord[i] < mStart[i]-getVecEpsilon() ) || 
						(coord[i] > mEnd[i]  +getVecEpsilon() )  ) {
					/* no hit... */
					hit  = false;
				} 
			}
			else {
				coord[i] = candPlane[i];
			}      
		}

		/* AABB hit... */
		if( hit ) {
			t = maxT[whichPlane];
	
			if(quadrant[whichPlane]==RIGHT) normal[whichPlane] = 1.0;
			else normal[whichPlane] = -1.0;
		}
	}
    

  if(t == GFX_REAL_MAX) t = -1.0;
  retnormal = normal;
  retcoord = coord;
}
#endif // ELBEEM_PLUGIN

//! intersect ray with AABB
void ntlRay::intersectCompleteAABB(ntlVec3Gfx mStart, ntlVec3Gfx mEnd, gfxReal &tmin, gfxReal &tmax) const
{
  char   inside = true;   /* inside box? */
  char   hit    = false;  /* ray hits box? */
  int    whichPlane;      /* intersection plane */
  gfxReal candPlane[NUMDIM]; /* candidate plane */
  gfxReal quadrant[NUMDIM];  /* quadrants */
  gfxReal maxT[NUMDIM];    /* max intersection T for planes */
  ntlVec3Gfx  coord;           /* intersection point */
  ntlVec3Gfx  dir = mDirection;
  ntlVec3Gfx  origin = mOrigin;
  gfxReal t = GFX_REAL_MAX;

  /* check intersection planes for AABB */
  for(int i=0;i<NUMDIM;i++) {
    if(origin[i] < mStart[i]) {
      quadrant[i] = LEFT;
      candPlane [i] = mStart[i];
      inside = false;
    } else if(origin[i] > mEnd[i]) {
      quadrant[i] = RIGHT;
      candPlane[i] = mEnd[i];
      inside = false;
    } else {
      /* intersect with backside */
      if(dir[i] > 0) {
				quadrant[i] = LEFT;
				candPlane [i] = mStart[i];
      } else 
				if(dir[i] < 0) {
					quadrant[i] = RIGHT;
					candPlane[i] = mEnd[i];
				} else {
					quadrant[i] = MIDDLE;
				}
    }
  }

  /* get t distances to planes */
  for(int i=0;i<NUMDIM;i++) {
    if((quadrant[i] != MIDDLE) && (fabs(dir[i]) > getVecEpsilon()) ) {
      maxT[i] = (candPlane[i] - origin[i]) / dir[i];
    } else {
      maxT[i] = GFX_REAL_MAX;
    }
  }
 
  /* largest max t */
  whichPlane = 0;
  for(int i=1;i<NUMDIM;i++) {
    if( ((maxT[whichPlane] < maxT[i])&&(maxT[i]!=GFX_REAL_MAX)) ||
				(maxT[whichPlane]==GFX_REAL_MAX) )
      whichPlane = i;
  }
    
  /* check final candidate */
  hit  = true;
  if(maxT[whichPlane]<GFX_REAL_MAX) {
    for(int i=0;i<NUMDIM;i++) {
      if(whichPlane != i) {
				coord[i] = origin[i] + maxT[whichPlane] * dir[i];
				if( (coord[i] < mStart[i]-getVecEpsilon() ) || 
						(coord[i] > mEnd[i]  +getVecEpsilon() )  ) {
					/* no hit... */
					hit  = false;
				} 
      }
      else { coord[i] = candPlane[i];	}      
    }

    /* AABB hit... */
    if( hit ) {
      t = maxT[whichPlane];	
    }
  }
  tmin = t;

  /* now the backside */
  t = GFX_REAL_MAX;
  for(int i=0;i<NUMDIM;i++) {
    if(origin[i] < mStart[i]) {
      quadrant[i] = LEFT;
      candPlane [i] = mEnd[i];
    } else if(origin[i] > mEnd[i]) {
      quadrant[i] = RIGHT;
      candPlane[i] = mStart[i];
    } else {
      if(dir[i] > 0) {
				quadrant[i] = LEFT;
				candPlane [i] = mEnd[i];
      } else 
				if(dir[i] < 0) {
					quadrant[i] = RIGHT;
					candPlane[i] = mStart[i];
				} else {
					quadrant[i] = MIDDLE;
				}
    }
  }


  /* get t distances to planes */
  for(int i=0;i<NUMDIM;i++) {
    if((quadrant[i] != MIDDLE) && (fabs(dir[i]) > getVecEpsilon()) ) {
      maxT[i] = (candPlane[i] - origin[i]) / dir[i];
    } else {
      maxT[i] = GFX_REAL_MAX;
    }
  }
    
  /* smallest max t */
  whichPlane = 0;
  for(int i=1;i<NUMDIM;i++) {
    if(maxT[whichPlane] > maxT[i]) whichPlane = i;
  }
    
  /* check final candidate */
  hit  = true;
  if(maxT[whichPlane] != GFX_REAL_MAX) {

    for(int i=0;i<NUMDIM;i++) {
      if(whichPlane != i) {
				coord[i] = origin[i] + maxT[whichPlane] * dir[i];
				if( (coord[i] < mStart[i]-getVecEpsilon() ) || 
						(coord[i] > mEnd[i]  +getVecEpsilon() )  ) {
					/* no hit... */
					hit  = false;
				} 
      }
      else {
				coord[i] = candPlane[i];
      }      
    }

    /* AABB hit... */
    if( hit ) {
      t = maxT[whichPlane];
    }
  }
   
  tmax = t;
}



/******************************************************************************
 * Determine color of this ray by tracing through the scene
 *****************************************************************************/
const ntlColor ntlRay::shade() //const
{
#ifndef ELBEEM_PLUGIN
  ntlGeometryObject           *closest = NULL;
  gfxReal                      minT = GFX_REAL_MAX;
  vector<ntlLightObject*>     *lightlist = mpGlob->getLightList();
  mpGlob->setCounterShades( mpGlob->getCounterShades()+1 );
	bool intersectionInside = 0;
	if(mpGlob->getDebugOut() > 5) errorOut(std::endl<<"New Ray: depth "<<mDepth<<", org "<<mOrigin<<", dir "<<mDirection );

	/* check if this ray contributes enough */
	if(mContribution <= RAY_MINCONTRIB) {
		//return ntlColor(0.0);
	}
	
  /* find closes object that intersects */
	ntlTriangle *tri = NULL;
	ntlVec3Gfx normal;
	mpGlob->getRenderScene()->intersectScene(*this, minT, normal, tri, 0);
	if(minT>0) {
		closest = mpGlob->getRenderScene()->getObject( tri->getObjectId() );
	}

  /* object hit... */
  if (closest != NULL) {

		ntlVec3Gfx triangleNormal = tri->getNormal();
		if( equal(triangleNormal, ntlVec3Gfx(0.0)) ) errorOut("ntlRay warning: trinagle normal= 0 "); // DEBUG
		/* intersection on inside faces? if yes invert normal afterwards */
		gfxReal valDN; // = mDirection | normal;
		valDN = dot(mDirection, triangleNormal);
		if( valDN > 0.0) {
			intersectionInside = 1;
			normal = normal * -1.0;
			triangleNormal = triangleNormal * -1.0;
		} 

    /* ... -> do reflection */
    ntlVec3Gfx intersectionPosition(mOrigin + (mDirection * (minT)) );
    ntlMaterial *clossurf = closest->getMaterial();
		/*if(mpGlob->getDebugOut() > 5) {
			errorOut("Ray hit: at "<<intersectionPosition<<" n:"<<normal<<"    dn:"<<valDN<<" ins:"<<intersectionInside<<"  cl:"<<((unsigned int)closest) ); 
			errorOut(" t1:"<<mpGlob->getRenderScene()->getVertex(tri->getPoints()[0])<<" t2:"<<mpGlob->getRenderScene()->getVertex(tri->getPoints()[1])<<" t3:"<<mpGlob->getScene()->getVertex(tri->getPoints()[2]) ); 
			errorOut(" trin:"<<tri->getNormal() );
		} // debug */

		/* current transparence and reflectivity */
		gfxReal currTrans = clossurf->getTransparence();
		gfxReal currRefl = clossurf->getMirror();

    /* correct intersectopm position */
    intersectionPosition += ( triangleNormal*getVecEpsilon() );
		/* reflection at normal */
		ntlVec3Gfx reflectedDir = getNormalized( reflectVector(mDirection, normal) );
		int badRefl = 0;
		if(dot(reflectedDir, triangleNormal)<0.0 ) {
			badRefl = 1;
			if(mpGlob->getDebugOut() > 5) { errorOut("Ray Bad reflection...!"); }
		}

		/* refraction direction, depending on in/outside hit */
		ntlVec3Gfx refractedDir;
		int refRefl = 0;
		/* refraction at normal is handled by inverting normal before */
		gfxReal myRefIndex = 1.0;
		if((currTrans>RAY_THRESHOLD)||(clossurf->getFresnel())) {
			if(intersectionInside) {
				myRefIndex = 1.0/clossurf->getRefracIndex();
			} else {
				myRefIndex = clossurf->getRefracIndex();
			}

			refractedDir = refractVector(mDirection, normal, myRefIndex , (gfxReal)(1.0) /* global ref index */, refRefl);
		}

		/* calculate fresnel? */
		if(clossurf->getFresnel()) {
			// for total reflection, just set trans to 0
			if(refRefl) {
				currRefl = 1.0; currTrans = 0.0;
			} else {
				// calculate fresnel coefficients
				clossurf->calculateFresnel( mDirection, normal, myRefIndex, currRefl,currTrans );
			}
		}

    ntlRay reflectedRay(intersectionPosition, reflectedDir, mDepth+1, mContribution*currRefl, mpGlob);
		reflectedRay.setNormal( normal );
    ntlColor currentColor(0.0);
    ntlColor highlightColor(0.0);

    /* first add reflected ambient color */
    currentColor += (clossurf->getAmbientRefl() * mpGlob->getAmbientLight() );

    /* calculate lighting, not on the insides of objects... */
		if(!intersectionInside) {
			for (vector<ntlLightObject*>::iterator iter = lightlist->begin();
					 iter != lightlist->end();
					 iter++) {
				
				/* let light illuminate point */
				currentColor += (*iter)->illuminatePoint( reflectedRay, closest, highlightColor );
				
			} // for all lights
		}

    // recurse ?
    if ((mDepth < mpGlob->getRayMaxDepth() )&&(currRefl>RAY_THRESHOLD)) {

				if(badRefl) {
					ntlVec3Gfx intersectionPosition2;
					ntlGeometryObject           *closest2 = NULL;
					gfxReal                      minT2 = GFX_REAL_MAX;
					ntlTriangle *tri2 = NULL;
					ntlVec3Gfx normal2;

					ntlVec3Gfx refractionPosition2(mOrigin + (mDirection * minT) );
					refractionPosition2 -= (triangleNormal*getVecEpsilon() );

					ntlRay reflectedRay2 = ntlRay(refractionPosition2, reflectedDir, mDepth+1, mContribution*currRefl, mpGlob);
					mpGlob->getRenderScene()->intersectScene(reflectedRay2, minT2, normal2, tri2, 0);
					if(minT2>0) {
						closest2 = mpGlob->getRenderScene()->getObject( tri2->getObjectId() );
					}

					/* object hit... */
					if (closest2 != NULL) {
						ntlVec3Gfx triangleNormal2 = tri2->getNormal();
						gfxReal valDN2; 
						valDN2 = dot(reflectedDir, triangleNormal2);
						if( valDN2 > 0.0) {
							triangleNormal2 = triangleNormal2 * -1.0;
							intersectionPosition2 = ntlVec3Gfx(intersectionPosition + (reflectedDir * (minT2)) );
							/* correct intersection position and create new reflected ray */
							intersectionPosition2 += ( triangleNormal2*getVecEpsilon() );
							reflectedRay = ntlRay(intersectionPosition2, reflectedDir, mDepth+1, mContribution*currRefl, mpGlob);
						} else { 
							// ray seems to work, continue normally ?
						}

					}

				}

      // add mirror color multiplied by mirror factor of surface
			if(mpGlob->getDebugOut() > 5) errorOut("Reflected ray from depth "<<mDepth<<", dir "<<reflectedDir );
			ntlColor reflectedColor = reflectedRay.shade() * currRefl;
			currentColor += reflectedColor;
    }

    /* Trace another ray on for transparent objects */
    if(currTrans > RAY_THRESHOLD) {
      /* position at the other side of the surface, along ray */
      ntlVec3Gfx refraction_position(mOrigin + (mDirection * minT) );
      refraction_position += (mDirection * getVecEpsilon());
			refraction_position -= (triangleNormal*getVecEpsilon() );
      ntlColor refracCol(0.0);         /* refracted color */

      /* trace refracted ray */
      ntlRay transRay(refraction_position, refractedDir, mDepth+1, mContribution*currTrans, mpGlob);
      transRay.setRefracted(1);
			transRay.setNormal( normal );
      if(mDepth < mpGlob->getRayMaxDepth() ) {
				// full reflection should make sure refracindex&fresnel are on...
				if((0)||(!refRefl)) {
					if(mpGlob->getDebugOut() > 5) errorOut("Refracted ray from depth "<<mDepth<<", dir "<<refractedDir );
					refracCol = transRay.shade();
				} else { 
					//we shouldnt reach this!
					if(mpGlob->getDebugOut() > 5) errorOut("Fully reflected ray from depth "<<mDepth<<", dir "<<reflectedDir );
					refracCol = reflectedRay.shade();
				}
      }
			//errMsg("REFMIR","t"<<currTrans<<" thres"<<RAY_THRESHOLD<<" mirr"<<currRefl<<" refRefl"<<refRefl<<" md"<<mDepth);

      /* calculate color */
			// use transadditive setting!?
      /* additive transparency "light amplification" */
      //? ntlColor add_col = currentColor + refracCol * currTrans;
      /* mix additive and subtractive */
			//? add_col += sub_col;
			//? currentColor += (refracCol * currTrans);

      /* subtractive transparency, more realistic */
      ntlColor sub_col = (refracCol * currTrans) + ( currentColor * (1.0-currTrans) );
			currentColor = sub_col;

    }

		/* add highlights (should not be affected by transparence as the diffuse reflections */
		currentColor += highlightColor;

		/* attentuate as a last step*/
		/* check if we're on the inside or outside */
		if(intersectionInside) {
			gfxReal kr,kg,kb;    /* attentuation */
			/* calculate attentuation */
			ntlColor attCol = clossurf->getTransAttCol();
			kr = exp( attCol[0] * minT );
			kg = exp( attCol[1] * minT );
			kb = exp( attCol[2] * minT );
			currentColor = currentColor * ntlColor(kr,kg,kb);
		}

    /* done... */
		if(mpGlob->getDebugOut() > 5) { errorOut("Ray "<<mDepth<<" color "<<currentColor ); }
    return ntlColor(currentColor);
  }

#endif // ELBEEM_PLUGIN
  /* no object hit -> ray goes to infinity */
  return mpGlob->getBackgroundCol(); 
}



/******************************************************************************
 ******************************************************************************
 ******************************************************************************
 * scene implementation
 ******************************************************************************
 ******************************************************************************
 *****************************************************************************/



/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlScene::ntlScene( ntlRenderGlobals *glob, bool del ) :
	mpGlob( glob ), mSceneDel(del),
	mpTree( NULL ),
	mSceneBuilt( false ), mFirstInitDone( false )
{
}


/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlScene::~ntlScene()
{
	if(mpTree != NULL) delete mpTree;

	// cleanup lists, only if this is the rendering cleanup scene
	if(mSceneDel) 
	{
		for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin();
				iter != mGeos.end(); iter++) {
			//errMsg("ntlScene::~ntlScene","Deleting obj "<<(*iter)->getName() );
			delete (*iter);
		}
		for (vector<ntlLightObject*>::iterator iter = mpGlob->getLightList()->begin();
				 iter != mpGlob->getLightList()->end(); iter++) {
			delete (*iter);
		}
		for (vector<ntlMaterial*>::iterator iter = mpGlob->getMaterials()->begin();
				 iter != mpGlob->getMaterials()->end(); iter++) {
			delete (*iter);
		}
	}
	errMsg("ntlScene::~ntlScene","Deleted, ObjFree:"<<mSceneDel);
}


/******************************************************************************
 * Build the scene arrays (obj, tris etc.)
 *****************************************************************************/
void ntlScene::buildScene(double time,bool firstInit)
{
	const bool buildInfo=true;

	if(firstInit) {
		mObjects.clear();
		/* init geometry array, first all standard objects */
		for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin();
				iter != mGeos.end(); iter++) {
			bool geoinit = false;
			int tid = (*iter)->getTypeId();
			if(tid & GEOCLASSTID_OBJECT) {
				ntlGeometryObject *geoobj = (ntlGeometryObject*)(*iter);
				geoinit = true;
				mObjects.push_back( geoobj );
				if(buildInfo) debMsgStd("ntlScene::BuildScene",DM_MSG,"added GeoObj "<<geoobj->getName()<<" Id:"<<geoobj->getObjectId(), 5 );
			}
			//if(geoshad) {
			if(tid & GEOCLASSTID_SHADER) {
				ntlGeometryShader *geoshad = (ntlGeometryShader*)(*iter);
				geoinit = true;
				if(!mFirstInitDone) {
					// only on first init
					geoshad->initializeShader();
				}
				for (vector<ntlGeometryObject*>::iterator siter = geoshad->getObjectsBegin();
						siter != geoshad->getObjectsEnd();
						siter++) {
					if(buildInfo) debMsgStd("ntlScene::BuildScene",DM_MSG,"added shader geometry "<<(*siter)->getName()<<" Id:"<<(*siter)->getObjectId(), 5 );
					mObjects.push_back( (*siter) );
				}
			}

			if(!geoinit) {
				errFatal("ntlScene::BuildScene","Invalid geometry class!", SIMWORLD_INITERROR);
				return;
			}
		}
	}

	// collect triangles
	mTriangles.clear();
	mVertices.clear();
	mVertNormals.clear();

	/* for test mode deactivate transparencies etc. */
	if( mpGlob->getTestMode() ) {
		debugOut("ntlScene::buildScene : Test Mode activated!", 2);
		// assign random colors to dark materials
		int matCounter = 0;
		ntlColor stdCols[] = { ntlColor(0,0,1.0), ntlColor(0,1.0,0), ntlColor(1.0,0.7,0) , ntlColor(0.7,0,0.6) };
		int stdColNum = 4;
		for (vector<ntlMaterial*>::iterator iter = mpGlob->getMaterials()->begin();
					 iter != mpGlob->getMaterials()->end(); iter++) {
			(*iter)->setTransparence(0.0);
			(*iter)->setMirror(0.0);
			(*iter)->setFresnel(false);
			// too dark?
			if( norm((*iter)->getDiffuseRefl()) <0.01) {
				(*iter)->setDiffuseRefl( stdCols[matCounter] );
				matCounter ++;
				matCounter = matCounter%stdColNum;
			}
		}

		// restrict output file size to 400
		float downscale = 1.0;
		if(mpGlob->getResX() > 400){ downscale = 400.0/(float)mpGlob->getResX(); }
		if(mpGlob->getResY() > 400){ 
			float downscale2 = 400.0/(float)mpGlob->getResY(); 
			if(downscale2<downscale) downscale=downscale2;
		}
		mpGlob->setResX( (int)(mpGlob->getResX() * downscale) );
		mpGlob->setResY( (int)(mpGlob->getResY() * downscale) );

	}

	/* collect triangles from objects */
	int idCnt = 0;          // give IDs to objects
	bool debugTriCollect = false;
	if(debugTriCollect) debMsgStd("ntlScene::buildScene",DM_MSG,"Start...",5);
  for (vector<ntlGeometryObject*>::iterator iter = mObjects.begin();
       iter != mObjects.end();
       iter++) {
		/* only add visible objects */
		if(firstInit) {
			if(debugTriCollect) debMsgStd("ntlScene::buildScene",DM_MSG,"Collect init of "<<(*iter)->getName()<<" idCnt:"<<idCnt, 4 );
			(*iter)->initialize( mpGlob ); }
		if(debugTriCollect) debMsgStd("ntlScene::buildScene",DM_MSG,"Collecting tris from "<<(*iter)->getName(), 4 );

		int vstart = mVertNormals.size();
		(*iter)->setObjectId(idCnt);
		(*iter)->getTriangles(time, &mTriangles, &mVertices, &mVertNormals, idCnt);
		(*iter)->applyTransformation(time, &mVertices, &mVertNormals, vstart, mVertices.size(), false );

		if(debugTriCollect) debMsgStd("ntlScene::buildScene",DM_MSG,"Done with "<<(*iter)->getName()<<" totTris:"<<mTriangles.size()<<" totVerts:"<<mVertices.size()<<" totNorms:"<<mVertNormals.size(), 4 );
		idCnt ++;
	}
	if(debugTriCollect) debMsgStd("ntlScene::buildScene",DM_MSG,"End",5);


	/* calculate triangle normals, and initialize flags */
  for (vector<ntlTriangle>::iterator iter = mTriangles.begin();
       iter != mTriangles.end();
       iter++) {

		// calculate normal from triangle points
		ntlVec3Gfx normal = 
			cross( (ntlVec3Gfx)( (mVertices[(*iter).getPoints()[2]] - mVertices[(*iter).getPoints()[0]]) *-1.0),  // BLITZ minus sign right??
			(ntlVec3Gfx)(mVertices[(*iter).getPoints()[1]] - mVertices[(*iter).getPoints()[0]]) );
		normalize(normal);
		(*iter).setNormal( normal );
	}



	// scene geometry built 
	mSceneBuilt = true;

	// init shaders that require complete geometry
	if(!mFirstInitDone) {
		// only on first init
		for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin();
				iter != mGeos.end(); iter++) {
			if( (*iter)->getTypeId() & GEOCLASSTID_SHADER ) {
				ntlGeometryShader *geoshad = (ntlGeometryShader*)(*iter);
				if(geoshad->postGeoConstrInit( mpGlob )) {
					errFatal("ntlScene::buildScene","Init failed for object '"<< (*iter)->getName() <<"' !", SIMWORLD_INITERROR );
					return;
				}
			}
		}
		mFirstInitDone = true;
	}

	// check unused attributes (for classes and objects!)
  for (vector<ntlGeometryObject*>::iterator iter = mObjects.begin(); iter != mObjects.end(); iter++) {
		if((*iter)->getAttributeList()->checkUnusedParams()) {
			(*iter)->getAttributeList()->print(); // DEBUG
			errFatal("ntlScene::buildScene","Unused params for object '"<< (*iter)->getName() <<"' !", SIMWORLD_INITERROR );
			return;
		}
	}
	for (vector<ntlGeometryClass*>::iterator iter = mGeos.begin(); iter != mGeos.end(); iter++) { 
		if((*iter)->getAttributeList()->checkUnusedParams()) {
			(*iter)->getAttributeList()->print(); // DEBUG
			errFatal("ntlScene::buildScene","Unused params for object '"<< (*iter)->getName() <<"' !", SIMWORLD_INITERROR );
			return;
		}
	}

}

/******************************************************************************
 * Prepare the scene triangles and maps for raytracing
 *****************************************************************************/
void ntlScene::prepareScene(double time)
{
	/* init triangles... */
	buildScene(time, false);
	// what for currently not used ???
	if(mpTree != NULL) delete mpTree;
	mpTree = new ntlTree( 
#			if FSGR_STRICT_DEBUG!=1
			mpGlob->getTreeMaxDepth(), mpGlob->getTreeMaxTriangles(), 
#			else
			mpGlob->getTreeMaxDepth()/3*2, mpGlob->getTreeMaxTriangles()*2, 
#			endif
			this, TRI_GEOMETRY );

	//debMsgStd("ntlScene::prepareScene",DM_MSG,"Stats - tris:"<< (int)mTriangles.size()<<" verts:"<<mVertices.size()<<" vnorms:"<<mVertNormals.size(), 5 );
}
/******************************************************************************
 * Do some memory cleaning, when frame is finished
 *****************************************************************************/
void ntlScene::cleanupScene( void )
{
	mTriangles.clear();
	mVertices.clear();
	mVertNormals.clear();

	if(mpTree != NULL) delete mpTree;
	mpTree = NULL;
}


/******************************************************************************
 * Intersect a ray with the scene triangles
 *****************************************************************************/
void ntlScene::intersectScene(const ntlRay &r, gfxReal &distance, ntlVec3Gfx &normal, ntlTriangle *&tri,int flags) const
{
	distance = -1.0;
  mpGlob->setCounterSceneInter( mpGlob->getCounterSceneInter()+1 );
	mpTree->intersect(r, distance, normal, tri, flags, false);
}





