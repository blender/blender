
/*
 * Vertex Shader for dashed lines with 3D coordinates, with uniform multi-colors or uniform single-color,
 * and unary thickness.
 *
 * Dashed is performed in screen space.
 */

uniform mat4 ModelViewProjectionMatrix;

#ifdef USE_WORLD_CLIP_PLANES
uniform mat4 ModelMatrix;
#endif

uniform vec4 color;

in vec3 pos;

out vec4 color_vert;

void main()
{
	vec4 pos_4d = vec4(pos, 1.0);
	gl_Position = ModelViewProjectionMatrix * pos_4d;
	color_vert = color;
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * pos_4d).xyz);
#endif
}
