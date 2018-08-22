
in vec2 texCoord_interp;
out vec4 fragColor;

uniform vec4 color;
uniform sampler2D image;

void main()
{
	fragColor = texture(image, texCoord_interp).r * color.rgba;
	/* Premul by alpha (not texture alpha)
	* Use blending function GPU_blend_set_func(GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA); */
	fragColor.rgb *= color.a;
}
