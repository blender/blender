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

/* Blend */

__device float svm_blend(float3 p, NodeBlendType type, NodeBlendAxis axis)
{
	float x, y;

	if(axis == NODE_BLEND_VERTICAL) {
		x= p.y;
		y= p.x;
	}
	else {
		x= p.x;
		y= p.y;
	}

	if(type == NODE_BLEND_LINEAR) {
		return (1.0f + x)/2.0f;
	}
	else if(type == NODE_BLEND_QUADRATIC) {
		float r = fmaxf((1.0f + x)/2.0f, 0.0f);
		return r*r;
	}
	else if(type == NODE_BLEND_EASING) {
		float r = min(fmaxf((1.0f + x)/2.0f, 0.0f), 1.0f);
		float t = r*r;
		
		return (3.0f*t - 2.0f*t*r);
	}
	else if(type == NODE_BLEND_DIAGONAL) {
		return (2.0f + x + y)/4.0f;
	}
	else if(type == NODE_BLEND_RADIAL) {
		return atan2(y, x)/(2.0f*M_PI_F) + 0.5f;
	}
	else {
		float r = fmaxf(1.0f - sqrtf(x*x + y*y + p.z*p.z), 0.0f);

		if(type == NODE_BLEND_QUADRATIC_SPHERE)
			return r*r;
		else if(type == NODE_BLEND_SPHERICAL)
			return r;
	}

	return 0.0f;
}

__device void svm_node_tex_blend(ShaderData *sd, float *stack, uint4 node)
{
	float3 co = stack_load_float3(stack, node.z);
	uint type, axis;

	decode_node_uchar4(node.y, &type, &axis, NULL, NULL);

	float f = svm_blend(co, (NodeBlendType)type, (NodeBlendAxis)axis);
	stack_store_float(stack, node.w, f);
}

CCL_NAMESPACE_END

