/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a pseudo Perlin noise
 */

#include "Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class PseudoNoise {
 public:
  virtual ~PseudoNoise() {}

  real smoothNoise(real x);
  real linearNoise(real x);

  real turbulenceSmooth(real x, unsigned nbOctave = 8);
  real turbulenceLinear(real x, unsigned nbOctave = 8);

  static void init(long seed);

 protected:
  static const unsigned NB_VALUE_NOISE = 512;
  static real _values[NB_VALUE_NOISE];

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:PseudoNoise")
#endif
};

} /* namespace Freestyle */
