
in vec3 pos;

out vec4 vPos;
flat out int face;

void main() {
	vPos = vec4(pos, 1.0);
	face = gl_InstanceID;
}
