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

DiagSplit::DiagSplit()
{
	test_steps = 3;
	split_threshold = 1;
	dicing_rate = 0.1f;
	camera = NULL;
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

float3 DiagSplit::project(Patch *patch, float2 uv)
{
	float3 P;

	patch->eval(&P, NULL, NULL, uv.x, uv.y);
	if(camera)
		P = transform_perspective(&camera->worldtoraster, P);

	return P;
}

int DiagSplit::T(Patch *patch, float2 Pstart, float2 Pend)
{
	float3 Plast = make_float3(0.0f, 0.0f, 0.0f);
	float Lsum = 0.0f;
	float Lmax = 0.0f;

	for(int i = 0; i < test_steps; i++) {
		float t = i/(float)(test_steps-1);

		float3 P = project(patch, Pstart + t*(Pend - Pstart));

		if(i > 0) {
			float L = len(P - Plast);
			Lsum += L;
			Lmax = max(L, Lmax);
		}

		Plast = P;
	}

	int tmin = (int)ceil(Lsum/dicing_rate);
	int tmax = (int)ceil((test_steps-1)*Lmax/dicing_rate); // XXX paper says N instead of N-1, seems wrong?

	if(tmax - tmin > split_threshold)
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
		int I = floor(t*0.5f);
		*P = interp(Pstart, Pend, (t == 0)? 0: I/(float)t); /* XXX is t faces or verts */
		*t0 = I;
		*t1 = t - I;
	}
}

void DiagSplit::split(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef, int depth)
{
	assert(ef.tu == T(sub.patch, sub.Pv, sub.Pw));
	assert(ef.tv == T(sub.patch, sub.Pw, sub.Pu));
	assert(ef.tw == T(sub.patch, sub.Pu, sub.Pv));

	if(depth == 0) {
		dispatch(sub, ef);
		return;
	}

	if(ef.tu == DSPLIT_NON_UNIFORM) {
		/* partition edges */
		TriangleDice::EdgeFactors ef0, ef1;
		float2 Psplit;

		partition_edge(sub.patch,
			&Psplit, &ef1.tu, &ef0.tu, sub.Pv, sub.Pw, ef.tu);

		/* split */
		int tsplit = T(sub.patch, sub.Pu, Psplit);
		ef0.tv = ef.tv;
		ef0.tw = tsplit;

		ef1.tv = tsplit;
		ef1.tw = ef.tw;

		/* create subpatches */
		TriangleDice::SubPatch sub0 = {sub.patch, sub.Pu, Psplit, sub.Pw};
		TriangleDice::SubPatch sub1 = {sub.patch, sub.Pu, sub.Pv, Psplit};

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else if(ef.tv == DSPLIT_NON_UNIFORM) {
		/* partition edges */
		TriangleDice::EdgeFactors ef0, ef1;
		float2 Psplit;

		partition_edge(sub.patch,
			&Psplit, &ef1.tu, &ef0.tu, sub.Pw, sub.Pu, ef.tv);

		/* split */
		int tsplit = T(sub.patch, sub.Pv, Psplit);
		ef0.tv = ef.tw;
		ef0.tw = tsplit;

		ef1.tv = tsplit;
		ef1.tw = ef.tu;

		/* create subpatches */
		TriangleDice::SubPatch sub0 = {sub.patch, sub.Pv, Psplit, sub.Pu};
		TriangleDice::SubPatch sub1 = {sub.patch, sub.Pv, sub.Pw, Psplit};

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else if(ef.tw == DSPLIT_NON_UNIFORM) {
		/* partition edges */
		TriangleDice::EdgeFactors ef0, ef1;
		float2 Psplit;

		partition_edge(sub.patch,
			&Psplit, &ef1.tu, &ef0.tu, sub.Pu, sub.Pv, ef.tw);

		/* split */
		int tsplit = T(sub.patch, sub.Pw, Psplit);
		ef0.tv = ef.tu;
		ef0.tw = tsplit;

		ef1.tv = tsplit;
		ef1.tw = ef.tv;

		/* create subpatches */
		TriangleDice::SubPatch sub0 = {sub.patch, sub.Pw, Psplit, sub.Pv};
		TriangleDice::SubPatch sub1 = {sub.patch, sub.Pw, sub.Pu, Psplit};

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else
		dispatch(sub, ef);
}

void DiagSplit::split(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef, int depth)
{
	if((ef.tu0 == DSPLIT_NON_UNIFORM || ef.tu1 == DSPLIT_NON_UNIFORM)) {
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

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else if(ef.tv0 == DSPLIT_NON_UNIFORM || ef.tv1 == DSPLIT_NON_UNIFORM) {
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

		split(sub0, ef0, depth+1);
		split(sub1, ef1, depth+1);
	}
	else
		dispatch(sub, ef);
}

void DiagSplit::split_triangle(Mesh *mesh, Patch *patch, int shader, bool smooth)
{
	TriangleDice::SubPatch sub;
	TriangleDice::EdgeFactors ef;

	sub.patch = patch;
	sub.Pu = make_float2(1.0f, 0.0f);
	sub.Pv = make_float2(0.0f, 1.0f);
	sub.Pw = make_float2(0.0f, 0.0f);

	ef.tu = T(patch, sub.Pv, sub.Pw);
	ef.tv = T(patch, sub.Pw, sub.Pu);
	ef.tw = T(patch, sub.Pu, sub.Pv);

	split(sub, ef);

	TriangleDice dice(mesh, shader, smooth, dicing_rate);
	dice.camera = camera;

	for(size_t i = 0; i < subpatches_triangle.size(); i++) {
		TriangleDice::SubPatch& sub = subpatches_triangle[i];
		TriangleDice::EdgeFactors& ef = edgefactors_triangle[i];

		ef.tu = 4;
		ef.tv = 4;
		ef.tw = 4;

		ef.tu = max(ef.tu, 1);
		ef.tv = max(ef.tv, 1);
		ef.tw = max(ef.tw, 1);

		dice.dice(sub, ef);
	}

	subpatches_triangle.clear();
	edgefactors_triangle.clear();
}

void DiagSplit::split_quad(Mesh *mesh, Patch *patch, int shader, bool smooth)
{
	QuadDice::SubPatch sub;
	QuadDice::EdgeFactors ef;

	sub.patch = patch;
	sub.P00 = make_float2(0.0f, 0.0f);
	sub.P10 = make_float2(1.0f, 0.0f);
	sub.P01 = make_float2(0.0f, 1.0f);
	sub.P11 = make_float2(1.0f, 1.0f);

	ef.tu0 = T(patch, sub.P00, sub.P10);
	ef.tu1 = T(patch, sub.P01, sub.P11);
	ef.tv0 = T(patch, sub.P00, sub.P01);
	ef.tv1 = T(patch, sub.P10, sub.P11);

	split(sub, ef);

	QuadDice dice(mesh, shader, smooth, dicing_rate);
	dice.camera = camera;

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

