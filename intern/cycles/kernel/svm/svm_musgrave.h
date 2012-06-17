/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

/* Musgrave fBm
 *
 * H: fractal increment parameter
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 *
 * from "Texturing and Modelling: A procedural approach"
 */

__device_noinline float noise_musgrave_fBm(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 0.0f;
	float pwr = 1.0f;
	float pwHL = pow(lacunarity, -H);
	int i;

	for(i = 0; i < (int)octaves; i++) {
		value += snoise(p) * pwr;
		pwr *= pwHL;
		p *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if(rmd != 0.0f)
		value += rmd * snoise(p) * pwr;

	return value;
}

/* Musgrave Multifractal
 *
 * H: highest fractal dimension
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 */

__device_noinline float noise_musgrave_multi_fractal(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 1.0f;
	float pwr = 1.0f;
	float pwHL = pow(lacunarity, -H);
	int i;

	for(i = 0; i < (int)octaves; i++) {
		value *= (pwr * snoise(p) + 1.0f);
		pwr *= pwHL;
		p *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if(rmd != 0.0f)
		value *= (rmd * pwr * snoise(p) + 1.0f); /* correct? */

	return value;
}

/* Musgrave Heterogeneous Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

__device_noinline float noise_musgrave_hetero_terrain(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves, float offset)
{
	float value, increment, rmd;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	/* first unscaled octave of function; later octaves are scaled */
	value = offset + snoise(p);
	p *= lacunarity;

	for(i = 1; i < (int)octaves; i++) {
		increment = (snoise(p) + offset) * pwr * value;
		value += increment;
		pwr *= pwHL;
		p *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if(rmd != 0.0f) {
		increment = (snoise(p) + offset) * pwr * value;
		value += rmd * increment;
	}

	return value;
}

/* Hybrid Additive/Multiplicative Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

__device_noinline float noise_musgrave_hybrid_multi_fractal(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight, rmd;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	result = snoise(p) + offset;
	weight = gain * result;
	p *= lacunarity;

	for(i = 1; (weight > 0.001f) && (i < (int)octaves); i++) {
		if(weight > 1.0f)
			weight = 1.0f;

		signal = (snoise(p) + offset) * pwr;
		pwr *= pwHL;
		result += weight * signal;
		weight *= gain * signal;
		p *= lacunarity;
	}

	rmd = octaves - floorf(octaves);
	if(rmd != 0.0f)
		result += rmd * ((snoise(p) + offset) * pwr);

	return result;
}

/* Ridged Multifractal Terrain
 *
 * H: fractal dimension of the roughest area
 * lacunarity: gap between successive frequencies
 * octaves: number of frequencies in the fBm
 * offset: raises the terrain from `sea level'
 */

__device_noinline float noise_musgrave_ridged_multi_fractal(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;
	int i;

	signal = offset - fabsf(snoise(p));
	signal *= signal;
	result = signal;
	weight = 1.0f;

	for(i = 1; i < (int)octaves; i++) {
		p *= lacunarity;
		weight = clamp(signal * gain, 0.0f, 1.0f);
		signal = offset - fabsf(snoise(p));
		signal *= signal;
		signal *= weight;
		result += signal * pwr;
		pwr *= pwHL;
	}

	return result;
}

/* Shader */

__device float svm_musgrave(NodeMusgraveType type, float dimension, float lacunarity, float octaves, float offset, float intensity, float gain, float scale, float3 p)
{
	NodeNoiseBasis basis = NODE_NOISE_PERLIN;
	p *= scale;

	if(type == NODE_MUSGRAVE_MULTIFRACTAL)
		return intensity*noise_musgrave_multi_fractal(p, basis, dimension, lacunarity, octaves);
	else if(type == NODE_MUSGRAVE_FBM)
		return intensity*noise_musgrave_fBm(p, basis, dimension, lacunarity, octaves);
	else if(type == NODE_MUSGRAVE_HYBRID_MULTIFRACTAL)
		return intensity*noise_musgrave_hybrid_multi_fractal(p, basis, dimension, lacunarity, octaves, offset, gain);
	else if(type == NODE_MUSGRAVE_RIDGED_MULTIFRACTAL)
		return intensity*noise_musgrave_ridged_multi_fractal(p, basis, dimension, lacunarity, octaves, offset, gain);
	else if(type == NODE_MUSGRAVE_HETERO_TERRAIN)
		return intensity*noise_musgrave_hetero_terrain(p, basis, dimension, lacunarity, octaves, offset);
	
	return 0.0f;
}

__device void svm_node_tex_musgrave(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);
	uint4 node3 = read_node(kg, offset);

	uint type, co_offset, color_offset, fac_offset;
	uint dimension_offset, lacunarity_offset, detail_offset, offset_offset;
	uint gain_offset, scale_offset;

	decode_node_uchar4(node.y, &type, &co_offset, &color_offset, &fac_offset);
	decode_node_uchar4(node.z, &dimension_offset, &lacunarity_offset, &detail_offset, &offset_offset);
	decode_node_uchar4(node.w, &gain_offset, &scale_offset, NULL, NULL);

	float3 co = stack_load_float3(stack, co_offset);
	float dimension = stack_load_float_default(stack, dimension_offset, node2.x);
	float lacunarity = stack_load_float_default(stack, lacunarity_offset, node2.y);
	float detail = stack_load_float_default(stack, detail_offset, node2.z);
	float foffset = stack_load_float_default(stack, offset_offset, node2.w);
	float gain = stack_load_float_default(stack, gain_offset, node3.x);
	float scale = stack_load_float_default(stack, scale_offset, node3.y);

	dimension = fmaxf(dimension, 1e-5f);
	detail = clamp(detail, 0.0f, 16.0f);
	lacunarity = fmaxf(lacunarity, 1e-5f);

	float f = svm_musgrave((NodeMusgraveType)type,
		dimension, lacunarity, detail, foffset, 1.0f, gain, scale, co);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END

