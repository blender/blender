/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <optional>

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_hash.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_memory_counter.hh"
#include "BLI_resource_scope.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_time.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_attribute_math.hh"
#include "BKE_attribute_storage.hh"
#include "BKE_attribute_storage_blend_write.hh"
#include "BKE_bake_data_block_id.hh"
#include "BKE_bpath.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_cache.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_paint_bvh.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

/** Using STACK_FIXED_DEPTH to keep the implementation in line with `pbvh.cc`. */
#define STACK_FIXED_DEPTH 100

using blender::float3;
using blender::int2;
using blender::MutableSpan;
using blender::OffsetIndices;
using blender::Span;
using blender::StringRef;
using blender::VArray;
using blender::Vector;

static void mesh_tessface_clear_intern(Mesh *mesh, int free_customdata);

static void mesh_init_data(ID *id)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mesh, id));

  MEMCPY_STRUCT_AFTER(mesh, DNA_struct_default_get(Mesh), id);

  CustomData_reset(&mesh->vert_data);
  CustomData_reset(&mesh->edge_data);
  CustomData_reset(&mesh->fdata_legacy);
  CustomData_reset(&mesh->face_data);
  CustomData_reset(&mesh->corner_data);

  new (&mesh->attribute_storage.wrap()) blender::bke::AttributeStorage();
  mesh->runtime = new blender::bke::MeshRuntime();

  mesh->face_sets_color_seed = BLI_hash_int(BLI_time_now_seconds_i() & UINT_MAX);
}

static void mesh_copy_data(Main *bmain,
                           std::optional<Library *> owner_library,
                           ID *id_dst,
                           const ID *id_src,
                           const int flag)
{
  Mesh *mesh_dst = reinterpret_cast<Mesh *>(id_dst);
  const Mesh *mesh_src = reinterpret_cast<const Mesh *>(id_src);

  mesh_dst->runtime = new blender::bke::MeshRuntime();
  mesh_dst->runtime->deformed_only = mesh_src->runtime->deformed_only;
  /* Subd runtime.mesh_eval is not copied, will need to be reevaluated. */
  mesh_dst->runtime->wrapper_type = (mesh_src->runtime->wrapper_type == ME_WRAPPER_TYPE_SUBD) ?
                                        ME_WRAPPER_TYPE_MDATA :
                                        mesh_src->runtime->wrapper_type;
  mesh_dst->runtime->subsurf_runtime_data = mesh_src->runtime->subsurf_runtime_data;
  mesh_dst->runtime->cd_mask_extra = mesh_src->runtime->cd_mask_extra;
  /* Copy face dot tags and edge tags, since meshes may be duplicated after a subsurf modifier or
   * node, but we still need to be able to draw face center vertices and "optimal edges"
   * differently. The tags may be cleared explicitly when the topology is changed. */
  mesh_dst->runtime->subsurf_face_dot_tags = mesh_src->runtime->subsurf_face_dot_tags;
  mesh_dst->runtime->subsurf_optimal_display_edges =
      mesh_src->runtime->subsurf_optimal_display_edges;
  if ((mesh_src->id.tag & ID_TAG_NO_MAIN) == 0) {
    /* This is a direct copy of a main mesh, so for now it has the same topology. */
    mesh_dst->runtime->deformed_only = true;
  }
  /* This option is set for run-time meshes that have been copied from the current object's mode.
   * Currently this is used for edit-mesh although it could be used for sculpt or other
   * kinds of data specific to an object's mode.
   *
   * The flag signals that the mesh hasn't been modified from the data that generated it,
   * allowing us to use the object-mode data for drawing.
   *
   * While this could be the caller's responsibility, keep here since it's
   * highly unlikely we want to create a duplicate and not use it for drawing. */
  mesh_dst->runtime->is_original_bmesh = false;

  /* Share various derived caches between the source and destination mesh for improved performance
   * when the source is persistent and edits to the destination mesh don't affect the caches.
   * Caches will be "un-shared" as necessary later on. */
  mesh_dst->runtime->bounds_cache = mesh_src->runtime->bounds_cache;
  mesh_dst->runtime->vert_normals_cache = mesh_src->runtime->vert_normals_cache;
  mesh_dst->runtime->vert_normals_true_cache = mesh_src->runtime->vert_normals_true_cache;
  mesh_dst->runtime->face_normals_cache = mesh_src->runtime->face_normals_cache;
  mesh_dst->runtime->face_normals_true_cache = mesh_src->runtime->face_normals_true_cache;
  mesh_dst->runtime->corner_normals_cache = mesh_src->runtime->corner_normals_cache;
  mesh_dst->runtime->loose_verts_cache = mesh_src->runtime->loose_verts_cache;
  mesh_dst->runtime->verts_no_face_cache = mesh_src->runtime->verts_no_face_cache;
  mesh_dst->runtime->loose_edges_cache = mesh_src->runtime->loose_edges_cache;
  mesh_dst->runtime->corner_tris_cache = mesh_src->runtime->corner_tris_cache;
  mesh_dst->runtime->corner_tri_faces_cache = mesh_src->runtime->corner_tri_faces_cache;
  mesh_dst->runtime->vert_to_face_offset_cache = mesh_src->runtime->vert_to_face_offset_cache;
  mesh_dst->runtime->vert_to_face_map_cache = mesh_src->runtime->vert_to_face_map_cache;
  mesh_dst->runtime->vert_to_corner_map_cache = mesh_src->runtime->vert_to_corner_map_cache;
  mesh_dst->runtime->corner_to_face_map_cache = mesh_src->runtime->corner_to_face_map_cache;
  mesh_dst->runtime->bvh_cache_verts = mesh_src->runtime->bvh_cache_verts;
  mesh_dst->runtime->bvh_cache_edges = mesh_src->runtime->bvh_cache_edges;
  mesh_dst->runtime->bvh_cache_faces = mesh_src->runtime->bvh_cache_faces;
  mesh_dst->runtime->bvh_cache_corner_tris = mesh_src->runtime->bvh_cache_corner_tris;
  mesh_dst->runtime->bvh_cache_corner_tris_no_hidden =
      mesh_src->runtime->bvh_cache_corner_tris_no_hidden;
  mesh_dst->runtime->bvh_cache_loose_verts = mesh_src->runtime->bvh_cache_loose_verts;
  mesh_dst->runtime->bvh_cache_loose_verts_no_hidden =
      mesh_src->runtime->bvh_cache_loose_verts_no_hidden;
  mesh_dst->runtime->bvh_cache_loose_edges = mesh_src->runtime->bvh_cache_loose_edges;
  mesh_dst->runtime->bvh_cache_loose_edges_no_hidden =
      mesh_src->runtime->bvh_cache_loose_edges_no_hidden;
  mesh_dst->runtime->max_material_index = mesh_src->runtime->max_material_index;
  if (mesh_src->runtime->bake_materials) {
    mesh_dst->runtime->bake_materials = std::make_unique<blender::bke::bake::BakeMaterialsList>(
        *mesh_src->runtime->bake_materials);
  }

  /* Only do tessface if we have no faces. */
  const bool do_tessface = ((mesh_src->totface_legacy != 0) && (mesh_src->faces_num == 0));

  CustomData_MeshMasks mask = CD_MASK_MESH;

  if (mesh_src->id.tag & ID_TAG_NO_MAIN) {
    /* For copies in depsgraph, keep data like #CD_ORIGINDEX and #CD_ORCO. */
    CustomData_MeshMasks_update(&mask, &CD_MASK_DERIVEDMESH);

    /* Meshes copied during evaluation pass the edit mesh pointer to determine whether a mapping
     * from the evaluated to the original state is possible. */
    mesh_dst->runtime->edit_mesh = mesh_src->runtime->edit_mesh;
    if (const blender::bke::EditMeshData *edit_data = mesh_src->runtime->edit_data.get()) {
      mesh_dst->runtime->edit_data = std::make_unique<blender::bke::EditMeshData>(*edit_data);
    }
  }

  mesh_dst->mat = (Material **)MEM_dupallocN(mesh_src->mat);

  BKE_defgroup_copy_list(&mesh_dst->vertex_group_names, &mesh_src->vertex_group_names);
  mesh_dst->active_color_attribute = static_cast<char *>(
      MEM_dupallocN(mesh_src->active_color_attribute));
  mesh_dst->default_color_attribute = static_cast<char *>(
      MEM_dupallocN(mesh_src->default_color_attribute));
  mesh_dst->active_uv_map_attribute = static_cast<char *>(
      MEM_dupallocN(mesh_src->active_uv_map_attribute));
  mesh_dst->default_uv_map_attribute = static_cast<char *>(
      MEM_dupallocN(mesh_src->default_uv_map_attribute));

  CustomData_init_from(
      &mesh_src->vert_data, &mesh_dst->vert_data, mask.vmask, mesh_dst->verts_num);
  CustomData_init_from(
      &mesh_src->edge_data, &mesh_dst->edge_data, mask.emask, mesh_dst->edges_num);
  CustomData_init_from(
      &mesh_src->corner_data, &mesh_dst->corner_data, mask.lmask, mesh_dst->corners_num);
  CustomData_init_from(
      &mesh_src->face_data, &mesh_dst->face_data, mask.pmask, mesh_dst->faces_num);
  new (&mesh_dst->attribute_storage.wrap())
      blender::bke::AttributeStorage(mesh_src->attribute_storage.wrap());
  blender::implicit_sharing::copy_shared_pointer(mesh_src->face_offset_indices,
                                                 mesh_src->runtime->face_offsets_sharing_info,
                                                 &mesh_dst->face_offset_indices,
                                                 &mesh_dst->runtime->face_offsets_sharing_info);
  if (do_tessface) {
    CustomData_init_from(
        &mesh_src->fdata_legacy, &mesh_dst->fdata_legacy, mask.fmask, mesh_dst->totface_legacy);
  }
  else {
    mesh_tessface_clear_intern(mesh_dst, false);
  }

  mesh_dst->mselect = (MSelect *)MEM_dupallocN(mesh_dst->mselect);

  if (mesh_src->key && (flag & LIB_ID_COPY_SHAPEKEY)) {
    BKE_id_copy_in_lib(bmain,
                       owner_library,
                       &mesh_src->key->id,
                       &mesh_dst->id,
                       reinterpret_cast<ID **>(&mesh_dst->key),
                       flag);
  }
}

