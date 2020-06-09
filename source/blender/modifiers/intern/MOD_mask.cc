/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.h" /* BKE_pose_channel_find_name */
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

/* SpaceType struct has a member called 'new' which obviously conflicts with C++
 * so temporarily redefining the new keyword to make it compile. */
#define new extern_new
#include "BKE_screen.h"
#undef new

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLI_array.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_vector.hh"

using blender::Array;
using blender::ArrayRef;
using blender::IndexRange;
using blender::ListBaseWrapper;
using blender::MutableArrayRef;
using blender::Vector;

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  MaskModifierData *mmd = (MaskModifierData *)md;
  walk(userData, ob, &mmd->ob_arm, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MaskModifierData *mmd = (MaskModifierData *)md;
  if (mmd->ob_arm) {
    bArmature *arm = (bArmature *)mmd->ob_arm->data;
    /* Tag relationship in depsgraph, but also on the armature. */
    /* TODO(sergey): Is it a proper relation here? */
    DEG_add_object_relation(ctx->node, mmd->ob_arm, DEG_OB_COMP_TRANSFORM, "Mask Modifier");
    arm->flag |= ARM_HAS_VIZ_DEPS;
    DEG_add_modifier_to_transform_relation(ctx->node, "Mask Modifier");
  }
}

/* A vertex will be in the mask if a selected bone influences it more than a certain threshold. */
static void compute_vertex_mask__armature_mode(MDeformVert *dvert,
                                               Object *ob,
                                               Object *armature_ob,
                                               float threshold,
                                               MutableArrayRef<bool> r_vertex_mask)
{
  /* Element i is true if there is a selected bone that uses vertex group i. */
  Vector<bool> selected_bone_uses_group;

  for (bDeformGroup *def : ListBaseWrapper<bDeformGroup>(ob->defbase)) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(armature_ob->pose, def->name);
    bool bone_for_group_exists = pchan && pchan->bone && (pchan->bone->flag & BONE_SELECTED);
    selected_bone_uses_group.append(bone_for_group_exists);
  }

  ArrayRef<bool> use_vertex_group = selected_bone_uses_group;

  for (int i : r_vertex_mask.index_range()) {
    ArrayRef<MDeformWeight> weights(dvert[i].dw, dvert[i].totweight);
    r_vertex_mask[i] = false;

    /* check the groups that vertex is assigned to, and see if it was any use */
    for (const MDeformWeight &dw : weights) {
      if (use_vertex_group.get(dw.def_nr, false)) {
        if (dw.weight > threshold) {
          r_vertex_mask[i] = true;
          break;
        }
      }
    }
  }
}

/* A vertex will be in the mask if the vertex group influences it more than a certain threshold. */
static void compute_vertex_mask__vertex_group_mode(MDeformVert *dvert,
                                                   int defgrp_index,
                                                   float threshold,
                                                   MutableArrayRef<bool> r_vertex_mask)
{
  for (int i : r_vertex_mask.index_range()) {
    const bool found = BKE_defvert_find_weight(&dvert[i], defgrp_index) > threshold;
    r_vertex_mask[i] = found;
  }
}

static void invert_boolean_array(MutableArrayRef<bool> array)
{
  for (bool &value : array) {
    value = !value;
  }
}

static void compute_masked_vertices(ArrayRef<bool> vertex_mask,
                                    MutableArrayRef<int> r_vertex_map,
                                    uint *r_num_masked_vertices)
{
  BLI_assert(vertex_mask.size() == r_vertex_map.size());

  uint num_masked_vertices = 0;
  for (uint i_src : r_vertex_map.index_range()) {
    if (vertex_mask[i_src]) {
      r_vertex_map[i_src] = num_masked_vertices;
      num_masked_vertices++;
    }
    else {
      r_vertex_map[i_src] = -1;
    }
  }

  *r_num_masked_vertices = num_masked_vertices;
}

