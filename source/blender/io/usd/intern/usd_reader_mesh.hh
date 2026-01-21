/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation.
 * Modifications Copyright 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved. */
#pragma once

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "usd.hh"
#include "usd_api_modifier.hh"
#include "usd_reader_geom.hh"

#include <pxr/usd/usdGeom/mesh.h>

namespace blender::io::usd {

struct USDMeshReadData {
 private:
  pxr::VtVec3fArray positions_;
  pxr::VtVec3fArray normals_;
  pxr::VtIntArray face_indices_;
  pxr::VtIntArray face_counts_;

 public:
  USDMeshReadData(const pxr::UsdGeomMesh &mesh_prim, pxr::UsdTimeCode time);

  Span<int> face_indices() const
  {
    return Span(face_indices_.cdata(), face_indices_.size());
  }
  Span<int> face_counts() const
  {
    return Span(face_counts_.cdata(), face_counts_.size());
  }
  Span<float3> positions() const
  {
    return Span(positions_.cdata(), positions_.size()).cast<float3>();
  }
  Span<float3> normals() const
  {
    return Span(normals_.cdata(), normals_.size()).cast<float3>();
  }
  MutableSpan<float3> normals_for_write()
  {
    return MutableSpan(normals_.data(), normals_.size()).cast<float3>();
  }

  pxr::TfToken normal_interpolation;
  pxr::TfToken orientation;
};

class USDMeshReader : public USDGeomReader {
 private:
  pxr::UsdGeomMesh mesh_prim_;

  bool is_left_handed_ = false;
  bool is_time_varying_ = false;

  /* This is to ensure we load all data once, because we reuse the read_mesh function
   * in the mesh seq modifier, and in initial load. Ideally, a better fix would be
   * implemented.  Note this will break if faces or positions vary. */
  bool is_initial_load_ = false;

  Map<const pxr::TfToken, bool> primvar_varying_map_;

 public:
  USDMeshReader(const pxr::UsdPrim &prim,
                const USDImportParams &import_params,
                const ImportSettings &settings)
      : USDGeomReader(prim, import_params, settings), mesh_prim_(prim)
  {
  }

  bool valid() const override
  {
    return bool(mesh_prim_);
  }

  void create_object(Main *bmain) override;
  void read_object_data(Main *bmain, pxr::UsdTimeCode time) override;

  void read_geometry(bke::GeometrySet &geometry_set,
                     USDMeshReadParams params,
                     const char **r_err_str) override;

  bool topology_changed(const Mesh *existing_mesh, pxr::UsdTimeCode time) override;

  /**
   * If the USD mesh prim has a valid `UsdSkel` schema defined, return the USD path
   * string to the bound skeleton, if any. Returns the empty string if no skeleton
   * binding is defined.
   *
   * The returned path is currently used to match armature modifiers with armature
   * objects during import.
   */
  pxr::SdfPath get_skeleton_path() const;

 private:
  void process_normals_vertex_varying(Mesh *mesh, MutableSpan<float3> usd_normals) const;
  void process_normals_face_varying(Mesh *mesh, Span<float3> usd_normals) const;
  /** Set USD uniform (per-face) normals as Blender loop normals. */
  void process_normals_uniform(Mesh *mesh, Span<float3> usd_normals) const;
  void readFaceSetsSample(Main *bmain, Mesh *mesh, pxr::UsdTimeCode time);
  void assign_facesets_to_material_indices(pxr::UsdTimeCode time,
                                           MutableSpan<int> material_indices,
                                           Map<pxr::SdfPath, int> *r_mat_map);

  bool read_faces(Mesh *mesh, const USDMeshReadData &usd_data) const;
  void read_subdiv();
  void read_vertex_creases(Mesh *mesh, pxr::UsdTimeCode time);
  void read_edge_creases(Mesh *mesh, pxr::UsdTimeCode time);
  void read_velocities(Mesh *mesh, pxr::UsdTimeCode time);

  void read_mesh_sample(ImportSettings *settings,
                        Mesh *mesh,
                        USDMeshReadData &usd_data,
                        pxr::UsdTimeCode time,
                        bool new_mesh);

  Mesh *read_mesh(struct Mesh *existing_mesh,
                  const USDMeshReadParams params,
                  const char **r_err_str);

  void read_custom_data(const ImportSettings *settings,
                        Mesh *mesh,
                        pxr::UsdTimeCode time,
                        bool new_mesh);

  void read_uv_data_primvar(Mesh *mesh,
                            const pxr::UsdGeomPrimvar &primvar,
                            const pxr::UsdTimeCode time);

  bool topology_changed(const Mesh *existing_mesh, const USDMeshReadData &usd_data) const;

  /**
   * Override transform computation to account for the binding
   * transformation for skinned meshes.
   */
  std::optional<XformResult> get_local_usd_xform(pxr::UsdTimeCode time) const override;
};

}  // namespace blender::io::usd
