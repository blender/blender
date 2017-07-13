
/*
 * Vertex Shader for dashed lines with 3D coordinates, with uniform multi-colors or uniform single-color,
 * and unary thickness.
 *
 * Dashed is performed in screen space.
 */

uniform mat4 ModelViewProjectionMatrix;

uniform vec4 color;

in vec3 pos;

out vec4 color_vert;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	color_vert = color;
}
