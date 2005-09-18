/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a geometry object
 * all other geometry objects are derived from this one
 *
 *****************************************************************************/
#ifndef NTL_MATERIAL_HH
#define NTL_MATERIAL_HH

#include "ntl_vector3dim.h"
class ntlRay;


//! Properties of an geo object, describing the reflection properties of the surface
class ntlMaterial
{
public:
  // CONSTRUCTORS
  //! Default constructor
  inline ntlMaterial( void );
  //! Constructor with parameters
  /*! Sets reflectance, ambient reflection, specular intensity
   *  specular exponent, mirror intensity 
   *  transparency, refraction index */
  inline ntlMaterial( string name,
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

};




/******************************************************************************
 * Default constructor
 *****************************************************************************/
inline ntlMaterial::ntlMaterial( void ) : 
	mName( "default" ),
  mDiffuseRefl(0.5,0.5,0.5),  mAmbientRefl(0.0,0.0,0.0),
  mSpecular(0.0), mSpecExponent(0.0), mMirror(0.0),
  mTransparence(0.0), mRefracIndex(0.0), mTransAdditive(0.0), mTransAttCol(0.0),
	mFresnel( 0 )
  //mNtfId(0), mNtfFluid(0), mNtfSolid(0)
{ 
  // just do default init...
}



/******************************************************************************
 * Init constructor
 *****************************************************************************/
inline 
ntlMaterial::ntlMaterial( string name,
													const ntlColor& Ref, const ntlColor& Amb,
													gfxReal Spec, gfxReal SpecEx, gfxReal Mirr,
													gfxReal Trans, gfxReal Refrac, gfxReal TAdd,
													const ntlColor& Att, int fres)
{
	mName					= name;
	mDiffuseRefl  = Ref;
	mAmbientRefl  = Amb;
	mSpecular     = Spec;
	mSpecExponent = SpecEx;
	mMirror       = Mirr;
	mTransparence = Trans;
	mRefracIndex  = Refrac;
	mTransAdditive = TAdd;
	mTransAttCol   = Att;
	mFresnel 			= fres;
}

/******************************************************************************
 * Macro to define the default surface properties for a newly created object
 *****************************************************************************/
#define GET_GLOBAL_DEFAULT_MATERIAL new ntlMaterial( "default",\
                                        ntlColor( 0.5 ), ntlColor(0.0), \
                                        1.0, 5.0, 0.0,   \
																				0.0, 1.0, 0.0, \
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
