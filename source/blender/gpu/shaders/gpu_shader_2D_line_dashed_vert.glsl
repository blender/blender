
// Draw dashed lines, perforated in screen space.
// Based on a (3D) version by Mike Erwin.

uniform mat4 ModelViewProjectionMatrix;
uniform float view_scale;

#if __VERSION__ == 120
  attribute vec2 pos;
  attribute vec2 line_origin; // = pos for one vertex of the line
  noperspective varying float distance_along_line;
#else
  in vec2 pos;
  in vec2 line_origin;
  noperspective out float distance_along_line;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	distance_along_line = distance(line_origin, pos) * view_scale;

	/* Another solution would be to compute line_origin in fragment coordinate, and then compute distance
	 * in fragment shader, but we'd need opengl window size for that...
	 *    vec4 point = ModelViewProjectionMatrix * vec4(line_origin, 0.0, 1.0);
	 *    ref_point = (point.xy / point.w) * 0.5 + 0.5;  // <- device coordinates in [0..1] range.
	 *    ref_point = ref_point * window_size;
	 */
}
