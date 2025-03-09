/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifdef WITH_OPENSUBDIV

#  include "subd/patch.h"

#  include "util/unique_ptr.h"
#  include "util/vector.h"

#  include <opensubdiv/far/patchMap.h>
#  include <opensubdiv/far/patchTableFactory.h>
#  include <opensubdiv/far/primvarRefiner.h>
#  include <opensubdiv/far/topologyRefinerFactory.h>

CCL_NAMESPACE_BEGIN

/* Directly use some OpenSubdiv namespaces for brevity. */
namespace Far = OpenSubdiv::Far;
namespace Sdc = OpenSubdiv::Sdc;

class Attribute;
class Mesh;

/* OpenSubdiv interface for vertex and attribute values. */

template<typename T> struct OsdValue {
  T value;

  OsdValue() = default;

  void Clear(void *unused = nullptr)
  {
    (void)unused;
    memset((void *)&value, 0, sizeof(T));
  }

  void AddWithWeight(OsdValue<T> const &src, float weight)
  {
    value += src.value * weight;
  }
};

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

/* Patch with OpenSubdiv evaluation. */

struct OsdPatch : Patch {
  OsdData *osd_data;

  OsdPatch() = default;
  OsdPatch(OsdData *data) : osd_data(data) {}

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) override;
};

CCL_NAMESPACE_END

#endif