static void mesh_free_data(ID *id)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);

  CustomData_free(&mesh->vert_data);
  CustomData_free(&mesh->edge_data);
  CustomData_free(&mesh->fdata_legacy);
  CustomData_free(&mesh->corner_data);
  CustomData_free(&mesh->face_data);
  BLI_freelistN(&mesh->vertex_group_names);
  MEM_SAFE_FREE(mesh->active_color_attribute);
  MEM_SAFE_FREE(mesh->default_color_attribute);
  MEM_SAFE_FREE(mesh->active_uv_map_attribute);
  MEM_SAFE_FREE(mesh->default_uv_map_attribute);
  mesh->attribute_storage.wrap().~AttributeStorage();
  if (mesh->face_offset_indices) {
    blender::implicit_sharing::free_shared_data(&mesh->face_offset_indices,
                                                &mesh->runtime->face_offsets_sharing_info);
  }
  MEM_SAFE_FREE(mesh->mselect);
  MEM_SAFE_FREE(mesh->mat);
  delete mesh->runtime;
}

static void mesh_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, mesh->texcomesh, IDWALK_CB_NEVER_SELF);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, mesh->key, IDWALK_CB_USER);
  for (int i = 0; i < mesh->totcol; i++) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, mesh->mat[i], IDWALK_CB_USER);
  }
}

static void mesh_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);
  if (mesh->corner_data.external) {
    BKE_bpath_foreach_path_fixed_process(bpath_data,
                                         mesh->corner_data.external->filepath,
                                         sizeof(mesh->corner_data.external->filepath));
  }
}

static void mesh_foreach_working_space_color(ID *id, const IDTypeForeachColorFunctionCallback &fn)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);
#if 0
  /* In the future we'll be able to use just this. */
  mesh->attribute_storage.wrap().foreach_working_space_color(fn);
#else
  auto convert_domain = [&fn](CustomData *customdata, size_t size) {
    for (int i = 0; i < customdata->totlayer; i++) {
      CustomDataLayer *layer = &customdata->layers[i];
      if (layer->data && layer->type == CD_PROP_COLOR) {
        fn.implicit_sharing_array(
            *reinterpret_cast<blender::ImplicitSharingPtr<> *>(&layer->sharing_info),
            reinterpret_cast<blender::ColorGeometry4f *&>(layer->data),
            size);
      }
    }
  };

  convert_domain(&mesh->vert_data, mesh->verts_num);
  convert_domain(&mesh->edge_data, mesh->edges_num);
  convert_domain(&mesh->face_data, mesh->faces_num);
  convert_domain(&mesh->corner_data, mesh->corners_num);
#endif
}

static void mesh_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  using namespace blender;
  using namespace blender::bke;
  Mesh *mesh = reinterpret_cast<Mesh *>(id);
  const bool is_undo = BLO_write_is_undo(writer);

  ResourceScope scope;
  Vector<CustomDataLayer, 16> vert_layers;
  Vector<CustomDataLayer, 16> edge_layers;
  Vector<CustomDataLayer, 16> loop_layers;
  Vector<CustomDataLayer, 16> face_layers;
  bke::AttributeStorage::BlendWriteData attribute_data{scope};

  /* Cache only - don't write. */
  mesh->mface = nullptr;
  mesh->totface_legacy = 0;
  mesh->fdata_legacy = CustomData{};

  /* Convert from the format still used at runtime (flags on #CustomDataLayer) to the format
   * reserved for future runtime use (names stored on #Mesh). */
  if (const char *name = CustomData_get_active_layer_name(&mesh->corner_data, CD_PROP_FLOAT2)) {
    mesh->active_uv_map_attribute = const_cast<char *>(
        scope.allocator().copy_string(name).c_str());
  }
  else {
    mesh->active_uv_map_attribute = nullptr;
  }
  if (const char *name = CustomData_get_render_layer_name(&mesh->corner_data, CD_PROP_FLOAT2)) {
    mesh->default_uv_map_attribute = const_cast<char *>(
        scope.allocator().copy_string(name).c_str());
  }
  else {
    mesh->default_uv_map_attribute = nullptr;
  }

  /* Do not store actual geometry data in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(mesh) && !is_undo) {
    mesh->verts_num = 0;
    mesh->vert_data = CustomData{};

    mesh->edges_num = 0;
    mesh->edge_data = CustomData{};

    mesh->corners_num = 0;
    mesh->corner_data = CustomData{};

    mesh->faces_num = 0;
    mesh->face_data = CustomData{};
    mesh->face_offset_indices = nullptr;
  }
  else {
    attribute_storage_blend_write_prepare(mesh->attribute_storage.wrap(), attribute_data);
    CustomData_blend_write_prepare(
        mesh->vert_data, AttrDomain::Point, mesh->verts_num, vert_layers, attribute_data);
    CustomData_blend_write_prepare(
        mesh->edge_data, AttrDomain::Edge, mesh->edges_num, edge_layers, attribute_data);
    CustomData_blend_write_prepare(
        mesh->face_data, AttrDomain::Face, mesh->faces_num, face_layers, attribute_data);
    CustomData_blend_write_prepare(
        mesh->corner_data, AttrDomain::Corner, mesh->corners_num, loop_layers, attribute_data);
    if (!is_undo) {
      mesh_freestyle_marks_to_legacy(
          attribute_data, mesh->edge_data, mesh->face_data, edge_layers, face_layers);
    }
    if (attribute_data.attributes.is_empty()) {
      mesh->attribute_storage.dna_attributes = nullptr;
      mesh->attribute_storage.dna_attributes_num = 0;
    }
    else {
      mesh->attribute_storage.dna_attributes = attribute_data.attributes.data();
      mesh->attribute_storage.dna_attributes_num = attribute_data.attributes.size();
    }
  }

  const blender::bke::MeshRuntime *mesh_runtime = mesh->runtime;
  mesh->runtime = nullptr;

  BLO_write_shared_tag(writer, mesh->face_offset_indices);

  BLO_write_id_struct(writer, Mesh, id_address, &mesh->id);
  BKE_id_blend_write(writer, &mesh->id);

  BKE_defbase_blend_write(writer, &mesh->vertex_group_names);
  BLO_write_string(writer, mesh->active_color_attribute);
  BLO_write_string(writer, mesh->default_color_attribute);
  BLO_write_string(writer, mesh->active_uv_map_attribute);
  BLO_write_string(writer, mesh->default_uv_map_attribute);

  BLO_write_pointer_array(writer, mesh->totcol, mesh->mat);
  BLO_write_struct_array(writer, MSelect, mesh->totselect, mesh->mselect);

  CustomData_blend_write(
      writer, &mesh->vert_data, vert_layers, mesh->verts_num, CD_MASK_MESH.vmask, &mesh->id);
  CustomData_blend_write(
      writer, &mesh->edge_data, edge_layers, mesh->edges_num, CD_MASK_MESH.emask, &mesh->id);
  /* `fdata` is cleared above but written so slots align. */
  CustomData_blend_write(
      writer, &mesh->fdata_legacy, {}, mesh->totface_legacy, CD_MASK_MESH.fmask, &mesh->id);
  CustomData_blend_write(
      writer, &mesh->corner_data, loop_layers, mesh->corners_num, CD_MASK_MESH.lmask, &mesh->id);
  CustomData_blend_write(
      writer, &mesh->face_data, face_layers, mesh->faces_num, CD_MASK_MESH.pmask, &mesh->id);

  mesh->attribute_storage.wrap().blend_write(*writer, attribute_data);

  if (mesh->face_offset_indices) {
    BLO_write_shared(
        writer,
        mesh->face_offset_indices,
        sizeof(int) * mesh->faces_num,
        mesh_runtime->face_offsets_sharing_info,
        [&]() { BLO_write_int32_array(writer, mesh->faces_num + 1, mesh->face_offset_indices); });
  }
}

