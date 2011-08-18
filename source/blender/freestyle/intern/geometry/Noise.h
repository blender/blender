//
//  Filename         : Noise.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Class to define Perlin noise
//  Date of creation : 12/01/2004
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef NOISE_H
# define NOISE_H


# include "../system/FreestyleConfig.h"
# include "Geom.h"

#define _Noise_B_ 0x100

using namespace Geometry;
using namespace std;

/*! Class to provide Perlin Noise functionalities */
class LIB_GEOMETRY_EXPORT Noise
{
 public:

  /*! Builds a Noise object */
  Noise(long seed = -1);
  /*! Destructor */
  ~Noise() {}

  /*! Returns a noise value for a 1D element */
  float turbulence1(float arg, float freq, float amp, unsigned oct = 4);

  /*! Returns a noise value for a 2D element */
  float turbulence2(Vec2f& v, float freq, float amp, unsigned oct = 4);

  /*! Returns a noise value for a 3D element */
  float turbulence3(Vec3f& v, float freq, float amp, unsigned oct = 4);

  /*! Returns a smooth noise value for a 1D element */
  float smoothNoise1(float arg);
  /*! Returns a smooth noise value for a 2D element */
  float smoothNoise2(Vec2f& vec);
  /*! Returns a smooth noise value for a 3D element */
  float smoothNoise3(Vec3f& vec);

 private:

  int p[ _Noise_B_ + _Noise_B_ + 2];
  float g3[ _Noise_B_ + _Noise_B_ + 2][3];
  float g2[ _Noise_B_ + _Noise_B_ + 2][2];
  float g1[ _Noise_B_ + _Noise_B_ + 2];
  int start;
};

#endif // NOISE_H
