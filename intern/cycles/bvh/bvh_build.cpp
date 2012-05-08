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
#include "bvh_node.h"
#include "bvh_params.h"
#include "bvh_sort.h"

#include "mesh.h"
#include "object.h"
#include "scene.h"

#include "util_algorithm.h"
#include "util_foreach.h"
#include "util_progress.h"
#include "util_time.h"

CCL_NAMESPACE_BEGIN

/* Constructor / Destructor */

BVHBuild::BVHBuild(const vector<Object*>& objects_,
	vector<int>& prim_index_, vector<int>& prim_object_,
	const BVHParams& params_, Progress& progress_)
: objects(objects_),
  prim_index(prim_index_),
  prim_object(prim_object_),
  params(params_),
  progress(progress_),
  progress_start_time(0.0)
{
	spatial_min_overlap = 0.0f;
	progress_num_duplicates = 0;
}

BVHBuild::~BVHBuild()
{
}

/* Adding References */

void BVHBuild::add_reference_mesh(NodeSpec& root, Mesh *mesh, int i)
{
	for(uint j = 0; j < mesh->triangles.size(); j++) {
		Mesh::Triangle t = mesh->triangles[j];
		Reference ref;

		for(int k = 0; k < 3; k++) {
			float3 pt = mesh->verts[t.v[k]];
			ref.bounds.grow(pt);
		}

		if(ref.bounds.valid()) {
			ref.prim_index = j;
			ref.prim_object = i;

			references.push_back(ref);
			root.bounds.grow(ref.bounds);
		}
	}
}

void BVHBuild::add_reference_object(NodeSpec& root, Object *ob, int i)
{
	Reference ref;

	ref.prim_index = -1;
	ref.prim_object = i;
	ref.bounds = ob->bounds;

	references.push_back(ref);
	root.bounds.grow(ref.bounds);
}

void BVHBuild::add_references(NodeSpec& root)
{
	/* init root spec */
	root.num = 0;
	root.bounds = BoundBox();

	/* add objects */
	int i = 0;

	foreach(Object *ob, objects) {
		if(params.top_level) {
			if(ob->mesh->transform_applied)
				add_reference_mesh(root, ob->mesh, i);
			else
				add_reference_object(root, ob, i);
		}
		else
			add_reference_mesh(root, ob->mesh, i);

		i++;

		if(progress.get_cancel()) return;
	}

	/* happens mostly on empty meshes */
	if(!root.bounds.valid())
		root.bounds.grow(make_float3(0.0f, 0.0f, 0.0f));

	root.num = references.size();
}

/* Build */

BVHNode* BVHBuild::run()
{
	NodeSpec root;

	/* add references */
	add_references(root);

	if(progress.get_cancel()) return NULL;

	/* init spatial splits */
	if(params.top_level) /* todo: get rid of this */
		params.use_spatial_split = false;

	spatial_min_overlap = root.bounds.area() * params.spatial_split_alpha;
	spatial_right_bounds.clear();
	spatial_right_bounds.resize(max(root.num, (int)BVHParams::NUM_SPATIAL_BINS) - 1);

	/* init progress updates */
	progress_num_duplicates = 0;
	progress_start_time = time_dt();

	/* build recursively */
	return build_node(root, 0, 0.0f, 1.0f);
}

void BVHBuild::progress_update(float progress_start, float progress_end)
{
	if(time_dt() - progress_start_time < 0.25f)
		return;

	float duplicates = (float)progress_num_duplicates/(float)references.size();
	string msg = string_printf("Building BVH %.0f%%, duplicates %.0f%%",
		progress_start*100.0f, duplicates*100.0f);

	progress.set_substatus(msg);
	progress_start_time = time_dt();
}