static void mesh_blend_read_data(BlendDataReader *reader, ID *id)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);
  BLO_read_pointer_array(reader, mesh->totcol, (void **)&mesh->mat);
  /* This check added for python created meshes. */
  if (!mesh->mat) {
    mesh->totcol = 0;
  }

  /* Deprecated pointers to custom data layers are read here for backward compatibility
   * with files where these were owning pointers rather than a view into custom data. */
  BLO_read_struct_array(reader, MVert, mesh->verts_num, &mesh->mvert);
  BLO_read_struct_array(reader, MEdge, mesh->edges_num, &mesh->medge);
  BLO_read_struct_array(reader, MFace, mesh->totface_legacy, &mesh->mface);
  BLO_read_struct_array(reader, MTFace, mesh->totface_legacy, &mesh->mtface);
  BLO_read_struct_array(reader, MDeformVert, mesh->verts_num, &mesh->dvert);
  BLO_read_struct_array(reader, TFace, mesh->totface_legacy, &mesh->tface);
  BLO_read_struct_array(reader, MCol, mesh->totface_legacy, &mesh->mcol);

  BLO_read_struct_array(reader, MSelect, mesh->totselect, &mesh->mselect);

  BLO_read_struct_list(reader, bDeformGroup, &mesh->vertex_group_names);

  CustomData_blend_read(reader, &mesh->vert_data, mesh->verts_num);
  CustomData_blend_read(reader, &mesh->edge_data, mesh->edges_num);
  CustomData_blend_read(reader, &mesh->fdata_legacy, mesh->totface_legacy);
  CustomData_blend_read(reader, &mesh->corner_data, mesh->corners_num);
  CustomData_blend_read(reader, &mesh->face_data, mesh->faces_num);
  mesh->attribute_storage.wrap().blend_read(*reader);
  if (mesh->deform_verts().is_empty()) {
    /* Vertex group data was also an owning pointer in old Blender versions.
     * Don't read them again if they were read as part of #CustomData. */
    BKE_defvert_blend_read(reader, mesh->verts_num, mesh->dvert);
  }
  BLO_read_string(reader, &mesh->active_color_attribute);
  BLO_read_string(reader, &mesh->default_color_attribute);
  BLO_read_string(reader, &mesh->active_uv_map_attribute);
  BLO_read_string(reader, &mesh->default_uv_map_attribute);

  /* Forward compatibility. To be removed when runtime format changes. */
  blender::bke::mesh_convert_storage_to_customdata(*mesh);

  mesh->texspace_flag &= ~ME_TEXSPACE_FLAG_AUTO_EVALUATED;

  mesh->runtime = new blender::bke::MeshRuntime();

  if (mesh->face_offset_indices) {
    mesh->runtime->face_offsets_sharing_info = BLO_read_shared(
        reader, &mesh->face_offset_indices, [&]() {
          BLO_read_int32_array(reader, mesh->faces_num + 1, &mesh->face_offset_indices);
          return blender::implicit_sharing::info_for_mem_free(mesh->face_offset_indices);
        });
  }

  if (mesh->mselect == nullptr) {
    mesh->totselect = 0;
  }

  /* NOTE: this is endianness-sensitive. */
  /* Each legacy TFace would need to undo the automatic DNA switch of its array of four uint32_t
   * RGBA colors. */
}

