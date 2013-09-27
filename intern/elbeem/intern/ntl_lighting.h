/** \file elbeem/intern/ntl_lighting.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * a light object
 * default omni light implementation
 *
 *****************************************************************************/
#ifndef NTL_LIGHTING_H
#define NTL_LIGHTING_H

#include "ntl_vector3dim.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

class ntlMaterial;
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

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:ntlLightObject")
#endif
};


//! Properties of an geo object, describing the reflection properties of the surface
class ntlMaterial
{
public:
  // CONSTRUCTORS
  //! Default constructor
  ntlMaterial( void );
  //! Constructor with parameters
  /*! Sets reflectance, ambient reflection, specular intensity
   *  specular exponent, mirror intensity 
   *  transparency, refraction index */
  ntlMaterial( string name,
         const ntlColor& Ref, const ntlColor& Amb, 
			   gfxReal Spec, gfxReal Exp, gfxReal Mirror,
			   gfxReal Trans, gfxReal Refrac, gfxReal TAdd,
			   const ntlColor& Att, int fres);
  //! Desctructor
  ~ntlMaterial() {};

	//! Calculate reflectance and refratance from Fresnel's law
	inline void calculateFresnel(const ntlVec3Gfx &dir, const ntlVec3Gfx &normal, gfxReal refIndex,
															 gfxReal &refl, gfxReal &trans );

protected:

  /* name of the material */
  string mName;

  //! Vector for reflectance of each color component (used in shade() of ray object)
  ntlColor  mDiffuseRefl;
  //! Ambient reflectance
  ntlColor mAmbientRefl;
  //! Specular reflection intensity
  gfxReal mSpecular;
  //! Specular phong exponent
  gfxReal mSpecExponent;
  //! Mirror intensity
  gfxReal mMirror;

  //! Transparence
  gfxReal mTransparence;
  //! Refraction index, nu(Air) is assumed 1
  gfxReal mRefracIndex;
  //! Should transparence be additive?
  gfxReal mTransAdditive;
  //! Color dependent transparency attentuation factors (negative logarithm stored)
  ntlColor mTransAttCol;
	//! Should the transparence and reflectivity be determined by fresnel?
	int mFresnel;


public:
  // access methods

  //! Returns the material name
  inline string getName() { return mName; }
  //! Returns the reflectance
  inline ntlColor  getDiffuseRefl() const { return ntlColor(mDiffuseRefl); }
  //! Returns the ambience
  inline ntlColor getAmbientRefl() const { return ntlColor(mAmbientRefl); }
  //! Returns the specular component
  inline gfxReal getSpecular() const { return mSpecular; }
  //! Returns the specular exponent component
  inline gfxReal getSpecExponent() const { return mSpecExponent; }
  //! Returns the mirror component
  inline gfxReal getMirror() const { return mMirror; }
  //! Returns the transparence component
  inline gfxReal getTransparence() const { return mTransparence; }
  //! Returns the refraction index component
  inline gfxReal getRefracIndex() const { return mRefracIndex; }
  //! Returns the transparency additive factor component
  inline gfxReal getTransAdditive() const { return mTransAdditive; }
  //! Returns the transparency attentuation
  inline ntlColor getTransAttCol() const { return mTransAttCol; }
	//! Get Fresnel flag
	inline int getFresnel( void ) { return mFresnel; }



  //! Returns the mat name
  inline void setName(string set) { mName = set; }
  //! Returns the reflectance
  inline void setDiffuseRefl(ntlColor set) { mDiffuseRefl=set; }
  //! Returns the ambience
  inline void setAmbientRefl(ntlColor set) { mAmbientRefl=set; }
  //! Returns the specular component
  inline void setSpecular(gfxReal set) { mSpecular=set; }
  //! Returns the specular exponent component
  inline void setSpecExponent(gfxReal set) { mSpecExponent=set; }
  //! Returns the mirror component
  inline void setMirror(gfxReal set) { mMirror=set; }
  //! Returns the transparence component
  inline void setTransparence(gfxReal set) { mTransparence=set; }
  //! Returns the refraction index component
  inline void setRefracIndex(gfxReal set) { mRefracIndex=set; }
  //! Returns the transparency additive factor component
  inline void setTransAdditive(gfxReal set) { mTransAdditive=set; }
  //! Returns the transparency attentuation
  inline void setTransAttCol(ntlColor set) { 
		ntlColor setlog = ntlColor( -log(set[0]), -log(set[1]), -log(set[2]) );
		mTransAttCol=setlog; }
	//! Set Fresnel on/off
	inline void setFresnel(int set) { mFresnel = set; }

private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:ntlMaterial")
#endif
};


/******************************************************************************
 * Macro to define the default surface properties for a newly created object
 *****************************************************************************/
#define GET_GLOBAL_DEFAULT_MATERIAL new ntlMaterial( "default",\
                                        ntlColor( 0.5 ), ntlColor(0.0), \
                                        1.0, 5.0, 0.0,   \
																				/*0.0 test:*/ 0.5 , 1.0, 0.0, \
																				ntlColor( 0.0 ), 0 ); 
																															


/******************************************************************************
 * Calculate reflectance and refratance from Fresnel's law
 * cf. Glassner p. 46
 *****************************************************************************/
inline void 
ntlMaterial::calculateFresnel(const ntlVec3Gfx &dir, const ntlVec3Gfx &normal, gfxReal refIndex,
															gfxReal &refl, gfxReal &trans)
{
	gfxReal c = -dot(dir, normal);
	if(c<0) { 
		refl = 0.0; trans = 0.0; return;
		//c = 0.0;
	}

	gfxReal r0 = ((refIndex-1.0)*(refIndex-1.0)) /
		((refIndex+1.0)*(refIndex+1.0));
	gfxReal omc = (1.0-c);
	gfxReal r =r0 + (1.0 - r0) * omc*omc*omc*omc*omc;

	//mMirror = r;
	//mTransparence = (1.0 - r);
	refl = r;
	trans = (1.0 - r);
	//errorOut(" fres ");
}


#endif


