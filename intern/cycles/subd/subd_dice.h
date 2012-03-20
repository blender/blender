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

#ifndef __SUBD_DICE_H__
#define __SUBD_DICE_H__

/* DX11 like EdgeDice implementation, with different tessellation factors for
 * each edge for watertight tessellation, with subpatch remapping to work with
 * DiagSplit. For more algorithm details, see the DiagSplit paper or the
 * ARB_tessellation_shader OpenGL extension, Section 2.X.2. */

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Camera;
class Mesh;
class Patch;

/* EdgeDice Base */

class EdgeDice {
public:
	Camera *camera;
	Mesh *mesh;
	float3 *mesh_P;
	float3 *mesh_N;
	float dicing_rate;
	size_t vert_offset;
	size_t tri_offset;
	int shader;
	bool smooth;

	EdgeDice(Mesh *mesh, int shader, bool smooth, float dicing_rate);

	void reserve(int num_verts, int num_tris);

	int add_vert(Patch *patch, float2 uv);
	void add_triangle(int v0, int v1, int v2);

	void stitch_triangles(vector<int>& outer, vector<int>& inner);
};

/* Quad EdgeDice
 *
 * Edge tessellation factors and subpatch coordinates are as follows:
 *
 *            tu1
 *     P01 --------- P11 
 *     |               |
 * tv0 |               | tv1
 *     |               |
 *     P00 --------- P10
 *            tu0
 */

class QuadDice : public EdgeDice {
public:
	struct SubPatch {
		Patch *patch;

		float2 P00;
		float2 P10;
		float2 P01;
		float2 P11;
	};

	struct EdgeFactors {
		int tu0;
		int tu1;
		int tv0;
		int tv1;
	};

	QuadDice(Mesh *mesh, int shader, bool smooth, float dicing_rate);

	void reserve(EdgeFactors& ef, int Mu, int Mv);
	float3 eval_projected(SubPatch& sub, float u, float v);

	float2 map_uv(SubPatch& sub, float u, float v);
	int add_vert(SubPatch& sub, float u, float v);

	void add_corners(SubPatch& sub);
	void add_grid(SubPatch& sub, int Mu, int Mv, int offset);

	void add_side_u(SubPatch& sub,
		vector<int>& outer, vector<int>& inner,
		int Mu, int Mv, int tu, int side, int offset);

	void add_side_v(SubPatch& sub,
		vector<int>& outer, vector<int>& inner,
		int Mu, int Mv, int tv, int side, int offset);

	float quad_area(const float3& a, const float3& b, const float3& c, const float3& d);
	float scale_factor(SubPatch& sub, EdgeFactors& ef, int Mu, int Mv);

	void dice(SubPatch& sub, EdgeFactors& ef);
};

/* Triangle EdgeDice
 *
 * Edge tessellation factors and subpatch coordinates are as follows:
 *
 *        Pw
 *        /\
 *    tv /  \ tu
 *      /    \
 *     /      \
 *  Pu -------- Pv
 *        tw     
 */

class TriangleDice : public EdgeDice {
public:
	struct SubPatch {
		Patch *patch;

		float2 Pu;
		float2 Pv;
		float2 Pw;
	};

	struct EdgeFactors {
		int tu;
		int tv;
		int tw;
	};

	TriangleDice(Mesh *mesh, int shader, bool smooth, float dicing_rate);

	void reserve(EdgeFactors& ef, int M);

	float2 map_uv(SubPatch& sub, float2 uv);
	int add_vert(SubPatch& sub, float2 uv);

	void add_grid(SubPatch& sub, EdgeFactors& ef, int M);
	void dice(SubPatch& sub, EdgeFactors& ef);
};

CCL_NAMESPACE_END

#endif /* __SUBD_DICE_H__ */

