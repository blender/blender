
uniform mat4 ShadowMatrix;
uniform mat4 ModelMatrix;

in vec3 pos;

out vec4 vPos;

void main() {
	vPos = ModelMatrix * vec4(pos, 1.0);
}