static void computed_masked_edges(const Mesh *mesh,
                                  ArrayRef<bool> vertex_mask,
                                  MutableArrayRef<int> r_edge_map,
                                  uint *r_num_masked_edges)
{
  BLI_assert(mesh->totedge == r_edge_map.size());

  uint num_masked_edges = 0;
  for (int i : IndexRange(mesh->totedge)) {
    const MEdge &edge = mesh->medge[i];

    /* only add if both verts will be in new mesh */
    if (vertex_mask[edge.v1] && vertex_mask[edge.v2]) {
      r_edge_map[i] = num_masked_edges;
      num_masked_edges++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_num_masked_edges = num_masked_edges;
}

static void computed_masked_polygons(const Mesh *mesh,
                                     ArrayRef<bool> vertex_mask,
                                     Vector<int> &r_masked_poly_indices,
                                     Vector<int> &r_loop_starts,
                                     uint *r_num_masked_polys,
                                     uint *r_num_masked_loops)
{
  BLI_assert(mesh->totvert == vertex_mask.size());

  r_masked_poly_indices.reserve(mesh->totpoly);
  r_loop_starts.reserve(mesh->totloop);

  uint num_masked_loops = 0;
  for (int i : IndexRange(mesh->totpoly)) {
    const MPoly &poly_src = mesh->mpoly[i];

    bool all_verts_in_mask = true;
    ArrayRef<MLoop> loops_src(&mesh->mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (!vertex_mask[loop.v]) {
        all_verts_in_mask = false;
        break;
      }
    }

    if (all_verts_in_mask) {
      r_masked_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_masked_loops);
      num_masked_loops += poly_src.totloop;
    }
  }

  *r_num_masked_polys = r_masked_poly_indices.size();
  *r_num_masked_loops = num_masked_loops;
}

static void copy_masked_vertices_to_new_mesh(const Mesh &src_mesh,
                                             Mesh &dst_mesh,
                                             ArrayRef<int> vertex_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  for (const int i_src : vertex_map.index_range()) {
    const int i_dst = vertex_map[i_src];
    if (i_dst == -1) {
      continue;
    }

    const MVert &v_src = src_mesh.mvert[i_src];
    MVert &v_dst = dst_mesh.mvert[i_dst];

    v_dst = v_src;
    CustomData_copy_data(&src_mesh.vdata, &dst_mesh.vdata, i_src, i_dst, 1);
  }
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          ArrayRef<int> vertex_map,
                                          ArrayRef<int> edge_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  BLI_assert(src_mesh.totedge == edge_map.size());
  for (const int i_src : IndexRange(src_mesh.totedge)) {
    const int i_dst = edge_map[i_src];
    if (i_dst == -1) {
      continue;
    }

    const MEdge &e_src = src_mesh.medge[i_src];
    MEdge &e_dst = dst_mesh.medge[i_dst];

    CustomData_copy_data(&src_mesh.edata, &dst_mesh.edata, i_src, i_dst, 1);
    e_dst = e_src;
    e_dst.v1 = vertex_map[e_src.v1];
    e_dst.v2 = vertex_map[e_src.v2];
  }
}

static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          ArrayRef<int> vertex_map,
                                          ArrayRef<int> edge_map,
                                          ArrayRef<int> masked_poly_indices,
                                          ArrayRef<int> new_loop_starts)
{
  for (const int i_dst : masked_poly_indices.index_range()) {
    const int i_src = masked_poly_indices[i_dst];

    const MPoly &mp_src = src_mesh.mpoly[i_src];
    MPoly &mp_dst = dst_mesh.mpoly[i_dst];
    const int i_ml_src = mp_src.loopstart;
    const int i_ml_dst = new_loop_starts[i_dst];

    CustomData_copy_data(&src_mesh.pdata, &dst_mesh.pdata, i_src, i_dst, 1);
    CustomData_copy_data(&src_mesh.ldata, &dst_mesh.ldata, i_ml_src, i_ml_dst, mp_src.totloop);

    const MLoop *ml_src = src_mesh.mloop + i_ml_src;
    MLoop *ml_dst = dst_mesh.mloop + i_ml_dst;

    mp_dst = mp_src;
    mp_dst.loopstart = i_ml_dst;
    for (int i : IndexRange(mp_src.totloop)) {
      ml_dst[i].v = vertex_map[ml_src[i].v];
      ml_dst[i].e = edge_map[ml_src[i].e];
    }
  }
}

/* Components of the algorithm:
 * 1. Figure out which vertices should be present in the output mesh.
 * 2. Find edges and polygons only using those vertices.
 * 3. Create a new mesh that only uses the found vertices, edges and polygons.
 */
