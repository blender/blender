
flat in vec4 finalColor;

out vec4 fragColor;

void main()
{
	fragColor = vec4(finalColor.rgb, 0.6); /* Hardcoded transparency factor. */
}
