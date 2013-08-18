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

