/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_volume_infos.hh"

VERTEX_SHADER_CREATE_INFO(overlay_volume_velocity_mac)

#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "select_lib.glsl"

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

float3x3 rotation_from_vector(float3 v)
{
  /* Add epsilon to avoid NaN. */
  float3 N = normalize(v + 1e-8f);
  float3 UpVector = abs(N.z) < 0.99999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
  float3 T = normalize(cross(UpVector, N));
  float3 B = cross(N, T);
  return float3x3(T, B, N);
}

float3 get_vector(int3 cell_co)
{
  float3 vector;

  vector.x = texelFetch(velocity_x, cell_co, 0).r;
  vector.y = texelFetch(velocity_y, cell_co, 0).r;
  vector.z = texelFetch(velocity_z, cell_co, 0).r;

  return vector;
}

/* Interpolate MAC information for cell-centered vectors. */
float3 get_vector_centered(int3 cell_co)
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
float3 get_vector_mac(int3 cell_co)
{
  float3 vector;

  vector.x = 0.5f * (texelFetch(velocity_x, int3(cell_co.x - 1, cell_co.yz), 0).r +
                     texelFetch(velocity_x, cell_co, 0).r);
  vector.y = 0.5f * (texelFetch(velocity_y, int3(cell_co.x, cell_co.y - 1, cell_co.z), 0).r +
                     texelFetch(velocity_y, cell_co, 0).r);
  vector.z = 0.5f * (texelFetch(velocity_z, int3(cell_co.xy, cell_co.z - 1), 0).r +
                     texelFetch(velocity_z, cell_co, 0).r);

  return vector;
}

void main()
{
  select_id_set(in_select_id);

#ifdef USE_NEEDLE
  int cell = gl_VertexID / 12;
#elif defined(USE_MAC)
  int cell = gl_VertexID / 6;
#else
  int cell = gl_VertexID / 2;
#endif

  int3 volume_size = textureSize(velocity_x, 0);

  int3 cell_ofs = int3(0);
  int3 cell_div = volume_size;
  if (slice_axis == 0) {
    cell_ofs.x = int(slice_position * float(volume_size.x));
    cell_div.x = 1;
  }
  else if (slice_axis == 1) {
    cell_ofs.y = int(slice_position * float(volume_size.y));
    cell_div.y = 1;
  }
  else if (slice_axis == 2) {
    cell_ofs.z = int(slice_position * float(volume_size.z));
    cell_div.z = 1;
  }

  int3 cell_co;
  cell_co.x = cell % cell_div.x;
  cell_co.y = (cell / cell_div.x) % cell_div.y;
  cell_co.z = cell / (cell_div.x * cell_div.y);
  cell_co += cell_ofs;

  float3 pos = domain_origin_offset + cell_size * (float3(cell_co + adaptive_cell_offset) + 0.5f);

  float3 vector;

#ifdef USE_MAC
  float3 color;
  vector = (is_cell_centered) ? get_vector_mac(cell_co) : get_vector(cell_co);

  switch (gl_VertexID % 6) {
    case 0: /* Tail of X component. */
      pos.x += (draw_macx) ? -0.5f * cell_size.x : 0.0f;
      color = float3(1.0f, 0.0f, 0.0f); /* red */
      break;
    case 1: /* Head of X component. */
      pos.x += (draw_macx) ? (-0.5f + vector.x * display_size) * cell_size.x : 0.0f;
      color = float3(1.0f, 1.0f, 0.0f); /* yellow */
      break;
    case 2: /* Tail of Y component. */
      pos.y += (draw_macy) ? -0.5f * cell_size.y : 0.0f;
      color = float3(0.0f, 1.0f, 0.0f); /* green */
      break;
    case 3: /* Head of Y component. */
      pos.y += (draw_macy) ? (-0.5f + vector.y * display_size) * cell_size.y : 0.0f;
      color = float3(1.0f, 1.0f, 0.0f); /* yellow */
      break;
    case 4: /* Tail of Z component. */
      pos.z += (draw_macz) ? -0.5f * cell_size.z : 0.0f;
      color = float3(0.0f, 0.0f, 1.0f); /* blue */
      break;
    case 5: /* Head of Z component. */
      pos.z += (draw_macz) ? (-0.5f + vector.z * display_size) * cell_size.z : 0.0f;
      color = float3(1.0f, 1.0f, 0.0f); /* yellow */
      break;
  }

  final_color = float4(color, 1.0f);
#else
  vector = (is_cell_centered) ? get_vector(cell_co) : get_vector_centered(cell_co);

  final_color = float4(weight_to_color(length(vector)), 1.0f);

  float vector_length = 1.0f;

  if (scale_with_magnitude) {
    vector_length = length(vector);
  }
  else if (length(vector) == 0.0f) {
    vector_length = 0.0f;
  }

  float3x3 rot_mat = rotation_from_vector(vector);

#  ifdef USE_NEEDLE
  /* NOTE(Metal): Declaring constant arrays in function scope to avoid increasing local shader
   * memory pressure. */
  constexpr float3 corners[4] = float3_array(float3(0.0f, 0.2f, -0.5f),
                                             float3(-0.2f * 0.866f, -0.2f * 0.5f, -0.5f),
                                             float3(0.2f * 0.866f, -0.2f * 0.5f, -0.5f),
                                             float3(0.0f, 0.0f, 0.5f));

  constexpr int indices[12] = int_array(0, 1, 1, 2, 2, 0, 0, 3, 1, 3, 2, 3);

  float3 rotated_pos = rot_mat * corners[indices[gl_VertexID % 12]];
  pos += rotated_pos * vector_length * display_size * cell_size;
#  else
  float3 rotated_pos = rot_mat * float3(0.0f, 0.0f, 1.0f);
  pos += ((gl_VertexID % 2) == 1) ? rotated_pos * vector_length * display_size * cell_size :
                                    float3(0.0f);
#  endif
#endif

  float3 world_pos = drw_point_object_to_world(pos);
  gl_Position = drw_point_world_to_homogenous(world_pos);
}
