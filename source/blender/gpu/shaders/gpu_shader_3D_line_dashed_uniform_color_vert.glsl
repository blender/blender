
/*
 * Vertex Shader for dashed lines with 3D coordinates,
 * with uniform multi-colors or uniform single-color, and unary thickness.
 *
 * Dashed is performed in screen space.
 */

#ifndef USE_GPU_SHADER_CREATE_INFO

uniform mat4 ModelViewProjectionMatrix;

#  ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#  endif

uniform vec4 color;
uniform vec2 viewport_size;

in vec3 pos;

flat out vec4 color_vert;

/* We leverage hardware interpolation to compute distance along the line. */
noperspective out vec2 stipple_pos; /* In screen space */
flat out vec2 stipple_start;        /* In screen space */
#endif

void main()
{
  vec4 pos_4d = vec4(pos, 1.0);
  gl_Position = ModelViewProjectionMatrix * pos_4d;
  stipple_start = stipple_pos = viewport_size * 0.5 * (gl_Position.xy / gl_Position.w);
  color_vert = color;
#ifdef USE_WORLD_CLIP_PLANES
  world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
