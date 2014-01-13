/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
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

ccl_device_noinline float noise_musgrave_fBm(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 0.0f;
	float pwr = 1.0f;
	float pwHL = powf(lacunarity, -H);
	int i;

	for(i = 0; i < float_to_int(octaves); i++) {
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

ccl_device_noinline float noise_musgrave_multi_fractal(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves)
{
	float rmd;
	float value = 1.0f;
	float pwr = 1.0f;
	float pwHL = powf(lacunarity, -H);
	int i;

	for(i = 0; i < float_to_int(octaves); i++) {
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

ccl_device_noinline float noise_musgrave_hetero_terrain(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves, float offset)
{
	float value, increment, rmd;
	float pwHL = powf(lacunarity, -H);
	float pwr = pwHL;
	int i;

	/* first unscaled octave of function; later octaves are scaled */
	value = offset + snoise(p);
	p *= lacunarity;

	for(i = 1; i < float_to_int(octaves); i++) {
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

ccl_device_noinline float noise_musgrave_hybrid_multi_fractal(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight, rmd;
	float pwHL = powf(lacunarity, -H);
	float pwr = pwHL;
	int i;

	result = snoise(p) + offset;
	weight = gain * result;
	p *= lacunarity;

	for(i = 1; (weight > 0.001f) && (i < float_to_int(octaves)); i++) {
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

ccl_device_noinline float noise_musgrave_ridged_multi_fractal(float3 p, NodeNoiseBasis basis, float H, float lacunarity, float octaves, float offset, float gain)
{
	float result, signal, weight;
	float pwHL = powf(lacunarity, -H);
	float pwr = pwHL;
	int i;

	signal = offset - fabsf(snoise(p));
	signal *= signal;
	result = signal;
	weight = 1.0f;

	for(i = 1; i < float_to_int(octaves); i++) {
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

ccl_device float svm_musgrave(NodeMusgraveType type, float dimension, float lacunarity, float octaves, float offset, float intensity, float gain, float3 p)
{
	NodeNoiseBasis basis = NODE_NOISE_PERLIN;

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

ccl_device void svm_node_tex_musgrave(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
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
		dimension, lacunarity, detail, foffset, 1.0f, gain, co*scale);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END

