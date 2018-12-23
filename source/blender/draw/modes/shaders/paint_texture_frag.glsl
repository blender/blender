
in vec2 uv_interp;
out vec4 fragColor;

uniform sampler2D image;
uniform float alpha = 1.0;
uniform bool nearestInterp;

void main()
{
	vec2 uv = uv_interp;
	if (nearestInterp) {
		vec2 tex_size = vec2(textureSize(image, 0).xy);
		uv = (floor(uv_interp * tex_size) + 0.5) / tex_size;
	}
	fragColor = vec4(texture(image, uv).rgb, alpha);
}
