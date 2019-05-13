
uniform mat4 ProjectionMatrix;
uniform mat4 ViewProjectionMatrix;

uniform mat4 ViewMatrix;
uniform vec2 viewportSize;

/* ---- Instantiated Attrs ---- */
in vec2 pos; /* bone aligned screen space */
in uint flag;

#define COL_WIRE 1u /* (1 << 0) */
#define COL_HEAD 2u /* (1 << 1) */
#define COL_TAIL 4u /* (1 << 2) */
#define COL_BONE 8u /* (1 << 3) */

#define POS_HEAD 16u                /* (1 << 4) */
#define POS_TAIL 32u /* (1 << 5) */ /* UNUSED */
#define POS_BONE 64u                /* (1 << 6) */

/* ---- Per instance Attrs ---- */
in vec3 boneStart;
in vec3 boneEnd;
in vec4 wireColor; /* alpha encode if we do wire. If 0.0 we dont. */
in vec4 boneColor; /* alpha encode if we do bone. If 0.0 we dont. */
in vec4 headColor; /* alpha encode if we do head. If 0.0 we dont. */
in vec4 tailColor; /* alpha encode if we do tail. If 0.0 we dont. */

#define do_wire (wireColor.a > 0.0)
#define is_head bool(flag & POS_HEAD)
#define is_bone bool(flag & POS_BONE)

noperspective out float colorFac;
flat out vec4 finalWireColor;
flat out vec4 finalInnerColor;

uniform float stickSize = 5.0; /* might be dependent on DPI setting in the future. */

/* project to screen space */
vec2 proj(vec4 pos)
{
  return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

void main()
{
  finalInnerColor = ((flag & COL_HEAD) != 0u) ? headColor : tailColor;
  finalInnerColor = ((flag & COL_BONE) != 0u) ? boneColor : finalInnerColor;
  finalWireColor = (do_wire) ? wireColor : finalInnerColor;
  /* Make the color */
  colorFac = ((flag & COL_WIRE) == 0u) ? ((flag & COL_BONE) != 0u) ? 1.0 : 2.0 : 0.0;

  vec4 boneStart_4d = vec4(boneStart, 1.0);
  vec4 boneEnd_4d = vec4(boneEnd, 1.0);
  vec4 v0 = ViewMatrix * boneStart_4d;
  vec4 v1 = ViewMatrix * boneEnd_4d;

  /* Clip the bone to the camera origin plane (not the clip plane)
   * to avoid glitches if one end is behind the camera origin (in persp). */
  float clip_dist = (ProjectionMatrix[3][3] == 0.0) ?
                        -1e-7 :
                        1e20; /* hardcoded, -1e-8 is giving gliches. */
  vec3 bvec = v1.xyz - v0.xyz;
  vec3 clip_pt = v0.xyz + bvec * ((v0.z - clip_dist) / -bvec.z);
  if (v0.z > clip_dist) {
    v0.xyz = clip_pt;
  }
  else if (v1.z > clip_dist) {
    v1.xyz = clip_pt;
  }

  vec4 p0 = ProjectionMatrix * v0;
  vec4 p1 = ProjectionMatrix * v1;

  float h = (is_head) ? p0.w : p1.w;

  vec2 x_screen_vec = normalize(proj(p1) - proj(p0) + 1e-8);
  vec2 y_screen_vec = vec2(x_screen_vec.y, -x_screen_vec.x);

  /* 2D screen aligned pos at the point */
  vec2 vpos = pos.x * x_screen_vec + pos.y * y_screen_vec;
  vpos *= (ProjectionMatrix[3][3] == 0.0) ? h : 1.0;
  vpos *= (do_wire) ? 1.0 : 0.5;

  if (finalInnerColor.a > 0.0) {
    gl_Position = (is_head) ? p0 : p1;
    gl_Position.xy += stickSize * (vpos / viewportSize);
    gl_Position.z += (is_bone) ? 0.0 : 1e-6; /* Avoid Z fighting of head/tails. */

#ifdef USE_WORLD_CLIP_PLANES
    world_clip_planes_calc_clip_distance((is_head ? boneStart_4d : boneEnd_4d).xyz);
#endif
  }
  else {
    gl_Position = vec4(0.0);
  }
}
