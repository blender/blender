/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Replaces std. raytracer, and only dumps time dep. objects to disc, header
 *
 *****************************************************************************/
#ifndef NTL_BLENDERDUMPER_H
#include "ntl_world.h"

class ntlBlenderDumper :
	public ntlWorld
{
public:
  /*! Constructor */
  ntlBlenderDumper();
  ntlBlenderDumper(string filename, bool commandlineMode);
  /*! Destructor */
  virtual ~ntlBlenderDumper( void );

  /*! render scene (a single pictures) */
  virtual int renderScene( void );

protected:

};

#define NTL_BLENDERDUMPER_H
#endif

