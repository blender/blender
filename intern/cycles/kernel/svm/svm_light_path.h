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

/* Light Path Node */

__device void svm_node_light_path(ShaderData *sd, float *stack, uint type, uint out_offset, int path_flag)
{
	float info = 0.0f;

	switch(type) {
		case NODE_LP_camera: info = (path_flag & PATH_RAY_CAMERA)? 1.0f: 0.0f; break;
		case NODE_LP_shadow: info = (path_flag & PATH_RAY_SHADOW)? 1.0f: 0.0f; break;
		case NODE_LP_diffuse: info = (path_flag & PATH_RAY_DIFFUSE)? 1.0f: 0.0f; break;
		case NODE_LP_glossy: info = (path_flag & PATH_RAY_GLOSSY)? 1.0f: 0.0f; break;
		case NODE_LP_singular: info = (path_flag & PATH_RAY_SINGULAR)? 1.0f: 0.0f; break;
		case NODE_LP_reflection: info = (path_flag & PATH_RAY_REFLECT)? 1.0f: 0.0f; break;
		case NODE_LP_transmission: info = (path_flag & PATH_RAY_TRANSMIT)? 1.0f: 0.0f; break;
		case NODE_LP_backfacing: info = (sd->flag & SD_BACKFACING)? 1.0f: 0.0f; break;
		case NODE_LP_ray_length: info = sd->ray_length; break;
	}

	stack_store_float(stack, out_offset, info);
}

/* Light Falloff Node */

__device void svm_node_light_falloff(ShaderData *sd, float *stack, uint4 node)
{
	uint strength_offset, out_offset, smooth_offset;

	decode_node_uchar4(node.z, &strength_offset, &smooth_offset, &out_offset, NULL);

	float strength = stack_load_float(stack, strength_offset);
	uint type = node.y;

	switch(type) {
		case NODE_LIGHT_FALLOFF_QUADRATIC: break;
		case NODE_LIGHT_FALLOFF_LINEAR: strength *= sd->ray_length; break;
		case NODE_LIGHT_FALLOFF_CONSTANT: strength *= sd->ray_length*sd->ray_length; break;
	}

	float smooth = stack_load_float(stack, smooth_offset);

	if(smooth > 0.0f) {
		float squared = sd->ray_length*sd->ray_length;
		strength *= squared/(smooth + squared);
	}

	stack_store_float(stack, out_offset, strength);
}

CCL_NAMESPACE_END

