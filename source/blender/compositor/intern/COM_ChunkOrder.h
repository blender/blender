/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

#include "COM_ChunkOrderHotspot.h"

namespace blender::compositor {

/** Helper to determine the order how chunks are prioritized during execution. */
struct ChunkOrder {
  unsigned int index = 0;
  int x = 0;
  int y = 0;
  double distance = 0.0;

  friend bool operator<(const ChunkOrder &a, const ChunkOrder &b);

  void update_distance(ChunkOrderHotspot *hotspots, unsigned int len_hotspots);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:ChunkOrderHotspot")
#endif
};

}  // namespace blender::compositor
