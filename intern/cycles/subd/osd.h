/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPENSUBDIV

#  include "scene/attribute.h"
#  include "scene/mesh.h"

#  include "subd/patch.h"
#  include "subd/split.h"

#  include <opensubdiv/far/patchMap.h>
#  include <opensubdiv/far/patchTableFactory.h>
#  include <opensubdiv/far/primvarRefiner.h>
#  include <opensubdiv/far/topologyRefinerFactory.h>

/* specializations of TopologyRefinerFactory for ccl::Mesh */

CCL_NAMESPACE_BEGIN

using namespace OpenSubdiv;

/* struct that implements OpenSubdiv's vertex interface */

template<typename T> struct OsdValue {
  T value;

  OsdValue() = default;

  void Clear(void * /*unused*/ = nullptr)
  {
    memset(&value, 0, sizeof(T));
  }

  void AddWithWeight(const OsdValue<T> &src, const float weight)
  {
    value += src.value * weight;
  }
};

template<>
inline void OsdValue<uchar4>::AddWithWeight(const OsdValue<uchar4> &src, const float weight)
{
  for (int i = 0; i < 4; i++) {
    value[i] += (uchar)(src.value[i] * weight);
  }
}

/* class for holding OpenSubdiv data used during tessellation */

class OsdData {
  Mesh *mesh = nullptr;
  vector<OsdValue<float3>> verts;
  unique_ptr<Far::TopologyRefiner> refiner;
  unique_ptr<Far::PatchTable> patch_table;
  unique_ptr<Far::PatchMap> patch_map;

 public:
  OsdData() = default;

  void build_from_mesh(Mesh *mesh_);
  int calculate_max_isolation();
  void subdivide_attribute(Attribute &attr);

  friend struct OsdPatch;
  friend class Mesh;
};

/* ccl::Patch implementation that uses OpenSubdiv for eval */

struct OsdPatch : Patch {
  OsdData *osd_data;

  OsdPatch() = default;
  OsdPatch(OsdData *data) : osd_data(data) {}

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) override;
};

CCL_NAMESPACE_END

#endif
