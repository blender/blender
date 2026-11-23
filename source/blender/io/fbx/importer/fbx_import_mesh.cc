/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup fbx
 */

#include <algorithm>
#include <cmath>

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_deform.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"

#include "BLI_array.hh"
#include "BLI_color_types.hh"
#include "BLI_listbase.hh"
#include "BLI_math_color_c.hh"
#include "BLI_math_matrix.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"
#include "BLI_task.hh"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "DNA_key_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "IO_fbx.hh"
#include "IO_validate.hh"

#include "fbx_import_mesh.hh"

#include "CLG_log.h"

namespace blender::io::fbx {

static CLG_LogRef LOG = {"io.fbx"};

static constexpr const char *temp_custom_normals_name = "fbx_temp_custom_normals";

static bool is_skin_deformer_usable(const ufbx_mesh *mesh, const ufbx_skin_deformer *skin)
{
  return mesh != nullptr && skin != nullptr && skin->clusters.count > 0 &&
         mesh->num_vertices > 0 && skin->vertices.count == mesh->num_vertices;
}

static const ufbx_skin_cluster *find_bind_cluster(const ufbx_skin_deformer &skin,
                                                  const FbxElementMapping &mapping)
{
  for (const ufbx_skin_cluster *cluster : skin.clusters) {
    if (cluster->num_weights > 0 && mapping.bone_to_armature.contains(cluster->bone_node)) {
      return cluster;
    }
  }
  return nullptr;
}

static ufbx_matrix mesh_bind_matrix(const ufbx_skin_cluster &cluster)
{
  return ufbx_matrix_mul(&cluster.bind_to_world, &cluster.geometry_to_bone);
}

/* FBX supports different bind pose for each skinned mesh (bind_to_world in skin cluster),
 * this allows several meshes at different bind poses to be driven by the same skeleton.
 * Blender's armature has just one rest pose that is shared by all the meshes, so the above
 * scenario produces incorrect deformations.
 *
 * Work around this similar to glTF importer: "bake" the mesh into armature's shared rest
 * pose (`bone_to_bind_matrix`):
 * - for each cluster, calculate matrix that takes geometry into that bone space at mesh own
 *   bind pose,
 * - then take that to world space using the shared rest pose of the bone,
 * - and finally back into the mesh local space as defined by bind cluster.
 *
 * The per-cluster matrices are blended by the per-vertex skin weights. Result is a matrix per
 * vertex.
 *
 * Returns an empty array when no correction is needed, or when it cannot be applied
 * (multiple armature skins, bones from another armature, non-invertible matrices).
 * The caller should apply the result to vertex positions and shape key offsets. Custom
 * normals should be set before the correction - they are stored relative to the surface,
 * so they follow the corrected shape. */
static Array<float4x4> calc_mesh_bind_corrections(const ufbx_mesh *mesh,
                                                  const FbxElementMapping &mapping)
{
  /* Find skin and cluster. */
  const ufbx_skin_deformer *skin = nullptr;
  const ufbx_skin_cluster *bind_cluster = nullptr;
  for (const ufbx_skin_deformer *deformer : mesh->skin_deformers) {
    if (!is_skin_deformer_usable(mesh, deformer)) {
      continue;
    }
    if (const ufbx_skin_cluster *cluster = find_bind_cluster(*deformer, mapping)) {
      if (skin != nullptr) {
        /* Multiple armature modifiers, can't bake. */
        return {};
      }
      skin = deformer;
      bind_cluster = cluster;
    }
  }
  if (skin == nullptr) {
    return {};
  }

  const Object *armature = mapping.bone_to_armature.lookup(bind_cluster->bone_node);
  const ufbx_matrix mesh_bind = mesh_bind_matrix(*bind_cluster);
  const ufbx_real determinant = ufbx_matrix_determinant(&mesh_bind);
  if (!std::isfinite(determinant) || std::abs(determinant) < 1e-150) {
    /* Non-invertible bind matrix, can't bake. */
    return {};
  }
  const ufbx_matrix world_to_mesh = ufbx_matrix_invert(&mesh_bind);

  /* Bring each cluster's bind space contribution to the armature's shared rest pose,
   * then back to the mesh's own local space. */
  Array<float4x4> cluster_corrections(skin->clusters.count, float4x4::identity());
  bool has_conflict = false;
  for (const int64_t index : cluster_corrections.index_range()) {
    const ufbx_skin_cluster &cluster = *skin->clusters[index];
    if (cluster.num_weights == 0) {
      continue;
    }
    if (mapping.bone_to_armature.lookup_default(cluster.bone_node, nullptr) != armature) {
      /* Cluster uses bones from another armature, can't bake. */
      return {};
    }
    const ufbx_matrix &shared_bind = mapping.bone_to_bind_matrix.lookup(cluster.bone_node);
    float4x4 shared_matrix;
    float4x4 cluster_matrix;
    matrix_to_m44(shared_bind, shared_matrix.ptr());
    matrix_to_m44(cluster.bind_to_world, cluster_matrix.ptr());
    has_conflict |= !math::is_equal(shared_matrix, cluster_matrix, 1e-5f);

    const ufbx_matrix geometry_to_shared_bind = ufbx_matrix_mul(&shared_bind,
                                                                &cluster.geometry_to_bone);
    const ufbx_matrix correction = ufbx_matrix_mul(&world_to_mesh, &geometry_to_shared_bind);
    matrix_to_m44(correction, cluster_corrections[index].ptr());
  }
  if (!has_conflict) {
    /* Bake corrections are not needed. */
    return {};
  }

  /* Bake the per-vertex weighted corrections. */
  Array<float4x4> vertex_corrections(mesh->num_vertices);
  for (const int64_t index : vertex_corrections.index_range()) {
    const ufbx_skin_vertex &vertex = skin->vertices[index];
    float4x4 correction = float4x4::zero();
    float total_weight = 0.0f;
    for (const uint32_t weight_index : IndexRange(vertex.weight_begin, vertex.num_weights)) {
      const ufbx_skin_weight &weight = skin->weights[weight_index];
      correction += cluster_corrections[weight.cluster_index] * float(weight.weight);
      total_weight += float(weight.weight);
    }
    vertex_corrections[index] = total_weight > 0.0f && std::isfinite(total_weight) ?
                                    correction * (1.0f / total_weight) :
                                    float4x4::identity();
  }
  return vertex_corrections;
}

static void import_vertex_positions(const ufbx_mesh *fmesh, Mesh *mesh)
{
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  BLI_assert(positions.size() == fmesh->vertex_position.values.count);
  for (const int64_t i : positions.index_range()) {
    ufbx_vec3 val = fmesh->vertex_position.values[i];
    positions[i] = float3(val.x, val.y, val.z);
  }
}

static void apply_bind_corrections(Mesh &mesh, const Span<float4x4> bind_corrections)
{
  if (bind_corrections.is_empty()) {
    return;
  }
  MutableSpan<float3> positions = mesh.vert_positions_for_write();
  for (const int64_t i : positions.index_range()) {
    positions[i] = math::transform_point(bind_corrections[i], positions[i]);
  }
  mesh.tag_positions_changed();
}

static void import_faces(const ufbx_mesh *fmesh, Mesh *mesh)
{
  MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  BLI_assert((face_offsets.size() == fmesh->num_faces + 1) ||
             (face_offsets.is_empty() && fmesh->num_faces == 0));
  for (size_t face_idx = 0; face_idx < fmesh->num_faces; face_idx++) {
    //@TODO: skip < 3 vertex faces?
    const ufbx_face &fface = fmesh->faces[face_idx];
    face_offsets[face_idx] = fface.index_begin;
    for (uint32_t i = 0; i < fface.num_indices; i++) {
      const uint32_t corner_idx = fface.index_begin + i;
      corner_verts[corner_idx] = fmesh->vertex_indices[corner_idx];
    }
  }
}

static void import_face_material_indices(const ufbx_mesh *fmesh,
                                         bke::MutableAttributeAccessor &attributes)
{
  if (fmesh->face_material.count == fmesh->num_faces) {
    bke::SpanAttributeWriter<int> materials = attributes.ensure_for_write_only_span<int>(
        "material_index", bke::AttrDomain::Face);
    for (const int64_t i : materials.span.index_range()) {
      materials.span[i] = fmesh->face_material[i];
    }
    materials.finish();
  }
}

static void import_face_smoothing(const ufbx_mesh *fmesh,
                                  bke::MutableAttributeAccessor &attributes)
{
  if (fmesh->face_smoothing.count > 0 && fmesh->face_smoothing.count == fmesh->num_faces) {
    bke::SpanAttributeWriter<bool> smooth = attributes.ensure_for_write_only_span<bool>(
        "sharp_face", bke::AttrDomain::Face);
    for (const int64_t i : smooth.span.index_range()) {
      smooth.span[i] = !fmesh->face_smoothing[i];
    }
    smooth.finish();
  }
}

static void import_edges(const ufbx_mesh *fmesh,
                         Mesh *mesh,
                         bke::MutableAttributeAccessor &attributes)
{
  MutableSpan<int2> edges = mesh->edges_for_write();
  BLI_assert(edges.size() == fmesh->num_edges);
  for (size_t i = 0; i < fmesh->num_edges; i++) {
    const ufbx_edge &fedge = fmesh->edges[i];
    const int va = fmesh->vertex_indices[fedge.a];
    const int vb = fmesh->vertex_indices[fedge.b];
    edges[i] = int2(va, vb);
  }

  /* Edge attributes are written here in the same order as the FBX edges. Mesh validation
   * preserves edge attributes when removing degenerate edges or computing missing ones. */
  if (fmesh->edge_crease.count > 0 && fmesh->edge_crease.count == fmesh->num_edges) {
    bke::SpanAttributeWriter<float> creases = attributes.ensure_for_write_only_span<float>(
        "crease_edge", bke::AttrDomain::Edge);
    for (int64_t i = 0; i < fmesh->num_edges; i++) {
      /* Python fbx importer was squaring the incoming crease values. */
      creases.span[i] = sqrtf(fmesh->edge_crease[i]);
    }
    creases.finish();
  }

  if (fmesh->edge_smoothing.count > 0 && fmesh->edge_smoothing.count == fmesh->num_edges) {
    bke::SpanAttributeWriter<bool> sharp = attributes.ensure_for_write_only_span<bool>(
        "sharp_edge", bke::AttrDomain::Edge);
    for (int64_t i = 0; i < fmesh->num_edges; i++) {
      sharp.span[i] = !fmesh->edge_smoothing[i];
    }
    sharp.finish();
  }
}

static void import_uvs(const ufbx_mesh *fmesh,
                       Mesh *mesh,
                       bke::MutableAttributeAccessor &attributes,
                       AttributeOwner attr_owner)
{
  bool set_active_uv = true;
  for (const ufbx_uv_set &fuv_set : fmesh->uv_sets) {
    std::string attr_name = BKE_attribute_calc_unique_name(attr_owner, fuv_set.name.data);
    if (set_active_uv) {
      mesh->uv_maps_active_set(attr_name);
      mesh->uv_maps_default_set(attr_name);
      set_active_uv = false;
    }
    if (bke::SpanAttributeWriter<float2> uvs =
            attributes.lookup_or_add_for_write_only_span<float2>(attr_name,
                                                                 bke::AttrDomain::Corner))
    {
      BLI_assert(fuv_set.vertex_uv.indices.count == uvs.span.size());
      for (const int64_t i : uvs.span.index_range()) {
        /* Note: we have turned off ufbx side clamping, so do it on our end. */
        const size_t val_idx = std::min<size_t>(fuv_set.vertex_uv.indices[i],
                                                fuv_set.vertex_uv.values.count - 1);
        const ufbx_vec2 &uv = fuv_set.vertex_uv.values[val_idx];
        uvs.span[i] = float2(uv.x, uv.y);
      }
      uvs.finish();
    }
  }
}

static void import_colors(const ufbx_mesh *fmesh,
                          Mesh *mesh,
                          bke::MutableAttributeAccessor &attributes,
                          AttributeOwner attr_owner,
                          eFBXVertexColorMode color_mode)
{
  std::string first_color_name;
  for (const ufbx_color_set &fcol_set : fmesh->color_sets) {
    std::string attr_name = BKE_attribute_calc_unique_name(attr_owner, fcol_set.name.data);
    if (first_color_name.empty()) {
      first_color_name = attr_name;
    }
    if (color_mode == eFBXVertexColorMode::sRGB) {
      /* sRGB colors, use 4 bytes per color. */
      if (bke::SpanAttributeWriter<ColorGeometry4b> cols =
              attributes.lookup_or_add_for_write_only_span<ColorGeometry4b>(
                  attr_name, bke::AttrDomain::Corner))
      {
        BLI_assert(fcol_set.vertex_color.indices.count == cols.span.size());
        for (const int64_t i : cols.span.index_range()) {
          /* Note: we have turned off ufbx side clamping, so do it on our end. */
          const size_t val_idx = std::min<size_t>(fcol_set.vertex_color.indices[i],
                                                  fcol_set.vertex_color.values.count - 1);
          const ufbx_vec4 &col = fcol_set.vertex_color.values[val_idx];
          /* Note: color values are expected to already be in sRGB space. */
          float4 fcol = float4(col.x, col.y, col.z, col.w);
          uchar4 bcol;
          rgba_float_to_uchar(bcol, fcol);
          cols.span[i] = ColorGeometry4b(bcol);
        }
        cols.finish();
      }
    }
    else if (color_mode == eFBXVertexColorMode::Linear) {
      /* Linear colors, use 4 floats per color. */
      if (bke::SpanAttributeWriter<ColorGeometry4f> cols =
              attributes.lookup_or_add_for_write_only_span<ColorGeometry4f>(
                  attr_name, bke::AttrDomain::Corner))
      {
        BLI_assert(fcol_set.vertex_color.indices.count == cols.span.size());
        for (const int64_t i : cols.span.index_range()) {
          /* Note: we have turned off ufbx side clamping, so do it on our end. */
          const size_t val_idx = std::min<size_t>(fcol_set.vertex_color.indices[i],
                                                  fcol_set.vertex_color.values.count - 1);
          const ufbx_vec4 &col = fcol_set.vertex_color.values[val_idx];
          cols.span[i] = ColorGeometry4f(col.x, col.y, col.z, col.w);
        }
        cols.finish();
      }
    }
    else {
      BLI_assert_unreachable();
    }
  }
  if (!first_color_name.empty()) {
    mesh->active_color_attribute = BLI_strdup(first_color_name.c_str());
    mesh->default_color_attribute = BLI_strdup(first_color_name.c_str());
  }
}

static bool import_normals_into_temp_attribute(const ufbx_mesh *fmesh,
                                               bke::MutableAttributeAccessor &attributes)
{
  if (!fmesh->vertex_normal.exists) {
    return false;
  }
  if (bke::SpanAttributeWriter<float3> normals =
          attributes.lookup_or_add_for_write_only_span<float3>(temp_custom_normals_name,
                                                               bke::AttrDomain::Corner))
  {
    BLI_assert(fmesh->vertex_normal.indices.count == normals.span.size());
    for (const int64_t i : normals.span.index_range()) {
      const uint32_t val_idx = fmesh->vertex_normal.indices[i];
      if (val_idx == UFBX_NO_INDEX) {
        normals.finish();
        attributes.remove(temp_custom_normals_name);
        return false;
      }
      const ufbx_vec3 &normal = fmesh->vertex_normal.values[val_idx];
      normals.span[i] = float3(normal.x, normal.y, normal.z);
    }
    normals.finish();
    return true;
  }
  return false;
}

static VectorSet<std::string> get_skin_bone_name_set(const FbxElementMapping &mapping,
                                                     const ufbx_mesh *fmesh)
{
  VectorSet<std::string> name_set;
  for (const ufbx_skin_deformer *skin : fmesh->skin_deformers) {
    if (!is_skin_deformer_usable(fmesh, skin)) {
      continue;
    }

    for (const ufbx_skin_cluster *cluster : skin->clusters) {
      if (cluster->num_weights == 0) {
        continue;
      }

      std::string bone_name = mapping.node_to_name.lookup_default(cluster->bone_node, "");
      name_set.add(bone_name);
    }
  }
  return name_set;
}

static void import_skin_vertex_groups(const FbxElementMapping &mapping,
                                      const ufbx_mesh *fmesh,
                                      Mesh *mesh)
{
  if (fmesh->skin_deformers.count == 0) {
    return;
  }

  /* A single mesh can be skinned by several armatures, so we need to build bone (vertex group)
   * name set, taking all skin deformers into account. */
  VectorSet<std::string> bone_set = get_skin_bone_name_set(mapping, fmesh);
  if (bone_set.is_empty()) {
    return;
  }

  MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();

  for (const ufbx_skin_deformer *skin : fmesh->skin_deformers) {
    if (!is_skin_deformer_usable(fmesh, skin)) {
      continue;
    }

    for (const ufbx_skin_cluster *cluster : skin->clusters) {
      if (cluster->num_weights == 0) {
        continue;
      }
      std::string bone_name = mapping.node_to_name.lookup_default(cluster->bone_node, "");
      const int group_index = bone_set.index_of_try(bone_name);
      if (group_index < 0) {
        continue;
      }

      for (int64_t i = 0; i < cluster->num_weights; i++) {
        const int vertex = cluster->vertices[i];
        if (validate::index_in_range(vertex, dverts.size())) {
          MDeformWeight *dw = BKE_defvert_ensure_index(&dverts[vertex], group_index);
          dw->weight = cluster->weights[i];
        }
      }
    }
  }
}

static bool import_blend_shapes(Main &bmain,
                                FbxElementMapping &mapping,
                                const ufbx_mesh *fmesh,
                                Mesh *mesh,
                                const Span<float4x4> bind_corrections)
{
  Key *mesh_key = nullptr;
  for (const ufbx_blend_deformer *fdeformer : fmesh->blend_deformers) {
    for (const ufbx_blend_channel *fchan : fdeformer->channels) {
      /* In theory fbx supports multiple keyframes within one blend shape
       * channel; we only take the final target keyframe. */
      if (fchan->target_shape == nullptr) {
        continue;
      }

      if (mesh_key == nullptr) {
        mesh_key = BKE_key_add(&bmain, &mesh->id);
        mesh_key->type = KEY_RELATIVE;
        mesh->key = mesh_key;

        KeyBlock *kb = BKE_keyblock_add(mesh_key, nullptr);
        BKE_keyblock_convert_from_mesh(mesh, mesh_key, kb);
      }

      KeyBlock *kb = BKE_keyblock_add(mesh_key, fchan->target_shape->name.data);
      const float weight = float(fchan->weight);
      expand_shape_key_range(*kb, weight, weight);
      kb->curval = std::clamp(weight, kb->slidermin, kb->slidermax);
      BKE_keyblock_convert_from_mesh(mesh, mesh_key, kb);
      if (!kb->data) {
        /* Nothing to do. This can happen if the mesh has no vertices. */
        continue;
      }
      float3 *kb_data = static_cast<float3 *>(kb->data);
      for (size_t i = 0; i < fchan->target_shape->num_offsets; i++) {
        const int idx = fchan->target_shape->offset_vertices[i];
        if (!validate::index_in_range(idx, mesh->verts_num)) {
          continue;
        }
        const ufbx_vec3 &delta = fchan->target_shape->position_offsets[i];
        float3 offset(delta.x, delta.y, delta.z);
        if (!bind_corrections.is_empty()) {
          offset = math::transform_direction(bind_corrections[idx], offset);
        }
        kb_data[idx] += offset;
      }
      mapping.el_to_shape_key.add(&fchan->element, {mesh_key, kb});
    }
  }
  return mesh_key != nullptr;
}

/* Handle Blender-specific "FullWeights" that for each blend shape also create
 * a weighted vertex group for itself. */
static void import_blend_shape_full_weights(const FbxElementMapping &mapping,
                                            const ufbx_mesh *fmesh,
                                            Mesh *mesh,
                                            Object *obj)
{
  for (const ufbx_blend_deformer *fdeformer : fmesh->blend_deformers) {
    for (const ufbx_blend_channel *fchan : fdeformer->channels) {
      const FbxShapeKey *shape_key = mapping.el_to_shape_key.lookup_ptr(&fchan->element);
      if (shape_key == nullptr) {
        continue;
      }
      if (fchan->target_shape->offset_weights.count != fchan->target_shape->num_offsets) {
        continue;
      }

      KeyBlock *kb = shape_key->block;

      /* Ignore cases where all weights are 1.0 (group has no effect),
       * and cases where any weights are outside of 0..1 range (apparently some files have
       * invalid negative weights and should be ignored). */
      bool all_one = true;
      bool all_unorm = true;
      for (ufbx_real w : fchan->target_shape->offset_weights) {
        if (w != 1.0) {
          all_one = false;
        }
        if (w < 0.0 || w > 1.0) {
          all_unorm = false;
        }
      }
      if (all_one || !all_unorm) {
        continue;
      }

      int group_index = BKE_defgroup_name_index(&mesh->vertex_group_names, kb->name);
      if (group_index < 0) {
        BKE_object_defgroup_add_name(obj, kb->name);
        group_index = BKE_defgroup_name_index(&mesh->vertex_group_names, kb->name);
        if (group_index < 0) {
          continue;
        }
      }

      MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
      for (size_t i = 0; i < fchan->target_shape->num_offsets; i++) {
        const int idx = fchan->target_shape->offset_vertices[i];
        if (validate::index_in_range(idx, dverts.size())) {
          const float w = fchan->target_shape->offset_weights[i];
          MDeformWeight *dw = BKE_defvert_ensure_index(&dverts[idx], group_index);
          dw->weight = w;
        }
      }

      STRNCPY_UTF8(kb->vgroup, kb->name);
    }
  }
}

void import_meshes(Main &bmain,
                   const ufbx_scene &fbx,
                   FbxElementMapping &mapping,
                   const FBXImportParams &params)
{
  /* Create Mesh objects outside of Main, in parallel. */
  Vector<Mesh *> meshes(fbx.meshes.count);
  Vector<Array<float4x4>> shape_bind_corrections(fbx.meshes.count);
  threading::parallel_for_each(IndexRange(fbx.meshes.count), [&](const int64_t index) {
    const ufbx_mesh *fmesh = fbx.meshes.data[index];
    if (fmesh->instances.count == 0) {
      meshes[index] = nullptr; /* Ignore if not used by any objects. */
      return;
    }

    if (!validate::size_fits_in_int(fmesh->num_vertices) ||
        !validate::size_fits_in_int(fmesh->num_edges) ||
        !validate::size_fits_in_int(fmesh->num_faces) ||
        !validate::size_fits_in_int(fmesh->num_indices))
    {
      CLOG_WARN(&LOG, "Mesh '%s' too large to import, exceeds max int size", fmesh->name.data);
      meshes[index] = nullptr;
      return;
    }

    /* Create Mesh outside of main. */
    Mesh *mesh = BKE_mesh_new_nomain(
        fmesh->num_vertices, fmesh->num_edges, fmesh->num_faces, fmesh->num_indices);
    bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
    AttributeOwner attr_owner = AttributeOwner::from_id(&mesh->id);

    Array<float4x4> bind_corrections = calc_mesh_bind_corrections(fmesh, mapping);
    import_vertex_positions(fmesh, mesh);
    import_faces(fmesh, mesh);
    import_face_material_indices(fmesh, attributes);
    import_face_smoothing(fmesh, attributes);
    import_edges(fmesh, mesh, attributes);
    import_uvs(fmesh, mesh, attributes, attr_owner);
    if (params.vertex_colors != eFBXVertexColorMode::None) {
      import_colors(fmesh, mesh, attributes, attr_owner, params.vertex_colors);
    }
    bool has_custom_normals = false;
    if (params.use_custom_normals) {
      /* Mesh validation below can alter the mesh, so we first write custom normals
       * into a temporary custom corner domain attribute, and then re-apply that
       * data as custom normals after the validation. */
      has_custom_normals = import_normals_into_temp_attribute(fmesh, attributes);
    }
    import_skin_vertex_groups(mapping, fmesh, mesh);

    /* Add vertex groups to the object. */
    VectorSet<std::string> bone_set = get_skin_bone_name_set(mapping, fmesh);
    for (const std::string &name : bone_set) {
      bDeformGroup *defgroup = MEM_new<bDeformGroup>("bDeformGroup");
      StringRef(name).copy_utf8_truncated(defgroup->name);
      BLI_addtail(&mesh->vertex_group_names, defgroup);
    }

    /* FBX files may not contain all edges, so missing edges must be added here.
     * Validation will do this, and otherwise calculate them explicitly. */
    if (params.validate_meshes) {
      const bool allow_missing_edges = true;
#ifndef NDEBUG
      const bool verbose_validate = true;
#else
      const bool verbose_validate = false;
#endif
      bke::mesh_validate(*mesh, verbose_validate, allow_missing_edges);
    }
    else {
      bke::mesh_calc_edges(*mesh, true, false);
    }

    if (has_custom_normals) {
      /* Actually set custom normals after the validation. */
      if (bke::SpanAttributeWriter<float3> normals =
              attributes.lookup_or_add_for_write_only_span<float3>(temp_custom_normals_name,
                                                                   bke::AttrDomain::Corner))
      {
        bke::mesh_set_custom_normals(*mesh, normals.span);
        normals.finish();
        attributes.remove(temp_custom_normals_name);
      }
    }

    /* Note: custom normals are stored relative to the surface, so it is correct to
     * bake position corrections after setting them. */
    apply_bind_corrections(*mesh, bind_corrections);

    if (fmesh->blend_deformers.count > 0) {
      shape_bind_corrections[index] = std::move(bind_corrections);
    }
    meshes[index] = mesh;
  });

  /* Create final mesh objects in Main, serially. And do steps that need to be done on the final
   * objects. */
  for (int64_t index : meshes.index_range()) {
    Mesh *mesh = meshes[index];
    if (mesh == nullptr) {
      continue;
    }
    const ufbx_mesh *fmesh = fbx.meshes[index];
    BLI_assert(fmesh != nullptr);

    Mesh *mesh_main = static_cast<Mesh *>(
        BKE_object_obdata_add_from_type(&bmain, OB_MESH, get_fbx_name(fmesh->name, "Mesh")));
    BKE_mesh_nomain_to_mesh(mesh, mesh_main, nullptr);
    meshes[index] = mesh_main;
    mesh = mesh_main;
    if (params.use_custom_props) {
      read_custom_properties(fmesh->props, mesh->id, params.props_enum_as_string);
    }

    const bool any_shapes = import_blend_shapes(
        bmain, mapping, fmesh, mesh, shape_bind_corrections[index]);

    /* Create objects that use this mesh. */
    for (const ufbx_node *node : fmesh->instances) {
      std::string name;
      if (node->is_geometry_transform_helper) {
        /* Name geometry transform adjustment helpers with parent name and _GeomAdjust suffix. */
        name = get_fbx_name(node->parent->name) + std::string("_GeomAdjust");
      }
      else {
        name = get_fbx_name(node->name);
      }
      Object *obj = BKE_object_add_only_object(&bmain, OB_MESH, name.c_str());
      obj->data = id_cast<ID *>(mesh_main);
      if (!node->visible) {
        obj->visibility_flag |= OB_HIDE_VIEWPORT;
      }

      if (any_shapes) {
        obj->shapenr = 1;
      }

      Object *first_armature = nullptr;
      const ufbx_skin_cluster *first_bind_cluster = nullptr;

      /* Skinned mesh. */
      if (fmesh->skin_deformers.count > 0) {
        /* Add armature modifiers for each skin deformer. */
        for (const ufbx_skin_deformer *skin : fmesh->skin_deformers) {
          if (!is_skin_deformer_usable(fmesh, skin)) {
            continue;
          }
          /* Add armature modifier. */
          if (const ufbx_skin_cluster *bind_cluster = find_bind_cluster(*skin, mapping)) {
            Object *arm_obj = mapping.bone_to_armature.lookup(bind_cluster->bone_node);
            ModifierData *md = BKE_modifier_new(eModifierType_Armature);
            STRNCPY_UTF8(md->name, BKE_id_name(arm_obj->id));
            BLI_addtail(&obj->modifiers, md);
            BKE_modifiers_persistent_uid_init(*obj, *md);
            ArmatureModifierData *ad = reinterpret_cast<ArmatureModifierData *>(md);
            ad->object = arm_obj;

            if (first_armature == nullptr) {
              first_armature = arm_obj;
              first_bind_cluster = bind_cluster;
            }
          }
        }
      }

      if (any_shapes) {
        import_blend_shape_full_weights(mapping, fmesh, mesh, obj);
      }

      /* Assign materials. */
      if (fmesh->materials.count > 0 && node->materials.count == fmesh->materials.count) {
        int mat_index = 0;
        for (size_t mi = 0; mi < fmesh->materials.count; mi++) {
          const ufbx_material *mesh_fmat = fmesh->materials[mi];
          const ufbx_material *node_fmat = node->materials[mi];
          Material *mesh_mat = mapping.mat_to_material.lookup_default(mesh_fmat, nullptr);
          Material *node_mat = mapping.mat_to_material.lookup_default(node_fmat, nullptr);
          if (mesh_mat != nullptr) {
            mat_index++;
            /* Assign material to the data block. */
            BKE_object_material_assign_single_obdata(&bmain, obj, mesh_mat, mat_index);

            /* If object material is different, assign that to object. */
            if (!ELEM(node_mat, nullptr, mesh_mat)) {
              BKE_object_material_assign(&bmain, obj, node_mat, mat_index, BKE_MAT_ASSIGN_OBJECT);
            }
          }
        }
        if (mat_index > 0) {
          obj->actcol = 1;
        }
      }

      /* Subdivision. */
      if (params.import_subdivision &&
          fmesh->subdivision_display_mode != UFBX_SUBDIVISION_DISPLAY_DISABLED &&
          (fmesh->subdivision_preview_levels > 0 || fmesh->subdivision_render_levels > 0))
      {
        ModifierData *md = BKE_modifier_new(eModifierType_Subsurf);
        BLI_addtail(&obj->modifiers, md);
        BKE_modifiers_persistent_uid_init(*obj, *md);

        SubsurfModifierData *ssd = reinterpret_cast<SubsurfModifierData *>(md);
        ssd->subdivType = SUBSURF_TYPE_CATMULL_CLARK;
        ssd->levels = fmesh->subdivision_preview_levels;
        ssd->renderLevels = fmesh->subdivision_render_levels;
        ssd->boundary_smooth = fmesh->subdivision_boundary ==
                                       UFBX_SUBDIVISION_BOUNDARY_SHARP_CORNERS ?
                                   SUBSURF_BOUNDARY_SMOOTH_PRESERVE_CORNERS :
                                   SUBSURF_BOUNDARY_SMOOTH_ALL;
      }

      if (params.use_custom_props) {
        read_custom_properties(node->props, obj->id, params.props_enum_as_string);
      }
      if (first_armature != nullptr) {
        mapping.skinned_meshes.append({node, obj, first_armature, first_bind_cluster});
      }
      node_matrix_to_obj(node, obj, mapping);
      mapping.el_to_object.add(&node->element, obj);
      mapping.imported_objects.add(obj);
    }
  }
}

void setup_skinned_mesh_parenting(FbxElementMapping &mapping)
{
  for (const FbxSkinnedMesh &mesh : mapping.skinned_meshes) {
    Object *obj = mesh.object;
    Object *arm_obj = mesh.armature;

    /* Parent skinned meshes to their armature so they follow its object transform.
     * If the armature is already below the mesh in the hierarchy, keep the original parent
     * and armature modifier to avoid a parenting cycle. */
    if (BKE_object_parent_loop_check(arm_obj, obj)) {
      continue;
    }
    obj->parent = arm_obj;
    obj->partype = PAROBJECT;
    obj->parsubstr[0] = '\0';

    /* Express the mesh transform relative to the armature FBX node transform. The bind pose
     * is accounted for separately in the parent inverse. */
    const ufbx_matrix world_to_arm = mapping.armature_world_to_arm_node_matrix.lookup_default(
        arm_obj, ufbx_identity_matrix);
    const ufbx_matrix local = ufbx_matrix_mul(&world_to_arm, &mesh.node->geometry_to_world);
    ufbx_matrix_to_obj(local, obj);

    /* The skin cluster binds the mesh geometry to a bone. Compose that with the bone's bind
     * transform to recover the mesh's world-space bind transform. */
    const ufbx_matrix world_to_arm_pose = mapping.armature_world_to_arm_pose_matrix.lookup_default(
        arm_obj, ufbx_identity_matrix);
    const ufbx_matrix local_inv = ufbx_matrix_invert(&local);
    const ufbx_matrix mesh_bind = mesh_bind_matrix(*mesh.bind_cluster);
    const ufbx_matrix bind_to_local = ufbx_matrix_mul(&mesh_bind, &local_inv);
    const ufbx_matrix parent_inverse = ufbx_matrix_mul(&world_to_arm_pose, &bind_to_local);
    matrix_to_m44(parent_inverse, obj->parentinv);
    mapping.meshes_reparented_to_armature.add(obj);
  }
}

}  // namespace blender::io::fbx
