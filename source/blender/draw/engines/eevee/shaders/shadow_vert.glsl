
uniform mat4 ModelViewProjectionMatrix;
#ifdef MESH_SHADER
uniform mat4 ModelViewMatrix;
uniform mat3 WorldNormalMatrix;
#  ifndef ATTRIB
uniform mat4 ModelMatrix;
uniform mat3 NormalMatrix;
#  endif
#endif

in vec3 pos;
in vec3 nor;

#ifdef MESH_SHADER
out vec3 worldPosition;
out vec3 viewPosition;
out vec3 worldNormal;
out vec3 viewNormal;
#endif

void main() {
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
#ifdef MESH_SHADER
	viewPosition = (ModelViewMatrix * vec4(pos, 1.0)).xyz;
	worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
	viewNormal = normalize(NormalMatrix * nor);
	worldNormal = normalize(WorldNormalMatrix * nor);
#ifdef ATTRIB
	pass_attrib(pos);
#endif
#endif
}
