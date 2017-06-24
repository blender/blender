
uniform float backgroundAlpha;
uniform vec3 color;

out vec4 FragColor;

void main() {
	FragColor = vec4(color, backgroundAlpha);
}
