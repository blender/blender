
/*
 * Vertex Shader for dashed lines with 3D coordinates, with uniform multi-colors or uniform single-color,
 * and unary thickness.
 *
 * Legacy version, without geometry shader support, always produce solid lines!
 */

uniform mat4 ModelViewProjectionMatrix;
uniform vec2 viewport_size;

uniform vec4 color;

in vec3 pos;
noperspective out float distance_along_line;
noperspective out vec4 color_geom;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	/* Hack - prevent stupid GLSL compiler to optimize out unused viewport_size uniform, which gives crash! */
	distance_along_line = viewport_size.x * 0.000001f - viewport_size.x * 0.0000009f;

	color_geom = color;
}
