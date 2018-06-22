
in vec2 uv_interp;
out vec4 fragColor;

uniform sampler2D image;
uniform float alpha = 1.0;

void main()
{
	fragColor = vec4(texture(image, uv_interp).rgb, alpha);
}
