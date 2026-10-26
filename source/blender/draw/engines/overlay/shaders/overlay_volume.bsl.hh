/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_model.bsl.hh"
#include "draw_view.bsl.hh"
#include "gpu_shader_math_matrix_construct.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"
#include "gpu_shader_math_vector_compare.bsl.hh"
#include "gpu_shader_math_vector_safe.bsl.hh"
#include "overlay_common.bsl.hh"

namespace overlay::volume {

/* -------------------------------------------------------------------- */
/** \name Volume Grid-Lines
 * \{ */

namespace gridlines {

struct Resources {
  [[push_constant]] float slice_position;
  [[push_constant]] int slice_axis;
  /* FluidDomainSettings.res */
  [[push_constant]] int3 volume_size;
  /* FluidDomainSettings.cell_size */
  [[push_constant]] float3 cell_size;
  /* FluidDomainSettings.p0 */
  [[push_constant]] float3 domain_origin_offset;
  /* FluidDomainSettings.res_min */
  [[push_constant]] int3 adaptive_cell_offset;
  [[push_constant]] int in_select_id;

  [[compilation_constant]] bool show_flags;
  [[compilation_constant]] bool show_range;

  [[push_constant, condition(show_range)]] float lower_bound;
  [[push_constant, condition(show_range)]] float upper_bound;
  [[push_constant, condition(show_range)]] float4 range_color;
  [[push_constant, condition(show_range)]] int cell_filter;
  [[sampler(0), condition(show_range || show_flags)]] usampler3D flag_tx;
  [[sampler(1), condition(show_range)]] sampler3D field_tx;
};

float4 flag_to_color(uint flag)
{
  /* Color mapping for flags */
  float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  /* Cell types: 1 is Fluid, 2 is Obstacle, 4 is Empty, 8 is Inflow, 16 is Outflow */
  if (bool(flag & uint(1))) {
    color.rgb += float3(0.0f, 0.0f, 0.75f); /* blue */
  }
  if (bool(flag & uint(2))) {
    color.rgb += float3(0.4f, 0.4f, 0.4f); /* gray */
  }
  if (bool(flag & uint(4))) {
    color.rgb += float3(0.25f, 0.0f, 0.2f); /* dark purple */
  }
  if (bool(flag & uint(8))) {
    color.rgb += float3(0.0f, 0.5f, 0.0f); /* dark green */
  }
  if (bool(flag & uint(16))) {
    color.rgb += float3(0.9f, 0.3f, 0.0f); /* orange */
  }
  if (is_zero(color.rgb)) {
    color.rgb += float3(0.5f, 0.0f, 0.0f); /* medium red */
  }
  return color;
}

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::Model &models,
          [[resource_table]] const draw::View &views,
          [[resource_table]] const draw::Resource &resources,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[instance_index]] const int inst_index,
          [[vertex_id]] const int vert_id,
          [[out]] VertOut &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  int cell = vert_id / 8;
  float3x3 rot_mat = float3x3(0.0f);

  float3 cell_offset = float3(0.5f);
  int3 cell_div = srt.volume_size;
  if (srt.slice_axis == 0) {
    cell_offset.x = srt.slice_position * float(srt.volume_size.x);
    cell_div.x = 1;
    rot_mat[2].x = 1.0f;
    rot_mat[0].y = 1.0f;
    rot_mat[1].z = 1.0f;
  }
  else if (srt.slice_axis == 1) {
    cell_offset.y = srt.slice_position * float(srt.volume_size.y);
    cell_div.y = 1;
    rot_mat[1].x = 1.0f;
    rot_mat[2].y = 1.0f;
    rot_mat[0].z = 1.0f;
  }
  else if (srt.slice_axis == 2) {
    cell_offset.z = srt.slice_position * float(srt.volume_size.z);
    cell_div.z = 1;
    rot_mat[0].x = 1.0f;
    rot_mat[1].y = 1.0f;
    rot_mat[2].z = 1.0f;
  }

