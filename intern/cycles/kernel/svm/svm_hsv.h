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

__device float3 rgb_to_hsv(float3 rgb)
{
	float cmax, cmin, h, s, v, cdelta;
	float3 c;

	cmax = fmaxf(rgb.x, fmaxf(rgb.y, rgb.z));
	cmin = min(rgb.x, min(rgb.y, rgb.z));
	cdelta = cmax - cmin;

	v = cmax;

	if(cmax != 0.0f) {
		s = cdelta/cmax;
	}
	else {
		s = 0.0f;
		h = 0.0f;
	}

	if(s == 0.0f) {
		h = 0.0f;
	}
	else {
		float3 cmax3 = make_float3(cmax, cmax, cmax);
		c = (cmax3 - rgb)/cdelta;

		if(rgb.x == cmax) h = c.z - c.y;
		else if(rgb.y == cmax) h = 2.0f + c.x -  c.z;
		else h = 4.0f + c.y - c.x;

		h /= 6.0f;

		if(h < 0.0f)
			h += 1.0f;
	}

	return make_float3(h, s, v);
}

__device float3 hsv_to_rgb(float3 hsv)
{
	float i, f, p, q, t, h, s, v;
	float3 rgb;

	h = hsv.x;
	s = hsv.y;
	v = hsv.z;

	if(s==0.0f) {
		rgb = make_float3(v, v, v);
	}
	else {
		if(h==1.0f)
			h = 0.0f;
		
		h *= 6.0f;
		i = floor(h);
		f = h - i;
		rgb = make_float3(f, f, f);
		p = v*(1.0f-s);
		q = v*(1.0f-(s*f));
		t = v*(1.0f-(s*(1.0f-f)));
		
		if(i == 0.0f) rgb = make_float3(v, t, p);
		else if(i == 1.0f) rgb = make_float3(q, v, p);
		else if(i == 2.0f) rgb = make_float3(p, v, t);
		else if(i == 3.0f) rgb = make_float3(p, q, v);
		else if(i == 4.0f) rgb = make_float3(t, p, v);
		else rgb = make_float3(v, p, q);
	}

	return rgb;
}

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

	// remember: fmod doesn't work for negative numbers
	color.x += hue + 0.5f;
	color.x = fmod(color.x, 1.0f);
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

