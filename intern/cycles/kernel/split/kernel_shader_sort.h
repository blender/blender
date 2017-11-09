/*
 * Copyright 2011-2017 Blender Foundation
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


ccl_device void kernel_shader_sort(KernelGlobals *kg,
                                   ccl_local_param ShaderSortLocals *locals)
{
#ifndef __KERNEL_CUDA__
	int tid = ccl_global_id(1) * ccl_global_size(0) + ccl_global_id(0);
	uint qsize = kernel_split_params.queue_index[QUEUE_ACTIVE_AND_REGENERATED_RAYS];
	if(tid == 0) {
		kernel_split_params.queue_index[QUEUE_SHADER_SORTED_RAYS] = qsize;
	}

	uint offset = (tid/SHADER_SORT_LOCAL_SIZE)*SHADER_SORT_BLOCK_SIZE;
	if(offset >= qsize) {
		return;
	}

	int lid = ccl_local_id(1) * ccl_local_size(0) + ccl_local_id(0);
	uint input = QUEUE_ACTIVE_AND_REGENERATED_RAYS * (kernel_split_params.queue_size);
	uint output = QUEUE_SHADER_SORTED_RAYS * (kernel_split_params.queue_size);
	ccl_local uint *local_value = &locals->local_value[0];
	ccl_local ushort *local_index = &locals->local_index[0];

	/* copy to local memory */
	for(uint i = 0; i < SHADER_SORT_BLOCK_SIZE; i += SHADER_SORT_LOCAL_SIZE) {
		uint idx = offset + i + lid;
		uint add = input + idx;
		uint value = (~0);
		if(idx < qsize) {
			int ray_index = kernel_split_state.queue_data[add];
			bool valid = (ray_index != QUEUE_EMPTY_SLOT) && IS_STATE(kernel_split_state.ray_state, ray_index, RAY_ACTIVE);
			if(valid) {
				value = kernel_split_sd(sd, ray_index)->shader & SHADER_MASK;
			}
		}
		local_value[i + lid] = value;
		local_index[i + lid] = i + lid;
	}
	ccl_barrier(CCL_LOCAL_MEM_FENCE);

	/* skip sorting for cpu split kernel */
#  ifdef __KERNEL_OPENCL__

	/* bitonic sort */
	for(uint length = 1; length < SHADER_SORT_BLOCK_SIZE; length <<= 1) {
		for(uint inc = length; inc > 0; inc >>= 1) {
			for(uint ii = 0; ii < SHADER_SORT_BLOCK_SIZE; ii += SHADER_SORT_LOCAL_SIZE) {
				uint i = lid + ii;
				bool direction = ((i & (length << 1)) != 0);
				uint j = i ^ inc;
				ushort ioff = local_index[i];
				ushort joff = local_index[j];
				uint iKey = local_value[ioff];
				uint jKey = local_value[joff];
				bool smaller = (jKey < iKey) || (jKey == iKey && j < i);
				bool swap = smaller ^ (j < i) ^ direction;
				ccl_barrier(CCL_LOCAL_MEM_FENCE);
				local_index[i] = (swap) ? joff : ioff;
				local_index[j] = (swap) ? ioff : joff;
				ccl_barrier(CCL_LOCAL_MEM_FENCE);
			}
		}
	}
#  endif /* __KERNEL_OPENCL__ */

	/* copy to destination */
	for(uint i = 0; i < SHADER_SORT_BLOCK_SIZE; i += SHADER_SORT_LOCAL_SIZE) {
		uint idx = offset + i + lid;
		uint lidx = local_index[i + lid];
		uint outi = output + idx;
		uint ini = input + offset + lidx;
		uint value = local_value[lidx];
		if(idx < qsize) {
			kernel_split_state.queue_data[outi] = (value == (~0)) ? QUEUE_EMPTY_SLOT : kernel_split_state.queue_data[ini];
		}
	}
#endif /* __KERNEL_CUDA__ */
}

CCL_NAMESPACE_END
