
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
#ifdef CLIP_PLANES
uniform vec4 ClipPlanes[1];
#endif

in vec3 pos;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#ifdef CLIP_PLANES
	vec4 worldPosition = (ModelMatrix * vec4(pos, 1.0));
	gl_ClipDistance[0] = dot(worldPosition, ClipPlanes[0]);
#endif
	/* TODO motion vectors */
}
