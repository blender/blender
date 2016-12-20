/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "opensubdiv_capi.h"

#include <cstdio>
#include <vector>

#ifdef _MSC_VER
#  include "iso646.h"
#endif

#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/osd/cpuEvaluator.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/osd/mesh.h>
#include <opensubdiv/osd/types.h>

#include "opensubdiv_intern.h"

#include "MEM_guardedalloc.h"

using OpenSubdiv::Osd::BufferDescriptor;
using OpenSubdiv::Osd::PatchCoord;
using OpenSubdiv::Far::PatchMap;
using OpenSubdiv::Far::PatchTable;
using OpenSubdiv::Far::PatchTableFactory;
using OpenSubdiv::Far::StencilTable;
using OpenSubdiv::Far::StencilTableFactory;
using OpenSubdiv::Far::TopologyRefiner;

namespace {

/* Helper class to wrap numerous of patch coords into a buffer.
 * Used to pass coordinates to the CPU evaluator. Other evaluators
 * are not supported.
 */
class PatchCoordBuffer : public std::vector<PatchCoord> {
public:
	static PatchCoordBuffer *Create(int size)
	{
		PatchCoordBuffer *buffer = new PatchCoordBuffer();
		buffer->resize(size);
		return buffer;
	}
	PatchCoord *BindCpuBuffer() {
		return (PatchCoord*)&(*this)[0];
	}
	int GetNumVertices() {
		return size();
	}
	void UpdateData(const PatchCoord *patch_coords,
	                int num_patch_coords)
	{
		memcpy(&(*this)[0],
		       (void*)patch_coords,
		       num_patch_coords * sizeof(PatchCoord));
	}
};

/* Helper class to wrap single of patch coord into a buffer.
 * Used to pass coordinates to the CPU evaluator. Other evaluators
 * are not supported.
 */
class SinglePatchCoordBuffer {
public:
	SinglePatchCoordBuffer() {
	}
	SinglePatchCoordBuffer(const PatchCoord& patch_coord)
	        : patch_coord_(patch_coord){
	}
	static SinglePatchCoordBuffer *Create()
	{
		SinglePatchCoordBuffer *buffer = new SinglePatchCoordBuffer();
		return buffer;
	}
	PatchCoord *BindCpuBuffer() {
		return (PatchCoord*)&patch_coord_;
	}
	int GetNumVertices() {
		return 1;
	}
	void UpdateData(const PatchCoord& patch_coord)
	{
		patch_coord_ = patch_coord;
	}
protected:
	PatchCoord patch_coord_;
};

/* Helper class which is aimed to be used in cases when buffer
 * is small enough and better to be allocated in stack rather
 * than in heap.
 *
 * TODO(sergey): Check if bare arrays could be used by CPU evalautor.
 */
template <int element_size, int num_verts>
class StackAllocatedBuffer {
public:
	static PatchCoordBuffer *Create(int /*size*/)
	{
		StackAllocatedBuffer<element_size, num_verts> *buffer =
		        new StackAllocatedBuffer<element_size, num_verts>();
		return buffer;
	}
	float *BindCpuBuffer() {
		return &data_[0];
	}
	int GetNumVertices() {
		return num_verts;
	}
	/* TODO(sergey): Support UpdateData(). */
protected:
	float data_[element_size * num_verts];
};

/* Volatile evaluator which can be used from threads.
 *
 * TODO(sergey): Make it possible to evaluate coordinates in chuncks.
 */
template<typename SRC_VERTEX_BUFFER,
         typename EVAL_VERTEX_BUFFER,
         typename STENCIL_TABLE,
         typename PATCH_TABLE,
         typename EVALUATOR,
         typename DEVICE_CONTEXT = void>
class VolatileEvalOutput {
public:
	typedef OpenSubdiv::Osd::EvaluatorCacheT<EVALUATOR> EvaluatorCache;

