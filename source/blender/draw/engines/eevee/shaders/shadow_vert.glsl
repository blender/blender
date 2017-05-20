
layout(std140) uniform shadow_render_block {
	mat4 ShadowMatrix[6];
	vec4 lampPosition;
	int layer;
};

uniform mat4 ShadowModelMatrix;

in vec3 pos;

out vec4 vPos;
out float lDist;

flat out int face;

void main() {
	vPos = ShadowModelMatrix * vec4(pos, 1.0);
	lDist = distance(lampPosition.xyz, vPos.xyz);
	face = gl_InstanceID;
}