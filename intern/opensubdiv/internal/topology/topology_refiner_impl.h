// Copyright 2016 Blender Foundation
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#ifndef OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_
#define OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/far/topologyRefiner.h>

#include "internal/base/memory.h"
#include "internal/topology/mesh_topology.h"
#include "opensubdiv_topology_refiner_capi.h"

struct OpenSubdiv_Converter;

namespace blender {
namespace opensubdiv {

class TopologyRefinerImpl {
 public:
  // NOTE: Will return nullptr if topology refiner can not be created (for
  // example, when topology is detected to be corrupted or invalid).
  static TopologyRefinerImpl *createFromConverter(
      OpenSubdiv_Converter *converter, const OpenSubdiv_TopologyRefinerSettings &settings);

  TopologyRefinerImpl();
  ~TopologyRefinerImpl();

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

}  // namespace opensubdiv
}  // namespace blender

struct OpenSubdiv_TopologyRefinerImpl : public blender::opensubdiv::TopologyRefinerImpl {
};

#endif  // OPENSUBDIV_TOPOLOGY_REFINER_IMPL_H_
