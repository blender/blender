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

struct SubdParams {
	Mesh *mesh;
	int shader;
	bool smooth;
	bool ptex;

	int test_steps;
	int split_threshold;
	float dicing_rate;
	Camera *camera;

	SubdParams(Mesh *mesh_, int shader_, bool smooth_ = true, bool ptex_ = false)
	{
		mesh = mesh_;
		shader = shader_;
		smooth = smooth_;
		ptex = ptex_;

		test_steps = 3;
		split_threshold = 1;
		dicing_rate = 0.1f;
		camera = NULL;
	}

};

/* EdgeDice Base */

class EdgeDice {
public:
	SubdParams params;
	float3 *mesh_P;
	float3 *mesh_N;
	size_t vert_offset;
	size_t tri_offset;

	EdgeDice(const SubdParams& params);

	void reserve(int num_verts, int num_tris);

	int add_vert(Patch *patch, float2 uv);
	void add_triangle(Patch *patch, int v0, int v1, int v2);

	void stitch_triangles(Patch *patch, vector<int>& outer, vector<int>& inner);
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

	QuadDice(const SubdParams& params);

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

	TriangleDice(const SubdParams& params);

	void reserve(EdgeFactors& ef, int M);

	float2 map_uv(SubPatch& sub, float2 uv);
	int add_vert(SubPatch& sub, float2 uv);

	void add_grid(SubPatch& sub, EdgeFactors& ef, int M);
	void dice(SubPatch& sub, EdgeFactors& ef);
};

CCL_NAMESPACE_END

#endif /* __SUBD_DICE_H__ */

