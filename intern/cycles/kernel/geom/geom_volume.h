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

/* Volume Primitive
 *
 * Volumes are just regions inside meshes with the mesh surface as boundaries.
 * There isn't as much data to access as for surfaces, there is only a position
 * to do lookups in 3D voxel or procedural textures.
 *
 * 3D voxel textures can be assigned as attributes per mesh, which means the
 * same shader can be used for volume objects with different densities, etc. */

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

/* Return position normalized to 0..1 in mesh bounds */

ccl_device float3 volume_normalized_position(KernelGlobals *kg, const ShaderData *sd, float3 P)
{
	/* todo: optimize this so it's just a single matrix multiplication when
	 * possible (not motion blur), or perhaps even just translation + scale */
	AttributeElement attr_elem;
	int attr_offset = find_attribute(kg, sd, ATTR_STD_GENERATED_TRANSFORM, &attr_elem);

	object_inverse_position_transform(kg, sd, &P);

	if(attr_offset != ATTR_STD_NOT_FOUND) {
		Transform tfm = primitive_attribute_matrix(kg, sd, attr_offset);
		P = transform_point(&tfm, P);
	}

	return P;
}

ccl_device float volume_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int id, float *dx, float *dy)
{
	float3 P = volume_normalized_position(kg, sd, sd->P);
#ifdef __KERNEL_GPU__
	float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
#else
	float4 r = kernel_tex_image_interp_3d(id, P.x, P.y, P.z);
#endif

	if(dx) *dx = 0.0f;
	if(dx) *dy = 0.0f;

	/* todo: support float textures to lower memory usage for single floats */
	return average(float4_to_float3(r));
}

ccl_device float3 volume_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int id, float3 *dx, float3 *dy)
{
	float3 P = volume_normalized_position(kg, sd, sd->P);
#ifdef __KERNEL_GPU__
	float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
#else
	float4 r = kernel_tex_image_interp_3d(id, P.x, P.y, P.z);
#endif

	if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
	if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);

	return float4_to_float3(r);
}

#endif

CCL_NAMESPACE_END

