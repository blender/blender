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

ccl_device void svm_node_camera(KernelGlobals *kg, ShaderData *sd, float *stack, uint out_vector, uint out_zdepth, uint out_distance)
{
	float distance;
	float zdepth;
	float3 vector;

	Transform tfm = kernel_data.cam.worldtocamera;
	vector = transform_point(&tfm, sd->P);
	zdepth = vector.z;
	distance = len(vector);

	if(stack_valid(out_vector))
		stack_store_float3(stack, out_vector, normalize(vector));

	if(stack_valid(out_zdepth))
		stack_store_float(stack, out_zdepth, zdepth);

	if(stack_valid(out_distance))
		stack_store_float(stack, out_distance, distance);
}

CCL_NAMESPACE_END
