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

#include <carve/carve.hpp>
#include <carve/geom.hpp>

namespace carve {
  namespace colour {
    static inline void HSV2RGB(float H, float S, float V, float &r, float &g, float &b) {
      H = 6.0f * H;
      if (S < 5.0e-6) {
        r = g = b = V; return;
      } else {
        int i = (int)H;
        float f = H - i;
        float p1 = V * (1.0f - S);
        float p2 = V * (1.0f - S * f);
        float p3 = V * (1.0f - S * (1.0f - f));
        switch (i) {
        case 0:  r = V;  g = p3; b = p1; return;
        case 1:  r = p2; g = V;  b = p1; return;
        case 2:  r = p1; g = V;  b = p3; return;
        case 3:  r = p1; g = p2; b = V;  return;
        case 4:  r = p3; g = p1; b = V;  return;
        case 5:  r = V;  g = p1; b = p2; return;
        }
      }
      r = g = b = 0.0;
    }
  }
}
