/*
 * Original code in the public domain -- castanyo@yahoo.es
 * 
 * Modifications copyright (c) 2011, Blender Foundation.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>

#include "subd_mesh.h"
#include "subd_patch.h"
#include "subd_split.h"

#include "util_debug.h"
#include "util_foreach.h"

#ifdef WITH_OPENSUBDIV

#include <osd/vertex.h>
#include <osd/mesh.h>
#include <osd/cpuComputeController.h>
#include <osd/cpuVertexBuffer.h>
#include <osd/cpuEvalLimitController.h>
#include <osd/evalLimitContext.h>

CCL_NAMESPACE_BEGIN

/* typedefs */
typedef OpenSubdiv::OsdVertex OsdVertex;
typedef OpenSubdiv::FarMesh<OsdVertex> OsdFarMesh;
typedef OpenSubdiv::FarMeshFactory<OsdVertex> OsdFarMeshFactory;
typedef OpenSubdiv::HbrCatmarkSubdivision<OsdVertex> OsdHbrCatmarkSubdivision;
typedef OpenSubdiv::HbrFace<OsdVertex> OsdHbrFace;
typedef OpenSubdiv::HbrHalfedge<OsdVertex> OsdHbrHalfEdge;
typedef OpenSubdiv::HbrMesh<OsdVertex> OsdHbrMesh;
typedef OpenSubdiv::HbrVertex<OsdVertex> OsdHbrVertex;
typedef OpenSubdiv::OsdCpuComputeContext OsdCpuComputeContext;
typedef OpenSubdiv::OsdCpuComputeController OsdCpuComputeController;
typedef OpenSubdiv::OsdCpuEvalLimitContext OsdCpuEvalLimitContext;
typedef OpenSubdiv::OsdCpuEvalLimitController OsdCpuEvalLimitController;
typedef OpenSubdiv::OsdCpuVertexBuffer OsdCpuVertexBuffer;
typedef OpenSubdiv::OsdEvalCoords OsdEvalCoords;
typedef OpenSubdiv::OsdVertexBufferDescriptor OsdVertexBufferDescriptor;

/* OpenSubdiv Patch */

class OpenSubdPatch : public Patch {
public:
	int face_id;

	OpenSubdPatch(OsdFarMesh *farmesh, OsdCpuVertexBuffer *vbuf_base)
	{
		face_id = 0;

		/* create buffers for evaluation */
		vbuf_P = OsdCpuVertexBuffer::Create(3, 1);
		vbuf_dPdu = OsdCpuVertexBuffer::Create(3, 1);
		vbuf_dPdv = OsdCpuVertexBuffer::Create(3, 1);

		P = vbuf_P->BindCpuBuffer();
		dPdu = vbuf_dPdu->BindCpuBuffer();
		dPdv = vbuf_dPdv->BindCpuBuffer();

		/* setup evaluation context */
		OsdVertexBufferDescriptor in_desc(0, 3, 3), out_desc(0, 3, 3); /* offset, length, stride */

		evalctx = OsdCpuEvalLimitContext::Create(farmesh, false);
		evalctx->GetVertexData().Bind(in_desc, vbuf_base, out_desc, vbuf_P, vbuf_dPdu, vbuf_dPdv);
	}

	~OpenSubdPatch()
	{
		evalctx->GetVertexData().Unbind();

		delete evalctx;
		delete vbuf_P;
		delete vbuf_dPdu;
		delete vbuf_dPdv;
	}

	void eval(float3 *P_, float3 *dPdu_, float3 *dPdv_, float u, float v)
	{
		OsdEvalCoords coords;
		coords.u = u;
		coords.v = v;
		coords.face = face_id;

		evalctrl.EvalLimitSample<OsdCpuVertexBuffer,OsdCpuVertexBuffer>(coords, evalctx, 0);

		*P_ = make_float3(P[0], P[1], P[2]);
		if (dPdu_) *dPdu_ = make_float3(dPdv[0], dPdv[1], dPdv[2]);
		if (dPdv_) *dPdv_ = make_float3(dPdu[0], dPdu[1], dPdu[2]);

		/* optimize: skip evaluating derivatives when not needed */
		/* todo: swapped derivatives, different winding convention? */
	}

