
in vec3 coords;
out vec4 fragColor;

uniform sampler3D flame_texture;
uniform sampler1D spectrum_texture;

void main()
{
	float flame = texture(flame_texture, coords).r;
	vec4 emission = texture(spectrum_texture, flame);

	fragColor.rgb = emission.a * emission.rgb;
	fragColor.a = emission.a;
}
