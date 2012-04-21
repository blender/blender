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

enum ObjectTransform {
	OBJECT_TRANSFORM = 0,
	OBJECT_INVERSE_TRANSFORM = 4,
	OBJECT_NORMAL_TRANSFORM = 8,
	OBJECT_PROPERTIES = 12
};

__device_inline Transform object_fetch_transform(KernelGlobals *kg, int object, enum ObjectTransform type)
{
	Transform tfm;

	int offset = object*OBJECT_SIZE + (int)type;

	tfm.x = kernel_tex_fetch(__objects, offset + 0);
	tfm.y = kernel_tex_fetch(__objects, offset + 1);
	tfm.z = kernel_tex_fetch(__objects, offset + 2);
	tfm.w = kernel_tex_fetch(__objects, offset + 3);

	return tfm;
}

__device_inline void object_position_transform(KernelGlobals *kg, int object, float3 *P)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
	*P = transform_point(&tfm, *P);
}

__device_inline void object_normal_transform(KernelGlobals *kg, int object, float3 *N)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_NORMAL_TRANSFORM);
	*N = normalize(transform_direction(&tfm, *N));
}

__device_inline void object_dir_transform(KernelGlobals *kg, int object, float3 *D)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
	*D = transform_direction(&tfm, *D);
}

__device_inline float object_surface_area(KernelGlobals *kg, int object)
{
	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.x;
}

__device_inline float object_pass_id(KernelGlobals *kg, int object)
{
	if(object == ~0)
		return 0.0f;

	int offset = object*OBJECT_SIZE + OBJECT_PROPERTIES;
	float4 f = kernel_tex_fetch(__objects, offset);
	return f.y;
}

CCL_NAMESPACE_END

