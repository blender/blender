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
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Conversion Nodes */

ccl_device void svm_node_convert(ShaderData *sd, float *stack, uint type, uint from, uint to)
{
	switch(type) {
		case NODE_CONVERT_FI: {
			float f = stack_load_float(stack, from);
			stack_store_int(stack, to, float_to_int(f));
			break;
		}
		case NODE_CONVERT_FV: {
			float f = stack_load_float(stack, from);
			stack_store_float3(stack, to, make_float3(f, f, f));
			break;
		}
		case NODE_CONVERT_CF: {
			float3 f = stack_load_float3(stack, from);
			float g = linear_rgb_to_gray(f);
			stack_store_float(stack, to, g);
			break;
		}
		case NODE_CONVERT_CI: {
			float3 f = stack_load_float3(stack, from);
			int i = (int)linear_rgb_to_gray(f);
			stack_store_int(stack, to, i);
			break;
		}
		case NODE_CONVERT_VF: {
			float3 f = stack_load_float3(stack, from);
			float g = average(f);
			stack_store_float(stack, to, g);
			break;
		}
		case NODE_CONVERT_VI: {
			float3 f = stack_load_float3(stack, from);
			int i = (int)average(f);
			stack_store_int(stack, to, i);
			break;
		}
		case NODE_CONVERT_IF: {
			float f = (float)stack_load_int(stack, from);
			stack_store_float(stack, to, f);
			break;
		}
		case NODE_CONVERT_IV: {
			float f = (float)stack_load_int(stack, from);
			stack_store_float3(stack, to, make_float3(f, f, f));
			break;
		}
	}
}

CCL_NAMESPACE_END