  int3 cell_co;
  cell_co.x = cell % cell_div.x;
  cell_co.y = (cell / cell_div.x) % cell_div.y;
  cell_co.z = cell / (cell_div.x * cell_div.y);

  v_out.final_color = float4(0.0f, 0.0f, 0.0f, 1.0f);

  if (srt.show_range || srt.show_flags) [[static_branch]] {
    uint flag = texelFetch(srt.flag_tx, cell_co + int3(cell_offset), 0).r;
    if (srt.show_flags) [[static_branch]] {
      v_out.final_color = flag_to_color(flag);
    }
    else if (srt.show_range) [[static_branch]] {
      float value = texelFetch(srt.field_tx, cell_co + int3(cell_offset), 0).r;
      if (value >= srt.lower_bound && value <= srt.upper_bound) {
        if (srt.cell_filter == 0 || bool(uint(srt.cell_filter) & flag)) {
          v_out.final_color = srt.range_color;
        }
      }
    }
  }
  /* NOTE(Metal): Declaring constant arrays in function scope to avoid increasing local shader
   * memory pressure. */
  const int indices[8] = {0, 1, 1, 2, 2, 3, 3, 0};

  /* Corners for cell outlines. 0.45 is arbitrary. Any value below 0.5f can be used to avoid
   * overlapping of the outlines. */
  const float3 corners[4] = {float3(-0.45f, 0.45f, 0.0f),
                             float3(0.45f, 0.45f, 0.0f),
                             float3(0.45f, -0.45f, 0.0f),
                             float3(-0.45f, -0.45f, 0.0f)};

  float3 pos = srt.domain_origin_offset +
               srt.cell_size * (float3(cell_co + srt.adaptive_cell_offset) + cell_offset);
  float3 rotated_pos = rot_mat * corners[indices[vert_id % 8]];
  pos += rotated_pos * srt.cell_size;

  uint id = resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = models.get(id);
  float3 world_pos = model.point_object_to_world(pos);

  ViewMatrices view = views.get(0);
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

}  // namespace gridlines

PipelineGraphic gridlines_flat(gridlines::vert,
                               wire_frag,
                               gridlines::Resources{
                                   .show_flags = false,
                                   .show_range = false,
                               },
                               ClippingConstant{.use_clipping = false},
                               draw::SelectConstant{.selectable = false});
PipelineGraphic gridlines_flat_clipped(gridlines::vert,
                                       wire_frag,
                                       gridlines::Resources{
                                           .show_flags = false,
                                           .show_range = false,
                                       },
                                       ClippingConstant{.use_clipping = true},
                                       draw::SelectConstant{.selectable = false});
PipelineGraphic gridlines_flat_selectable(gridlines::vert,
                                          wire_frag,
                                          gridlines::Resources{
                                              .show_flags = false,
                                              .show_range = false,
                                          },
                                          ClippingConstant{.use_clipping = false},
                                          draw::SelectConstant{.selectable = true});
PipelineGraphic gridlines_flat_selectable_clipped(gridlines::vert,
                                                  wire_frag,
                                                  gridlines::Resources{
                                                      .show_flags = false,
                                                      .show_range = false,
                                                  },
                                                  ClippingConstant{.use_clipping = true},
                                                  draw::SelectConstant{.selectable = true});

PipelineGraphic gridlines_flags(gridlines::vert,
                                wire_frag,
                                gridlines::Resources{
                                    .show_flags = true,
                                    .show_range = false,
                                },
                                ClippingConstant{.use_clipping = false},
                                draw::SelectConstant{.selectable = false});
PipelineGraphic gridlines_flags_clipped(gridlines::vert,
                                        wire_frag,
                                        gridlines::Resources{
                                            .show_flags = true,
                                            .show_range = false,
                                        },
                                        ClippingConstant{.use_clipping = true},
                                        draw::SelectConstant{.selectable = false});
