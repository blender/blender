/* Keep 'gpu_shader_3D_smooth_color_vert.glsl' compatible. */
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in vec3 pos;
in vec4 color;

out vec4 finalColor;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalColor = color;
#ifdef USE_WORLD_CLIP_PLANES
       world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
