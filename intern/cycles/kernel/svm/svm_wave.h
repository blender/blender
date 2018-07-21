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
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Wave */

ccl_device_noinline float svm_wave(NodeWaveType type, NodeWaveProfile profile, float3 p, float detail, float distortion, float dscale)
{
	float n;

	if(type == NODE_WAVE_BANDS)
		n = (p.x + p.y + p.z) * 10.0f;
	else /* NODE_WAVE_RINGS */
		n = len(p) * 20.0f;

	if(distortion != 0.0f)
		n += distortion * noise_turbulence(p*dscale, detail, 0);

	if(profile == NODE_WAVE_PROFILE_SIN) {
		return 0.5f + 0.5f * sinf(n);
	}
	else { /* NODE_WAVE_PROFILE_SAW */
		n /= M_2PI_F;
		n -= (int) n;
		return (n < 0.0f)? n + 1.0f: n;
	}
}

ccl_device void svm_node_tex_wave(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node, int *offset)
{
	uint4 node2 = read_node(kg, offset);

	uint type;
	uint co_offset, scale_offset, detail_offset, dscale_offset, distortion_offset, color_offset, fac_offset;

	decode_node_uchar4(node.y, &type, &color_offset, &fac_offset, &dscale_offset);
	decode_node_uchar4(node.z, &co_offset, &scale_offset, &detail_offset, &distortion_offset);

	float3 co = stack_load_float3(stack, co_offset);
	float scale = stack_load_float_default(stack, scale_offset, node2.x);
	float detail = stack_load_float_default(stack, detail_offset, node2.y);
	float distortion = stack_load_float_default(stack, distortion_offset, node2.z);
	float dscale = stack_load_float_default(stack, dscale_offset, node2.w);

	float f = svm_wave((NodeWaveType)type, (NodeWaveProfile)node.w, co*scale, detail, distortion, dscale);

	if(stack_valid(fac_offset)) stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset)) stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END
