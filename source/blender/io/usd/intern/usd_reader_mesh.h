/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Adapted from the Blender Alembic importer implementation.
 *
 * Modifications Copyright (C) 2021 Tangent Animation and
 * NVIDIA Corporation.  All rights reserved.
 */
#pragma once

#include "usd.h"
#include "usd_reader_geom.h"

#include "pxr/usd/usdGeom/mesh.h"

struct MPoly;

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
  bool has_uvs_;
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
                         double motionSampleTime,
                         int read_flag,
                         const char **err_str) override;

  bool topology_changed(Mesh *existing_mesh, double motionSampleTime) override;

 private:
  void process_normals_vertex_varying(Mesh *mesh);
  void process_normals_face_varying(Mesh *mesh);
  /** Set USD uniform (per-face) normals as Blender loop normals. */
  void process_normals_uniform(Mesh *mesh);
  void readFaceSetsSample(Main *bmain, Mesh *mesh, double motionSampleTime);
  void assign_facesets_to_mpoly(double motionSampleTime,
                                struct MPoly *mpoly,
                                int totpoly,
                                std::map<pxr::SdfPath, int> *r_mat_map);

  void read_mpolys(Mesh *mesh);
  void read_uvs(Mesh *mesh, double motionSampleTime, bool load_uvs = false);
  void read_colors(Mesh *mesh, double motionSampleTime);
  void read_vertex_creases(Mesh *mesh, double motionSampleTime);

  void read_mesh_sample(ImportSettings *settings,
                        Mesh *mesh,
                        double motionSampleTime,
                        bool new_mesh);
};

}  // namespace blender::io::usd
