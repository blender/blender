/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_math_vector.h"
#include "BLI_rand.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_screen_types.h"

#include "DEG_depsgraph_query.hh"

#include "BKE_attribute_legacy_convert.hh"
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
  using namespace blender;
  Mesh *result;
  BuildModifierData *bmd = (BuildModifierData *)md;
  int i, j, k;
  int faces_dst_num, edges_dst_num, loops_dst_num = 0;
  float frac;
  /* maps vert indices in old mesh to indices in new mesh */
  blender::Map<int, int> vertHash;
  /* maps edge indices in new mesh to indices in old mesh */
  blender::Map<int, int> edgeHash;
  /* maps edge indices in old mesh to indices in new mesh */
  blender::Map<int, int> edgeHash2;

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
        if (vertHash.add(vert_i, hash_num)) {
          hash_num++;
        }
      }

      loops_dst_num += face.size();
    }
    BLI_assert(hash_num == vertHash.size());

    /* get the set of edges that will be in the new mesh (i.e. all edges
     * that have both verts in the new mesh)
     */
    hash_num = 0;
    hash_num_alt = 0;
    for (i = 0; i < edges_src.size(); i++, hash_num_alt++) {
      const blender::int2 &edge = edges_src[i];

      if (vertHash.contains(edge[0]) && vertHash.contains(edge[1])) {
        edgeHash.add(hash_num, hash_num_alt);
        edgeHash2.add(hash_num_alt, hash_num);
        hash_num++;
      }
    }
    BLI_assert(hash_num == edgeHash.size());
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
    BLI_assert(hash_num == vertHash.size());
    for (i = 0; i < edges_dst_num; i++) {
      const blender::int2 &edge = edges[edgeMap[i]];
      if (vertHash.add(edge[0], hash_num)) {
        hash_num++;
      }
      if (vertHash.add(edge[1], hash_num)) {
        hash_num++;
      }
    }
    BLI_assert(hash_num == vertHash.size());

    /* get the set of edges that will be in the new mesh */
    for (i = 0; i < edges_dst_num; i++) {
      j = edgeHash.size();

      edgeHash.add(j, edgeMap[i]);
      edgeHash2.add(edgeMap[i], j);
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
      vertHash.add(vertMap[i], i);
    }
  }

  /* now we know the number of verts, edges and faces, we can create the mesh. */
  result = BKE_mesh_new_nomain_from_template(
      mesh, vertHash.size(), edgeHash.size(), faces_dst_num, loops_dst_num);
  blender::MutableSpan<blender::int2> result_edges = result->edges_for_write();
  blender::MutableSpan<int> result_face_offsets = result->face_offsets_for_write();
  blender::MutableSpan<int> result_corner_verts = result->corner_verts_for_write();
  blender::MutableSpan<int> result_corner_edges = result->corner_edges_for_write();

  bke::LegacyMeshInterpolator vert_interp(*mesh, *result, bke::AttrDomain::Point);
  bke::LegacyMeshInterpolator edge_interp(*mesh, *result, bke::AttrDomain::Edge);
  bke::LegacyMeshInterpolator face_interp(*mesh, *result, bke::AttrDomain::Face);
  bke::LegacyMeshInterpolator corner_interp(*mesh, *result, bke::AttrDomain::Corner);

  /* copy the vertices across */
  for (const auto &item : vertHash.items()) {
    const int oldIndex = item.key;
    const int newIndex = item.value;
    vert_interp.copy(oldIndex, newIndex, 1);
  }

  /* copy the edges across, remapping indices */
  for (i = 0; i < edgeHash.size(); i++) {
    blender::int2 source;
    blender::int2 *dest;
    int oldIndex = edgeHash.lookup(i);

    source = edges_src[oldIndex];
    dest = &result_edges[i];

    source[0] = vertHash.lookup(source[0]);
    source[1] = vertHash.lookup(source[1]);

    edge_interp.copy(oldIndex, i, 1);
    *dest = source;
  }

  /* copy the faces across, remapping indices */
  k = 0;
  for (i = 0; i < faces_dst_num; i++) {
    const blender::IndexRange src_face = faces_src[faceMap[i]];
    result_face_offsets[i] = k;

    face_interp.copy(faceMap[i], i, 1);

    corner_interp.copy(src_face.start(), k, src_face.size());

    for (j = 0; j < src_face.size(); j++, k++) {
      const int vert_src = corner_verts_src[src_face[j]];
      const int edge_src = corner_edges_src[src_face[j]];
      result_corner_verts[k] = vertHash.lookup(vert_src);
      result_corner_edges[k] = edgeHash2.lookup(edge_src);
    }
  }

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
