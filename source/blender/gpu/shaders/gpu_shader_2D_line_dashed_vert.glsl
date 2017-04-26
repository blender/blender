
// Draw dashed lines, perforated in screen space.
// Based on a (3D) version by Mike Erwin.

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewport_size;

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

	vec4 point = ModelViewProjectionMatrix * vec4(line_origin, 0.0, 1.0);
	vec2 ref_point = (point.xy / point.w) * 0.5 + 0.5;  // <- device coordinates in [0..1] range.
	ref_point = ref_point * viewport_size;  // <- 'virtual' screen coordinates.

	vec2 curr_point = (gl_Position.xy / gl_Position.w) * 0.5 + 0.5;  // <- device coordinates in [0..1] range.
	curr_point = curr_point * viewport_size;  // <- 'virtual' screen coordinates.

	distance_along_line = distance(ref_point, curr_point);

	/* Note: we could also use similar approach as diag_stripes_frag, but this would give us dashed 'anchored'
	 * to the screen, and not to one end of the line... */
}
