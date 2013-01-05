/*
 * Adapted from code copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
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

#include "bvh_build.h"
#include "bvh_split.h"
#include "bvh_sort.h"

#include "mesh.h"
#include "object.h"

#include "util_algorithm.h"

CCL_NAMESPACE_BEGIN

/* Object Split */

BVHObjectSplit::BVHObjectSplit(BVHBuild *builder, const BVHRange& range, float nodeSAH)
: sah(FLT_MAX), dim(0), num_left(0), left_bounds(BoundBox::empty), right_bounds(BoundBox::empty)
{
	const BVHReference *ref_ptr = &builder->references[range.start()];
	float min_sah = FLT_MAX;

	for(int dim = 0; dim < 3; dim++) {
		/* sort references */
		bvh_reference_sort(range.start(), range.end(), &builder->references[0], dim);

		/* sweep right to left and determine bounds. */
		BoundBox right_bounds = BoundBox::empty;

		for(int i = range.size() - 1; i > 0; i--) {
			right_bounds.grow(ref_ptr[i].bounds());
			builder->spatial_right_bounds[i - 1] = right_bounds;
		}

		/* sweep left to right and select lowest SAH. */
		BoundBox left_bounds = BoundBox::empty;

		for(int i = 1; i < range.size(); i++) {
			left_bounds.grow(ref_ptr[i - 1].bounds());
			right_bounds = builder->spatial_right_bounds[i - 1];

			float sah = nodeSAH +
				left_bounds.safe_area() * builder->params.triangle_cost(i) +
				right_bounds.safe_area() * builder->params.triangle_cost(range.size() - i);

			if(sah < min_sah) {
				min_sah = sah;

				this->sah = sah;
				this->dim = dim;
				this->num_left = i;
				this->left_bounds = left_bounds;
				this->right_bounds = right_bounds;
			}
		}
	}
}

void BVHObjectSplit::split(BVHBuild *builder, BVHRange& left, BVHRange& right, const BVHRange& range)
{
	/* sort references according to split */
	bvh_reference_sort(range.start(), range.end(), &builder->references[0], this->dim);

	/* split node ranges */
	left = BVHRange(this->left_bounds, range.start(), this->num_left);
	right = BVHRange(this->right_bounds, left.end(), range.size() - this->num_left);

}

/* Spatial Split */

