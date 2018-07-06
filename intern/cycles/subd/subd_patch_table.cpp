/*
 * Based on code from OpenSubdiv released under this license:
 *
 * Copyright 2014 DreamWorks Animation LLC.
 *
 * Licensed under the Apache License, Version 2.0 (the "Apache License")
 * with the following modification; you may not use this file except in
 * compliance with the Apache License and the following modification to it:
 * Section 6. Trademarks. is deleted and replaced with:
 *
 * 6. Trademarks. This License does not grant permission to use the trade
 *   names, trademarks, service marks, or product names of the Licensor
 *   and its affiliates, except as required to comply with Section 4(c) of
 *   the License and to reproduce the content of the NOTICE file.
 *
 * You may obtain a copy of the Apache License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Apache License with the above modification is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the Apache License for the specific
 * language governing permissions and limitations under the Apache License.
 *
 */

#include "subd/subd_patch_table.h"
#include "kernel/kernel_types.h"

#include "util/util_math.h"

#ifdef WITH_OPENSUBDIV
#include <opensubdiv/far/patchTable.h>
#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENSUBDIV

using namespace OpenSubdiv;

/* functions for building patch maps */

struct PatchMapQuadNode {
	/* sets all the children to point to the patch of index */
	void set_child(int index)
	{
		for(int i = 0; i < 4; i++) {
			children[i] = index | PATCH_MAP_NODE_IS_SET | PATCH_MAP_NODE_IS_LEAF;
		}
	}

	/* sets the child in quadrant to point to the node or patch of the given index */
	void set_child(unsigned char quadrant, int index, bool is_leaf=true)
	{
		assert(quadrant < 4);
		children[quadrant] = index | PATCH_MAP_NODE_IS_SET | (is_leaf ? PATCH_MAP_NODE_IS_LEAF : 0);
	}

	uint children[4];
};

template<class T>
static int resolve_quadrant(T& median, T& u, T& v)
{
	int quadrant = -1;

	if(u < median) {
		if(v < median) {
			quadrant = 0;
		}
		else {
			quadrant = 1;
			v -= median;
		}
	}
	else {
		if(v < median) {
			quadrant = 3;
		}
		else {
			quadrant = 2;
			v -= median;
		}
		u -= median;
	}

	return quadrant;
}

static void build_patch_map(PackedPatchTable& table, OpenSubdiv::Far::PatchTable* patch_table, int offset)
{
	int num_faces = 0;

	for(int array = 0; array < table.num_arrays; array++) {
		Far::ConstPatchParamArray params = patch_table->GetPatchParams(array);

		for(int j = 0; j < patch_table->GetNumPatches(array); j++) {
			num_faces = max(num_faces, (int)params[j].GetFaceId());
		}
	}
	num_faces++;

	vector<PatchMapQuadNode> quadtree;
	quadtree.reserve(num_faces + table.num_patches);
	quadtree.resize(num_faces);

	/* adjust offsets to make indices relative to the table */
	int handle_index = -(table.num_patches * PATCH_HANDLE_SIZE);
	offset += table.total_size();

	/* populate the quadtree from the FarPatchArrays sub-patches */
	for(int array = 0; array < table.num_arrays; array++) {
		Far::ConstPatchParamArray params = patch_table->GetPatchParams(array);

		for(int i = 0; i < patch_table->GetNumPatches(array); i++, handle_index += PATCH_HANDLE_SIZE) {
			const Far::PatchParam& param = params[i];
			unsigned short depth = param.GetDepth();

			PatchMapQuadNode* node = &quadtree[params[i].GetFaceId()];

			if(depth == (param.NonQuadRoot() ? 1 : 0)) {
				/* special case : regular BSpline face w/ no sub-patches */
				node->set_child(handle_index + offset);
				continue;
			}

			int u = param.GetU();
			int v = param.GetV();
			int pdepth = param.NonQuadRoot() ? depth-2 : depth-1;
			int half = 1 << pdepth;

			for(int j = 0; j < depth; j++) {
				int delta = half >> 1;

				int quadrant = resolve_quadrant(half, u, v);
				assert(quadrant >= 0);

				half = delta;

				if(j == pdepth) {
					/* we have reached the depth of the sub-patch : add a leaf */
					assert(!(node->children[quadrant] & PATCH_MAP_NODE_IS_SET));
					node->set_child(quadrant, handle_index + offset, true);
					break;
				}
				else {
					/* travel down the child node of the corresponding quadrant */
					if(!(node->children[quadrant] & PATCH_MAP_NODE_IS_SET)) {
						/* create a new branch in the quadrant */
						quadtree.push_back(PatchMapQuadNode());

						int idx = (int)quadtree.size() - 1;
						node->set_child(quadrant, idx*4 + offset, false);

						node = &quadtree[idx];
					}
					else {
						/* travel down an existing branch */
						uint idx = node->children[quadrant] & PATCH_MAP_NODE_INDEX_MASK;
						node = &(quadtree[(idx - offset)/4]);
					}
				}
			}
		}
	}

	/* copy into table */
	assert(table.table.size() == table.total_size());
	uint map_offset = table.total_size();

	table.num_nodes = quadtree.size() * 4;
	table.table.resize(table.total_size());

	uint* data = &table.table[map_offset];

	for(int i = 0; i < quadtree.size(); i++) {
		for(int j = 0; j < 4; j++) {
			assert(quadtree[i].children[j] & PATCH_MAP_NODE_IS_SET);
			*(data++) = quadtree[i].children[j];
		}
	}
}

