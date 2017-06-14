
uniform sampler2D irradianceGrid;

#ifdef IRRADIANCE_CUBEMAP
struct IrradianceData {
	vec3 color;
};
#elif defined(IRRADIANCE_SH_L2)
struct IrradianceData {
	vec3 shcoefs[9];
};
#else /* defined(IRRADIANCE_HL2) */
struct IrradianceData {
	vec3 cubesides[3];
};
#endif

IrradianceData load_irradiance_cell(int cell, vec3 N)
{
	/* Keep in sync with diffuse_filter_probe() */

#if defined(IRRADIANCE_CUBEMAP)

	#define AMBIANT_CUBESIZE 8
	ivec2 cell_co = ivec2(AMBIANT_CUBESIZE);
	int cell_per_row = textureSize(irradianceGrid, 0).x / cell_co.x;
	cell_co.x *= cell % cell_per_row;
	cell_co.y *= cell / cell_per_row;

	vec2 texelSize = 1.0 / vec2(AMBIANT_CUBESIZE);

	vec2 uvs = mapping_octahedron(N, texelSize);
	uvs *= vec2(AMBIANT_CUBESIZE) / vec2(textureSize(irradianceGrid, 0));
	uvs += vec2(cell_co) / vec2(textureSize(irradianceGrid, 0));

	IrradianceData ir;
	ir.color = texture(irradianceGrid, uvs).rgb;

#elif defined(IRRADIANCE_SH_L2)

	ivec2 cell_co = ivec2(3, 3);
	int cell_per_row = textureSize(irradianceGrid, 0).x / cell_co.x;
	cell_co.x *= cell % cell_per_row;
	cell_co.y *= cell / cell_per_row;

	ivec3 ofs = ivec3(0, 1, 2);

	IrradianceData ir;
	ir.shcoefs[0] = texelFetch(irradianceGrid, cell_co + ofs.xx, 0).rgb;
	ir.shcoefs[1] = texelFetch(irradianceGrid, cell_co + ofs.yx, 0).rgb;
	ir.shcoefs[2] = texelFetch(irradianceGrid, cell_co + ofs.zx, 0).rgb;
	ir.shcoefs[3] = texelFetch(irradianceGrid, cell_co + ofs.xy, 0).rgb;
	ir.shcoefs[4] = texelFetch(irradianceGrid, cell_co + ofs.yy, 0).rgb;
	ir.shcoefs[5] = texelFetch(irradianceGrid, cell_co + ofs.zy, 0).rgb;
	ir.shcoefs[6] = texelFetch(irradianceGrid, cell_co + ofs.xz, 0).rgb;
	ir.shcoefs[7] = texelFetch(irradianceGrid, cell_co + ofs.yz, 0).rgb;
	ir.shcoefs[8] = texelFetch(irradianceGrid, cell_co + ofs.zz, 0).rgb;

#else /* defined(IRRADIANCE_HL2) */

	ivec2 cell_co = ivec2(3, 2);
	int cell_per_row = textureSize(irradianceGrid, 0).x / cell_co.x;
	cell_co.x *= cell % cell_per_row;
	cell_co.y *= cell / cell_per_row;

	ivec3 is_negative = ivec3(step(0.0, -N));

	IrradianceData ir;
	ir.cubesides[0] = texelFetch(irradianceGrid, cell_co + ivec2(0, is_negative.x), 0).rgb;
	ir.cubesides[1] = texelFetch(irradianceGrid, cell_co + ivec2(1, is_negative.y), 0).rgb;
	ir.cubesides[2] = texelFetch(irradianceGrid, cell_co + ivec2(2, is_negative.z), 0).rgb;

#endif

	return ir;
}

/* http://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/ */
vec3 spherical_harmonics_L1(vec3 N, vec3 shcoefs[4])
{
	vec3 sh = vec3(0.0);

	sh += 0.282095 * shcoefs[0];

	sh += -0.488603 * N.z * shcoefs[1];
	sh += 0.488603 * N.y * shcoefs[2];
	sh += -0.488603 * N.x * shcoefs[3];

	return sh;
}

vec3 spherical_harmonics_L2(vec3 N, vec3 shcoefs[9])
{
	vec3 sh = vec3(0.0);

	sh += 0.282095 * shcoefs[0];

	sh += -0.488603 * N.z * shcoefs[1];
	sh += 0.488603 * N.y * shcoefs[2];
	sh += -0.488603 * N.x * shcoefs[3];

	sh += 1.092548 * N.x * N.z * shcoefs[4];
	sh += -1.092548 * N.z * N.y * shcoefs[5];
	sh += 0.315392 * (3.0 * N.y * N.y - 1.0) * shcoefs[6];
	sh += -1.092548 * N.x * N.y * shcoefs[7];
	sh += 0.546274 * (N.x * N.x - N.z * N.z) * shcoefs[8];

	return sh;
}

vec3 hl2_basis(vec3 N, vec3 cubesides[3])
{
	vec3 irradiance = vec3(0.0);

	vec3 n_squared = N * N;

	irradiance += n_squared.x * cubesides[0];
	irradiance += n_squared.y * cubesides[1];
	irradiance += n_squared.z * cubesides[2];

	return irradiance;
}

vec3 compute_irradiance(vec3 N, IrradianceData ird)
{
#if defined(IRRADIANCE_CUBEMAP)
	return ird.color;
#elif defined(IRRADIANCE_SH_L2)
	return spherical_harmonics_L2(N, ird.shcoefs);
#else /* defined(IRRADIANCE_HL2) */
	return hl2_basis(N, ird.cubesides);
#endif
}

vec3 get_cell_color(ivec3 localpos, ivec3 gridres, int offset, vec3 ir_dir)
{
	/* Keep in sync with update_irradiance_probe */
	int cell = offset + localpos.z + localpos.y * gridres.z + localpos.x * gridres.z * gridres.y;
	IrradianceData ir_data = load_irradiance_cell(cell, ir_dir);
	return compute_irradiance(ir_dir, ir_data);
}
