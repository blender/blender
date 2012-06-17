/*
 * Adapted from code copyright 2009-2011 Intel Corporation
 * Modifications Copyright 2012, Blender Foundation.
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

//#define __KERNEL_SSE__

#include <stdlib.h>

#include "bvh_binning.h"

#include "util_algorithm.h"
#include "util_boundbox.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* SSE replacements */

__forceinline void prefetch_L1 (const void* ptr) { }
__forceinline void prefetch_L2 (const void* ptr) { }
__forceinline void prefetch_L3 (const void* ptr) { }
__forceinline void prefetch_NTA(const void* ptr) { }

template<size_t src> __forceinline float extract(const int4& b)
{ return b[src]; }
template<size_t dst> __forceinline const float4 insert(const float4& a, const float b)
{ float4 r = a; r[dst] = b; return r; }

__forceinline int get_best_dimension(const float4& bestSAH)
{
	// return (int)__bsf(movemask(reduce_min(bestSAH) == bestSAH));

	float minSAH = min(bestSAH.x, min(bestSAH.y, bestSAH.z));

	if(bestSAH.x == minSAH) return 0;
	else if(bestSAH.y == minSAH) return 1;
	else return 2;
}

/* BVH Object Binning */

BVHObjectBinning::BVHObjectBinning(const BVHRange& job, BVHReference *prims)
: BVHRange(job), splitSAH(FLT_MAX), dim(0), pos(0)
{
	/* compute number of bins to use and precompute scaling factor for binning */
	num_bins = min(size_t(MAX_BINS), size_t(4.0f + 0.05f*size()));
	scale = rcp(cent_bounds().size()) * make_float3((float)num_bins);

	/* initialize binning counter and bounds */
	BoundBox bin_bounds[MAX_BINS][4];	/* bounds for every bin in every dimension */
	int4 bin_count[MAX_BINS];			/* number of primitives mapped to bin */

	for(size_t i = 0; i < num_bins; i++) {
		bin_count[i] = make_int4(0);
		bin_bounds[i][0] = bin_bounds[i][1] = bin_bounds[i][2] = BoundBox::empty;
	}

	/* map geometry to bins, unrolled once */
	{
		ssize_t i;

		for(i = 0; i < ssize_t(size()) - 1; i += 2) {
			prefetch_L2(&prims[start() + i + 8]);

			/* map even and odd primitive to bin */
			BVHReference prim0 = prims[start() + i + 0];
			BVHReference prim1 = prims[start() + i + 1];

			int4 bin0 = get_bin(prim0.bounds());
			int4 bin1 = get_bin(prim1.bounds());

			/* increase bounds for bins for even primitive */
			int b00 = extract<0>(bin0); bin_count[b00][0]++; bin_bounds[b00][0].grow(prim0.bounds());
			int b01 = extract<1>(bin0); bin_count[b01][1]++; bin_bounds[b01][1].grow(prim0.bounds());
			int b02 = extract<2>(bin0); bin_count[b02][2]++; bin_bounds[b02][2].grow(prim0.bounds());

			/* increase bounds of bins for odd primitive */
			int b10 = extract<0>(bin1); bin_count[b10][0]++; bin_bounds[b10][0].grow(prim1.bounds());
			int b11 = extract<1>(bin1); bin_count[b11][1]++; bin_bounds[b11][1].grow(prim1.bounds());
			int b12 = extract<2>(bin1); bin_count[b12][2]++; bin_bounds[b12][2].grow(prim1.bounds());
		}

		/* for uneven number of primitives */
		if(i < ssize_t(size())) {
			/* map primitive to bin */
			BVHReference prim0 = prims[start() + i];
			int4 bin0 = get_bin(prim0.bounds());

			/* increase bounds of bins */
			int b00 = extract<0>(bin0); bin_count[b00][0]++; bin_bounds[b00][0].grow(prim0.bounds());
			int b01 = extract<1>(bin0); bin_count[b01][1]++; bin_bounds[b01][1].grow(prim0.bounds());
			int b02 = extract<2>(bin0); bin_count[b02][2]++; bin_bounds[b02][2].grow(prim0.bounds());
		}
	}

	/* sweep from right to left and compute parallel prefix of merged bounds */
	float4 r_area[MAX_BINS];	/* area of bounds of primitives on the right */
	float4 r_count[MAX_BINS];	/* number of primitives on the right */
	int4 count = make_int4(0);

	BoundBox bx = BoundBox::empty;
	BoundBox by = BoundBox::empty;
	BoundBox bz = BoundBox::empty;

	for(size_t i = num_bins - 1; i > 0; i--) {
		count = count + bin_count[i];
		r_count[i] = blocks(count);

		bx = merge(bx,bin_bounds[i][0]); r_area[i][0] = bx.half_area();
		by = merge(by,bin_bounds[i][1]); r_area[i][1] = by.half_area();
		bz = merge(bz,bin_bounds[i][2]); r_area[i][2] = bz.half_area();
		r_area[i][3] = r_area[i][2];
	}

	/* sweep from left to right and compute SAH */
	int4 ii = make_int4(1);
	float4 bestSAH = make_float4(FLT_MAX);
	int4 bestSplit = make_int4(-1);

	count = make_int4(0);

	bx = BoundBox::empty;
	by = BoundBox::empty;
	bz = BoundBox::empty;

	for(size_t i = 1; i < num_bins; i++, ii += make_int4(1)) {
		count = count + bin_count[i-1];

		bx = merge(bx,bin_bounds[i-1][0]); float Ax = bx.half_area();
		by = merge(by,bin_bounds[i-1][1]); float Ay = by.half_area();
		bz = merge(bz,bin_bounds[i-1][2]); float Az = bz.half_area();

		float4 lCount = blocks(count);
		float4 lArea = make_float4(Ax,Ay,Az,Az);
		float4 sah = lArea*lCount + r_area[i]*r_count[i];

		bestSplit = select(sah < bestSAH,ii,bestSplit);
		bestSAH = min(sah,bestSAH);
	}

	int4 mask = float3_to_float4(cent_bounds().size()) <= make_float4(0.0f);
	bestSAH = insert<3>(select(mask, make_float4(FLT_MAX), bestSAH), FLT_MAX);

	/* find best dimension */
	dim = get_best_dimension(bestSAH);
	splitSAH = bestSAH[dim];
	pos = bestSplit[dim];
	leafSAH	= bounds().half_area() * blocks(size());
}

