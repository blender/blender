
/* Make to be used with dynamic batching so no Model Matrix needed */
uniform mat4 ViewProjectionMatrix;

layout(points) in;
layout(line_strip, max_vertices = 2) out;

void main()
{
	vec3 vert = gl_in[0].gl_Position.xyz;

	gl_Position = ViewProjectionMatrix * vec4(vert.xyz, 1.0);
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_set_clip_distance(gl_in[0].gl_ClipDistance);
#endif
	EmitVertex();

	gl_Position = ViewProjectionMatrix * vec4(vert.xy, 0.0, 1.0);
#ifdef USE_WORLD_CLIP_PLANES
	world_clip_planes_calc_clip_distance(vec3(vert.xy, 0.0));
#endif
	EmitVertex();

	EndPrimitive();
}
