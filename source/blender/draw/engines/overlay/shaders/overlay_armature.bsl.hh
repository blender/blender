/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view.bsl.hh"
#include "gpu_shader_attribute_load.bsl.hh"
#include "gpu_shader_index_load.bsl.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"
#include "overlay_common.bsl.hh"

namespace overlay::armature {

struct Armature {
  [[push_constant]] float alpha;
};

/* -------------------------------------------------------------------- */
/** \name Armature Degrees of Freedom
 * \{ */

namespace degrees_of_freedom {

struct Resources {
  [[storage(0, read)]] const ExtraInstanceData (&data_buf)[];
};

struct VertIn {
  [[attribute(0)]] float2 pos;
};

float3 sphere_project(float ax, float az)
{
  float sine = 1.0f - ax * ax - az * az;
  float q3 = sqrt(max(0.0f, sine));

  return float3(-az * q3, 0.5f - sine, ax * q3) * 2.0f;
}

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  float4x4 inst_obmat = srt.data_buf[inst_index].object_to_world;
  float4x4 model_mat = inst_obmat;
  model_mat[0][3] = model_mat[1][3] = model_mat[2][3] = 0.0f;
  model_mat[3][3] = 1.0f;

  float2 amin = float2(inst_obmat[0][3], inst_obmat[1][3]);
  float2 amax = float2(inst_obmat[2][3], inst_obmat[3][3]);

  float3 final_pos = sphere_project(v_in.pos.x * abs((v_in.pos.x > 0.0f) ? amax.x : amin.x),
                                    v_in.pos.y * abs((v_in.pos.y > 0.0f) ? amax.y : amin.y));

  float3 world_pos = (model_mat * float4(final_pos, 1.0f)).xyz;

  ViewMatrices view = views.get(0);
  out_pos = view.point_world_to_homogenous(world_pos);
  v_out.final_color = srt.data_buf[inst_index].color_;
  v_out.final_color.a *= armature.alpha;

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

/* TODO(fclem): Deduplicate. */
[[fragment]]
void frag([[in]] const VertOut &v_out, [[out]] FragOut &frag_out)
{
  frag_out.color = v_out.final_color;
  frag_out.line_output = float4(0.0f);
}

}  // namespace degrees_of_freedom

PipelineGraphic dof(degrees_of_freedom::vert,
                    degrees_of_freedom::frag,
                    ClippingConstant{.use_clipping = false});
PipelineGraphic dof_clipped(degrees_of_freedom::vert,
                            degrees_of_freedom::frag,
                            ClippingConstant{.use_clipping = true});
/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Envelope
 * \{ */

namespace envelope {

float3x3 compute_mat(ViewMatrices view, float4 sphere, float3 bone_vec, float &z_ofs)
{
  bool is_persp = (view.winmat[3][3] == 0.0f);
  float3 cam_ray = (is_persp) ? sphere.xyz - view.viewinv[3].xyz : -view.viewinv[2].xyz;

  /* Sphere center distance from the camera (persp) in world space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  float3 z_axis = cam_ray / cam_dist;
  float3 x_axis = normalize(cross(bone_vec, z_axis));
  float3 y_axis = cross(z_axis, x_axis);
  z_ofs = 0.0f;

  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    constexpr float half_pi = 3.1415926f * 0.5f;
    float rad = sphere.w;
    /* Let be :
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0f - cos_b * cos_b, 0.0f, 1.0f));

    x_axis *= sin_b;
    y_axis *= sin_b;
    z_ofs = -rad * cos_b;
  }

  return float3x3(x_axis, y_axis, z_axis);
}

namespace outline {

struct Resources {
  [[storage(0, read)]] const BoneEnvelopeData (&data_buf)[];
};

struct VertIn {
  [[attribute(0)]] float2 pos0;
  [[attribute(1)]] float2 pos1;
  [[attribute(2)]] float2 pos2;
};

struct Bone {
  float3 vec;
  float sinb;
};

bool bone_blend_starts(float3 p, Bone b)
{
  /* we just want to know when the head sphere starts interpolating. */
  return dot(p, b.vec) > -b.sinb;
}

float3 get_outline_point(float2 pos,
                         float4 sph_near,
                         float4 sph_far,
                         float3x3 mat_near,
                         float3x3 mat_far,
                         float z_ofs_near,
                         float z_ofs_far,
                         Bone b)
{
  /* Compute outline position on the nearest sphere and check
   * if it penetrates the capsule body. If it does, put this
   * vertex on the farthest sphere. */
  float3 wpos = mat_near * float3(pos * sph_near.w, z_ofs_near);
  if (bone_blend_starts(wpos, b)) {
    wpos = sph_far.xyz + mat_far * float3(pos * sph_far.w, z_ofs_far);
  }
  else {
    wpos += sph_near.xyz;
  }
  return wpos;
}

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  float dst_head = distance(srt.data_buf[inst_index].head_sphere.xyz, view.viewinv[3].xyz);
  float dst_tail = distance(srt.data_buf[inst_index].tail_sphere.xyz, view.viewinv[3].xyz);
  // float dst_head = -dot(data_buf[inst_index].head_sphere.xyz, view.viewmat[2].xyz);
  // float dst_tail = -dot(data_buf[inst_index].tail_sphere.xyz, view.viewmat[2].xyz);

  float4 sph_near, sph_far;
  if ((dst_head > dst_tail) && (view.winmat[3][3] == 0.0f)) {
    sph_near = srt.data_buf[inst_index].tail_sphere;
    sph_far = srt.data_buf[inst_index].head_sphere;
  }
  else {
    sph_near = srt.data_buf[inst_index].head_sphere;
    sph_far = srt.data_buf[inst_index].tail_sphere;
  }

  float3 bone_vec = (sph_far.xyz - sph_near.xyz) + 1e-8f;

  Bone b;
  float bone_lenrcp = 1.0f / max(1e-8f, sqrt(dot(bone_vec, bone_vec)));
  b.sinb = (sph_far.w - sph_near.w) * bone_lenrcp * sph_near.w;
  b.vec = bone_vec * bone_lenrcp;

  float z_ofs_near, z_ofs_far;
  float3x3 mat_near = compute_mat(view, sph_near, bone_vec, z_ofs_near);
  float3x3 mat_far = compute_mat(view, sph_far, bone_vec, z_ofs_far);

  float3 wpos0 = get_outline_point(
      v_in.pos0, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);
  float3 wpos1 = get_outline_point(
      v_in.pos1, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);
  float3 wpos2 = get_outline_point(
      v_in.pos2, sph_near, sph_far, mat_near, mat_far, z_ofs_near, z_ofs_far, b);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(wpos1,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }

  float4 p0 = view.point_world_to_homogenous(wpos0);
  float4 p1 = view.point_world_to_homogenous(wpos1);
  float4 p2 = view.point_world_to_homogenous(wpos2);

  out_pos = p1;

  /* compute position from 3 vertex because the change in direction
   * can happen very quickly and lead to very thin edges. */
  float2 ss0 = uni.proj(p0);
  float2 ss1 = uni.proj(p1);
  float2 ss2 = uni.proj(p2);
  float2 ofs_dir = compute_dir(ss0, ss1, ss2);

  /* Offset away from the center to avoid overlap with solid shape. */
  out_pos.xy += ofs_dir * uni.uniform_buf.size_viewport_inv * out_pos.w;

  v_out.edge_start = v_out.edge_pos = uni.proj(out_pos);

  v_out.final_color = float4(srt.data_buf[inst_index].bone_color_and_wire_width.rgb,
                             armature.alpha);
}

}  // namespace outline

namespace solid {

struct Resources {
  [[storage(0, read)]] const BoneEnvelopeData (&data_buf)[];
  [[push_constant]] bool is_distance;
};

struct VertIn {
  [[attribute(0)]] float3 pos;
};

struct VertOut {
  [[flat]] float3 final_state_color;
  [[flat]] float3 final_bone_color;
  [[smooth]] float3 view_normal;
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  float3 bone_vec = srt.data_buf[inst_index].tail_sphere.xyz -
                    srt.data_buf[inst_index].head_sphere.xyz;
  float bone_len = max(1e-8f, sqrt(dot(bone_vec, bone_vec)));
  float bone_lenrcp = 1.0f / bone_len;
#if 0 /* SMOOTH_ENVELOPE  */
  float sinb = (srt.data_buf[inst_index].tail_sphere.w - srt.data_buf[inst_index].head_sphere.w) *
               bone_lenrcp;
#else
  constexpr float sinb = 0.0f;
#endif

