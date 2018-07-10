
flat in int pid;
in vec2 quadCoord;

out vec4 FragColor;

void main()
{
	float dist_sqr = dot(quadCoord, quadCoord);

	/* Discard outside the circle. */
	if (dist_sqr > 1.0)
		discard;

	vec3 view_nor = vec3(quadCoord, sqrt(max(0.0, 1.0 - dist_sqr)));
	vec3 world_ref = mat3(ViewMatrixInverse) * reflect(vec3(0.0, 0.0, -1.0), view_nor);
	FragColor = vec4(textureLod_octahedron(probeCubes, vec4(world_ref, pid), 0.0, prbLodCubeMax).rgb, 1.0);
}
