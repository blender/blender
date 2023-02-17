/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_array_utils.hh"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_defaults.h"
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
#include "BKE_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLI_array.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_vector.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::ListBaseWrapper;
using blender::MutableSpan;
using blender::Span;
using blender::Vector;

static void initData(ModifierData *md)
{
  MaskModifierData *mmd = (MaskModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(MaskModifierData), modifier);
}

static void requiredDataMask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);
  walk(userData, ob, (ID **)&mmd->ob_arm, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);
  if (mmd->ob_arm) {
    bArmature *arm = (bArmature *)mmd->ob_arm->data;
    /* Tag relationship in depsgraph, but also on the armature. */
    /* TODO(sergey): Is it a proper relation here? */
    DEG_add_object_relation(ctx->node, mmd->ob_arm, DEG_OB_COMP_TRANSFORM, "Mask Modifier");
    arm->flag |= ARM_HAS_VIZ_DEPS;
    DEG_add_depends_on_transform_relation(ctx->node, "Mask Modifier");
  }
}

/* A vertex will be in the mask if a selected bone influences it more than a certain threshold. */
static void compute_vertex_mask__armature_mode(const MDeformVert *dvert,
                                               Mesh *mesh,
                                               Object *armature_ob,
                                               float threshold,
                                               MutableSpan<bool> r_vertex_mask)
{
  /* Element i is true if there is a selected bone that uses vertex group i. */
  Vector<bool> selected_bone_uses_group;

  LISTBASE_FOREACH (bDeformGroup *, def, &mesh->vertex_group_names) {
    bPoseChannel *pchan = BKE_pose_channel_find_name(armature_ob->pose, def->name);
    bool bone_for_group_exists = pchan && pchan->bone && (pchan->bone->flag & BONE_SELECTED);
    selected_bone_uses_group.append(bone_for_group_exists);
  }

  Span<bool> use_vertex_group = selected_bone_uses_group;

  for (int i : r_vertex_mask.index_range()) {
    Span<MDeformWeight> weights(dvert[i].dw, dvert[i].totweight);
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
static void compute_vertex_mask__vertex_group_mode(const MDeformVert *dvert,
                                                   int defgrp_index,
                                                   float threshold,
                                                   MutableSpan<bool> r_vertex_mask)
{
  for (int i : r_vertex_mask.index_range()) {
    const bool found = BKE_defvert_find_weight(&dvert[i], defgrp_index) > threshold;
    r_vertex_mask[i] = found;
  }
}

static void compute_masked_verts(Span<bool> vertex_mask,
                                 MutableSpan<int> r_vertex_map,
                                 uint *r_verts_masked_num)
{
  BLI_assert(vertex_mask.size() == r_vertex_map.size());

  uint verts_masked_num = 0;
  for (uint i_src : r_vertex_map.index_range()) {
    if (vertex_mask[i_src]) {
      r_vertex_map[i_src] = verts_masked_num;
      verts_masked_num++;
    }
    else {
      r_vertex_map[i_src] = -1;
    }
  }

  *r_verts_masked_num = verts_masked_num;
}

static void computed_masked_edges(const Mesh *mesh,
                                  Span<bool> vertex_mask,
                                  MutableSpan<int> r_edge_map,
                                  uint *r_edges_masked_num)
{
  BLI_assert(mesh->totedge == r_edge_map.size());
  const Span<MEdge> edges = mesh->edges();

  uint edges_masked_num = 0;
  for (int i : IndexRange(mesh->totedge)) {
    const MEdge &edge = edges[i];

    /* only add if both verts will be in new mesh */
    if (vertex_mask[edge.v1] && vertex_mask[edge.v2]) {
      r_edge_map[i] = edges_masked_num;
      edges_masked_num++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_edges_masked_num = edges_masked_num;
}

static void computed_masked_edges_smooth(const Mesh *mesh,
                                         Span<bool> vertex_mask,
                                         MutableSpan<int> r_edge_map,
                                         uint *r_edges_masked_num,
                                         uint *r_verts_add_num)
{
  BLI_assert(mesh->totedge == r_edge_map.size());
  const Span<MEdge> edges = mesh->edges();

  uint edges_masked_num = 0;
  uint verts_add_num = 0;
  for (int i : IndexRange(mesh->totedge)) {
    const MEdge &edge = edges[i];

    /* only add if both verts will be in new mesh */
    bool v1 = vertex_mask[edge.v1];
    bool v2 = vertex_mask[edge.v2];
    if (v1 && v2) {
      r_edge_map[i] = edges_masked_num;
      edges_masked_num++;
    }
    else if (v1 != v2) {
      r_edge_map[i] = -2;
      verts_add_num++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  edges_masked_num += verts_add_num;
  *r_edges_masked_num = edges_masked_num;
  *r_verts_add_num = verts_add_num;
}

static void computed_masked_polys(const Mesh *mesh,
                                  Span<bool> vertex_mask,
                                  Vector<int> &r_masked_poly_indices,
                                  Vector<int> &r_loop_starts,
                                  uint *r_polys_masked_num,
                                  uint *r_loops_masked_num)
{
  BLI_assert(mesh->totvert == vertex_mask.size());
  const Span<MPoly> polys = mesh->polys();
  const Span<MLoop> loops = mesh->loops();

  r_masked_poly_indices.reserve(mesh->totpoly);
  r_loop_starts.reserve(mesh->totpoly);

  uint loops_masked_num = 0;
  for (int i : IndexRange(mesh->totpoly)) {
    const MPoly &poly_src = polys[i];

    bool all_verts_in_mask = true;
    Span<MLoop> loops_src = loops.slice(poly_src.loopstart, poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (!vertex_mask[loop.v]) {
        all_verts_in_mask = false;
        break;
      }
    }

    if (all_verts_in_mask) {
      r_masked_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(loops_masked_num);
      loops_masked_num += poly_src.totloop;
    }
  }

  *r_polys_masked_num = r_masked_poly_indices.size();
  *r_loops_masked_num = loops_masked_num;
}

static void compute_interpolated_polys(const Mesh *mesh,
                                       Span<bool> vertex_mask,
                                       uint verts_add_num,
                                       uint loops_masked_num,
                                       Vector<int> &r_masked_poly_indices,
                                       Vector<int> &r_loop_starts,
                                       uint *r_edges_add_num,
                                       uint *r_polys_add_num,
                                       uint *r_loops_add_num)
{
  BLI_assert(mesh->totvert == vertex_mask.size());

  /* Can't really know ahead of time how much space to use exactly. Estimate limit instead. */
  /* NOTE: this reserve can only lift the capacity if there are ngons, which get split. */
  r_masked_poly_indices.reserve(r_masked_poly_indices.size() + verts_add_num);
  r_loop_starts.reserve(r_loop_starts.size() + verts_add_num);
  const Span<MPoly> polys = mesh->polys();
  const Span<MLoop> loops = mesh->loops();

  uint edges_add_num = 0;
  uint polys_add_num = 0;
  uint loops_add_num = 0;
  for (int i : IndexRange(mesh->totpoly)) {
    const MPoly &poly_src = polys[i];

    int in_count = 0;
    int start = -1;
    int dst_totloop = -1;
    const Span<MLoop> loops_src = loops.slice(poly_src.loopstart, poly_src.totloop);
    for (const int j : loops_src.index_range()) {
      const MLoop &loop = loops_src[j];
      if (vertex_mask[loop.v]) {
        in_count++;
      }
      else if (start == -1) {
        start = j;
      }
    }
    if (0 < in_count && in_count < poly_src.totloop) {
      /* Ring search starting at a vertex which is not included in the mask. */
      const MLoop *last_loop = &loops_src[start];
      bool v_loop_in_mask_last = vertex_mask[last_loop->v];
      for (const int j : loops_src.index_range()) {
        const MLoop &loop = loops_src[(start + 1 + j) % poly_src.totloop];
        const bool v_loop_in_mask = vertex_mask[loop.v];
        if (v_loop_in_mask && !v_loop_in_mask_last) {
          dst_totloop = 3;
        }
        else if (!v_loop_in_mask && v_loop_in_mask_last) {
          BLI_assert(dst_totloop > 2);
          r_masked_poly_indices.append(i);
          r_loop_starts.append(loops_masked_num + loops_add_num);
          loops_add_num += dst_totloop;
          polys_add_num++;
          edges_add_num++;
          dst_totloop = -1;
        }
        else if (v_loop_in_mask && v_loop_in_mask_last) {
          BLI_assert(dst_totloop > 2);
          dst_totloop++;
        }
        last_loop = &loop;
        v_loop_in_mask_last = v_loop_in_mask;
      }
    }
  }

  *r_edges_add_num = edges_add_num;
  *r_polys_add_num = polys_add_num;
  *r_loops_add_num = loops_add_num;
}

static void copy_masked_verts_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  for (const int i_src : vertex_map.index_range()) {
    const int i_dst = vertex_map[i_src];
    if (i_dst == -1) {
      continue;
    }

    CustomData_copy_data(&src_mesh.vdata, &dst_mesh.vdata, i_src, i_dst, 1);
  }
}

static float get_interp_factor_from_vgroup(
    const MDeformVert *dvert, int defgrp_index, float threshold, uint v1, uint v2)
{
  /* NOTE: this calculation is done twice for every vertex,
   * instead of storing it the first time and then reusing it. */
  float value1 = BKE_defvert_find_weight(&dvert[v1], defgrp_index);
  float value2 = BKE_defvert_find_weight(&dvert[v2], defgrp_index);
  return (threshold - value1) / (value2 - value1);
}

static void add_interp_verts_copy_edges_to_new_mesh(const Mesh &src_mesh,
                                                    Mesh &dst_mesh,
                                                    Span<bool> vertex_mask,
                                                    Span<int> vertex_map,
                                                    const MDeformVert *dvert,
                                                    int defgrp_index,
                                                    float threshold,
                                                    uint edges_masked_num,
                                                    uint verts_add_num,
                                                    MutableSpan<int> r_edge_map)
{
  BLI_assert(src_mesh.totvert == vertex_mask.size());
  BLI_assert(src_mesh.totedge == r_edge_map.size());
  const Span<MEdge> src_edges = src_mesh.edges();
  MutableSpan<MEdge> dst_edges = dst_mesh.edges_for_write();

  uint vert_index = dst_mesh.totvert - verts_add_num;
  uint edge_index = edges_masked_num - verts_add_num;
  for (int i_src : IndexRange(src_mesh.totedge)) {
    if (r_edge_map[i_src] != -1) {
      int i_dst = r_edge_map[i_src];
      if (i_dst == -2) {
        i_dst = edge_index;
      }
      const MEdge &e_src = src_edges[i_src];
      MEdge &e_dst = dst_edges[i_dst];

      CustomData_copy_data(&src_mesh.edata, &dst_mesh.edata, i_src, i_dst, 1);
      e_dst = e_src;
      e_dst.v1 = vertex_map[e_src.v1];
      e_dst.v2 = vertex_map[e_src.v2];
    }
    if (r_edge_map[i_src] == -2) {
      const int i_dst = edge_index++;
      r_edge_map[i_src] = i_dst;
      const MEdge &e_src = src_edges[i_src];
      /* Cut destination edge and make v1 the new vertex. */
      MEdge &e_dst = dst_edges[i_dst];
      if (!vertex_mask[e_src.v1]) {
        e_dst.v1 = vert_index;
      }
      else {
        BLI_assert(!vertex_mask[e_src.v2]);
        e_dst.v2 = e_dst.v1;
        e_dst.v1 = vert_index;
      }
      /* Create the new vertex. */
      float fac = get_interp_factor_from_vgroup(
          dvert, defgrp_index, threshold, e_src.v1, e_src.v2);

      float weights[2] = {1.0f - fac, fac};
      CustomData_interp(
          &src_mesh.vdata, &dst_mesh.vdata, (int *)&e_src.v1, weights, nullptr, 2, vert_index);
      vert_index++;
    }
  }
  BLI_assert(vert_index == dst_mesh.totvert);
  BLI_assert(edge_index == edges_masked_num);
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map)
{
  const Span<MEdge> src_edges = src_mesh.edges();
  MutableSpan<MEdge> dst_edges = dst_mesh.edges_for_write();

  BLI_assert(src_mesh.totvert == vertex_map.size());
  BLI_assert(src_mesh.totedge == edge_map.size());
  for (const int i_src : IndexRange(src_mesh.totedge)) {
    const int i_dst = edge_map[i_src];
    if (ELEM(i_dst, -1, -2)) {
      continue;
    }

    const MEdge &e_src = src_edges[i_src];
    MEdge &e_dst = dst_edges[i_dst];

    CustomData_copy_data(&src_mesh.edata, &dst_mesh.edata, i_src, i_dst, 1);
    e_dst = e_src;
    e_dst.v1 = vertex_map[e_src.v1];
    e_dst.v2 = vertex_map[e_src.v2];
  }
}

static void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts,
                                          int polys_masked_num)
{
  const Span<MPoly> src_polys = src_mesh.polys();
  const Span<MLoop> src_loops = src_mesh.loops();
  MutableSpan<MPoly> dst_polys = dst_mesh.polys_for_write();
  MutableSpan<MLoop> dst_loops = dst_mesh.loops_for_write();

  for (const int i_dst : IndexRange(polys_masked_num)) {
    const int i_src = masked_poly_indices[i_dst];

    const MPoly &mp_src = src_polys[i_src];
    MPoly &mp_dst = dst_polys[i_dst];
    const int i_ml_src = mp_src.loopstart;
    const int i_ml_dst = new_loop_starts[i_dst];

    CustomData_copy_data(&src_mesh.pdata, &dst_mesh.pdata, i_src, i_dst, 1);
    CustomData_copy_data(&src_mesh.ldata, &dst_mesh.ldata, i_ml_src, i_ml_dst, mp_src.totloop);

    const MLoop *ml_src = src_loops.data() + i_ml_src;
    MLoop *ml_dst = dst_loops.data() + i_ml_dst;

    mp_dst = mp_src;
    mp_dst.loopstart = i_ml_dst;
    for (int i : IndexRange(mp_src.totloop)) {
      ml_dst[i].v = vertex_map[ml_src[i].v];
      ml_dst[i].e = edge_map[ml_src[i].e];
    }
  }
}

static void add_interpolated_polys_to_new_mesh(const Mesh &src_mesh,
                                               Mesh &dst_mesh,
                                               Span<bool> vertex_mask,
                                               Span<int> vertex_map,
                                               Span<int> edge_map,
                                               const MDeformVert *dvert,
                                               int defgrp_index,
                                               float threshold,
                                               Span<int> masked_poly_indices,
                                               Span<int> new_loop_starts,
                                               int polys_masked_num,
                                               int edges_add_num)
{
  const Span<MPoly> src_polys = src_mesh.polys();
  const Span<MLoop> src_loops = src_mesh.loops();
  MutableSpan<MEdge> dst_edges = dst_mesh.edges_for_write();
  MutableSpan<MPoly> dst_polys = dst_mesh.polys_for_write();
  MutableSpan<MLoop> dst_loops = dst_mesh.loops_for_write();

  int edge_index = dst_mesh.totedge - edges_add_num;
  int sub_poly_index = 0;
  int last_i_src = -1;
  for (const int i_dst :
       IndexRange(polys_masked_num, masked_poly_indices.size() - polys_masked_num)) {
    const int i_src = masked_poly_indices[i_dst];
    if (i_src == last_i_src) {
      sub_poly_index++;
    }
    else {
      sub_poly_index = 0;
      last_i_src = i_src;
    }

    const MPoly &mp_src = src_polys[i_src];
    MPoly &mp_dst = dst_polys[i_dst];
    const int i_ml_src = mp_src.loopstart;
    int i_ml_dst = new_loop_starts[i_dst];
    const int mp_totloop = (i_dst + 1 < new_loop_starts.size() ? new_loop_starts[i_dst + 1] :
                                                                 dst_mesh.totloop) -
                           i_ml_dst;

    CustomData_copy_data(&src_mesh.pdata, &dst_mesh.pdata, i_src, i_dst, 1);

    mp_dst = mp_src;
    mp_dst.loopstart = i_ml_dst;
    mp_dst.totloop = mp_totloop;

    /* Ring search starting at a vertex which is not included in the mask. */
    int start = -sub_poly_index - 1;
    bool skip = false;
    Span<MLoop> loops_src(&src_loops[i_ml_src], mp_src.totloop);
    for (const int j : loops_src.index_range()) {
      if (!vertex_mask[loops_src[j].v]) {
        if (start == -1) {
          start = j;
          break;
        }
        if (!skip) {
          skip = true;
        }
      }
      else if (skip) {
        skip = false;
        start++;
      }
    }

    BLI_assert(start >= 0);
    BLI_assert(edge_index < dst_mesh.totedge);

    const MLoop *last_loop = &loops_src[start];
    bool v_loop_in_mask_last = vertex_mask[last_loop->v];
    int last_index = start;
    for (const int j : loops_src.index_range()) {
      const int index = (start + 1 + j) % mp_src.totloop;
      const MLoop &loop = loops_src[index];
      const bool v_loop_in_mask = vertex_mask[loop.v];
      if (v_loop_in_mask && !v_loop_in_mask_last) {
        /* Start new cut. */
        float fac = get_interp_factor_from_vgroup(
            dvert, defgrp_index, threshold, last_loop->v, loop.v);
        float weights[2] = {1.0f - fac, fac};
        int indices[2] = {i_ml_src + last_index, i_ml_src + index};
        CustomData_interp(
            &src_mesh.ldata, &dst_mesh.ldata, indices, weights, nullptr, 2, i_ml_dst);
        MLoop &cut_dst_loop = dst_loops[i_ml_dst];
        cut_dst_loop.e = edge_map[last_loop->e];
        cut_dst_loop.v = dst_edges[cut_dst_loop.e].v1;
        i_ml_dst++;

        CustomData_copy_data(&src_mesh.ldata, &dst_mesh.ldata, i_ml_src + index, i_ml_dst, 1);
        MLoop &next_dst_loop = dst_loops[i_ml_dst];
        next_dst_loop.v = vertex_map[loop.v];
        next_dst_loop.e = edge_map[loop.e];
        i_ml_dst++;
      }
      else if (!v_loop_in_mask && v_loop_in_mask_last) {
        BLI_assert(i_ml_dst != mp_dst.loopstart);
        /* End active cut. */
        float fac = get_interp_factor_from_vgroup(
            dvert, defgrp_index, threshold, last_loop->v, loop.v);
        float weights[2] = {1.0f - fac, fac};
        int indices[2] = {i_ml_src + last_index, i_ml_src + index};
        CustomData_interp(
            &src_mesh.ldata, &dst_mesh.ldata, indices, weights, nullptr, 2, i_ml_dst);
        MLoop &cut_dst_loop = dst_loops[i_ml_dst];
        cut_dst_loop.e = edge_index;
        cut_dst_loop.v = dst_edges[edge_map[last_loop->e]].v1;
        i_ml_dst++;

        /* Create closing edge. */
        MEdge &cut_edge = dst_edges[edge_index];
        cut_edge.v1 = dst_loops[mp_dst.loopstart].v;
        cut_edge.v2 = cut_dst_loop.v;
        BLI_assert(cut_edge.v1 != cut_edge.v2);
        cut_edge.flag = 0;
        edge_index++;

        /* Only handle one of the cuts per iteration. */
        break;
      }
      else if (v_loop_in_mask && v_loop_in_mask_last) {
        BLI_assert(i_ml_dst != mp_dst.loopstart);
        /* Extend active poly. */
        CustomData_copy_data(&src_mesh.ldata, &dst_mesh.ldata, i_ml_src + index, i_ml_dst, 1);
        MLoop &dst_loop = dst_loops[i_ml_dst];
        dst_loop.v = vertex_map[loop.v];
        dst_loop.e = edge_map[loop.e];
        i_ml_dst++;
      }
      last_loop = &loop;
      last_index = index;
      v_loop_in_mask_last = v_loop_in_mask;
    }
    BLI_assert(mp_dst.loopstart + mp_dst.totloop == i_ml_dst);
  }
  BLI_assert(edge_index == dst_mesh.totedge);
}

/* Components of the algorithm:
 * 1. Figure out which vertices should be present in the output mesh.
 * 2. Find edges and polygons only using those vertices.
 * 3. Create a new mesh that only uses the found vertices, edges and polygons.
 */
static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);
  const bool invert_mask = mmd->flag & MOD_MASK_INV;
  const bool use_interpolation = mmd->mode == MOD_MASK_MODE_VGROUP &&
                                 (mmd->flag & MOD_MASK_SMOOTH);

  /* Return empty or input mesh when there are no vertex groups. */
  const Span<MDeformVert> dverts = mesh->deform_verts();
  if (dverts.is_empty()) {
    return invert_mask ? mesh : BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0, 0);
  }

  /* Quick test to see if we can return early. */
  if (!ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP) || (mesh->totvert == 0) ||
      BLI_listbase_is_empty(&mesh->vertex_group_names)) {
    return mesh;
  }

  int defgrp_index = -1;

  Array<bool> vertex_mask;
  if (mmd->mode == MOD_MASK_MODE_ARM) {
    Object *armature_ob = mmd->ob_arm;

    /* Return input mesh if there is no armature with bones. */
    if (ELEM(nullptr, armature_ob, armature_ob->pose)) {
      return mesh;
    }

    vertex_mask = Array<bool>(mesh->totvert);
    compute_vertex_mask__armature_mode(
        dverts.data(), mesh, armature_ob, mmd->threshold, vertex_mask);
  }
  else {
    BLI_assert(mmd->mode == MOD_MASK_MODE_VGROUP);
    defgrp_index = BKE_id_defgroup_name_index(&mesh->id, mmd->vgroup);

    /* Return input mesh if the vertex group does not exist. */
    if (defgrp_index == -1) {
      return mesh;
    }

    vertex_mask = Array<bool>(mesh->totvert);
    compute_vertex_mask__vertex_group_mode(
        dverts.data(), defgrp_index, mmd->threshold, vertex_mask);
  }

  if (invert_mask) {
    blender::array_utils::invert_booleans(vertex_mask);
  }

  Array<int> vertex_map(mesh->totvert);
  uint verts_masked_num;
  compute_masked_verts(vertex_mask, vertex_map, &verts_masked_num);

  Array<int> edge_map(mesh->totedge);
  uint edges_masked_num;
  uint verts_add_num;
  if (use_interpolation) {
    computed_masked_edges_smooth(mesh, vertex_mask, edge_map, &edges_masked_num, &verts_add_num);
  }
  else {
    computed_masked_edges(mesh, vertex_mask, edge_map, &edges_masked_num);
    verts_add_num = 0;
  }

  Vector<int> masked_poly_indices;
  Vector<int> new_loop_starts;
  uint polys_masked_num;
  uint loops_masked_num;
  computed_masked_polys(mesh,
                        vertex_mask,
                        masked_poly_indices,
                        new_loop_starts,
                        &polys_masked_num,
                        &loops_masked_num);

  uint edges_add_num = 0;
  uint polys_add_num = 0;
  uint loops_add_num = 0;
  if (use_interpolation) {
    compute_interpolated_polys(mesh,
                               vertex_mask,
                               verts_add_num,
                               loops_masked_num,
                               masked_poly_indices,
                               new_loop_starts,
                               &edges_add_num,
                               &polys_add_num,
                               &loops_add_num);
  }

  Mesh *result = BKE_mesh_new_nomain_from_template(mesh,
                                                   verts_masked_num + verts_add_num,
                                                   edges_masked_num + edges_add_num,
                                                   0,
                                                   loops_masked_num + loops_add_num,
                                                   polys_masked_num + polys_add_num);

  copy_masked_verts_to_new_mesh(*mesh, *result, vertex_map);
  if (use_interpolation) {
    add_interp_verts_copy_edges_to_new_mesh(*mesh,
                                            *result,
                                            vertex_mask,
                                            vertex_map,
                                            dverts.data(),
                                            defgrp_index,
                                            mmd->threshold,
                                            edges_masked_num,
                                            verts_add_num,
                                            edge_map);
  }
  else {
    copy_masked_edges_to_new_mesh(*mesh, *result, vertex_map, edge_map);
  }
  copy_masked_polys_to_new_mesh(*mesh,
                                *result,
                                vertex_map,
                                edge_map,
                                masked_poly_indices,
                                new_loop_starts,
                                polys_masked_num);
  if (use_interpolation) {
    add_interpolated_polys_to_new_mesh(*mesh,
                                       *result,
                                       vertex_mask,
                                       vertex_map,
                                       edge_map,
                                       dverts.data(),
                                       defgrp_index,
                                       mmd->threshold,
                                       masked_poly_indices,
                                       new_loop_starts,
                                       polys_masked_num,
                                       edges_add_num);
  }

  return result;
}

