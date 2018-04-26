
in vec2 texCoord_interp;
out vec4 fragColor;

uniform vec4 color;
uniform sampler2D image;

void main()
{
	vec4 tex = texture(image, texCoord_interp);
	tex.rgb = 0.3333333 * vec3(tex.r + tex.g + tex.b);
	fragColor = tex * color;
}