IDTypeInfo IDType_ID_ME = {
    /*id_code*/ Mesh::id_type,
    /*id_filter*/ FILTER_ID_ME,
    /*dependencies_id_types*/ FILTER_ID_ME | FILTER_ID_MA | FILTER_ID_IM | FILTER_ID_KE,
    /*main_listbase_index*/ INDEX_ID_ME,
    /*struct_size*/ sizeof(Mesh),
    /*name*/ "Mesh",
    /*name_plural*/ N_("meshes"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_MESH,
    /*flags*/ IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /*init_data*/ mesh_init_data,
    /*copy_data*/ mesh_copy_data,
    /*free_data*/ mesh_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ mesh_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ mesh_foreach_path,
    /*foreach_working_space_color*/ mesh_foreach_working_space_color,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ mesh_blend_write,
    /*blend_read_data*/ mesh_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

bool BKE_mesh_attribute_required(const StringRef name)
{
  return ELEM(name, "position", ".corner_vert", ".corner_edge", ".edge_verts");
}

void BKE_mesh_ensure_skin_customdata(Mesh *mesh)
{
  BMesh *bm = mesh->runtime->edit_mesh ? mesh->runtime->edit_mesh->bm : nullptr;
  MVertSkin *vs;

  if (bm) {
    if (!CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
      BMVert *v;
      BMIter iter;

      BM_data_layer_add(bm, &bm->vdata, CD_MVERT_SKIN);

      /* Mark an arbitrary vertex as root */
      BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
        vs = (MVertSkin *)CustomData_bmesh_get(&bm->vdata, v->head.data, CD_MVERT_SKIN);
        vs->flag |= MVERT_SKIN_ROOT;
        break;
      }
    }
  }
  else {
    if (!CustomData_has_layer(&mesh->vert_data, CD_MVERT_SKIN)) {
      vs = (MVertSkin *)CustomData_add_layer(
          &mesh->vert_data, CD_MVERT_SKIN, CD_SET_DEFAULT, mesh->verts_num);

      /* Mark an arbitrary vertex as root */
      if (vs) {
        vs->flag |= MVERT_SKIN_ROOT;
      }
    }
  }
}

bool BKE_mesh_has_custom_loop_normals(Mesh *mesh)
{
  if (mesh->runtime->edit_mesh) {
    return CustomData_has_layer_named(
        &mesh->runtime->edit_mesh->bm->ldata, CD_PROP_INT16_2D, "custom_normal");
  }

  return mesh->attributes().contains("custom_normal");
}

namespace blender::bke {

void mesh_ensure_default_color_attribute_on_add(Mesh &mesh,
                                                const StringRef id,
                                                AttrDomain domain,
                                                bke::AttrType data_type)
{
  if (bke::attribute_name_is_anonymous(id)) {
    return;
  }
  if (!mesh::is_color_attribute({domain, data_type})) {
    return;
  }
  if (mesh.default_color_attribute) {
    return;
  }
  mesh.default_color_attribute = BLI_strdupn(id.data(), id.size());
}

void mesh_ensure_required_data_layers(Mesh &mesh)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  AttributeInitConstruct attribute_init;

  /* Try to create attributes if they do not exist. */
  attributes.add("position", AttrDomain::Point, bke::AttrType::Float3, attribute_init);
  attributes.add(".edge_verts", AttrDomain::Edge, bke::AttrType::Int32_2D, attribute_init);
  attributes.add(".corner_vert", AttrDomain::Corner, bke::AttrType::Int32, attribute_init);
  attributes.add(".corner_edge", AttrDomain::Corner, bke::AttrType::Int32, attribute_init);
}

void mesh_remove_invalid_attribute_strings(Mesh &mesh)
{
  bke::AttributeAccessor attributes = mesh.attributes();
  if (!mesh::is_color_attribute(attributes.lookup_meta_data(mesh.active_color_attribute))) {
    MEM_SAFE_FREE(mesh.active_color_attribute);
  }
  if (!mesh::is_color_attribute(attributes.lookup_meta_data(mesh.default_color_attribute))) {
    MEM_SAFE_FREE(mesh.default_color_attribute);
  }
}

static Bounds<float3> merge_bounds(const Bounds<float3> &a, const Bounds<float3> &b)
{
  return bounds::merge(a, b);
}

static Bounds<float3> negative_bounds()
{
  return {float3(std::numeric_limits<float>::max()), float3(std::numeric_limits<float>::lowest())};
}

struct NonContiguousGroup {
  Array<int> unique_verts;
  Array<int> faces;
  Array<int> shared_verts;
  int corner_count;
  int parent;
  int children_offset;
};

static void partition_faces_recursively(const Span<float3> face_centers,
                                        MutableSpan<int> face_indices,
                                        Vector<NonContiguousGroup> &groups,
                                        int node_index,
                                        int depth,
                                        const std::optional<Bounds<float3>> &bounds_precalc,
                                        const Span<int> material_indices,
                                        int target_group_size)
{
  if (face_indices.size() <= target_group_size || depth >= STACK_FIXED_DEPTH - 1) {
    if (!blender::bke::pbvh::leaf_needs_material_split(face_indices, material_indices)) {
      groups[node_index].children_offset = 0;
      groups[node_index].faces = Array<int>(face_indices.size(), NoInitialization());
      std::copy(face_indices.begin(), face_indices.end(), groups[node_index].faces.begin());
      return;
    }
  }

  const int children_start = groups.size();
  groups[node_index].children_offset = children_start;

  groups.resize(groups.size() + 2);
  groups[children_start].parent = node_index;
  groups[children_start + 1].parent = node_index;

  int split;
  if (!(face_indices.size() <= target_group_size || depth >= STACK_FIXED_DEPTH - 1)) {
    Bounds<float3> bounds;
    if (bounds_precalc) {
      bounds = *bounds_precalc;
    }
    else {
      bounds = threading::parallel_reduce(
          face_indices.index_range(),
          1024,
          negative_bounds(),
          [&](const IndexRange range, Bounds<float3> value) {
            for (const int face : face_indices.slice(range)) {
              math::min_max(face_centers[face], value.min, value.max);
            }
            return value;
          },
          merge_bounds);
    }
    const int axis = math::dominant_axis(bounds.max - bounds.min);

    split = blender::bke::pbvh::partition_along_axis(
        face_centers, face_indices, axis, math::midpoint(bounds.min[axis], bounds.max[axis]));
  }
  else {
    split = blender::bke::pbvh::partition_material_indices(material_indices, face_indices);
  }

  partition_faces_recursively(face_centers,
                              face_indices.take_front(split),
                              groups,
                              children_start,
                              depth + 1,
                              std::nullopt,
                              material_indices,
                              target_group_size);
  partition_faces_recursively(face_centers,
                              face_indices.drop_front(split),
                              groups,
                              children_start + 1,
                              depth + 1,
                              std::nullopt,
                              material_indices,
                              target_group_size);
}

static void build_vertex_groups_for_leaves(const int verts_num,
                                           const OffsetIndices<int> faces,
                                           const Span<int> corner_verts,
                                           Vector<NonContiguousGroup> &groups)
{
  Vector<int> leaf_indices;
  for (const int i : groups.index_range()) {
    if (groups[i].children_offset == 0 && !groups[i].faces.is_empty()) {
      leaf_indices.append(i);
    }
  }

  Array<Array<int>> verts_per_leaf(leaf_indices.size(), NoInitialization());

  threading::parallel_for(leaf_indices.index_range(), 8, [&](const IndexRange range) {
    Set<int> verts;
    for (const int i : range) {
      const int group_idx = leaf_indices[i];
      NonContiguousGroup &group = groups[group_idx];
      verts.clear();
      int corners_count = 0;

      for (const int face_index : group.faces) {
        const IndexRange face = faces[face_index];
        verts.add_multiple(corner_verts.slice(face));
        corners_count += face.size();
      }

      new (&verts_per_leaf[i]) Array<int>(verts.size());
      std::copy(verts.begin(), verts.end(), verts_per_leaf[i].begin());
      std::sort(verts_per_leaf[i].begin(), verts_per_leaf[i].end());
      group.corner_count = corners_count;
    }
  });

  Vector<int> owned_verts;
  Vector<int> shared_verts;
  BitVector<> vert_used(verts_num);

  for (const int i : leaf_indices.index_range()) {
    const int group_idx = leaf_indices[i];
    NonContiguousGroup &group = groups[group_idx];
    owned_verts.clear();
    shared_verts.clear();

    for (const int vert : verts_per_leaf[i]) {
      if (vert_used[vert]) {
        shared_verts.append(vert);
      }
      else {
        vert_used[vert].set();
        owned_verts.append(vert);
      }
    }

    if (!owned_verts.is_empty()) {
      group.unique_verts = Array<int>(owned_verts.size());
      std::copy(owned_verts.begin(), owned_verts.end(), group.unique_verts.begin());
    }

    if (!shared_verts.is_empty()) {
      group.shared_verts = Array<int>(shared_verts.size());
      std::copy(shared_verts.begin(), shared_verts.end(), group.shared_verts.begin());
    }
  }
}

static Vector<NonContiguousGroup> compute_local_mesh_groups(Mesh &mesh)
{
  const Span<float3> vert_positions = mesh.vert_positions();
  const OffsetIndices<int> faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  if (faces.is_empty()) {
    return {};
  }

  Array<float3> face_centers(faces.size());
  const Bounds<float3> bounds = threading::parallel_reduce(
      faces.index_range(),
      1024,
      negative_bounds(),
      [&](const IndexRange range, const Bounds<float3> &init) {
        Bounds<float3> current = init;
        for (const int face : range) {
          const Bounds<float3> bounds = blender::bke::pbvh::calc_face_bounds(
              vert_positions, corner_verts.slice(faces[face]));
          face_centers[face] = bounds.center();
          current = bounds::merge(current, bounds);
        }
        return current;
      },
      merge_bounds);

  Array<int> prim_face_indices(mesh.faces_num);
  array_utils::fill_index_range<int>(prim_face_indices);

  Vector<NonContiguousGroup> groups;
  groups.resize(1);
  groups[0].parent = -1;
  groups[0].children_offset = 0;

  const AttributeAccessor attributes = mesh.attributes();
  const VArraySpan material_index = *attributes.lookup<int>("material_index", AttrDomain::Face);

  partition_faces_recursively(
      face_centers, prim_face_indices, groups, 0, 0, bounds, material_index, 2500);

  build_vertex_groups_for_leaves(mesh.verts_num, faces, corner_verts, groups);

  return groups;
}

void mesh_apply_spatial_organization(Mesh &mesh)
{
  Vector<NonContiguousGroup> local_groups = compute_local_mesh_groups(mesh);

  Vector<int> new_vert_order;
  new_vert_order.reserve(mesh.verts_num);

  Vector<int> new_face_order;
  new_face_order.reserve(mesh.faces_num);

  BitVector<> added_verts(mesh.verts_num, false);

  Vector<int> group_unique_offsets;
  group_unique_offsets.reserve(local_groups.size() + 1);
  group_unique_offsets.append(0);

  Vector<int> group_face_offsets;
  group_face_offsets.reserve(local_groups.size() + 1);
  group_face_offsets.append(0);

  for (const int group_index : local_groups.index_range()) {
    const NonContiguousGroup &local_group = local_groups[group_index];

    for (const int vert_idx : local_group.unique_verts) {
      if (!added_verts[vert_idx]) {
        new_vert_order.append(vert_idx);
        added_verts[vert_idx].set();
      }
    }
    group_unique_offsets.append(new_vert_order.size());

    for (const int vert_idx : local_group.shared_verts) {
      if (!added_verts[vert_idx]) {
        new_vert_order.append(vert_idx);
        added_verts[vert_idx].set();
      }
    }

    for (const int face_idx : local_group.faces) {
      new_face_order.append(face_idx);
    }
    group_face_offsets.append(new_face_order.size());
  }

  for (const int vert : IndexRange(mesh.verts_num)) {
    if (!added_verts[vert]) {
      new_vert_order.append(vert);
      added_verts[vert].set();
    }
  }

  Array<int> vert_reverse_map(mesh.verts_num);
  for (const int i : IndexRange(mesh.verts_num)) {
    vert_reverse_map[new_vert_order[i]] = i;
  }

  MutableSpan edges = mesh.edges_for_write();
  for (int2 &edge : edges) {
    edge.x = vert_reverse_map[edge.x];
    edge.y = vert_reverse_map[edge.y];
  }

  MutableSpan<int> corner_verts = mesh.corner_verts_for_write();
  Array<int> new_corner_verts(corner_verts.size());
  const OffsetIndices<int> old_faces = mesh.faces();

  int new_corner_idx = 0;
  for (const int old_face_idx : new_face_order) {
    const IndexRange face = old_faces[old_face_idx];
    for (const int corner : face) {
      new_corner_verts[new_corner_idx] = vert_reverse_map[corner_verts[corner]];
      new_corner_idx++;
    }
  }
  corner_verts.copy_from(new_corner_verts);

  MutableSpan<int> face_offsets = mesh.face_offsets_for_write();
  Vector<int> face_sizes(new_face_order.size());
  gather_group_sizes(old_faces, new_face_order, face_sizes);
  face_offsets.take_front(face_sizes.size()).copy_from(face_sizes);
  offset_indices::accumulate_counts_to_offsets(face_offsets);

  MutableAttributeAccessor attributes_for_write = mesh.attributes_for_write();
  attributes_for_write.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.domain == bke::AttrDomain::Face) {
      bke::GSpanAttributeWriter attribute = attributes_for_write.lookup_for_write_span(iter.name);
      const CPPType &type = attribute.span.type();
      GArray<> new_values(type, new_face_order.size());
      bke::attribute_math::gather(attribute.span, new_face_order, new_values.as_mutable_span());
      attribute.span.copy_from(new_values.as_span());
      attribute.finish();
    }
    else if (iter.domain == bke::AttrDomain::Point) {
      bke::GSpanAttributeWriter attribute = attributes_for_write.lookup_for_write_span(iter.name);
      const CPPType &type = attribute.span.type();
      GArray<> new_values(type, new_vert_order.size());
      bke::attribute_math::gather(attribute.span, new_vert_order, new_values.as_mutable_span());
      attribute.span.copy_from(new_values.as_span());
      attribute.finish();
    }
    else if (iter.domain == bke::AttrDomain::Corner && iter.name != ".corner_vert") {
      bke::GSpanAttributeWriter attribute = attributes_for_write.lookup_for_write_span(iter.name);
      GMutableSpan attribute_data = attribute.span;
      const CPPType &type = attribute_data.type();
      GArray<> new_values(type, attribute_data.size());

      int new_corner_idx = 0;
      for (const int old_face_idx : new_face_order) {
        const IndexRange face = old_faces[old_face_idx];
        for (const int old_corner_idx : face) {
          type.copy_construct(attribute_data[old_corner_idx], new_values[new_corner_idx]);
          new_corner_idx++;
        }
      }
      attribute_data.copy_from(new_values.as_span());
      attribute.finish();
    }
  });

  for (NonContiguousGroup &local_group : local_groups) {
    for (int &vert_idx : local_group.unique_verts) {
      vert_idx = vert_reverse_map[vert_idx];
    }
    for (int &vert_idx : local_group.shared_verts) {
      vert_idx = vert_reverse_map[vert_idx];
    }
  }

  Array<MeshGroup> nodes(local_groups.size());

  for (const int node_idx : local_groups.index_range()) {
    const NonContiguousGroup &local_group = local_groups[node_idx];
    MeshGroup &node = nodes[node_idx];

    node.parent = local_group.parent;
    node.children_offset = local_group.children_offset;
    node.corners_count = local_group.corner_count;
    node.unique_verts = IndexRange(0, 0);
    node.faces = IndexRange(0, 0);
    if (local_group.children_offset == 0 && !local_group.faces.is_empty()) {
      int unique_start = (node_idx == 0) ? 0 : group_unique_offsets[node_idx];
      int unique_end = group_unique_offsets[node_idx + 1];
      node.unique_verts = IndexRange(unique_start, unique_end - unique_start);

      int face_start = (node_idx == 0) ? 0 : group_face_offsets[node_idx];
      int face_end = group_face_offsets[node_idx + 1];
      node.faces = IndexRange(face_start, face_end - face_start);

      if (!local_group.shared_verts.is_empty()) {
        node.shared_verts = Array<int>(local_group.shared_verts.size());
        for (const int j : local_group.shared_verts.index_range()) {
          node.shared_verts[j] = local_group.shared_verts[j];
        }
      }
    }
  }

  mesh.tag_positions_changed();
  mesh.tag_topology_changed();
  mesh.runtime->spatial_groups = std::make_unique<Array<MeshGroup>>(std::move(nodes));
}

}  // namespace blender::bke

/**
 * \note on data that this function intentionally doesn't free:
 *
 * - Materials and shape keys are not freed here (#Mesh.mat & #Mesh.key).
 *   As freeing shape keys requires tagging the depsgraph for updated relations,
 *   which is expensive.
 *   Material slots should be kept in sync with the object.
 *
 * - Edit-Mesh (#Mesh.edit_mesh)
 *   Since edit-mesh is tied to the object's mode, which crashes when called in edit-mode.
 *   See: #90972.
 */
