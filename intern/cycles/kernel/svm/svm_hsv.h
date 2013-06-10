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

#ifndef __SVM_HSV_H__
#define __SVM_HSV_H__

CCL_NAMESPACE_BEGIN

__device void svm_node_hsv(KernelGlobals *kg, ShaderData *sd, float *stack, uint in_color_offset, uint fac_offset, uint out_color_offset, int *offset)
{
	/* read extra data */
	uint4 node1 = read_node(kg, offset);

	float fac = stack_load_float(stack, fac_offset);
	float3 in_color = stack_load_float3(stack, in_color_offset);
	float3 color = in_color;

	float hue = stack_load_float(stack, node1.y);
	float sat = stack_load_float(stack, node1.z);
	float val = stack_load_float(stack, node1.w);

	color = rgb_to_hsv(color);

	/* remember: fmod doesn't work for negative numbers here */
	color.x += hue + 0.5f;
	color.x = fmodf(color.x, 1.0f);
	color.y *= sat;
	color.z *= val;

	color = hsv_to_rgb(color);

	color.x = fac*color.x + (1.0f - fac)*in_color.x;
	color.y = fac*color.y + (1.0f - fac)*in_color.y;
	color.z = fac*color.z + (1.0f - fac)*in_color.z;

	if (stack_valid(out_color_offset))
		stack_store_float3(stack, out_color_offset, color);
}

CCL_NAMESPACE_END

#endif /* __SVM_HSV_H__ */

