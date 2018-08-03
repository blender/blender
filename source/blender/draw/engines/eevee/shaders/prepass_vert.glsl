
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

#ifdef CLIP_PLANES
/* keep in sync with DRWManager.view_data */
layout(std140) uniform clip_block {
	vec4 ClipPlanes[1];
};
#endif

#ifndef HAIR_SHADER
in vec3 pos;
#endif

void main()
{
#ifdef HAIR_SHADER
	float time, thick_time, thickness;
	vec3 pos, tan, binor;
	hair_get_pos_tan_binor_time(
	        (ProjectionMatrix[3][3] == 0.0),
	        ViewMatrixInverse[3].xyz, ViewMatrixInverse[2].xyz,
	        pos, tan, binor, time, thickness, thick_time);

	gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);
	vec4 worldPosition = vec4(pos, 1.0);
#else
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vec4 worldPosition = (ModelMatrix * vec4(pos, 1.0));
#endif

#ifdef CLIP_PLANES
	gl_ClipDistance[0] = dot(vec4(worldPosition.xyz, 1.0), ClipPlanes[0]);
#endif
	/* TODO motion vectors */
}
