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

#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h"

#include "BLI_array.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_vector.hh"

using blender::Array;
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

static void requiredDataMask(Object *UNUSED(ob),
                             ModifierData *UNUSED(md),
                             CustomData_MeshMasks *r_cddata_masks)
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
    DEG_add_modifier_to_transform_relation(ctx->node, "Mask Modifier");
  }
}

/* A vertex will be in the mask if a selected bone influences it more than a certain threshold. */
static void compute_vertex_mask__armature_mode(MDeformVert *dvert,
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
static void compute_vertex_mask__vertex_group_mode(MDeformVert *dvert,
                                                   int defgrp_index,
                                                   float threshold,
                                                   MutableSpan<bool> r_vertex_mask)
{
  for (int i : r_vertex_mask.index_range()) {
    const bool found = BKE_defvert_find_weight(&dvert[i], defgrp_index) > threshold;
    r_vertex_mask[i] = found;
  }
}

static void invert_boolean_array(MutableSpan<bool> array)
{
  for (bool &value : array) {
    value = !value;
  }
}

static void compute_masked_vertices(Span<bool> vertex_mask,
                                    MutableSpan<int> r_vertex_map,
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
                                  Span<bool> vertex_mask,
                                  MutableSpan<int> r_edge_map,
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

static void computed_masked_edges_smooth(const Mesh *mesh,
                                         Span<bool> vertex_mask,
                                         MutableSpan<int> r_edge_map,
                                         uint *r_num_masked_edges,
                                         uint *r_num_add_vertices)
{
  BLI_assert(mesh->totedge == r_edge_map.size());

  uint num_masked_edges = 0;
  uint num_add_vertices = 0;
  for (int i : IndexRange(mesh->totedge)) {
    const MEdge &edge = mesh->medge[i];

    /* only add if both verts will be in new mesh */
    bool v1 = vertex_mask[edge.v1];
    bool v2 = vertex_mask[edge.v2];
    if (v1 && v2) {
      r_edge_map[i] = num_masked_edges;
      num_masked_edges++;
    }
    else if (v1 != v2) {
      r_edge_map[i] = -2;
      num_add_vertices++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  num_masked_edges += num_add_vertices;
  *r_num_masked_edges = num_masked_edges;
  *r_num_add_vertices = num_add_vertices;
}

static void computed_masked_polygons(const Mesh *mesh,
                                     Span<bool> vertex_mask,
                                     Vector<int> &r_masked_poly_indices,
                                     Vector<int> &r_loop_starts,
                                     uint *r_num_masked_polys,
                                     uint *r_num_masked_loops)
{
  BLI_assert(mesh->totvert == vertex_mask.size());

  r_masked_poly_indices.reserve(mesh->totpoly);
  r_loop_starts.reserve(mesh->totpoly);

  uint num_masked_loops = 0;
  for (int i : IndexRange(mesh->totpoly)) {
    const MPoly &poly_src = mesh->mpoly[i];

    bool all_verts_in_mask = true;
    Span<MLoop> loops_src(&mesh->mloop[poly_src.loopstart], poly_src.totloop);
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

static void compute_interpolated_polygons(const Mesh *mesh,
                                          Span<bool> vertex_mask,
                                          uint num_add_vertices,
                                          uint num_masked_loops,
                                          Vector<int> &r_masked_poly_indices,
                                          Vector<int> &r_loop_starts,
                                          uint *r_num_add_edges,
                                          uint *r_num_add_polys,
                                          uint *r_num_add_loops)
{
  BLI_assert(mesh->totvert == vertex_mask.size());

  /* Can't really know ahead of time how much space to use exactly. Estimate limit instead. */
  /* NOTE: this reserve can only lift the capacity if there are ngons, which get split. */
  r_masked_poly_indices.reserve(r_masked_poly_indices.size() + num_add_vertices);
  r_loop_starts.reserve(r_loop_starts.size() + num_add_vertices);

  uint num_add_edges = 0;
  uint num_add_polys = 0;
  uint num_add_loops = 0;
  for (int i : IndexRange(mesh->totpoly)) {
    const MPoly &poly_src = mesh->mpoly[i];

    int in_count = 0;
    int start = -1;
    int dst_totloop = -1;
    Span<MLoop> loops_src(&mesh->mloop[poly_src.loopstart], poly_src.totloop);
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
          r_loop_starts.append(num_masked_loops + num_add_loops);
          num_add_loops += dst_totloop;
          num_add_polys++;
          num_add_edges++;
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

  *r_num_add_edges = num_add_edges;
  *r_num_add_polys = num_add_polys;
  *r_num_add_loops = num_add_loops;
}

static void copy_masked_vertices_to_new_mesh(const Mesh &src_mesh,
                                             Mesh &dst_mesh,
                                             Span<int> vertex_map)
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

static float get_interp_factor_from_vgroup(
    MDeformVert *dvert, int defgrp_index, float threshold, uint v1, uint v2)
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
                                                    MDeformVert *dvert,
                                                    int defgrp_index,
                                                    float threshold,
                                                    uint num_masked_edges,
                                                    uint num_add_verts,
                                                    MutableSpan<int> r_edge_map)
{
  BLI_assert(src_mesh.totvert == vertex_mask.size());
  BLI_assert(src_mesh.totedge == r_edge_map.size());

  uint vert_index = dst_mesh.totvert - num_add_verts;
  uint edge_index = num_masked_edges - num_add_verts;
  for (int i_src : IndexRange(src_mesh.totedge)) {
    if (r_edge_map[i_src] != -1) {
      int i_dst = r_edge_map[i_src];
      if (i_dst == -2) {
        i_dst = edge_index;
      }
      const MEdge &e_src = src_mesh.medge[i_src];
      MEdge &e_dst = dst_mesh.medge[i_dst];

      CustomData_copy_data(&src_mesh.edata, &dst_mesh.edata, i_src, i_dst, 1);
      e_dst = e_src;
      e_dst.v1 = vertex_map[e_src.v1];
      e_dst.v2 = vertex_map[e_src.v2];
    }
    if (r_edge_map[i_src] == -2) {
      const int i_dst = edge_index++;
      r_edge_map[i_src] = i_dst;
      const MEdge &e_src = src_mesh.medge[i_src];
      /* Cut destination edge and make v1 the new vertex. */
      MEdge &e_dst = dst_mesh.medge[i_dst];
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
      MVert &v = dst_mesh.mvert[vert_index];
      MVert &v1 = src_mesh.mvert[e_src.v1];
      MVert &v2 = src_mesh.mvert[e_src.v2];

      interp_v3_v3v3(v.co, v1.co, v2.co, fac);
      vert_index++;
    }
  }
  BLI_assert(vert_index == dst_mesh.totvert);
  BLI_assert(edge_index == num_masked_edges);
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map)
{
  BLI_assert(src_mesh.totvert == vertex_map.size());
  BLI_assert(src_mesh.totedge == edge_map.size());
  for (const int i_src : IndexRange(src_mesh.totedge)) {
    const int i_dst = edge_map[i_src];
    if (ELEM(i_dst, -1, -2)) {
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
                                          Span<int> vertex_map,
                                          Span<int> edge_map,
                                          Span<int> masked_poly_indices,
                                          Span<int> new_loop_starts,
                                          int num_masked_polys)
{
  for (const int i_dst : IndexRange(num_masked_polys)) {
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

static void add_interpolated_polys_to_new_mesh(const Mesh &src_mesh,
                                               Mesh &dst_mesh,
                                               Span<bool> vertex_mask,
                                               Span<int> vertex_map,
                                               Span<int> edge_map,
                                               MDeformVert *dvert,
                                               int defgrp_index,
                                               float threshold,
                                               Span<int> masked_poly_indices,
                                               Span<int> new_loop_starts,
                                               int num_masked_polys,
                                               int num_add_edges)
{
  int edge_index = dst_mesh.totedge - num_add_edges;
  int sub_poly_index = 0;
  int last_i_src = -1;
  for (const int i_dst :
       IndexRange(num_masked_polys, masked_poly_indices.size() - num_masked_polys)) {
    const int i_src = masked_poly_indices[i_dst];
    if (i_src == last_i_src) {
      sub_poly_index++;
    }
    else {
      sub_poly_index = 0;
      last_i_src = i_src;
    }

    const MPoly &mp_src = src_mesh.mpoly[i_src];
    MPoly &mp_dst = dst_mesh.mpoly[i_dst];
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
    Span<MLoop> loops_src(&src_mesh.mloop[i_ml_src], mp_src.totloop);
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
        MLoop &cut_dst_loop = dst_mesh.mloop[i_ml_dst];
        cut_dst_loop.e = edge_map[last_loop->e];
        cut_dst_loop.v = dst_mesh.medge[cut_dst_loop.e].v1;
        i_ml_dst++;

        CustomData_copy_data(&src_mesh.ldata, &dst_mesh.ldata, i_ml_src + index, i_ml_dst, 1);
        MLoop &next_dst_loop = dst_mesh.mloop[i_ml_dst];
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
        MLoop &cut_dst_loop = dst_mesh.mloop[i_ml_dst];
        cut_dst_loop.e = edge_index;
        cut_dst_loop.v = dst_mesh.medge[edge_map[last_loop->e]].v1;
        i_ml_dst++;

        /* Create closing edge. */
        MEdge &cut_edge = dst_mesh.medge[edge_index];
        cut_edge.v1 = dst_mesh.mloop[mp_dst.loopstart].v;
        cut_edge.v2 = cut_dst_loop.v;
        BLI_assert(cut_edge.v1 != cut_edge.v2);
        cut_edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
        edge_index++;

        /* Only handle one of the cuts per iteration. */
        break;
      }
      else if (v_loop_in_mask && v_loop_in_mask_last) {
        BLI_assert(i_ml_dst != mp_dst.loopstart);
        /* Extend active poly. */
        CustomData_copy_data(&src_mesh.ldata, &dst_mesh.ldata, i_ml_src + index, i_ml_dst, 1);
        MLoop &dst_loop = dst_mesh.mloop[i_ml_dst];
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
static Mesh *modifyMesh(ModifierData *md, const ModifierEvalContext *UNUSED(ctx), Mesh *mesh)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);
  const bool invert_mask = mmd->flag & MOD_MASK_INV;
  const bool use_interpolation = mmd->mode == MOD_MASK_MODE_VGROUP &&
                                 (mmd->flag & MOD_MASK_SMOOTH);

  /* Return empty or input mesh when there are no vertex groups. */
  MDeformVert *dvert = (MDeformVert *)CustomData_get_layer(&mesh->vdata, CD_MDEFORMVERT);
  if (dvert == nullptr) {
    return invert_mask ? mesh : BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0, 0);
  }

  /* Quick test to see if we can return early. */
  if (!(ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP)) || (mesh->totvert == 0) ||
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
    compute_vertex_mask__armature_mode(dvert, mesh, armature_ob, mmd->threshold, vertex_mask);
  }
  else {
    BLI_assert(mmd->mode == MOD_MASK_MODE_VGROUP);
    defgrp_index = BKE_id_defgroup_name_index(&mesh->id, mmd->vgroup);

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
  uint num_add_vertices;
  if (use_interpolation) {
    computed_masked_edges_smooth(
        mesh, vertex_mask, edge_map, &num_masked_edges, &num_add_vertices);
  }
  else {
    computed_masked_edges(mesh, vertex_mask, edge_map, &num_masked_edges);
    num_add_vertices = 0;
  }

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

  uint num_add_edges = 0;
  uint num_add_polys = 0;
  uint num_add_loops = 0;
  if (use_interpolation) {
    compute_interpolated_polygons(mesh,
                                  vertex_mask,
                                  num_add_vertices,
                                  num_masked_loops,
                                  masked_poly_indices,
                                  new_loop_starts,
                                  &num_add_edges,
                                  &num_add_polys,
                                  &num_add_loops);
  }

  Mesh *result = BKE_mesh_new_nomain_from_template(mesh,
                                                   num_masked_vertices + num_add_vertices,
                                                   num_masked_edges + num_add_edges,
                                                   0,
                                                   num_masked_loops + num_add_loops,
                                                   num_masked_polys + num_add_polys);

  copy_masked_vertices_to_new_mesh(*mesh, *result, vertex_map);
  if (use_interpolation) {
    add_interp_verts_copy_edges_to_new_mesh(*mesh,
                                            *result,
                                            vertex_mask,
                                            vertex_map,
                                            dvert,
                                            defgrp_index,
                                            mmd->threshold,
                                            num_masked_edges,
                                            num_add_vertices,
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
                                num_masked_polys);
  if (use_interpolation) {
    add_interpolated_polys_to_new_mesh(*mesh,
                                       *result,
                                       vertex_mask,
                                       vertex_map,
                                       edge_map,
                                       dvert,
                                       defgrp_index,
                                       mmd->threshold,
                                       masked_poly_indices,
                                       new_loop_starts,
                                       num_masked_polys,
                                       num_add_edges);
  }

  BKE_mesh_calc_edges_loose(result);
  BKE_mesh_normals_tag_dirty(result);

  return result;
}

static bool isDisabled(const struct Scene *UNUSED(scene),
                       ModifierData *md,
                       bool UNUSED(useRenderParams))
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);

  /* The object type check is only needed here in case we have a placeholder
   * object assigned (because the library containing the armature is missing).
   *
   * In other cases it should be impossible to have a type mismatch.
   */
  return mmd->ob_arm && mmd->ob_arm->type != OB_ARMATURE;
}

static void panel_draw(const bContext *UNUSED(C), Panel *panel)
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
    /* name */ "Mask",
    /* structName */ "MaskModifierData",
    /* structSize */ sizeof(MaskModifierData),
    /* srna */ &RNA_MaskModifier,
    /* type */ eModifierTypeType_Nonconstructive,
    /* flags */
    (ModifierTypeFlag)(eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
                       eModifierTypeFlag_SupportsEditmode),
    /* icon */ ICON_MOD_MASK,

    /* copyData */ BKE_modifier_copydata_generic,

    /* deformVerts */ nullptr,
    /* deformMatrices */ nullptr,
    /* deformVertsEM */ nullptr,
    /* deformMatricesEM */ nullptr,
    /* modifyMesh */ modifyMesh,
    /* modifyHair */ nullptr,
    /* modifyGeometrySet */ nullptr,

    /* initData */ initData,
    /* requiredDataMask */ requiredDataMask,
    /* freeData */ nullptr,
    /* isDisabled */ isDisabled,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ nullptr,
    /* dependsOnNormals */ nullptr,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ nullptr,
    /* freeRuntimeData */ nullptr,
    /* panelRegister */ panelRegister,
    /* blendWrite */ nullptr,
    /* blendRead */ nullptr,
};
