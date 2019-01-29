
/* Made to be used with dynamic batching so no Model Matrix needed */
uniform mat4 ViewProjectionMatrix;

in vec3 pos;

void main()
{
	vec4 pos_4d = vec4(pos.xy, 0.0, 1.0);
	gl_Position = ViewProjectionMatrix * pos_4d;
	gl_PointSize = 2.0;

#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance(pos_4d.xyz);
#endif
}
