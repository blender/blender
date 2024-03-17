/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_attribute.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"

#include "GEO_mesh_merge_by_distance.hh"

#include "BLI_color.hh"
#include "BLI_math_vector.h"
#include "BLI_span.hh"

#include "ply_import_mesh.hh"

namespace blender::io::ply {
Mesh *convert_ply_to_mesh(PlyData &data, const PLYImportParams &params)
{
  Mesh *mesh = BKE_mesh_new_nomain(
      data.vertices.size(), data.edges.size(), data.face_sizes.size(), data.face_vertices.size());

  mesh->vert_positions_for_write().copy_from(data.vertices);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  if (!data.edges.is_empty()) {
    MutableSpan<int2> edges = mesh->edges_for_write();
    for (const int i : data.edges.index_range()) {
      int32_t v1 = data.edges[i].first;
      int32_t v2 = data.edges[i].second;
      if (v1 >= mesh->verts_num) {
        fprintf(stderr, "Invalid PLY vertex index in edge %i/1: %d\n", i, v1);
        v1 = 0;
      }
      if (v2 >= mesh->verts_num) {
        fprintf(stderr, "Invalid PLY vertex index in edge %i/2: %d\n", i, v2);
        v2 = 0;
      }
      edges[i] = {v1, v2};
    }
  }

  /* Add faces to the mesh. */
  if (!data.face_sizes.is_empty()) {
    MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
    MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

    /* Fill in face data. */
    uint32_t offset = 0;
    for (const int i : data.face_sizes.index_range()) {
      uint32_t size = data.face_sizes[i];
      face_offsets[i] = offset;
      for (int j = 0; j < size; j++) {
        uint32_t v = data.face_vertices[offset + j];
        if (v >= mesh->verts_num) {
          fprintf(stderr, "Invalid PLY vertex index in face %i loop %i: %u\n", i, j, v);
          v = 0;
        }
        corner_verts[offset + j] = data.face_vertices[offset + j];
      }
      offset += size;
    }
  }

  /* Vertex colors */
  if (!data.vertex_colors.is_empty() && params.vertex_colors != PLY_VERTEX_COLOR_NONE) {
    /* Create a data layer for vertex colors and set them. */
    bke::SpanAttributeWriter colors = attributes.lookup_or_add_for_write_span<ColorGeometry4f>(
        "Col", bke::AttrDomain::Point);

    if (params.vertex_colors == PLY_VERTEX_COLOR_SRGB) {
      for (const int i : data.vertex_colors.index_range()) {
        srgb_to_linearrgb_v4(colors.span[i], data.vertex_colors[i]);
      }
    }
    else {
      for (const int i : data.vertex_colors.index_range()) {
        copy_v4_v4(colors.span[i], data.vertex_colors[i]);
      }
    }
    colors.finish();
    BKE_id_attributes_active_color_set(&mesh->id, "Col");
    BKE_id_attributes_default_color_set(&mesh->id, "Col");
  }

  /* Uvmap */
  if (!data.uv_coordinates.is_empty()) {
    bke::SpanAttributeWriter<float2> uv_map = attributes.lookup_or_add_for_write_only_span<float2>(
        "UVMap", bke::AttrDomain::Corner);
    for (const int i : data.face_vertices.index_range()) {
      uv_map.span[i] = data.uv_coordinates[data.face_vertices[i]];
    }
    uv_map.finish();
  }

  /* Calculate edges from the rest of the mesh. */
  bke::mesh_calc_edges(*mesh, true, false);

  /* If we have custom vertex normals, set them (note: important to do this
   * after initializing the loops). */
  if (!data.vertex_normals.is_empty()) {
    if (!data.face_sizes.is_empty()) {
      /* For a non-point-cloud mesh, set custom normals. */
      BKE_mesh_set_custom_normals_from_verts(
          mesh, reinterpret_cast<float(*)[3]>(data.vertex_normals.data()));
    }
    else if (params.import_attributes) {
      /* If we have no faces, add vertex normals as custom attribute. */
      attributes.add<float3>(
          "normal",
          bke::AttrDomain::Point,
          bke::AttributeInitVArray(VArray<float3>::ForSpan(data.vertex_normals)));
    }
  }
  else {
    /* No vertex normals: set faces to sharp. */
    bke::mesh_smooth_set(*mesh, false);
  }

  /* Custom attributes: add them after anything above. */
  if (params.import_attributes && !data.vertex_custom_attr.is_empty()) {
    for (const PlyCustomAttribute &attr : data.vertex_custom_attr) {
      attributes.add<float>(attr.name,
                            bke::AttrDomain::Point,
                            bke::AttributeInitVArray(VArray<float>::ForSpan(attr.data)));
    }
  }

  /* Merge all vertices on the same location. */
  if (params.merge_verts) {
    std::optional<Mesh *> merged_mesh = blender::geometry::mesh_merge_by_distance_all(
        *mesh, IndexMask(mesh->verts_num), 0.0001f);
    if (merged_mesh) {
      BKE_id_free(nullptr, &mesh->id);
      mesh = *merged_mesh;
    }
  }

  return mesh;
}
}  // namespace blender::io::ply
