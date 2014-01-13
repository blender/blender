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

/* Noise */

ccl_device_inline void svm_noise(float3 p, float detail, float distortion, float *fac, float3 *color)
{
	NodeNoiseBasis basis = NODE_NOISE_PERLIN;
	int hard = 0;

	if(distortion != 0.0f) {
		float3 r, offset = make_float3(13.5f, 13.5f, 13.5f);

		r.x = noise_basis(p + offset, basis) * distortion;
		r.y = noise_basis(p, basis) * distortion;
		r.z = noise_basis(p - offset, basis) * distortion;

		p += r;
	}

	*fac = noise_turbulence(p, basis, detail, hard);
	*color = make_float3(*fac,
		noise_turbulence(make_float3(p.y, p.x, p.z), basis, detail, hard),
		noise_turbulence(make_float3(p.y, p.z, p.x), basis, detail, hard));
}

ccl_device void svm_node_tex_noise(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint co_offset, scale_offset, detail_offset, distortion_offset, fac_offset, color_offset;

	decode_node_uchar4(node.y, &co_offset, &scale_offset, &detail_offset, &distortion_offset);

	uint4 node2 = read_node(kg, offset);

	float scale = stack_load_float_default(stack, scale_offset, node2.x);
	float detail = stack_load_float_default(stack, detail_offset, node2.y);
	float distortion = stack_load_float_default(stack, distortion_offset, node2.z);
	float3 co = stack_load_float3(stack, co_offset);

	float3 color;
	float f;

	svm_noise(co*scale, detail, distortion, &f, &color);

	decode_node_uchar4(node.z, &color_offset, &fac_offset, NULL, NULL);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, color);
}

CCL_NAMESPACE_END