	BoundBox bound()
	{
		/* not implemented */
		BoundBox bbox = BoundBox::empty;
		return bbox;
	}

	int ptex_face_id()
	{
		return face_id;
	}

protected:
	OsdCpuEvalLimitController evalctrl;
	OsdCpuEvalLimitContext *evalctx;
	OsdCpuVertexBuffer *vbuf_P;
	OsdCpuVertexBuffer *vbuf_dPdu;
	OsdCpuVertexBuffer *vbuf_dPdv;
	float *P;
	float *dPdu;
	float *dPdv;
};

/* OpenSubdiv Mesh */

OpenSubdMesh::OpenSubdMesh()
{
	/* create osd mesh */
	static OsdHbrCatmarkSubdivision	catmark;
	OsdHbrMesh *hbrmesh = new OsdHbrMesh(&catmark);

	/* initialize class */
	num_verts = 0;
	num_ptex_faces = 0;
	_hbrmesh = (void*)hbrmesh;
}

OpenSubdMesh::~OpenSubdMesh()
{
	OsdHbrMesh *hbrmesh = (OsdHbrMesh*)_hbrmesh;

	if(hbrmesh)
		delete hbrmesh;
}

void OpenSubdMesh::add_vert(const float3& co)
{
	OsdHbrMesh *hbrmesh = (OsdHbrMesh*)_hbrmesh;

	OsdVertex v;
	positions.push_back(co.x);
	positions.push_back(co.y);
	positions.push_back(co.z);
	hbrmesh->NewVertex(num_verts++, v);
}

void OpenSubdMesh::add_face(int v0, int v1, int v2)
{
	int index[3] = {v0, v1, v2};
	return add_face(index, 3);
}

void OpenSubdMesh::add_face(int v0, int v1, int v2, int v3)
{
	int index[4] = {v0, v1, v2, v3};
	add_face(index, 4);
}

void OpenSubdMesh::add_face(int *index, int num)
{
	OsdHbrMesh *hbrmesh = (OsdHbrMesh*)_hbrmesh;

#ifndef NDEBUG
	/* sanity checks */
	for(int j = 0; j < num; j++) {
		OsdHbrVertex *origin = hbrmesh->GetVertex(index[j]);
		OsdHbrVertex *destination = hbrmesh->GetVertex(index[(j+1)%num]);
		OsdHbrHalfEdge *opposite = destination->GetEdge(origin);

		if(origin==NULL || destination==NULL)
			assert("An edge was specified that connected a nonexistent vertex\n");

		if(origin == destination)
			assert("An edge was specified that connected a vertex to itself\n");

		if(opposite && opposite->GetOpposite())
			assert("A non-manifold edge incident to more than 2 faces was found\n");

		if(origin->GetEdge(destination))
			assert("An edge connecting two vertices was specified more than once."
				 "It's likely that an incident face was flipped\n");
	}
#endif

	OsdHbrFace *face = hbrmesh->NewFace(num, index, 0);

	/* this is required for limit eval patch table? */
	face->SetPtexIndex(num_ptex_faces);

	if(num == 4)
		num_ptex_faces++;
	else
		num_ptex_faces += num;
}

bool OpenSubdMesh::finish()
{
	OsdHbrMesh *hbrmesh = (OsdHbrMesh*)_hbrmesh;

	/* finish hbr mesh construction */
	hbrmesh->SetInterpolateBoundaryMethod(OsdHbrMesh::k_InterpolateBoundaryEdgeOnly);
	hbrmesh->Finish();

	return true;
}