BVHNode* BVHBuild::build_node(const NodeSpec& spec, int level, float progress_start, float progress_end)
{
	/* progress update */
	progress_update(progress_start, progress_end);
	if(progress.get_cancel()) return NULL;

	/* small enough or too deep => create leaf. */
	if(spec.num <= params.min_leaf_size || level >= BVHParams::MAX_DEPTH)
		return create_leaf_node(spec);

	/* find split candidates. */
	float area = spec.bounds.area();
	float leafSAH = area * params.triangle_cost(spec.num);
	float nodeSAH = area * params.node_cost(2);
	ObjectSplit object = find_object_split(spec, nodeSAH);
	SpatialSplit spatial;

	if(params.use_spatial_split && level < BVHParams::MAX_SPATIAL_DEPTH) {
		BoundBox overlap = object.left_bounds;
		overlap.intersect(object.right_bounds);

		if(overlap.area() >= spatial_min_overlap)
			spatial = find_spatial_split(spec, nodeSAH);
	}

	/* leaf SAH is the lowest => create leaf. */
	float minSAH = min(min(leafSAH, object.sah), spatial.sah);

	if(minSAH == leafSAH && spec.num <= params.max_leaf_size)
		return create_leaf_node(spec);

	/* perform split. */
	NodeSpec left, right;

	if(params.use_spatial_split && minSAH == spatial.sah)
		do_spatial_split(left, right, spec, spatial);
	if(!left.num || !right.num)
		do_object_split(left, right, spec, object);

	/* create inner node. */
	progress_num_duplicates += left.num + right.num - spec.num;

	float progress_mid = lerp(progress_start, progress_end, (float)right.num / (float)(left.num + right.num));

	BVHNode* rightNode = build_node(right, level + 1, progress_start, progress_mid);
	if(progress.get_cancel()) {
		if(rightNode) rightNode->deleteSubtree();
		return NULL;
	}

	BVHNode* leftNode = build_node(left, level + 1, progress_mid, progress_end);
	if(progress.get_cancel()) {
		if(leftNode) leftNode->deleteSubtree();
		return NULL;
	}

	return new InnerNode(spec.bounds, leftNode, rightNode);
}

BVHNode *BVHBuild::create_object_leaf_nodes(const Reference *ref, int num)
{
	if(num == 0) {
		BoundBox bounds;
		return new LeafNode(bounds, 0, 0, 0);
	}
	else if(num == 1) {
		prim_index.push_back(ref[0].prim_index);
		prim_object.push_back(ref[0].prim_object);
		uint visibility = objects[ref[0].prim_object]->visibility;
		return new LeafNode(ref[0].bounds, visibility, prim_index.size()-1, prim_index.size());
	}
	else {
		int mid = num/2;
		BVHNode *leaf0 = create_object_leaf_nodes(ref, mid); 
		BVHNode *leaf1 = create_object_leaf_nodes(ref+mid, num-mid); 

		BoundBox bounds;
		bounds.grow(leaf0->m_bounds);
		bounds.grow(leaf1->m_bounds);

		return new InnerNode(bounds, leaf0, leaf1);
	}
}

BVHNode* BVHBuild::create_leaf_node(const NodeSpec& spec)
{
	vector<int>& p_index = prim_index;
	vector<int>& p_object = prim_object;
	BoundBox bounds;
	int num = 0;
	uint visibility = 0;

	for(int i = 0; i < spec.num; i++) {
		if(references.back().prim_index != -1) {
			p_index.push_back(references.back().prim_index);
			p_object.push_back(references.back().prim_object);
			bounds.grow(references.back().bounds);
			visibility |= objects[references.back().prim_object]->visibility;
			references.pop_back();
			num++;
		}
	}

	BVHNode *leaf = NULL;
	
	if(num > 0) {
		leaf = new LeafNode(bounds, visibility, p_index.size() - num, p_index.size());

		if(num == spec.num)
			return leaf;
	}

	/* while there may be multiple triangles in a leaf, for object primitives
	 * we want them to be the only one, so we  */
	int ob_num = spec.num - num;
	const Reference *ref = (ob_num)? &references.back() - (ob_num - 1): NULL;
	BVHNode *oleaf = create_object_leaf_nodes(ref, ob_num);
	for(int i = 0; i < ob_num; i++)
		references.pop_back();
	
	if(leaf)
		return new InnerNode(spec.bounds, leaf, oleaf);
	else
		return oleaf;
}

/* Object Split */

