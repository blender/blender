
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

/* keep in sync with DRWManager.view_data */
layout(std140) uniform clip_block {
	vec4 ClipPlanes[1];
};

in vec3 pos;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#ifdef CLIP_PLANES
	vec4 worldPosition = (ModelMatrix * vec4(pos, 1.0));
	gl_ClipDistance[0] = dot(vec4(worldPosition.xyz, 1.0), ClipPlanes[0]);
#endif
	/* TODO motion vectors */
}
