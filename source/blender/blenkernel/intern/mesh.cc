/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_defaults.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_bounds.hh"
#include "BLI_edgehash.h"
#include "BLI_endian_switch.h"
#include "BLI_ghash.h"
#include "BLI_hash.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_index_range.hh"
#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_resource_scope.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_attribute.hh"
#include "BKE_bpath.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"

#include "PIL_time.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

using blender::float3;
using blender::MutableSpan;
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

  CustomData_reset(&mesh->vdata);
  CustomData_reset(&mesh->edata);
  CustomData_reset(&mesh->fdata);
  CustomData_reset(&mesh->pdata);
  CustomData_reset(&mesh->ldata);

  mesh->runtime = new blender::bke::MeshRuntime();

  mesh->face_sets_color_seed = BLI_hash_int(PIL_check_seconds_timer_i() & UINT_MAX);
}

static void mesh_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Mesh *mesh_dst = reinterpret_cast<Mesh *>(id_dst);
  const Mesh *mesh_src = reinterpret_cast<const Mesh *>(id_src);

  mesh_dst->runtime = new blender::bke::MeshRuntime();
  mesh_dst->runtime->deformed_only = mesh_src->runtime->deformed_only;
  mesh_dst->runtime->wrapper_type = mesh_src->runtime->wrapper_type;
  mesh_dst->runtime->wrapper_type_finalize = mesh_src->runtime->wrapper_type_finalize;
  mesh_dst->runtime->subsurf_runtime_data = mesh_src->runtime->subsurf_runtime_data;
  mesh_dst->runtime->cd_mask_extra = mesh_src->runtime->cd_mask_extra;
  /* Copy face dot tags and edge tags, since meshes may be duplicated after a subsurf modifier or
   * node, but we still need to be able to draw face center vertices and "optimal edges"
   * differently. The tags may be cleared explicitly when the topology is changed. */
  mesh_dst->runtime->subsurf_face_dot_tags = mesh_src->runtime->subsurf_face_dot_tags;
  mesh_dst->runtime->subsurf_optimal_display_edges =
      mesh_src->runtime->subsurf_optimal_display_edges;
  if ((mesh_src->id.tag & LIB_TAG_NO_MAIN) == 0) {
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
  mesh_dst->runtime->loose_verts_cache = mesh_src->runtime->loose_verts_cache;
  mesh_dst->runtime->verts_no_face_cache = mesh_src->runtime->verts_no_face_cache;
  mesh_dst->runtime->loose_edges_cache = mesh_src->runtime->loose_edges_cache;
  mesh_dst->runtime->looptris_cache = mesh_src->runtime->looptris_cache;
  mesh_dst->runtime->looptri_polys_cache = mesh_src->runtime->looptri_polys_cache;

  /* Only do tessface if we have no polys. */
  const bool do_tessface = ((mesh_src->totface != 0) && (mesh_src->totpoly == 0));

  CustomData_MeshMasks mask = CD_MASK_MESH;

  if (mesh_src->id.tag & LIB_TAG_NO_MAIN) {
    /* For copies in depsgraph, keep data like #CD_ORIGINDEX and #CD_ORCO. */
    CustomData_MeshMasks_update(&mask, &CD_MASK_DERIVEDMESH);
  }

  mesh_dst->mat = (Material **)MEM_dupallocN(mesh_src->mat);

  BKE_defgroup_copy_list(&mesh_dst->vertex_group_names, &mesh_src->vertex_group_names);
  mesh_dst->active_color_attribute = static_cast<char *>(
      MEM_dupallocN(mesh_src->active_color_attribute));
  mesh_dst->default_color_attribute = static_cast<char *>(
      MEM_dupallocN(mesh_src->default_color_attribute));

  CustomData_copy(&mesh_src->vdata, &mesh_dst->vdata, mask.vmask, mesh_dst->totvert);
  CustomData_copy(&mesh_src->edata, &mesh_dst->edata, mask.emask, mesh_dst->totedge);
  CustomData_copy(&mesh_src->ldata, &mesh_dst->ldata, mask.lmask, mesh_dst->totloop);
  CustomData_copy(&mesh_src->pdata, &mesh_dst->pdata, mask.pmask, mesh_dst->totpoly);
  blender::implicit_sharing::copy_shared_pointer(mesh_src->poly_offset_indices,
                                                 mesh_src->runtime->poly_offsets_sharing_info,
                                                 &mesh_dst->poly_offset_indices,
                                                 &mesh_dst->runtime->poly_offsets_sharing_info);
  if (do_tessface) {
    CustomData_copy(&mesh_src->fdata, &mesh_dst->fdata, mask.fmask, mesh_dst->totface);
  }
  else {
    mesh_tessface_clear_intern(mesh_dst, false);
  }

  mesh_dst->edit_mesh = nullptr;

  mesh_dst->mselect = (MSelect *)MEM_dupallocN(mesh_dst->mselect);

  /* TODO: Do we want to add flag to prevent this? */
  if (mesh_src->key && (flag & LIB_ID_COPY_SHAPEKEY)) {
    BKE_id_copy_ex(bmain, &mesh_src->key->id, (ID **)&mesh_dst->key, flag);
    /* XXX This is not nice, we need to make BKE_id_copy_ex fully re-entrant... */
    mesh_dst->key->from = &mesh_dst->id;
  }
}

void BKE_mesh_free_editmesh(Mesh *mesh)
{
  if (mesh->edit_mesh == nullptr) {
    return;
  }

  if (mesh->edit_mesh->is_shallow_copy == false) {
    BKE_editmesh_free_data(mesh->edit_mesh);
  }
  MEM_freeN(mesh->edit_mesh);
  mesh->edit_mesh = nullptr;
}

static void mesh_free_data(ID *id)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);

  BKE_mesh_free_editmesh(mesh);

  BKE_mesh_clear_geometry_and_metadata(mesh);
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
  Mesh *me = reinterpret_cast<Mesh *>(id);
  if (me->ldata.external) {
    BKE_bpath_foreach_path_fixed_process(
        bpath_data, me->ldata.external->filepath, sizeof(me->ldata.external->filepath));
  }
}