	VolatileEvalOutput(const StencilTable *vertex_stencils,
	                   const StencilTable *varying_stencils,
	                   int num_coarse_verts,
	                   int num_total_verts,
	                   const PatchTable *patch_table,
	                   EvaluatorCache *evaluator_cache = NULL,
	                   DEVICE_CONTEXT *device_context = NULL)
	    : src_desc_(        /*offset*/ 0, /*length*/ 3, /*stride*/ 3),
	      src_varying_desc_(/*offset*/ 0, /*length*/ 3, /*stride*/ 3),
	      num_coarse_verts_(num_coarse_verts),
	      evaluator_cache_ (evaluator_cache),
	      device_context_(device_context)
	{
		using OpenSubdiv::Osd::convertToCompatibleStencilTable;
		src_data_ = SRC_VERTEX_BUFFER::Create(3, num_total_verts, device_context_);
		src_varying_data_ = SRC_VERTEX_BUFFER::Create(3, num_total_verts, device_context_);
		patch_table_ = PATCH_TABLE::Create(patch_table, device_context_);
		patch_coords_ = NULL;
		vertex_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(vertex_stencils,
		                                                                  device_context_);
		varying_stencils_ = convertToCompatibleStencilTable<STENCIL_TABLE>(varying_stencils,
		                                                                   device_context_);
	}

	~VolatileEvalOutput()
	{
		delete src_data_;
		delete src_varying_data_;
		delete patch_table_;
		delete vertex_stencils_;
		delete varying_stencils_;
	}

	void UpdateData(const float *src, int start_vertex, int num_vertices)
	{
		src_data_->UpdateData(src, start_vertex, num_vertices, device_context_);
	}

	void UpdateVaryingData(const float *src, int start_vertex, int num_vertices)
	{
		src_varying_data_->UpdateData(src,
		                              start_vertex,
		                              num_vertices,
		                              device_context_);
	}

	void Refine()
	{
		BufferDescriptor dst_desc = src_desc_;
		dst_desc.offset += num_coarse_verts_ * src_desc_.stride;

		const EVALUATOR *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_desc_,
		                                                 dst_desc,
		                                                 device_context_);

		EVALUATOR::EvalStencils(src_data_, src_desc_,
		                        src_data_, dst_desc,
		                        vertex_stencils_,
		                        eval_instance,
		                        device_context_);

		dst_desc = src_varying_desc_;
		dst_desc.offset += num_coarse_verts_ * src_varying_desc_.stride;
		eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_varying_desc_,
		                                                 dst_desc,
		                                                 device_context_);

		EVALUATOR::EvalStencils(src_varying_data_, src_varying_desc_,
		                        src_varying_data_, dst_desc,
		                        varying_stencils_,
		                        eval_instance,
		                        device_context_);
	}

	void EvalPatchCoord(PatchCoord& patch_coord, float P[3])
	{
		StackAllocatedBuffer<6, 1> vertex_data;
		BufferDescriptor vertex_desc(0, 3, 6);
		SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
		const EVALUATOR *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_desc_,
		                                                 vertex_desc,
		                                                 device_context_);
		EVALUATOR::EvalPatches(src_data_, src_desc_,
		                       &vertex_data, vertex_desc,
		                       patch_coord_buffer.GetNumVertices(),
		                       &patch_coord_buffer,
		                       patch_table_, eval_instance, device_context_);
		float *refined_verts = vertex_data.BindCpuBuffer();
		memcpy(P, refined_verts, sizeof(float) * 3);
	}

	void EvalPatchesWithDerivatives(PatchCoord& patch_coord,
	                                float P[3],
	                                float dPdu[3],
	                                float dPdv[3])
	{
		StackAllocatedBuffer<6, 1> vertex_data, derivatives;
		BufferDescriptor vertex_desc(0, 3, 6),
		                 du_desc(0, 3, 6),
		                 dv_desc(3, 3, 6);
		SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
		const EVALUATOR *eval_instance =
		        OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
		                                                 src_desc_,
		                                                 vertex_desc,
		                                                 du_desc,
		                                                 dv_desc,
		                                                 device_context_);
		EVALUATOR::EvalPatches(src_data_, src_desc_,
		                       &vertex_data, vertex_desc,
		                       &derivatives, du_desc,
		                       &derivatives, dv_desc,
		                       patch_coord_buffer.GetNumVertices(),
		                       &patch_coord_buffer,
		                       patch_table_, eval_instance, device_context_);
		float *refined_verts = vertex_data.BindCpuBuffer();
		memcpy(P, refined_verts, sizeof(float) * 3);
		if (dPdu != NULL || dPdv != NULL) {
			float *refined_drivatives = derivatives.BindCpuBuffer();
			if (dPdu) {
				memcpy(dPdu, refined_drivatives, sizeof(float) * 3);
			}
			if (dPdv) {
				memcpy(dPdv, refined_drivatives + 3, sizeof(float) * 3);
			}
		}
	}

	void EvalPatchVarying(PatchCoord& patch_coord,
	                      float varying[3]) {
		StackAllocatedBuffer<3, 1> varying_data;
		BufferDescriptor varying_desc(0, 3, 3);
		SinglePatchCoordBuffer patch_coord_buffer(patch_coord);
		EVALUATOR const *eval_instance =
			OpenSubdiv::Osd::GetEvaluator<EVALUATOR>(evaluator_cache_,
			                                         src_varying_desc_,
			                                         varying_desc,
			                                         device_context_);

		EVALUATOR::EvalPatches(src_varying_data_, src_varying_desc_,
		                       &varying_data, varying_desc,
		                       patch_coord_buffer.GetNumVertices(),
		                       &patch_coord_buffer,
		                       patch_table_, eval_instance, device_context_);
		float *refined_varying = varying_data.BindCpuBuffer();
		memcpy(varying, refined_varying, sizeof(float) * 3);
	}
