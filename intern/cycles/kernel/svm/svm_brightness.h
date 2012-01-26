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

__device void svm_node_brightness(ShaderData *sd, float *stack, uint in_color, uint out_color, uint node)
{
	uint bright_offset, contrast_offset;
	float3 color = stack_load_float3(stack, in_color);

	decode_node_uchar4(node, &bright_offset, &contrast_offset, NULL, NULL);
	float brightness = stack_load_float(stack, bright_offset);
	float contrast  = stack_load_float(stack, contrast_offset);

	brightness *= 1.0f/100.0f;
	float delta = contrast * (1.0f/200.0f);
	float a = 1.0f - delta * 2.0f;
	float b;

	/*
	* The algorithm is by Werner D. Streidt
	* (http://visca.com/ffactory/archives/5-99/msg00021.html)
	* Extracted of OpenCV demhist.c
	*/
	if (contrast > 0.0f) {
		a = (a > 0.0f? (1.0f / a): 0.0f);
		b = a * (brightness - delta);
	}
	else {
		delta *= -1.0f;
		b = a * (brightness + delta);
	}

	color.x = a*color.x + b;
	color.y = a*color.y + b;
	color.z = a*color.z + b;

	if (stack_valid(out_color))
		stack_store_float3(stack, out_color, color);
}

CCL_NAMESPACE_END
