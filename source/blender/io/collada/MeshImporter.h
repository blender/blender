/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <map>
#include <vector>

#include "COLLADAFWIndexList.h"
#include "COLLADAFWInstanceGeometry.h"
#include "COLLADAFWMaterialBinding.h"
#include "COLLADAFWMesh.h"
#include "COLLADAFWMeshVertexData.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWPolygons.h"
#include "COLLADAFWTextureCoordinateBinding.h"
#include "COLLADAFWTypes.h"
#include "COLLADAFWUniqueId.h"

#include "ArmatureImporter.h"
#include "collada_utils.h"

#include "BLI_edgehash.h"
#include "BLI_math_vector_types.hh"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

/* only for ArmatureImporter to "see" MeshImporter::get_object_by_geom_uid */
class MeshImporterBase {
 public:
  virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId &geom_uid) = 0;
  virtual Mesh *get_mesh_by_geom_uid(const COLLADAFW::UniqueId &mesh_uid) = 0;
  virtual std::string *get_geometry_name(const std::string &mesh_name) = 0;
};

class UVDataWrapper {
  COLLADAFW::MeshVertexData *mVData;

 public:
  UVDataWrapper(COLLADAFW::MeshVertexData &vdata);

#ifdef COLLADA_DEBUG
  void print();
#endif

  void getUV(int uv_index, float *uv);
};

class VCOLDataWrapper {
  COLLADAFW::MeshVertexData *mVData;

 public:
  VCOLDataWrapper(COLLADAFW::MeshVertexData &vdata);
  void get_vcol(int v_index, MLoopCol *mloopcol);
};

class MeshImporter : public MeshImporterBase {
 private:
  UnitConverter *unitconverter;
  bool use_custom_normals;

  Main *m_bmain;
  Scene *scene;
  ViewLayer *view_layer;

  ArmatureImporter *armature_importer;

  std::map<std::string, std::string> mesh_geom_map;       /* needed for correct shape key naming */
  std::map<COLLADAFW::UniqueId, Mesh *> uid_mesh_map;     /* geometry unique id-to-mesh map */
  std::map<COLLADAFW::UniqueId, Object *> uid_object_map; /* geom UID-to-object */
  std::vector<Object *> imported_objects;                 /* list of imported objects */

  /* this structure is used to assign material indices to faces
   * it holds a portion of Mesh faces and corresponds to a DAE primitive list
   * (`<triangles>`, `<polylist>`, etc.) */
  struct Primitive {
    int face_index;
    int *material_indices;
    uint faces_num;
  };
  typedef std::map<COLLADAFW::MaterialId, std::vector<Primitive>> MaterialIdPrimitiveArrayMap;
  /* crazy name! */
  std::map<COLLADAFW::UniqueId, MaterialIdPrimitiveArrayMap> geom_uid_mat_mapping_map;
  /* < materials that have already been mapped to a geometry.
   * A pair/of geom UID and mat UID, one geometry can have several materials. */
  std::multimap<COLLADAFW::UniqueId, COLLADAFW::UniqueId> materials_mapped_to_geom;

  bool set_poly_indices(int *face_verts, int loop_index, const uint *indices, int loop_count);

  void set_face_uv(blender::float2 *mloopuv,
                   UVDataWrapper &uvs,
                   int start_index,
                   COLLADAFW::IndexList &index_list,
                   int count);

  void set_vcol(MLoopCol *mloopcol,
                VCOLDataWrapper &vob,
                int loop_index,
                COLLADAFW::IndexList &index_list,
                int count);

#ifdef COLLADA_DEBUG
  void print_index_list(COLLADAFW::IndexList &index_list);
#endif

  /**
   * Checks if mesh has supported primitive types:
   * `lines`, `polylist`, `triangles`, `triangle_fans`.
   */
  bool is_nice_mesh(COLLADAFW::Mesh *mesh);

  void read_vertices(COLLADAFW::Mesh *mesh, Mesh *me);

