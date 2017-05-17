
// Draw dashed lines, perforated in screen space.
// Based on a (3D) version by Mike Erwin.

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewport_size;

in vec2 pos;
noperspective out float distance_along_line;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);

	/* Hack - prevent stupid GLSL compiler to optimize out unused viewport_size uniform, which gives crash! */
	distance_along_line = viewport_size.x * 0.000001f - viewport_size.x * 0.0000009f;
}