private:
	SRC_VERTEX_BUFFER *src_data_;
	SRC_VERTEX_BUFFER *src_varying_data_;
	PatchCoordBuffer *patch_coords_;
	PATCH_TABLE *patch_table_;
	BufferDescriptor src_desc_;
	BufferDescriptor src_varying_desc_;
	int num_coarse_verts_;

	const STENCIL_TABLE *vertex_stencils_;
	const STENCIL_TABLE *varying_stencils_;

	EvaluatorCache *evaluator_cache_;
	DEVICE_CONTEXT *device_context_;
};

}  /* namespace */

typedef VolatileEvalOutput<OpenSubdiv::Osd::CpuVertexBuffer,
                           OpenSubdiv::Osd::CpuVertexBuffer,
                           OpenSubdiv::Far::StencilTable,
                           OpenSubdiv::Osd::CpuPatchTable,
                           OpenSubdiv::Osd::CpuEvaluator> CpuEvalOutput;

typedef struct OpenSubdiv_EvaluatorDescr {
	CpuEvalOutput *eval_output;
	const PatchMap *patch_map;
	const PatchTable *patch_table;
} OpenSubdiv_EvaluatorDescr;

OpenSubdiv_EvaluatorDescr *openSubdiv_createEvaluatorDescr(
        OpenSubdiv_TopologyRefinerDescr *topology_refiner,
        int subsurf_level)
{
	/* TODO(sergey): Look into re-using refiner with GLMesh. */
	TopologyRefiner *refiner = (TopologyRefiner *)topology_refiner;
	if(refiner == NULL) {
		/* Happens on bad topology. */
		return NULL;
	}

	const StencilTable *vertex_stencils = NULL;
	const StencilTable *varying_stencils = NULL;
	int num_total_verts = 0;

	/* Apply uniform refinement to the mesh so that we can use the
	 * limit evaluation API features.
	 */
	TopologyRefiner::UniformOptions options(subsurf_level);
	refiner->RefineUniform(options);

	/* Generate stencil table to update the bi-cubic patches control
	 * vertices after they have been re-posed (both for vertex & varying
	 * interpolation).
	 */
	StencilTableFactory::Options soptions;
	soptions.generateOffsets = true;
	soptions.generateIntermediateLevels = false;

	vertex_stencils = StencilTableFactory::Create(*refiner, soptions);

	soptions.interpolationMode = StencilTableFactory::INTERPOLATE_VARYING;
	varying_stencils = StencilTableFactory::Create(*refiner, soptions);

	/* Generate bi-cubic patch table for the limit surface. */
	PatchTableFactory::Options poptions;
	poptions.SetEndCapType(PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS);

	const PatchTable *patch_table = PatchTableFactory::Create(*refiner, poptions);

	/* Append local points stencils. */
	/* TODO(sergey): Do we really need to worry about local points stencils? */
	if (const StencilTable *local_point_stencil_table =
	    patch_table->GetLocalPointStencilTable())
	{
		const StencilTable *table =
			StencilTableFactory::AppendLocalPointStencilTable(*refiner,
			                                                  vertex_stencils,
			                                                  local_point_stencil_table);
		delete vertex_stencils;
		vertex_stencils = table;
	}
	if (const StencilTable *local_point_varying_stencil_table =
	     patch_table->GetLocalPointVaryingStencilTable())
	{
		const StencilTable *table =
			StencilTableFactory::AppendLocalPointStencilTable(*refiner,
			                                                  varying_stencils,
			                                                  local_point_varying_stencil_table);
		delete varying_stencils;
		varying_stencils = table;
	}

	/* Total number of vertices = coarse verts + refined verts + gregory basis verts. */
	num_total_verts = vertex_stencils->GetNumControlVertices() +
		vertex_stencils->GetNumStencils();

	const int num_coarse_verts = refiner->GetLevel(0).GetNumVertices();

	CpuEvalOutput *eval_output = new CpuEvalOutput(vertex_stencils,
	                                               varying_stencils,
	                                               num_coarse_verts,
	                                               num_total_verts,
	                                               patch_table);

	OpenSubdiv::Far::PatchMap *patch_map = new PatchMap(*patch_table);

	OpenSubdiv_EvaluatorDescr *evaluator_descr;
	evaluator_descr = OBJECT_GUARDED_NEW(OpenSubdiv_EvaluatorDescr);
	evaluator_descr->eval_output = eval_output;
	evaluator_descr->patch_map = patch_map;
	evaluator_descr->patch_table = patch_table;

	/* TOOD(sergey): Look into whether w've got duplicated stencils arrays. */
	delete varying_stencils;
	delete vertex_stencils;

	delete refiner;

	return evaluator_descr;
}