BVHSpatialSplit::BVHSpatialSplit(BVHBuild *builder, const BVHRange& range, float nodeSAH)
: sah(FLT_MAX), dim(0), pos(0.0f)
{
	/* initialize bins. */
	float3 origin = range.bounds().min;
	float3 binSize = (range.bounds().max - origin) * (1.0f / (float)BVHParams::NUM_SPATIAL_BINS);
	float3 invBinSize = 1.0f / binSize;

	for(int dim = 0; dim < 3; dim++) {
		for(int i = 0; i < BVHParams::NUM_SPATIAL_BINS; i++) {
			BVHSpatialBin& bin = builder->spatial_bins[dim][i];

			bin.bounds = BoundBox::empty;
			bin.enter = 0;
			bin.exit = 0;
		}
	}

	/* chop references into bins. */
	for(unsigned int refIdx = range.start(); refIdx < range.end(); refIdx++) {
		const BVHReference& ref = builder->references[refIdx];
		float3 firstBinf = (ref.bounds().min - origin) * invBinSize;
		float3 lastBinf = (ref.bounds().max - origin) * invBinSize;
		int3 firstBin = make_int3((int)firstBinf.x, (int)firstBinf.y, (int)firstBinf.z);
		int3 lastBin = make_int3((int)lastBinf.x, (int)lastBinf.y, (int)lastBinf.z);

		firstBin = clamp(firstBin, 0, BVHParams::NUM_SPATIAL_BINS - 1);
		lastBin = clamp(lastBin, firstBin, BVHParams::NUM_SPATIAL_BINS - 1);

		for(int dim = 0; dim < 3; dim++) {
			BVHReference currRef = ref;

			for(int i = firstBin[dim]; i < lastBin[dim]; i++) {
				BVHReference leftRef, rightRef;

				split_reference(builder, leftRef, rightRef, currRef, dim, origin[dim] + binSize[dim] * (float)(i + 1));
				builder->spatial_bins[dim][i].bounds.grow(leftRef.bounds());
				currRef = rightRef;
			}

			builder->spatial_bins[dim][lastBin[dim]].bounds.grow(currRef.bounds());
			builder->spatial_bins[dim][firstBin[dim]].enter++;
			builder->spatial_bins[dim][lastBin[dim]].exit++;
		}
	}

	/* select best split plane. */
	for(int dim = 0; dim < 3; dim++) {
		/* sweep right to left and determine bounds. */
		BoundBox right_bounds = BoundBox::empty;

		for(int i = BVHParams::NUM_SPATIAL_BINS - 1; i > 0; i--) {
			right_bounds.grow(builder->spatial_bins[dim][i].bounds);
			builder->spatial_right_bounds[i - 1] = right_bounds;
		}

		/* sweep left to right and select lowest SAH. */
		BoundBox left_bounds = BoundBox::empty;
		int leftNum = 0;
		int rightNum = range.size();

		for(int i = 1; i < BVHParams::NUM_SPATIAL_BINS; i++) {
			left_bounds.grow(builder->spatial_bins[dim][i - 1].bounds);
			leftNum += builder->spatial_bins[dim][i - 1].enter;
			rightNum -= builder->spatial_bins[dim][i - 1].exit;

			float sah = nodeSAH +
				left_bounds.safe_area() * builder->params.triangle_cost(leftNum) +
				builder->spatial_right_bounds[i - 1].safe_area() * builder->params.triangle_cost(rightNum);

			if(sah < this->sah) {
				this->sah = sah;
				this->dim = dim;
				this->pos = origin[dim] + binSize[dim] * (float)i;
			}
		}
	}
}

void BVHSpatialSplit::split(BVHBuild *builder, BVHRange& left, BVHRange& right, const BVHRange& range)
{
	/* Categorize references and compute bounds.
	 *
	 * Left-hand side:			[left_start, left_end[
	 * Uncategorized/split:		[left_end, right_start[
	 * Right-hand side:			[right_start, refs.size()[ */

	vector<BVHReference>& refs = builder->references;
	int left_start = range.start();
	int left_end = left_start;
	int right_start = range.end();
	int right_end = range.end();
	BoundBox left_bounds = BoundBox::empty;
	BoundBox right_bounds = BoundBox::empty;

	for(int i = left_end; i < right_start; i++) {
		if(refs[i].bounds().max[this->dim] <= this->pos) {
			/* entirely on the left-hand side */
			left_bounds.grow(refs[i].bounds());
			swap(refs[i], refs[left_end++]);
		}
		else if(refs[i].bounds().min[this->dim] >= this->pos) {
			/* entirely on the right-hand side */
			right_bounds.grow(refs[i].bounds());
			swap(refs[i--], refs[--right_start]);
		}
	}

	/* duplicate or unsplit references intersecting both sides. */
	while(left_end < right_start) {
		/* split reference. */
		BVHReference lref, rref;

		split_reference(builder, lref, rref, refs[left_end], this->dim, this->pos);

		/* compute SAH for duplicate/unsplit candidates. */
		BoundBox lub = left_bounds;		// Unsplit to left:		new left-hand bounds.
		BoundBox rub = right_bounds;	// Unsplit to right:	new right-hand bounds.
		BoundBox ldb = left_bounds;		// Duplicate:			new left-hand bounds.
		BoundBox rdb = right_bounds;	// Duplicate:			new right-hand bounds.

		lub.grow(refs[left_end].bounds());
		rub.grow(refs[left_end].bounds());
		ldb.grow(lref.bounds());
		rdb.grow(rref.bounds());

		float lac = builder->params.triangle_cost(left_end - left_start);
		float rac = builder->params.triangle_cost(right_end - right_start);
		float lbc = builder->params.triangle_cost(left_end - left_start + 1);
		float rbc = builder->params.triangle_cost(right_end - right_start + 1);

		float unsplitLeftSAH = lub.safe_area() * lbc + right_bounds.safe_area() * rac;
		float unsplitRightSAH = left_bounds.safe_area() * lac + rub.safe_area() * rbc;
		float duplicateSAH = ldb.safe_area() * lbc + rdb.safe_area() * rbc;
		float minSAH = min(min(unsplitLeftSAH, unsplitRightSAH), duplicateSAH);

		if(minSAH == unsplitLeftSAH) {
			/* unsplit to left */
			left_bounds = lub;
			left_end++;
		}
		else if(minSAH == unsplitRightSAH) {
			/* unsplit to right */
			right_bounds = rub;
			swap(refs[left_end], refs[--right_start]);
		}
		else {
			/* duplicate */
			left_bounds = ldb;
			right_bounds = rdb;
			refs[left_end++] = lref;
			refs.insert(refs.begin() + right_end, rref);
			right_end++;
		}
	}

	left = BVHRange(left_bounds, left_start, left_end - left_start);
	right = BVHRange(right_bounds, right_start, right_end - right_start);
}

