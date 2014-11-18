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

/* Primitive Utilities
 *
 * Generic functions to look up mesh, curve and volume primitive attributes for
 * shading and render passes. */

CCL_NAMESPACE_BEGIN

/* Generic primitive attribute reading functions */

ccl_device float primitive_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
{
	if(sd->type & PRIMITIVE_ALL_TRIANGLE) {
		return triangle_attribute_float(kg, sd, elem, offset, dx, dy);
	}
#ifdef __HAIR__
	else if(sd->type & PRIMITIVE_ALL_CURVE) {
		return curve_attribute_float(kg, sd, elem, offset, dx, dy);
	}
#endif
#ifdef __VOLUME__
	else if(sd->object != OBJECT_NONE && elem == ATTR_ELEMENT_VOXEL) {
		return volume_attribute_float(kg, sd, elem, offset, dx, dy);
	}
#endif
	else {
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;
		return 0.0f;
	}
}

ccl_device float3 primitive_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float3 *dx, float3 *dy)
{
	if(sd->type & PRIMITIVE_ALL_TRIANGLE) {
		return triangle_attribute_float3(kg, sd, elem, offset, dx, dy);
	}
#ifdef __HAIR__
	else if(sd->type & PRIMITIVE_ALL_CURVE) {
		return curve_attribute_float3(kg, sd, elem, offset, dx, dy);
	}
#endif
#ifdef __VOLUME__
	else if(sd->object != OBJECT_NONE && elem == ATTR_ELEMENT_VOXEL) {
		return volume_attribute_float3(kg, sd, elem, offset, dx, dy);
	}
#endif
	else {
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

/* Default UV coordinate */

ccl_device float3 primitive_uv(KernelGlobals *kg, ShaderData *sd)
{
	AttributeElement elem_uv;
	int offset_uv = find_attribute(kg, sd, ATTR_STD_UV, &elem_uv);

	if(offset_uv == ATTR_STD_NOT_FOUND)
		return make_float3(0.0f, 0.0f, 0.0f);

	float3 uv = primitive_attribute_float3(kg, sd, elem_uv, offset_uv, NULL, NULL);
	uv.z = 1.0f;
	return uv;
}

/* Ptex coordinates */

ccl_device bool primitive_ptex(KernelGlobals *kg, ShaderData *sd, float2 *uv, int *face_id)
{
	/* storing ptex data as attributes is not memory efficient but simple for tests */
	AttributeElement elem_face_id, elem_uv;
	int offset_face_id = find_attribute(kg, sd, ATTR_STD_PTEX_FACE_ID, &elem_face_id);
	int offset_uv = find_attribute(kg, sd, ATTR_STD_PTEX_UV, &elem_uv);

	if(offset_face_id == ATTR_STD_NOT_FOUND || offset_uv == ATTR_STD_NOT_FOUND)
		return false;

	float3 uv3 = primitive_attribute_float3(kg, sd, elem_uv, offset_uv, NULL, NULL);
	float face_id_f = primitive_attribute_float(kg, sd, elem_face_id, offset_face_id, NULL, NULL);

	*uv = make_float2(uv3.x, uv3.y);
	*face_id = (int)face_id_f;

	return true;
}

/* Surface tangent */

ccl_device float3 primitive_tangent(KernelGlobals *kg, ShaderData *sd)
{
#ifdef __HAIR__
	if(sd->type & PRIMITIVE_ALL_CURVE)
#ifdef __DPDU__
		return normalize(sd->dPdu);
#else
		return make_float3(0.0f, 0.0f, 0.0f);
#endif
#endif

	/* try to create spherical tangent from generated coordinates */
	AttributeElement attr_elem;
	int attr_offset = find_attribute(kg, sd, ATTR_STD_GENERATED, &attr_elem);

	if(attr_offset != ATTR_STD_NOT_FOUND) {
		float3 data = primitive_attribute_float3(kg, sd, attr_elem, attr_offset, NULL, NULL);
		data = make_float3(-(data.y - 0.5f), (data.x - 0.5f), 0.0f);
		object_normal_transform(kg, sd, &data);
		return cross(sd->N, normalize(cross(data, sd->N)));
	}
	else {
		/* otherwise use surface derivatives */
#ifdef __DPDU__
		return normalize(sd->dPdu);
#else
		return make_float3(0.0f, 0.0f, 0.0f);
#endif
	}
}

/* Motion vector for motion pass */

ccl_device float4 primitive_motion_vector(KernelGlobals *kg, ShaderData *sd)
{
	/* center position */
	float3 center;

#ifdef __HAIR__
	bool is_curve_primitive = sd->type & PRIMITIVE_ALL_CURVE;
	if(is_curve_primitive) {
		center = curve_motion_center_location(kg, sd);

		if(!(sd->flag & SD_TRANSFORM_APPLIED))
			object_position_transform(kg, sd, &center);
	}
	else
#endif
		center = sd->P;

	float3 motion_pre = center, motion_post = center;

	/* deformation motion */
	AttributeElement elem;
	int offset = find_attribute(kg, sd, ATTR_STD_MOTION_VERTEX_POSITION, &elem);

	if(offset != ATTR_STD_NOT_FOUND) {
		/* get motion info */
		int numverts, numkeys;
		object_motion_info(kg, sd->object, NULL, &numverts, &numkeys);

		/* lookup attributes */
		int offset_next = (sd->type & PRIMITIVE_ALL_TRIANGLE)? offset + numverts: offset + numkeys;

		motion_pre = primitive_attribute_float3(kg, sd, elem, offset, NULL, NULL);
		motion_post = primitive_attribute_float3(kg, sd, elem, offset_next, NULL, NULL);

#ifdef __HAIR__
		if(is_curve_primitive) {
			object_position_transform(kg, sd, &motion_pre);
			object_position_transform(kg, sd, &motion_post);
		}
#endif
	}

	/* object motion. note that depending on the mesh having motion vectors, this
	 * transformation was set match the world/object space of motion_pre/post */
	Transform tfm;
	
	tfm = object_fetch_vector_transform(kg, sd->object, OBJECT_VECTOR_MOTION_PRE);
	motion_pre = transform_point(&tfm, motion_pre);

	tfm = object_fetch_vector_transform(kg, sd->object, OBJECT_VECTOR_MOTION_POST);
	motion_post = transform_point(&tfm, motion_post);

	float3 motion_center;

	/* camera motion, for perspective/orthographic motion.pre/post will be a
	 * world-to-raster matrix, for panorama it's world-to-camera */
	if (kernel_data.cam.type != CAMERA_PANORAMA) {
		tfm = kernel_data.cam.worldtoraster;
		motion_center = transform_perspective(&tfm, center);

		tfm = kernel_data.cam.motion.pre;
		motion_pre = transform_perspective(&tfm, motion_pre);

		tfm = kernel_data.cam.motion.post;
		motion_post = transform_perspective(&tfm, motion_post);
	}
	else {
		tfm = kernel_data.cam.worldtocamera;
		motion_center = normalize(transform_point(&tfm, center));
		motion_center = float2_to_float3(direction_to_panorama(kg, motion_center));
		motion_center.x *= kernel_data.cam.width;
		motion_center.y *= kernel_data.cam.height;

		tfm = kernel_data.cam.motion.pre;
		motion_pre = normalize(transform_point(&tfm, motion_pre));
		motion_pre = float2_to_float3(direction_to_panorama(kg, motion_pre));
		motion_pre.x *= kernel_data.cam.width;
		motion_pre.y *= kernel_data.cam.height;

		tfm = kernel_data.cam.motion.post;
		motion_post = normalize(transform_point(&tfm, motion_post));
		motion_post = float2_to_float3(direction_to_panorama(kg, motion_post));
		motion_post.x *= kernel_data.cam.width;
		motion_post.y *= kernel_data.cam.height;
	}

	motion_pre = motion_pre - motion_center;
	motion_post = motion_center - motion_post;

	return make_float4(motion_pre.x, motion_pre.y, motion_post.x, motion_post.y);
}

CCL_NAMESPACE_END

