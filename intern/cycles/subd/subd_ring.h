/*
 * Copyright 2006, NVIDIA Corporation Ignacio Castano <icastano@nvidia.com>
 *
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __SUBD_FACE_RING_H__
#define __SUBD_FACE_RING_H__

CCL_NAMESPACE_BEGIN

class StencilMask;
class SubdVert;
class SubdEdge;
class SubdFace;

class SubdFaceRing
{
public:
	SubdFaceRing(SubdFace *face, SubdEdge *edge);

	SubdFace *face() { return m_face; }
	SubdEdge *firstEdge() { return m_firstEdge; }

	int num_verts();
	SubdVert *vertexAt(int i);
	int vert_index(SubdVert *vertex);

	void evaluate_stencils(float3 *P, StencilMask *mask, int num);

	bool is_triangle();
	bool is_quad();
	int num_edges();

	static bool is_regular(SubdFace *face);
	static bool is_triangle(SubdFace *face);
	static bool is_quad(SubdFace *face);
	static bool is_boundary(SubdFace *face);

protected:
	void initVerts();
	void add_vert(SubdVert *vertex);
	bool has_vert(SubdVert *vertex);

protected:
	SubdFace *m_face;
	SubdEdge *m_firstEdge;

	int m_num_edges;
	vector<SubdVert*> m_verts;
};

CCL_NAMESPACE_END

#endif /* __SUBD_FACE_RING_H__ */

