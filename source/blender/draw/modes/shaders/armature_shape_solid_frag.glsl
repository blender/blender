
uniform float alpha = 0.6;

in vec4 finalColor;

out vec4 fragColor;

void main()
{
	fragColor = vec4(finalColor.rgb, alpha);
}
