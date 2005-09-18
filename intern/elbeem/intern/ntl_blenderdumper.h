/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Replaces std. raytracer, and only dumps time dep. objects to disc, header
 *
 *****************************************************************************/
#ifndef NTL_BLENDERDUMPER_H
#include "ntl_raytracer.h"

template<class Scalar> class ntlMatrix4x4;

class ntlBlenderDumper :
	public ntlRaytracer
{
public:
  /*! Constructor */
  ntlBlenderDumper(string filename, bool commandlineMode);
  /*! Destructor */
  virtual ~ntlBlenderDumper( void );

  /*! render scene (a single pictures) */
  virtual int renderScene( void );

protected:

	//! transform matrix
	ntlMatrix4x4<gfxReal> *mpTrafo;
};

#define NTL_BLENDERDUMPER_H
#endif

