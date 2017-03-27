
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in float edgeWidthModulator;

out vec4 pos_xformed;
out float widthModulator;

void main() {
	pos_xformed = ModelViewProjectionMatrix * vec4(pos, 1.0);
	widthModulator = edgeWidthModulator;
}
