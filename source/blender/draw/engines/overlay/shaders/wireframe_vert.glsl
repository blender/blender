
uniform float wireStepParam;
uniform bool useColoring;
uniform bool isTransform;
uniform bool isObjectColor;
uniform bool isRandomColor;

in vec3 pos;
in vec3 nor;
in float wd; /* wiredata */

#ifndef SELECT_EDGES
out vec3 finalColor;
flat out vec2 edgeStart;
noperspective out vec2 edgePos;
#endif

float get_edge_sharpness(float wd)
{
  return ((wd == 0.0) ? -1.5 : wd) + wireStepParam;
}

void wire_color_get(out vec3 rim_col, out vec3 wire_col)
{
  int flag = int(abs(ObjectInfo.w));
  bool is_selected = (flag & DRW_BASE_SELECTED) != 0;
  bool is_from_dupli = (flag & DRW_BASE_FROM_DUPLI) != 0;
  bool is_from_set = (flag & DRW_BASE_FROM_SET) != 0;
  bool is_active = (flag & DRW_BASE_ACTIVE) != 0;

  if (is_from_set) {
    rim_col = colorDupli.rgb;
    wire_col = colorDupli.rgb;
  }
  else if (is_from_dupli) {
    if (is_selected) {
      if (isTransform) {
        rim_col = colorTransform.rgb;
      }
      else {
        rim_col = colorDupliSelect.rgb;
      }
    }
    else {
      rim_col = colorDupli.rgb;
    }
    wire_col = colorDupli.rgb;
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

  if (isObjectColor) {
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
  vec3 wpos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(wpos);

  if (get_edge_sharpness(wd) < 0.0) {
    /* Discard primitive by placing any of the verts at the camera origin. */
    gl_Position = vec4(0.0, 0.0, -3e36, 0.0);
  }

#ifndef SELECT_EDGES
  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;

  vec3 rim_col, wire_col;
  if (isObjectColor || isRandomColor) {
    wire_object_color_get(rim_col, wire_col);
  }
  else {
    wire_color_get(rim_col, wire_col);
  }

  vec3 wnor = normalize(normal_object_to_world(nor));
  float facing = dot(wnor, ViewMatrixInverse[2].xyz);
  facing = clamp(abs(facing), 0.0, 1.0);

  vec3 final_front_col = mix(rim_col, wire_col, 0.4);
  vec3 final_rim_col = mix(rim_col, wire_col, 0.1);
  finalColor = mix(final_rim_col, final_front_col, facing);
#endif

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(wpos);
#endif
}
