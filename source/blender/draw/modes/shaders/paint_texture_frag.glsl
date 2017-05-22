
in vec2 uv_interp;
out vec4 fragColor;

uniform sampler2D image;


void main()
{
	fragColor = texture(image, uv_interp);
}
