uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

#ifdef USE_WORLD_CLIP_PLANES
uniform vec4 WorldClipPlanes[6];
uniform int  WorldClipPlanesLen;
#endif

in vec3 pos;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

#ifdef USE_WORLD_CLIP_PLANES
	{
		vec3 worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
		for (int i = 0; i < WorldClipPlanesLen; i++) {
			gl_ClipDistance[i] = dot(WorldClipPlanes[i].xyz, worldPosition) + WorldClipPlanes[i].w;
		}
	}
#endif
}
