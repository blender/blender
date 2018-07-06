/*
 * Copyright 2011-2016 Blender Foundation
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

/* Bump Eval Nodes */

ccl_device void svm_node_enter_bump_eval(KernelGlobals *kg, ShaderData *sd, float *stack, uint offset)
{
	/* save state */
	stack_store_float3(stack, offset+0, sd->P);
	stack_store_float3(stack, offset+3, sd->dP.dx);
	stack_store_float3(stack, offset+6, sd->dP.dy);

	/* set state as if undisplaced */
	const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_POSITION_UNDISPLACED);

	if(desc.offset != ATTR_STD_NOT_FOUND) {
		float3 P, dPdx, dPdy;
		P = primitive_attribute_float3(kg, sd, desc, &dPdx, &dPdy);

		object_position_transform(kg, sd, &P);
		object_dir_transform(kg, sd, &dPdx);
		object_dir_transform(kg, sd, &dPdy);

		sd->P = P;
		sd->dP.dx = dPdx;
		sd->dP.dy = dPdy;
	}
}

ccl_device void svm_node_leave_bump_eval(KernelGlobals *kg, ShaderData *sd, float *stack, uint offset)
{
	/* restore state */
	sd->P = stack_load_float3(stack, offset+0);
	sd->dP.dx = stack_load_float3(stack, offset+3);
	sd->dP.dy = stack_load_float3(stack, offset+6);
}

CCL_NAMESPACE_END
