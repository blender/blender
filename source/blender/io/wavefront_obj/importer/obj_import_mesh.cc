/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_node_tree_update.h"
#include "BKE_object.h"
#include "BKE_object_deform.h"

#include "BLI_math_vector.h"
#include "BLI_set.hh"

#include "importer_mesh_utils.hh"
#include "obj_import_mesh.hh"

namespace blender::io::obj {

Object *MeshFromGeometry::create_mesh(
    Main *bmain,
    const Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
    Map<std::string, Material *> &created_materials,
    const OBJImportParams &import_params)
{
  std::string ob_name{mesh_geometry_.geometry_name_};
  if (ob_name.empty()) {
    ob_name = "Untitled";
  }
  fixup_invalid_faces();

  const int64_t tot_verts_object{mesh_geometry_.vertex_indices_.size()};
  /* Total explicitly imported edges, not the ones belonging the polygons to be created. */
  const int64_t tot_edges{mesh_geometry_.edges_.size()};
  const int64_t tot_face_elems{mesh_geometry_.face_elements_.size()};
  const int64_t tot_loops{mesh_geometry_.total_loops_};

  Mesh *mesh = BKE_mesh_new_nomain(tot_verts_object, tot_edges, 0, tot_loops, tot_face_elems);
  Object *obj = BKE_object_add_only_object(bmain, OB_MESH, ob_name.c_str());
  obj->data = BKE_object_obdata_add_from_type(bmain, OB_MESH, ob_name.c_str());

  create_vertices(mesh);
  create_polys_loops(obj, mesh);
  create_edges(mesh);
  create_uv_verts(mesh);
  create_normals(mesh);
  create_materials(bmain, materials, created_materials, obj);

  bool verbose_validate = false;
#ifdef DEBUG
  verbose_validate = true;
#endif
  BKE_mesh_validate(mesh, verbose_validate, false);
  transform_object(obj, import_params);

  /* FIXME: after 2.80; `mesh->flag` isn't copied by #BKE_mesh_nomain_to_mesh() */
  const short autosmooth = (mesh->flag & ME_AUTOSMOOTH);
  Mesh *dst = static_cast<Mesh *>(obj->data);
  BKE_mesh_nomain_to_mesh(mesh, dst, obj, &CD_MASK_EVERYTHING, true);
  dst->flag |= autosmooth;

  return obj;
}

void MeshFromGeometry::fixup_invalid_faces()
{
  for (int64_t face_idx = 0; face_idx < mesh_geometry_.face_elements_.size(); ++face_idx) {
    const PolyElem &curr_face = mesh_geometry_.face_elements_[face_idx];

    if (curr_face.face_corners.size() < 3) {
      /* Skip and remove faces that have fewer than 3 corners. */
      mesh_geometry_.total_loops_ -= curr_face.face_corners.size();
      mesh_geometry_.face_elements_.remove_and_reorder(face_idx);
      continue;
    }

    /* Check if face is invalid for Blender conventions:
     * basically whether it has duplicate vertex indices. */
    bool valid = true;
    Set<int, 8> used_verts;
    for (const PolyCorner &corner : curr_face.face_corners) {
      if (used_verts.contains(corner.vert_index)) {
        valid = false;
        break;
      }
      used_verts.add(corner.vert_index);
    }
    if (valid) {
      continue;
    }

    /* We have an invalid face, have to turn it into possibly
     * multiple valid faces. */
    Vector<int, 8> face_verts;
    Vector<int, 8> face_uvs;
    Vector<int, 8> face_normals;
    face_verts.reserve(curr_face.face_corners.size());
    face_uvs.reserve(curr_face.face_corners.size());
    face_normals.reserve(curr_face.face_corners.size());
    for (const PolyCorner &corner : curr_face.face_corners) {
      face_verts.append(corner.vert_index);
      face_normals.append(corner.vertex_normal_index);
      face_uvs.append(corner.uv_vert_index);
    }
    std::string face_vertex_group = curr_face.vertex_group;
    std::string face_material_name = curr_face.material_name;
    bool face_shaded_smooth = curr_face.shaded_smooth;

    /* Remove the invalid face. */
    mesh_geometry_.total_loops_ -= curr_face.face_corners.size();
    mesh_geometry_.face_elements_.remove_and_reorder(face_idx);

    Vector<Vector<int>> new_faces = fixup_invalid_polygon(global_vertices_.vertices, face_verts);

    /* Create the newly formed faces. */
    for (Span<int> face : new_faces) {
      if (face.size() < 3) {
        continue;
      }
      PolyElem new_face{};
      new_face.vertex_group = face_vertex_group;
      new_face.material_name = face_material_name;
      new_face.shaded_smooth = face_shaded_smooth;
      new_face.face_corners.reserve(face.size());
      for (int idx : face) {
        BLI_assert(idx >= 0 && idx < face_verts.size());
        new_face.face_corners.append({face_verts[idx], face_uvs[idx], face_normals[idx]});
      }
      mesh_geometry_.face_elements_.append(new_face);
      mesh_geometry_.total_loops_ += face.size();
    }
  }
}

void MeshFromGeometry::create_vertices(Mesh *mesh)
{
  const int64_t tot_verts_object{mesh_geometry_.vertex_indices_.size()};
  for (int i = 0; i < tot_verts_object; ++i) {
    if (mesh_geometry_.vertex_indices_[i] < global_vertices_.vertices.size()) {
      copy_v3_v3(mesh->mvert[i].co, global_vertices_.vertices[mesh_geometry_.vertex_indices_[i]]);
    }
    else {
      std::cerr << "Vertex index:" << mesh_geometry_.vertex_indices_[i]
                << " larger than total vertices:" << global_vertices_.vertices.size() << " ."
                << std::endl;
    }
  }
}

void MeshFromGeometry::create_polys_loops(Object *obj, Mesh *mesh)
{
  /* Will not be used if vertex groups are not imported. */
  mesh->dvert = nullptr;
  float weight = 0.0f;
  const int64_t total_verts = mesh_geometry_.vertex_indices_.size();
  if (total_verts && mesh_geometry_.use_vertex_groups_) {
    mesh->dvert = static_cast<MDeformVert *>(
        CustomData_add_layer(&mesh->vdata, CD_MDEFORMVERT, CD_CALLOC, nullptr, total_verts));
    weight = 1.0f / total_verts;
  }
  else {
    UNUSED_VARS(weight);
  }

  /* Do not remove elements from the VectorSet since order of insertion is required.
   * StringRef is fine since per-face deform group name outlives the VectorSet. */
  VectorSet<StringRef> group_names;
  const int64_t tot_face_elems{mesh->totpoly};
  int tot_loop_idx = 0;

  for (int poly_idx = 0; poly_idx < tot_face_elems; ++poly_idx) {
    const PolyElem &curr_face = mesh_geometry_.face_elements_[poly_idx];
    if (curr_face.face_corners.size() < 3) {
      /* Don't add single vertex face, or edges. */
      std::cerr << "Face with less than 3 vertices found, skipping." << std::endl;
      continue;
    }

    MPoly &mpoly = mesh->mpoly[poly_idx];
    mpoly.totloop = curr_face.face_corners.size();
    mpoly.loopstart = tot_loop_idx;
    if (curr_face.shaded_smooth) {
      mpoly.flag |= ME_SMOOTH;
    }
    mpoly.mat_nr = mesh_geometry_.material_names_.index_of_try(curr_face.material_name);
    /* Importing obj files without any materials would result in negative indices, which is not
     * supported. */
    if (mpoly.mat_nr < 0) {
      mpoly.mat_nr = 0;
    }

    for (const PolyCorner &curr_corner : curr_face.face_corners) {
      MLoop &mloop = mesh->mloop[tot_loop_idx];
      tot_loop_idx++;
      mloop.v = curr_corner.vert_index;

      if (!mesh->dvert) {
        continue;
      }
      /* Iterating over mloop results in finding the same vertex multiple times.
       * Another way is to allocate memory for dvert while creating vertices and fill them here.
       */
      MDeformVert &def_vert = mesh->dvert[mloop.v];
      if (!def_vert.dw) {
        def_vert.dw = static_cast<MDeformWeight *>(
            MEM_callocN(sizeof(MDeformWeight), "OBJ Import Deform Weight"));
      }
      /* Every vertex in a face is assigned the same deform group. */
      int64_t pos_name{group_names.index_of_try(curr_face.vertex_group)};
      if (pos_name == -1) {
        group_names.add_new(curr_face.vertex_group);
        pos_name = group_names.size() - 1;
      }
      BLI_assert(pos_name >= 0);
      /* Deform group number (def_nr) must behave like an index into the names' list. */
      *(def_vert.dw) = {static_cast<unsigned int>(pos_name), weight};
    }
  }

  if (!mesh->dvert) {
    return;
  }
  /* Add deform group(s) to the object's defbase. */
  for (StringRef name : group_names) {
    /* Adding groups in this order assumes that def_nr is an index into the names' list. */
    BKE_object_defgroup_add_name(obj, name.data());
  }
}

void MeshFromGeometry::create_edges(Mesh *mesh)
{
  const int64_t tot_edges{mesh_geometry_.edges_.size()};
  const int64_t total_verts{mesh_geometry_.vertex_indices_.size()};
  UNUSED_VARS_NDEBUG(total_verts);
  for (int i = 0; i < tot_edges; ++i) {
    const MEdge &src_edge = mesh_geometry_.edges_[i];
    MEdge &dst_edge = mesh->medge[i];
    BLI_assert(src_edge.v1 < total_verts && src_edge.v2 < total_verts);
    dst_edge.v1 = src_edge.v1;
    dst_edge.v2 = src_edge.v2;
    dst_edge.flag = ME_LOOSEEDGE;
  }

  /* Set argument `update` to true so that existing, explicitly imported edges can be merged
   * with the new ones created from polygons. */
  BKE_mesh_calc_edges(mesh, true, false);
  BKE_mesh_calc_edges_loose(mesh);
}

void MeshFromGeometry::create_uv_verts(Mesh *mesh)
{
  if (global_vertices_.uv_vertices.size() <= 0) {
    return;
  }
  MLoopUV *mluv_dst = static_cast<MLoopUV *>(CustomData_add_layer(
      &mesh->ldata, CD_MLOOPUV, CD_DEFAULT, nullptr, mesh_geometry_.total_loops_));
  int tot_loop_idx = 0;

  for (const PolyElem &curr_face : mesh_geometry_.face_elements_) {
    for (const PolyCorner &curr_corner : curr_face.face_corners) {
      if (curr_corner.uv_vert_index >= 0 &&
          curr_corner.uv_vert_index < global_vertices_.uv_vertices.size()) {
        const float2 &mluv_src = global_vertices_.uv_vertices[curr_corner.uv_vert_index];
        copy_v2_v2(mluv_dst[tot_loop_idx].uv, mluv_src);
        tot_loop_idx++;
      }
    }
  }
}

static Material *get_or_create_material(
    Main *bmain,
    const std::string &name,
    const Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
    Map<std::string, Material *> &created_materials)
{
  /* Have we created this material already? */
  Material **found_mat = created_materials.lookup_ptr(name);
  if (found_mat != nullptr) {
    return *found_mat;
  }

  /* We have not, will have to create it. */
  if (!materials.contains(name)) {
    std::cerr << "Material named '" << name << "' not found in material library." << std::endl;
    return nullptr;
  }

  Material *mat = BKE_material_add(bmain, name.c_str());
  const MTLMaterial &mtl = *materials.lookup(name);
  ShaderNodetreeWrap mat_wrap{bmain, mtl, mat};

  /* Viewport shading uses legacy r,g,b material values. */
  if (mtl.Kd[0] >= 0 && mtl.Kd[1] >= 0 && mtl.Kd[2] >= 0) {
    mat->r = mtl.Kd[0];
    mat->g = mtl.Kd[1];
    mat->b = mtl.Kd[2];
  }

  mat->use_nodes = true;
  mat->nodetree = mat_wrap.get_nodetree();
  BKE_ntree_update_main_tree(bmain, mat->nodetree, nullptr);

  created_materials.add_new(name, mat);
  return mat;
}

void MeshFromGeometry::create_materials(
    Main *bmain,
    const Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
    Map<std::string, Material *> &created_materials,
    Object *obj)
{
  for (const std::string &name : mesh_geometry_.material_names_) {
    Material *mat = get_or_create_material(bmain, name, materials, created_materials);
    if (mat == nullptr) {
      continue;
    }
    BKE_object_material_slot_add(bmain, obj);
    BKE_object_material_assign(bmain, obj, mat, obj->totcol, BKE_MAT_ASSIGN_USERPREF);
  }
}

void MeshFromGeometry::create_normals(Mesh *mesh)
{
  /* NOTE: Needs more clarity about what is expected in the viewport if the function works. */

  /* No normal data: nothing to do. */
  if (global_vertices_.vertex_normals.is_empty() || !mesh_geometry_.has_vertex_normals_) {
    return;
  }

  float(*loop_normals)[3] = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(mesh_geometry_.total_loops_, sizeof(float[3]), __func__));
  int tot_loop_idx = 0;
  for (const PolyElem &curr_face : mesh_geometry_.face_elements_) {
    for (const PolyCorner &curr_corner : curr_face.face_corners) {
      int n_index = curr_corner.vertex_normal_index;
      float3 normal(0, 0, 0);
      if (n_index >= 0) {
        normal = global_vertices_.vertex_normals[n_index];
      }
      copy_v3_v3(loop_normals[tot_loop_idx], normal);
      tot_loop_idx++;
    }
  }
  mesh->flag |= ME_AUTOSMOOTH;
  BKE_mesh_set_custom_normals(mesh, loop_normals);
  MEM_freeN(loop_normals);
}

}  // namespace blender::io::obj