void BVHSpatialSplit::split_reference(BVHBuild *builder, BVHReference& left, BVHReference& right, const BVHReference& ref, int dim, float pos)
{
	/* initialize boundboxes */
	BoundBox left_bounds = BoundBox::empty;
	BoundBox right_bounds = BoundBox::empty;

	/* loop over vertices/edges. */
	Object *ob = builder->objects[ref.prim_object()];
	const Mesh *mesh = ob->mesh;

	if (ref.prim_segment() == ~0) {
		const int *inds = mesh->triangles[ref.prim_index()].v;
		const float3 *verts = &mesh->verts[0];
		const float3* v1 = &verts[inds[2]];

		for(int i = 0; i < 3; i++) {
			const float3* v0 = v1;
			int vindex = inds[i];
			v1 = &verts[vindex];
			float v0p = (*v0)[dim];
			float v1p = (*v1)[dim];

			/* insert vertex to the boxes it belongs to. */
			if(v0p <= pos)
				left_bounds.grow(*v0);

			if(v0p >= pos)
				right_bounds.grow(*v0);

			/* edge intersects the plane => insert intersection to both boxes. */
			if((v0p < pos && v1p > pos) || (v0p > pos && v1p < pos)) {
				float3 t = lerp(*v0, *v1, clamp((pos - v0p) / (v1p - v0p), 0.0f, 1.0f));
				left_bounds.grow(t);
				right_bounds.grow(t);
			}
		}
	}
	else {
		/* curve split: NOTE - Currently ignores curve width and needs to be fixed.*/
		const int k0 = mesh->curves[ref.prim_index()].first_key + ref.prim_segment();
		const int k1 = k0 + 1;
		const float3* v0 = &mesh->curve_keys[k0].co;
		const float3* v1 = &mesh->curve_keys[k1].co;

		float v0p = (*v0)[dim];
		float v1p = (*v1)[dim];

		/* insert vertex to the boxes it belongs to. */
		if(v0p <= pos)
			left_bounds.grow(*v0);

		if(v0p >= pos)
			right_bounds.grow(*v0);

		if(v1p <= pos)
			left_bounds.grow(*v1);

		if(v1p >= pos)
			right_bounds.grow(*v1);

		/* edge intersects the plane => insert intersection to both boxes. */
		if((v0p < pos && v1p > pos) || (v0p > pos && v1p < pos)) {
			float3 t = lerp(*v0, *v1, clamp((pos - v0p) / (v1p - v0p), 0.0f, 1.0f));
			left_bounds.grow(t);
			right_bounds.grow(t);
		}
	}

	/* intersect with original bounds. */
	left_bounds.max[dim] = pos;
	right_bounds.min[dim] = pos;
	left_bounds.intersect(ref.bounds());
	right_bounds.intersect(ref.bounds());

	/* set references */
	left = BVHReference(left_bounds, ref.prim_index(), ref.prim_object(), ref.prim_segment());
	right = BVHReference(right_bounds, ref.prim_index(), ref.prim_object(), ref.prim_segment());
}

CCL_NAMESPACE_END

