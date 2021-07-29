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

#include <carve/csg.hpp>

#include "csg_detail.hpp"

struct carve::csg::detail::Data {
//        * @param[out] vmap A mapping from vertex pointer to intersection point.
//        * @param[out] emap A mapping from edge pointer to intersection points.
//        * @param[out] fmap A mapping from face pointer to intersection points.
//        * @param[out] fmap_rev A mapping from intersection points to face pointers.
  // map from intersected vertex to intersection point.
  VVMap vmap;

  // map from intersected edge to intersection points.
  EIntMap emap;

  // map from intersected face to intersection points.
  FVSMap fmap;

  // map from intersection point to intersected faces.
  VFSMap fmap_rev;

  // created by divideEdges().
  // holds, for each edge, an ordered vector of inserted vertices.
  EVVMap divided_edges;

  // created by faceSplitEdges.
  FV2SMap face_split_edges;

  // mapping from vertex to edge for potentially intersected
  // faces. Saves building the vertex to edge map for all faces of
  // both meshes.
  VEVecMap vert_to_edges;
};
