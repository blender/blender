
uniform mat4 ModelMatrix;
#ifdef MESH_SHADER
uniform mat3 WorldNormalMatrix;
#endif

in vec3 pos;
#ifdef MESH_SHADER
in vec3 nor;
#endif

out vec4 vPos;
#ifdef MESH_SHADER
out vec3 vNor;
#endif

flat out int face;

void main() {
	vPos = ModelMatrix * vec4(pos, 1.0);
	face = gl_InstanceID;

#ifdef MESH_SHADER
	vNor = WorldNormalMatrix * nor;
#ifdef ATTRIB
	pass_attrib(pos);
#endif
#endif
}