static void mesh_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  using namespace blender;
  Mesh *mesh = reinterpret_cast<Mesh *>(id);
  const bool is_undo = BLO_write_is_undo(writer);

  Vector<CustomDataLayer, 16> vert_layers;
  Vector<CustomDataLayer, 16> edge_layers;
  Vector<CustomDataLayer, 16> loop_layers;
  Vector<CustomDataLayer, 16> poly_layers;

  /* Cache only - don't write. */
  mesh->mface = nullptr;
  mesh->totface = 0;
  memset(&mesh->fdata, 0, sizeof(mesh->fdata));

  /* Do not store actual geometry data in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(mesh) && !is_undo) {
    mesh->totvert = 0;
    memset(&mesh->vdata, 0, sizeof(mesh->vdata));

    mesh->totedge = 0;
    memset(&mesh->edata, 0, sizeof(mesh->edata));

    mesh->totloop = 0;
    memset(&mesh->ldata, 0, sizeof(mesh->ldata));

    mesh->totpoly = 0;
    memset(&mesh->pdata, 0, sizeof(mesh->pdata));
    mesh->poly_offset_indices = nullptr;
  }
  else {
    CustomData_blend_write_prepare(mesh->vdata, vert_layers, {});
    CustomData_blend_write_prepare(mesh->edata, edge_layers, {});
    CustomData_blend_write_prepare(mesh->ldata, loop_layers, {});
    CustomData_blend_write_prepare(mesh->pdata, poly_layers, {});
  }

  mesh->runtime = nullptr;

  BLO_write_id_struct(writer, Mesh, id_address, &mesh->id);
  BKE_id_blend_write(writer, &mesh->id);

  if (mesh->adt) {
    BKE_animdata_blend_write(writer, mesh->adt);
  }

  BKE_defbase_blend_write(writer, &mesh->vertex_group_names);
  BLO_write_string(writer, mesh->active_color_attribute);
  BLO_write_string(writer, mesh->default_color_attribute);

  BLO_write_pointer_array(writer, mesh->totcol, mesh->mat);
  BLO_write_raw(writer, sizeof(MSelect) * mesh->totselect, mesh->mselect);

  CustomData_blend_write(
      writer, &mesh->vdata, vert_layers, mesh->totvert, CD_MASK_MESH.vmask, &mesh->id);
  CustomData_blend_write(
      writer, &mesh->edata, edge_layers, mesh->totedge, CD_MASK_MESH.emask, &mesh->id);
  /* `fdata` is cleared above but written so slots align. */
  CustomData_blend_write(writer, &mesh->fdata, {}, mesh->totface, CD_MASK_MESH.fmask, &mesh->id);
  CustomData_blend_write(
      writer, &mesh->ldata, loop_layers, mesh->totloop, CD_MASK_MESH.lmask, &mesh->id);
  CustomData_blend_write(
      writer, &mesh->pdata, poly_layers, mesh->totpoly, CD_MASK_MESH.pmask, &mesh->id);

  if (mesh->poly_offset_indices) {
    BLO_write_int32_array(writer, mesh->totpoly + 1, mesh->poly_offset_indices);
  }
}

static void mesh_blend_read_data(BlendDataReader *reader, ID *id)
{
  Mesh *mesh = reinterpret_cast<Mesh *>(id);
  BLO_read_pointer_array(reader, (void **)&mesh->mat);

  /* Deprecated pointers to custom data layers are read here for backward compatibility
   * with files where these were owning pointers rather than a view into custom data. */
  BLO_read_data_address(reader, &mesh->mvert);
  BLO_read_data_address(reader, &mesh->medge);
  BLO_read_data_address(reader, &mesh->mface);
  BLO_read_data_address(reader, &mesh->mtface);
  BLO_read_data_address(reader, &mesh->dvert);
  BLO_read_data_address(reader, &mesh->tface);
  BLO_read_data_address(reader, &mesh->mcol);

  BLO_read_data_address(reader, &mesh->mselect);

  BLO_read_data_address(reader, &mesh->adt);
  BKE_animdata_blend_read_data(reader, mesh->adt);

  BLO_read_list(reader, &mesh->vertex_group_names);

  CustomData_blend_read(reader, &mesh->vdata, mesh->totvert);
  CustomData_blend_read(reader, &mesh->edata, mesh->totedge);
  CustomData_blend_read(reader, &mesh->fdata, mesh->totface);
  CustomData_blend_read(reader, &mesh->ldata, mesh->totloop);
  CustomData_blend_read(reader, &mesh->pdata, mesh->totpoly);
  if (mesh->deform_verts().is_empty()) {
    /* Vertex group data was also an owning pointer in old Blender versions.
     * Don't read them again if they were read as part of #CustomData. */
    BKE_defvert_blend_read(reader, mesh->totvert, mesh->dvert);
  }
  BLO_read_data_address(reader, &mesh->active_color_attribute);
  BLO_read_data_address(reader, &mesh->default_color_attribute);

  mesh->texspace_flag &= ~ME_TEXSPACE_FLAG_AUTO_EVALUATED;
  mesh->edit_mesh = nullptr;

  mesh->runtime = new blender::bke::MeshRuntime();

  if (mesh->poly_offset_indices) {
    BLO_read_int32_array(reader, mesh->totpoly + 1, &mesh->poly_offset_indices);
    mesh->runtime->poly_offsets_sharing_info = blender::implicit_sharing::info_for_mem_free(
        mesh->poly_offset_indices);
  }

  if (mesh->mselect == nullptr) {
    mesh->totselect = 0;
  }

  if (BLO_read_requires_endian_switch(reader) && mesh->tface) {
    TFace *tf = mesh->tface;
    for (int i = 0; i < mesh->totface; i++, tf++) {
      BLI_endian_switch_uint32_array(tf->col, 4);
    }
  }
}

static void mesh_blend_read_lib(BlendLibReader *reader, ID *id)
{
  Mesh *me = reinterpret_cast<Mesh *>(id);
  /* This check added for python created meshes. */
  if (me->mat) {
    for (int i = 0; i < me->totcol; i++) {
      BLO_read_id_address(reader, id, &me->mat[i]);
    }
  }
  else {
    me->totcol = 0;
  }

  BLO_read_id_address(reader, id, &me->ipo);  // XXX: deprecated: old anim sys
  BLO_read_id_address(reader, id, &me->key);
  BLO_read_id_address(reader, id, &me->texcomesh);
}

static void mesh_read_expand(BlendExpander *expander, ID *id)
{
  Mesh *me = reinterpret_cast<Mesh *>(id);
  for (int a = 0; a < me->totcol; a++) {
    BLO_expand(expander, me->mat[a]);
  }

  BLO_expand(expander, me->key);
  BLO_expand(expander, me->texcomesh);
}

