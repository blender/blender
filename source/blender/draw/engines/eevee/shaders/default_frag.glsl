
uniform vec3 basecol;
uniform float metallic;
uniform float specular;
uniform float roughness;

out vec4 FragColor;

void main()
{
	vec3 dielectric = vec3(0.034) * specular * 2.0;
	vec3 diffuse = mix(basecol, vec3(0.0), metallic);
	vec3 f0 = mix(dielectric, basecol, metallic);
	FragColor = vec4(eevee_surface_lit((gl_FrontFacing) ? worldNormal : -worldNormal, diffuse, f0, roughness, 1.0), length(viewPosition));
}