BVHBuild::ObjectSplit BVHBuild::find_object_split(const NodeSpec& spec, float nodeSAH)
{
	ObjectSplit split;
	const Reference *ref_ptr = &references[references.size() - spec.num];

	for(int dim = 0; dim < 3; dim++) {
		/* sort references */
		bvh_reference_sort(references.size() - spec.num, references.size(), &references[0], dim);

		/* sweep right to left and determine bounds. */
		BoundBox right_bounds;

		for(int i = spec.num - 1; i > 0; i--) {
			right_bounds.grow(ref_ptr[i].bounds);
			spatial_right_bounds[i - 1] = right_bounds;
		}

		/* sweep left to right and select lowest SAH. */
		BoundBox left_bounds;

		for(int i = 1; i < spec.num; i++) {
			left_bounds.grow(ref_ptr[i - 1].bounds);
			right_bounds = spatial_right_bounds[i - 1];

			float sah = nodeSAH +
				left_bounds.area() * params.triangle_cost(i) +
				right_bounds.area() * params.triangle_cost(spec.num - i);

			if(sah < split.sah) {
				split.sah = sah;
				split.dim = dim;
				split.num_left = i;
				split.left_bounds = left_bounds;
				split.right_bounds = right_bounds;
			}
		}
	}

	return split;
}

void BVHBuild::do_object_split(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const ObjectSplit& split)
{
	/* sort references according to split */
	int start = references.size() - spec.num;
	int end = references.size(); /* todo: is this right? */

	bvh_reference_sort(start, end, &references[0], split.dim);

	/* split node specs */
	left.num = split.num_left;
	left.bounds = split.left_bounds;
	right.num = spec.num - split.num_left;
	right.bounds = split.right_bounds;
}

/* Spatial Split */

BVHBuild::SpatialSplit BVHBuild::find_spatial_split(const NodeSpec& spec, float nodeSAH)
{
	/* initialize bins. */
	float3 origin = spec.bounds.min;
	float3 binSize = (spec.bounds.max - origin) * (1.0f / (float)BVHParams::NUM_SPATIAL_BINS);
	float3 invBinSize = 1.0f / binSize;

	for(int dim = 0; dim < 3; dim++) {
		for(int i = 0; i < BVHParams::NUM_SPATIAL_BINS; i++) {
			SpatialBin& bin = spatial_bins[dim][i];

			bin.bounds = BoundBox();
			bin.enter = 0;
			bin.exit = 0;
		}
	}

	/* chop references into bins. */
	for(unsigned int refIdx = references.size() - spec.num; refIdx < references.size(); refIdx++) {
		const Reference& ref = references[refIdx];
		float3 firstBinf = (ref.bounds.min - origin) * invBinSize;
		float3 lastBinf = (ref.bounds.max - origin) * invBinSize;
		int3 firstBin = make_int3((int)firstBinf.x, (int)firstBinf.y, (int)firstBinf.z);
		int3 lastBin = make_int3((int)lastBinf.x, (int)lastBinf.y, (int)lastBinf.z);

		firstBin = clamp(firstBin, 0, BVHParams::NUM_SPATIAL_BINS - 1);
		lastBin = clamp(lastBin, firstBin, BVHParams::NUM_SPATIAL_BINS - 1);

		for(int dim = 0; dim < 3; dim++) {
			Reference currRef = ref;

			for(int i = firstBin[dim]; i < lastBin[dim]; i++) {
				Reference leftRef, rightRef;

				split_reference(leftRef, rightRef, currRef, dim, origin[dim] + binSize[dim] * (float)(i + 1));
				spatial_bins[dim][i].bounds.grow(leftRef.bounds);
				currRef = rightRef;
			}

			spatial_bins[dim][lastBin[dim]].bounds.grow(currRef.bounds);
			spatial_bins[dim][firstBin[dim]].enter++;
			spatial_bins[dim][lastBin[dim]].exit++;
		}
	}

	/* select best split plane. */
	SpatialSplit split;

	for(int dim = 0; dim < 3; dim++) {
		/* sweep right to left and determine bounds. */
		BoundBox right_bounds;

		for(int i = BVHParams::NUM_SPATIAL_BINS - 1; i > 0; i--) {
			right_bounds.grow(spatial_bins[dim][i].bounds);
			spatial_right_bounds[i - 1] = right_bounds;
		}

		/* sweep left to right and select lowest SAH. */
		BoundBox left_bounds;
		int leftNum = 0;
		int rightNum = spec.num;

		for(int i = 1; i < BVHParams::NUM_SPATIAL_BINS; i++) {
			left_bounds.grow(spatial_bins[dim][i - 1].bounds);
			leftNum += spatial_bins[dim][i - 1].enter;
			rightNum -= spatial_bins[dim][i - 1].exit;

			float sah = nodeSAH +
				left_bounds.area() * params.triangle_cost(leftNum) +
				spatial_right_bounds[i - 1].area() * params.triangle_cost(rightNum);

			if(sah < split.sah) {
				split.sah = sah;
				split.dim = dim;
				split.pos = origin[dim] + binSize[dim] * (float)i;
			}
		}
	}

	return split;
}

