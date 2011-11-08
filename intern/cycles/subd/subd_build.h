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

#ifndef __SUBD_BUILD_H__
#define __SUBD_BUILD_H__

CCL_NAMESPACE_BEGIN

class SubdFace;
class SubdFaceRing;
class SubdVert;
class GregoryAccStencil;
class Patch;
class StencilMask;

/* Builder */

class SubdBuilder
{
public:
	virtual ~SubdBuilder() {};
	virtual Patch *run(SubdFace *face) = 0;
	static SubdBuilder *create(bool linear);
};

/* Approximate Catmull Clark using Loop's approximation */

class SubdAccBuilder : public SubdBuilder
{
public:
	SubdAccBuilder();
	~SubdAccBuilder();

	Patch *run(SubdFace *face);

protected:
	/* Gregory Patch */
	void computeCornerStencil(SubdFaceRing *ring, GregoryAccStencil *stencil);
	void computeEdgeStencil(SubdFaceRing *ring, GregoryAccStencil *stencil);
	void computeInteriorStencil(SubdFaceRing *ring, GregoryAccStencil *stencil);
	void computeBoundaryTangentStencils(SubdFaceRing *ring,
		SubdVert *vert,
		StencilMask & r0, StencilMask & r1);
};

/* Linear Subdivision */

class SubdLinearBuilder : public SubdBuilder
{
public:
	SubdLinearBuilder();
	~SubdLinearBuilder();

	Patch *run(SubdFace *face);
};

CCL_NAMESPACE_END

#endif /* __SUBD_BUILD_H__ */