void BVHObjectBinning::split(BVHReference* prims, BVHObjectBinning& left_o, BVHObjectBinning& right_o) const
{
	size_t N = size();

	BoundBox lgeom_bounds = BoundBox::empty;
	BoundBox rgeom_bounds = BoundBox::empty;
	BoundBox lcent_bounds = BoundBox::empty;
	BoundBox rcent_bounds = BoundBox::empty;

	ssize_t l = 0, r = N-1;

	while(l <= r) {
		prefetch_L2(&prims[start() + l + 8]);
		prefetch_L2(&prims[start() + r - 8]);

		BVHReference prim = prims[start() + l];
		float3 center = prim.bounds().center2();

		if(get_bin(center)[dim] < pos) {
			lgeom_bounds.grow(prim.bounds());
			lcent_bounds.grow(center);
			l++;
		}
		else {
			rgeom_bounds.grow(prim.bounds());
			rcent_bounds.grow(center);
			swap(prims[start()+l],prims[start()+r]);
			r--;
		}
	}

	/* finish */
	if(l != 0 && N-1-r != 0) {
		right_o = BVHObjectBinning(BVHRange(rgeom_bounds, rcent_bounds, start() + l, N-1-r), prims);
		left_o  = BVHObjectBinning(BVHRange(lgeom_bounds, lcent_bounds, start(), l), prims);
		return;
	}

	/* object medium split if we did not make progress, can happen when all
	 * primitives have same centroid */
	lgeom_bounds = BoundBox::empty;
	rgeom_bounds = BoundBox::empty;
	lcent_bounds = BoundBox::empty;
	rcent_bounds = BoundBox::empty;

	for(size_t i = 0; i < N/2; i++) {
		lgeom_bounds.grow(prims[start()+i].bounds());
		lcent_bounds.grow(prims[start()+i].bounds().center2());
	}

	for(size_t i = N/2; i < N; i++) {
		rgeom_bounds.grow(prims[start()+i].bounds());
		rcent_bounds.grow(prims[start()+i].bounds().center2());
	}

	right_o = BVHObjectBinning(BVHRange(rgeom_bounds, rcent_bounds, start() + N/2, N/2 + N%2), prims);
	left_o  = BVHObjectBinning(BVHRange(lgeom_bounds, lcent_bounds, start(), N/2), prims);
}

CCL_NAMESPACE_END