static void mesh_clear_geometry(Mesh &mesh)
{
  CustomData_free(&mesh.vert_data);
  CustomData_free(&mesh.edge_data);
  CustomData_free(&mesh.fdata_legacy);
  CustomData_free(&mesh.corner_data);
  CustomData_free(&mesh.face_data);
  mesh.attribute_storage.wrap() = blender::bke::AttributeStorage();
  if (mesh.face_offset_indices) {
    blender::implicit_sharing::free_shared_data(&mesh.face_offset_indices,
                                                &mesh.runtime->face_offsets_sharing_info);
  }
  MEM_SAFE_FREE(mesh.mselect);

  mesh.verts_num = 0;
  mesh.edges_num = 0;
  mesh.totface_legacy = 0;
  mesh.corners_num = 0;
  mesh.faces_num = 0;
  mesh.act_face = -1;
  mesh.totselect = 0;
}

static void clear_attribute_names(Mesh &mesh)
{
  BLI_freelistN(&mesh.vertex_group_names);
  MEM_SAFE_FREE(mesh.active_color_attribute);
  MEM_SAFE_FREE(mesh.default_color_attribute);
}

void BKE_mesh_clear_geometry(Mesh *mesh)
{
  BKE_mesh_runtime_clear_cache(mesh);
  mesh_clear_geometry(*mesh);
}

void BKE_mesh_clear_geometry_and_metadata(Mesh *mesh)
{
  BKE_mesh_runtime_clear_cache(mesh);
  mesh_clear_geometry(*mesh);
  clear_attribute_names(*mesh);
}

static void mesh_tessface_clear_intern(Mesh *mesh, int free_customdata)
{
  if (free_customdata) {
    CustomData_free(&mesh->fdata_legacy);
  }
  else {
    CustomData_reset(&mesh->fdata_legacy);
  }

  mesh->totface_legacy = 0;
}

Mesh *BKE_mesh_add(Main *bmain, const char *name)
{
  return BKE_id_new<Mesh>(bmain, name);
}

void BKE_mesh_face_offsets_ensure_alloc(Mesh *mesh)
{
  BLI_assert(mesh->face_offset_indices == nullptr);
  BLI_assert(mesh->runtime->face_offsets_sharing_info == nullptr);
  if (mesh->faces_num == 0) {
    return;
  }
  mesh->face_offset_indices = MEM_malloc_arrayN<int>(size_t(mesh->faces_num) + 1, __func__);
  mesh->runtime->face_offsets_sharing_info = blender::implicit_sharing::info_for_mem_free(
      mesh->face_offset_indices);

#ifndef NDEBUG
  /* Fill offsets with obviously bad values to simplify finding missing initialization. */
  mesh->face_offsets_for_write().fill(-1);
#endif
  /* Set common values for convenience. */
  mesh->face_offset_indices[0] = 0;
  mesh->face_offset_indices[mesh->faces_num] = mesh->corners_num;
}

Span<float3> Mesh::vert_positions() const
{
  return {static_cast<const float3 *>(
              CustomData_get_layer_named(&this->vert_data, CD_PROP_FLOAT3, "position")),
          this->verts_num};
}
MutableSpan<float3> Mesh::vert_positions_for_write()
{
  return {static_cast<float3 *>(CustomData_get_layer_named_for_write(
              &this->vert_data, CD_PROP_FLOAT3, "position", this->verts_num)),
          this->verts_num};
}

Span<int2> Mesh::edges() const
{
  return {static_cast<const int2 *>(
              CustomData_get_layer_named(&this->edge_data, CD_PROP_INT32_2D, ".edge_verts")),
          this->edges_num};
}
MutableSpan<int2> Mesh::edges_for_write()
{
  return {static_cast<int2 *>(CustomData_get_layer_named_for_write(
              &this->edge_data, CD_PROP_INT32_2D, ".edge_verts", this->edges_num)),
          this->edges_num};
}

OffsetIndices<int> Mesh::faces() const
{
  return Span(this->face_offset_indices, this->faces_num + 1);
}
Span<int> Mesh::face_offsets() const
{
  if (this->faces_num == 0) {
    return {};
  }
  return {this->face_offset_indices, this->faces_num + 1};
}
MutableSpan<int> Mesh::face_offsets_for_write()
{
  if (this->faces_num == 0) {
    return {};
  }
  blender::implicit_sharing::make_trivial_data_mutable(
      &this->face_offset_indices, &this->runtime->face_offsets_sharing_info, this->faces_num + 1);
  return {this->face_offset_indices, this->faces_num + 1};
}

Span<int> Mesh::corner_verts() const
{
  return {static_cast<const int *>(
              CustomData_get_layer_named(&this->corner_data, CD_PROP_INT32, ".corner_vert")),
          this->corners_num};
}
MutableSpan<int> Mesh::corner_verts_for_write()
{
  return {static_cast<int *>(CustomData_get_layer_named_for_write(
              &this->corner_data, CD_PROP_INT32, ".corner_vert", this->corners_num)),
          this->corners_num};
}

Span<int> Mesh::corner_edges() const
{
  return {static_cast<const int *>(
              CustomData_get_layer_named(&this->corner_data, CD_PROP_INT32, ".corner_edge")),
          this->corners_num};
}
MutableSpan<int> Mesh::corner_edges_for_write()
{
  return {static_cast<int *>(CustomData_get_layer_named_for_write(
              &this->corner_data, CD_PROP_INT32, ".corner_edge", this->corners_num)),
          this->corners_num};
}

Span<MDeformVert> Mesh::deform_verts() const
{
  const MDeformVert *dverts = static_cast<const MDeformVert *>(
      CustomData_get_layer(&this->vert_data, CD_MDEFORMVERT));
  if (!dverts) {
    return {};
  }
  return {dverts, this->verts_num};
}
MutableSpan<MDeformVert> Mesh::deform_verts_for_write()
{
  MDeformVert *dvert = static_cast<MDeformVert *>(
      CustomData_get_layer_for_write(&this->vert_data, CD_MDEFORMVERT, this->verts_num));
  if (dvert) {
    return {dvert, this->verts_num};
  }
  return {static_cast<MDeformVert *>(CustomData_add_layer(
              &this->vert_data, CD_MDEFORMVERT, CD_SET_DEFAULT, this->verts_num)),
          this->verts_num};
}

void Mesh::count_memory(blender::MemoryCounter &memory) const
{
  memory.add_shared(this->runtime->face_offsets_sharing_info,
                    this->face_offsets().size_in_bytes());
  CustomData_count_memory(this->vert_data, this->verts_num, memory);
  CustomData_count_memory(this->edge_data, this->edges_num, memory);
  CustomData_count_memory(this->face_data, this->faces_num, memory);
  CustomData_count_memory(this->corner_data, this->corners_num, memory);
}

blender::bke::AttributeAccessor Mesh::attributes() const
{
  return blender::bke::AttributeAccessor(this, blender::bke::mesh_attribute_accessor_functions());
}

blender::bke::MutableAttributeAccessor Mesh::attributes_for_write()
{
  return blender::bke::MutableAttributeAccessor(this,
                                                blender::bke::mesh_attribute_accessor_functions());
}

blender::VectorSet<blender::StringRefNull> Mesh::uv_map_names() const
{
  blender::VectorSet<blender::StringRefNull> names;
  this->attributes().foreach_attribute([&](const blender::bke::AttributeIter &iter) {
    if (blender::bke::mesh::is_uv_map({iter.domain, iter.data_type})) {
      names.add_new(iter.name);
    }
  });
  return names;
}

blender::StringRefNull Mesh::active_uv_map_name() const
{
  /* Currently this information is stored in CustomData. Once it switches to using
   * #Mesh::active_uv_map_attribute, this logic can be removed. This function's only purpose is to
   * ease that transition. */
  if (this->runtime->edit_mesh) {
    const char *name = CustomData_get_active_layer_name(&this->runtime->edit_mesh->bm->ldata,
                                                        CD_PROP_FLOAT2);
    return name ? name : "";
  }
  const char *name = CustomData_get_active_layer_name(&this->corner_data, CD_PROP_FLOAT2);
  return name ? name : "";
}

blender::StringRefNull Mesh::default_uv_map_name() const
{
  /* Currently this information is stored in CustomData. Once it switches to using
   * #Mesh::default_uv_map_attribute, this logic can be removed. This function's only purpose is to
   * ease that transition. */
  if (this->runtime->edit_mesh) {
    const char *name = CustomData_get_render_layer_name(&this->runtime->edit_mesh->bm->ldata,
                                                        CD_PROP_FLOAT2);
    return name ? name : "";
  }
  const char *name = CustomData_get_render_layer_name(&this->corner_data, CD_PROP_FLOAT2);
  return name ? name : "";
}

Mesh *BKE_mesh_new_nomain(const int verts_num,
                          const int edges_num,
                          const int faces_num,
                          const int corners_num)
{
  Mesh *mesh = static_cast<Mesh *>(BKE_libblock_alloc(
      nullptr, ID_ME, BKE_idtype_idcode_to_name(ID_ME), LIB_ID_CREATE_LOCALIZE));
  BKE_libblock_init_empty(&mesh->id);

  mesh->verts_num = verts_num;
  mesh->edges_num = edges_num;
  mesh->faces_num = faces_num;
  mesh->corners_num = corners_num;

  blender::bke::mesh_ensure_required_data_layers(*mesh);
  BKE_mesh_face_offsets_ensure_alloc(mesh);

  return mesh;
}

