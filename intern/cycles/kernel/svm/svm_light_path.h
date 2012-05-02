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

CCL_NAMESPACE_END

