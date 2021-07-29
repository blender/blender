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

#if defined(__KERNEL_CUDA__) && __CUDA_ARCH__ < 300
ccl_device float4 volume_image_texture_3d(int id, float x, float y, float z)
{
	float4 r;
	switch(id) {
		case 0: r = kernel_tex_image_interp_3d(__tex_image_float4_3d_000, x, y, z); break;
		case 1: r = kernel_tex_image_interp_3d(__tex_image_float4_3d_001, x, y, z); break;
		case 2: r = kernel_tex_image_interp_3d(__tex_image_float4_3d_002, x, y, z); break;
		case 3: r = kernel_tex_image_interp_3d(__tex_image_float4_3d_003, x, y, z); break;
		case 4: r = kernel_tex_image_interp_3d(__tex_image_float4_3d_004, x, y, z); break;
	}
	return r;
}
#endif  /* __KERNEL_CUDA__ */

ccl_device_inline float3 volume_normalized_position(KernelGlobals *kg,
                                                    const ShaderData *sd,
                                                    float3 P)
{
	/* todo: optimize this so it's just a single matrix multiplication when
	 * possible (not motion blur), or perhaps even just translation + scale */
	const AttributeDescriptor desc = find_attribute(kg, sd, ATTR_STD_GENERATED_TRANSFORM);

	object_inverse_position_transform(kg, sd, &P);

	if(desc.offset != ATTR_STD_NOT_FOUND) {
		Transform tfm = primitive_attribute_matrix(kg, sd, desc);
		P = transform_point(&tfm, P);
	}

	return P;
}

ccl_device float volume_attribute_float(KernelGlobals *kg, const ShaderData *sd, const AttributeDescriptor desc, float *dx, float *dy)
{
	float3 P = volume_normalized_position(kg, sd, sd->P);
#ifdef __KERNEL_CUDA__
#  if __CUDA_ARCH__ >= 300
	CUtexObject tex = kernel_tex_fetch(__bindless_mapping, desc.offset);
	float f = kernel_tex_image_interp_3d_float(tex, P.x, P.y, P.z);
	float4 r = make_float4(f, f, f, 1.0f);
#  else
	float4 r = volume_image_texture_3d(desc.offset, P.x, P.y, P.z);
#  endif
#elif defined(__KERNEL_OPENCL__)
	float4 r = kernel_tex_image_interp_3d(kg, desc.offset, P.x, P.y, P.z);
#else
	float4 r;
	if(sd->flag & SD_VOLUME_CUBIC)
		r = kernel_tex_image_interp_3d_ex(desc.offset, P.x, P.y, P.z, INTERPOLATION_CUBIC);
	else
		r = kernel_tex_image_interp_3d(desc.offset, P.x, P.y, P.z);
#endif

	if(dx) *dx = 0.0f;
	if(dy) *dy = 0.0f;

	return average(float4_to_float3(r));
}

ccl_device float3 volume_attribute_float3(KernelGlobals *kg, const ShaderData *sd, const AttributeDescriptor desc, float3 *dx, float3 *dy)
{
	float3 P = volume_normalized_position(kg, sd, sd->P);
#ifdef __KERNEL_CUDA__
#  if __CUDA_ARCH__ >= 300
	CUtexObject tex = kernel_tex_fetch(__bindless_mapping, desc.offset);
	float4 r = kernel_tex_image_interp_3d_float4(tex, P.x, P.y, P.z);
#  else
	float4 r = volume_image_texture_3d(desc.offset, P.x, P.y, P.z);
#  endif
#elif defined(__KERNEL_OPENCL__)
	float4 r = kernel_tex_image_interp_3d(kg, desc.offset, P.x, P.y, P.z);
#else
	float4 r;
	if(sd->flag & SD_VOLUME_CUBIC)
		r = kernel_tex_image_interp_3d_ex(desc.offset, P.x, P.y, P.z, INTERPOLATION_CUBIC);
	else
		r = kernel_tex_image_interp_3d(desc.offset, P.x, P.y, P.z);
#endif

	if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
	if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);

	return float4_to_float3(r);
}

#endif

CCL_NAMESPACE_END

