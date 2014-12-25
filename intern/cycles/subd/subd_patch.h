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

#ifndef __SUBD_PATCH_H__
#define __SUBD_PATCH_H__

#include "util_boundbox.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Patch {
public:
	virtual ~Patch() {}
	virtual void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v) = 0;
	virtual bool is_triangle() { return false; }
	virtual BoundBox bound() = 0;
	virtual int ptex_face_id() { return -1; }
};

/* Linear Quad Patch */

class LinearQuadPatch : public Patch {
public:
	float3 hull[4];

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v);
	bool is_triangle() { return false; }
	BoundBox bound();
};

/* Linear Triangle Patch */

class LinearTrianglePatch : public Patch {
public:
	float3 hull[3];

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v);
	bool is_triangle() { return true; }
	BoundBox bound();
};

/* Bicubic Patch */

class BicubicPatch : public Patch {
public:
	float3 hull[16];

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v);
	bool is_triangle() { return false; }
	BoundBox bound();
};

CCL_NAMESPACE_END

#endif /* __SUBD_PATCH_H__ */

