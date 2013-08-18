/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCL_OSLUTIL_H
#define CCL_OSLUTIL_H

/* NB: must match the value in kernel_types.h */
#define RAMP_TABLE_SIZE 256

// Return wireframe opacity factor [0, 1] given a geometry type in
// ("triangles", "polygons" or "patches"), and a line_width in raster
// or world space depending on the last (raster) boolean argument.
//
float wireframe(string edge_type, float line_width, int raster)
{
   // ray differentials are so big in diffuse context that this function would always return "wire"
   if (raytype("path:diffuse")) return 0.0;

   int np = 0;
   point p[64];
   float pixelWidth = 1;

   if (edge_type == "triangles")
   {
      np = 3;
      if (!getattribute("geom:trianglevertices", p))
         return 0.0;
   }
   else if (edge_type == "polygons" || edge_type == "patches")
   {
      getattribute("geom:numpolyvertices", np);
      if (np < 3 || !getattribute("geom:polyvertices", p))
         return 0.0;
   }

   if (raster)
   {
      // Project the derivatives of P to the viewing plane defined
      // by I so we have a measure of how big is a pixel at this point
      float pixelWidthX = length(Dx(P) - dot(Dx(P), I) * I);
      float pixelWidthY = length(Dy(P) - dot(Dy(P), I) * I);
      // Take the average of both axis' length
      pixelWidth = (pixelWidthX + pixelWidthY) / 2;
   }

   // Use half the width as the neighbor face will render the
   // other half. And take the square for fast comparison
   pixelWidth *= 0.5 * line_width;
   pixelWidth *= pixelWidth;
   for (int i = 0; i < np; i++)
   {
      int i2 = i ? i - 1 : np - 1;
      vector dir = P - p[i];
      vector edge = p[i] - p[i2];
      vector crs = cross(edge, dir);
      // At this point dot(crs, crs) / dot(edge, edge) is
      // the square of area / length(edge) == square of the
      // distance to the edge.
      if (dot(crs, crs) < (dot(edge, edge) * pixelWidth))
         return 1;
   }
   return 0;
}

float wireframe(string edge_type, float line_width) { return wireframe(edge_type, line_width, 1); }
float wireframe(string edge_type) { return wireframe(edge_type, 1.0, 1); }
float wireframe() { return wireframe("polygons", 1.0, 1); }

#endif /* CCL_OSLUTIL_H */
