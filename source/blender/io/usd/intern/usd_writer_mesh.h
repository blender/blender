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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#ifndef __USD_WRITER_MESH_H__
#define __USD_WRITER_MESH_H__

#include "usd_writer_abstract.h"

#include <pxr/usd/usdGeom/mesh.h>

namespace blender {
namespace io {
namespace usd {

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

 private:
  /* Mapping from material slot number to array of face indices with that material. */
  typedef std::map<short, pxr::VtIntArray> MaterialFaceGroups;

  void write_mesh(HierarchyContext &context, Mesh *mesh);
  void get_geometry_data(const Mesh *mesh, struct USDMeshData &usd_mesh_data);
  void assign_materials(const HierarchyContext &context,
                        pxr::UsdGeomMesh usd_mesh,
                        const MaterialFaceGroups &usd_face_groups);
  void write_uv_maps(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh);
  void write_normals(const Mesh *mesh, pxr::UsdGeomMesh usd_mesh);
  void write_surface_velocity(Object *object, const Mesh *mesh, pxr::UsdGeomMesh usd_mesh);
};

class USDMeshWriter : public USDGenericMeshWriter {
 public:
  USDMeshWriter(const USDExporterContext &ctx);

 protected:
  virtual Mesh *get_export_mesh(Object *object_eval, bool &r_needsfree) override;
};

}  // namespace usd
}  // namespace io
}  // namespace blender

#endif /* __USD_WRITER_MESH_H__ */
