uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

in vec3 pos;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance((ModelMatrix * vec4(pos, 1.0)).xyz);
#endif
}