namespace blender::bke {

namespace mesh {

bool is_uv_map(const AttributeMetaData &meta_data)
{
  return meta_data.domain == AttrDomain::Corner && meta_data.data_type == AttrType::Float2;
}

bool is_uv_map(const std::optional<AttributeMetaData> &meta_data)
{
  return meta_data && is_uv_map(*meta_data);
}

bool is_color_attribute(const blender::bke::AttributeMetaData &meta_data)
{
  return ELEM(meta_data.domain,
              blender::bke::AttrDomain::Point,
              blender::bke::AttrDomain::Corner) &&
         ELEM(meta_data.data_type,
              blender::bke::AttrType::ColorByte,
              blender::bke::AttrType::ColorFloat);
}

bool is_color_attribute(const std::optional<blender::bke::AttributeMetaData> &meta_data)
{
  return meta_data && is_color_attribute(*meta_data);
}

}  // namespace mesh

Mesh *mesh_new_no_attributes(const int verts_num,
                             const int edges_num,
                             const int faces_num,
                             const int corners_num)
{
  Mesh *mesh = BKE_mesh_new_nomain(0, 0, faces_num, 0);
  mesh->verts_num = verts_num;
  mesh->edges_num = edges_num;
  mesh->corners_num = corners_num;
  CustomData_free_layer_named(&mesh->vert_data, "position");
  CustomData_free_layer_named(&mesh->edge_data, ".edge_verts");
  CustomData_free_layer_named(&mesh->corner_data, ".corner_vert");
  CustomData_free_layer_named(&mesh->corner_data, ".corner_edge");
  return mesh;
}

}  // namespace blender::bke

static void copy_attribute_names(const Mesh &mesh_src, Mesh &mesh_dst)
{
  if (mesh_src.active_color_attribute) {
    MEM_SAFE_FREE(mesh_dst.active_color_attribute);
    mesh_dst.active_color_attribute = BLI_strdup(mesh_src.active_color_attribute);
  }
  if (mesh_src.default_color_attribute) {
    MEM_SAFE_FREE(mesh_dst.default_color_attribute);
    mesh_dst.default_color_attribute = BLI_strdup(mesh_src.default_color_attribute);
  }
}

void BKE_mesh_copy_parameters(Mesh *me_dst, const Mesh *me_src)
{
  /* Copy general settings. */
  me_dst->editflag = me_src->editflag;
  me_dst->flag = me_src->flag;
  me_dst->remesh_voxel_size = me_src->remesh_voxel_size;
  me_dst->remesh_voxel_adaptivity = me_src->remesh_voxel_adaptivity;
  me_dst->remesh_mode = me_src->remesh_mode;
  me_dst->symmetry = me_src->symmetry;

  me_dst->face_sets_color_seed = me_src->face_sets_color_seed;
  me_dst->face_sets_color_default = me_src->face_sets_color_default;

  /* Copy texture space. */
  me_dst->texspace_flag = me_src->texspace_flag;
  copy_v3_v3(me_dst->texspace_location, me_src->texspace_location);
  copy_v3_v3(me_dst->texspace_size, me_src->texspace_size);

  me_dst->vertex_group_active_index = me_src->vertex_group_active_index;
  me_dst->attributes_active_index = me_src->attributes_active_index;
}

void BKE_mesh_copy_parameters_for_eval(Mesh *me_dst, const Mesh *me_src)
{
  /* User counts aren't handled, don't copy into a mesh from #G_MAIN. */
  BLI_assert(me_dst->id.tag & (ID_TAG_NO_MAIN | ID_TAG_COPIED_ON_EVAL));

  BKE_mesh_copy_parameters(me_dst, me_src);
  copy_attribute_names(*me_src, *me_dst);

  /* Copy vertex group names. */
  BLI_assert(BLI_listbase_is_empty(&me_dst->vertex_group_names));
  BKE_defgroup_copy_list(&me_dst->vertex_group_names, &me_src->vertex_group_names);

  /* Copy materials. */
  if (me_dst->mat != nullptr) {
    MEM_freeN(me_dst->mat);
  }
  me_dst->mat = (Material **)MEM_dupallocN(me_src->mat);
  me_dst->totcol = me_src->totcol;

  me_dst->runtime->edit_mesh = me_src->runtime->edit_mesh;
}

Mesh *BKE_mesh_new_nomain_from_template_ex(const Mesh *me_src,
                                           const int verts_num,
                                           const int edges_num,
                                           const int tessface_num,
                                           const int faces_num,
                                           const int corners_num,
                                           const CustomData_MeshMasks mask)
{
  /* Only do tessface if we are creating tessfaces or copying from mesh with only tessfaces. */
  const bool do_tessface = (tessface_num ||
                            ((me_src->totface_legacy != 0) && (me_src->faces_num == 0)));

  Mesh *me_dst = BKE_id_new_nomain<Mesh>(nullptr);

  me_dst->mselect = (MSelect *)MEM_dupallocN(me_src->mselect);

  me_dst->verts_num = verts_num;
  me_dst->edges_num = edges_num;
  me_dst->faces_num = faces_num;
  me_dst->corners_num = corners_num;
  me_dst->totface_legacy = tessface_num;

  BKE_mesh_copy_parameters_for_eval(me_dst, me_src);

  CustomData_init_layout_from(
      &me_src->vert_data, &me_dst->vert_data, mask.vmask, CD_SET_DEFAULT, verts_num);
  CustomData_init_layout_from(
      &me_src->edge_data, &me_dst->edge_data, mask.emask, CD_SET_DEFAULT, edges_num);
  CustomData_init_layout_from(
      &me_src->face_data, &me_dst->face_data, mask.pmask, CD_SET_DEFAULT, faces_num);
  CustomData_init_layout_from(
      &me_src->corner_data, &me_dst->corner_data, mask.lmask, CD_SET_DEFAULT, corners_num);
  if (do_tessface) {
    CustomData_init_layout_from(
        &me_src->fdata_legacy, &me_dst->fdata_legacy, mask.fmask, CD_SET_DEFAULT, tessface_num);
  }
  else {
    mesh_tessface_clear_intern(me_dst, false);
  }

  /* The destination mesh should at least have valid primary CD layers,
   * even in cases where the source mesh does not. */
  blender::bke::mesh_ensure_required_data_layers(*me_dst);
  BKE_mesh_face_offsets_ensure_alloc(me_dst);
  if (do_tessface && !CustomData_get_layer(&me_dst->fdata_legacy, CD_MFACE)) {
    CustomData_add_layer(&me_dst->fdata_legacy, CD_MFACE, CD_SET_DEFAULT, me_dst->totface_legacy);
  }

  return me_dst;
}

Mesh *BKE_mesh_new_nomain_from_template(const Mesh *me_src,
                                        const int verts_num,
                                        const int edges_num,
                                        const int faces_num,
                                        const int corners_num)
{
  return BKE_mesh_new_nomain_from_template_ex(
      me_src, verts_num, edges_num, 0, faces_num, corners_num, CD_MASK_EVERYTHING);
}

Mesh *BKE_mesh_copy_for_eval(const Mesh &source)
{
  return reinterpret_cast<Mesh *>(
      BKE_id_copy_ex(nullptr, &source.id, nullptr, LIB_ID_COPY_LOCALIZE));
}

BMesh *BKE_mesh_to_bmesh_ex(const Mesh *mesh,
                            const BMeshCreateParams *create_params,
                            const BMeshFromMeshParams *convert_params)
{
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(mesh);

  BMesh *bm = BM_mesh_create(&allocsize, create_params);
  BM_mesh_bm_from_me(bm, mesh, convert_params);

  return bm;
}

BMesh *BKE_mesh_to_bmesh(Mesh *mesh,
                         const int active_shapekey,
                         const bool add_key_index,
                         const BMeshCreateParams *params)
{
  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = false;
  bmesh_from_mesh_params.calc_vert_normal = false;
  bmesh_from_mesh_params.add_key_index = add_key_index;
  bmesh_from_mesh_params.use_shapekey = true;
  bmesh_from_mesh_params.active_shapekey = active_shapekey;
  return BKE_mesh_to_bmesh_ex(mesh, params, &bmesh_from_mesh_params);
}

Mesh *BKE_mesh_from_bmesh_nomain(BMesh *bm,
                                 const BMeshToMeshParams *params,
                                 const Mesh *me_settings)
{
  BLI_assert(params->calc_object_remap == false);
  Mesh *mesh = BKE_id_new_nomain<Mesh>(nullptr);
  BM_mesh_bm_to_me(nullptr, bm, mesh, params);
  BKE_mesh_copy_parameters_for_eval(mesh, me_settings);
  return mesh;
}

Mesh *BKE_mesh_from_bmesh_for_eval_nomain(BMesh *bm,
                                          const CustomData_MeshMasks *cd_mask_extra,
                                          const Mesh *me_settings)
{
  Mesh *mesh = BKE_id_new_nomain<Mesh>(nullptr);
  BM_mesh_bm_to_me_for_eval(*bm, *mesh, cd_mask_extra);
  BKE_mesh_copy_parameters_for_eval(mesh, me_settings);
  return mesh;
}

static void ensure_orig_index_layer(CustomData &data, const int size)
{
  if (CustomData_has_layer(&data, CD_ORIGINDEX)) {
    return;
  }
  int *indices = (int *)CustomData_add_layer(&data, CD_ORIGINDEX, CD_SET_DEFAULT, size);
  range_vn_i(indices, size, 0);
}

void BKE_mesh_ensure_default_orig_index_customdata(Mesh *mesh)
{
  BLI_assert(mesh->runtime->wrapper_type == ME_WRAPPER_TYPE_MDATA);
  BKE_mesh_ensure_default_orig_index_customdata_no_check(mesh);
}

void BKE_mesh_ensure_default_orig_index_customdata_no_check(Mesh *mesh)
{
  ensure_orig_index_layer(mesh->vert_data, mesh->verts_num);
  ensure_orig_index_layer(mesh->edge_data, mesh->edges_num);
  ensure_orig_index_layer(mesh->face_data, mesh->faces_num);
}

