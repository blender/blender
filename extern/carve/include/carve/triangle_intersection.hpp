// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <carve/carve.hpp>

#include <carve/geom.hpp>

namespace carve {
  namespace geom {

    enum TriangleIntType {
      TR_TYPE_NONE = 0,
      TR_TYPE_TOUCH = 1,
      TR_TYPE_INT = 2
    };

    enum TriangleInt {
      TR_INT_NONE = 0,      // no intersection.
      TR_INT_INT = 1,       // intersection.
      TR_INT_VERT = 2,      // intersection due to shared vertex.
      TR_INT_EDGE = 3,      // intersection due to shared edge.
      TR_INT_TRI = 4        // intersection due to identical triangle.
    };

    TriangleInt triangle_intersection(const vector<2> tri_a[3], const vector<2> tri_b[3]);
    TriangleInt triangle_intersection(const vector<3> tri_a[3], const vector<3> tri_b[3]);

    bool triangle_intersection_simple(const vector<2> tri_a[3], const vector<2> tri_b[3]);
    bool triangle_intersection_simple(const vector<3> tri_a[3], const vector<3> tri_b[3]);

    TriangleIntType triangle_intersection_exact(const vector<2> tri_a[3], const vector<2> tri_b[3]);
    TriangleIntType triangle_intersection_exact(const vector<3> tri_a[3], const vector<3> tri_b[3]);

    TriangleIntType triangle_linesegment_intersection_exact(const vector<2> tri_a[3], const vector<2> line_b[2]);
    TriangleIntType triangle_point_intersection_exact(const vector<2> tri_a[3], const vector<2> &pt_b);
  }
}
