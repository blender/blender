
in vec2 texCoord_interp;
out vec4 fragColor;

uniform sampler2D image;

void main()
{
	float depth = texture(image, texCoord_interp).r;
	fragColor = vec4(depth);
	gl_FragDepth = depth;
}
