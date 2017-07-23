
uniform vec3 basecol;
uniform float metallic;
uniform float specular;
uniform float roughness;

Closure nodetree_exec(void)
{
	vec3 dielectric = vec3(0.034) * specular * 2.0;
	vec3 diffuse = mix(basecol, vec3(0.0), metallic);
	vec3 f0 = mix(dielectric, basecol, metallic);
	vec3 ssr_spec;
	vec3 radiance = eevee_surface_lit((gl_FrontFacing) ? worldNormal : -worldNormal, diffuse, f0, roughness, 1.0, 0, ssr_spec);

	Closure result = Closure(radiance, 1.0, vec4(ssr_spec, roughness), normal_encode(normalize(viewNormal), viewCameraVec), 0);

	return result;
}