void BKE_mesh_texspace_calc(Mesh *mesh)
{
  using namespace blender;
  if (mesh->texspace_flag & ME_TEXSPACE_FLAG_AUTO) {
    const Bounds<float3> bounds = mesh->bounds_min_max().value_or(
        Bounds(float3(-1.0f), float3(1.0f)));

    float texspace_location[3], texspace_size[3];
    mid_v3_v3v3(texspace_location, bounds.min, bounds.max);

    texspace_size[0] = (bounds.max[0] - bounds.min[0]) / 2.0f;
    texspace_size[1] = (bounds.max[1] - bounds.min[1]) / 2.0f;
    texspace_size[2] = (bounds.max[2] - bounds.min[2]) / 2.0f;

    for (int a = 0; a < 3; a++) {
      if (texspace_size[a] == 0.0f) {
        texspace_size[a] = 1.0f;
      }
      else if (texspace_size[a] > 0.0f && texspace_size[a] < 0.00001f) {
        texspace_size[a] = 0.00001f;
      }
      else if (texspace_size[a] < 0.0f && texspace_size[a] > -0.00001f) {
        texspace_size[a] = -0.00001f;
      }
    }

    copy_v3_v3(mesh->texspace_location, texspace_location);
    copy_v3_v3(mesh->texspace_size, texspace_size);

    mesh->texspace_flag |= ME_TEXSPACE_FLAG_AUTO_EVALUATED;
  }
}

void BKE_mesh_texspace_ensure(Mesh *mesh)
{
  if ((mesh->texspace_flag & ME_TEXSPACE_FLAG_AUTO) &&
      !(mesh->texspace_flag & ME_TEXSPACE_FLAG_AUTO_EVALUATED))
  {
    BKE_mesh_texspace_calc(mesh);
  }
}

void BKE_mesh_texspace_get(Mesh *mesh, float r_texspace_location[3], float r_texspace_size[3])
{
  BKE_mesh_texspace_ensure(mesh);

  if (r_texspace_location) {
    copy_v3_v3(r_texspace_location, mesh->texspace_location);
  }
  if (r_texspace_size) {
    copy_v3_v3(r_texspace_size, mesh->texspace_size);
  }
}

void BKE_mesh_texspace_get_reference(Mesh *mesh,
                                     char **r_texspace_flag,
                                     float **r_texspace_location,
                                     float **r_texspace_size)
{
  BKE_mesh_texspace_ensure(mesh);

  if (r_texspace_flag != nullptr) {
    *r_texspace_flag = &mesh->texspace_flag;
  }
  if (r_texspace_location != nullptr) {
    *r_texspace_location = mesh->texspace_location;
  }
  if (r_texspace_size != nullptr) {
    *r_texspace_size = mesh->texspace_size;
  }
}

blender::Array<float3> BKE_mesh_orco_verts_get(const Object *ob)
{
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);
  const Mesh *tme = mesh->texcomesh ? mesh->texcomesh : mesh;

  blender::Array<float3> result(mesh->verts_num);
  const Span<float3> positions = tme->vert_positions();
  result.as_mutable_span().take_front(positions.size()).copy_from(positions);
  result.as_mutable_span().drop_front(positions.size()).fill(float3(0));

  return result;
}

void BKE_mesh_orco_verts_transform(Mesh *mesh, MutableSpan<float3> orco, const bool invert)
{
  float texspace_location[3], texspace_size[3];

  BKE_mesh_texspace_get(
      mesh->texcomesh ? mesh->texcomesh : mesh, texspace_location, texspace_size);

  if (invert) {
    for (const int a : orco.index_range()) {
      float3 &co = orco[a];
      madd_v3_v3v3v3(co, texspace_location, co, texspace_size);
    }
  }
  else {
    for (const int a : orco.index_range()) {
      float3 &co = orco[a];
      co[0] = (co[0] - texspace_location[0]) / texspace_size[0];
      co[1] = (co[1] - texspace_location[1]) / texspace_size[1];
      co[2] = (co[2] - texspace_location[2]) / texspace_size[2];
    }
  }
}

void BKE_mesh_orco_verts_transform(Mesh *mesh, float (*orco)[3], int totvert, bool invert)
{
  BKE_mesh_orco_verts_transform(mesh, {reinterpret_cast<float3 *>(orco), totvert}, invert);
}

void BKE_mesh_orco_ensure(Object *ob, Mesh *mesh)
{
  if (CustomData_has_layer(&mesh->vert_data, CD_ORCO)) {
    return;
  }

  /* Orcos are stored in normalized 0..1 range by convention. */
  blender::Array<float3> orcodata = BKE_mesh_orco_verts_get(ob);
  BKE_mesh_orco_verts_transform(mesh, orcodata, false);
  float3 *data = static_cast<float3 *>(
      CustomData_add_layer(&mesh->vert_data, CD_ORCO, CD_CONSTRUCT, mesh->verts_num));
  MutableSpan(data, mesh->verts_num).copy_from(orcodata);
}

Mesh *BKE_mesh_from_object(Object *ob)
{
  if (ob == nullptr) {
    return nullptr;
  }
  if (ob->type == OB_MESH) {
    return static_cast<Mesh *>(ob->data);
  }

  return nullptr;
}

void BKE_mesh_assign_object(Main *bmain, Object *ob, Mesh *mesh)
{
  Mesh *old = nullptr;

  if (ob == nullptr) {
    return;
  }

  multires_force_sculpt_rebuild(ob);

  if (ob->type == OB_MESH) {
    old = static_cast<Mesh *>(ob->data);
    if (old) {
      id_us_min(&old->id);
    }
    ob->data = mesh;
    id_us_plus((ID *)mesh);
  }

  BKE_object_materials_sync_length(bmain, ob, (ID *)mesh);

  BKE_modifiers_test_object(ob);
}

void BKE_mesh_material_index_remove(Mesh *mesh, short index)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  AttributeWriter<int> material_indices = attributes.lookup_for_write<int>("material_index");
  if (!material_indices) {
    return;
  }
  if (material_indices.domain != AttrDomain::Face) {
    BLI_assert_unreachable();
    return;
  }
  MutableVArraySpan<int> indices_span(material_indices.varray);
  for (const int i : indices_span.index_range()) {
    if (indices_span[i] > 0 && indices_span[i] >= index) {
      indices_span[i]--;
    }
  }
  indices_span.save();
  material_indices.finish();

  BKE_mesh_tessface_clear(mesh);
}

bool BKE_mesh_material_index_used(Mesh *mesh, short index)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor attributes = mesh->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", AttrDomain::Face, 0);
  if (material_indices.is_single()) {
    return material_indices.get_internal_single() == index;
  }
  const VArraySpan<int> indices_span(material_indices);
  return indices_span.contains(index);
}

void BKE_mesh_material_index_clear(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  attributes.remove("material_index");

  BKE_mesh_tessface_clear(mesh);
}

void BKE_mesh_material_remap(Mesh *mesh, const uint *remap, uint remap_len)
{
  using namespace blender;
  using namespace blender::bke;
  const short remap_len_short = short(remap_len);

#define MAT_NR_REMAP(n) \
  if (n < remap_len_short) { \
    BLI_assert(n >= 0 && remap[n] < remap_len_short); \
    n = remap[n]; \
  } \
  ((void)0)

  if (BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    BMIter iter;
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      MAT_NR_REMAP(efa->mat_nr);
    }
  }
  else {
    MutableAttributeAccessor attributes = mesh->attributes_for_write();
    SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
        "material_index", AttrDomain::Face);
    if (!material_indices) {
      return;
    }
    for (const int i : material_indices.span.index_range()) {
      MAT_NR_REMAP(material_indices.span[i]);
    }
    material_indices.span.save();
    material_indices.finish();
  }

#undef MAT_NR_REMAP
}

namespace blender::bke {

void mesh_smooth_set(Mesh &mesh, const bool use_smooth, const bool keep_sharp_edges)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (!keep_sharp_edges) {
    attributes.remove("sharp_edge");
  }
  attributes.remove("sharp_face");
  if (!use_smooth) {
    attributes.add<bool>("sharp_face",
                         AttrDomain::Face,
                         AttributeInitVArray(VArray<bool>::from_single(true, mesh.faces_num)));
  }
}

void mesh_sharp_edges_set_from_angle(Mesh &mesh, const float angle, const bool keep_sharp_edges)
{
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  if (angle >= M_PI) {
    mesh_smooth_set(mesh, true, keep_sharp_edges);
    return;
  }
  if (angle == 0.0f) {
    mesh_smooth_set(mesh, false, keep_sharp_edges);
    return;
  }
  if (!keep_sharp_edges) {
    attributes.remove("sharp_edge");
  }
  SpanAttributeWriter<bool> sharp_edges = attributes.lookup_or_add_for_write_span<bool>(
      "sharp_edge", AttrDomain::Edge);
  const VArraySpan<bool> sharp_faces = *attributes.lookup<bool>("sharp_face", AttrDomain::Face);
  mesh::edges_sharp_from_angle_set(mesh.faces(),
                                   mesh.corner_verts(),
                                   mesh.corner_edges(),
                                   mesh.face_normals(),
                                   mesh.corner_to_face_map(),
                                   sharp_faces,
                                   angle,
                                   sharp_edges.span);
  sharp_edges.finish();
}

}  // namespace blender::bke

std::optional<blender::Bounds<blender::float3>> Mesh::bounds_min_max() const
{
  using namespace blender;
  const int verts_num = BKE_mesh_wrapper_vert_len(this);
  if (verts_num == 0) {
    return std::nullopt;
  }
  this->runtime->bounds_cache.ensure([&](Bounds<float3> &r_bounds) {
    switch (this->runtime->wrapper_type) {
      case ME_WRAPPER_TYPE_BMESH:
        r_bounds = *BKE_editmesh_cache_calc_minmax(*this->runtime->edit_mesh,
                                                   *this->runtime->edit_data);
        break;
      case ME_WRAPPER_TYPE_MDATA:
      case ME_WRAPPER_TYPE_SUBD:
        r_bounds = *bounds::min_max(this->vert_positions());
        break;
    }
  });
  return this->runtime->bounds_cache.data();
}

