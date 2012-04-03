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

#ifndef __SUBD_SPLIT_H__
#define __SUBD_SPLIT_H__

/* DiagSplit: Parallel, Crack-free, Adaptive Tessellation for Micropolygon Rendering
 * Splits up patches and determines edge tessellation factors for dicing. Patch
 * evaluation at arbitrary points is required for this to work. See the paper
 * for more details. */

#include "subd_dice.h"

#include "util_types.h"
#include "util_vector.h"

CCL_NAMESPACE_BEGIN

class Mesh;
class Patch;

#define DSPLIT_NON_UNIFORM -1

class DiagSplit {
public:
	vector<QuadDice::SubPatch> subpatches_quad;
	vector<QuadDice::EdgeFactors> edgefactors_quad;
	vector<TriangleDice::SubPatch> subpatches_triangle;
	vector<TriangleDice::EdgeFactors> edgefactors_triangle;

	int test_steps;
	int split_threshold;
	float dicing_rate;
	Camera *camera;

	DiagSplit();

	float3 project(Patch *patch, float2 uv);
	int T(Patch *patch, float2 Pstart, float2 Pend);
	void partition_edge(Patch *patch, float2 *P, int *t0, int *t1,
		float2 Pstart, float2 Pend, int t);

	void dispatch(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef);
	void split(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef, int depth=0);

	void dispatch(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef);
	void split(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef, int depth=0);

	void split_triangle(Mesh *mesh, Patch *patch, int shader, bool smooth);
	void split_quad(Mesh *mesh, Patch *patch, int shader, bool smooth);
};

CCL_NAMESPACE_END

#endif /* __SUBD_SPLIT_H__ */