PipelineGraphic gridlines_flags_selectable(gridlines::vert,
                                           wire_frag,
                                           gridlines::Resources{
                                               .show_flags = true,
                                               .show_range = false,
                                           },
                                           ClippingConstant{.use_clipping = false},
                                           draw::SelectConstant{.selectable = true});
PipelineGraphic gridlines_flags_selectable_clipped(gridlines::vert,
                                                   wire_frag,
                                                   gridlines::Resources{
                                                       .show_flags = true,
                                                       .show_range = false,
                                                   },
                                                   ClippingConstant{.use_clipping = true},
                                                   draw::SelectConstant{.selectable = true});

PipelineGraphic gridlines_range(gridlines::vert,
                                wire_frag,
                                gridlines::Resources{
                                    .show_flags = false,
                                    .show_range = true,
                                },
                                ClippingConstant{.use_clipping = false},
                                draw::SelectConstant{.selectable = false});
PipelineGraphic gridlines_range_clipped(gridlines::vert,
                                        wire_frag,
                                        gridlines::Resources{
                                            .show_flags = false,
                                            .show_range = true,
                                        },
                                        ClippingConstant{.use_clipping = true},
                                        draw::SelectConstant{.selectable = false});
PipelineGraphic gridlines_range_selectable(gridlines::vert,
                                           wire_frag,
                                           gridlines::Resources{
                                               .show_flags = false,
                                               .show_range = true,
                                           },
                                           ClippingConstant{.use_clipping = false},
                                           draw::SelectConstant{.selectable = true});
