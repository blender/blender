
/*
 * Vertex Shader for dashed lines with 2D coordinates,
 * with uniform multi-colors or uniform single-color, and unary thickness.
 *
 * Dashed is performed in screen space.
 */

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
  stipple_start = stipple_pos = viewport_size * 0.5 * (gl_Position.xy / gl_Position.w);
}
