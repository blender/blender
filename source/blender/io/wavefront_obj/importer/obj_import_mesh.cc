/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include <algorithm>

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_deform.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"

#include "BLI_math_vector.h"
#include "BLI_set.hh"

#include "IO_wavefront_obj.hh"
#include "importer_mesh_utils.hh"
#include "obj_export_mtl.hh"
#include "obj_import_mesh.hh"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.obj"};

namespace blender::io::obj {

Mesh *MeshFromGeometry::create_mesh(const OBJImportParams &import_params)
{
  const int64_t tot_verts_object{mesh_geometry_.get_vertex_count()};
  if (tot_verts_object <= 0) {
    /* Empty mesh */
    return nullptr;
  }

  this->fixup_invalid_faces();

  /* Includes explicitly imported edges, not the ones belonging the faces to be created. */
  Mesh *mesh = BKE_mesh_new_nomain(tot_verts_object,
                                   mesh_geometry_.edges_.size(),
                                   mesh_geometry_.face_elements_.size(),
                                   mesh_geometry_.total_corner_);

  this->create_vertices(mesh);
  this->create_faces(mesh, import_params.import_vertex_groups && !import_params.use_split_groups);
  this->create_edges(mesh);
  this->create_uv_verts(mesh);
  this->create_normals(mesh);
  this->create_colors(mesh);

  if (import_params.validate_meshes || mesh_geometry_.has_invalid_faces_) {
    bool verbose_validate = false;
#ifndef NDEBUG
    verbose_validate = true;
#endif
    BKE_mesh_validate(mesh, verbose_validate, false);
  }

  return mesh;
}

Object *MeshFromGeometry::create_mesh_object(
    Main *bmain,
    Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
    Map<std::string, Material *> &created_materials,
    const OBJImportParams &import_params)
{
  Mesh *mesh = this->create_mesh(import_params);

  if (mesh == nullptr) {
    return nullptr;
  }

  std::string ob_name = get_geometry_name(mesh_geometry_.geometry_name_,
                                          import_params.collection_separator);
  if (ob_name.empty()) {
    ob_name = "Untitled";
  }

  Object *obj = BKE_object_add_only_object(bmain, OB_MESH, ob_name.c_str());
  obj->data = BKE_object_obdata_add_from_type(bmain, OB_MESH, ob_name.c_str());

  this->create_materials(bmain,
                         materials,
                         created_materials,
                         obj,
                         import_params.relative_paths,
                         import_params.mtl_name_collision_mode);

  BKE_mesh_nomain_to_mesh(mesh, static_cast<Mesh *>(obj->data), obj);

  transform_object(obj, import_params);

  /* NOTE: vertex groups have to be created after final mesh is assigned to the object. */
  this->create_vertex_groups(obj);

  return obj;
}

void MeshFromGeometry::fixup_invalid_faces()
{
  for (int64_t face_idx = 0; face_idx < mesh_geometry_.face_elements_.size(); ++face_idx) {
    const FaceElem &curr_face = mesh_geometry_.face_elements_[face_idx];

    if (curr_face.corner_count_ < 3) {
      /* Skip and remove faces that have fewer than 3 corners. */
      mesh_geometry_.total_corner_ -= curr_face.corner_count_;
      mesh_geometry_.face_elements_.remove_and_reorder(face_idx);
      --face_idx;
      continue;
    }

    /* Check if face is invalid for Blender conventions:
     * basically whether it has duplicate vertex indices. */
    bool valid = true;
    Set<int, 8> used_verts;
    for (int i = 0; i < curr_face.corner_count_; ++i) {
      int corner_idx = curr_face.start_index_ + i;
      int vertex_idx = mesh_geometry_.face_corners_[corner_idx].vert_index;
      if (used_verts.contains(vertex_idx)) {
        valid = false;
        break;
      }
      used_verts.add(vertex_idx);
    }
    if (valid) {
      continue;
    }

    /* We have an invalid face, have to turn it into possibly
     * multiple valid faces. */
    Vector<int, 8> face_verts;
    Vector<int, 8> face_uvs;
    Vector<int, 8> face_normals;
    face_verts.reserve(curr_face.corner_count_);
    face_uvs.reserve(curr_face.corner_count_);
    face_normals.reserve(curr_face.corner_count_);
    for (int i = 0; i < curr_face.corner_count_; ++i) {
      int corner_idx = curr_face.start_index_ + i;
      const FaceCorner &corner = mesh_geometry_.face_corners_[corner_idx];
      face_verts.append(corner.vert_index);
      face_normals.append(corner.vertex_normal_index);
      face_uvs.append(corner.uv_vert_index);
    }
    int face_vertex_group = curr_face.vertex_group_index;
    int face_material = curr_face.material_index;
    bool face_shaded_smooth = curr_face.shaded_smooth;

    /* Remove the invalid face. */
    mesh_geometry_.total_corner_ -= curr_face.corner_count_;
    mesh_geometry_.face_elements_.remove_and_reorder(face_idx);
    --face_idx;

    Vector<Vector<int>> new_faces = fixup_invalid_face(global_vertices_.vertices, face_verts);

    /* Create the newly formed faces. */
    for (Span<int> face : new_faces) {
      if (face.size() < 3) {
        continue;
      }
      FaceElem new_face{};
      new_face.vertex_group_index = face_vertex_group;
      new_face.material_index = face_material;
      new_face.shaded_smooth = face_shaded_smooth;
      new_face.start_index_ = mesh_geometry_.face_corners_.size();
      new_face.corner_count_ = face.size();
      for (int idx : face) {
        BLI_assert(idx >= 0 && idx < face_verts.size());
        mesh_geometry_.face_corners_.append({face_verts[idx], face_uvs[idx], face_normals[idx]});
      }
      mesh_geometry_.face_elements_.append(new_face);
      mesh_geometry_.total_corner_ += face.size();
    }
  }
}

void MeshFromGeometry::create_vertices(Mesh *mesh)
{
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  /* Go through all the global vertex indices from min to max,
   * checking which ones are actually and building a global->local
   * index mapping. Write out the used vertex positions into the Mesh
   * data. */
  mesh_geometry_.global_to_local_vertices_.clear();
  mesh_geometry_.global_to_local_vertices_.reserve(mesh_geometry_.vertices_.size());
  for (int vi = mesh_geometry_.vertex_index_min_; vi <= mesh_geometry_.vertex_index_max_; ++vi) {
    BLI_assert(vi >= 0 && vi < global_vertices_.vertices.size());
    if (!mesh_geometry_.vertices_.contains(vi)) {
      continue;
    }
    int local_vi = int(mesh_geometry_.global_to_local_vertices_.size());
    BLI_assert(local_vi >= 0 && local_vi < mesh->verts_num);
    copy_v3_v3(positions[local_vi], global_vertices_.vertices[vi]);
    mesh_geometry_.global_to_local_vertices_.add_new(vi, local_vi);
  }
}

void MeshFromGeometry::create_faces(Mesh *mesh, bool use_vertex_groups)
{
  MutableSpan<MDeformVert> dverts;
  const int64_t total_verts = mesh_geometry_.get_vertex_count();
  if (use_vertex_groups && total_verts && mesh_geometry_.has_vertex_groups_) {
    dverts = mesh->deform_verts_for_write();
  }

  Span<float3> positions = mesh->vert_positions();
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<int> material_indices =
      attributes.lookup_or_add_for_write_only_span<int>("material_index", bke::AttrDomain::Face);

  const bool set_face_sharpness = !has_normals();
  bke::SpanAttributeWriter<bool> sharp_faces = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_face", bke::AttrDomain::Face);

  int corner_index = 0;

  for (int face_idx = 0; face_idx < mesh->faces_num; ++face_idx) {
    const FaceElem &curr_face = mesh_geometry_.face_elements_[face_idx];
    if (curr_face.corner_count_ < 3) {
      /* Don't add single vertex face, or edges. */
      CLOG_WARN(&LOG, "Face with less than 3 vertices found, skipping.");
      continue;
    }

    face_offsets[face_idx] = corner_index;
    if (set_face_sharpness) {
      /* If we have no vertex normals, set face sharpness flag based on
       * whether smooth shading is off. */
      sharp_faces.span[face_idx] = !curr_face.shaded_smooth;
    }

    material_indices.span[face_idx] = curr_face.material_index;
    /* Importing obj files without any materials would result in negative indices, which is not
     * supported. */
    material_indices.span[face_idx] = std::max(material_indices.span[face_idx], 0);

    for (int idx = 0; idx < curr_face.corner_count_; ++idx) {
      const FaceCorner &curr_corner = mesh_geometry_.face_corners_[curr_face.start_index_ + idx];
      corner_verts[corner_index] = mesh_geometry_.global_to_local_vertices_.lookup_default(
          curr_corner.vert_index, 0);

      /* Setup vertex group data, if needed. */
      if (!dverts.is_empty()) {
        const int group_index = curr_face.vertex_group_index;
        /* NOTE: face might not belong to any group. */
        if (group_index >= 0 || true) {
          MDeformWeight *dw = BKE_defvert_ensure_index(&dverts[corner_verts[corner_index]],
                                                       group_index);
          dw->weight = 1.0f;
        }
      }

      corner_index++;
    }

    if (!set_face_sharpness) {
      /* If we do have vertex normals, we do not want to set face sharpness.
       * Exception is, if degenerate faces (zero area, with co-colocated
       * vertices) are present in the input data; this confuses custom
       * corner normals calculation in Blender. Set such faces as sharp,
       * they will be not shared across smooth vertex face fans. */
      const float area = bke::mesh::face_area_calc(
          positions, corner_verts.slice(face_offsets[face_idx], curr_face.corner_count_));
      if (area < 1.0e-12f) {
        sharp_faces.span[face_idx] = true;
      }
    }
  }

  material_indices.finish();
  sharp_faces.finish();
}

void MeshFromGeometry::create_vertex_groups(Object *obj)
{
  Mesh *mesh = static_cast<Mesh *>(obj->data);
  if (mesh->deform_verts().is_empty()) {
    return;
  }
  for (const std::string &name : mesh_geometry_.group_order_) {
    BKE_object_defgroup_add_name(obj, name.data());
  }
}

void MeshFromGeometry::create_edges(Mesh *mesh)
{
  MutableSpan<int2> edges = mesh->edges_for_write();

  const int64_t tot_edges{mesh_geometry_.edges_.size()};
  const int64_t total_verts{mesh_geometry_.get_vertex_count()};
  UNUSED_VARS_NDEBUG(total_verts);
  for (int i = 0; i < tot_edges; ++i) {
    const int2 &src_edge = mesh_geometry_.edges_[i];
    int2 &dst_edge = edges[i];
    dst_edge[0] = mesh_geometry_.global_to_local_vertices_.lookup_default(src_edge[0], 0);
    dst_edge[1] = mesh_geometry_.global_to_local_vertices_.lookup_default(src_edge[1], 0);
    BLI_assert(dst_edge[0] < total_verts && dst_edge[1] < total_verts);
  }

  /* Set argument `update` to true so that existing, explicitly imported edges can be merged
   * with the new ones created from faces. */
  bke::mesh_calc_edges(*mesh, true, false);
}

void MeshFromGeometry::create_uv_verts(Mesh *mesh)
{
  if (global_vertices_.uv_vertices.size() <= 0) {
    return;
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<float2> uv_map = attributes.lookup_or_add_for_write_only_span<float2>(
      "UVMap", bke::AttrDomain::Corner);

  int corner_index = 0;
  bool added_uv = false;

  for (const FaceElem &curr_face : mesh_geometry_.face_elements_) {
    for (int idx = 0; idx < curr_face.corner_count_; ++idx) {
      const FaceCorner &curr_corner = mesh_geometry_.face_corners_[curr_face.start_index_ + idx];
      if (curr_corner.uv_vert_index >= 0 &&
          curr_corner.uv_vert_index < global_vertices_.uv_vertices.size())
      {
        uv_map.span[corner_index] = global_vertices_.uv_vertices[curr_corner.uv_vert_index];
        added_uv = true;
      }
      else {
        uv_map.span[corner_index] = {0.0f, 0.0f};
      }
      corner_index++;
    }
  }

  uv_map.finish();

  /* If we have an object without UVs which resides in the same `.obj` file
   * as an object which *does* have UVs we can end up adding a UV layer
   * filled with zeroes.
   * We could maybe check before creating this layer but that would need
   * iterating over the whole mesh to check for UVs and as this is probably
   * the exception rather than the rule, just delete it afterwards.
   */
  if (!added_uv) {
    attributes.remove("UVMap");
  }
}

static Material *get_or_create_material(Main *bmain,
                                        const std::string &name,
                                        Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                                        Map<std::string, Material *> &created_materials,
                                        bool relative_paths,
                                        eOBJMtlNameCollisionMode mtl_name_collision_mode)
{
  /* Have we created this material already in this import session? */
  Material **found_mat = created_materials.lookup_ptr(name);
  if (found_mat != nullptr) {
    return *found_mat;
  }

  /* Check if a material with this name already exists in the main database */
  Material *existing_mat = (Material *)BKE_libblock_find_name(bmain, ID_MA, name.c_str());
  if (existing_mat != nullptr &&
      mtl_name_collision_mode == OBJ_MTL_NAME_COLLISION_REFERENCE_EXISTING)
  {
    /* If the collision mode is set to reference existing materials, use the existing one */
    created_materials.add_new(name, existing_mat);
    return existing_mat;
  }

  /* We need to create a new material */
  const MTLMaterial &mtl = *materials.lookup_or_add(name, std::make_unique<MTLMaterial>());

  /* If we're in MAKE_UNIQUE mode and a material with this name already exists,
   * BKE_material_add will automatically create a unique name */
  Material *mat = BKE_material_add(bmain, name.c_str());
  id_us_min(&mat->id);

  mat->nodetree = create_mtl_node_tree(bmain, mtl, mat, relative_paths);
  BKE_ntree_update_after_single_tree_change(*bmain, *mat->nodetree);

  created_materials.add_new(name, mat);
  return mat;
}

void MeshFromGeometry::create_materials(Main *bmain,
                                        Map<std::string, std::unique_ptr<MTLMaterial>> &materials,
                                        Map<std::string, Material *> &created_materials,
                                        Object *obj,
                                        bool relative_paths,
                                        eOBJMtlNameCollisionMode mtl_name_collision_mode)
{
  for (const std::string &name : mesh_geometry_.material_order_) {
    Material *mat = get_or_create_material(
        bmain, name, materials, created_materials, relative_paths, mtl_name_collision_mode);
    if (mat == nullptr) {
      continue;
    }
    BKE_object_material_assign_single_obdata(bmain, obj, mat, obj->totcol + 1);
  }
  if (obj->totcol > 0) {
    obj->actcol = 1;
  }
}

bool MeshFromGeometry::has_normals() const
{
  return !global_vertices_.vert_normals.is_empty() && mesh_geometry_.total_corner_ != 0;
}

void MeshFromGeometry::create_normals(Mesh *mesh)
{
  if (!has_normals()) {
    return;
  }

  Array<float3> corner_normals(mesh_geometry_.total_corner_);
  int corner_index = 0;
  for (const FaceElem &curr_face : mesh_geometry_.face_elements_) {
    for (int idx = 0; idx < curr_face.corner_count_; ++idx) {
      const FaceCorner &curr_corner = mesh_geometry_.face_corners_[curr_face.start_index_ + idx];
      int n_index = curr_corner.vertex_normal_index;
      float3 normal(0, 0, 0);
      if (n_index >= 0 && n_index < global_vertices_.vert_normals.size()) {
        normal = global_vertices_.vert_normals[n_index];
      }
      corner_normals[corner_index] = normal;
      corner_index++;
    }
  }
  bke::mesh_set_custom_normals(*mesh, corner_normals);
}

void MeshFromGeometry::create_colors(Mesh *mesh)
{
  /* Nothing to do if we don't have vertex colors at all. */
  if (global_vertices_.vertex_colors.is_empty()) {
    return;
  }

  /* First pass to determine if we need to create a color attribute. */
  for (int vi : mesh_geometry_.vertices_) {
    if (!global_vertices_.has_vertex_color(vi)) {
      return;
    }
  }

  AttributeOwner owner = AttributeOwner::from_id(&mesh->id);
  const std::string name = BKE_attribute_calc_unique_name(owner, "Color");
  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter attr = attributes.lookup_or_add_for_write_span<ColorGeometry4f>(
      name, bke::AttrDomain::Point);
  BKE_id_attributes_active_color_set(&mesh->id, name);
  BKE_id_attributes_default_color_set(&mesh->id, name);
  MutableSpan<float4> colors = attr.span.cast<float4>();

  /* Second pass to fill out the data. */
  for (auto item : mesh_geometry_.global_to_local_vertices_.items()) {
    const int vi = item.key;
    const int local_vi = item.value;
    BLI_assert(vi >= 0 && vi < global_vertices_.vertex_colors.size());
    BLI_assert(local_vi >= 0 && local_vi < mesh->verts_num);
    const float3 &c = global_vertices_.vertex_colors[vi];
    colors[local_vi] = float4(c.x, c.y, c.z, 1.0);
  }

  attr.finish();
}

}  // namespace blender::io::obj
