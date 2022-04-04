/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BKE_lib_id.h"

#include "BLI_utility_mixins.hh"

#include "obj_import_mtl.hh"
#include "obj_import_objects.hh"

struct Material;

namespace blender::io::obj {

/**
 * Make a Blender Mesh Object from a Geometry of GEOM_MESH type.
 */
class MeshFromGeometry : NonMovable, NonCopyable {
 private:
  Geometry &mesh_geometry_;
  const GlobalVertices &global_vertices_;

 public:
  MeshFromGeometry(Geometry &mesh_geometry, const GlobalVertices &global_vertices)
      : mesh_geometry_(mesh_geometry), global_vertices_(global_vertices)
  {
  }

  Object *create_mesh(Main *bmain,
                      const Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                      Map<std::string, Material *> &created_materials,
                      const OBJImportParams &import_params);

 private:
  void fixup_invalid_faces();
  void create_vertices(Mesh *mesh);
  void create_polys_loops(Object *obj, Mesh *mesh);
  void create_edges(Mesh *mesh);
  void create_uv_verts(Mesh *mesh);
  void create_materials(Main *bmain,
                        const Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                        Map<std::string, Material *> &created_materials,
                        Object *obj);
  void create_normals(Mesh *mesh);
};

}  // namespace blender::io::obj
