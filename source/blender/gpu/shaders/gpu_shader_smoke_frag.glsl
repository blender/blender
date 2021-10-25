
varying vec3 coords;

uniform vec3 active_color;
uniform float step_size;
uniform float density_scale;

uniform sampler3D soot_texture;
uniform sampler3D shadow_texture;

#ifdef USE_COBA
uniform sampler1D transfer_texture;
uniform sampler3D color_band_texture;
#endif

void main()
{
	/* compute color and density from volume texture */
	vec4 soot = texture3D(soot_texture, coords);

#ifndef USE_COBA
	vec3 soot_color;
	if (soot.a != 0) {
		soot_color = active_color * soot.rgb / soot.a;
	}
	else {
		soot_color = vec3(0, 0, 0);
	}
	float soot_density = density_scale * soot.a;

	/* compute transmittance and alpha */
	float soot_transmittance = pow(2.71828182846, -soot_density * step_size);
	float soot_alpha = 1.0 - soot_transmittance;

	/* shade */
	float shadow = texture3D(shadow_texture, coords).r;
	soot_color *= soot_transmittance * shadow;

	/* premultiply alpha */
	vec4 color = vec4(soot_alpha * soot_color, soot_alpha);
#else
	float color_band = texture3D(color_band_texture, coords).r;
	vec4 transfer_function = texture1D(transfer_texture, color_band);
	vec4 color = transfer_function * density_scale;
#endif

	gl_FragColor = color;
}
