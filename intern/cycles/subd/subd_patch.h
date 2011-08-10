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

#ifndef __SUBD_PATCH_H__
#define __SUBD_PATCH_H__

#include "util_boundbox.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Mesh;

/* Base */

class Patch {
public:
	virtual ~Patch() {}
	virtual void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v) = 0;
	virtual bool is_triangle() = 0;
	virtual BoundBox bound() = 0;
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

/* Bicubic Patch with Tangent Fields */

class BicubicTangentPatch : public Patch {
public:
	float3 hull[16];
	float3 utan[12];
	float3 vtan[12];

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v);
	bool is_triangle() { return false; }
	BoundBox bound();
};

/* Gregory Patches */

class GregoryQuadPatch : public Patch {
public:
	float3 hull[20];

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v);
	bool is_triangle() { return false; }
	BoundBox bound();
};

class GregoryTrianglePatch : public Patch  {
public:
	float3 hull[20];

	void eval(float3 *P, float3 *dPdu, float3 *dPdv, float u, float v);
	bool is_triangle() { return true; }
	BoundBox bound();
};

CCL_NAMESPACE_END

#endif /* __SUBD_PATCH_H__ */

