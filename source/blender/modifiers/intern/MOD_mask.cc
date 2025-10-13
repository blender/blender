/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_utildefines.h"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_action.hh" /* BKE_pose_channel_find_name */
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_lib_query.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DEG_depsgraph_build.hh"

#include "MOD_ui_common.hh"

#include "BLI_array.hh"
#include "BLI_listbase_wrapper.hh"
#include "BLI_vector.hh"

using blender::Array;
using blender::float3;
using blender::IndexRange;
using blender::int2;
using blender::ListBaseWrapper;
using blender::MutableSpan;
using blender::Span;
using blender::Vector;

static void init_data(ModifierData *md)
{
  MaskModifierData *mmd = (MaskModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(mmd, modifier));

  MEMCPY_STRUCT_AFTER(mmd, DNA_struct_default_get(MaskModifierData), modifier);
}

static void required_data_mask(ModifierData * /*md*/, CustomData_MeshMasks *r_cddata_masks)
{
  r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);
  walk(user_data, ob, (ID **)&mmd->ob_arm, IDWALK_CB_NOP);
}

static void update_depsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
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
    bool bone_for_group_exists = pchan && pchan->bone && (pchan->flag & POSE_SELECTED);
    selected_bone_uses_group.append(bone_for_group_exists);
  }
  const int64_t total_size = selected_bone_uses_group.size();

  for (int i : r_vertex_mask.index_range()) {
    Span<MDeformWeight> weights(dvert[i].dw, dvert[i].totweight);
    r_vertex_mask[i] = false;

    /* check the groups that vertex is assigned to, and see if it was any use */
    for (const MDeformWeight &dw : weights) {
      if (dw.def_nr >= total_size) {
        continue;
      }
      if (selected_bone_uses_group[dw.def_nr]) {
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
  BLI_assert(mesh->edges_num == r_edge_map.size());
  const Span<int2> edges = mesh->edges();

  uint edges_masked_num = 0;
  for (int i : IndexRange(mesh->edges_num)) {
    const int2 &edge = edges[i];

    /* only add if both verts will be in new mesh */
    if (vertex_mask[edge[0]] && vertex_mask[edge[1]]) {
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
  BLI_assert(mesh->edges_num == r_edge_map.size());
  const Span<int2> edges = mesh->edges();

  uint edges_masked_num = 0;
  uint verts_add_num = 0;
  for (int i : IndexRange(mesh->edges_num)) {
    const int2 &edge = edges[i];

    /* only add if both verts will be in new mesh */
    bool v1 = vertex_mask[edge[0]];
    bool v2 = vertex_mask[edge[1]];
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

static void computed_masked_faces(const Mesh *mesh,
                                  Span<bool> vertex_mask,
                                  Vector<int> &r_masked_face_indices,
                                  Vector<int> &r_loop_starts,
                                  uint *r_faces_masked_num,
                                  uint *r_loops_masked_num)
{
  BLI_assert(mesh->verts_num == vertex_mask.size());
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  r_masked_face_indices.reserve(mesh->faces_num);
  r_loop_starts.reserve(mesh->faces_num);

  uint loops_masked_num = 0;
  for (int i : IndexRange(mesh->faces_num)) {
    const blender::IndexRange face = faces[i];

    bool all_verts_in_mask = true;
    for (const int vert_i : corner_verts.slice(face)) {
      if (!vertex_mask[vert_i]) {
        all_verts_in_mask = false;
        break;
      }
    }

    if (all_verts_in_mask) {
      r_masked_face_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(loops_masked_num);
      loops_masked_num += face.size();
    }
  }

  *r_faces_masked_num = r_masked_face_indices.size();
  *r_loops_masked_num = loops_masked_num;
}

static void compute_interpolated_faces(const Mesh *mesh,
                                       Span<bool> vertex_mask,
                                       uint verts_add_num,
                                       uint loops_masked_num,
                                       Vector<int> &r_masked_face_indices,
                                       Vector<int> &r_loop_starts,
                                       uint *r_edges_add_num,
                                       uint *r_faces_add_num,
                                       uint *r_loops_add_num)
{
  BLI_assert(mesh->verts_num == vertex_mask.size());

  /* Can't really know ahead of time how much space to use exactly. Estimate limit instead. */
  /* NOTE: this reserve can only lift the capacity if there are ngons, which get split. */
  r_masked_face_indices.reserve(r_masked_face_indices.size() + verts_add_num);
  r_loop_starts.reserve(r_loop_starts.size() + verts_add_num);
  const blender::OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  uint edges_add_num = 0;
  uint faces_add_num = 0;
  uint loops_add_num = 0;
  for (int i : IndexRange(mesh->faces_num)) {
    const blender::IndexRange face_src = faces[i];

    int in_count = 0;
    int start = -1;
    int dst_totloop = -1;
    const Span<int> face_verts_src = corner_verts.slice(face_src);
    for (const int j : face_verts_src.index_range()) {
      const int vert_i = face_verts_src[j];
      if (vertex_mask[vert_i]) {
        in_count++;
      }
      else if (start == -1) {
        start = j;
      }
    }
    if (0 < in_count && in_count < face_src.size()) {
      /* Ring search starting at a vertex which is not included in the mask. */
      int last_corner_vert = face_verts_src[start];
      bool v_loop_in_mask_last = vertex_mask[last_corner_vert];
      for (const int j : face_verts_src.index_range()) {
        const int corner_vert = face_verts_src[(start + 1 + j) % face_src.size()];
        const bool v_loop_in_mask = vertex_mask[corner_vert];
        if (v_loop_in_mask && !v_loop_in_mask_last) {
          dst_totloop = 3;
        }
        else if (!v_loop_in_mask && v_loop_in_mask_last) {
          BLI_assert(dst_totloop > 2);
          r_masked_face_indices.append(i);
          r_loop_starts.append(loops_masked_num + loops_add_num);
          loops_add_num += dst_totloop;
          faces_add_num++;
          edges_add_num++;
          dst_totloop = -1;
        }
        else if (v_loop_in_mask && v_loop_in_mask_last) {
          BLI_assert(dst_totloop > 2);
          dst_totloop++;
        }
        last_corner_vert = corner_vert;
        v_loop_in_mask_last = v_loop_in_mask;
      }
    }
  }

  *r_edges_add_num = edges_add_num;
  *r_faces_add_num = faces_add_num;
  *r_loops_add_num = loops_add_num;
}

static void copy_masked_verts_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map)
{
  BLI_assert(src_mesh.verts_num == vertex_map.size());
  for (const int i_src : vertex_map.index_range()) {
    const int i_dst = vertex_map[i_src];
    if (i_dst == -1) {
      continue;
    }

    CustomData_copy_data(&src_mesh.vert_data, &dst_mesh.vert_data, i_src, i_dst, 1);
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
  BLI_assert(src_mesh.verts_num == vertex_mask.size());
  BLI_assert(src_mesh.edges_num == r_edge_map.size());
  const Span<int2> src_edges = src_mesh.edges();
  MutableSpan<int2> dst_edges = dst_mesh.edges_for_write();

  uint vert_index = dst_mesh.verts_num - verts_add_num;
  uint edge_index = edges_masked_num - verts_add_num;
  for (int i_src : IndexRange(src_mesh.edges_num)) {
    if (r_edge_map[i_src] != -1) {
      int i_dst = r_edge_map[i_src];
      if (i_dst == -2) {
        i_dst = edge_index;
      }
      const int2 &e_src = src_edges[i_src];
      int2 &e_dst = dst_edges[i_dst];

      CustomData_copy_data(&src_mesh.edge_data, &dst_mesh.edge_data, i_src, i_dst, 1);
      e_dst = e_src;
      e_dst[0] = vertex_map[e_src[0]];
      e_dst[1] = vertex_map[e_src[1]];
    }
    if (r_edge_map[i_src] == -2) {
      const int i_dst = edge_index++;
      r_edge_map[i_src] = i_dst;
      const int2 &e_src = src_edges[i_src];
      /* Cut destination edge and make v1 the new vertex. */
      int2 &e_dst = dst_edges[i_dst];
      if (!vertex_mask[e_src[0]]) {
        e_dst[0] = vert_index;
      }
      else {
        BLI_assert(!vertex_mask[e_src[1]]);
        e_dst[1] = e_dst[0];
        e_dst[0] = vert_index;
      }
      /* Create the new vertex. */
      float fac = get_interp_factor_from_vgroup(
          dvert, defgrp_index, threshold, e_src[0], e_src[1]);

      float weights[2] = {1.0f - fac, fac};
      CustomData_interp(
          &src_mesh.vert_data, &dst_mesh.vert_data, (int *)&e_src[0], weights, 2, vert_index);
      vert_index++;
    }
  }
  BLI_assert(vert_index == dst_mesh.verts_num);
  BLI_assert(edge_index == edges_masked_num);
}

static void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map)
{
  const Span<int2> src_edges = src_mesh.edges();
  MutableSpan<int2> dst_edges = dst_mesh.edges_for_write();

  BLI_assert(src_mesh.verts_num == vertex_map.size());
  BLI_assert(src_mesh.edges_num == edge_map.size());
  for (const int i_src : IndexRange(src_mesh.edges_num)) {
    const int i_dst = edge_map[i_src];
    if (ELEM(i_dst, -1, -2)) {
      continue;
    }

    CustomData_copy_data(&src_mesh.edge_data, &dst_mesh.edge_data, i_src, i_dst, 1);
    dst_edges[i_dst][0] = vertex_map[src_edges[i_src][0]];
    dst_edges[i_dst][1] = vertex_map[src_edges[i_src][1]];
  }
}

static void copy_masked_faces_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          Span<int> vertex_map,
                                          Span<int> edge_map,
                                          Span<int> masked_face_indices,
                                          Span<int> new_loop_starts,
                                          int faces_masked_num)
{
  const blender::OffsetIndices src_faces = src_mesh.faces();
  MutableSpan<int> dst_face_offsets = dst_mesh.face_offsets_for_write();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  MutableSpan<int> dst_corner_verts = dst_mesh.corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh.corner_edges_for_write();

  for (const int i_dst : IndexRange(faces_masked_num)) {
    const int i_src = masked_face_indices[i_dst];
    const blender::IndexRange src_face = src_faces[i_src];

    dst_face_offsets[i_dst] = new_loop_starts[i_dst];

    CustomData_copy_data(&src_mesh.face_data, &dst_mesh.face_data, i_src, i_dst, 1);
    CustomData_copy_data(&src_mesh.corner_data,
                         &dst_mesh.corner_data,
                         src_face.start(),
                         dst_face_offsets[i_dst],
                         src_face.size());

    for (int i : IndexRange(src_face.size())) {
      dst_corner_verts[new_loop_starts[i_dst] + i] = vertex_map[src_corner_verts[src_face[i]]];
      dst_corner_edges[new_loop_starts[i_dst] + i] = edge_map[src_corner_edges[src_face[i]]];
    }
  }
}

static void add_interpolated_faces_to_new_mesh(const Mesh &src_mesh,
                                               Mesh &dst_mesh,
                                               Span<bool> vertex_mask,
                                               Span<int> vertex_map,
                                               Span<int> edge_map,
                                               const MDeformVert *dvert,
                                               int defgrp_index,
                                               float threshold,
                                               Span<int> masked_face_indices,
                                               Span<int> new_loop_starts,
                                               int faces_masked_num,
                                               int edges_add_num)
{
  const blender::OffsetIndices src_faces = src_mesh.faces();
  MutableSpan<int> dst_face_offsets = dst_mesh.face_offsets_for_write();
  MutableSpan<int2> dst_edges = dst_mesh.edges_for_write();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();
  MutableSpan<int> dst_corner_verts = dst_mesh.corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst_mesh.corner_edges_for_write();

  int edge_index = dst_mesh.edges_num - edges_add_num;
  int sub_face_index = 0;
  int last_i_src = -1;
  for (const int i_dst :
       IndexRange(faces_masked_num, masked_face_indices.size() - faces_masked_num))
  {
    const int i_src = masked_face_indices[i_dst];
    if (i_src == last_i_src) {
      sub_face_index++;
    }
    else {
      sub_face_index = 0;
      last_i_src = i_src;
    }

    const blender::IndexRange src_face = src_faces[i_src];
    const int i_ml_src = src_face.start();
    int i_ml_dst = new_loop_starts[i_dst];
    CustomData_copy_data(&src_mesh.face_data, &dst_mesh.face_data, i_src, i_dst, 1);

    dst_face_offsets[i_dst] = i_ml_dst;

    /* Ring search starting at a vertex which is not included in the mask. */
    int start = -sub_face_index - 1;
    bool skip = false;
    const Span<int> face_verts_src = src_corner_verts.slice(src_face);
    const Span<int> face_edges_src = src_corner_edges.slice(src_face);
    for (const int j : face_verts_src.index_range()) {
      if (!vertex_mask[face_verts_src[j]]) {
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
    BLI_assert(edge_index < dst_mesh.edges_num);

    int last_index = start;
    bool v_loop_in_mask_last = vertex_mask[face_verts_src[last_index]];
    for (const int j : face_verts_src.index_range()) {
      const int index = (start + 1 + j) % src_face.size();
      const bool v_loop_in_mask = vertex_mask[face_verts_src[index]];
      if (v_loop_in_mask && !v_loop_in_mask_last) {
        /* Start new cut. */
        float fac = get_interp_factor_from_vgroup(
            dvert, defgrp_index, threshold, face_verts_src[last_index], face_verts_src[index]);
        float weights[2] = {1.0f - fac, fac};
        int indices[2] = {i_ml_src + last_index, i_ml_src + index};
        CustomData_interp(
            &src_mesh.corner_data, &dst_mesh.corner_data, indices, weights, 2, i_ml_dst);
        dst_corner_edges[i_ml_dst] = edge_map[face_edges_src[last_index]];
        dst_corner_verts[i_ml_dst] = dst_edges[dst_corner_edges[i_ml_dst]][0];
        i_ml_dst++;

        CustomData_copy_data(
            &src_mesh.corner_data, &dst_mesh.corner_data, i_ml_src + index, i_ml_dst, 1);
        dst_corner_verts[i_ml_dst] = vertex_map[face_verts_src[index]];
        dst_corner_edges[i_ml_dst] = edge_map[face_edges_src[index]];
        i_ml_dst++;
      }
      else if (!v_loop_in_mask && v_loop_in_mask_last) {
        BLI_assert(i_ml_dst != dst_face_offsets[i_dst]);
        /* End active cut. */
        float fac = get_interp_factor_from_vgroup(
            dvert, defgrp_index, threshold, face_verts_src[last_index], face_verts_src[index]);
        float weights[2] = {1.0f - fac, fac};
        int indices[2] = {i_ml_src + last_index, i_ml_src + index};
        CustomData_interp(
            &src_mesh.corner_data, &dst_mesh.corner_data, indices, weights, 2, i_ml_dst);
        dst_corner_edges[i_ml_dst] = edge_index;
        dst_corner_verts[i_ml_dst] = dst_edges[edge_map[face_edges_src[last_index]]][0];

        /* Create closing edge. */
        int2 &cut_edge = dst_edges[edge_index];
        cut_edge[0] = dst_corner_verts[dst_face_offsets[i_dst]];
        cut_edge[1] = dst_corner_verts[i_ml_dst];
        BLI_assert(cut_edge[0] != cut_edge[1]);
        edge_index++;
        i_ml_dst++;

        /* Only handle one of the cuts per iteration. */
        break;
      }
      else if (v_loop_in_mask && v_loop_in_mask_last) {
        BLI_assert(i_ml_dst != dst_face_offsets[i_dst]);
        /* Extend active face. */
        CustomData_copy_data(
            &src_mesh.corner_data, &dst_mesh.corner_data, i_ml_src + index, i_ml_dst, 1);
        dst_corner_verts[i_ml_dst] = vertex_map[face_verts_src[index]];
        dst_corner_edges[i_ml_dst] = edge_map[face_edges_src[index]];
        i_ml_dst++;
      }
      last_index = index;
      v_loop_in_mask_last = v_loop_in_mask;
    }
  }
  BLI_assert(edge_index == dst_mesh.edges_num);
}

/* Components of the algorithm:
 * 1. Figure out which vertices should be present in the output mesh.
 * 2. Find edges and faces only using those vertices.
 * 3. Create a new mesh that only uses the found vertices, edges and faces.
 */
static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext * /*ctx*/, Mesh *mesh)
{
  MaskModifierData *mmd = reinterpret_cast<MaskModifierData *>(md);
  const bool invert_mask = mmd->flag & MOD_MASK_INV;
  const bool use_interpolation = mmd->mode == MOD_MASK_MODE_VGROUP &&
                                 (mmd->flag & MOD_MASK_SMOOTH);

  /* Return empty or input mesh when there are no vertex groups. */
  const Span<MDeformVert> dverts = mesh->deform_verts();
  if (dverts.is_empty()) {
    return invert_mask ? mesh : BKE_mesh_new_nomain_from_template(mesh, 0, 0, 0, 0);
  }

  /* Quick test to see if we can return early. */
  if (!ELEM(mmd->mode, MOD_MASK_MODE_ARM, MOD_MASK_MODE_VGROUP) || (mesh->verts_num == 0) ||
      BLI_listbase_is_empty(&mesh->vertex_group_names))
  {
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

    vertex_mask = Array<bool>(mesh->verts_num);
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

    vertex_mask = Array<bool>(mesh->verts_num);
    compute_vertex_mask__vertex_group_mode(
        dverts.data(), defgrp_index, mmd->threshold, vertex_mask);
  }

  if (invert_mask) {
    blender::array_utils::invert_booleans(vertex_mask);
  }

  Array<int> vertex_map(mesh->verts_num);
  uint verts_masked_num;
  compute_masked_verts(vertex_mask, vertex_map, &verts_masked_num);

  Array<int> edge_map(mesh->edges_num);
  uint edges_masked_num;
  uint verts_add_num;
  if (use_interpolation) {
    computed_masked_edges_smooth(mesh, vertex_mask, edge_map, &edges_masked_num, &verts_add_num);
  }
  else {
    computed_masked_edges(mesh, vertex_mask, edge_map, &edges_masked_num);
    verts_add_num = 0;
  }

  Vector<int> masked_face_indices;
  Vector<int> new_loop_starts;
  uint faces_masked_num;
  uint loops_masked_num;
  computed_masked_faces(mesh,
                        vertex_mask,
                        masked_face_indices,
                        new_loop_starts,
                        &faces_masked_num,
                        &loops_masked_num);

  uint edges_add_num = 0;
  uint faces_add_num = 0;
  uint loops_add_num = 0;
  if (use_interpolation) {
    compute_interpolated_faces(mesh,
                               vertex_mask,
                               verts_add_num,
                               loops_masked_num,
                               masked_face_indices,
                               new_loop_starts,
                               &edges_add_num,
                               &faces_add_num,
                               &loops_add_num);
  }

  Mesh *result = BKE_mesh_new_nomain_from_template(mesh,
                                                   verts_masked_num + verts_add_num,
                                                   edges_masked_num + edges_add_num,
                                                   faces_masked_num + faces_add_num,
                                                   loops_masked_num + loops_add_num);

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
  copy_masked_faces_to_new_mesh(*mesh,
                                *result,
                                vertex_map,
                                edge_map,
                                masked_face_indices,
                                new_loop_starts,
                                faces_masked_num);
  if (use_interpolation) {
    add_interpolated_faces_to_new_mesh(*mesh,
                                       *result,
                                       vertex_mask,
                                       vertex_map,
                                       edge_map,
                                       dverts.data(),
                                       defgrp_index,
                                       mmd->threshold,
                                       masked_face_indices,
                                       new_loop_starts,
                                       faces_masked_num,
                                       edges_add_num);
  }

  return result;
}

static bool is_disabled(const Scene * /*scene*/, ModifierData *md, bool /*use_render_params*/)
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

  layout->prop(ptr, "mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  layout->use_property_split_set(true);

  if (mode == MOD_MASK_MODE_ARM) {
    row = &layout->row(true);
    row->prop(ptr, "armature", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub = &row->row(true);
    sub->use_property_decorate_set(false);
    sub->prop(ptr, "invert_vertex_group", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
  }
  else if (mode == MOD_MASK_MODE_VGROUP) {
    modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", std::nullopt);
    layout->prop(ptr, "use_smooth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->prop(ptr, "threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  modifier_error_message_draw(layout, ptr);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_Mask, panel_draw);
}

ModifierTypeInfo modifierType_Mask = {
    /*idname*/ "Mask",
    /*name*/ N_("Mask"),
    /*struct_name*/ "MaskModifierData",
    /*struct_size*/ sizeof(MaskModifierData),
    /*srna*/ &RNA_MaskModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/
    (eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsMapping |
     eModifierTypeFlag_SupportsEditmode),
    /*icon*/ ICON_MOD_MASK,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ is_disabled,
    /*update_depsgraph*/ update_depsgraph,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
};
