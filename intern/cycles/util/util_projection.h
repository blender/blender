/*
 * Copyright 2011-2018 Blender Foundation
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

#ifndef __UTIL_PROJECTION_H__
#define __UTIL_PROJECTION_H__

#include "util/util_transform.h"

CCL_NAMESPACE_BEGIN

/* 4x4 projection matrix, perspective or orthographic. */

typedef struct ProjectionTransform {
	float4 x, y, z, w; /* rows */

#ifndef __KERNEL_GPU__
	ProjectionTransform()
	{
	}

	explicit ProjectionTransform(const Transform& tfm)
	: x(tfm.x),
	  y(tfm.y),
	  z(tfm.z),
	  w(make_float4(0.0f, 0.0f, 0.0f, 1.0f))
	{
	}
#endif
} ProjectionTransform;

typedef struct PerspectiveMotionTransform {
	ProjectionTransform pre;
	ProjectionTransform post;
} PerspectiveMotionTransform;

/* Functions */

ccl_device_inline float3 transform_perspective(const ProjectionTransform *t, const float3 a)
{
	float4 b = make_float4(a.x, a.y, a.z, 1.0f);
	float3 c = make_float3(dot(t->x, b), dot(t->y, b), dot(t->z, b));
	float w = dot(t->w, b);

	return (w != 0.0f)? c/w: make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline float3 transform_perspective_direction(const ProjectionTransform *t, const float3 a)
{
	float3 c = make_float3(
		a.x*t->x.x + a.y*t->x.y + a.z*t->x.z,
		a.x*t->y.x + a.y*t->y.y + a.z*t->y.z,
		a.x*t->z.x + a.y*t->z.y + a.z*t->z.z);

	return c;
}

#ifndef __KERNEL_GPU__

ccl_device_inline Transform projection_to_transform(const ProjectionTransform& a)
{
	Transform tfm = {a.x, a.y, a.z};
	return tfm;
}

ccl_device_inline ProjectionTransform projection_transpose(const ProjectionTransform& a)
{
	ProjectionTransform t;

	t.x.x = a.x.x; t.x.y = a.y.x; t.x.z = a.z.x; t.x.w = a.w.x;
	t.y.x = a.x.y; t.y.y = a.y.y; t.y.z = a.z.y; t.y.w = a.w.y;
	t.z.x = a.x.z; t.z.y = a.y.z; t.z.z = a.z.z; t.z.w = a.w.z;
	t.w.x = a.x.w; t.w.y = a.y.w; t.w.z = a.z.w; t.w.w = a.w.w;

	return t;
}

ProjectionTransform projection_inverse(const ProjectionTransform& a);

ccl_device_inline ProjectionTransform make_projection(
	float a, float b, float c, float d,
	float e, float f, float g, float h,
	float i, float j, float k, float l,
	float m, float n, float o, float p)
{
	ProjectionTransform t;

	t.x.x = a; t.x.y = b; t.x.z = c; t.x.w = d;
	t.y.x = e; t.y.y = f; t.y.z = g; t.y.w = h;
	t.z.x = i; t.z.y = j; t.z.z = k; t.z.w = l;
	t.w.x = m; t.w.y = n; t.w.z = o; t.w.w = p;

	return t;
}
ccl_device_inline ProjectionTransform projection_identity()
{
	return make_projection(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);
}

ccl_device_inline ProjectionTransform operator*(const ProjectionTransform& a, const ProjectionTransform& b)
{
	ProjectionTransform c = projection_transpose(b);
	ProjectionTransform t;

	t.x = make_float4(dot(a.x, c.x), dot(a.x, c.y), dot(a.x, c.z), dot(a.x, c.w));
	t.y = make_float4(dot(a.y, c.x), dot(a.y, c.y), dot(a.y, c.z), dot(a.y, c.w));
	t.z = make_float4(dot(a.z, c.x), dot(a.z, c.y), dot(a.z, c.z), dot(a.z, c.w));
	t.w = make_float4(dot(a.w, c.x), dot(a.w, c.y), dot(a.w, c.z), dot(a.w, c.w));

	return t;
}

ccl_device_inline ProjectionTransform operator*(const ProjectionTransform& a, const Transform& b)
{
	return a * ProjectionTransform(b);
}

ccl_device_inline ProjectionTransform operator*(const Transform& a, const ProjectionTransform& b)
{
	return ProjectionTransform(a) * b;
}

ccl_device_inline void print_projection(const char *label, const ProjectionTransform& t)
{
	print_float4(label, t.x);
	print_float4(label, t.y);
	print_float4(label, t.z);
	print_float4(label, t.w);
	printf("\n");
}

ccl_device_inline ProjectionTransform projection_perspective(float fov, float n, float f)
{
	ProjectionTransform persp = make_projection(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, f / (f - n), -f*n / (f - n),
		0, 0, 1, 0);

	float inv_angle = 1.0f/tanf(0.5f*fov);

	Transform scale = transform_scale(inv_angle, inv_angle, 1);

	return scale * persp;
}

ccl_device_inline ProjectionTransform projection_orthographic(float znear, float zfar)
{
	Transform t =
		transform_scale(1.0f, 1.0f, 1.0f / (zfar-znear)) *
		transform_translate(0.0f, 0.0f, -znear);

	return ProjectionTransform(t);
}

#endif /* __KERNEL_GPU__ */

CCL_NAMESPACE_END

#endif /* __UTIL_PROJECTION_H__ */
