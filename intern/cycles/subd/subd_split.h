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

	SubdParams params;

	DiagSplit(const SubdParams& params);

	float3 project(Patch *patch, float2 uv);
	int T(Patch *patch, float2 Pstart, float2 Pend);
	void partition_edge(Patch *patch, float2 *P, int *t0, int *t1,
		float2 Pstart, float2 Pend, int t);

	void dispatch(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef);
	void split(QuadDice::SubPatch& sub, QuadDice::EdgeFactors& ef, int depth=0);

	void dispatch(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef);
	void split(TriangleDice::SubPatch& sub, TriangleDice::EdgeFactors& ef, int depth=0);

	void split_triangle(Patch *patch);
	void split_quad(Patch *patch);
};

CCL_NAMESPACE_END

#endif /* __SUBD_SPLIT_H__ */

