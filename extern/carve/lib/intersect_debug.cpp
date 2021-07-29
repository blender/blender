// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/csg.hpp>

#include <list>
#include <set>
#include <iostream>

#include <algorithm>

#include "intersect_debug.hpp"

namespace carve {
  namespace csg {

#if defined(CARVE_DEBUG)

#define DEBUG_DRAW_FACE_EDGES
#define DEBUG_DRAW_INTERSECTIONS
// #define DEBUG_DRAW_OCTREE
#define DEBUG_DRAW_INTERSECTION_LINE
// #define DEBUG_DRAW_GROUPS
// #define DEBUG_PRINT_RESULT_FACES

    IntersectDebugHooks *g_debug = NULL;

    IntersectDebugHooks *intersect_installDebugHooks(IntersectDebugHooks *hooks) {
      IntersectDebugHooks *h = g_debug;
      g_debug = hooks;
      return h;
    }

    bool intersect_debugEnabled() { return true; }

#else

    IntersectDebugHooks *intersect_installDebugHooks(IntersectDebugHooks * /* hooks */) {
      return NULL;
    }

    bool intersect_debugEnabled() { return false; }

#endif

  }
}
