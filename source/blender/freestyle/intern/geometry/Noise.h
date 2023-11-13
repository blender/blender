/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define Perlin noise
 */

#include "Geom.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

#define _NOISE_B 0x100

using namespace Geometry;

/** Class to provide Perlin Noise functionalities */
class Noise {
 public:
  /** Builds a Noise object */
  Noise(long seed = -1);

  /** Destructor */
  ~Noise() {}

  /** Returns a noise value for a 1D element */
  float turbulence1(float arg, float freq, float amp, uint oct = 4);

  /** Returns a noise value for a 2D element */
  float turbulence2(Vec2f &v, float freq, float amp, uint oct = 4);

  /** Returns a noise value for a 3D element */
  float turbulence3(Vec3f &v, float freq, float amp, uint oct = 4);

  /** Returns a smooth noise value for a 1D element */
  float smoothNoise1(float arg);

  /** Returns a smooth noise value for a 2D element */
  float smoothNoise2(Vec2f &vec);

  /** Returns a smooth noise value for a 3D element */
  float smoothNoise3(Vec3f &vec);

 private:
  int p[_NOISE_B + _NOISE_B + 2];
  float g3[_NOISE_B + _NOISE_B + 2][3];
  float g2[_NOISE_B + _NOISE_B + 2][2];
  float g1[_NOISE_B + _NOISE_B + 2];
  /* UNUSED */
  // int start;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Noise")
#endif
};

} /* namespace Freestyle */
