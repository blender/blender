/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a pseudo Perlin noise
 */

#include "Precision.h"

#include "MEM_guardedalloc.h"

namespace Freestyle {

class PseudoNoise {
 public:
  virtual ~PseudoNoise() {}

  real smoothNoise(real x);
  real linearNoise(real x);

  real turbulenceSmooth(real x, uint nbOctave = 8);
  real turbulenceLinear(real x, uint nbOctave = 8);

  static void init(long seed);

 protected:
  static const uint NB_VALUE_NOISE = 512;
  static real _values[NB_VALUE_NOISE];

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:PseudoNoise")
};

} /* namespace Freestyle */