  float3 y_axis = bone_vec * bone_lenrcp;
  float3 z_axis = normalize(cross(srt.data_buf[inst_index].x_axis.xyz, -y_axis));
  /* cannot trust srt.data_buf[inst_index].x_axis.xyz to be orthogonal. */
  float3 x_axis = cross(y_axis, z_axis);

  float3 sp, nor;
  nor = sp = v_in.pos.xyz;

  /* In bone space */
  bool is_head = (v_in.pos.z < -sinb);
  sp *= (is_head) ? srt.data_buf[inst_index].head_sphere.w :
                    srt.data_buf[inst_index].tail_sphere.w;
  sp.z += (is_head) ? 0.0f : bone_len;

  /* Convert to world space */
  float3x3 bone_mat = float3x3(x_axis, y_axis, z_axis);
  sp = bone_mat * sp.xzy + srt.data_buf[inst_index].head_sphere.xyz;
  nor = bone_mat * nor.xzy;

  v_out.view_normal = to_float3x3(view.viewmat) * nor;

  v_out.final_state_color = srt.data_buf[inst_index].state_color.xyz;
  v_out.final_bone_color = srt.data_buf[inst_index].bone_color_and_wire_width.xyz;

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(sp,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }

  float4 pos_4d = float4(sp, 1.0f);
  out_pos = view.winmat * (view.viewmat * pos_4d);
}

[[fragment]]
void frag([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table, condition(selectable)]] draw::Select &sel,
          [[frag_coord]] const float4 frag_co,
          [[in]] const VertOut &v_out,
          [[in, condition(selectable)]] const draw::SelectOut &sel_out,
          [[out]] FragOut &frag_out)
{
  float n = normalize(v_out.view_normal).z;
  if (srt.is_distance) {
    n = 1.0f - clamp(-n, 0.0f, 1.0f);
    frag_out.color = float4(1.0f, 1.0f, 1.0f, 0.33f * armature.alpha) * n;
  }
  else {
    /* Smooth lighting factor. */
    constexpr float s = 0.2f; /* [0.0f-0.5f] range */
    float fac = clamp((n * (1.0f - s)) + s, 0.0f, 1.0f);
    frag_out.color.rgb = mix(v_out.final_state_color, v_out.final_bone_color, fac * fac);
    frag_out.color.a = armature.alpha;
  }
  frag_out.line_output = float4(0.0f);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

}  // namespace solid

}  // namespace envelope

PipelineGraphic envelope_outline(envelope::outline::vert,
                                 wire_frag,
                                 ClippingConstant{.use_clipping = false},
                                 draw::SelectConstant{.selectable = false});
PipelineGraphic envelope_outline_clipped(envelope::outline::vert,
                                         wire_frag,
                                         ClippingConstant{.use_clipping = true},
                                         draw::SelectConstant{.selectable = false});
PipelineGraphic envelope_outline_selectable(envelope::outline::vert,
                                            wire_frag,
                                            ClippingConstant{.use_clipping = false},
                                            draw::SelectConstant{.selectable = true});
PipelineGraphic envelope_outline_selectable_clipped(envelope::outline::vert,
                                                    wire_frag,
                                                    ClippingConstant{.use_clipping = true},
                                                    draw::SelectConstant{.selectable = true});

PipelineGraphic envelope_solid(envelope::solid::vert,
                               envelope::solid::frag,
                               ClippingConstant{.use_clipping = false},
                               draw::SelectConstant{.selectable = false});
PipelineGraphic envelope_solid_clipped(envelope::solid::vert,
                                       envelope::solid::frag,
                                       ClippingConstant{.use_clipping = true},
                                       draw::SelectConstant{.selectable = false});
PipelineGraphic envelope_solid_selectable(envelope::solid::vert,
                                          envelope::solid::frag,
                                          ClippingConstant{.use_clipping = false},
                                          draw::SelectConstant{.selectable = true});
