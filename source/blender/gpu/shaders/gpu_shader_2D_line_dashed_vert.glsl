
// Draw dashed lines, perforated in screen space.
// Based on a (3D) version by Mike Erwin.

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
	gl_Position = gl_ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	distance_along_line = distance(line_origin, pos);
}