void OpenSubdMesh::tessellate(DiagSplit *split)
{
	if (num_ptex_faces == 0)
		return;

	const int level = 3;
	const bool requirefvar = false;

	/* convert HRB to FAR mesh */
	OsdHbrMesh *hbrmesh = (OsdHbrMesh*)_hbrmesh;

	OsdFarMeshFactory meshFactory(hbrmesh, level, true);
	OsdFarMesh *farmesh = meshFactory.Create(requirefvar);
	int num_hbr_verts = hbrmesh->GetNumVertices();

	delete hbrmesh;
	hbrmesh = NULL;
	_hbrmesh = NULL;

	/* refine HBR mesh with vertex coordinates */
	OsdCpuComputeController *compute_controller = new OsdCpuComputeController();
	OsdCpuComputeContext *compute_context = OsdCpuComputeContext::Create(farmesh);

	OsdCpuVertexBuffer *vbuf_base = OsdCpuVertexBuffer::Create(3, num_hbr_verts);
	vbuf_base->UpdateData(&positions[0], 0, num_verts);

	compute_controller->Refine(compute_context, farmesh->GetKernelBatches(), vbuf_base);
	compute_controller->Synchronize();

	/* split & dice patches */
	OpenSubdPatch patch(farmesh, vbuf_base);

	for(int f = 0; f < num_ptex_faces; f++) {
		patch.face_id = f;
		split->split_quad(&patch);
	}

	/* clean up */
	delete farmesh;
	delete compute_controller;
	delete compute_context;
	delete vbuf_base;
}

CCL_NAMESPACE_END

#else /* WITH_OPENSUBDIV */

CCL_NAMESPACE_BEGIN

/* Subd Vertex */

class SubdVert
{
public:
	int id;
	float3 co;
	
	SubdVert(int id_)
	{
		id = id_;
		co = make_float3(0.0f, 0.0f, 0.0f);
	}
};

/* Subd Face */

class SubdFace
{
public:
	int id;
	int numverts;
	int verts[4];

	SubdFace(int id_)
	{
		id = id_;
		numverts = 0;
	}
};

/* Subd Mesh */

SubdMesh::SubdMesh()
{
}

SubdMesh::~SubdMesh()
{
	foreach(SubdVert *vertex, verts)
		delete vertex;
	foreach(SubdFace *face, faces)
		delete face;

	verts.clear();
	faces.clear();
}

SubdVert *SubdMesh::add_vert(const float3& co)
{
	SubdVert *v = new SubdVert(verts.size());
	v->co = co;
	verts.push_back(v);

	return v;
}

SubdFace *SubdMesh::add_face(int v0, int v1, int v2)
{
	int index[3] = {v0, v1, v2};
	return add_face(index, 3);
}

SubdFace *SubdMesh::add_face(int v0, int v1, int v2, int v3)
{
	int index[4] = {v0, v1, v2, v3};
	return add_face(index, 4);
}

SubdFace *SubdMesh::add_face(int *index, int num)
{
	/* skip ngons */
	if(num < 3 || num > 4)
		return NULL;

	SubdFace *f = new SubdFace(faces.size());

	for(int i = 0; i < num; i++)
		f->verts[i] = index[i];

	f->numverts = num;
	faces.push_back(f);

	return f;
}

bool SubdMesh::finish()
{
	return true;
}

void SubdMesh::tessellate(DiagSplit *split)
{
	int num_faces = faces.size();
		        
	for(int f = 0; f < num_faces; f++) {
		SubdFace *face = faces[f];
		Patch *patch;
		float3 *hull;

		if(face->numverts == 3) {
			LinearTrianglePatch *lpatch = new LinearTrianglePatch();
			hull = lpatch->hull;
			patch = lpatch;
		}
		else if(face->numverts == 4) {
			LinearQuadPatch *lpatch = new LinearQuadPatch();
			hull = lpatch->hull;
			patch = lpatch;
		}
		else {
			assert(0); /* n-gons should have been split already */
			continue;
		}

		for(int i = 0; i < face->numverts; i++)
			hull[i] = verts[face->verts[i]]->co;

		if(face->numverts == 4)
			swap(hull[2], hull[3]);

		if(patch->is_triangle())
			split->split_triangle(patch);
		else
			split->split_quad(patch);

		delete patch;
	}
}

CCL_NAMESPACE_END

#endif /* WITH_OPENSUBDIV */