PipelineGraphic envelope_solid_selectable_clipped(envelope::solid::vert,
                                                  envelope::solid::frag,
                                                  ClippingConstant{.use_clipping = true},
                                                  draw::SelectConstant{.selectable = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Shapes
 *
 * Used for custom shapes and B-bones.
 * \{ */

namespace shape {

namespace outline {

struct VertIn {
  float3 ls_P;
  float4x4 inst_matrix;
};

struct VertOut {
  float3 vs_P;
  float3 ws_P;
  float4 hs_P;
  float2 ss_P;
  float4 color_size;
  int inverted;
};

struct Resources {
  [[push_constant]] int2 gpu_attr_0;
  [[storage(0, read), frequency(GEOMETRY)]] float (&pos)[];
  [[storage(1, read)]] float4x4 (&data_buf)[];

  [[resource_table]] Uniform uni;
  [[resource_table]] IndexLoad indices;

  VertIn input_assembly(uint in_vertex_id, float4x4 inst_matrix) const
  {
    uint v_i = indices.load(in_vertex_id);
    float3 ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
    return {.ls_P = ls_P, .inst_matrix = inst_matrix};
  }

  VertOut vertex_main(const ViewMatrices view, VertIn v_in) const
  {
    float4 bone_color, state_color;
    float4x4 model_mat = extract_matrix_packed_data(v_in.inst_matrix, state_color, bone_color);

    VertOut v_out;
    v_out.ws_P = transform_point(model_mat, v_in.ls_P);
    v_out.vs_P = view.point_world_to_view(v_out.ws_P);
    v_out.hs_P = view.point_view_to_homogenous(v_out.vs_P);
    v_out.ss_P = view.perspective_divide(v_out.hs_P).xy * uni.uniform_buf.size_viewport;
    v_out.inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0f);
    v_out.color_size = bone_color;

    return v_out;
  }

  void emit_vertex(const uint strip_index,
                   uint out_vertex_id,
                   uint out_primitive_id,
                   float4 color,
                   float4 hs_P,
                   float3 ws_P,
                   float2 offset,
                   bool is_persp,
                   overlay::VertOut &v_out,
                   float4 &out_pos,
                   float3 &out_wP) const
  {
    bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
    /* Maps triangle list primitives to triangle strip indices. */
    uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                           out_primitive_id;

    if (out_strip_index != strip_index) {
      return;
    }

    v_out.final_color = color;

    out_pos = hs_P;
    /* Offset away from the center to avoid overlap with solid shape. */
    out_pos.xy += offset * uni.uniform_buf.size_viewport_inv * out_pos.w;
    /* Improve AA bleeding inside bone silhouette. */
    out_pos.z -= (is_persp) ? 1e-4f : 1e-6f;

    v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                        uni.uniform_buf.size_viewport;

    out_wP = ws_P;
  }

  void geometry_main(const ViewMatrices view,
                     const VertOut geom_in[4],
                     uint out_vertex_id,
                     uint out_primitive_id,
                     uint /*out_invocation_id*/,
                     overlay::VertOut &v_out,
                     float4 &out_pos,
                     float3 &out_wP) const
  {
    bool is_persp = (view.winmat[3][3] == 0.0f);

    float3 view_vec = (is_persp) ? normalize(geom_in[1].vs_P) : float3(0.0f, 0.0f, -1.0f);
    float3 v10 = geom_in[0].vs_P - geom_in[1].vs_P;
    float3 v12 = geom_in[2].vs_P - geom_in[1].vs_P;
    float3 v13 = geom_in[3].vs_P - geom_in[1].vs_P;

    /* Known Issue: This also generates outlines for connected-overlapping edges, since their
     * vector is zero-length. */
    float3 n0 = cross(v12, v10);
    n0 *= safe_rcp(length(n0));
    float3 n3 = cross(v13, v12);
    n3 *= safe_rcp(length(n3));

    float fac0 = dot(view_vec, n0);
    float fac3 = dot(view_vec, n3);

    /* If one of the face is perpendicular to the view,
     * consider it an outline edge. */
    if (abs(fac0) > 1e-5f && abs(fac3) > 1e-5f) {
      /* If both adjacent verts are facing the camera the same way,
       * then it isn't an outline edge. */
      if (sign(fac0) == sign(fac3)) {
        return;
      }
    }

    n0 = (geom_in[0].inverted == 1) ? -n0 : n0;
    /* Don't outline if concave edge. */
    if (dot(n0, v13) > 0.0001f) {
      return;
    }

    float2 perp = normalize(geom_in[2].ss_P - geom_in[1].ss_P);
    float2 edge_dir = float2(-perp.y, perp.x);

    float2 hidden_point;
    /* Take the farthest point to compute edge direction
     * (avoid problems with point behind near plane).
     * If the chosen point is parallel to the edge in screen space,
     * choose the other point anyway.
     * This fixes some issue with cubes in orthographic views. */
    if (geom_in[0].vs_P.z < geom_in[3].vs_P.z) {
      hidden_point = (abs(fac0) > 1e-5f) ? geom_in[0].ss_P : geom_in[3].ss_P;
    }
    else {
      hidden_point = (abs(fac3) > 1e-5f) ? geom_in[3].ss_P : geom_in[0].ss_P;
    }
    float2 hidden_dir = normalize(hidden_point - geom_in[1].ss_P);

    float fac = dot(-hidden_dir, edge_dir);
    edge_dir *= (fac < 0.0f) ? -1.0f : 1.0f;

    emit_vertex(0,
                out_vertex_id,
                out_primitive_id,
                float4(geom_in[0].color_size.rgb, 1.0f),
                geom_in[1].hs_P,
                geom_in[1].ws_P,
                edge_dir - perp,
                is_persp,
                v_out,
                out_pos,
                out_wP);

    emit_vertex(1,
                out_vertex_id,
                out_primitive_id,
                float4(geom_in[0].color_size.rgb, 1.0f),
                geom_in[2].hs_P,
                geom_in[2].ws_P,
                edge_dir + perp,
                is_persp,
                v_out,
                out_pos,
                out_wP);
  }
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[vertex_id]] const int vert_id,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[out]] overlay::VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  /* Line Adjacency primitive. */
  constexpr uint input_primitive_vertex_count = 4u;
  /* Line list primitive. */
  constexpr uint output_primitive_vertex_count = 2u;
  constexpr uint output_primitive_count = 1u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(vert_id) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(vert_id) / output_primitive_vertex_count) % output_primitive_count;
  uint out_invocation_id = (uint(vert_id) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  float4x4 inst_matrix = srt.data_buf[inst_index];

  VertIn vert_in[input_primitive_vertex_count];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u, inst_matrix);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u, inst_matrix);
  vert_in[2] = srt.input_assembly(in_primitive_first_vertex + 2u, inst_matrix);
  vert_in[3] = srt.input_assembly(in_primitive_first_vertex + 3u, inst_matrix);

  const ViewMatrices view = views.get(0);

  VertOut vert_out[input_primitive_vertex_count];
  vert_out[0] = srt.vertex_main(view, vert_in[0]);
  vert_out[1] = srt.vertex_main(view, vert_in[1]);
  vert_out[2] = srt.vertex_main(view, vert_in[2]);
  vert_out[3] = srt.vertex_main(view, vert_in[3]);

  /* Discard by default. */
  float3 ws_P = float3(NAN_FLT);
  out_pos = float4(NAN_FLT);
  srt.geometry_main(
      view, vert_out, out_vertex_id, out_primitive_id, out_invocation_id, v_out, out_pos, ws_P);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(ws_P,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }

  v_out.final_color.a = armature.alpha;
}

}  // namespace outline

namespace solid {

struct VertIn {
  [[attribute(0)]] float3 pos;
  [[attribute(1)]] float3 nor;
};

struct VertOut {
  [[smooth]] float4 final_color;
  [[flat]] int inverted;
};

struct Resources {
  [[storage(0, read)]] const float4x4 (&data_buf)[];
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  float4 bone_color, state_color;
  float4x4 inst_obmat = srt.data_buf[inst_index];
  float4x4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  /* This is slow and run per vertex, but it's still faster than
   * doing it per instance on CPU and sending it on via instance attribute. */
  float3x3 normal_mat = transpose(inverse(to_float3x3(model_mat)));
  float3 normal = normalize(view.normal_world_to_view(normal_mat * v_in.nor));

