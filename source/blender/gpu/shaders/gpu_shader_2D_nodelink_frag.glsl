
in float colorGradient;
in vec4 finalColor;

out vec4 fragColor;

void main() {
	fragColor = finalColor;
	fragColor.a *= smoothstep(1.0, 0.1, abs(colorGradient));
}
