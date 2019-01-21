
layout(points) in;
layout(line_strip, max_vertices=2) out;

#ifdef USE_WORLD_CLIP_PLANES
uniform vec4 WorldClipPlanes[6];
uniform int  WorldClipPlanesLen;
#endif

flat in vec4 v1[1];
flat in vec4 v2[1];
#ifdef USE_WORLD_CLIP_PLANES
flat in vec3 wsPos[1];
#endif

void main()
{
#ifdef USE_WORLD_CLIP_PLANES
	float clip_distance[6];
	{
		vec3 worldPosition = wsPos[0];
		for (int i = 0; i < WorldClipPlanesLen; i++) {
			clip_distance[i] = dot(WorldClipPlanes[i].xyz, worldPosition) + WorldClipPlanes[i].w;
		}
	}
#endif

	for (int v = 0; v < 2; v++) {
		gl_Position = (v == 0) ? v1[0] : v2[0];
#ifdef USE_WORLD_CLIP_PLANES
		for (int i = 0; i < WorldClipPlanesLen; i++) {
			gl_ClipDistance[i] = clip_distance[i];
		}
#endif
		EmitVertex();
	}
	EndPrimitive();
}