void Mesh::bounds_set_eager(const blender::Bounds<float3> &bounds)
{
  this->runtime->bounds_cache.ensure([&](blender::Bounds<float3> &r_data) { r_data = bounds; });
}

static bool use_bmesh_material_indices(const Mesh &mesh)
{
  return mesh.runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH && mesh.runtime->edit_mesh &&
         mesh.runtime->edit_mesh->bm;
}

std::optional<int> Mesh::material_index_max() const
{
  this->runtime->max_material_index.ensure([&](std::optional<int> &value) {
    if (use_bmesh_material_indices(*this)) {
      BMesh *bm = this->runtime->edit_mesh->bm;
      if (bm->totface == 0) {
        value = std::nullopt;
        return;
      }
      int max_material_index = 0;
      BMFace *efa;
      BMIter iter;
      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        max_material_index = std::max<int>(max_material_index, efa->mat_nr);
      }
      value = max_material_index;
      return;
    }
    if (this->faces_num == 0) {
      value = std::nullopt;
      return;
    }
    value = blender::bounds::max<int>(
        this->attributes()
            .lookup_or_default<int>("material_index", blender::bke::AttrDomain::Face, 0)
            .varray);
    if (value.has_value()) {
      value = std::clamp(*value, 0, MAXMAT);
    }
  });
  return this->runtime->max_material_index.data();
}

const blender::VectorSet<int> &Mesh::material_indices_used() const
{
  using namespace blender;
  this->runtime->used_material_indices.ensure([&](VectorSet<int> &r_data) {
    const std::optional<int> max_material_index_opt = this->material_index_max();
    r_data.clear();
    if (!max_material_index_opt.has_value()) {
      return;
    }
    const int max_material_index = *max_material_index_opt;
    const auto clamp_material_index = [&](const int index) {
      return std::clamp<int>(index, 0, max_material_index);
    };

    /* Find used indices in parallel and then create the vector set in the end. */
    Array<bool> used_indices(max_material_index + 1, false);
    if (use_bmesh_material_indices(*this)) {
      BMesh *bm = this->runtime->edit_mesh->bm;
      BMFace *efa;
      BMIter iter;
      BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
        used_indices[clamp_material_index(efa->mat_nr)] = true;
      }
    }
    else if (const VArray<int> material_indices =
                 this->attributes()
                     .lookup_or_default<int>("material_index", bke::AttrDomain::Face, 0)
                     .varray)
    {
      if (const std::optional<int> single_material_index = material_indices.get_if_single()) {
        used_indices[clamp_material_index(*single_material_index)] = true;
      }
      else {
        VArraySpan<int> material_indices_span = material_indices;
        threading::parallel_for(
            material_indices_span.index_range(), 1024, [&](const IndexRange range) {
              for (const int i : range) {
                used_indices[clamp_material_index(material_indices_span[i])] = true;
              }
            });
      }
    }
    for (const int i : used_indices.index_range()) {
      if (used_indices[i]) {
        r_data.add_new(i);
      }
    }
  });
  return this->runtime->used_material_indices.data();
}

namespace blender::bke {

static void translate_positions(MutableSpan<float3> positions, const float3 &translation)
{
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

void mesh_translate(Mesh &mesh, const float3 &translation, const bool do_shape_keys)
{
  if (math::is_zero(translation)) {
    return;
  }

  std::optional<Bounds<float3>> bounds;
  if (mesh.runtime->bounds_cache.is_cached()) {
    bounds = mesh.runtime->bounds_cache.data();
  }

  translate_positions(mesh.vert_positions_for_write(), translation);

  if (do_shape_keys && mesh.key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &mesh.key->block) {
      translate_positions({static_cast<float3 *>(kb->data), kb->totelem}, translation);
    }
  }

  mesh.tag_positions_changed_uniformly();

  if (bounds) {
    bounds->min += translation;
    bounds->max += translation;
    mesh.bounds_set_eager(*bounds);
  }
}

void mesh_transform(Mesh &mesh, const float4x4 &transform, bool do_shape_keys)
{
  math::transform_points(transform, mesh.vert_positions_for_write());

  if (do_shape_keys && mesh.key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &mesh.key->block) {
      math::transform_points(transform, MutableSpan(static_cast<float3 *>(kb->data), kb->totelem));
    }
  }
  MutableAttributeAccessor attributes = mesh.attributes_for_write();
  transform_custom_normal_attribute(transform, attributes);

  mesh.tag_positions_changed();
}

}  // namespace blender::bke

void BKE_mesh_tessface_clear(Mesh *mesh)
{
  mesh_tessface_clear_intern(mesh, true);
}

/* -------------------------------------------------------------------- */
/* MSelect functions (currently used in weight paint mode) */

void BKE_mesh_mselect_clear(Mesh *mesh)
{
  MEM_SAFE_FREE(mesh->mselect);
  mesh->totselect = 0;
}

void BKE_mesh_mselect_validate(Mesh *mesh)
{
  using namespace blender;
  using namespace blender::bke;
  MSelect *mselect_src, *mselect_dst;
  int i_src, i_dst;

  if (mesh->totselect == 0) {
    return;
  }

  mselect_src = mesh->mselect;
  mselect_dst = MEM_malloc_arrayN<MSelect>(size_t(mesh->totselect), "Mesh selection history");

  const AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", AttrDomain::Point, false);
  const VArray<bool> select_edge = *attributes.lookup_or_default<bool>(
      ".select_edge", AttrDomain::Edge, false);
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", AttrDomain::Face, false);

  for (i_src = 0, i_dst = 0; i_src < mesh->totselect; i_src++) {
    int index = mselect_src[i_src].index;
    switch (mselect_src[i_src].type) {
      case ME_VSEL: {
        if (select_vert[index]) {
          mselect_dst[i_dst] = mselect_src[i_src];
          i_dst++;
        }
        break;
      }
      case ME_ESEL: {
        if (select_edge[index]) {
          mselect_dst[i_dst] = mselect_src[i_src];
          i_dst++;
        }
        break;
      }
      case ME_FSEL: {
        if (select_poly[index]) {
          mselect_dst[i_dst] = mselect_src[i_src];
          i_dst++;
        }
        break;
      }
      default: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  MEM_freeN(mselect_src);

  if (i_dst == 0) {
    MEM_freeN(mselect_dst);
    mselect_dst = nullptr;
  }
  else if (i_dst != mesh->totselect) {
    mselect_dst = (MSelect *)MEM_reallocN(mselect_dst, sizeof(MSelect) * i_dst);
  }

  mesh->totselect = i_dst;
  mesh->mselect = mselect_dst;
}

int BKE_mesh_mselect_find(const Mesh *mesh, int index, int type)
{
  BLI_assert(ELEM(type, ME_VSEL, ME_ESEL, ME_FSEL));

  for (int i = 0; i < mesh->totselect; i++) {
    if ((mesh->mselect[i].index == index) && (mesh->mselect[i].type == type)) {
      return i;
    }
  }

  return -1;
}

int BKE_mesh_mselect_active_get(const Mesh *mesh, int type)
{
  BLI_assert(ELEM(type, ME_VSEL, ME_ESEL, ME_FSEL));

  if (mesh->totselect) {
    if (mesh->mselect[mesh->totselect - 1].type == type) {
      return mesh->mselect[mesh->totselect - 1].index;
    }
  }
  return -1;
}

void BKE_mesh_mselect_active_set(Mesh *mesh, int index, int type)
{
  const int msel_index = BKE_mesh_mselect_find(mesh, index, type);

  if (msel_index == -1) {
    /* add to the end */
    mesh->mselect = (MSelect *)MEM_reallocN(mesh->mselect,
                                            sizeof(MSelect) * (mesh->totselect + 1));
    mesh->mselect[mesh->totselect].index = index;
    mesh->mselect[mesh->totselect].type = type;
    mesh->totselect++;
  }
  else if (msel_index != mesh->totselect - 1) {
    /* move to the end */
    std::swap(mesh->mselect[msel_index], mesh->mselect[mesh->totselect - 1]);
  }

  BLI_assert((mesh->mselect[mesh->totselect - 1].index == index) &&
             (mesh->mselect[mesh->totselect - 1].type == type));
}

void BKE_mesh_count_selected_items(const Mesh *mesh, int r_count[3])
{
  r_count[0] = r_count[1] = r_count[2] = 0;
  if (mesh->runtime->edit_mesh) {
    BMesh *bm = mesh->runtime->edit_mesh->bm;
    r_count[0] = bm->totvertsel;
    r_count[1] = bm->totedgesel;
    r_count[2] = bm->totfacesel;
  }
  /* We could support faces in paint modes. */
}

/* **** Depsgraph evaluation **** */

void BKE_mesh_eval_geometry(Depsgraph *depsgraph, Mesh *mesh)
{
  DEG_debug_print_eval(depsgraph, __func__, mesh->id.name, mesh);
  BKE_mesh_texspace_calc(mesh);
  /* We are here because something did change in the mesh. This means we can not trust the existing
   * evaluated mesh, and we don't know what parts of the mesh did change. So we simply delete the
   * evaluated mesh and let objects to re-create it with updated settings. */
  if (mesh->runtime->mesh_eval != nullptr) {
    BKE_id_free(nullptr, mesh->runtime->mesh_eval);
    mesh->runtime->mesh_eval = nullptr;
  }
  if (DEG_is_active(depsgraph)) {
    Mesh *mesh_orig = DEG_get_original(mesh);
    if (mesh->texspace_flag & ME_TEXSPACE_FLAG_AUTO_EVALUATED) {
      mesh_orig->texspace_flag |= ME_TEXSPACE_FLAG_AUTO_EVALUATED;
      copy_v3_v3(mesh_orig->texspace_location, mesh->texspace_location);
      copy_v3_v3(mesh_orig->texspace_size, mesh->texspace_size);
    }
  }
}
