
in vec2 pos;

out int instance;
out vec2 vPos;

void main() {
	instance = gl_InstanceID;
	vPos = pos;
}
