
uniform vec3 diffuse_col;
uniform vec3 specular_col;
uniform int hardness;

void main()
{
	float roughness = 1.0 - float(hardness) / 511.0;
	fragColor = vec4(eevee_surface_lit(worldNormal, diffuse_col, specular_col, roughness), 1.0);
}
