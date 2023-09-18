/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd_writer_abstract.h"

#include "BLI_map.hh"

#include "BKE_attribute.hh"

#include <pxr/usd/usdGeom/mesh.h>

struct ModifierData;

namespace blender::io::usd {

struct USDMeshData;

/* Writer for USD geometry. Does not assume the object is a mesh object. */
class USDGenericMeshWriter : public USDAbstractWriter {
 public:
  USDGenericMeshWriter(const USDExporterContext &ctx);

 protected:
  virtual bool is_supported(const HierarchyContext *context) const override;
  virtual void do_write(HierarchyContext &context) override;

  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) = 0;
  virtual void free_export_mesh(Mesh *mesh);

  /* Get time code for writing mesh properties.
   * The default implementation is equivalent to
   * calling USDAbstractWriter::get_export_time_code(). */
  virtual pxr::UsdTimeCode get_mesh_export_time_code() const;

 private:
  /* Mapping from material slot number to array of face indices with that material. */
  using MaterialFaceGroups = Map<short, pxr::VtIntArray>;

  void write_mesh(HierarchyContext &context, Mesh *mesh);
  void get_geometry_data(const Mesh *mesh, struct USDMeshData &usd_mesh_data);
  void assign_materials(const HierarchyContext &context,
                        pxr::UsdGeomMesh usd_mesh,
                        const MaterialFaceGroups &usd_face_groups);

  void write_normals(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh);
  void write_surface_velocity(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh);

  void write_custom_data(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh);
  void write_generic_data(const Mesh *mesh,
                          pxr::UsdGeomMesh usd_mesh,
                          const bke::AttributeIDRef &attribute_id,
                          const bke::AttributeMetaData &meta_data);
  void write_uv_data(const Mesh *mesh,
                     pxr::UsdGeomMesh usd_mesh,
                     const bke::AttributeIDRef &attribute_id,
                     const char *active_set_name);
  void write_color_data(const Mesh *mesh,
                        pxr::UsdGeomMesh usd_mesh,
                        const bke::AttributeIDRef &attribute_id,
                        const bke::AttributeMetaData &meta_data);

  ModifierData *m_subsurf_mod;

  template<typename BlenderT, typename USDT>
  void copy_blender_buffer_to_prim(const Span<BlenderT> buffer,
                                   const pxr::UsdTimeCode timecode,
                                   pxr::UsdGeomPrimvar attribute_pv);
};

class USDMeshWriter : public USDGenericMeshWriter {
 public:
  USDMeshWriter(const USDExporterContext &ctx);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
};

}  // namespace blender::io::usd
