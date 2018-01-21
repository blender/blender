
uniform sampler2DArray irradianceGrid;

#define IRRADIANCE_LIB

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
	ir.color = texture(irradianceGrid, vec3(uvs, 0.0)).rgb;

#elif defined(IRRADIANCE_SH_L2)

	ivec2 cell_co = ivec2(3, 3);
	int cell_per_row = textureSize(irradianceGrid, 0).x / cell_co.x;
	cell_co.x *= cell % cell_per_row;
	cell_co.y *= cell / cell_per_row;

	ivec3 ofs = ivec3(0, 1, 2);

	IrradianceData ir;
	ir.shcoefs[0] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.xx, 0), 0).rgb;
	ir.shcoefs[1] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.yx, 0), 0).rgb;
	ir.shcoefs[2] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.zx, 0), 0).rgb;
	ir.shcoefs[3] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.xy, 0), 0).rgb;
	ir.shcoefs[4] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.yy, 0), 0).rgb;
	ir.shcoefs[5] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.zy, 0), 0).rgb;
	ir.shcoefs[6] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.xz, 0), 0).rgb;
	ir.shcoefs[7] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.yz, 0), 0).rgb;
	ir.shcoefs[8] = texelFetch(irradianceGrid, ivec3(cell_co + ofs.zz, 0), 0).rgb;

#else /* defined(IRRADIANCE_HL2) */

	ivec2 cell_co = ivec2(3, 2);
	int cell_per_row = textureSize(irradianceGrid, 0).x / cell_co.x;
	cell_co.x *= cell % cell_per_row;
	cell_co.y *= cell / cell_per_row;

	ivec3 is_negative = ivec3(step(0.0, -N));

	IrradianceData ir;
	ir.cubesides[0] = irradiance_decode(texelFetch(irradianceGrid, ivec3(cell_co + ivec2(0, is_negative.x), 0), 0));
	ir.cubesides[1] = irradiance_decode(texelFetch(irradianceGrid, ivec3(cell_co + ivec2(1, is_negative.y), 0), 0));
	ir.cubesides[2] = irradiance_decode(texelFetch(irradianceGrid, ivec3(cell_co + ivec2(2, is_negative.z), 0), 0));

#endif

	return ir;
}

float load_visibility_cell(int cell, vec3 L, float dist, float bias, float bleed_bias, float range)
{
	/* Keep in sync with diffuse_filter_probe() */
	ivec2 cell_co = ivec2(prbIrradianceVisSize);
	ivec2 cell_per_row_col = textureSize(irradianceGrid, 0).xy / prbIrradianceVisSize;
	cell_co.x *= (cell % cell_per_row_col.x);
	cell_co.y *= (cell / cell_per_row_col.x) % cell_per_row_col.y;
	float layer = 1.0 + float((cell / cell_per_row_col.x) / cell_per_row_col.y);

	vec2 texel_size = 1.0 / vec2(textureSize(irradianceGrid, 0).xy);
	vec2 co = vec2(cell_co) * texel_size;

	vec2 uv = mapping_octahedron(-L, vec2(1.0 / float(prbIrradianceVisSize)));
	uv *= vec2(prbIrradianceVisSize) * texel_size;

	vec4 data = texture(irradianceGrid, vec3(co + uv, layer));

	/* Decoding compressed data */
	vec2 moments = visibility_decode(data, range);

	/* Doing chebishev test */
	float variance = abs(moments.x * moments.x - moments.y);
	variance = max(variance, bias / 10.0);

	float d = dist - moments.x;
	float p_max = variance / (variance + d * d);

	/* Increase contrast in the weight by squaring it */
	p_max *= p_max;

	/* Now reduce light-bleeding by removing the [0, x] tail and linearly rescaling (x, 1] */
	p_max = clamp((p_max - bleed_bias) / (1.0 - bleed_bias), 0.0, 1.0);

	return (dist <= moments.x) ? 1.0 : p_max;
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

vec3 irradiance_from_cell_get(int cell, vec3 ir_dir)
{
	IrradianceData ir_data = load_irradiance_cell(cell, ir_dir);
	return compute_irradiance(ir_dir, ir_data);
}