static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  MaskModifierData *mmd = (MaskModifierData *)md;
  Object *ob = ctx->object;
  const bool invert_mask = mmd->flag & MOD_MASK_INV;

  /* Return empty or input mesh when there are no vertex groups. */
  MDeformVert *dvert = (MDeformVert *)CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
  if (dvert == NULL) {
    return invert_mask ? mesh : BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0, 0);
  }

  /* Quick test to see if we can return early. */
  if (!(ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP)) || (mesh->totvert == 0) ||
      BLI_listbase_is_empty(&ob->defbase)) {
    return mesh;
  }

  Array<bool> vertex_mask;
  if (mmd->mode == MOD_MASK_MODE_ARM) {
    Object *armature_ob = mmd->ob_arm;

    /* Return input mesh if there is no armature with bones. */
    if (ELEM(NULL, armature_ob, armature_ob->pose, ob->defbase.first)) {
      return mesh;
    }

    vertex_mask = Array<bool>(mesh->totvert);
    compute_vertex_mask__armature_mode(dvert, ob, armature_ob, mmd->threshold, vertex_mask);
  }
  else {
    int defgrp_index = BKE_object_defgroup_name_index(ob, mmd->vgroup);

    /* Return input mesh if the vertex group does not exist. */
    if (defgrp_index == -1) {
      return mesh;
    }

    vertex_mask = Array<bool>(mesh->totvert);
    compute_vertex_mask__vertex_group_mode(dvert, defgrp_index, mmd->threshold, vertex_mask);
  }

  if (invert_mask) {
    invert_boolean_array(vertex_mask);
  }

  Array<int> vertex_map(mesh->totvert);
  uint num_masked_vertices;
  compute_masked_vertices(vertex_mask, vertex_map, &num_masked_vertices);

  Array<int> edge_map(mesh->totedge);
  uint num_masked_edges;
  computed_masked_edges(mesh, vertex_mask, edge_map, &num_masked_edges);

  Vector<int> masked_poly_indices;
  Vector<int> new_loop_starts;
  uint num_masked_polys;
  uint num_masked_loops;
  computed_masked_polygons(mesh,
                           vertex_mask,
                           masked_poly_indices,
                           new_loop_starts,
                           &num_masked_polys,
                           &num_masked_loops);

  Mesh *result = BKE_mesh_new_nomain_from_template(
      mesh, num_masked_vertices, num_masked_edges, 0, num_masked_loops, num_masked_polys);

  copy_masked_vertices_to_new_mesh(*mesh, *result, vertex_map);
  copy_masked_edges_to_new_mesh(*mesh, *result, vertex_map, edge_map);
  copy_masked_polys_to_new_mesh(
      *mesh, *result, vertex_map, edge_map, masked_poly_indices, new_loop_starts);

  BKE_mesh_calc_edges_loose(result);
  /* Tag to recalculate normals later. */
  result->runtime.cd_dirty_vert |= CD_MASK_NORMAL;

  return result;
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  MaskModifierData *mmd = (MaskModifierData *)md;

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the armature is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return mmd->ob_arm && mmd->ob_arm->type != OB_ARMATURE;
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *sub, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  PointerRNA ob_ptr;
  modifier_panel_get_property_pointers(C, panel, &ob_ptr, &ptr);

  int mode = RNA_enum_get(&ptr, "mode");

  uiItemR(layout, &ptr, "mode", UI_ITEM_R_EXPAND, NULL, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (mode == MOD_MASK_MODE_ARM) {
    row = uiLayoutRow(layout, true);
    uiItemR(row, &ptr, "armature", 0, NULL, ICON_NONE);
    sub = uiLayoutRow(row, true);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, &ptr, "invert_vertex_group", 0, "", ICON_ARROW_LEFTRIGHT);
  }
  else if (mode == MOD_MASK_MODE_VGROUP) {
    modifier_vgroup_ui(layout, &ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);
  }

  uiItemR(layout, &ptr, "threshold", 0, NULL, ICON_NONE);

  modifier_panel_end(layout, &ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Mask, panel_draw);
}

ModifierTypeInfo modifierType_Mask = {
    /* name */ "Mask",
    /* structName */ "MaskModifierData",
    /* structSize */ sizeof(MaskModifierData),
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */
    (ModifierTypeFlag)(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
                       eModifierTypeFlag_SupportsEditmode),

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ NULL,
    /* modifyPointCloud */ NULL,
    /* modifyVolume */ NULL,

    /* initData */ NULL,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ NULL,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
    /* panelRegister */ panelRegister,
};