  v_out.inverted = int(dot(cross(model_mat[0].xyz, model_mat[1].xyz), model_mat[2].xyz) < 0.0f);

  /* Do lighting at an angle to avoid flat shading on front facing bone. */
  constexpr float3 light = float3(0.1f, 0.1f, 0.8f);
  float n = dot(normal, light);

  /* Smooth lighting factor. */
  constexpr float s = 0.2f; /* [0.0f-0.5f] range */
  float fac = clamp((n * (1.0f - s)) + s, 0.0f, 1.0f);
  v_out.final_color.rgb = mix(state_color.rgb, bone_color.rgb, fac * fac);
  v_out.final_color.a = armature.alpha;

  float3 ws_P = transform_point(model_mat, v_in.pos);
  out_pos = view.point_world_to_homogenous(ws_P);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(ws_P,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

[[fragment]]
void frag([[resource_table, condition(selectable)]] draw::Select &sel,
          [[frag_coord]] const float4 frag_co,
          [[front_facing]] const bool facing,
          [[in]] const VertOut &v_out,
          [[in, condition(selectable)]] const draw::SelectOut &sel_out,
          [[out]] FragOut &frag_out)
{
  /* Manual back-face culling. Not ideal for performance
   * but needed for view clarity in X-ray mode and support
   * for inverted bone matrices. */
  if ((v_out.inverted == 1) == facing) {
    gpu_discard_fragment();
    return;
  }
  frag_out.color = v_out.final_color;
  frag_out.line_output = float4(0.0f);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

}  // namespace solid

namespace wire {

struct VertIn {
  float3 lP;
  float4x4 inst_matrix;
};

struct VertOut {
  float4 gpu_position;
  float4 final_color;
  float3 world_pos;
  float wire_width;
};

struct GeomOut {
  [[flat]] float4 final_color;
  [[flat]] float wire_width;
  [[no_perspective]] float edge_coord;
};

struct Constants {
  [[compilation_constant]] bool from_line_strip;
};

struct Resources {
  [[resource_table]] Uniform uni;
  [[resource_table]] IndexLoad indices;

  [[storage(0, read), frequency(GEOMETRY)]] float (&pos)[];
  [[storage(1, read)]] float4x4 (&data_buf)[];

  [[push_constant]] int2 gpu_attr_0;
  [[push_constant]] bool use_arrow_drawing;
  [[push_constant]] bool do_smooth_wire;

  VertIn input_assembly(uint in_vertex_id, float4x4 inst_matrix) const
  {
    uint v_i = indices.load(in_vertex_id);
    float3 ls_P = gpu_attr_load_float3(pos, gpu_attr_0, v_i);
    return {.lP = ls_P, .inst_matrix = inst_matrix};
  }

  VertOut vertex_main(const ViewMatrices view, VertIn v_in) const
  {
    float4 bone_color, state_color;
    float4x4 model_mat = extract_matrix_packed_data(v_in.inst_matrix, state_color, bone_color);

    VertOut v_out;

    /* WORKAROUND: This shape needs a special vertex shader path that should be triggered by
     * its `vclass` attribute. However, to avoid many changes in the primitive expansion API,
     * we create a specific path inside the shader only for this shape batch and infer the
     * value of the `vclass` attribute based on the vertex index. */
    if (use_arrow_drawing) {
      /* Keep in sync with the arrows shape batch creation. */
      /* Adapted from `overlay_extra_vert.glsl`. */
      float3 vpos = v_in.lP;
      float3 vofs = float3(0.0f);
      uint axis = uint(vpos.z);
      /* Assumes origin vertices are the only one at Z=0. */
      if (vpos.z > 0.0f) {
        vofs[axis] = (1.0f + fract(vpos.z));
      }
      /* Scale uniformly by axis length */
      vpos *= length(model_mat[axis].xyz);
      /* World sized, camera facing geometry. */
      float3 screen_pos = view.viewinv[0].xyz * vpos.x + view.viewinv[1].xyz * vpos.y;
      v_out.world_pos = (model_mat * float4(vofs, 1.0f)).xyz + screen_pos;
    }
    else {
      v_out.world_pos = (model_mat * float4(v_in.lP, 1.0f)).xyz;
    }
    v_out.gpu_position = view.point_world_to_homogenous(v_out.world_pos);

    v_out.final_color.rgb = mix(state_color.rgb, bone_color.rgb, 0.5f);
    v_out.final_color.a = 1.0f;
    /* Because the packing clamps the value, the wire width is passed in compressed. */
    v_out.wire_width = bone_color.a * WIRE_WIDTH_COMPRESSION;

    return v_out;
  }

  void do_vertex(const uint strip_index,
                 uint out_vertex_id,
                 uint out_primitive_id,
                 float4 color,
                 float4 hs_P,
                 float3 ws_P,
                 float coord,
                 float2 offset,
                 GeomOut &v_out,
                 float4 &out_pos,
                 float3 &out_wP) const
  {
    bool is_odd_primitive = (out_primitive_id & 1u) != 0u;
    /* Maps triangle list primitives to triangle strip indices. */
    uint out_strip_index = (is_odd_primitive ? (2u - out_vertex_id) : out_vertex_id) +
                           out_primitive_id;

    if (out_strip_index != strip_index) {
      return;
    }

    v_out.final_color = color;
    v_out.edge_coord = coord;
    out_pos = hs_P;
    /* Multiply offset by 2 because gl_Position range is [-1..1]. */
    out_pos.xy += offset * 2.0f * hs_P.w;
    out_wP = ws_P;
  }

  void geometry_main(VertOut geom_in[2],
                     uint out_vertex_id,
                     uint out_primitive_id,
                     uint /*out_invocation_id*/,
                     GeomOut &v_out,
                     float4 &out_pos,
                     float3 &out_wP) const
  {
    /* Clip line against near plane to avoid deformed lines. */
    float4 pos0 = geom_in[0].gpu_position;
    float4 pos1 = geom_in[1].gpu_position;
    float2 pz_ndc = float2(pos0.z / pos0.w, pos1.z / pos1.w);
    bool2 clipped = lessThan(pz_ndc, float2(-1.0f));
    if (all(clipped)) {
      /* Totally clipped. */
      return;
    }

    float4 pos01 = pos0 - pos1;
    float ofs = abs((pz_ndc.y + 1.0f) / (pz_ndc.x - pz_ndc.y));
    if (clipped.y) {
      pos1 += pos01 * ofs;
    }
    else if (clipped.x) {
      pos0 -= pos01 * (1.0f - ofs);
    }

    float2 screen_space_pos[2];
    screen_space_pos[0] = pos0.xy / pos0.w;
    screen_space_pos[1] = pos1.xy / pos1.w;

    /* `theme.sizes.edge` is defined as the distance from the center to the outer edge.
     * As such to get the total width it needs to be doubled. */
    v_out.wire_width = geom_in[0].wire_width * (uni.uniform_buf.sizes.edge * 2);
    float half_size = max(v_out.wire_width / 2.0f, 0.5f);

    if (do_smooth_wire) {
      /* Add 1px for AA */
      half_size += 0.5f;
    }

    float2 line = (screen_space_pos[0] - screen_space_pos[1]) * uni.uniform_buf.size_viewport;
    float2 line_norm = normalize(float2(line[1], -line[0]));
    float2 edge_ofs = (half_size * line_norm) * uni.uniform_buf.size_viewport_inv;

    float4 final_color = geom_in[0].final_color;
    do_vertex(0,
              out_vertex_id,
              out_primitive_id,
              final_color,
              pos0,
              geom_in[0].world_pos,
              half_size,
              edge_ofs,
              v_out,
              out_pos,
              out_wP);
    do_vertex(1,
              out_vertex_id,
              out_primitive_id,
              final_color,
              pos0,
              geom_in[0].world_pos,
              -half_size,
              -edge_ofs,
              v_out,
              out_pos,
              out_wP);

    do_vertex(2,
              out_vertex_id,
              out_primitive_id,
              final_color,
              pos1,
              geom_in[1].world_pos,
              half_size,
              edge_ofs,
              v_out,
              out_pos,
              out_wP);
    do_vertex(3,
              out_vertex_id,
              out_primitive_id,
              final_color,
              pos1,
              geom_in[1].world_pos,
              -half_size,
              -edge_ofs,
              v_out,
              out_pos,
              out_wP);
  }
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Constants &consts,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[vertex_id]] const int vert_id,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[out]] GeomOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  /* Line primitive. */
  uint input_primitive_vertex_count = 2u;
  if (consts.from_line_strip) [[static_branch]] {
    /* Line strip primitive. */
    input_primitive_vertex_count = 1u;
  }
  /* Triangle list primitive. */
  constexpr uint output_primitive_vertex_count = 3u;
  constexpr uint output_primitive_count = 2u;
  constexpr uint output_invocation_count = 1u;
  constexpr uint output_vertex_count_per_invocation = output_primitive_count *
                                                      output_primitive_vertex_count;
  constexpr uint output_vertex_count_per_input_primitive = output_vertex_count_per_invocation *
                                                           output_invocation_count;

  uint in_primitive_id = uint(vert_id) / output_vertex_count_per_input_primitive;
  uint in_primitive_first_vertex = in_primitive_id * input_primitive_vertex_count;

  uint out_vertex_id = uint(vert_id) % output_primitive_vertex_count;
  uint out_primitive_id = (uint(vert_id) / output_primitive_vertex_count) % output_primitive_count;
  uint out_invocation_id = (uint(vert_id) / output_vertex_count_per_invocation) %
                           output_invocation_count;

  float4x4 inst_obmat = srt.data_buf[inst_index];
  float4x4 inst_matrix = inst_obmat;

  if (consts.from_line_strip) [[static_branch]] {
    uint32_t RESTART_INDEX = srt.indices.gpu_index_16bit ? 0xFFFF : 0xFFFFFFFF;
    if (srt.indices.load(in_primitive_first_vertex + 0u) == RESTART_INDEX ||
        srt.indices.load(in_primitive_first_vertex + 1u) == RESTART_INDEX)
    {
      /* Discard. */
      out_pos = float4(NAN_FLT);
      return;
    }
  }

  VertIn vert_in[2];
  vert_in[0] = srt.input_assembly(in_primitive_first_vertex + 0u, inst_matrix);
  vert_in[1] = srt.input_assembly(in_primitive_first_vertex + 1u, inst_matrix);

  VertOut vert_out[2];
  vert_out[0] = srt.vertex_main(view, vert_in[0]);
  vert_out[1] = srt.vertex_main(view, vert_in[1]);

  /* Discard by default. */
  float3 ws_P = float3(NAN_FLT);
  out_pos = float4(NAN_FLT);
  srt.geometry_main(
      vert_out, out_vertex_id, out_primitive_id, out_invocation_id, v_out, out_pos, ws_P);

  v_out.final_color.a = armature.alpha;

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(ws_P,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

float edge_step(bool do_smooth_wire, float dist)
{
  if (do_smooth_wire) {
    return smoothstep(LINE_SMOOTH_START, LINE_SMOOTH_END, dist);
  }
  return step(0.5f, dist);
}

[[fragment]]
void frag([[resource_table]] const Resources &srt,
          [[resource_table, condition(selectable)]] draw::Select &sel,
          [[frag_coord]] const float4 frag_co,
          [[in]] const GeomOut &v_out,
          [[in, condition(selectable)]] const draw::SelectOut &sel_out,
          [[out]] FragOut &frag_out)
{
  float half_size = (srt.do_smooth_wire ? v_out.wire_width - 0.5f : v_out.wire_width) / 2.0f;

  float dist = abs(v_out.edge_coord) - half_size;
  float mix_w = saturate(edge_step(srt.do_smooth_wire, dist));

  frag_out.color = mix(v_out.final_color, float4(0), mix_w);
  frag_out.color.a *= 1.0f - mix_w;
  frag_out.line_output = float4(0.0f);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

}  // namespace wire
}  // namespace shape

PipelineGraphic shape_outline(shape::outline::vert,
                              wire_frag,
                              ClippingConstant{.use_clipping = false},
                              draw::SelectConstant{.selectable = false});
PipelineGraphic shape_outline_selectable(shape::outline::vert,
                                         wire_frag,
                                         ClippingConstant{.use_clipping = false},
                                         draw::SelectConstant{.selectable = true});
PipelineGraphic shape_outline_clipped(shape::outline::vert,
                                      wire_frag,
                                      ClippingConstant{.use_clipping = true},
                                      draw::SelectConstant{.selectable = false});
PipelineGraphic shape_outline_selectable_clipped(shape::outline::vert,
                                                 wire_frag,
                                                 ClippingConstant{.use_clipping = true},
                                                 draw::SelectConstant{.selectable = true});

PipelineGraphic shape_solid(shape::solid::vert,
                            shape::solid::frag,
                            ClippingConstant{.use_clipping = false},
                            draw::SelectConstant{.selectable = false});
PipelineGraphic shape_solid_selectable(shape::solid::vert,
                                       shape::solid::frag,
                                       ClippingConstant{.use_clipping = false},
                                       draw::SelectConstant{.selectable = true});
PipelineGraphic shape_solid_clipped(shape::solid::vert,
                                    shape::solid::frag,
                                    ClippingConstant{.use_clipping = true},
                                    draw::SelectConstant{.selectable = false});
PipelineGraphic shape_solid_selectable_clipped(shape::solid::vert,
                                               shape::solid::frag,
                                               ClippingConstant{.use_clipping = true},
                                               draw::SelectConstant{.selectable = true});

PipelineGraphic shape_wire(shape::wire::vert,
                           shape::wire::frag,
                           shape::wire::Constants{.from_line_strip = false},
                           ClippingConstant{.use_clipping = false},
                           draw::SelectConstant{.selectable = false});
PipelineGraphic shape_wire_selectable(shape::wire::vert,
                                      shape::wire::frag,
                                      shape::wire::Constants{.from_line_strip = false},
                                      ClippingConstant{.use_clipping = false},
                                      draw::SelectConstant{.selectable = true});
PipelineGraphic shape_wire_clipped(shape::wire::vert,
                                   shape::wire::frag,
                                   shape::wire::Constants{.from_line_strip = false},
                                   ClippingConstant{.use_clipping = true},
                                   draw::SelectConstant{.selectable = false});
PipelineGraphic shape_wire_selectable_clipped(shape::wire::vert,
                                              shape::wire::frag,
                                              shape::wire::Constants{.from_line_strip = false},
                                              ClippingConstant{.use_clipping = true},
                                              draw::SelectConstant{.selectable = true});

PipelineGraphic shape_wire_strip(shape::wire::vert,
                                 shape::wire::frag,
                                 shape::wire::Constants{.from_line_strip = true},
                                 ClippingConstant{.use_clipping = false},
                                 draw::SelectConstant{.selectable = false});
PipelineGraphic shape_wire_strip_selectable(shape::wire::vert,
                                            shape::wire::frag,
                                            shape::wire::Constants{.from_line_strip = true},
                                            ClippingConstant{.use_clipping = false},
                                            draw::SelectConstant{.selectable = true});
PipelineGraphic shape_wire_strip_clipped(shape::wire::vert,
                                         shape::wire::frag,
                                         shape::wire::Constants{.from_line_strip = true},
                                         ClippingConstant{.use_clipping = true},
                                         draw::SelectConstant{.selectable = false});
PipelineGraphic shape_wire_strip_selectable_clipped(shape::wire::vert,
                                                    shape::wire::frag,
                                                    shape::wire::Constants{
                                                        .from_line_strip = true},
                                                    ClippingConstant{.use_clipping = true},
                                                    draw::SelectConstant{.selectable = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Sphere
 * \{ */

namespace sphere {

namespace outline {

struct VertIn {
  [[attribute(0)]] float2 pos;
};

struct Resources {
  [[storage(0, read)]] const float4x4 (&data_buf)[];
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  float4 bone_color, state_color;
  float4x4 inst_obmat = srt.data_buf[inst_index];
  float4x4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  float4x4 model_view_matrix = view.viewmat * model_mat;
  float4x4 sphere_matrix = inverse(model_view_matrix);

  bool is_persp = (view.winmat[3][3] == 0.0f);

  /* This is the local space camera ray (not normalize).
   * In perspective mode it's also the view-space position
   * of the sphere center. */
  float3 cam_ray = (is_persp) ? model_view_matrix[3].xyz : float3(0.0f, 0.0f, -1.0f);
  cam_ray = to_float3x3(sphere_matrix) * cam_ray;

  /* Sphere center distance from the camera (persp) in local space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  float3 z_axis = cam_ray / cam_dist;
  float3 x_axis = normalize(cross(sphere_matrix[1].xyz, z_axis));
  float3 y_axis = cross(z_axis, x_axis);
  float z_ofs = 0.0f;

  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    constexpr float half_pi = 3.1415926f * 0.5f;
    constexpr float rad = 0.05f;
    /* Let be (in local space):
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0f - cos_b * cos_b, 0.0f, 1.0f));

    x_axis *= sin_b;
    y_axis *= sin_b;
    z_ofs = -rad * cos_b;
  }

  /* Camera oriented position (but still in local space) */
  float3 cam_pos0 = x_axis * v_in.pos.x + y_axis * v_in.pos.y + z_axis * z_ofs;

  float4 V = model_view_matrix * float4(cam_pos0, 1.0f);
  out_pos = view.winmat * V;
  float4 center = view.winmat * float4(model_view_matrix[3].xyz, 1.0f);

  /* Offset away from the center to avoid overlap with solid shape. */
  float2 ofs_dir = normalize(uni.proj(out_pos) - uni.proj(center));
  out_pos.xy += ofs_dir * uni.uniform_buf.size_viewport_inv * out_pos.w;

  v_out.edge_start = v_out.edge_pos = uni.proj(out_pos);

  v_out.final_color = float4(bone_color.rgb, armature.alpha);

  float3 world_pos = transform_point(model_mat, cam_pos0);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace outline

namespace solid {

struct VertIn {
  [[attribute(0)]] float2 pos;
  /* Per instance. */
  [[attribute(1)]] float4 color;
};

struct VertOut {
  [[flat]] float3 final_state_color;
  [[flat]] float3 final_bone_color;
  /* Cannot interpolate matrix. */
  [[flat]] float4 sphere_matrix0;
  [[flat]] float4 sphere_matrix1;
  [[flat]] float4 sphere_matrix2;
  [[flat]] float4 sphere_matrix3;
  [[smooth]] float3 view_position;
};

struct Resources {
  [[storage(0, read)]] const float4x4 (&data_buf)[];
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  float4 bone_color, state_color;
  float4x4 inst_obmat = srt.data_buf[inst_index];
  float4x4 model_mat = extract_matrix_packed_data(inst_obmat, state_color, bone_color);

  float4x4 model_view_matrix = view.viewmat * model_mat;
  const float4x4 sphere_matrix = inverse(model_view_matrix);
  v_out.sphere_matrix0 = sphere_matrix[0];
  v_out.sphere_matrix1 = sphere_matrix[1];
  v_out.sphere_matrix2 = sphere_matrix[2];
  v_out.sphere_matrix3 = sphere_matrix[3];

  bool is_persp = (view.winmat[3][3] == 0.0f);

  /* This is the local space camera ray (not normalize).
   * In perspective mode it's also the view-space position
   * of the sphere center. */
  float3 cam_ray = (is_persp) ? model_view_matrix[3].xyz : float3(0.0f, 0.0f, -1.0f);
  cam_ray = to_float3x3(sphere_matrix) * cam_ray;

  /* Sphere center distance from the camera (persp) in local space. */
  float cam_dist = length(cam_ray);

  /* Compute view aligned orthonormal space. */
  float3 z_axis = cam_ray / cam_dist;
  float3 x_axis = normalize(cross(sphere_matrix[1].xyz, z_axis));
  float3 y_axis = cross(z_axis, x_axis);

  /* Sphere radius */
  constexpr float rad = 0.05f;

  float z_ofs = -rad - 1e-8f; /* offset to the front of the sphere */
  if (is_persp) {
    /* For perspective, the projected sphere radius
     * can be bigger than the center disc. Compute the
     * max angular size and compensate by sliding the disc
     * towards the camera and scale it accordingly. */
    constexpr float half_pi = 3.1415926f * 0.5f;
    /* Let be (in local space):
     * V the view vector origin.
     * O the sphere origin.
     * T the point on the target circle.
     * We compute the angle between (OV) and (OT). */
    float a = half_pi - asin(rad / cam_dist);
    float cos_b = cos(a);
    float sin_b = sqrt(clamp(1.0f - cos_b * cos_b, 0.0f, 1.0f));
#if 1
    /* Instead of choosing the biggest circle in screen-space,
     * we choose the nearest with the same angular size. This
     * permit us to leverage GL_ARB_conservative_depth in the
     * fragment shader. */
    float minor = cam_dist - rad;
    float major = cam_dist - cos_b * rad;
    float fac = minor / major;
    sin_b *= fac;
#else
    z_ofs = -rad * cos_b;
#endif
    x_axis *= sin_b;
    y_axis *= sin_b;
  }

  /* Camera oriented position (but still in local space) */
  float3 cam_pos = x_axis * v_in.pos.x + y_axis * v_in.pos.y + z_axis * z_ofs;

  float4 pos_4d = float4(cam_pos, 1.0f);
  float4 V = model_view_matrix * pos_4d;
  out_pos = view.winmat * V;
  v_out.view_position = V.xyz;

  v_out.final_state_color = state_color.xyz;
  v_out.final_bone_color = bone_color.xyz;

  float3 world_pos = transform_point(model_mat, cam_pos);

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

[[fragment]]
void frag([[resource_table]] const Armature &armature,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] draw::Select &sel,
          [[frag_coord]] const float4 frag_co,
          [[in]] const VertOut &v_out,
          [[in, condition(selectable)]] const draw::SelectOut &sel_out,
          [[frag_depth(GREATER), condition(!selectable)]] float &depth_out,
          [[out]] FragOut &frag_out)
{
  const ViewMatrices view = views.get(0);

  constexpr float sphere_radius = 0.05f;
  float4x4 sphere_matrix;
  sphere_matrix[0] = v_out.sphere_matrix0;
  sphere_matrix[1] = v_out.sphere_matrix1;
  sphere_matrix[2] = v_out.sphere_matrix2;
  sphere_matrix[3] = v_out.sphere_matrix3;

  bool is_perp = (view.winmat[3][3] == 0.0f);
  float3 ray_ori_view = (is_perp) ? float3(0.0f) : v_out.view_position.xyz;
  float3 ray_dir_view = (is_perp) ? v_out.view_position : float3(0.0f, 0.0f, -1.0f);

  /* Single matrix mul without branch. */
  float4 mul_vec = (is_perp) ? float4(ray_dir_view, 0.0f) : float4(ray_ori_view, 1.0f);
  float3 mul_res = (sphere_matrix * mul_vec).xyz;

  /* Reminder :
   * sphere_matrix[3] is the view space origin in sphere space (sph_ori -> view_ori).
   * sphere_matrix[2] is the view space Z axis in sphere space. */

  /* convert to sphere local space */
  float3 ray_ori = (is_perp) ? sphere_matrix[3].xyz : mul_res;
  float3 ray_dir = (is_perp) ? mul_res : -sphere_matrix[2].xyz;
  float ray_len = length(ray_dir);
  ray_dir /= ray_len;

  /* Line to sphere intersect */
  constexpr float sphere_radius_sqr = sphere_radius * sphere_radius;
  float b = dot(ray_ori, ray_dir);
  float c = dot(ray_ori, ray_ori) - sphere_radius_sqr;
  float h = b * b - c;
  float t = -sqrt(max(0.0f, h)) - b;

  /* Compute dot product for lighting */
  float3 p = ray_dir * t + ray_ori; /* Point on sphere */
  float3 n = normalize(p);          /* Normal is just the point in sphere space, normalized. */
  float3 l = normalize(sphere_matrix[2].xyz); /* Just the view Z axis in the sphere space. */

  /* Smooth lighting factor. */
  constexpr float s = 0.2f; /* [0.0f-0.5f] range */
  float fac = clamp((dot(n, l) * (1.0f - s)) + s, 0.0f, 1.0f);
  frag_out.color.rgb = mix(v_out.final_state_color, v_out.final_bone_color, fac * fac);

  /* 2x2 dither pattern to smooth the lighting. */
  float dither = (0.5f + dot(float2(int2(frag_co.xy) & int2(1)), float2(1.0f, 2.0f))) * 0.25f;
  dither *= (1.0f / 255.0f); /* Assume 8bit per color buffer. */

  frag_out.color = float4(frag_out.color.rgb + dither, armature.alpha);
  frag_out.line_output = float4(0.0f);

  t /= ray_len;

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
  else {
    depth_out = view.depth_view_to_screen(ray_dir_view.z * t + ray_ori_view.z);
  }
}

}  // namespace solid
}  // namespace sphere

PipelineGraphic sphere_outline(sphere::outline::vert,
                               wire_frag,
                               ClippingConstant{.use_clipping = false},
                               draw::SelectConstant{.selectable = false});
PipelineGraphic sphere_outline_selectable(sphere::outline::vert,
                                          wire_frag,
                                          ClippingConstant{.use_clipping = false},
                                          draw::SelectConstant{.selectable = true});
PipelineGraphic sphere_outline_clipped(sphere::outline::vert,
                                       wire_frag,
                                       ClippingConstant{.use_clipping = true},
                                       draw::SelectConstant{.selectable = false});
PipelineGraphic sphere_outline_selectable_clipped(sphere::outline::vert,
                                                  wire_frag,
                                                  ClippingConstant{.use_clipping = true},
                                                  draw::SelectConstant{.selectable = true});

PipelineGraphic sphere_solid(sphere::solid::vert,
                             sphere::solid::frag,
                             ClippingConstant{.use_clipping = false},
                             draw::SelectConstant{.selectable = false});
PipelineGraphic sphere_solid_selectable(sphere::solid::vert,
                                        sphere::solid::frag,
                                        ClippingConstant{.use_clipping = false},
                                        draw::SelectConstant{.selectable = true});
PipelineGraphic sphere_solid_clipped(sphere::solid::vert,
                                     sphere::solid::frag,
                                     ClippingConstant{.use_clipping = true},
                                     draw::SelectConstant{.selectable = false});
PipelineGraphic sphere_solid_selectable_clipped(sphere::solid::vert,
                                                sphere::solid::frag,
                                                ClippingConstant{.use_clipping = true},
                                                draw::SelectConstant{.selectable = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Stick
 * \{ */

namespace stick_bone {

struct VertIn {
  /* Position in Bone-aligned screen space. */
  [[attribute(0)]] float2 pos;
  [[attribute(1)]] int vclass;
};

struct VertOut {
  [[no_perspective]] float color_fac;
  [[flat]] float4 final_wire_color;
  [[flat]] float4 final_inner_color;
};

struct Resources {
  [[storage(0, read)]] const BoneStickData (&data_buf)[];
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[instance_index]] const int inst_index,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[in]] const VertIn &v_in,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  const ViewMatrices view = views.get(0);

  StickBoneFlag bone_flag = StickBoneFlag(v_in.vclass);
  v_out.final_inner_color = flag_test(bone_flag, COL_HEAD) ? srt.data_buf[inst_index].head_color :
                                                             srt.data_buf[inst_index].tail_color;
  v_out.final_inner_color = flag_test(bone_flag, COL_BONE) ? srt.data_buf[inst_index].bone_color :
                                                             v_out.final_inner_color;
  v_out.final_wire_color = (srt.data_buf[inst_index].wire_color.a > 0.0f) ?
                               srt.data_buf[inst_index].wire_color :
                               v_out.final_inner_color;
  v_out.color_fac = flag_test(bone_flag, COL_WIRE) ?
                        0.0f :
                        (flag_test(bone_flag, COL_BONE) ? 1.0f : 2.0f);

  float4 boneStart_4d = float4(srt.data_buf[inst_index].bone_start.xyz, 1.0f);
  float4 boneEnd_4d = float4(srt.data_buf[inst_index].bone_end.xyz, 1.0f);
  float4 v0 = view.viewmat * boneStart_4d;
  float4 v1 = view.viewmat * boneEnd_4d;

  /* Clip the bone to the camera origin plane (not the clip plane)
   * to avoid glitches if one end is behind the camera origin (in perspective mode). */
  float clip_dist = (view.winmat[3][3] == 0.0f) ?
                        -1e-7f :
                        1e20f; /* hard-coded, -1e-8f is giving glitches. */
  float3 bvec = v1.xyz - v0.xyz;
  float3 clip_pt = v0.xyz + bvec * ((v0.z - clip_dist) / -bvec.z);
  if (v0.z > clip_dist) {
    v0.xyz = clip_pt;
  }
  else if (v1.z > clip_dist) {
    v1.xyz = clip_pt;
  }

  float4 p0 = view.winmat * v0;
  float4 p1 = view.winmat * v1;

  bool is_head = flag_test(bone_flag, POS_HEAD);
  bool is_bone = flag_test(bone_flag, POS_BONE);

  float h = (is_head) ? p0.w : p1.w;

  float2 x_screen_vec = normalize(uni.proj(p1) - uni.proj(p0) + 1e-8f);
  float2 y_screen_vec = float2(x_screen_vec.y, -x_screen_vec.x);

  /* 2D screen aligned pos at the point */
  float2 vpos = v_in.pos.x * x_screen_vec + v_in.pos.y * y_screen_vec;
  vpos *= (view.winmat[3][3] == 0.0f) ? h : 1.0f;
  vpos *= (srt.data_buf[inst_index].wire_color.a > 0.0f) ? 1.0f : 0.5f;

  if (v_out.final_inner_color.a > 0.0f) {
    float stick_size = uni.uniform_buf.sizes.pixel * 5.0f;
    out_pos = (is_head) ? p0 : p1;
    out_pos.xy += stick_size * (vpos * uni.uniform_buf.size_viewport_inv);
    out_pos.z += (is_bone) ? 0.0f : 1e-6f; /* Avoid Z fighting of head/tails. */

    if (clip.constants.use_clipping) [[static_branch]] {
      clip.set_clipping_distances((is_head ? boneStart_4d : boneEnd_4d).xyz,
                                  clip_distances[0],
                                  clip_distances[1],
                                  clip_distances[2],
                                  clip_distances[3],
                                  clip_distances[4],
                                  clip_distances[5]);
    }
  }
  else {
    out_pos = float4(0.0f);
  }
}

[[fragment]]
void frag([[resource_table]] const Armature &armature,
          [[resource_table, condition(selectable)]] draw::Select &sel,
          [[frag_coord]] const float4 frag_co,
          [[in]] const VertOut &v_out,
          [[in, condition(selectable)]] const draw::SelectOut &sel_out,
          [[out]] FragOut &frag_out)
{
  float fac = smoothstep(1.0f, 0.2f, v_out.color_fac);
  frag_out.color.rgb = mix(v_out.final_inner_color.rgb, v_out.final_wire_color.rgb, fac);
  frag_out.color.a = armature.alpha;
  frag_out.line_output = float4(0.0f);

  if (sel.consts.selectable) [[static_branch]] {
    sel.select_id_output(sel_out.select_id, frag_co);
  }
}

}  // namespace stick_bone

PipelineGraphic stick(stick_bone::vert,
                      stick_bone::frag,
                      ClippingConstant{.use_clipping = false},
                      draw::SelectConstant{.selectable = false});
PipelineGraphic stick_selectable(stick_bone::vert,
                                 stick_bone::frag,
                                 ClippingConstant{.use_clipping = false},
                                 draw::SelectConstant{.selectable = true});
PipelineGraphic stick_clipped(stick_bone::vert,
                              stick_bone::frag,
                              ClippingConstant{.use_clipping = true},
                              draw::SelectConstant{.selectable = false});
PipelineGraphic stick_selectable_clipped(stick_bone::vert,
                                         stick_bone::frag,
                                         ClippingConstant{.use_clipping = true},
                                         draw::SelectConstant{.selectable = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Wire
 * \{ */

namespace wire_bone {

struct Resources {
  [[storage(0, read)]] const VertexData (&data_buf)[];
};

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Armature &armature,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::View &views,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[vertex_id]] const int vert_id,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[vert_id / 2];
  }

  const ViewMatrices view = views.get(0);

  v_out.final_color.rgb = srt.data_buf[vert_id].color_.rgb;
  v_out.final_color.a = armature.alpha;

  float3 world_pos = srt.data_buf[vert_id].pos_.xyz;
  out_pos = view.point_world_to_homogenous(world_pos);

  v_out.edge_start = v_out.edge_pos = ((out_pos.xy / out_pos.w) * 0.5f + 0.5f) *
                                      uni.uniform_buf.size_viewport;

  if (clip.constants.use_clipping) [[static_branch]] {
    clip.set_clipping_distances(world_pos,
                                clip_distances[0],
                                clip_distances[1],
                                clip_distances[2],
                                clip_distances[3],
                                clip_distances[4],
                                clip_distances[5]);
  }
}

}  // namespace wire_bone

/** \} */

PipelineGraphic wire(wire_bone::vert,
                     wire_frag,
                     ClippingConstant{.use_clipping = false},
                     draw::SelectConstant{.selectable = false});
PipelineGraphic wire_selectable(wire_bone::vert,
                                wire_frag,
                                ClippingConstant{.use_clipping = false},
                                draw::SelectConstant{.selectable = true});
PipelineGraphic wire_clipped(wire_bone::vert,
                             wire_frag,
                             ClippingConstant{.use_clipping = true},
                             draw::SelectConstant{.selectable = false});
PipelineGraphic wire_selectable_clipped(wire_bone::vert,
                                        wire_frag,
                                        ClippingConstant{.use_clipping = true},
                                        draw::SelectConstant{.selectable = true});

}  // namespace overlay::armature