PipelineGraphic gridlines_range_selectable_clipped(gridlines::vert,
                                                   wire_frag,
                                                   gridlines::Resources{
                                                       .show_flags = false,
                                                       .show_range = true,
                                                   },
                                                   ClippingConstant{.use_clipping = true},
                                                   draw::SelectConstant{.selectable = true});

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Velocity
 * \{ */

namespace velocity {

struct Resources {
  [[sampler(0)]] sampler3D velocity_x;
  [[sampler(1)]] sampler3D velocity_y;
  [[sampler(2)]] sampler3D velocity_z;

  [[push_constant]] float display_size;
  [[push_constant]] float slice_position;
  [[push_constant]] int slice_axis;
  [[push_constant]] bool scale_with_magnitude;
  [[push_constant]] bool is_cell_centered;
  /* FluidDomainSettings.cell_size */
  [[push_constant]] float3 cell_size;
  /* FluidDomainSettings.p0 */
  [[push_constant]] float3 domain_origin_offset;
  /* FluidDomainSettings.res_min */
  [[push_constant]] int3 adaptive_cell_offset;
  [[push_constant]] int in_select_id;

  [[compilation_constant]] bool use_needle;
  [[compilation_constant]] bool use_mac;

  [[push_constant, condition(use_mac)]] bool draw_macx;
  [[push_constant, condition(use_mac)]] bool draw_macy;
  [[push_constant, condition(use_mac)]] bool draw_macz;

  float3 get_vector(int3 cell_co) const
  {
    float3 vector;

    vector.x = texelFetch(velocity_x, cell_co, 0).r;
    vector.y = texelFetch(velocity_y, cell_co, 0).r;
    vector.z = texelFetch(velocity_z, cell_co, 0).r;

    return vector;
  }

  /* Interpolate MAC information for cell-centered vectors. */
  float3 get_vector_centered(int3 cell_co) const
  {
    float3 vector;

    vector.x = 0.5f * (texelFetch(velocity_x, cell_co, 0).r +
                       texelFetch(velocity_x, int3(cell_co.x + 1, cell_co.yz), 0).r);
    vector.y = 0.5f * (texelFetch(velocity_y, cell_co, 0).r +
                       texelFetch(velocity_y, int3(cell_co.x, cell_co.y + 1, cell_co.z), 0).r);
    vector.z = 0.5f * (texelFetch(velocity_z, cell_co, 0).r +
                       texelFetch(velocity_z, int3(cell_co.xy, cell_co.z + 1), 0).r);

    return vector;
  }

  /* Interpolate cell-centered information for MAC vectors. */
  float3 get_vector_mac(int3 cell_co) const
  {
    float3 vector;
    if (use_mac) [[static_branch]] {
      vector.x = 0.5f * (texelFetch(velocity_x, int3(cell_co.x - 1, cell_co.yz), 0).r +
                         texelFetch(velocity_x, cell_co, 0).r);
      vector.y = 0.5f * (texelFetch(velocity_y, int3(cell_co.x, cell_co.y - 1, cell_co.z), 0).r +
                         texelFetch(velocity_y, cell_co, 0).r);
      vector.z = 0.5f * (texelFetch(velocity_z, int3(cell_co.xy, cell_co.z - 1), 0).r +
                         texelFetch(velocity_z, cell_co, 0).r);
    }
    return vector;
  }
};

/* Straight Port from BKE_defvert_weight_to_rgb()
 * TODO: port this to a color ramp. */
float3 weight_to_color(float weight)
{
  float3 rgb = float3(0.0f);
  float blend = ((weight / 2.0f) + 0.5f);

  if (weight <= 0.25f) { /* blue->cyan */
    rgb.g = blend * weight * 4.0f;
    rgb.b = blend;
  }
  else if (weight <= 0.50f) { /* cyan->green */
    rgb.g = blend;
    rgb.b = blend * (1.0f - ((weight - 0.25f) * 4.0f));
  }
  else if (weight <= 0.75f) { /* green->yellow */
    rgb.r = blend * ((weight - 0.50f) * 4.0f);
    rgb.g = blend;
  }
  else if (weight <= 1.0f) { /* yellow->red */
    rgb.r = blend;
    rgb.g = blend * (1.0f - ((weight - 0.75f) * 4.0f));
  }
  else {
    /* exceptional value, unclamped or nan,
     * avoid uninitialized memory use */
    rgb = float3(1.0f, 0.0f, 1.0f);
  }

  return rgb;
}

[[vertex]]
void vert([[resource_table]] const Resources &srt,
          [[resource_table]] const Uniform &uni,
          [[resource_table]] const Clipping &clip,
          [[resource_table]] const draw::Model &models,
          [[resource_table]] const draw::View &views,
          [[resource_table]] const draw::Resource &resources,
          [[resource_table, condition(selectable)]] const draw::Select &sel,
          [[out, condition(selectable)]] draw::SelectOut &sel_out,
          [[clip_distance, condition(use_clipping)]] float (&clip_distances)[6],
          [[instance_index]] const int inst_index,
          [[vertex_id]] const int vert_id,
          [[out]] VertOutSmooth &v_out,
          [[position]] float4 &out_pos)
{
  if (sel.consts.selectable) [[static_branch]] {
    sel_out.select_id = sel.in_select_buf[inst_index];
  }

  int cell = vert_id / 2; /* Line */
  if (srt.use_needle) [[static_branch]] {
    cell = vert_id / 12;
  }
  else if (srt.use_mac) [[static_branch]] {
    cell = vert_id / 6;
  }

  int3 volume_size = textureSize(srt.velocity_x, 0);

  int3 cell_ofs = int3(0);
  int3 cell_div = volume_size;
  if (srt.slice_axis == 0) {
    cell_ofs.x = int(srt.slice_position * float(volume_size.x));
    cell_div.x = 1;
  }
  else if (srt.slice_axis == 1) {
    cell_ofs.y = int(srt.slice_position * float(volume_size.y));
    cell_div.y = 1;
  }
  else if (srt.slice_axis == 2) {
    cell_ofs.z = int(srt.slice_position * float(volume_size.z));
    cell_div.z = 1;
  }

  int3 cell_co;
  cell_co.x = cell % cell_div.x;
  cell_co.y = (cell / cell_div.x) % cell_div.y;
  cell_co.z = cell / (cell_div.x * cell_div.y);
  cell_co += cell_ofs;

  float3 pos = srt.domain_origin_offset +
               srt.cell_size * (float3(cell_co + srt.adaptive_cell_offset) + 0.5f);

  float3 vector;
  if (srt.use_mac) [[static_branch]] {
    vector = (srt.is_cell_centered) ? srt.get_vector_mac(cell_co) : srt.get_vector(cell_co);

    float3 color;
    switch (vert_id % 6) {
      case 0: /* Tail of X component. */
        pos.x += (srt.draw_macx) ? -0.5f * srt.cell_size.x : 0.0f;
        color = float3(1.0f, 0.0f, 0.0f); /* red */
        break;
      case 1: /* Head of X component. */
        pos.x += (srt.draw_macx) ? (-0.5f + vector.x * srt.display_size) * srt.cell_size.x : 0.0f;
        color = float3(1.0f, 1.0f, 0.0f); /* yellow */
        break;
      case 2: /* Tail of Y component. */
        pos.y += (srt.draw_macy) ? -0.5f * srt.cell_size.y : 0.0f;
        color = float3(0.0f, 1.0f, 0.0f); /* green */
        break;
      case 3: /* Head of Y component. */
        pos.y += (srt.draw_macy) ? (-0.5f + vector.y * srt.display_size) * srt.cell_size.y : 0.0f;
        color = float3(1.0f, 1.0f, 0.0f); /* yellow */
        break;
      case 4: /* Tail of Z component. */
        pos.z += (srt.draw_macz) ? -0.5f * srt.cell_size.z : 0.0f;
        color = float3(0.0f, 0.0f, 1.0f); /* blue */
        break;
      case 5: /* Head of Z component. */
        pos.z += (srt.draw_macz) ? (-0.5f + vector.z * srt.display_size) * srt.cell_size.z : 0.0f;
        color = float3(1.0f, 1.0f, 0.0f); /* yellow */
        break;
    }
    v_out.final_color = float4(color, 1.0f);
  }
  else {
    vector = (srt.is_cell_centered) ? srt.get_vector(cell_co) : srt.get_vector_centered(cell_co);

    v_out.final_color = float4(weight_to_color(length(vector)), 1.0f);

    float vector_length = 1.0f;
    if (srt.scale_with_magnitude) {
      vector_length = length(vector);
    }
    else if (length(vector) == 0.0f) {
      vector_length = 0.0f;
    }

    float3x3 rot_mat = from_up_axis(safe_normalize(vector));

    if (srt.use_needle) [[static_branch]] {
      /* NOTE(Metal): Declaring constant arrays in function scope to avoid increasing local shader
       * memory pressure. */
      const float3 corners[4] = {float3(0.0f, 0.2f, -0.5f),
                                 float3(-0.2f * 0.866f, -0.2f * 0.5f, -0.5f),
                                 float3(0.2f * 0.866f, -0.2f * 0.5f, -0.5f),
                                 float3(0.0f, 0.0f, 0.5f)};

      const int indices[12] = {0, 1, 1, 2, 2, 0, 0, 3, 1, 3, 2, 3};

      float3 rotated_pos = rot_mat * corners[indices[vert_id % 12]];
      pos += rotated_pos * vector_length * srt.display_size * srt.cell_size;
    }
    else {
      float3 rotated_pos = rot_mat * float3(0.0f, 0.0f, 1.0f);
      pos += ((vert_id % 2) == 1) ?
                 rotated_pos * vector_length * srt.display_size * srt.cell_size :
                 float3(0.0f);
    }
  }

  uint id = resources.get(inst_index).resource_id<1>();
  ObjectMatrices model = models.get(id);
  float3 world_pos = model.point_object_to_world(pos);

  ViewMatrices view = views.get(0);
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

}  // namespace velocity

/** \} */

PipelineGraphic velocity_streamline(velocity::vert,
                                    wire_frag_smooth,
                                    velocity::Resources{
                                        .use_needle = false,
                                        .use_mac = false,
                                    },
                                    ClippingConstant{.use_clipping = false},
                                    draw::SelectConstant{.selectable = false});
PipelineGraphic velocity_streamline_clipped(velocity::vert,
                                            wire_frag_smooth,
                                            velocity::Resources{
                                                .use_needle = false,
                                                .use_mac = false,
                                            },
                                            ClippingConstant{.use_clipping = true},
                                            draw::SelectConstant{.selectable = false});
PipelineGraphic velocity_streamline_selectable(velocity::vert,
                                               wire_frag_smooth,
                                               velocity::Resources{
                                                   .use_needle = false,
                                                   .use_mac = false,
                                               },
                                               ClippingConstant{.use_clipping = false},
                                               draw::SelectConstant{.selectable = true});
PipelineGraphic velocity_streamline_selectable_clipped(velocity::vert,
                                                       wire_frag_smooth,
                                                       velocity::Resources{
                                                           .use_needle = false,
                                                           .use_mac = false,
                                                       },
                                                       ClippingConstant{.use_clipping = true},
                                                       draw::SelectConstant{.selectable = true});

PipelineGraphic velocity_mac(velocity::vert,
                             wire_frag_smooth,
                             velocity::Resources{
                                 .use_needle = false,
                                 .use_mac = true,
                             },
                             ClippingConstant{.use_clipping = false},
                             draw::SelectConstant{.selectable = false});
PipelineGraphic velocity_mac_clipped(velocity::vert,
                                     wire_frag_smooth,
                                     velocity::Resources{
                                         .use_needle = false,
                                         .use_mac = true,
                                     },
                                     ClippingConstant{.use_clipping = true},
                                     draw::SelectConstant{.selectable = false});
PipelineGraphic velocity_mac_selectable(velocity::vert,
                                        wire_frag_smooth,
                                        velocity::Resources{
                                            .use_needle = false,
                                            .use_mac = true,
                                        },
                                        ClippingConstant{.use_clipping = false},
                                        draw::SelectConstant{.selectable = true});
PipelineGraphic velocity_mac_selectable_clipped(velocity::vert,
                                                wire_frag_smooth,
                                                velocity::Resources{
                                                    .use_needle = false,
                                                    .use_mac = true,
                                                },
                                                ClippingConstant{.use_clipping = true},
                                                draw::SelectConstant{.selectable = true});

PipelineGraphic velocity_needle(velocity::vert,
                                wire_frag_smooth,
                                velocity::Resources{
                                    .use_needle = true,
                                    .use_mac = false,
                                },
                                ClippingConstant{.use_clipping = false},
                                draw::SelectConstant{.selectable = false});
PipelineGraphic velocity_needle_clipped(velocity::vert,
                                        wire_frag_smooth,
                                        velocity::Resources{
                                            .use_needle = true,
                                            .use_mac = false,
                                        },
                                        ClippingConstant{.use_clipping = true},
                                        draw::SelectConstant{.selectable = false});
PipelineGraphic velocity_needle_selectable(velocity::vert,
                                           wire_frag_smooth,
                                           velocity::Resources{
                                               .use_needle = true,
                                               .use_mac = false,
                                           },
                                           ClippingConstant{.use_clipping = false},
                                           draw::SelectConstant{.selectable = true});
PipelineGraphic velocity_needle_selectable_clipped(velocity::vert,
                                                   wire_frag_smooth,
                                                   velocity::Resources{
                                                       .use_needle = true,
                                                       .use_mac = false,
                                                   },
                                                   ClippingConstant{.use_clipping = true},
                                                   draw::SelectConstant{.selectable = true});

}  // namespace overlay::volume
