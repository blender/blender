/* SPDX-FileCopyrightText: 2021-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_attribute_shader_shared.hh"
#include "draw_subdiv_shader_shared.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "subdiv_common.bsl.hh"

namespace subdiv {

void add_newell_cross_v3_v3v3(float3 &n, float3 v_prev, float3 v_curr)
{
  n[0] += (v_prev[1] - v_curr[1]) * (v_prev[2] + v_curr[2]);
  n[1] += (v_prev[2] - v_curr[2]) * (v_prev[0] + v_curr[0]);
  n[2] += (v_prev[0] - v_curr[0]) * (v_prev[1] + v_curr[1]);
}

/* Adapted from BLI_math_vector_c.hh */
float angle_normalized_v3v3(float3 v1, float3 v2)
{
  /* this is the same as acos(dot_v3v3(v1, v2)), but more accurate */
  bool q = (dot(v1, v2) >= 0.0f);
  float3 v = (q) ? (v1 - v2) : (v1 + v2);
  float a = 2.0f * asin(length(v) / 2.0f);
  return (q) ? a : M_PI - a;
}

/* -------------------------------------------------------------------- */
/** \name Accumulate vertex normals from their adjacent faces.
 *
 * Accumulated normals needs to be finalized `subdiv_vbo_lnor_comp.glsl`.
 * to be stored as loops.
 * \{ */

struct NormalAccumulate {
  [[storage(NORMALS_ACCUMULATE_POS_BUF_SLOT, read)]] StoredFloat3 (&positions)[];
  [[storage(NORMALS_ACCUMULATE_FACE_ADJACENCY_OFFSETS_BUF_SLOT,
            read)]] uint (&face_adjacency_offsets)[];
  [[storage(NORMALS_ACCUMULATE_FACE_ADJACENCY_LISTS_BUF_SLOT,
            read)]] uint (&face_adjacency_lists)[];
  [[storage(NORMALS_ACCUMULATE_VERTEX_LOOP_MAP_BUF_SLOT, read)]] uint (&vert_loop_map)[];
  [[storage(NORMALS_ACCUMULATE_NORMALS_BUF_SLOT, write)]] StoredFloat3 (&vert_normals)[];

