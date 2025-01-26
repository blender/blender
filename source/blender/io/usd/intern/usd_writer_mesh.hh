/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.hh"

#include "BLI_map.hh"

#include <pxr/usd/usdGeom/mesh.h>

struct SubsurfModifierData;

namespace blender::bke {
class AttributeIter;
}  // namespace blender::bke

namespace blender::io::usd {

struct USDMeshData;

/* Mapping from material slot number to array of face indices with that material. */
using MaterialFaceGroups = Map<short, pxr::VtArray<int>>;

/* Writer for USD geometry. Does not assume the object is a mesh object. */
class USDGenericMeshWriter : public USDAbstractWriter {
 public:
  USDGenericMeshWriter(const USDExporterContext &ctx);

 protected:
  bool is_supported(const HierarchyContext *context) const override;
  void do_write(HierarchyContext &context) override;

  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) = 0;
  virtual void free_export_mesh(Mesh *mesh);

 private:
  void write_mesh(HierarchyContext &context, Mesh *mesh, const SubsurfModifierData *subsurfData);
  pxr::TfToken get_subdiv_scheme(const SubsurfModifierData *subsurfData);
  void write_subdiv(const pxr::TfToken &subdiv_scheme,
                    const pxr::UsdGeomMesh &usd_mesh,
                    const SubsurfModifierData *subsurfData);
  void get_geometry_data(const Mesh *mesh, struct USDMeshData &usd_mesh_data);
  void assign_materials(const HierarchyContext &context,
                        const pxr::UsdGeomMesh &usd_mesh,
                        const MaterialFaceGroups &usd_face_groups);
  void write_normals(const Mesh *mesh, pxr::UsdGeomMesh &usd_mesh);
  void write_surface_velocity(const Mesh *mesh, const pxr::UsdGeomMesh &usd_mesh);

  void write_custom_data(const Object *obj, const Mesh *mesh, const pxr::UsdGeomMesh &usd_mesh);
  void write_generic_data(const Mesh *mesh,
                          const pxr::UsdGeomMesh &usd_mesh,
                          const bke::AttributeIter &attr);
  void write_uv_data(const pxr::UsdGeomMesh &usd_mesh,
                     const bke::AttributeIter &attr,
                     const StringRef active_uvmap_name);
};

class USDMeshWriter : public USDGenericMeshWriter {
  bool write_skinned_mesh_;
  bool write_blend_shapes_;

 public:
  USDMeshWriter(const USDExporterContext &ctx);

 protected:
  void do_write(HierarchyContext &context) override;

  Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;

  /**
   * Determine whether we should write skinned mesh or blend shape data
   * based on the export parameters and the modifiers enabled on the object.
   */
  void set_skel_export_flags(const HierarchyContext &context);

  void init_skinned_mesh(const HierarchyContext &context);
  void init_blend_shapes(const HierarchyContext &context);

  void add_shape_key_weights_sample(const Object *obj);
};

}  // namespace blender::io::usd
