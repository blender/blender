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

/* Gradient */

__device float svm_gradient(float3 p, NodeGradientType type)
{
	float x, y, z;

	x= p.x;
	y= p.y;
	z= p.z;

	if(type == NODE_BLEND_LINEAR) {
		return x;
	}
	else if(type == NODE_BLEND_QUADRATIC) {
		float r = fmaxf(x, 0.0f);
		return r*r;
	}
	else if(type == NODE_BLEND_EASING) {
		float r = fminf(fmaxf(x, 0.0f), 1.0f);
		float t = r*r;
		
		return (3.0f*t - 2.0f*t*r);
	}
	else if(type == NODE_BLEND_DIAGONAL) {
		return (x + y)/2.0f;
	}
	else if(type == NODE_BLEND_RADIAL) {
		return atan2(y, x)/(2.0f*M_PI_F) + 0.5f;
	}
	else {
		float r = fmaxf(1.0f - sqrtf(x*x + y*y + z*z), 0.0f);

		if(type == NODE_BLEND_QUADRATIC_SPHERE)
			return r*r;
		else if(type == NODE_BLEND_SPHERICAL)
			return r;
	}

	return 0.0f;
}

__device void svm_node_tex_gradient(ShaderData *sd, float *stack, uint4 node)
{
	uint type, co_offset, color_offset, fac_offset;

	decode_node_uchar4(node.y, &type, &co_offset, &fac_offset, &color_offset);

	float3 co = stack_load_float3(stack, co_offset);

	float f = svm_gradient(co, (NodeGradientType)type);
	f = clamp(f, 0.0f, 1.0f);

	if(stack_valid(fac_offset))
		stack_store_float(stack, fac_offset, f);
	if(stack_valid(color_offset))
		stack_store_float3(stack, color_offset, make_float3(f, f, f));
}

CCL_NAMESPACE_END

