/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

struct ChunkOrderHotspot {
  int x;
  int y;
  float addition;

  ChunkOrderHotspot(int x, int y, float addition) : x(x), y(y), addition(addition) {}

  double calc_distance(int x, int y);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:ChunkOrderHotspot")
#endif
};

}  // namespace blender::compositor
