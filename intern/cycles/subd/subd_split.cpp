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

#include "camera.h"
#include "mesh.h"

#include "subd_dice.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_debug.h"
#include "util_math.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* DiagSplit */

DiagSplit::DiagSplit(const SubdParams& params_)
: params(params_)
{
}

void DiagSplit::dispatch(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef)
{
	subpatches_quad.push_back(sub);
	edgefactors_quad.push_back(ef);
}

void DiagSplit::dispatch(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef)
{
	subpatches_triangle.push_back(sub);
	edgefactors_triangle.push_back(ef);
}

float3 DiagSplit::to_world(Patch *patch, float2 uv)
{
	float3 P;

	patch->eval(&P, NULL, NULL, NULL, uv.x, uv.y);
	if(params.camera)
		P = transform_point(&params.objecttoworld, P);

	return P;
}

int DiagSplit::T(Patch *patch, float2 Pstart, float2 Pend)
{
	float3 Plast = make_float3(0.0f, 0.0f, 0.0f);
	float Lsum = 0.0f;
	float Lmax = 0.0f;

	for(int i = 0; i < params.test_steps; i++) {
		float t = i/(float)(params.test_steps-1);

		float3 P = to_world(patch, Pstart + t*(Pend - Pstart));

		if(i > 0) {
			float L;

			if(!params.camera) {
				L = len(P - Plast);
			}
			else {
				Camera* cam = params.camera;

				float pixel_width = cam->world_to_raster_size((P + Plast) * 0.5f);
				L = len(P - Plast) / pixel_width;
			}

			Lsum += L;
			Lmax = max(L, Lmax);
		}

		Plast = P;
	}

	int tmin = (int)ceil(Lsum/params.dicing_rate);
	int tmax = (int)ceil((params.test_steps-1)*Lmax/params.dicing_rate); // XXX paper says N instead of N-1, seems wrong?

	if(tmax - tmin > params.split_threshold)
		return DSPLIT_NON_UNIFORM;
	
	return tmax;
}

void DiagSplit::partition_edge(Patch *patch, float2 *P, int *t0, int *t1, float2 Pstart, float2 Pend, int t)
{
	if(t == DSPLIT_NON_UNIFORM) {
		*P = (Pstart + Pend)*0.5f;
		*t0 = T(patch, Pstart, *P);
		*t1 = T(patch, *P, Pend);
	}
	else {
		int I = (int)floor((float)t*0.5f);
		*P = interp(Pstart, Pend, (t == 0)? 0: I/(float)t); /* XXX is t faces or verts */
		*t0 = I;
		*t1 = t - I;
	}
}

static float2 right_to_equilateral(float2 P)
{
	static const float2 A = make_float2(1.0f, 0.5f);
	static const float2 B = make_float2(0.0f, sinf(M_PI_F/3.0f));
	return make_float2(dot(P, A), dot(P, B));
}

static void limit_edge_factors(const TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef, int max_t)
{
	float2 Pu = sub.Pu;
	float2 Pv = sub.Pv;
	float2 Pw = sub.Pw;

	if(sub.patch->is_triangle()) {
		Pu = right_to_equilateral(Pu);
		Pv = right_to_equilateral(Pv);
		Pw = right_to_equilateral(Pw);
	}

	int tu = int(max_t * len(Pw - Pv));
	int tv = int(max_t * len(Pw - Pu));
	int tw = int(max_t * len(Pv - Pu));

	ef.tu = tu <= 1 ? 1 : min(ef.tu, tu);
	ef.tv = tv <= 1 ? 1 : min(ef.tv, tv);
	ef.tw = tw <= 1 ? 1 : min(ef.tw, tw);
}

static void limit_edge_factors(const QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef, int max_t)
{
	float2 P00 = sub.P00;
	float2 P01 = sub.P01;
	float2 P10 = sub.P10;
	float2 P11 = sub.P11;

	if(sub.patch->is_triangle()) {
		P00 = right_to_equilateral(P00);
		P01 = right_to_equilateral(P01);
		P10 = right_to_equilateral(P10);
		P11 = right_to_equilateral(P11);
	}

	int tu0 = int(max_t * len(P10 - P00));
	int tu1 = int(max_t * len(P11 - P01));
	int tv0 = int(max_t * len(P01 - P00));
	int tv1 = int(max_t * len(P11 - P10));

	ef.tu0 = tu0 <= 1 ? 1 : min(ef.tu0, tu0);
	ef.tu1 = tu1 <= 1 ? 1 : min(ef.tu1, tu1);
	ef.tv0 = tv0 <= 1 ? 1 : min(ef.tv0, tv0);
	ef.tv1 = tv1 <= 1 ? 1 : min(ef.tv1, tv1);
}