#endif

/* packed patch table functions */

size_t PackedPatchTable::total_size()
{
	return num_arrays * PATCH_ARRAY_SIZE +
		   num_indices +
		   num_patches * (PATCH_PARAM_SIZE + PATCH_HANDLE_SIZE) +
		   num_nodes * PATCH_NODE_SIZE;
}

void PackedPatchTable::pack(Far::PatchTable* patch_table, int offset)
{
	num_arrays = 0;
	num_patches = 0;
	num_indices = 0;
	num_nodes = 0;

#ifdef WITH_OPENSUBDIV
	num_arrays = patch_table->GetNumPatchArrays();

	for(int i = 0; i < num_arrays; i++) {
		int patches = patch_table->GetNumPatches(i);
		int num_control = patch_table->GetPatchArrayDescriptor(i).GetNumControlVertices();

		num_patches += patches;
		num_indices += patches * num_control;
	}

	table.resize(total_size());
	uint* data = table.data();

	uint* array = data;
	uint* index = array + num_arrays * PATCH_ARRAY_SIZE;
	uint* param = index + num_indices;
	uint* handle = param + num_patches * PATCH_PARAM_SIZE;

	uint current_param = 0;

	for(int i = 0; i < num_arrays; i++) {
		*(array++) = patch_table->GetPatchArrayDescriptor(i).GetType();
		*(array++) = patch_table->GetNumPatches(i);
		*(array++) = (index - data) + offset;
		*(array++) = (param - data) + offset;

		Far::ConstIndexArray indices = patch_table->GetPatchArrayVertices(i);

		for(int j = 0; j < indices.size(); j++) {
			*(index++) = indices[j];
		}

		const Far::PatchParamTable& param_table = patch_table->GetPatchParamTable();

		int num_control = patch_table->GetPatchArrayDescriptor(i).GetNumControlVertices();
		int patches = patch_table->GetNumPatches(i);

		for(int j = 0; j < patches; j++, current_param++) {
			*(param++) = param_table[current_param].field0;
			*(param++) = param_table[current_param].field1;

			*(handle++) = (array - data) - PATCH_ARRAY_SIZE + offset;
			*(handle++) = (param - data) - PATCH_PARAM_SIZE + offset;
			*(handle++) = j * num_control;
		}
	}

	build_patch_map(*this, patch_table, offset);
#else
	(void)patch_table;
	(void)offset;
#endif
}

void PackedPatchTable::copy_adjusting_offsets(uint* dest, int doffset)
{
	uint* src = table.data();

	/* arrays */
	for(int i = 0; i < num_arrays; i++) {
		*(dest++) = *(src++);
		*(dest++) = *(src++);
		*(dest++) = *(src++) + doffset;
		*(dest++) = *(src++) + doffset;
	}

	/* indices */
	for(int i = 0; i < num_indices; i++) {
		*(dest++) = *(src++);
	}

	/* params */
	for(int i = 0; i < num_patches; i++) {
		*(dest++) = *(src++);
		*(dest++) = *(src++);
	}

	/* handles */
	for(int i = 0; i < num_patches; i++) {
		*(dest++) = *(src++) + doffset;
		*(dest++) = *(src++) + doffset;
		*(dest++) = *(src++);
	}

	/* nodes */
	for(int i = 0; i < num_nodes; i++) {
		*(dest++) = *(src++) + doffset;
	}
}

CCL_NAMESPACE_END
