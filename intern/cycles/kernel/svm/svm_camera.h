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

__device void svm_node_camera(KernelGlobals *kg, ShaderData *sd, float *stack, uint out_vector, uint out_zdepth, uint out_distance)
{
	float distance;
	float zdepth;
	float3 vector;

	Transform tfm = kernel_data.cam.worldtocamera;
	vector = transform(&tfm, sd->P);
	zdepth = vector.z;
	distance = len(vector);

	if (stack_valid(out_vector))
		stack_store_float3(stack, out_vector, normalize(vector));

	if (stack_valid(out_zdepth))
		stack_store_float(stack, out_zdepth, zdepth);

	if (stack_valid(out_distance))
		stack_store_float(stack, out_distance, distance);
}

CCL_NAMESPACE_END

