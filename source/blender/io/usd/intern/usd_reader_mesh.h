/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Adapted from the Blender Alembic importer implementation.
 * Modifications Copyright 2021 Tangent Animation and. NVIDIA Corporation. All rights reserved. */
#pragma once

#include "BKE_attribute.hh"
#include "BLI_span.hh"

#include "usd.h"
#include "usd_reader_geom.h"

#include "pxr/usd/usdGeom/mesh.h"

namespace blender::io::usd {

class USDMeshReader : public USDGeomReader {
 private:
  pxr::UsdGeomMesh mesh_prim_;

  std::unordered_map<std::string, pxr::TfToken> uv_token_map_;
  std::map<const pxr::TfToken, bool> primvar_varying_map_;

  /* TODO(makowalski): Is it the best strategy to cache the
   * mesh geometry in the following members? It appears these
   * arrays are never cleared, so this might bloat memory. */
  pxr::VtIntArray face_indices_;
  pxr::VtIntArray face_counts_;
  pxr::VtVec3fArray positions_;
  pxr::VtVec3fArray normals_;

  pxr::TfToken normal_interpolation_;
  pxr::TfToken orientation_;
  bool is_left_handed_;
  bool is_time_varying_;

  /* This is to ensure we load all data once, because we reuse the read_mesh function
   * in the mesh seq modifier, and in initial load. Ideally, a better fix would be
   * implemented.  Note this will break if faces or positions vary. */
  bool is_initial_load_;

 public:
  USDMeshReader(const pxr::UsdPrim &prim,
                const USDImportParams &import_params,
                const ImportSettings &settings);

  bool valid() const override;

  void create_object(Main *bmain, double motionSampleTime) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;

  struct Mesh *read_mesh(struct Mesh *existing_mesh,
                         USDMeshReadParams params,
                         const char **err_str) override;

  bool topology_changed(const Mesh *existing_mesh, double motionSampleTime) override;

  /**
   * If the USD mesh prim has a valid UsdSkel schema defined, return the USD path
   * string to the bound skeleton, if any.  Returns the empty string if no skeleton
   * binding is defined.
   *
   * The returned path is currently used to match armature modifiers with armature
   * objects during import.
   */
  std::string get_skeleton_path() const;

 private:
  void process_normals_vertex_varying(Mesh *mesh);
  void process_normals_face_varying(Mesh *mesh);
  /** Set USD uniform (per-face) normals as Blender loop normals. */
  void process_normals_uniform(Mesh *mesh);
  void readFaceSetsSample(Main *bmain, Mesh *mesh, double motionSampleTime);
  void assign_facesets_to_material_indices(double motionSampleTime,
                                           MutableSpan<int> material_indices,
                                           std::map<pxr::SdfPath, int> *r_mat_map);

  void read_mpolys(Mesh *mesh);
  void read_vertex_creases(Mesh *mesh, double motionSampleTime);

  void read_mesh_sample(ImportSettings *settings,
                        Mesh *mesh,
                        double motionSampleTime,
                        bool new_mesh);

  void read_custom_data(const ImportSettings *settings,
                        Mesh *mesh,
                        double motionSampleTime,
                        bool new_mesh);

  void read_color_data_primvar(Mesh *mesh,
                               const pxr::UsdGeomPrimvar &color_primvar,
                               const double motionSampleTime);

  void read_uv_data_primvar(Mesh *mesh,
                            const pxr::UsdGeomPrimvar &primvar,
                            const double motionSampleTime);
  void read_generic_data_primvar(Mesh *mesh,
                                 const pxr::UsdGeomPrimvar &primvar,
                                 const double motionSampleTime);

  template<typename USDT, typename BlenderT>
  void copy_prim_array_to_blender_attribute(const Mesh *mesh,
                                            const pxr::UsdGeomPrimvar &primvar,
                                            const double motionSampleTime,
                                            MutableSpan<BlenderT> attribute);

  /**
   * Override transform computation to account for the binding
   * transformation for skinned meshes.
   */
  std::optional<XformResult> get_local_usd_xform(float time) const override;
};

}  // namespace blender::io::usd
