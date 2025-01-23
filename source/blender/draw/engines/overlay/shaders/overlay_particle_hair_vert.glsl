/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Draw particles as shapes using primitive expansion.
 */

#include "common_view_clipping_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_view_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"
#include "overlay_common_lib.glsl"
#include "select_lib.glsl"

/* TODO(fclem): Deduplicate wireframe color. */

void wire_color_get(out vec3 rim_col, out vec3 wire_col)
{
  eObjectInfoFlag ob_flag = eObjectInfoFlag(floatBitsToUint(drw_infos[resource_id].infos.w));
  bool is_selected = flag_test(ob_flag, OBJECT_SELECTED);
  bool is_from_set = flag_test(ob_flag, OBJECT_FROM_SET);
  bool is_active = flag_test(ob_flag, OBJECT_ACTIVE);

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

  vec3 ws_P = drw_point_object_to_world(pos);
  vec3 ws_N = normalize(drw_normal_object_to_world(-nor));

  gl_Position = drw_point_world_to_homogenous(ws_P);

  edgeStart = edgePos = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

  vec3 rim_col, wire_col;
  if (colorType == V3D_SHADING_OBJECT_COLOR || colorType == V3D_SHADING_RANDOM_COLOR) {
    wire_object_color_get(rim_col, wire_col);
  }
  else {
    wire_color_get(rim_col, wire_col);
  }

  float facing = clamp(abs(dot(ws_N, drw_world_incident_vector(ws_P))), 0.0, 1.0);

  /* Do interpolation in a non-linear space to have a better visual result. */
  rim_col = sqrt(rim_col);
  wire_col = sqrt(wire_col);
  vec3 final_front_col = mix(rim_col, wire_col, 0.35);
  finalColor.rgb = mix(rim_col, final_front_col, facing);
  finalColor.rgb = square(finalColor.rgb);
  finalColor.a = 1.0;

  view_clipping_distances(ws_P);
}