static bool isDisabled(const struct Scene * /*scene*/, ModifierData *md, bool /*useRenderParams*/)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the armature is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return mmd->ob_arm && mmd->ob_arm->type != OB_ARMATURE;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *sub, *row;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int mode = RNA_enum_get(ptr, "mode");

  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);

  uiLayoutSetPropSep(layout, true);

  if (mode == MOD_MASK_MODE_ARM) {
    row = uiLayoutRow(layout, true);
    uiItemR(row, ptr, "armature", 0, nullptr, ICON_NONE);
    sub = uiLayoutRow(row, true);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, "invert_vertex_group", 0, "", ICON_ARROW_LEFTRIGHT);
  }
  else if (mode == MOD_MASK_MODE_VGROUP) {
    modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);
    uiItemR(layout, ptr, "use_smooth", 0, nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "threshold", 0, nullptr, ICON_NONE);

  modifier_panel_end(layout, ptr);
}

static void panelRegister(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Mask, panel_draw);
}

ModifierTypeInfo modifierType_Mask = {
    /*name*/ N_("Mask"),
    /*structName*/ "MaskModifierData",
    /*structSize*/ sizeof(MaskModifierData),
    /*srna*/ &RNA_MaskModifier,
    /*type*/ eModifierTypeType_Nonconstructive,
    /*flags*/
    (ModifierTypeFlag)(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
                       eModifierTypeFlag_SupportsEditmode),
    /*icon*/ ICON_MOD_MASK,

    /*copyData*/ BKE_modifier_copydata_generic,

    /*deformVerts*/ nullptr,
    /*deformMatrices*/ nullptr,
    /*deformVertsEM*/ nullptr,
    /*deformMatricesEM*/ nullptr,
    /*modifyMesh*/ modifyMesh,
    /*modifyGeometrySet*/ nullptr,

    /*initData*/ initData,
    /*requiredDataMask*/ requiredDataMask,
    /*freeData*/ nullptr,
    /*isDisabled*/ isDisabled,
    /*updateDepsgraph*/ updateDepsgraph,
    /*dependsOnTime*/ nullptr,
    /*dependsOnNormals*/ nullptr,
    /*foreachIDLink*/ foreachIDLink,
    /*foreachTexLink*/ nullptr,
    /*freeRuntimeData*/ nullptr,
    /*panelRegister*/ panelRegister,
    /*blendWrite*/ nullptr,
    /*blendRead*/ nullptr,
};