void DiagSplit::split(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef, int depth)
{
	if(depth > 32) {
		/* We should never get here, but just in case end recursion safely. */
		ef.tu = 1;
		ef.tv = 1;
		ef.tw = 1;

		dispatch(sub, ef);
		return;
	}

	assert(ef.tu == T(sub.patch, sub.Pv, sub.Pw));
	assert(ef.tv == T(sub.patch, sub.Pw, sub.Pu));
	assert(ef.tw == T(sub.patch, sub.Pu, sub.Pv));

	int non_uniform_count = int(ef.tu == DSPLIT_NON_UNIFORM) +
	                        int(ef.tv == DSPLIT_NON_UNIFORM) +
                            int(ef.tw == DSPLIT_NON_UNIFORM);

	switch(non_uniform_count) {
		case 1: {
			/* TODO(mai): one edge is non-uniform, split into two triangles */
			// fallthru
		}
		case 2: {
			/* TODO(mai): two edges are non-uniform, split into triangle and quad */
			// fallthru
		}
		case 3: {
			/* all three edges are non-uniform, split into three quads */

			/* partition edges */
			QuadDice::EdgeFactors ef0, ef1, ef2;
			float2 Pu, Pv, Pw, Pcenter;

			partition_edge(sub.patch, &Pu, &ef1.tv0, &ef2.tu0, sub.Pw, sub.Pv, ef.tu);
			partition_edge(sub.patch, &Pv, &ef0.tv0, &ef1.tu0, sub.Pu, sub.Pw, ef.tv);
			partition_edge(sub.patch, &Pw, &ef2.tv0, &ef0.tu0, sub.Pv, sub.Pu, ef.tw);
			Pcenter = (Pu + Pv + Pw) * (1.0f / 3.0f);

			/* split */
			int tsplit01 = T(sub.patch, Pv, Pcenter);
			int tsplit12 = T(sub.patch, Pu, Pcenter);
			int tsplit20 = T(sub.patch, Pw, Pcenter);

			ef0.tu1 = tsplit01;
			ef0.tv1 = tsplit20;

			ef1.tu1 = tsplit12;
			ef1.tv1 = tsplit01;

			ef2.tu1 = tsplit20;
			ef2.tv1 = tsplit12;

			/* create subpatches */
			QuadDice::SubPatch sub0 = {sub.patch, sub.Pu, Pw, Pv, Pcenter};
			QuadDice::SubPatch sub1 = {sub.patch, sub.Pw, Pv, Pu, Pcenter};
			QuadDice::SubPatch sub2 = {sub.patch, sub.Pv, Pu, Pw, Pcenter};

			limit_edge_factors(sub0, ef0, 1 << params.max_level);
			limit_edge_factors(sub1, ef1, 1 << params.max_level);
			limit_edge_factors(sub2, ef2, 1 << params.max_level);

			split(sub0, ef0, depth+1);
			split(sub1, ef1, depth+1);
			split(sub2, ef2, depth+1);

			break;
		}
		default: {
			/* all edges uniform, no splitting needed */
			dispatch(sub, ef);
			break;
		}
	}
}

