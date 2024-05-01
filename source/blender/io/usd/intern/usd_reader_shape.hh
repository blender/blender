/* SPDX-FileCopyrightText: 2023 Nvidia. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "usd.hh"
#include "usd_reader_geom.hh"

struct Mesh;

namespace blender::io::usd {

/*
 * Read USDGeom primitive shapes as Blender Meshes.  This class uses the same adapter functions
 * as the GL viewport to generate geometry for each of the supported types.
 */
class USDShapeReader : public USDGeomReader {
  /* A cache to record whether a given primvar is time-varying, so that static primvars are not
   * read more than once when the mesh is evaluated for animation by the cache file modifier.
   * The map is mutable so that it can be updated in const functions. */
  mutable blender::Map<const pxr::TfToken, bool> primvar_time_varying_map_;

 private:
  /* Template required to read mesh information out of Shape prims,
   * as each prim type has a separate subclass. */
  template<typename Adapter>
  void read_values(double motionSampleTime,
                   pxr::VtVec3fArray &positions,
                   pxr::VtIntArray &face_indices,
                   pxr::VtIntArray &face_counts) const;

  /* Wrapper for the templated method read_values, calling the correct template
   * instantiation based on the introspected prim type. */
  bool read_mesh_values(double motionSampleTime,
                        pxr::VtVec3fArray &positions,
                        pxr::VtIntArray &face_indices,
                        pxr::VtIntArray &face_counts) const;

  void apply_primvars_to_mesh(Mesh *mesh, double motionSampleTime) const;

  /* Read the pxr:UsdGeomMesh values and convert them to a Blender Mesh,
   * also returning face_indices and counts for further loop processing. */
  Mesh *mesh_from_prim(Mesh *existing_mesh,
                       USDMeshReadParams params,
                       pxr::VtIntArray &face_indices,
                       pxr::VtIntArray &face_counts) const;

  Mesh *read_mesh(Mesh *existing_mesh, USDMeshReadParams params, const char ** /*err_str*/);

 public:
  USDShapeReader(const pxr::UsdPrim &prim,
                 const USDImportParams &import_params,
                 const ImportSettings &settings);

  void create_object(Main *bmain, double /*motionSampleTime*/) override;
  void read_object_data(Main *bmain, double motionSampleTime) override;
  void read_geometry(bke::GeometrySet & /*geometry_set*/,
                     USDMeshReadParams /*params*/,
                     const char ** /*err_str*/) override;

  /* Returns the generated mesh might be affected by time-varying attributes.
   * This assumes mesh_from_prim() has been called.  */
  bool is_time_varying();

  virtual bool topology_changed(const Mesh * /*existing_mesh*/,
                                double /*motionSampleTime*/) override
  {
    return false;
  };
};

}  // namespace blender::io::usd
