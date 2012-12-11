//
//  Filename         : PseudoNoise.h
//  Author(s)        : Fredo Durand
//  Purpose          : Class to define a pseudo Perlin noise
//  Date of creation : 16/06/2003
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

#ifndef  PSEUDONOISE_H
# define PSEUDONOISE_H

# include "FreestyleConfig.h"
# include "Precision.h"

class LIB_SYSTEM_EXPORT PseudoNoise 
{
public:

  PseudoNoise ();

  virtual ~PseudoNoise () {}
	
  real smoothNoise (real x);
  real linearNoise (real x);

  real turbulenceSmooth (real x, unsigned nbOctave = 8);
  real turbulenceLinear (real x, unsigned nbOctave = 8);

  static void init (long seed);

protected:

  static real *_values;
};

#endif // PSEUDONOISE_H