void DiagSplit::split(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef, int depth)
{
	if(depth > 32) {
		/* We should never get here, but just in case end recursion safely. */
		ef.tu0 = 1;
		ef.tu1 = 1;
		ef.tv0 = 1;
		ef.tv1 = 1;

		dispatch(sub, ef);
		return;
	}

	bool split_u = (ef.tu0 == DSPLIT_NON_UNIFORM || ef.tu1 == DSPLIT_NON_UNIFORM);
	bool split_v = (ef.tv0 == DSPLIT_NON_UNIFORM || ef.tv1 == DSPLIT_NON_UNIFORM);

	if(split_u && split_v) {
		split_u = depth % 2;
	}

	if(split_u) {
		/* partition edges */
		QuadDice::EdgeFactors ef0, ef1;
		float2 Pu0, Pu1;

		partition_edge(sub.patch,
			&Pu0, &ef0.tu0, &ef1.tu0, sub.P00, sub.P10, ef.tu0);
		partition_edge(sub.patch,
			&Pu1, &ef0.tu1, &ef1.tu1, sub.P01, sub.P11, ef.tu1);

		/* split */
		int tsplit = T(sub.patch, Pu0, Pu1);
		ef0.tv0 = ef.tv0;
		ef0.tv1 = tsplit;

		ef1.tv0 = tsplit;
		ef1.tv1 = ef.tv1;

		/* create subpatches */
		QuadDice::SubPatch sub0 = {sub.patch, sub.P00, Pu0, sub.P01, Pu1};
		QuadDice::SubPatch sub1 = {sub.patch, Pu0, sub.P10, Pu1, sub.P11};

		limit_edge_factors(sub0, ef0, 1 << params.max_level);
		limit_edge_factors(sub1, ef1, 1 << params.max_level);

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else if(split_v) {
		/* partition edges */
		QuadDice::EdgeFactors ef0, ef1;
		float2 Pv0, Pv1;

		partition_edge(sub.patch,
			&Pv0, &ef0.tv0, &ef1.tv0, sub.P00, sub.P01, ef.tv0);
		partition_edge(sub.patch,
			&Pv1, &ef0.tv1, &ef1.tv1, sub.P10, sub.P11, ef.tv1);

		/* split */
		int tsplit = T(sub.patch, Pv0, Pv1);
		ef0.tu0 = ef.tu0;
		ef0.tu1 = tsplit;

		ef1.tu0 = tsplit;
		ef1.tu1 = ef.tu1;

		/* create subpatches */
		QuadDice::SubPatch sub0 = {sub.patch, sub.P00, sub.P10, Pv0, Pv1};
		QuadDice::SubPatch sub1 = {sub.patch, Pv0, Pv1, sub.P01, sub.P11};

		limit_edge_factors(sub0, ef0, 1 << params.max_level);
		limit_edge_factors(sub1, ef1, 1 << params.max_level);

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else {
		dispatch(sub, ef);
	}
}

void DiagSplit::split_triangle(Patch *patch)
{
	TriangleDice::SubPatch sub_split;
	TriangleDice::EdgeFactors ef_split;

	sub_split.patch = patch;
	sub_split.Pu = make_float2(1.0f, 0.0f);
	sub_split.Pv = make_float2(0.0f, 1.0f);
	sub_split.Pw = make_float2(0.0f, 0.0f);

	ef_split.tu = T(patch, sub_split.Pv, sub_split.Pw);
	ef_split.tv = T(patch, sub_split.Pw, sub_split.Pu);
	ef_split.tw = T(patch, sub_split.Pu, sub_split.Pv);

	limit_edge_factors(sub_split, ef_split, 1 << params.max_level);

	split(sub_split, ef_split);

	TriangleDice dice(params);

	for(size_t i = 0; i < subpatches_triangle.size(); i++) {
		TriangleDice::SubPatch& sub = subpatches_triangle[i];
		TriangleDice::EdgeFactors& ef = edgefactors_triangle[i];

		ef.tu = max(ef.tu, 1);
		ef.tv = max(ef.tv, 1);
		ef.tw = max(ef.tw, 1);

		dice.dice(sub, ef);
	}

	subpatches_triangle.clear();
	edgefactors_triangle.clear();

	/* triangle might be split into quads so dice quad subpatches as well */
	QuadDice qdice(params);

	for(size_t i = 0; i < subpatches_quad.size(); i++) {
		QuadDice::SubPatch& sub = subpatches_quad[i];
		QuadDice::EdgeFactors& ef = edgefactors_quad[i];

		ef.tu0 = max(ef.tu0, 1);
		ef.tu1 = max(ef.tu1, 1);
		ef.tv0 = max(ef.tv0, 1);
		ef.tv1 = max(ef.tv1, 1);

		qdice.dice(sub, ef);
	}

	subpatches_quad.clear();
	edgefactors_quad.clear();
}

void DiagSplit::split_quad(Patch *patch)
{
	QuadDice::SubPatch sub_split;
	QuadDice::EdgeFactors ef_split;

	sub_split.patch = patch;
	sub_split.P00 = make_float2(0.0f, 0.0f);
	sub_split.P10 = make_float2(1.0f, 0.0f);
	sub_split.P01 = make_float2(0.0f, 1.0f);
	sub_split.P11 = make_float2(1.0f, 1.0f);

	ef_split.tu0 = T(patch, sub_split.P00, sub_split.P10);
	ef_split.tu1 = T(patch, sub_split.P01, sub_split.P11);
	ef_split.tv0 = T(patch, sub_split.P00, sub_split.P01);
	ef_split.tv1 = T(patch, sub_split.P10, sub_split.P11);

	limit_edge_factors(sub_split, ef_split, 1 << params.max_level);

	split(sub_split, ef_split);

	QuadDice dice(params);

	for(size_t i = 0; i < subpatches_quad.size(); i++) {
		QuadDice::SubPatch& sub = subpatches_quad[i];
		QuadDice::EdgeFactors& ef = edgefactors_quad[i];

		ef.tu0 = max(ef.tu0, 1);
		ef.tu1 = max(ef.tu1, 1);
		ef.tv0 = max(ef.tv0, 1);
		ef.tv1 = max(ef.tv1, 1);

		dice.dice(sub, ef);
	}

	subpatches_quad.clear();
	edgefactors_quad.clear();
}

CCL_NAMESPACE_END

