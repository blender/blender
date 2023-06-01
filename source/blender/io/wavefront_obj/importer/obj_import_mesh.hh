/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
                      Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                      Map<std::string, Material *> &created_materials,
                      const OBJImportParams &import_params);

 private:
  /**
   * OBJ files coming from the wild might have faces that are invalid in Blender
   * (mostly with duplicate vertex indices, used by some software to indicate
   * polygons with holes). This method tries to fix them up.
   */
  void fixup_invalid_faces();
  void create_vertices(Mesh *mesh);
  /**
   * Create polygons for the Mesh, set smooth shading flags, Materials.
   */
  void create_polys_loops(Mesh *mesh, bool use_vertex_groups);
  /**
   * Add explicitly imported OBJ edges to the mesh.
   */
  void create_edges(Mesh *mesh);
  /**
   * Add UV layer and vertices to the Mesh.
   */
  void create_uv_verts(Mesh *mesh);
  /**
   * Add materials and the node-tree to the Mesh Object.
   */
  void create_materials(Main *bmain,
                        Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                        Map<std::string, Material *> &created_materials,
                        Object *obj,
                        bool relative_paths);
  void create_normals(Mesh *mesh);
  void create_colors(Mesh *mesh);
  void create_vertex_groups(Object *obj);
};

}  // namespace blender::io::obj
