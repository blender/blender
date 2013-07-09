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

__device void svm_node_gamma(ShaderData *sd, float *stack, uint in_gamma, uint in_color, uint out_color)
{
	float3 color = stack_load_float3(stack, in_color);
	float gamma = stack_load_float(stack, in_gamma);

	if (color.x > 0.0f)
		color.x = powf(color.x, gamma);
	if (color.y > 0.0f)
		color.y = powf(color.y, gamma);
	if (color.z > 0.0f)
		color.z = powf(color.z, gamma);

	if (stack_valid(out_color))
		stack_store_float3(stack, out_color, color);
}

CCL_NAMESPACE_END
