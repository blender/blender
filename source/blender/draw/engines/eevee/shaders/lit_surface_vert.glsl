
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat4 ModelViewMatrix;
uniform mat3 WorldNormalMatrix;
#ifndef ATTRIB
uniform mat3 NormalMatrix;
uniform mat4 ModelMatrixInverse;
#endif

#ifndef HAIR_SHADER
in vec3 pos;
in vec3 nor;
#endif

out vec3 worldPosition;
out vec3 viewPosition;

/* Used for planar reflections */
/* keep in sync with EEVEE_ClipPlanesUniformBuffer */
layout(std140) uniform clip_block {
	vec4 ClipPlanes[1];
};

#ifdef USE_FLAT_NORMAL
flat out vec3 worldNormal;
flat out vec3 viewNormal;
#else
out vec3 worldNormal;
out vec3 viewNormal;
#endif

#ifdef HAIR_SHADER
out vec3 hairTangent;
out float hairThickTime;
out float hairThickness;
out float hairTime;
#endif

void main()
{
#ifdef HAIR_SHADER
	vec3 pos, nor;
	hair_get_pos_tan_nor_time(
	        (ProjectionMatrix[3][3] == 0.0),
	        ViewMatrixInverse[3].xyz, ViewMatrixInverse[2].xyz,
	        pos, nor, hairTangent, hairTime, hairThickness, hairThickTime);

	gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);
	viewPosition = (ViewMatrix * vec4(pos, 1.0)).xyz;
	worldPosition = pos;
	worldNormal = nor;
	viewNormal = normalize(mat3(ViewMatrixInverse) * nor);
#else
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	viewPosition = (ModelViewMatrix * vec4(pos, 1.0)).xyz;
	worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
	worldNormal = normalize(WorldNormalMatrix * nor);
	viewNormal = normalize(NormalMatrix * nor);
#endif

	/* Used for planar reflections */
	gl_ClipDistance[0] = dot(vec4(worldPosition, 1.0), ClipPlanes[0]);

#ifdef ATTRIB
	pass_attrib(pos);
#endif
}
