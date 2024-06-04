/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

/* Needed for BKE_ccg.hh. */
#include "BLI_assert.h"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_struct_equality_utils.hh"
#include "BLI_virtual_array.hh"

#include "DNA_customdata_types.h"

#include "BKE_ccg.hh"

namespace blender::gpu {
class Batch;
}
struct PBVHNode;
struct Mesh;
struct CustomData;
struct SubdivCCG;
struct BMesh;
struct BMFace;
namespace blender::bke {
enum class AttrDomain : int8_t;
}

namespace blender::draw::pbvh {

class GenericRequest {
 public:
  std::string name;
  eCustomDataType type;
  bke::AttrDomain domain;
  GenericRequest(const StringRef name, const eCustomDataType type, const bke::AttrDomain domain)
      : name(name), type(type), domain(domain)
  {
  }
  BLI_STRUCT_EQUALITY_OPERATORS_3(GenericRequest, type, domain, name);
};

enum class CustomRequest : int8_t {
  Position,
  Normal,
  Mask,
  FaceSet,
};

using AttributeRequest = std::variant<CustomRequest, GenericRequest>;

struct PBVHBatches;

struct PBVH_GPU_Args {
  int pbvh_type;

  BMesh *bm;
  const Mesh *mesh;
  MutableSpan<float3> vert_positions;
  Span<int> corner_verts;
  Span<int> corner_edges;
  const CustomData *vert_data;
  const CustomData *corner_data;
  const CustomData *face_data;
  Span<float3> vert_normals;
  Span<float3> face_normals;

  const char *active_color;
  const char *render_color;

  int face_sets_color_seed;
  int face_sets_color_default;

  SubdivCCG *subdiv_ccg;
  Span<int> grid_indices;
  CCGKey ccg_key;
  Span<CCGElem *> grids;

  Span<int> prim_indices;

  VArraySpan<bool> hide_poly;

  Span<int3> corner_tris;
  Span<int> tri_faces;

  /* BMesh. */
  const Set<BMFace *, 0> *bm_faces;
  int cd_mask_layer;
};

void node_update(PBVHBatches *batches, const PBVH_GPU_Args &args);
void update_pre(PBVHBatches *batches, const PBVH_GPU_Args &args);

void node_gpu_flush(PBVHBatches *batches);
PBVHBatches *node_create(const PBVH_GPU_Args &args);
void node_free(PBVHBatches *batches);
gpu::Batch *tris_get(PBVHBatches *batches,
                     Span<AttributeRequest> attrs,
                     const PBVH_GPU_Args &args,
                     bool do_coarse_grids);
gpu::Batch *lines_get(PBVHBatches *batches,
                      Span<AttributeRequest> attrs,
                      const PBVH_GPU_Args &args,
                      bool do_coarse_grids);

}  // namespace blender::draw::pbvh
