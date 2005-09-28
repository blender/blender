/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a light object
 * default omni light implementation
 *
 *****************************************************************************/
#ifndef NTL_LIGHTOBJECT_HH
#define NTL_LIGHTOBJECT_HH

#include "ntl_vector3dim.h"
#include "ntl_material.h"
class ntlRay;
class ntlRenderGlobals;
class ntlGeometryObject;



/* shadow map directions */
#define LSM_RIGHT 0
#define LSM_LEFT  1
#define LSM_UP    2
#define LSM_DOWN  3
#define LSM_FRONT 4
#define LSM_BACK  5

/*! Basic object for lights, all other light are derived from this one */
class ntlLightObject
{
public:
  /* CONSTRUCTORS */
  /*! Default constructor */
  ntlLightObject(ntlRenderGlobals *glob);
  /*! Constructor with parameters */
  ntlLightObject(ntlRenderGlobals *glob, const ntlColor& col);
  /*! Destructor */
  virtual ~ntlLightObject();

	/*! prepare light for rendering (for example shadow maps) */
	virtual void prepare( bool );
	
	/*! do the illumination... */
	virtual ntlColor illuminatePoint(ntlRay &reflectedRay, 
																	 ntlGeometryObject *closest,
																	 ntlColor &highlight);
	/*! shade the point */
	const ntlColor
	getShadedColor(const ntlRay &reflectedray, ntlVec3Gfx lightDir,
								 ntlMaterial *surf, ntlColor &highlight) const;


  /* access methods */
  /*! Access the active flag */
  inline void setActive(bool set) { mActive = set; }
  inline bool getActive() const { return mActive; }
  /*! Access the shadow flag */
  inline void setCastShadows(bool set) { mCastShadows = set; }
  inline bool getCastShadows() const { return mCastShadows; }
  /*! Access the light color */
  inline void setColor(ntlColor set) { mcColor = set; }
  inline ntlColor getColor() const { return mcColor; }
  
  /*! Access the omni light position */
  void setPosition(ntlVec3Gfx set) { mvPosition = set; }
  ntlVec3Gfx getPosition() const { return mvPosition; }
	

protected:
	/*! render globals */
	ntlRenderGlobals *mpGlob;

	/*! is this light acitve? */
	bool mActive;

	/*! does it cast shadows? */
	bool mCastShadows;

  /*! color of this light */
	ntlColor  mcColor;

	/*! light position */
	ntlVec3Gfx  mvPosition;

private:

};

#endif