void BVHBuild::do_spatial_split(NodeSpec& left, NodeSpec& right, const NodeSpec& spec, const SpatialSplit& split)
{
	/* Categorize references and compute bounds.
	 *
	 * Left-hand side:			[left_start, left_end[
	 * Uncategorized/split:		[left_end, right_start[
	 * Right-hand side:			[right_start, refs.size()[ */

	vector<Reference>& refs = references;
	int left_start = refs.size() - spec.num;
	int left_end = left_start;
	int right_start = refs.size();

	left.bounds = right.bounds = BoundBox();

	for(int i = left_end; i < right_start; i++) {
		if(refs[i].bounds.max[split.dim] <= split.pos) {
			/* entirely on the left-hand side */
			left.bounds.grow(refs[i].bounds);
			swap(refs[i], refs[left_end++]);
		}
		else if(refs[i].bounds.min[split.dim] >= split.pos) {
			/* entirely on the right-hand side */
			right.bounds.grow(refs[i].bounds);
			swap(refs[i--], refs[--right_start]);
		}
	}

	/* duplicate or unsplit references intersecting both sides. */
	while(left_end < right_start) {
		/* split reference. */
		Reference lref, rref;

		split_reference(lref, rref, refs[left_end], split.dim, split.pos);

		/* compute SAH for duplicate/unsplit candidates. */
		BoundBox lub = left.bounds;		// Unsplit to left:		new left-hand bounds.
		BoundBox rub = right.bounds;	// Unsplit to right:	new right-hand bounds.
		BoundBox ldb = left.bounds;		// Duplicate:			new left-hand bounds.
		BoundBox rdb = right.bounds;	// Duplicate:			new right-hand bounds.

		lub.grow(refs[left_end].bounds);
		rub.grow(refs[left_end].bounds);
		ldb.grow(lref.bounds);
		rdb.grow(rref.bounds);

		float lac = params.triangle_cost(left_end - left_start);
		float rac = params.triangle_cost(refs.size() - right_start);
		float lbc = params.triangle_cost(left_end - left_start + 1);
		float rbc = params.triangle_cost(refs.size() - right_start + 1);

		float unsplitLeftSAH = lub.area() * lbc + right.bounds.area() * rac;
		float unsplitRightSAH = left.bounds.area() * lac + rub.area() * rbc;
		float duplicateSAH = ldb.area() * lbc + rdb.area() * rbc;
		float minSAH = min(min(unsplitLeftSAH, unsplitRightSAH), duplicateSAH);

		if(minSAH == unsplitLeftSAH) {
			/* unsplit to left */
			left.bounds = lub;
			left_end++;
		}
		else if(minSAH == unsplitRightSAH) {
			/* unsplit to right */
			right.bounds = rub;
			swap(refs[left_end], refs[--right_start]);
		}
		else {
			/* duplicate */
			left.bounds = ldb;
			right.bounds = rdb;
			refs[left_end++] = lref;
			refs.push_back(rref);
		}
	}

	left.num = left_end - left_start;
	right.num = refs.size() - right_start;
}

void BVHBuild::split_reference(Reference& left, Reference& right, const Reference& ref, int dim, float pos)
{
	/* initialize references. */
	left.prim_index = right.prim_index = ref.prim_index;
	left.prim_object = right.prim_object = ref.prim_object;
	left.bounds = right.bounds = BoundBox();

	/* loop over vertices/edges. */
	Object *ob = objects[ref.prim_object];
	const Mesh *mesh = ob->mesh;
	const int *inds = mesh->triangles[ref.prim_index].v;
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
			left.bounds.grow(*v0);

		if(v0p >= pos)
			right.bounds.grow(*v0);

		/* edge intersects the plane => insert intersection to both boxes. */
		if((v0p < pos && v1p > pos) || (v0p > pos && v1p < pos)) {
			float3 t = lerp(*v0, *v1, clamp((pos - v0p) / (v1p - v0p), 0.0f, 1.0f));
			left.bounds.grow(t);
			right.bounds.grow(t);
		}
	}

	/* intersect with original bounds. */
	left.bounds.max[dim] = pos;
	right.bounds.min[dim] = pos;
	left.bounds.intersect(ref.bounds);
	right.bounds.intersect(ref.bounds);
}

CCL_NAMESPACE_END

