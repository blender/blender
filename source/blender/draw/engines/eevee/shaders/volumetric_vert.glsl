/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  /* Generate Triangle : less memory fetches from a VBO */
  int v_id = gl_VertexID % 3; /* Vertex Id */
  int t_id = gl_VertexID / 3; /* Triangle Id */

  /* Crappy diagram
   * ex 1
   *    | \
   *    |   \
   *  1 |     \
   *    |       \
   *    |         \
   *  0 |           \
   *    |             \
   *    |               \
   * -1 0 --------------- 2
   *   -1     0     1     ex
   */
  volumetric_vert_iface.vPos.x = float(v_id / 2) * 4.0 - 1.0; /* int divisor round down */
  volumetric_vert_iface.vPos.y = float(v_id % 2) * 4.0 - 1.0;
  volumetric_vert_iface.vPos.z = float(t_id);
  volumetric_vert_iface.vPos.w = 1.0;

  PASS_RESOURCE_ID

#ifdef GPU_METAL
  volumetric_geom_iface.slice = int(volumetric_vert_iface.vPos.z);
  gpu_Layer = int(volumetric_vert_iface.vPos.z);
  gl_Position = volumetric_vert_iface.vPos.xyww;
#endif
}

/* Stubs */
vec2 bsdf_lut(float a, float b, float c, bool d)
{
  return vec2(0.0);
}

void bsdf_lut(vec3 F0,
              vec3 F90,
              vec3 transmission_tint,
              float cos_theta,
              float roughness,
              float ior,
              bool do_multiscatter,
              out vec3 reflectance,
              out vec3 transmittance)
{
  reflectance = vec3(0.0);
  transmittance = vec3(0.0);
  return;
}

vec2 brdf_lut(float a, float b)
{
  return vec2(0.0);
}

void brdf_f82_tint_lut(vec3 F0,
                       vec3 F82,
                       float cos_theta,
                       float roughness,
                       bool do_multiscatter,
                       out vec3 reflectance)
{
}

vec3 F_brdf_multi_scatter(vec3 a, vec3 b, vec2 c)
{
  return vec3(0.0);
}

vec3 F_brdf_single_scatter(vec3 a, vec3 b, vec2 c)
{
  return vec3(0.0);
}

float F_eta(float a, float b)
{
  return 0.0;
}

vec3 coordinate_camera(vec3 P)
{
  return vec3(0.0);
}

vec3 coordinate_screen(vec3 P)
{
  return vec3(0.0);
}

vec3 coordinate_reflect(vec3 P, vec3 N)
{
  return vec3(0.0);
}

vec3 coordinate_incoming(vec3 P)
{
  return vec3(0.0);
}

float attr_load_temperature_post(float attr)
{
  return attr;
}

vec4 attr_load_color_post(vec4 attr)
{
  return attr;
}

vec4 attr_load_uniform(vec4 attr, const uint attr_hash)
{
  return attr;
}
