/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

#include "GEO_mesh_merge_verts.hh"

#include "BLI_color_types.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_span.hh"

#include "IO_validate.hh"

#include "ply_import_mesh.hh"

#include "CLG_log.h"

#include <cinttypes>

namespace blender {

static CLG_LogRef LOG = {"io.ply"};

namespace io::ply {
Mesh *convert_ply_to_mesh(PlyData &data, const PLYImportParams &params)
{
  if (!validate::size_fits_in_int(data.vertices.size()) ||
      !validate::size_fits_in_int(data.edges.size()) ||
      !validate::size_fits_in_int(data.face_sizes.size()) ||
      !validate::size_fits_in_int(data.face_vertices.size()))
  {
    CLOG_WARN(&LOG, "PLY mesh too large to import, exceeds max int size");
    return BKE_mesh_new_nomain(0, 0, 0, 0);
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      data.vertices.size(), data.edges.size(), data.face_sizes.size(), data.face_vertices.size());

  mesh->vert_positions_for_write().copy_from(data.vertices);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();

  if (!data.edges.is_empty()) {
    MutableSpan<int2> edges = mesh->edges_for_write();
    for (const int i : data.edges.index_range()) {
      int32_t v1 = data.edges[i].first;
      int32_t v2 = data.edges[i].second;
      if (!validate::index_in_range(v1, mesh->verts_num)) {
        CLOG_WARN(&LOG, "Invalid PLY vertex index in edge %i/1: %d", i, v1);
        v1 = 0;
      }
      if (!validate::index_in_range(v2, mesh->verts_num)) {
        CLOG_WARN(&LOG, "Invalid PLY vertex index in edge %i/2: %d", i, v2);
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
    int64_t offset = 0;
    for (const int64_t i : data.face_sizes.index_range()) {
      const int64_t size = data.face_sizes[i];
      face_offsets[i] = offset;
      for (int64_t j = 0; j < size; j++) {
        uint32_t v = data.face_vertices[offset + j];
        if (!validate::index_in_range(v, mesh->verts_num)) {
          CLOG_WARN(
              &LOG, "Invalid PLY vertex index in face %" PRId64 " loop %" PRId64 ": %u", i, j, v);
          v = 0;
        }
        corner_verts[offset + j] = v;
      }
      offset += size;
    }
  }

  /* Vertex colors */
  if (!data.vertex_colors.is_empty() && params.vertex_colors != ePLYVertexColorMode::None) {
    /* Create a data layer for vertex colors and set them. */
    bke::SpanAttributeWriter colors = attributes.lookup_or_add_for_write_span<ColorGeometry4f>(
        "Col", bke::AttrDomain::Point);

    if (params.vertex_colors == ePLYVertexColorMode::sRGB) {
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
      uint32_t v = data.face_vertices[i];
      uv_map.span[i] = validate::index_in_range(v, data.uv_coordinates.size()) ?
                           data.uv_coordinates[v] :
                           float2(0.0f);
    }
    uv_map.finish();
    mesh->uv_maps_active_set("UVMap");
    mesh->uv_maps_default_set("UVMap");
  }

  /* If we have custom vertex normals, set them
   * (NOTE: important to do this after initializing the loops). */
  bool set_custom_normals_for_verts = false;
  if (!data.vertex_normals.is_empty()) {
    if (!data.face_sizes.is_empty()) {
      /* For a non-point-cloud mesh, set custom normals. */
      /* Deferred because this relies on valid mesh data. */
      set_custom_normals_for_verts = true;
    }
    else if (params.import_attributes) {
      /* If we have no faces, add vertex normals as custom attribute. */
      attributes.add<float3>(
          "normal",
          bke::AttrDomain::Point,
          bke::AttributeInitVArray(VArray<float3>::from_span(data.vertex_normals)));
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
                            bke::AttributeInitVArray(VArray<float>::from_span(attr.data)));
    }
  }

  /* It's important to validate the mesh before using it's geometry to calculate derived data. */
  {
    const bool allow_missing_edges = true;
#ifndef NDEBUG
    const bool verbose_validate = true;
#else
    const bool verbose_validate = false;
#endif
    bke::mesh_validate(*mesh, verbose_validate, allow_missing_edges);
  }

  if (set_custom_normals_for_verts) {
    bke::mesh_set_custom_normals_from_verts(*mesh, data.vertex_normals);
  }

  /* Merge all vertices on the same location. */
  if (params.merge_verts) {
    std::optional<Mesh *> merged_mesh = geometry::mesh_merge_by_distance_all(
        *mesh, IndexMask(mesh->verts_num), 0.0001f);
    if (merged_mesh) {
      BKE_id_free(nullptr, &mesh->id);
      mesh = *merged_mesh;
    }
  }

  return mesh;
}
}  // namespace io::ply
}  // namespace blender
