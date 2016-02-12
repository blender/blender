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

/*
 * Utility functions for work stealing
 */

#ifdef __WORK_STEALING__

#ifdef __KERNEL_OPENCL__
#  pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable
#endif

uint get_group_id_with_ray_index(uint ray_index,
                                 uint tile_dim_x,
                                 uint tile_dim_y,
                                 uint parallel_samples,
                                 int dim)
{
	if(dim == 0) {
		uint x_span = ray_index % (tile_dim_x * parallel_samples);
		return x_span / get_local_size(0);
	}
	else /*if(dim == 1)*/ {
		kernel_assert(dim == 1);
		uint y_span = ray_index / (tile_dim_x * parallel_samples);
		return y_span / get_local_size(1);
	}
}

uint get_total_work(uint tile_dim_x,
                    uint tile_dim_y,
                    uint grp_idx,
                    uint grp_idy,
                    uint num_samples)
{
	uint threads_within_tile_border_x =
		(grp_idx == (get_num_groups(0) - 1)) ? tile_dim_x % get_local_size(0)
		                                     : get_local_size(0);
	uint threads_within_tile_border_y =
		(grp_idy == (get_num_groups(1) - 1)) ? tile_dim_y % get_local_size(1)
		                                     : get_local_size(1);

	threads_within_tile_border_x =
		(threads_within_tile_border_x == 0) ? get_local_size(0)
		                                    : threads_within_tile_border_x;
	threads_within_tile_border_y =
		(threads_within_tile_border_y == 0) ? get_local_size(1)
		                                    : threads_within_tile_border_y;

	return threads_within_tile_border_x *
	       threads_within_tile_border_y *
	       num_samples;
}

/* Returns 0 in case there is no next work available */
/* Returns 1 in case work assigned is valid */
int get_next_work(ccl_global uint *work_pool,
                  ccl_private uint *my_work,
                  uint tile_dim_x,
                  uint tile_dim_y,
                  uint num_samples,
                  uint parallel_samples,
                  uint ray_index)
{
	uint grp_idx = get_group_id_with_ray_index(ray_index,
	                                           tile_dim_x,
	                                           tile_dim_y,
	                                           parallel_samples,
	                                           0);
	uint grp_idy = get_group_id_with_ray_index(ray_index,
	                                           tile_dim_x,
	                                           tile_dim_y,
	                                           parallel_samples,
	                                           1);
	uint total_work = get_total_work(tile_dim_x,
	                                 tile_dim_y,
	                                 grp_idx,
	                                 grp_idy,
	                                 num_samples);
	uint group_index = grp_idy * get_num_groups(0) + grp_idx;
	*my_work = atomic_inc(&work_pool[group_index]);
	return (*my_work < total_work) ? 1 : 0;
}

/* This function assumes that the passed my_work is valid. */
/* Decode sample number w.r.t. assigned my_work. */
uint get_my_sample(uint my_work,
                   uint tile_dim_x,
                   uint tile_dim_y,
                   uint parallel_samples,
                   uint ray_index)
{
	uint grp_idx = get_group_id_with_ray_index(ray_index,
	                                           tile_dim_x,
	                                           tile_dim_y,
	                                           parallel_samples,
	                                           0);
	uint grp_idy = get_group_id_with_ray_index(ray_index,
	                                           tile_dim_x,
	                                           tile_dim_y,
	                                           parallel_samples,
	                                           1);
	uint threads_within_tile_border_x =
		(grp_idx == (get_num_groups(0) - 1)) ? tile_dim_x % get_local_size(0)
		                                     : get_local_size(0);
	uint threads_within_tile_border_y =
		(grp_idy == (get_num_groups(1) - 1)) ? tile_dim_y % get_local_size(1)
		                                     : get_local_size(1);

	threads_within_tile_border_x =
		(threads_within_tile_border_x == 0) ? get_local_size(0)
		                                    : threads_within_tile_border_x;
	threads_within_tile_border_y =
		(threads_within_tile_border_y == 0) ? get_local_size(1)
		                                    : threads_within_tile_border_y;

	return my_work /
	       (threads_within_tile_border_x * threads_within_tile_border_y);
}

/* Decode pixel and tile position w.r.t. assigned my_work. */
void get_pixel_tile_position(ccl_private uint *pixel_x,
                             ccl_private uint *pixel_y,
                             ccl_private uint *tile_x,
                             ccl_private uint *tile_y,
                             uint my_work,
                             uint tile_dim_x,
                             uint tile_dim_y,
                             uint tile_offset_x,
                             uint tile_offset_y,
                             uint parallel_samples,
                             uint ray_index)
{
	uint grp_idx = get_group_id_with_ray_index(ray_index,
	                                           tile_dim_x,
	                                           tile_dim_y,
	                                           parallel_samples,
	                                           0);
	uint grp_idy = get_group_id_with_ray_index(ray_index,
	                                           tile_dim_x,
	                                           tile_dim_y,
	                                           parallel_samples,
	                                           1);
	uint threads_within_tile_border_x =
		(grp_idx == (get_num_groups(0) - 1)) ? tile_dim_x % get_local_size(0)
		                                     : get_local_size(0);
	uint threads_within_tile_border_y =
		(grp_idy == (get_num_groups(1) - 1)) ? tile_dim_y % get_local_size(1)
		                                     : get_local_size(1);

	threads_within_tile_border_x =
		(threads_within_tile_border_x == 0) ? get_local_size(0)
		                                    : threads_within_tile_border_x;
	threads_within_tile_border_y =
		(threads_within_tile_border_y == 0) ? get_local_size(1)
		                                    : threads_within_tile_border_y;

	uint total_associated_pixels =
		threads_within_tile_border_x * threads_within_tile_border_y;
	uint work_group_pixel_index = my_work % total_associated_pixels;
	uint work_group_pixel_x =
		work_group_pixel_index % threads_within_tile_border_x;
	uint work_group_pixel_y =
		work_group_pixel_index / threads_within_tile_border_x;

	*pixel_x =
		tile_offset_x + (grp_idx * get_local_size(0)) + work_group_pixel_x;
	*pixel_y =
		tile_offset_y + (grp_idy * get_local_size(1)) + work_group_pixel_y;
	*tile_x = *pixel_x - tile_offset_x;
	*tile_y = *pixel_y - tile_offset_y;
}

#endif  /* __WORK_STEALING__ */

#endif  /* __KERNEL_WORK_STEALING_H__ */
