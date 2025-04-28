/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
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

/* Wrapper around Mesh for TopologyRefinerFactory. */

class OsdMesh {
 public:
  /* Face-varying attribute that requires merging of corners with the same value, typically a UV
   * map. The resulting topology after merging is stored in a topology refiner fvar channel. The
   * merged attribute values are stored here, in a generic buffer used for different data types. */
  struct MergedFVar {
    const Attribute &attr;
    int channel = -1;
    vector<char> values;
  };

  Mesh &mesh;
  vector<MergedFVar> merged_fvars;

  explicit OsdMesh(Mesh &mesh) : mesh(mesh) {}

  Sdc::Options sdc_options();
  bool use_smooth_fvar(const Attribute &attr) const;
  bool use_smooth_fvar() const;
};

/* OpenSubdiv refiner and patch data structures. */

struct OsdData {
  unique_ptr<Far::TopologyRefiner> refiner;
  unique_ptr<Far::PatchTable> patch_table;
  unique_ptr<Far::PatchMap> patch_map;
  vector<OsdValue<float3>> refined_verts;

  void build(OsdMesh &osd_mesh);
};

/* Patch with OpenSubdiv evaluation. */

struct OsdPatch final : Patch {
  OsdData &osd_data;

  explicit OsdPatch(OsdData &data) : osd_data(data) {}
  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, const float v)
      const override;
};

CCL_NAMESPACE_END

#endif
