/*
 * Copyright 2011-2015 Blender Foundation
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

#ifndef __KERNEL_WORK_STEALING_H__
#define __KERNEL_WORK_STEALING_H__

CCL_NAMESPACE_BEGIN

/*
 * Utility functions for work stealing
 */

#ifdef __KERNEL_OPENCL__
#  pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#endif

ccl_device_inline uint kernel_total_work_size(KernelGlobals *kg)
{
	return kernel_split_params.w * kernel_split_params.h * kernel_split_params.num_samples;
}

ccl_device_inline uint kernel_num_work_pools(KernelGlobals *kg)
{
	return ccl_global_size(0) * ccl_global_size(1) / WORK_POOL_SIZE;
}

ccl_device_inline uint work_pool_from_ray_index(KernelGlobals *kg, uint ray_index)
{
	return ray_index / WORK_POOL_SIZE;
}

ccl_device_inline uint work_pool_work_size(KernelGlobals *kg, uint work_pool)
{
	uint total_work_size = kernel_total_work_size(kg);
	uint num_pools = kernel_num_work_pools(kg);

	if(work_pool >= num_pools || work_pool * WORK_POOL_SIZE >= total_work_size) {
		return 0;
	}

	uint work_size = (total_work_size / (num_pools * WORK_POOL_SIZE)) * WORK_POOL_SIZE;

	uint remainder = (total_work_size % (num_pools * WORK_POOL_SIZE));
	if(work_pool < remainder / WORK_POOL_SIZE) {
		work_size += WORK_POOL_SIZE;
	}
	else if(work_pool == remainder / WORK_POOL_SIZE) {
		work_size += remainder % WORK_POOL_SIZE;
	}

	return work_size;
}

ccl_device_inline uint get_global_work_index(KernelGlobals *kg, uint work_index, uint ray_index)
{
	uint num_pools = kernel_num_work_pools(kg);
	uint pool = work_pool_from_ray_index(kg, ray_index);

	return (work_index / WORK_POOL_SIZE) * (num_pools * WORK_POOL_SIZE)
	       + (pool * WORK_POOL_SIZE)
	       + (work_index % WORK_POOL_SIZE);
}

/* Returns true if there is work */
ccl_device bool get_next_work(KernelGlobals *kg, ccl_private uint *work_index, uint ray_index)
{
	uint work_pool = work_pool_from_ray_index(kg, ray_index);
	uint pool_size = work_pool_work_size(kg, work_pool);

	if(pool_size == 0) {
		return false;
	}

	*work_index = atomic_fetch_and_inc_uint32(&kernel_split_params.work_pools[work_pool]);
	return (*work_index < pool_size);
}

/* This function assumes that the passed `work` is valid. */
/* Decode sample number w.r.t. assigned `work`. */
ccl_device uint get_work_sample(KernelGlobals *kg, uint work_index, uint ray_index)
{
	return get_global_work_index(kg, work_index, ray_index) / (kernel_split_params.w * kernel_split_params.h);
}

/* Decode pixel and tile position w.r.t. assigned `work`. */
ccl_device void get_work_pixel_tile_position(KernelGlobals *kg,
                             ccl_private uint *pixel_x,
                             ccl_private uint *pixel_y,
                             ccl_private uint *tile_x,
                             ccl_private uint *tile_y,
                             uint work_index,
                             uint ray_index)
{
	uint pixel_index = get_global_work_index(kg, work_index, ray_index) % (kernel_split_params.w*kernel_split_params.h);

	*tile_x = pixel_index % kernel_split_params.w;
	*tile_y = pixel_index / kernel_split_params.w;

	*pixel_x = *tile_x + kernel_split_params.x;
	*pixel_y = *tile_y + kernel_split_params.y;
}

CCL_NAMESPACE_END

#endif  /* __KERNEL_WORK_STEALING_H__ */
