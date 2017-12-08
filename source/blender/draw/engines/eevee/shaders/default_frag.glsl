
uniform vec3 basecol;
uniform float metallic;
uniform float specular;
uniform float roughness;

Closure nodetree_exec(void)
{
	vec3 dielectric = vec3(0.034) * specular * 2.0;
	vec3 albedo = mix(basecol, vec3(0.0), metallic);
	vec3 f0 = mix(dielectric, basecol, metallic);
	vec3 N = (gl_FrontFacing) ? worldNormal : -worldNormal;
	vec3 out_diff, out_spec, ssr_spec;
	eevee_closure_default(N, albedo, f0, 1, roughness, 1.0, out_diff, out_spec, ssr_spec);

	Closure result = Closure(out_spec + out_diff * albedo, 1.0, vec4(ssr_spec, roughness), normal_encode(normalize(viewNormal), viewCameraVec), 0);

	return result;
}
