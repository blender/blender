/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_math_vector.h"
#include "BLI_rand.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "DEG_depsgraph_query.hh"

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_scene.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_ui_common.hh"

static void init_data(ModifierData *md)
{
  BuildModifierData *bmd = (BuildModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(bmd, modifier));

  MEMCPY_STRUCT_AFTER(bmd, DNA_struct_default_get(BuildModifierData), modifier);
}

static bool depends_on_time(Scene * /*scene*/, ModifierData * /*md*/)
{
  return true;
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  Mesh *result;
  BuildModifierData *bmd = (BuildModifierData *)md;
  int i, j, k;
  int faces_dst_num, edges_dst_num, loops_dst_num = 0;
  float frac;
  GHashIterator gh_iter;
  /* maps vert indices in old mesh to indices in new mesh */
  GHash *vertHash = BLI_ghash_int_new("build ve apply gh");
  /* maps edge indices in new mesh to indices in old mesh */
  GHash *edgeHash = BLI_ghash_int_new("build ed apply gh");
  /* maps edge indices in old mesh to indices in new mesh */
  GHash *edgeHash2 = BLI_ghash_int_new("build ed apply gh");

  const int vert_src_num = mesh->verts_num;
  const blender::Span<blender::int2> edges_src = mesh->edges();
  const blender::OffsetIndices faces_src = mesh->faces();
  const blender::Span<int> corner_verts_src = mesh->corner_verts();
  const blender::Span<int> corner_edges_src = mesh->corner_edges();

  int *vertMap = MEM_malloc_arrayN<int>(size_t(vert_src_num), __func__);
  int *edgeMap = MEM_malloc_arrayN<int>(size_t(edges_src.size()), __func__);
  int *faceMap = MEM_malloc_arrayN<int>(size_t(faces_src.size()), __func__);

  range_vn_i(vertMap, vert_src_num, 0);
  range_vn_i(edgeMap, edges_src.size(), 0);
  range_vn_i(faceMap, faces_src.size(), 0);

  Scene *scene = DEG_get_input_scene(ctx->depsgraph);
  frac = (BKE_scene_ctime_get(scene) - bmd->start) / bmd->length;
  CLAMP(frac, 0.0f, 1.0f);
  if (bmd->flag & MOD_BUILD_FLAG_REVERSE) {
    frac = 1.0f - frac;
  }

  faces_dst_num = faces_src.size() * frac;
  edges_dst_num = edges_src.size() * frac;

  /* if there's at least one face, build based on faces */
  if (faces_dst_num) {
    uintptr_t hash_num, hash_num_alt;

    if (bmd->flag & MOD_BUILD_FLAG_RANDOMIZE) {
      BLI_array_randomize(faceMap, sizeof(*faceMap), faces_src.size(), bmd->seed);
    }

    /* get the set of all vert indices that will be in the final mesh,
     * mapped to the new indices
     */
    hash_num = 0;
    for (i = 0; i < faces_dst_num; i++) {
      const blender::IndexRange face = faces_src[faceMap[i]];
      for (j = 0; j < face.size(); j++) {
        const int vert_i = corner_verts_src[face[j]];
        void **val_p;
        if (!BLI_ghash_ensure_p(vertHash, POINTER_FROM_INT(vert_i), &val_p)) {
          *val_p = (void *)hash_num;
          hash_num++;
        }
      }

      loops_dst_num += face.size();
    }
    BLI_assert(hash_num == BLI_ghash_len(vertHash));

    /* get the set of edges that will be in the new mesh (i.e. all edges
     * that have both verts in the new mesh)
     */
    hash_num = 0;
    hash_num_alt = 0;
    for (i = 0; i < edges_src.size(); i++, hash_num_alt++) {
      const blender::int2 &edge = edges_src[i];

      if (BLI_ghash_haskey(vertHash, POINTER_FROM_INT(edge[0])) &&
          BLI_ghash_haskey(vertHash, POINTER_FROM_INT(edge[1])))
      {
        BLI_ghash_insert(edgeHash, (void *)hash_num, (void *)hash_num_alt);
        BLI_ghash_insert(edgeHash2, (void *)hash_num_alt, (void *)hash_num);
        hash_num++;
      }
    }
    BLI_assert(hash_num == BLI_ghash_len(edgeHash));
  }
  else if (edges_dst_num) {
    uintptr_t hash_num;

    if (bmd->flag & MOD_BUILD_FLAG_RANDOMIZE) {
      BLI_array_randomize(edgeMap, sizeof(*edgeMap), edges_src.size(), bmd->seed);
    }

    /* get the set of all vert indices that will be in the final mesh,
     * mapped to the new indices
     */
    const blender::int2 *edges = edges_src.data();
    hash_num = 0;
    BLI_assert(hash_num == BLI_ghash_len(vertHash));
    for (i = 0; i < edges_dst_num; i++) {
      void **val_p;
      const blender::int2 &edge = edges[edgeMap[i]];

      if (!BLI_ghash_ensure_p(vertHash, POINTER_FROM_INT(edge[0]), &val_p)) {
        *val_p = (void *)hash_num;
        hash_num++;
      }
      if (!BLI_ghash_ensure_p(vertHash, POINTER_FROM_INT(edge[1]), &val_p)) {
        *val_p = (void *)hash_num;
        hash_num++;
      }
    }
    BLI_assert(hash_num == BLI_ghash_len(vertHash));

    /* get the set of edges that will be in the new mesh */
    for (i = 0; i < edges_dst_num; i++) {
      j = BLI_ghash_len(edgeHash);

      BLI_ghash_insert(edgeHash, POINTER_FROM_INT(j), POINTER_FROM_INT(edgeMap[i]));
      BLI_ghash_insert(edgeHash2, POINTER_FROM_INT(edgeMap[i]), POINTER_FROM_INT(j));
    }
  }
  else {
    int verts_num = vert_src_num * frac;

    if (bmd->flag & MOD_BUILD_FLAG_RANDOMIZE) {
      BLI_array_randomize(vertMap, sizeof(*vertMap), vert_src_num, bmd->seed);
    }

    /* get the set of all vert indices that will be in the final mesh,
     * mapped to the new indices
     */
    for (i = 0; i < verts_num; i++) {
      BLI_ghash_insert(vertHash, POINTER_FROM_INT(vertMap[i]), POINTER_FROM_INT(i));
    }
  }

  /* now we know the number of verts, edges and faces, we can create the mesh. */
  result = BKE_mesh_new_nomain_from_template(
      mesh, BLI_ghash_len(vertHash), BLI_ghash_len(edgeHash), faces_dst_num, loops_dst_num);
  blender::MutableSpan<blender::int2> result_edges = result->edges_for_write();
  blender::MutableSpan<int> result_face_offsets = result->face_offsets_for_write();
  blender::MutableSpan<int> result_corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> result_corner_edges = result->corner_edges_for_write();

  /* copy the vertices across */
  GHASH_ITER (gh_iter, vertHash) {
    int oldIndex = POINTER_AS_INT(BLI_ghashIterator_getKey(&gh_iter));
    int newIndex = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));
    CustomData_copy_data(&mesh->vert_data, &result->vert_data, oldIndex, newIndex, 1);
  }

  /* copy the edges across, remapping indices */
  for (i = 0; i < BLI_ghash_len(edgeHash); i++) {
    blender::int2 source;
    blender::int2 *dest;
    int oldIndex = POINTER_AS_INT(BLI_ghash_lookup(edgeHash, POINTER_FROM_INT(i)));

    source = edges_src[oldIndex];
    dest = &result_edges[i];

    source[0] = POINTER_AS_INT(BLI_ghash_lookup(vertHash, POINTER_FROM_INT(source[0])));
    source[1] = POINTER_AS_INT(BLI_ghash_lookup(vertHash, POINTER_FROM_INT(source[1])));

    CustomData_copy_data(&mesh->edge_data, &result->edge_data, oldIndex, i, 1);
    *dest = source;
  }

  /* copy the faces across, remapping indices */
  k = 0;
  for (i = 0; i < faces_dst_num; i++) {
    const blender::IndexRange src_face = faces_src[faceMap[i]];
    result_face_offsets[i] = k;

    CustomData_copy_data(&mesh->face_data, &result->face_data, faceMap[i], i, 1);

    CustomData_copy_data(
        &mesh->corner_data, &result->corner_data, src_face.start(), k, src_face.size());

    for (j = 0; j < src_face.size(); j++, k++) {
      const int vert_src = corner_verts_src[src_face[j]];
      const int edge_src = corner_edges_src[src_face[j]];
      result_corner_verts[k] = POINTER_AS_INT(
          BLI_ghash_lookup(vertHash, POINTER_FROM_INT(vert_src)));
      result_corner_edges[k] = POINTER_AS_INT(
          BLI_ghash_lookup(edgeHash2, POINTER_FROM_INT(edge_src)));
    }
  }

  BLI_ghash_free(vertHash, nullptr, nullptr);
  BLI_ghash_free(edgeHash, nullptr, nullptr);
  BLI_ghash_free(edgeHash2, nullptr, nullptr);

  MEM_freeN(vertMap);
  MEM_freeN(edgeMap);
  MEM_freeN(faceMap);

  /* TODO(sybren): also copy flags & tags? */
  return result;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->prop(ptr, "frame_start", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "frame_duration", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(ptr, "use_reverse", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void random_panel_header_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->prop(ptr, "use_random_order", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void random_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, nullptr);

  layout->use_property_split_set(true);

  layout->active_set(RNA_boolean_get(ptr, "use_random_order"));
  layout->prop(ptr, "seed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Build, panel_draw);
  modifier_subpanel_register(
      region_type, "randomize", "", random_panel_header_draw, random_panel_draw, panel_type);
}

ModifierTypeInfo modifierType_Build = {
    /*idname*/ "Build",
    /*name*/ N_("Build"),
    /*struct_name*/ "BuildModifierData",
    /*struct_size*/ sizeof(BuildModifierData),
    /*srna*/ &RNA_BuildModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs,
    /*icon*/ ICON_MOD_BUILD,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ depends_on_time,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
