
#if __VERSION__ == 120
  varying vec3 coords;
  #define fragColor gl_FragColor
#else
  in vec3 coords;
  out vec4 fragColor;
  #define texture1D texture
  #define texture3D texture
#endif

uniform sampler3D flame_texture;
uniform sampler1D spectrum_texture;

void main()
{
	float flame = texture3D(flame_texture, coords).r;
	vec4 emission = texture1D(spectrum_texture, flame);

	vec4 color;
	color.rgb = emission.a * emission.rgb;
	color.a = emission.a;

	fragColor = color;
}