IDTypeInfo IDType_ID_ME = {
    /*id_code*/ ID_ME,
    /*id_filter*/ FILTER_ID_ME,
    /*main_listbase_index*/ INDEX_ID_ME,
    /*struct_size*/ sizeof(Mesh),
    /*name*/ "Mesh",
    /*name_plural*/ "meshes",
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
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ mesh_blend_write,
    /*blend_read_data*/ mesh_blend_read_data,
    /*blend_read_lib*/ mesh_blend_read_lib,
    /*blend_read_expand*/ mesh_read_expand,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

enum {
  MESHCMP_DVERT_WEIGHTMISMATCH = 1,
  MESHCMP_DVERT_GROUPMISMATCH,
  MESHCMP_DVERT_TOTGROUPMISMATCH,
  MESHCMP_LOOPCOLMISMATCH,
  MESHCMP_LOOPUVMISMATCH,
  MESHCMP_LOOPMISMATCH,
  MESHCMP_POLYVERTMISMATCH,
  MESHCMP_POLYMISMATCH,
  MESHCMP_EDGEUNKNOWN,
  MESHCMP_VERTCOMISMATCH,
  MESHCMP_CDLAYERS_MISMATCH,
  MESHCMP_ATTRIBUTE_VALUE_MISMATCH,
};

static const char *cmpcode_to_str(int code)
{
  switch (code) {
    case MESHCMP_DVERT_WEIGHTMISMATCH:
      return "Vertex Weight Mismatch";
    case MESHCMP_DVERT_GROUPMISMATCH:
      return "Vertex Group Mismatch";
    case MESHCMP_DVERT_TOTGROUPMISMATCH:
      return "Vertex Doesn't Belong To Same Number Of Groups";
    case MESHCMP_LOOPCOLMISMATCH:
      return "Color Attribute Mismatch";
    case MESHCMP_LOOPUVMISMATCH:
      return "UV Mismatch";
    case MESHCMP_LOOPMISMATCH:
      return "Loop Mismatch";
    case MESHCMP_POLYVERTMISMATCH:
      return "Loop Vert Mismatch In Poly Test";
    case MESHCMP_POLYMISMATCH:
      return "Loop Vert Mismatch";
    case MESHCMP_EDGEUNKNOWN:
      return "Edge Mismatch";
    case MESHCMP_VERTCOMISMATCH:
      return "Vertex Coordinate Mismatch";
    case MESHCMP_CDLAYERS_MISMATCH:
      return "CustomData Layer Count Mismatch";
    case MESHCMP_ATTRIBUTE_VALUE_MISMATCH:
      return "Attribute Value Mismatch";
    default:
      return "Mesh Comparison Code Unknown";
  }
}

static bool is_sublayer_name(char const *sublayer_name, char const *name)
{
  BLI_assert(strlen(sublayer_name) == 2);
  if (name[1] != sublayer_name[0]) {
    return false;
  }
  if (name[2] != sublayer_name[1]) {
    return false;
  }
  if (name[3] != '.') {
    return false;
  }
  return true;
}

static bool is_uv_bool_sublayer(const CustomDataLayer &layer)
{
  char const *name = layer.name;

  if (name[0] != '.') {
    return false;
  }

  return is_sublayer_name(UV_VERTSEL_NAME, name) || is_sublayer_name(UV_EDGESEL_NAME, name) ||
         is_sublayer_name(UV_PINNED_NAME, name);
}

/** Thresh is threshold for comparing vertices, UVs, vertex colors, weights, etc. */
static int customdata_compare(
    CustomData *c1, CustomData *c2, const int total_length, Mesh *m1, const float thresh)
{
  CustomDataLayer *l1, *l2;
  int layer_count1 = 0, layer_count2 = 0, j;
  const uint64_t cd_mask_non_generic = CD_MASK_MDEFORMVERT;
  const uint64_t cd_mask_all_attr = CD_MASK_PROP_ALL | cd_mask_non_generic;

  /* The uv selection / pin layers are ignored in the comparisons because
   * the original flags they replace were ignored as well. Because of the
   * lazy creation of these layers it would need careful handling of the
   * test files to compare these layers. For now it has been decided to
   * skip them.
   */

  for (int i = 0; i < c1->totlayer; i++) {
    l1 = &c1->layers[i];
    if ((CD_TYPE_AS_MASK(l1->type) & cd_mask_all_attr) && l1->anonymous_id == nullptr &&
        !is_uv_bool_sublayer(*l1))
    {
      layer_count1++;
    }
  }

  for (int i = 0; i < c2->totlayer; i++) {
    l2 = &c2->layers[i];
    if ((CD_TYPE_AS_MASK(l2->type) & cd_mask_all_attr) && l2->anonymous_id == nullptr &&
        !is_uv_bool_sublayer(*l2))
    {
      layer_count2++;
    }
  }

  if (layer_count1 != layer_count2) {
    /* TODO(@HooglyBoogly): Re-enable after tests are updated for material index refactor and UV as
     * generic attribute refactor. */
    // return MESHCMP_CDLAYERS_MISMATCH;
  }

  l1 = c1->layers;
  l2 = c2->layers;

  for (int i1 = 0; i1 < c1->totlayer; i1++) {
    l1 = c1->layers + i1;
    if (l1->anonymous_id != nullptr || is_uv_bool_sublayer(*l1)) {
      continue;
    }
    bool found_corresponding_layer = false;
    for (int i2 = 0; i2 < c2->totlayer; i2++) {
      l2 = c2->layers + i2;
      if (l1->type != l2->type || !STREQ(l1->name, l2->name) || l2->anonymous_id != nullptr) {
        continue;
      }
      /* At this point `l1` and `l2` have the same name and type, so they should be compared. */

      found_corresponding_layer = true;

      if (StringRef(l1->name) == ".corner_edge") {
        /* TODO(Hans): This attribute wasn't tested before loops were refactored into separate
         * corner edges and corner verts attributes. Remove after updating tests. */
        continue;
      }

      switch (l1->type) {
        case CD_PROP_INT32_2D: {
          blender::int2 *e1 = (blender::int2 *)l1->data;
          blender::int2 *e2 = (blender::int2 *)l2->data;

          if (StringRef(l1->name) == ".edge_verts") {
            int etot = m1->totedge;
            EdgeHash *eh = BLI_edgehash_new_ex(__func__, etot);

            for (j = 0; j < etot; j++, e1++) {
              BLI_edgehash_insert(eh, (*e1)[0], (*e1)[1], e1);
            }

            for (j = 0; j < etot; j++, e2++) {
              if (!BLI_edgehash_lookup(eh, (*e2)[0], (*e2)[1])) {
                return MESHCMP_EDGEUNKNOWN;
              }
            }
            BLI_edgehash_free(eh, nullptr);
          }
          else {
            for (j = 0; j < total_length; j++) {
              if (e1[j] != e2[j]) {
                return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
              }
            }
          }
          break;
        }
        case CD_PROP_BYTE_COLOR: {
          MLoopCol *lp1 = (MLoopCol *)l1->data;
          MLoopCol *lp2 = (MLoopCol *)l2->data;
          int ltot = m1->totloop;

          for (j = 0; j < ltot; j++, lp1++, lp2++) {
            if (lp1->r != lp2->r || lp1->g != lp2->g || lp1->b != lp2->b || lp1->a != lp2->a) {
              return MESHCMP_LOOPCOLMISMATCH;
            }
          }
          break;
        }
        case CD_MDEFORMVERT: {
          MDeformVert *dv1 = (MDeformVert *)l1->data;
          MDeformVert *dv2 = (MDeformVert *)l2->data;
          int dvtot = m1->totvert;

          for (j = 0; j < dvtot; j++, dv1++, dv2++) {
            int k;
            MDeformWeight *dw1 = dv1->dw, *dw2 = dv2->dw;

            if (dv1->totweight != dv2->totweight) {
              return MESHCMP_DVERT_TOTGROUPMISMATCH;
            }

            for (k = 0; k < dv1->totweight; k++, dw1++, dw2++) {
              if (dw1->def_nr != dw2->def_nr) {
                return MESHCMP_DVERT_GROUPMISMATCH;
              }
              if (fabsf(dw1->weight - dw2->weight) > thresh) {
                return MESHCMP_DVERT_WEIGHTMISMATCH;
              }
            }
          }
          break;
        }
        case CD_PROP_FLOAT: {
          const float *l1_data = (float *)l1->data;
          const float *l2_data = (float *)l2->data;

          for (int i = 0; i < total_length; i++) {
            if (compare_threshold_relative(l1_data[i], l2_data[i], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_FLOAT2: {
          const float(*l1_data)[2] = (float(*)[2])l1->data;
          const float(*l2_data)[2] = (float(*)[2])l2->data;

          for (int i = 0; i < total_length; i++) {
            if (compare_threshold_relative(l1_data[i][0], l2_data[i][0], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
            if (compare_threshold_relative(l1_data[i][1], l2_data[i][1], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_FLOAT3: {
          const float(*l1_data)[3] = (float(*)[3])l1->data;
          const float(*l2_data)[3] = (float(*)[3])l2->data;

          for (int i = 0; i < total_length; i++) {
            if (compare_threshold_relative(l1_data[i][0], l2_data[i][0], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
            if (compare_threshold_relative(l1_data[i][1], l2_data[i][1], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
            if (compare_threshold_relative(l1_data[i][2], l2_data[i][2], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_QUATERNION: {
          const float(*l1_data)[4] = (float(*)[4])l1->data;
          const float(*l2_data)[4] = (float(*)[4])l2->data;

          for (int i = 0; i < total_length; i++) {
            if (compare_threshold_relative(l1_data[i][0], l2_data[i][0], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
            if (compare_threshold_relative(l1_data[i][1], l2_data[i][1], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
            if (compare_threshold_relative(l1_data[i][2], l2_data[i][2], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
            if (compare_threshold_relative(l1_data[i][3], l2_data[i][3], thresh)) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_INT32: {
          const int *l1_data = (int *)l1->data;
          const int *l2_data = (int *)l2->data;

          for (int i = 0; i < total_length; i++) {
            if (l1_data[i] != l2_data[i]) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_INT8: {
          const int8_t *l1_data = (int8_t *)l1->data;
          const int8_t *l2_data = (int8_t *)l2->data;

          for (int i = 0; i < total_length; i++) {
            if (l1_data[i] != l2_data[i]) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_BOOL: {
          const bool *l1_data = (bool *)l1->data;
          const bool *l2_data = (bool *)l2->data;
          for (int i = 0; i < total_length; i++) {
            if (l1_data[i] != l2_data[i]) {
              return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
            }
          }
          break;
        }
        case CD_PROP_COLOR: {
          const MPropCol *l1_data = (MPropCol *)l1->data;
          const MPropCol *l2_data = (MPropCol *)l2->data;

          for (int i = 0; i < total_length; i++) {
            for (j = 0; j < 4; j++) {
              if (compare_threshold_relative(l1_data[i].color[j], l2_data[i].color[j], thresh)) {
                return MESHCMP_ATTRIBUTE_VALUE_MISMATCH;
              }
            }
          }
          break;
        }
        default: {
          break;
        }
      }
    }
    if (!found_corresponding_layer) {
      if ((uint64_t(1) << l1->type) & CD_MASK_PROP_ALL) {
        return MESHCMP_CDLAYERS_MISMATCH;
      }
    }
  }

  return 0;
}

const char *BKE_mesh_cmp(Mesh *me1, Mesh *me2, float thresh)
{
  int c;

  if (!me1 || !me2) {
    return "Requires two input meshes";
  }

  if (me1->totvert != me2->totvert) {
    return "Number of verts don't match";
  }

  if (me1->totedge != me2->totedge) {
    return "Number of edges don't match";
  }

  if (me1->totpoly != me2->totpoly) {
    return "Number of faces don't match";
  }

  if (me1->totloop != me2->totloop) {
    return "Number of loops don't match";
  }

  if (!std::equal(
          me1->poly_offsets().begin(), me1->poly_offsets().end(), me2->poly_offsets().begin()))
  {
    return "Face sizes don't match";
  }

  if ((c = customdata_compare(&me1->vdata, &me2->vdata, me1->totvert, me1, thresh))) {
    return cmpcode_to_str(c);
  }

  if ((c = customdata_compare(&me1->edata, &me2->edata, me1->totedge, me1, thresh))) {
    return cmpcode_to_str(c);
  }

  if ((c = customdata_compare(&me1->ldata, &me2->ldata, me1->totloop, me1, thresh))) {
    return cmpcode_to_str(c);
  }

  if ((c = customdata_compare(&me1->pdata, &me2->pdata, me1->totpoly, me1, thresh))) {
    return cmpcode_to_str(c);
  }

  return nullptr;
}

bool BKE_mesh_attribute_required(const char *name)
{
  return ELEM(StringRef(name), "position", ".corner_vert", ".corner_edge", ".edge_verts");
}

void BKE_mesh_ensure_skin_customdata(Mesh *me)
{
  BMesh *bm = me->edit_mesh ? me->edit_mesh->bm : nullptr;
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
    if (!CustomData_has_layer(&me->vdata, CD_MVERT_SKIN)) {
      vs = (MVertSkin *)CustomData_add_layer(
          &me->vdata, CD_MVERT_SKIN, CD_SET_DEFAULT, me->totvert);

      /* Mark an arbitrary vertex as root */
      if (vs) {
        vs->flag |= MVERT_SKIN_ROOT;
      }
    }
  }
}

bool BKE_mesh_has_custom_loop_normals(Mesh *me)
{
  if (me->edit_mesh) {
    return CustomData_has_layer(&me->edit_mesh->bm->ldata, CD_CUSTOMLOOPNORMAL);
  }

  return CustomData_has_layer(&me->ldata, CD_CUSTOMLOOPNORMAL);
}

void BKE_mesh_free_data_for_undo(Mesh *me)
{
  mesh_free_data(&me->id);
}

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
  CustomData_free(&mesh.vdata, mesh.totvert);
  CustomData_free(&mesh.edata, mesh.totedge);
  CustomData_free(&mesh.fdata, mesh.totface);
  CustomData_free(&mesh.ldata, mesh.totloop);
  CustomData_free(&mesh.pdata, mesh.totpoly);
  if (mesh.poly_offset_indices) {
    blender::implicit_sharing::free_shared_data(&mesh.poly_offset_indices,
                                                &mesh.runtime->poly_offsets_sharing_info);
  }
  MEM_SAFE_FREE(mesh.mselect);

  mesh.totvert = 0;
  mesh.totedge = 0;
  mesh.totface = 0;
  mesh.totloop = 0;
  mesh.totpoly = 0;
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
    CustomData_free(&mesh->fdata, mesh->totface);
  }
  else {
    CustomData_reset(&mesh->fdata);
  }

  mesh->totface = 0;
}

Mesh *BKE_mesh_add(Main *bmain, const char *name)
{
  return static_cast<Mesh *>(BKE_id_new(bmain, ID_ME, name));
}

void BKE_mesh_poly_offsets_ensure_alloc(Mesh *mesh)
{
  BLI_assert(mesh->poly_offset_indices == nullptr);
  BLI_assert(mesh->runtime->poly_offsets_sharing_info == nullptr);
  if (mesh->totpoly == 0) {
    return;
  }
  mesh->poly_offset_indices = static_cast<int *>(
      MEM_malloc_arrayN(mesh->totpoly + 1, sizeof(int), __func__));
  mesh->runtime->poly_offsets_sharing_info = blender::implicit_sharing::info_for_mem_free(
      mesh->poly_offset_indices);

#ifdef DEBUG
  /* Fill offsets with obviously bad values to simplify finding missing initialization. */
  mesh->poly_offsets_for_write().fill(-1);
#endif
  /* Set common values for convenience. */
  mesh->poly_offset_indices[0] = 0;
  mesh->poly_offset_indices[mesh->totpoly] = mesh->totloop;
}

MutableSpan<int> Mesh::poly_offsets_for_write()
{
  if (this->totpoly == 0) {
    return {};
  }
  blender::implicit_sharing::make_trivial_data_mutable(
      &this->poly_offset_indices, &this->runtime->poly_offsets_sharing_info, this->totpoly + 1);
  return {this->poly_offset_indices, this->totpoly + 1};
}

static void mesh_ensure_cdlayers_primary(Mesh &mesh)
{
  blender::bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  blender::bke::AttributeInitConstruct attribute_init;

  /* Try to create attributes if they do not exist. */
  attributes.add("position", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, attribute_init);
  attributes.add(".edge_verts", ATTR_DOMAIN_EDGE, CD_PROP_INT32_2D, attribute_init);
  attributes.add(".corner_vert", ATTR_DOMAIN_CORNER, CD_PROP_INT32, attribute_init);
  attributes.add(".corner_edge", ATTR_DOMAIN_CORNER, CD_PROP_INT32, attribute_init);
}

Mesh *BKE_mesh_new_nomain(const int verts_num,
                          const int edges_num,
                          const int polys_num,
                          const int loops_num)
{
  Mesh *mesh = static_cast<Mesh *>(BKE_libblock_alloc(
      nullptr, ID_ME, BKE_idtype_idcode_to_name(ID_ME), LIB_ID_CREATE_LOCALIZE));
  BKE_libblock_init_empty(&mesh->id);

  mesh->totvert = verts_num;
  mesh->totedge = edges_num;
  mesh->totpoly = polys_num;
  mesh->totloop = loops_num;

  mesh_ensure_cdlayers_primary(*mesh);
  BKE_mesh_poly_offsets_ensure_alloc(mesh);

  return mesh;
}

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
  me_dst->smoothresh = me_src->smoothresh;
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
  BLI_assert(me_dst->id.tag & (LIB_TAG_NO_MAIN | LIB_TAG_COPIED_ON_WRITE));

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
}

Mesh *BKE_mesh_new_nomain_from_template_ex(const Mesh *me_src,
                                           const int verts_num,
                                           const int edges_num,
                                           const int tessface_num,
                                           const int polys_num,
                                           const int loops_num,
                                           const CustomData_MeshMasks mask)
{
  /* Only do tessface if we are creating tessfaces or copying from mesh with only tessfaces. */
  const bool do_tessface = (tessface_num || ((me_src->totface != 0) && (me_src->totpoly == 0)));

  Mesh *me_dst = static_cast<Mesh *>(BKE_id_new_nomain(ID_ME, nullptr));

  me_dst->mselect = (MSelect *)MEM_dupallocN(me_src->mselect);

  me_dst->totvert = verts_num;
  me_dst->totedge = edges_num;
  me_dst->totpoly = polys_num;
  me_dst->totloop = loops_num;
  me_dst->totface = tessface_num;

  BKE_mesh_copy_parameters_for_eval(me_dst, me_src);

  CustomData_copy_layout(&me_src->vdata, &me_dst->vdata, mask.vmask, CD_SET_DEFAULT, verts_num);
  CustomData_copy_layout(&me_src->edata, &me_dst->edata, mask.emask, CD_SET_DEFAULT, edges_num);
  CustomData_copy_layout(&me_src->pdata, &me_dst->pdata, mask.pmask, CD_SET_DEFAULT, polys_num);
  CustomData_copy_layout(&me_src->ldata, &me_dst->ldata, mask.lmask, CD_SET_DEFAULT, loops_num);
  if (do_tessface) {
    CustomData_copy_layout(
        &me_src->fdata, &me_dst->fdata, mask.fmask, CD_SET_DEFAULT, tessface_num);
  }
  else {
    mesh_tessface_clear_intern(me_dst, false);
  }

  /* The destination mesh should at least have valid primary CD layers,
   * even in cases where the source mesh does not. */
  mesh_ensure_cdlayers_primary(*me_dst);
  BKE_mesh_poly_offsets_ensure_alloc(me_dst);
  if (do_tessface && !CustomData_get_layer(&me_dst->fdata, CD_MFACE)) {
    CustomData_add_layer(&me_dst->fdata, CD_MFACE, CD_SET_DEFAULT, me_dst->totface);
  }

  /* Expect that normals aren't copied at all, since the destination mesh is new. */
  BLI_assert(BKE_mesh_vert_normals_are_dirty(me_dst));

  return me_dst;
}

Mesh *BKE_mesh_new_nomain_from_template(const Mesh *me_src,
                                        const int verts_num,
                                        const int edges_num,
                                        const int polys_num,
                                        const int loops_num)
{
  return BKE_mesh_new_nomain_from_template_ex(
      me_src, verts_num, edges_num, 0, polys_num, loops_num, CD_MASK_EVERYTHING);
}

void BKE_mesh_eval_delete(Mesh *mesh_eval)
{
  /* Evaluated mesh may point to edit mesh, but never owns it. */
  mesh_eval->edit_mesh = nullptr;
  mesh_free_data(&mesh_eval->id);
  BKE_libblock_free_data(&mesh_eval->id, false);
  MEM_freeN(mesh_eval);
}

Mesh *BKE_mesh_copy_for_eval(const Mesh *source)
{
  return reinterpret_cast<Mesh *>(
      BKE_id_copy_ex(nullptr, &source->id, nullptr, LIB_ID_COPY_LOCALIZE));
}

BMesh *BKE_mesh_to_bmesh_ex(const Mesh *me,
                            const BMeshCreateParams *create_params,
                            const BMeshFromMeshParams *convert_params)
{
  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(me);

  BMesh *bm = BM_mesh_create(&allocsize, create_params);
  BM_mesh_bm_from_me(bm, me, convert_params);

  return bm;
}

BMesh *BKE_mesh_to_bmesh(Mesh *me,
                         Object *ob,
                         const bool add_key_index,
                         const BMeshCreateParams *params)
{
  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.calc_face_normal = false;
  bmesh_from_mesh_params.calc_vert_normal = false;
  bmesh_from_mesh_params.add_key_index = add_key_index;
  bmesh_from_mesh_params.use_shapekey = true;
  bmesh_from_mesh_params.active_shapekey = ob->shapenr;
  return BKE_mesh_to_bmesh_ex(me, params, &bmesh_from_mesh_params);
}

Mesh *BKE_mesh_from_bmesh_nomain(BMesh *bm,
                                 const BMeshToMeshParams *params,
                                 const Mesh *me_settings)
{
  BLI_assert(params->calc_object_remap == false);
  Mesh *mesh = static_cast<Mesh *>(BKE_id_new_nomain(ID_ME, nullptr));
  BM_mesh_bm_to_me(nullptr, bm, mesh, params);
  BKE_mesh_copy_parameters_for_eval(mesh, me_settings);
  return mesh;
}

Mesh *BKE_mesh_from_bmesh_for_eval_nomain(BMesh *bm,
                                          const CustomData_MeshMasks *cd_mask_extra,
                                          const Mesh *me_settings)
{
  Mesh *mesh = static_cast<Mesh *>(BKE_id_new_nomain(ID_ME, nullptr));
  BM_mesh_bm_to_me_for_eval(bm, mesh, cd_mask_extra);
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
  ensure_orig_index_layer(mesh->vdata, mesh->totvert);
  ensure_orig_index_layer(mesh->edata, mesh->totedge);
  ensure_orig_index_layer(mesh->pdata, mesh->totpoly);
}

BoundBox *BKE_mesh_boundbox_get(Object *ob)
{
  /* This is Object-level data access,
   * DO NOT touch to Mesh's bb, would be totally thread-unsafe. */
  if (ob->runtime.bb == nullptr || ob->runtime.bb->flag & BOUNDBOX_DIRTY) {
    Mesh *me = static_cast<Mesh *>(ob->data);
    float min[3], max[3];

    INIT_MINMAX(min, max);
    if (!BKE_mesh_wrapper_minmax(me, min, max)) {
      min[0] = min[1] = min[2] = -1.0f;
      max[0] = max[1] = max[2] = 1.0f;
    }

    if (ob->runtime.bb == nullptr) {
      ob->runtime.bb = (BoundBox *)MEM_mallocN(sizeof(*ob->runtime.bb), __func__);
    }
    BKE_boundbox_init_from_minmax(ob->runtime.bb, min, max);
    ob->runtime.bb->flag &= ~BOUNDBOX_DIRTY;
  }

  return ob->runtime.bb;
}

void BKE_mesh_texspace_calc(Mesh *me)
{
  if (me->texspace_flag & ME_TEXSPACE_FLAG_AUTO) {
    float min[3], max[3];

    INIT_MINMAX(min, max);
    if (!BKE_mesh_wrapper_minmax(me, min, max)) {
      min[0] = min[1] = min[2] = -1.0f;
      max[0] = max[1] = max[2] = 1.0f;
    }

    float texspace_location[3], texspace_size[3];
    mid_v3_v3v3(texspace_location, min, max);

    texspace_size[0] = (max[0] - min[0]) / 2.0f;
    texspace_size[1] = (max[1] - min[1]) / 2.0f;
    texspace_size[2] = (max[2] - min[2]) / 2.0f;

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

    copy_v3_v3(me->texspace_location, texspace_location);
    copy_v3_v3(me->texspace_size, texspace_size);

    me->texspace_flag |= ME_TEXSPACE_FLAG_AUTO_EVALUATED;
  }
}

void BKE_mesh_texspace_ensure(Mesh *me)
{
  if ((me->texspace_flag & ME_TEXSPACE_FLAG_AUTO) &&
      !(me->texspace_flag & ME_TEXSPACE_FLAG_AUTO_EVALUATED))
  {
    BKE_mesh_texspace_calc(me);
  }
}

void BKE_mesh_texspace_get(Mesh *me, float r_texspace_location[3], float r_texspace_size[3])
{
  BKE_mesh_texspace_ensure(me);

  if (r_texspace_location) {
    copy_v3_v3(r_texspace_location, me->texspace_location);
  }
  if (r_texspace_size) {
    copy_v3_v3(r_texspace_size, me->texspace_size);
  }
}

void BKE_mesh_texspace_get_reference(Mesh *me,
                                     char **r_texspace_flag,
                                     float **r_texspace_location,
                                     float **r_texspace_size)
{
  BKE_mesh_texspace_ensure(me);

  if (r_texspace_flag != nullptr) {
    *r_texspace_flag = &me->texspace_flag;
  }
  if (r_texspace_location != nullptr) {
    *r_texspace_location = me->texspace_location;
  }
  if (r_texspace_size != nullptr) {
    *r_texspace_size = me->texspace_size;
  }
}

float (*BKE_mesh_orco_verts_get(Object *ob))[3]
{
  Mesh *me = static_cast<Mesh *>(ob->data);
  Mesh *tme = me->texcomesh ? me->texcomesh : me;

  /* Get appropriate vertex coordinates */
  float(*vcos)[3] = (float(*)[3])MEM_calloc_arrayN(me->totvert, sizeof(*vcos), "orco mesh");
  const Span<float3> positions = tme->vert_positions();

  int totvert = min_ii(tme->totvert, me->totvert);

  for (int a = 0; a < totvert; a++) {
    copy_v3_v3(vcos[a], positions[a]);
  }

  return vcos;
}

void BKE_mesh_orco_verts_transform(Mesh *me, float (*orco)[3], int totvert, int invert)
{
  float texspace_location[3], texspace_size[3];

  BKE_mesh_texspace_get(me->texcomesh ? me->texcomesh : me, texspace_location, texspace_size);

  if (invert) {
    for (int a = 0; a < totvert; a++) {
      float *co = orco[a];
      madd_v3_v3v3v3(co, texspace_location, co, texspace_size);
    }
  }
  else {
    for (int a = 0; a < totvert; a++) {
      float *co = orco[a];
      co[0] = (co[0] - texspace_location[0]) / texspace_size[0];
      co[1] = (co[1] - texspace_location[1]) / texspace_size[1];
      co[2] = (co[2] - texspace_location[2]) / texspace_size[2];
    }
  }
}

void BKE_mesh_orco_ensure(Object *ob, Mesh *mesh)
{
  if (CustomData_has_layer(&mesh->vdata, CD_ORCO)) {
    return;
  }

  /* Orcos are stored in normalized 0..1 range by convention. */
  float(*orcodata)[3] = BKE_mesh_orco_verts_get(ob);
  BKE_mesh_orco_verts_transform(mesh, orcodata, mesh->totvert, false);
  CustomData_add_layer_with_data(&mesh->vdata, CD_ORCO, orcodata, mesh->totvert, nullptr);
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

void BKE_mesh_assign_object(Main *bmain, Object *ob, Mesh *me)
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
    ob->data = me;
    id_us_plus((ID *)me);
  }

  BKE_object_materials_test(bmain, ob, (ID *)me);

  BKE_modifiers_test_object(ob);
}

void BKE_mesh_material_index_remove(Mesh *me, short index)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();
  AttributeWriter<int> material_indices = attributes.lookup_for_write<int>("material_index");
  if (!material_indices) {
    return;
  }
  if (material_indices.domain != ATTR_DOMAIN_FACE) {
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

  BKE_mesh_tessface_clear(me);
}

bool BKE_mesh_material_index_used(Mesh *me, short index)
{
  using namespace blender;
  using namespace blender::bke;
  const AttributeAccessor attributes = me->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  if (material_indices.is_single()) {
    return material_indices.get_internal_single() == index;
  }
  const VArraySpan<int> indices_span(material_indices);
  return indices_span.contains(index);
}

void BKE_mesh_material_index_clear(Mesh *me)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();
  attributes.remove("material_index");

  BKE_mesh_tessface_clear(me);
}

void BKE_mesh_material_remap(Mesh *me, const uint *remap, uint remap_len)
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

  if (me->edit_mesh) {
    BMEditMesh *em = me->edit_mesh;
    BMIter iter;
    BMFace *efa;

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      MAT_NR_REMAP(efa->mat_nr);
    }
  }
  else {
    MutableAttributeAccessor attributes = me->attributes_for_write();
    SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
        "material_index", ATTR_DOMAIN_FACE);
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

void BKE_mesh_smooth_flag_set(Mesh *me, const bool use_smooth)
{
  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = me->attributes_for_write();
  if (use_smooth) {
    attributes.remove("sharp_face");
  }
  else {
    SpanAttributeWriter<bool> sharp_faces = attributes.lookup_or_add_for_write_only_span<bool>(
        "sharp_face", ATTR_DOMAIN_FACE);
    sharp_faces.span.fill(true);
    sharp_faces.finish();
  }
}

void BKE_mesh_auto_smooth_flag_set(Mesh *me,
                                   const bool use_auto_smooth,
                                   const float auto_smooth_angle)
{
  if (use_auto_smooth) {
    me->flag |= ME_AUTOSMOOTH;
    me->smoothresh = auto_smooth_angle;
  }
  else {
    me->flag &= ~ME_AUTOSMOOTH;
  }
}

void BKE_mesh_looptri_get_real_edges(const blender::int2 *edges,
                                     const int *corner_verts,
                                     const int *corner_edges,
                                     const MLoopTri *tri,
                                     int r_edges[3])
{
  for (int i = 2, i_next = 0; i_next < 3; i = i_next++) {
    const int corner_1 = tri->tri[i];
    const int corner_2 = tri->tri[i_next];
    const int vert_1 = corner_verts[corner_1];
    const int vert_2 = corner_verts[corner_2];
    const int edge_i = corner_edges[corner_1];
    const blender::int2 &edge = edges[edge_i];

    bool is_real = (vert_1 == edge[0] && vert_2 == edge[1]) ||
                   (vert_1 == edge[1] && vert_2 == edge[0]);

    r_edges[i] = is_real ? edge_i : -1;
  }
}

std::optional<blender::Bounds<blender::float3>> Mesh::bounds_min_max() const
{
  using namespace blender;
  if (this->totvert == 0) {
    return std::nullopt;
  }
  this->runtime->bounds_cache.ensure(
      [&](Bounds<float3> &r_bounds) { r_bounds = *bounds::min_max(this->vert_positions()); });
  return this->runtime->bounds_cache.data();
}

void Mesh::bounds_set_eager(const blender::Bounds<float3> &bounds)
{
  this->runtime->bounds_cache.ensure([&](blender::Bounds<float3> &r_data) { r_data = bounds; });
}

void BKE_mesh_transform(Mesh *me, const float mat[4][4], bool do_keys)
{
  MutableSpan<float3> positions = me->vert_positions_for_write();

  for (float3 &position : positions) {
    mul_m4_v3(mat, position);
  }

  if (do_keys && me->key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &me->key->block) {
      float *fp = (float *)kb->data;
      for (int i = kb->totelem; i--; fp += 3) {
        mul_m4_v3(mat, fp);
      }
    }
  }

  /* don't update normals, caller can do this explicitly.
   * We do update loop normals though, those may not be auto-generated
   * (see e.g. STL import script)! */
  float(*lnors)[3] = (float(*)[3])CustomData_get_layer_for_write(
      &me->ldata, CD_NORMAL, me->totloop);
  if (lnors) {
    float m3[3][3];

    copy_m3_m4(m3, mat);
    normalize_m3(m3);
    for (int i = 0; i < me->totloop; i++, lnors++) {
      mul_m3_v3(m3, *lnors);
    }
  }
  BKE_mesh_tag_positions_changed(me);
}

static void translate_positions(MutableSpan<float3> positions, const float3 &translation)
{
  using namespace blender;
  threading::parallel_for(positions.index_range(), 2048, [&](const IndexRange range) {
    for (float3 &position : positions.slice(range)) {
      position += translation;
    }
  });
}

void BKE_mesh_translate(Mesh *mesh, const float offset[3], const bool do_keys)
{
  using namespace blender;
  if (math::is_zero(float3(offset))) {
    return;
  }

  std::optional<Bounds<float3>> bounds;
  if (mesh->runtime->bounds_cache.is_cached()) {
    bounds = mesh->runtime->bounds_cache.data();
  }

  translate_positions(mesh->vert_positions_for_write(), offset);
  if (do_keys && mesh->key) {
    LISTBASE_FOREACH (KeyBlock *, kb, &mesh->key->block) {
      translate_positions({static_cast<float3 *>(kb->data), kb->totelem}, offset);
    }
  }

  BKE_mesh_tag_positions_changed_uniformly(mesh);

  if (bounds) {
    bounds->min += offset;
    bounds->max += offset;
    mesh->bounds_set_eager(*bounds);
  }
}

void BKE_mesh_tessface_clear(Mesh *mesh)
{
  mesh_tessface_clear_intern(mesh, true);
}

/* -------------------------------------------------------------------- */
/* MSelect functions (currently used in weight paint mode) */

void BKE_mesh_mselect_clear(Mesh *me)
{
  MEM_SAFE_FREE(me->mselect);
  me->totselect = 0;
}

void BKE_mesh_mselect_validate(Mesh *me)
{
  using namespace blender;
  using namespace blender::bke;
  MSelect *mselect_src, *mselect_dst;
  int i_src, i_dst;

  if (me->totselect == 0) {
    return;
  }

  mselect_src = me->mselect;
  mselect_dst = (MSelect *)MEM_malloc_arrayN(
      (me->totselect), sizeof(MSelect), "Mesh selection history");

  const AttributeAccessor attributes = me->attributes();
  const VArray<bool> select_vert = *attributes.lookup_or_default<bool>(
      ".select_vert", ATTR_DOMAIN_POINT, false);
  const VArray<bool> select_edge = *attributes.lookup_or_default<bool>(
      ".select_edge", ATTR_DOMAIN_EDGE, false);
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", ATTR_DOMAIN_FACE, false);

  for (i_src = 0, i_dst = 0; i_src < me->totselect; i_src++) {
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
  else if (i_dst != me->totselect) {
    mselect_dst = (MSelect *)MEM_reallocN(mselect_dst, sizeof(MSelect) * i_dst);
  }

  me->totselect = i_dst;
  me->mselect = mselect_dst;
}

int BKE_mesh_mselect_find(Mesh *me, int index, int type)
{
  BLI_assert(ELEM(type, ME_VSEL, ME_ESEL, ME_FSEL));

  for (int i = 0; i < me->totselect; i++) {
    if ((me->mselect[i].index == index) && (me->mselect[i].type == type)) {
      return i;
    }
  }

  return -1;
}

int BKE_mesh_mselect_active_get(Mesh *me, int type)
{
  BLI_assert(ELEM(type, ME_VSEL, ME_ESEL, ME_FSEL));

  if (me->totselect) {
    if (me->mselect[me->totselect - 1].type == type) {
      return me->mselect[me->totselect - 1].index;
    }
  }
  return -1;
}

void BKE_mesh_mselect_active_set(Mesh *me, int index, int type)
{
  const int msel_index = BKE_mesh_mselect_find(me, index, type);

  if (msel_index == -1) {
    /* add to the end */
    me->mselect = (MSelect *)MEM_reallocN(me->mselect, sizeof(MSelect) * (me->totselect + 1));
    me->mselect[me->totselect].index = index;
    me->mselect[me->totselect].type = type;
    me->totselect++;
  }
  else if (msel_index != me->totselect - 1) {
    /* move to the end */
    std::swap(me->mselect[msel_index], me->mselect[me->totselect - 1]);
  }

  BLI_assert((me->mselect[me->totselect - 1].index == index) &&
             (me->mselect[me->totselect - 1].type == type));
}

void BKE_mesh_count_selected_items(const Mesh *mesh, int r_count[3])
{
  r_count[0] = r_count[1] = r_count[2] = 0;
  if (mesh->edit_mesh) {
    BMesh *bm = mesh->edit_mesh->bm;
    r_count[0] = bm->totvertsel;
    r_count[1] = bm->totedgesel;
    r_count[2] = bm->totfacesel;
  }
  /* We could support faces in paint modes. */
}

void BKE_mesh_vert_coords_get(const Mesh *mesh, float (*vert_coords)[3])
{
  blender::bke::AttributeAccessor attributes = mesh->attributes();
  VArray<float3> positions = *attributes.lookup_or_default(
      "position", ATTR_DOMAIN_POINT, float3(0));
  positions.materialize({(float3 *)vert_coords, mesh->totvert});
}

float (*BKE_mesh_vert_coords_alloc(const Mesh *mesh, int *r_vert_len))[3]
{
  float(*vert_coords)[3] = (float(*)[3])MEM_mallocN(sizeof(float[3]) * mesh->totvert, __func__);
  BKE_mesh_vert_coords_get(mesh, vert_coords);
  if (r_vert_len) {
    *r_vert_len = mesh->totvert;
  }
  return vert_coords;
}

void BKE_mesh_vert_coords_apply(Mesh *mesh, const float (*vert_coords)[3])
{
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  for (const int i : positions.index_range()) {
    copy_v3_v3(positions[i], vert_coords[i]);
  }
  BKE_mesh_tag_positions_changed(mesh);
}

void BKE_mesh_vert_coords_apply_with_mat4(Mesh *mesh,
                                          const float (*vert_coords)[3],
                                          const float mat[4][4])
{
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  for (const int i : positions.index_range()) {
    mul_v3_m4v3(positions[i], mat, vert_coords[i]);
  }
  BKE_mesh_tag_positions_changed(mesh);
}

static float (*ensure_corner_normal_layer(Mesh &mesh))[3]
{
  float(*r_loop_normals)[3];
  if (CustomData_has_layer(&mesh.ldata, CD_NORMAL)) {
    r_loop_normals = (float(*)[3])CustomData_get_layer_for_write(
        &mesh.ldata, CD_NORMAL, mesh.totloop);
    memset(r_loop_normals, 0, sizeof(float[3]) * mesh.totloop);
  }
  else {
    r_loop_normals = (float(*)[3])CustomData_add_layer(
        &mesh.ldata, CD_NORMAL, CD_SET_DEFAULT, mesh.totloop);
    CustomData_set_layer_flag(&mesh.ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
  }
  return r_loop_normals;
}

void BKE_mesh_calc_normals_split_ex(Mesh *mesh,
                                    MLoopNorSpaceArray *r_lnors_spacearr,
                                    float (*r_corner_normals)[3])
{
  /* Note that we enforce computing clnors when the clnor space array is requested by caller here.
   * However, we obviously only use the auto-smooth angle threshold
   * only in case auto-smooth is enabled. */
  const bool use_split_normals = (r_lnors_spacearr != nullptr) ||
                                 ((mesh->flag & ME_AUTOSMOOTH) != 0);
  const float split_angle = (mesh->flag & ME_AUTOSMOOTH) != 0 ? mesh->smoothresh : float(M_PI);

  /* may be nullptr */
  blender::short2 *clnors = (blender::short2 *)CustomData_get_layer_for_write(
      &mesh->ldata, CD_CUSTOMLOOPNORMAL, mesh->totloop);
  const bool *sharp_edges = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->edata, CD_PROP_BOOL, "sharp_edge"));
  const bool *sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(&mesh->pdata, CD_PROP_BOOL, "sharp_face"));

  blender::bke::mesh::normals_calc_loop(
      mesh->vert_positions(),
      mesh->edges(),
      mesh->polys(),
      mesh->corner_verts(),
      mesh->corner_edges(),
      {},
      mesh->vert_normals(),
      mesh->poly_normals(),
      sharp_edges,
      sharp_faces,
      use_split_normals,
      split_angle,
      clnors,
      nullptr,
      {reinterpret_cast<float3 *>(r_corner_normals), mesh->totloop});
}

void BKE_mesh_calc_normals_split(Mesh *mesh)
{
  BKE_mesh_calc_normals_split_ex(mesh, nullptr, ensure_corner_normal_layer(*mesh));
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
    mesh->runtime->mesh_eval->edit_mesh = nullptr;
    BKE_id_free(nullptr, mesh->runtime->mesh_eval);
    mesh->runtime->mesh_eval = nullptr;
  }
  if (DEG_is_active(depsgraph)) {
    Mesh *mesh_orig = reinterpret_cast<Mesh *>(DEG_get_original_id(&mesh->id));
    if (mesh->texspace_flag & ME_TEXSPACE_FLAG_AUTO_EVALUATED) {
      mesh_orig->texspace_flag |= ME_TEXSPACE_FLAG_AUTO_EVALUATED;
      copy_v3_v3(mesh_orig->texspace_location, mesh->texspace_location);
      copy_v3_v3(mesh_orig->texspace_size, mesh->texspace_size);
    }
  }
}