void openSubdiv_deleteEvaluatorDescr(OpenSubdiv_EvaluatorDescr *evaluator_descr)
{
	delete evaluator_descr->eval_output;
	delete evaluator_descr->patch_map;
	delete evaluator_descr->patch_table;
	OBJECT_GUARDED_DELETE(evaluator_descr, OpenSubdiv_EvaluatorDescr);
}

void openSubdiv_setEvaluatorCoarsePositions(OpenSubdiv_EvaluatorDescr *evaluator_descr,
                                            float *positions,
                                            int start_vert,
                                            int num_verts)
{
	/* TODO(sergey): Add sanity check on indices. */
	evaluator_descr->eval_output->UpdateData(positions, start_vert, num_verts);
	/* TODO(sergey): Consider moving this to a separate call,
	 * so we can updatwe coordinates in chunks.
	 */
	evaluator_descr->eval_output->Refine();
}

void openSubdiv_setEvaluatorVaryingData(OpenSubdiv_EvaluatorDescr *evaluator_descr,
                                        float *varying_data,
                                        int start_vert,
                                        int num_verts)
{
	/* TODO(sergey): Add sanity check on indices. */
	evaluator_descr->eval_output->UpdateVaryingData(varying_data, start_vert, num_verts);
	/* TODO(sergey): Get rid of this ASAP. */
	evaluator_descr->eval_output->Refine();
}

void openSubdiv_evaluateLimit(OpenSubdiv_EvaluatorDescr *evaluator_descr,
                              int osd_face_index,
                              float face_u, float face_v,
                              float P[3],
                              float dPdu[3],
                              float dPdv[3])
{
	assert((face_u >= 0.0f) && (face_u <= 1.0f) && (face_v >= 0.0f) && (face_v <= 1.0f));
	const PatchTable::PatchHandle *handle =
	        evaluator_descr->patch_map->FindPatch(osd_face_index, face_u, face_v);
	PatchCoord patch_coord(*handle, face_u, face_v);
	if (dPdu != NULL || dPdv != NULL) {
		evaluator_descr->eval_output->EvalPatchesWithDerivatives(patch_coord,
		                                                         P,
		                                                         dPdu,
		                                                         dPdv);
	}
	else {
		evaluator_descr->eval_output->EvalPatchCoord(patch_coord, P);
	}
}

void openSubdiv_evaluateVarying(OpenSubdiv_EvaluatorDescr *evaluator_descr,
                               int osd_face_index,
                               float face_u, float face_v,
                               float varying[3])
{
	assert((face_u >= 0.0f) && (face_u <= 1.0f) && (face_v >= 0.0f) && (face_v <= 1.0f));
	const PatchTable::PatchHandle *handle =
	        evaluator_descr->patch_map->FindPatch(osd_face_index, face_u, face_v);
	PatchCoord patch_coord(*handle, face_u, face_v);
	evaluator_descr->eval_output->EvalPatchVarying(patch_coord, varying);
}
