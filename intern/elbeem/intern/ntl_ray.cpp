/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * main renderer class
 *
 *****************************************************************************/


#include "ntl_ray.h"
#include "ntl_scene.h"


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
#ifndef ELBEEM_BLENDER
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
#endif // ELBEEM_BLENDER

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
#ifndef ELBEEM_BLENDER
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
	mpGlob->getScene()->intersectScene(*this, minT, normal, tri, 0);
	if(minT>0) {
		closest = mpGlob->getScene()->getObject( tri->getObjectId() );
	}

  /* object hit... */
  if (closest != NULL) {

		ntlVec3Gfx triangleNormal = tri->getNormal();
		if( equal(triangleNormal, ntlVec3Gfx(0.0)) ) errorOut("ntlRaytracer warning: trinagle normal= 0 "); // DEBUG
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
			errorOut(" t1:"<<mpGlob->getScene()->getVertex(tri->getPoints()[0])<<" t2:"<<mpGlob->getScene()->getVertex(tri->getPoints()[1])<<" t3:"<<mpGlob->getScene()->getVertex(tri->getPoints()[2]) ); 
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
					mpGlob->getScene()->intersectScene(reflectedRay2, minT2, normal2, tri2, 0);
					if(minT2>0) {
						closest2 = mpGlob->getScene()->getObject( tri2->getObjectId() );
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

      //gfxReal refrac_dn = mDirection | normal;
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
				if(!refRefl) {
					if(mpGlob->getDebugOut() > 5) errorOut("Refracted ray from depth "<<mDepth<<", dir "<<refractedDir );
					refracCol = transRay.shade();
				} else { 
					//we shouldnt reach this!
					if(mpGlob->getDebugOut() > 5) errorOut("Fully reflected ray from depth "<<mDepth<<", dir "<<reflectedDir );
					refracCol = reflectedRay.shade();
				}
      }

      /* calculate color */
      /* additive transparency "light amplification" */
      ntlColor add_col = currentColor + refracCol * currTrans;
      /* subtractive transparency, more realistic */
      ntlColor sub_col = (refracCol * currTrans) + 
        ( currentColor * (1.0-currTrans) );

      /* mix additive and subtractive */
			add_col += sub_col;
			currentColor += (refracCol * currTrans);

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

#endif // ELBEEM_BLENDER
  /* no object hit -> ray goes to infinity */
  return mpGlob->getBackgroundCol(); 
}






