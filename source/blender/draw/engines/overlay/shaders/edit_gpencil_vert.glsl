
uniform float normalSize;
uniform bool doMultiframe;
uniform bool doStrokeEndpoints;
uniform bool hideSelect;
uniform bool doWeightColor;
uniform float gpEditOpacity;
uniform vec4 gpEditColor;
uniform sampler1D weightTex;

in vec3 pos;
in int ma;
in uint vflag;
in float weight;

out vec4 finalColor;

void discard_vert()
{
  /* We set the vertex at the camera origin to generate 0 fragments. */
  gl_Position = vec4(0.0, 0.0, -3e36, 0.0);
}

#define GP_EDIT_POINT_SELECTED 1u  /* 1 << 0 */
#define GP_EDIT_STROKE_SELECTED 2u /* 1 << 1 */
#define GP_EDIT_MULTIFRAME 4u      /* 1 << 2 */
#define GP_EDIT_STROKE_START 8u    /* 1 << 3 */
#define GP_EDIT_STROKE_END 16u     /* 1 << 4 */

#ifdef USE_POINTS
#  define colorUnselect colorGpencilVertex
#  define colorSelect colorGpencilVertexSelect
#else
#  define colorUnselect gpEditColor
#  define colorSelect (hideSelect ? colorUnselect : colorGpencilVertexSelect)
#endif

vec3 weight_to_rgb(float t)
{
  if (t < 0.0) {
    /* No weight */
    return colorUnselect.rgb;
  }
  else if (t > 1.0) {
    /* Error color */
    return vec3(1.0, 0.0, 1.0);
  }
  else {
    return texture(weightTex, t).rgb;
  }
}

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec3 world_pos = point_object_to_world(pos);
  gl_Position = point_world_to_ndc(world_pos);

  bool is_multiframe = (vflag & GP_EDIT_MULTIFRAME) != 0u;
  bool is_stroke_sel = (vflag & GP_EDIT_STROKE_SELECTED) != 0u;
  bool is_point_sel = (vflag & GP_EDIT_POINT_SELECTED) != 0u;

  if (doWeightColor) {
    finalColor.rgb = weight_to_rgb(weight);
    finalColor.a = gpEditOpacity;
  }
  else {
    finalColor = (is_point_sel) ? colorSelect : colorUnselect;
    finalColor.a *= gpEditOpacity;
  }

#ifdef USE_POINTS
  gl_PointSize = sizeVertex * 2.0;

  if (doStrokeEndpoints && !doWeightColor) {
    bool is_stroke_start = (vflag & GP_EDIT_STROKE_START) != 0u;
    bool is_stroke_end = (vflag & GP_EDIT_STROKE_END) != 0u;

    if (is_stroke_start) {
      gl_PointSize *= 2.0;
      finalColor.rgb = vec3(0.0, 1.0, 0.0);
    }
    else if (is_stroke_end) {
      gl_PointSize *= 1.5;
      finalColor.rgb = vec3(1.0, 0.0, 0.0);
    }
  }

  if ((!is_stroke_sel && !doWeightColor) || (!doMultiframe && is_multiframe)) {
    discard_vert();
  }
#endif

  /* Discard unwanted padding vertices. */
  if (ma == -1 || (is_multiframe && !doMultiframe)) {
    discard_vert();
  }

#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance(world_pos);
#endif
}
