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

#include <opensubdiv/far/topologyRefiner.h>

#include "internal/topology/mesh_topology.h"

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

  // Topology of the mesh which corresponds to the base level.
  //
  // All the indices and values are kept exactly the same as user-defined
  // converter provided them. This allows to easily compare values which might
  // be touched by the refinement process.
  //
  // On a more technical note this allows to easier/faster to compare following
  // things:
  //
  //  - Face vertices, where OpenSubdiv could re-arrange them to keep winding
  //    uniform.
  //
  //  - Vertex crease where OpenSubdiv will force crease for non-manifold or
  //    corner vertices.
  MeshTopology base_mesh_topology;

  MEM_CXX_CLASS_ALLOC_FUNCS("TopologyRefinerImpl");
};

}  // namespace blender::opensubdiv

#endif  // OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_
