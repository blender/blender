/*
 * Copyright 2011-2014 Blender Foundation
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

ccl_device_inline void debug_data_init(DebugData *debug_data)
{
	debug_data->num_bvh_traversed_nodes = 0;
	debug_data->num_bvh_traversed_instances = 0;
	debug_data->num_bvh_intersections = 0;
	debug_data->num_ray_bounces = 0;
}

ccl_device_inline void kernel_write_debug_passes(KernelGlobals *kg,
                                                 ccl_global float *buffer,
                                                 ccl_addr_space PathState *state,
                                                 DebugData *debug_data,
                                                 int sample)
{
	int flag = kernel_data.film.pass_flag;
	if(flag & PASS_BVH_TRAVERSED_NODES) {
		kernel_write_pass_float(buffer + kernel_data.film.pass_bvh_traversed_nodes,
		                        sample,
		                        debug_data->num_bvh_traversed_nodes);
	}
	if(flag & PASS_BVH_TRAVERSED_INSTANCES) {
		kernel_write_pass_float(buffer + kernel_data.film.pass_bvh_traversed_instances,
		                        sample,
		                        debug_data->num_bvh_traversed_instances);
	}
	if(flag & PASS_BVH_INTERSECTIONS) {
		kernel_write_pass_float(buffer + kernel_data.film.pass_bvh_intersections,
		                        sample,
		                        debug_data->num_bvh_intersections);
	}
	if(flag & PASS_RAY_BOUNCES) {
		kernel_write_pass_float(buffer + kernel_data.film.pass_ray_bounces,
		                        sample,
		                        debug_data->num_ray_bounces);
	}
}

CCL_NAMESPACE_END
