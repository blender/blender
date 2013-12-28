/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

/* Light Path Node */

ccl_device void svm_node_light_path(ShaderData *sd, float *stack, uint type, uint out_offset, int path_flag)
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
		case NODE_LP_volume_scatter: info = (path_flag & PATH_RAY_VOLUME_SCATTER)? 1.0f: 0.0f; break;
		case NODE_LP_backfacing: info = (sd->flag & SD_BACKFACING)? 1.0f: 0.0f; break;
		case NODE_LP_ray_length: info = sd->ray_length; break;
		case NODE_LP_ray_depth: info = (float)sd->ray_depth; break;
	}

	stack_store_float(stack, out_offset, info);
}

/* Light Falloff Node */

ccl_device void svm_node_light_falloff(ShaderData *sd, float *stack, uint4 node)
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

