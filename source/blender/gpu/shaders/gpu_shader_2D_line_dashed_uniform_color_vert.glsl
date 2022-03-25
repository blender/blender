
/*
 * Vertex Shader for dashed lines with 2D coordinates,
 * with uniform multi-colors or uniform single-color, and unary thickness.
 *
 * Dashed is performed in screen space.
 */
#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;

uniform vec4 color;
uniform vec2 viewport_size;

in vec2 pos;

flat out vec4 color_vert;

/* We leverage hardware interpolation to compute distance along the line. */
noperspective out vec2 stipple_pos; /* In screen space */
flat out vec2 stipple_start;        /* In screen space */
#endif

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
  stipple_start = stipple_pos = viewport_size * 0.5 * (gl_Position.xy / gl_Position.w);
  color_vert = color;
}
