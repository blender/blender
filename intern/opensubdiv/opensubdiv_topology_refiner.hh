/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#ifndef OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_
#define OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <vector>

#include <opensubdiv/far/topologyRefiner.h>

#include "MEM_guardedalloc.h"

// Those settings don't really belong to OpenSubdiv's topology refiner, but
// we are keeping track of them on our side of topology refiner. This is to
// make it possible to ensure we are not trying to abuse same OpenSubdiv's
// topology refiner with different subdivision levels or with different
// adaptive settings.
struct OpenSubdiv_TopologyRefinerSettings {
  bool is_adaptive;
  int level;
};

struct OpenSubdiv_Converter;

namespace blender::opensubdiv {

class TopologyRefinerImpl {
 public:
  // NOTE: Will return nullptr if topology refiner can not be created (for
  // example, when topology is detected to be corrupted or invalid).
  static TopologyRefinerImpl *createFromConverter(
      OpenSubdiv_Converter *converter, const OpenSubdiv_TopologyRefinerSettings &settings);

  TopologyRefinerImpl();
  ~TopologyRefinerImpl();

  const OpenSubdiv::Far::TopologyLevel &base_level() const
  {
    return topology_refiner->GetLevel(0);
  }

  // Check whether this topology refiner defines same topology as the given
  // converter.
  // Covers options, geometry, and geometry tags.
  bool isEqualToConverter(const OpenSubdiv_Converter *converter) const;

  OpenSubdiv::Far::TopologyRefiner *topology_refiner;

  // Subdivision settingsa this refiner is created for.
  OpenSubdiv_TopologyRefinerSettings settings;

  //////////////////////////////////////////////////////////////////////////////
  // Topology of the mesh which corresponds to the base level.
  //
  // Only the parts which are needed to tell whether a different converter describes the same mesh
  // are stored. See #isEqualToConverter.
  //
  // All the indices and values are exactly the ones the converter provided: they are copied
  // straight from it before the refiner is built, never read back out of the refiner. That is
  // what makes them comparable against a later converter, since the refinement process changes
  // the values the refiner itself stores:
  //
  //  - Face vertices, where OpenSubdiv could re-arrange them to keep winding
  //    uniform.
  //
  //  - Vertex crease where OpenSubdiv will force crease for non-manifold or
  //    corner vertices.

  int base_verts_num = 0;

  // The pair of vertices each edge connects, but only up to the last edge which has sharpness.
  //
  // Sharpness is the only per-edge data which can not be derived from the faces, so the vertices
  // of the remaining edges never have to be compared. Many meshes have no sharp edges at all, in
  // which case this stays empty.
  std::vector<std::pair<int, int>> base_edges_sparse;

  // Sharpness of each edge, matching #OpenSubdiv_Converter::edge_sharpness: either one element per
  // edge of the base mesh, or empty when no edge is sharp. Since it is stored densely, comparing
  // it against a converter also compares the number of edges.
  std::vector<float> base_edge_sharpness;

  // The face topology of the base mesh, see #OffsetIndices and #Mesh::faces().
  std::vector<int> base_face_offsets;

  // Continuous array of all vertices of all faces, sliced by the face offsets.
  std::vector<int> base_corner_verts;

  // Sharpness of each vertex, matching #OpenSubdiv_Converter::vert_sharpness: either
  // #base_verts_num elements, or empty when no vertex is sharp.
  std::vector<float> base_vert_sharpness;

  MEM_CXX_CLASS_ALLOC_FUNCS("TopologyRefinerImpl");
};

}  // namespace blender::opensubdiv

#endif  // OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_
