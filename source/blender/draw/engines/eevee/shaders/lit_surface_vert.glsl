
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat4 ModelViewMatrix;
uniform mat3 WorldNormalMatrix;
#ifndef ATTRIB
uniform mat3 NormalMatrix;
#endif

in vec3 pos;
in vec3 nor;

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

void main() {
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	viewPosition = (ModelViewMatrix * vec4(pos, 1.0)).xyz;
	worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
	viewNormal = normalize(NormalMatrix * nor);
	worldNormal = normalize(WorldNormalMatrix * nor);

	/* Used for planar reflections */
	gl_ClipDistance[0] = dot(vec4(worldPosition, 1.0), ClipPlanes[0]);

#ifdef ATTRIB
	pass_attrib(pos);
#endif
}
