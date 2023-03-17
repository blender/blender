/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include "BKE_mesh.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"
#include "BLI_hash.hh"
#include "BLI_math.h"

#include "RNA_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "DNA_layer_types.h"

#include "ply_data.hh"

namespace blender::io::ply {

Mesh *do_triangulation(const Mesh *mesh, bool force_triangulation);
void set_world_axes_transform(Object *object, const eIOAxis forward, const eIOAxis up);

struct UV_vertex_key {
  float2 UV;
  int mesh_vertex_index;

  UV_vertex_key(float2 UV, int vertex_index) : UV(UV), mesh_vertex_index(vertex_index)
  {
  }

  bool operator==(const UV_vertex_key &r) const
  {
    return (UV == r.UV && mesh_vertex_index == r.mesh_vertex_index);
  }

  uint64_t hash() const
  {
    return get_default_hash_3(UV.x, UV.y, mesh_vertex_index);
  }
};

void generate_vertex_map(const Mesh *mesh,
                         const float2 *uv_map,
                         const PLYExportParams &export_params,
                         Map<UV_vertex_key, int> &r_map);

void load_plydata(PlyData &plyData, Depsgraph *depsgraph, const PLYExportParams &export_params);
}  // namespace blender::io::ply
