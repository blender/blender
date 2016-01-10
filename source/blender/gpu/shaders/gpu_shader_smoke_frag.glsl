
varying vec3 coords;

uniform vec3 active_color;
uniform float step_size;
uniform float density_scale;

uniform sampler3D soot_texture;
uniform sampler3D shadow_texture;

#ifdef USE_FIRE
uniform sampler3D flame_texture;
uniform sampler1D spectrum_texture;
#endif

void main()
{
	/* compute color and density from volume texture */
	vec4 soot = texture3D(soot_texture, coords);
	vec3 soot_color = active_color * soot.rgb / soot.a;
	float soot_density = density_scale * soot.a;

	/* compute transmittance and alpha */
	float soot_transmittance = pow(2.71828182846, -soot_density * step_size);
	float soot_alpha = 1.0 - soot_transmittance;

	/* shade */
	float shadow = texture3D(shadow_texture, coords).r;
	soot_color *= soot_transmittance * shadow;

	/* premultiply alpha */
	vec4 color = vec4(soot_alpha * soot_color, soot_alpha);

#ifdef USE_FIRE
	/* fire */
	float flame = texture3D(flame_texture, coords).r;
	vec4 emission = texture1D(spectrum_texture, flame);
	color.rgb += (1 - color.a) * emission.a * emission.rgb;
#endif

	gl_FragColor = color;
}