  void find_prev_and_next_vertex_on_face(
      uint face_index, uint vertex_index, uint &curr, uint &next, uint &prev)
  {
    uint start_loop_index = face_index * 4;

    for (uint i = 0; i < 4; i++) {
      uint subdiv_vert_index = vert_loop_map[start_loop_index + i];

      if (subdiv_vert_index == vertex_index) {
        curr = i;
        next = (i + 1) % 4;
        prev = (i + 4 - 1) % 4;
        break;
      }
    }
  }
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void normals_accumulate_main([[resource_table]] const SubdivResources &srt,
                             [[resource_table]] NormalAccumulate &accum,
                             [[global_invocation_id]] const uint3 global_id,
                             [[num_work_groups]] const uint3 num_work_groups)
{
  uint vertex_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (vertex_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  uint first_adjacent_face_offset = accum.face_adjacency_offsets[vertex_index];
  uint number_of_adjacent_faces = accum.face_adjacency_offsets[vertex_index + 1] -
                                  first_adjacent_face_offset;

  float3 accumulated_normal = float3(0.0f);

  /* For each adjacent face. */
  for (uint i = 0; i < number_of_adjacent_faces; i++) {
    uint adjacent_face = accum.face_adjacency_lists[first_adjacent_face_offset + i];
    uint start_loop_index = adjacent_face * 4;

    /* Compute the face normal using Newell's method. */
    float3 verts[4];
    for (uint j = 0; j < 4; j++) [[unroll]] {
      StoredFloat3 data = accum.positions[start_loop_index + j];
      verts[j] = load_data(data);
    }

    float3 face_normal = float3(0.0f);
    add_newell_cross_v3_v3v3(face_normal, verts[0], verts[1]);
    add_newell_cross_v3_v3v3(face_normal, verts[1], verts[2]);
    add_newell_cross_v3_v3v3(face_normal, verts[2], verts[3]);
    add_newell_cross_v3_v3v3(face_normal, verts[3], verts[0]);

    /* Accumulate angle weighted normal. */
    uint curr_vert = 0;
    uint next_vert = 0;
    uint prev_vert = 0;
    accum.find_prev_and_next_vertex_on_face(
        adjacent_face, vertex_index, curr_vert, next_vert, prev_vert);

    float3 curr_co = verts[curr_vert];
    float3 prev_co = verts[next_vert];
    float3 next_co = verts[prev_vert];

    float3 edvec_prev = normalize(prev_co - curr_co);
    float3 edvec_next = normalize(curr_co - next_co);

    float fac = acos(-dot(edvec_prev, edvec_next));

    accumulated_normal += face_normal * fac;
  }

  accum.vert_normals[vertex_index] = as_data(normalize(accumulated_normal));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge data for object mode wireframe
 * \{ */

/* From extract_mesh_vbo_edge_fac.cc, keep in sync! */
float loop_edge_factor_get(float3 fa_no, float3 fb_no)
{
  float cosine = dot(fa_no, fb_no);
  /* Re-scale to the slider range. */
  float fac = (200 * (cosine - 1.0f)) + 1.0f;
  /* The maximum value (255) is unreachable through the UI. */
  return clamp(fac, 0.0f, 1.0f) * (254.0f / 255.0f);
}

struct EdgeFac {
  [[storage(EDGE_FAC_POS_BUF_SLOT, read)]] StoredFloat3 (&positions)[];
  [[storage(EDGE_FAC_EDGE_DRAW_FLAG_BUF_SLOT, read)]] uint (&input_edge_draw_flag)[];
  [[storage(EDGE_FAC_POLY_OTHER_MAP_BUF_SLOT, read)]] int (&input_poly_other_map)[];
  [[storage(EDGE_FAC_EDGE_FAC_BUF_SLOT, write)]] float (&output_edge_fac)[];

  float compute_line_factor(uint corner_index, float3 face_normal)
  {
    if (input_edge_draw_flag[corner_index] == 0) {
      return 1.0f;
    }

    int quad_other = input_poly_other_map[corner_index];
    if (quad_other == -1) {
      /* Boundary edge or non-manifold. */
      return 0.0f;
    }

    const uint start_corner_index_other = uint(quad_other * 4);
    const StoredFloat3 f0 = positions[start_corner_index_other + 0];
    const StoredFloat3 f1 = positions[start_corner_index_other + 1];
    const StoredFloat3 f2 = positions[start_corner_index_other + 2];
    const float3 v0 = load_data(f0);
    const float3 v1 = load_data(f1);
    const float3 v2 = load_data(f2);
    const float3 face_normal_other = normalize(cross(v1 - v0, v2 - v0));

    return loop_edge_factor_get(face_normal, face_normal_other);
  }
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void edge_fac_main([[resource_table]] const SubdivResources &srt,
                   [[resource_table]] EdgeFac &edges,
                   [[global_invocation_id]] const uint3 global_id,
                   [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  const uint start_loop_index = quad_index * 4;

  /* First compute the face normal, we need it to compute the bihedral edge angle. */
  StoredFloat3 f0 = edges.positions[start_loop_index + 0];
  StoredFloat3 f1 = edges.positions[start_loop_index + 1];
  StoredFloat3 f2 = edges.positions[start_loop_index + 2];
  float3 v0 = load_data(f0);
  float3 v1 = load_data(f1);
  float3 v2 = load_data(f2);
  float3 face_normal = normalize(cross(v1 - v0, v2 - v0));

  for (uint i = 0; i < 4; i++) {
    edges.output_edge_fac[start_loop_index + i] = edges.compute_line_factor(start_loop_index + i,
                                                                            face_normal);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit UV stretch
 * \{ */

struct EditUVStretchArea {
  [[storage(STRETCH_AREA_COARSE_STRETCH_AREA_BUF_SLOT, read)]] float (&coarse_stretch_area)[];
  [[storage(STRETCH_AREA_SUBDIV_STRETCH_AREA_BUF_SLOT, write)]] float (&subdiv_stretch_area)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void edituv_stretch_area_main([[resource_table]] const SubdivResources &srt,
                              [[resource_table]] const PolygonOffsetBase &poly_ofs,
                              [[resource_table]] EditUVStretchArea &edit,
                              [[global_invocation_id]] const uint3 global_id,
                              [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  uint start_loop_index = quad_index * 4;

  uint coarse_quad_index = poly_ofs.coarse_face_index_from_subdiv_quad_index(
      quad_index, uint(srt.shader_data.coarse_face_count));

  for (uint i = 0; i < 4; i++) [[unroll]] {
    edit.subdiv_stretch_area[start_loop_index + i] = edit.coarse_stretch_area[coarse_quad_index];
  }
}

struct EditUVStretchAngle {
  [[storage(STRETCH_ANGLE_POS_BUF_SLOT, read)]] StoredFloat3 (&positions)[];
  [[storage(STRETCH_ANGLE_UVS_BUF_SLOT, read)]] packed_float2 (&uvs)[];
  [[storage(STRETCH_ANGLE_UV_STRETCHES_BUF_SLOT, write)]] UVStretchAngle (&uv_stretches)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void edituv_stretch_angle_main([[resource_table]] const SubdivResources &srt,
                               [[resource_table]] EditUVStretchAngle &edit,
                               [[global_invocation_id]] const uint3 global_id,
                               [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  uint start_loop_index = quad_index * 4;

  for (uint i = 0; i < 4; i++) {
    uint cur_loop_index = start_loop_index + i;
    uint next_loop_index = start_loop_index + (i + 1) % 4;
    uint prev_loop_index = start_loop_index + (i + 3) % 4;

    /* Compute 2d edge vectors from UVs. */
    float2 cur_uv = edit.uvs[uint(srt.shader_data.src_offset) + cur_loop_index];
    float2 next_uv = edit.uvs[uint(srt.shader_data.src_offset) + next_loop_index];
    float2 prev_uv = edit.uvs[uint(srt.shader_data.src_offset) + prev_loop_index];

    float2 norm_uv_edge0 = normalize(prev_uv - cur_uv);
    float2 norm_uv_edge1 = normalize(cur_uv - next_uv);

    /* Compute 3d edge vectors from positions. */
    float3 cur_pos = load_data(edit.positions[cur_loop_index]);
    float3 next_pos = load_data(edit.positions[next_loop_index]);
    float3 prev_pos = load_data(edit.positions[prev_loop_index]);

    float3 norm_pos_edge0 = normalize(prev_pos - cur_pos);
    float3 norm_pos_edge1 = normalize(cur_pos - next_pos);

    /* Compute stretches, this logic is adapted from #edituv_get_edituv_stretch_angle.
     * Keep in sync! */
    UVStretchAngle stretch;
    stretch.uv_angle0 = atan(norm_uv_edge0.y, norm_uv_edge0.x) * M_1_PI;
    stretch.uv_angle1 = atan(norm_uv_edge1.y, norm_uv_edge1.x) * M_1_PI;
    stretch.angle = angle_normalized_v3v3(norm_pos_edge0, norm_pos_edge1) * M_1_PI;

    edit.uv_stretches[cur_loop_index] = stretch;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Overlay Flag
 * \{ */

struct LoopNormalsResource {
  [[storage(LOOP_NORMALS_POS_SLOT, read)]] StoredFloat3 (&positions)[];
  [[storage(LOOP_NORMALS_EXTRA_COARSE_FACE_DATA_BUF_SLOT, read)]] uint (&extra_coarse_face_data)[];
  [[storage(LOOP_NORMALS_VERT_NORMALS_BUF_SLOT, read)]] StoredFloat3 (&vert_normals)[];
  [[storage(LOOP_NORMALS_VERTEX_LOOP_MAP_BUF_SLOT, read)]] uint (&vert_loop_map)[];
  [[storage(LOOP_NORMALS_OUTPUT_LNOR_BUF_SLOT, write)]] StoredFloat3 (&output_lnor)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void loop_normals_main([[resource_table]] const SubdivResources &srt,
                       [[resource_table]] const PolygonOffsetBase &poly_ofs,
                       [[resource_table]] LoopNormalsResource &lnors,
                       [[global_invocation_id]] const uint3 global_id,
                       [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  const uint start_loop_index = quad_index * 4;

  const uint coarse_quad_index = poly_ofs.coarse_face_index_from_subdiv_quad_index(
      quad_index, uint(srt.shader_data.coarse_face_count));

  if ((lnors.extra_coarse_face_data[coarse_quad_index] &
       srt.shader_data.coarse_face_smooth_mask) != 0)
  {
    /* Face is smooth, use vertex normals. */
    for (uint i = 0; i < 4u; i++) [[unroll]] {
      const uint subdiv_vert_index = lnors.vert_loop_map[start_loop_index + i];
      lnors.output_lnor[start_loop_index + i] = lnors.vert_normals[subdiv_vert_index];
    }
  }
  else {
    StoredFloat3 f0 = lnors.positions[start_loop_index + 0];
    StoredFloat3 f1 = lnors.positions[start_loop_index + 1];
    StoredFloat3 f2 = lnors.positions[start_loop_index + 2];
    StoredFloat3 f3 = lnors.positions[start_loop_index + 3];
    float3 v0 = load_data(f0);
    float3 v1 = load_data(f1);
    float3 v2 = load_data(f2);
    float3 v3 = load_data(f3);

    float3 face_normal = float3(0.0f);
    add_newell_cross_v3_v3v3(face_normal, v0, v1);
    add_newell_cross_v3_v3v3(face_normal, v1, v2);
    add_newell_cross_v3_v3v3(face_normal, v2, v3);
    add_newell_cross_v3_v3v3(face_normal, v3, v0);

    StoredFloat3 normal = as_data(normalize(face_normal));

    for (uint i = 0; i < 4; i++) [[unroll]] {
      lnors.output_lnor[start_loop_index + i] = normal;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paint Overlay Flag
 * \{ */

struct PaintResource {
  [[resource_table]] const SubdivResources &srt;

  [[storage(PAINT_OVERLAY_EXTRA_COARSE_FACE_DATA_BUF_SLOT,
            read)]] uint (&extra_coarse_face_data)[];
  [[storage(PAINT_OVERLAY_EXTRA_INPUT_VERT_ORIG_INDEX_SLOT, read)]] int (&input_vert_origindex)[];
  [[storage(PAINT_OVERLAY_OUTPUT_FLAG_SLOT, write)]] int (&flags)[];

  bool is_face_selected(uint coarse_quad_index)
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_select_mask) !=
           0u;
  }

  bool is_face_hidden(uint coarse_quad_index)
  {
    return (extra_coarse_face_data[coarse_quad_index] & srt.shader_data.coarse_face_hidden_mask) !=
           0u;
  }

  /* Flag for paint mode overlay and normals drawing in edit-mode. */
  int get_loop_flag(uint coarse_quad_index, int vert_origindex)
  {
    if (is_face_hidden(coarse_quad_index) ||
        (srt.shader_data.is_edit_mode && vert_origindex == -1))
    {
      return -1;
    }
    if (is_face_selected(coarse_quad_index)) {
      return 1;
    }
    return 0;
  }
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void paint_overlay_flag_main([[resource_table]] const SubdivResources &srt,
                             [[resource_table]] const PolygonOffsetBase &poly_ofs,
                             [[resource_table]] PaintResource &paint,
                             [[global_invocation_id]] const uint3 global_id,
                             [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  /* The start index of the loop is quad_index * 4. */
  const uint start_loop_index = quad_index * 4;

  const uint coarse_quad_index = poly_ofs.coarse_face_index_from_subdiv_quad_index(
      quad_index, uint(srt.shader_data.coarse_face_count));
  for (uint i = 0; i < 4; i++) [[unroll]] {
    int origindex = paint.input_vert_origindex[start_loop_index + i];
    int flag = paint.get_loop_flag(coarse_quad_index, origindex);

    paint.flags[start_loop_index + i] = flag;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Data
 * \{ */

struct SculptResources {
  [[storage(SCULPT_DATA_SCULPT_FACE_SET_COLOR_BUF_SLOT, read)]] uint (&face_set_color)[];
  [[storage(SCULPT_DATA_SCULPT_MASK_BUF_SLOT, read)]] float (&mask)[];
  [[storage(SCULPT_DATA_SCULPT_DATA_BUF_SLOT, write)]] SculptData (&data_buf)[];
};

[[compute, local_size(SUBDIV_GROUP_SIZE)]]
void sculpt_data_main([[resource_table]] const SubdivResources &srt,
                      [[resource_table]] SculptResources &sculpt,
                      [[global_invocation_id]] const uint3 global_id,
                      [[num_work_groups]] const uint3 num_work_groups)
{
  /* We execute for each quad. */
  const uint quad_index = srt.get_global_invocation_index(global_id, num_work_groups);
  if (quad_index >= srt.shader_data.total_dispatch_size) {
    return;
  }

  const uint start_loop_index = quad_index * 4;

  for (uint loop_index = start_loop_index; loop_index < start_loop_index + 4; loop_index++) {
    SculptData data = {
        .face_set_color = sculpt.face_set_color[loop_index],
        .mask = 0.0f,
    };

    if (srt.shader_data.has_sculpt_mask) {
      data.mask = sculpt.mask[loop_index];
    }

    sculpt.data_buf[loop_index] = data;
  }
}

/** \} */

PipelineCompute edge_fac(edge_fac_main);
PipelineCompute edituv_stretch_area(edituv_stretch_area_main);
PipelineCompute normals_accumulate(normals_accumulate_main);
PipelineCompute loop_normals(loop_normals_main);
PipelineCompute paint_overlay_flag(paint_overlay_flag_main);
PipelineCompute sculpt_data(sculpt_data_main);

}  // namespace subdiv