  /**
   * Condition 1: The Primitive has normals
   * condition 2: The number of normals equals the number of faces.
   * return true if both conditions apply.
   * return false otherwise.
   */
  bool primitive_has_useable_normals(COLLADAFW::MeshPrimitive *mp);
  /**
   * Assume that only TRIANGLES, TRIANGLE_FANS, POLYLIST and POLYGONS
   * have faces. (to be verified).
   */
  bool primitive_has_faces(COLLADAFW::MeshPrimitive *mp);

  /**
   * This function is copied from `source/blender/editors/mesh/mesh_data.cc`
   *
   * TODO: (As discussed with sergey-) :
   * Maybe move this function to `blenderkernel/intern/mesh.cc`.
   * and add definition to BKE_mesh.c.
   */
  static void mesh_add_edges(Mesh *mesh, int len);

  uint get_loose_edge_count(COLLADAFW::Mesh *mesh);

  /**
   * Return the number of faces by summing up
   * the face-counts of the parts.
   * HINT: This is done because `mesh->getFacesCount()` does
   * count loose edges as extra faces, which is not what we want here.
   */
  void allocate_poly_data(COLLADAFW::Mesh *collada_mesh, Mesh *me);

  /* TODO: import uv set names */
  /**
   * Read all faces from TRIANGLES, TRIANGLE_FANS, POLYLIST, POLYGON
   * IMPORTANT: This function MUST be called before read_lines()
   * Otherwise we will lose all edges from faces (see read_lines() above)
   *
   * TODO: import uv set names.
   */
  void read_polys(COLLADAFW::Mesh *mesh, Mesh *me, blender::Vector<blender::float3> &loop_normals);
  /**
   * Read all loose edges.
   * IMPORTANT: This function assumes that all edges from existing
   * faces have already been generated and added to me->medge
   * So this function MUST be called after read_faces() (see below)
   */
  void read_lines(COLLADAFW::Mesh *mesh, Mesh *me);
  uint get_vertex_count(COLLADAFW::Polygons *mp, int index);

  void get_vector(float v[3], COLLADAFW::MeshVertexData &arr, int i, int stride);

  bool is_flat_face(uint *nind, COLLADAFW::MeshVertexData &nor, int count);

  /**
   * Returns the list of Users of the given Mesh object.
   * NOTE: This function uses the object user flag to control
   * which objects have already been processed.
   */
  std::vector<Object *> get_all_users_of(Mesh *reference_mesh);

 public:
  MeshImporter(UnitConverter *unitconv,
               bool use_custom_normals,
               ArmatureImporter *arm,
               Main *bmain,
               Scene *sce,
               ViewLayer *view_layer);

  virtual Object *get_object_by_geom_uid(const COLLADAFW::UniqueId &geom_uid);

  virtual Mesh *get_mesh_by_geom_uid(const COLLADAFW::UniqueId &geom_uid);

  /**
   *
   * During import all materials have been assigned to Object.
   * Now we iterate over the imported objects and optimize
   * the assignments as follows:
   *
   * for each imported geometry:
   *     if number of users is 1:
   *         get the user (object)
   *         move the materials from Object to Data
   *     else:
   *         determine which materials are assigned to the first user
   *         check if all other users have the same materials in the same order
   *         if the check is positive:
   *             Add the materials of the first user to the geometry
   *             adjust all other users accordingly.
   */
  void optimize_material_assignements();

  /**
   * We do not know in advance which objects will share geometries.
   * And we do not know either if the objects which share geometries
   * come along with different materials. So we first create the objects
   * and assign the materials to Object, then in a later cleanup we decide
   * which materials shall be moved to the created geometries. Also see
   * optimize_material_assignements() above.
   */
  void assign_material_to_geom(COLLADAFW::MaterialBinding cmaterial,
                               std::map<COLLADAFW::UniqueId, Material *> &uid_material_map,
                               Object *ob,
                               const COLLADAFW::UniqueId *geom_uid,
                               short mat_index);

  Object *create_mesh_object(COLLADAFW::Node *node,
                             COLLADAFW::InstanceGeometry *geom,
                             bool isController,
                             std::map<COLLADAFW::UniqueId, Material *> &uid_material_map);

  /** Create a mesh storing a pointer in a map so it can be retrieved later by geometry UID. */
  bool write_geometry(const COLLADAFW::Geometry *geom);
  std::string *get_geometry_name(const std::string &mesh_name);
};
