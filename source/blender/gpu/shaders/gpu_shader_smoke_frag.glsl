
varying vec3 coords;

uniform vec4 active_color;
uniform float cell_spacing;

uniform sampler3D soot_texture;
uniform sampler3D shadow_texture;

#ifdef USE_FIRE
uniform sampler3D flame_texture;
uniform sampler1D spectrum_texture;
#endif

void main()
{
	vec4 soot = texture3D(soot_texture, coords);

	/* unpremultiply volume texture */
	float value = 1.0f / soot.a;
	soot.xyz *= vec3(value);

	/* calculate shading factor from soot */
	value = soot.a * active_color.a;
	value *= cell_spacing;
	value *= 1.442695041;
	soot = vec4(pow(2.0, -value));

	/* alpha */
	soot.a = 1.0 - soot.r;

	/* shade colors */
	vec3 shadow = texture3D(shadow_texture, coords).rrr;
	soot.xyz *= shadow;
	soot.xyz *= active_color.xyz;

	/* premultiply alpha */
	vec4 color = vec4(soot.a * soot.rgb, soot.a);

#ifdef USE_FIRE
	/* blend in fire */
	float flame = texture3D(flame_texture, coords).r;
	vec4 spec = texture1D(spectrum_texture, flame);
	color = vec4(color.rgb + (1 - color.a) * spec.a * spec.rgb, color.a);
#endif

	gl_FragColor = color;
}
