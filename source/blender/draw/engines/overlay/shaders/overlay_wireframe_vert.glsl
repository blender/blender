/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "common_view_clipping_lib.glsl"
#include "common_view_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "select_lib.glsl"

#if !defined(POINTS) && !defined(CURVES)
bool is_edge_sharpness_visible(float wire_data)
{
  return wire_data <= wireStepParam;
}
#endif

void wire_color_get(out vec3 rim_col, out vec3 wire_col)
{
#ifdef OBINFO_NEW
  eObjectInfoFlag ob_flag = eObjectInfoFlag(floatBitsToUint(drw_infos[resource_id].infos.w));
  bool is_selected = flag_test(ob_flag, OBJECT_SELECTED);
  bool is_from_set = flag_test(ob_flag, OBJECT_FROM_SET);
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);
#else
  int flag = int(abs(ObjectInfo.w));
  bool is_selected = (flag & DRW_BASE_SELECTED) != 0;
  bool is_from_set = (flag & DRW_BASE_FROM_SET) != 0;
  bool is_active = (flag & DRW_BASE_ACTIVE) != 0;
#endif

  if (is_from_set) {
    rim_col = colorWire.rgb;
    wire_col = colorWire.rgb;
  }
  else if (is_selected && useColoring) {
    if (isTransform) {
      rim_col = colorTransform.rgb;
    }
    else if (is_active) {
      rim_col = colorActive.rgb;
    }
    else {
      rim_col = colorSelect.rgb;
    }
    wire_col = colorWire.rgb;
  }
  else {
    rim_col = colorWire.rgb;
    wire_col = colorBackground.rgb;
  }
}

vec3 hsv_to_rgb(vec3 hsv)
{
  vec3 nrgb = abs(hsv.x * 6.0 - vec3(3.0, 2.0, 4.0)) * vec3(1, -1, -1) + vec3(-1, 2, 2);
  nrgb = clamp(nrgb, 0.0, 1.0);
  return ((nrgb - 1.0) * hsv.y + 1.0) * hsv.z;
}

void wire_object_color_get(out vec3 rim_col, out vec3 wire_col)
{
  int flag = int(abs(ObjectInfo.w));
  bool is_selected = (flag & DRW_BASE_SELECTED) != 0;

  if (colorType == V3D_SHADING_OBJECT_COLOR) {
    rim_col = wire_col = ObjectColor.rgb * 0.5;
  }
  else {
    float hue = ObjectInfo.z;
    vec3 hsv = vec3(hue, 0.75, 0.8);
    rim_col = wire_col = hsv_to_rgb(hsv);
  }

  if (is_selected && useColoring) {
    /* "Normalize" color. */
    wire_col += 1e-4; /* Avoid division by 0. */
    float brightness = max(wire_col.x, max(wire_col.y, wire_col.z));
    wire_col *= 0.5 / brightness;
    rim_col += 0.75;
  }
  else {
    rim_col *= 0.5;
    wire_col += 0.5;
  }
}

void main()
{
  select_id_set(drw_CustomID);

  vec3 wpos = point_object_to_world(pos);
#if defined(POINTS)
  gl_PointSize = sizeVertex * 2.0;
#elif defined(CURVES)
  /* Noop */
#else
  bool no_attr = all(equal(nor, vec3(0)));
  vec3 wnor = no_attr ? drw_view.viewinv[2].xyz : normalize(normal_object_to_world(nor));

  if (isHair) {
    mat4 obmat = hairDupliMatrix;
    wpos = (obmat * vec4(pos, 1.0)).xyz;
    wnor = -normalize(to_float3x3(obmat) * nor);
  }

  bool is_persp = (drw_view.winmat[3][3] == 0.0);
  vec3 V = (is_persp) ? normalize(drw_view.viewinv[3].xyz - wpos) : drw_view.viewinv[2].xyz;

  float facing = dot(wnor, V);
#endif

  gl_Position = point_world_to_ndc(wpos);

#ifndef CUSTOM_DEPTH_BIAS_CONST
/* TODO(fclem): Cleanup after overlay next. */
#  ifndef CUSTOM_DEPTH_BIAS
  const bool use_custom_depth_bias = false;
#  else
  const bool use_custom_depth_bias = true;
#  endif
#endif

#if !defined(POINTS) && !defined(CURVES)
  if (!use_custom_depth_bias) {
    float facing_ratio = clamp(1.0 - facing * facing, 0.0, 1.0);
    float flip = sign(facing);           /* Flip when not facing the normal (i.e.: back-facing). */
    float curvature = (1.0 - wd * 0.75); /* Avoid making things worse for curvy areas. */
    vec3 wofs = wnor * (facing_ratio * curvature * flip);
    wofs = normal_world_to_view(wofs);

    /* Push vertex half a pixel (maximum) in normal direction. */
    gl_Position.xy += wofs.xy * sizeViewportInv * gl_Position.w;

    /* Push the vertex towards the camera. Helps a bit. */
    gl_Position.z -= facing_ratio * curvature * 1.0e-6 * gl_Position.w;
  }
#endif

  vec3 rim_col, wire_col;
  if (colorType == V3D_SHADING_OBJECT_COLOR || colorType == V3D_SHADING_RANDOM_COLOR) {
    wire_object_color_get(rim_col, wire_col);
  }
  else {
    wire_color_get(rim_col, wire_col);
  }

#if defined(POINTS)
  finalColor = wire_col.rgbb;
  finalColorInner = rim_col.rgbb;

#else
  /* Convert to screen position [0..sizeVp]. */
  edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
  edgePos = edgeStart;

#  if defined(CURVES)
  finalColor.rgb = rim_col;
#  elif !defined(SELECT_EDGES)
  facing = clamp(abs(facing), 0.0, 1.0);
  /* Do interpolation in a non-linear space to have a better visual result. */
  rim_col = pow(rim_col, vec3(1.0 / 2.2));
  wire_col = pow(wire_col, vec3(1.0 / 2.2));
  vec3 final_front_col = mix(rim_col, wire_col, 0.35);
  finalColor.rgb = mix(rim_col, final_front_col, facing);
  finalColor.rgb = pow(finalColor.rgb, vec3(2.2));
#  endif

  finalColor.a = wireOpacity;
  finalColor.rgb *= wireOpacity;

#  if !defined(CURVES)
  /* Cull flat edges below threshold. */
  if (!no_attr && !is_edge_sharpness_visible(wd)) {
    edgeStart = vec2(-1.0);
  }
#  endif

#  ifdef SELECT_EDGES
  /* HACK: to avoid losing sub-pixel object in selections, we add a bit of randomness to the
   * wire to at least create one fragment that will pass the occlusion query. */
  gl_Position.xy += sizeViewportInv * gl_Position.w * ((gl_VertexID % 2 == 0) ? -1.0 : 1.0);
#  endif
#endif

  view_clipping_distances(wpos);
}
