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

#pragma once

#include <vector>

#include <carve/carve.hpp>
#include <carve/tag.hpp>
#include <carve/geom.hpp>
#include <carve/kd_node.hpp>
#include <carve/geom3d.hpp>
#include <carve/aabb.hpp>

namespace carve {
  namespace point {

    inline size_t PointSet::vertexToIndex_fast(const Vertex *v) const {
      return (size_t)(v - &vertices[0]);
    }

  }
}
