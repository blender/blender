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

__device float invert(float color, float factor)
{
	return factor*(1.0f - color) + (1.0f - factor) * color;
}

__device void svm_node_invert(ShaderData *sd, float *stack, uint in_fac, uint in_color, uint out_color)
{
	float factor = stack_load_float(stack, in_fac);
	float3 color = stack_load_float3(stack, in_color);

	color.x = invert(color.x, factor);
	color.y = invert(color.y, factor);
	color.z = invert(color.z, factor);

	if (stack_valid(out_color))
		stack_store_float3(stack, out_color, color);
}

CCL_NAMESPACE_END

