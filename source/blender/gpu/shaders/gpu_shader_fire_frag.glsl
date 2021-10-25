
varying vec3 coords;

uniform sampler3D flame_texture;
uniform sampler1D spectrum_texture;

void main()
{
	float flame = texture3D(flame_texture, coords).r;
	vec4 emission = texture1D(spectrum_texture, flame);

	vec4 color;
	color.rgb = emission.a * emission.rgb;
	color.a = emission.a;

	gl_FragColor = color;
}
