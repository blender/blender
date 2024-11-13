/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Pseudo-random number generator
 */

/* TODO: Check whether we could replace this with BLI rand stuff. */

#include "../system/Precision.h"

#include "MEM_guardedalloc.h"

namespace Freestyle {

class RandGen {
 public:
  static real drand48();
  static void srand48(long seedval);

 private:
  static void next();

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:RandGen")
};

} /* namespace Freestyle */
