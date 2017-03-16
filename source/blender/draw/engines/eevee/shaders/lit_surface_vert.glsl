
uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;
uniform mat3 WorldNormalMatrix;

in vec3 pos;
in vec3 nor;

out vec3 worldPosition;
out vec3 worldNormal;

void main() {
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
	worldNormal = WorldNormalMatrix * nor;
}