
#ifndef CSG_Iterator_H
#define CSG_Iterator_H

#include "CSG_BlenderMesh.h"
#include "CSG_Interface.h"

#include "MEM_SmartPtr.h"
/**
 * This class defines 2 C style iterators over a CSG mesh, one for
 * vertices and 1 for faces. They conform to the iterator interface
 * defined in CSG_BooleanOps.h
 */

struct AMesh_VertexIt {
	AMesh* mesh;
	AMesh::VLIST::const_iterator pos;
};


static
	void
AMesh_VertexIt_Destruct(
	CSG_VertexIteratorDescriptor * vIterator
) {
	delete ((AMesh_VertexIt *)(vIterator->it));
	vIterator->it = NULL;
	vIterator->Done = NULL;
	vIterator->Fill = NULL;
	vIterator->Reset = NULL;
	vIterator->Step = NULL;
	vIterator->num_elements = 0;
};


static
	int
AMesh_VertexIt_Done(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_VertexIt * vertex_it = (AMesh_VertexIt *)it;

	if (vertex_it->pos < vertex_it->mesh->Verts().end() ) return 0;
	return 1;
};

static
	void
AMesh_VertexIt_Fill(
	CSG_IteratorPtr it,
	CSG_IVertex *vert
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_VertexIt * vertex_it = (AMesh_VertexIt *)it;
			
	vertex_it->pos->Pos().getValue(vert->position);
};

static
	void
AMesh_VertexIt_Step(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_VertexIt * vertex_it = (AMesh_VertexIt *)it;

	++(vertex_it->pos);
};

static
	void
AMesh_VertexIt_Reset(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_VertexIt * vertex_it = (AMesh_VertexIt *)it;
	vertex_it->pos = vertex_it->mesh->Verts().begin();
};	

static
	void
AMesh_VertexIt_Construct(
	AMesh *mesh,
	CSG_VertexIteratorDescriptor *output
){
	// user should have insured mesh is not equal to NULL.
	
	output->Done = AMesh_VertexIt_Done;
	output->Fill = AMesh_VertexIt_Fill;
	output->Step = AMesh_VertexIt_Step;
	output->Reset = AMesh_VertexIt_Reset;
	output->num_elements = mesh->Verts().size();
	
	AMesh_VertexIt * v_it = new AMesh_VertexIt;
	v_it->mesh = mesh;
	v_it->pos = mesh->Verts().begin();
	output->it = v_it;
};			


/**
 * Face iterator.
 */

struct AMesh_FaceIt {
	AMesh* mesh;
	AMesh::PLIST::const_iterator pos;
};


static
	void
AMesh_FaceIt_Destruct(
	CSG_FaceIteratorDescriptor *fIterator
) {
	delete ((AMesh_FaceIt *)(fIterator->it));
	fIterator->it = NULL;
	fIterator->Done = NULL;
	fIterator->Fill = NULL;
	fIterator->Reset = NULL;
	fIterator->Step = NULL;
	fIterator->num_elements = 0;
};


static
	int
AMesh_FaceIt_Done(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_FaceIt * face_it = (AMesh_FaceIt *)it;

	return face_it->pos >= face_it->mesh->Polys().end();
};

static
	void
AMesh_FaceIt_Fill(
	CSG_IteratorPtr it,
	CSG_IFace *face
){
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_FaceIt * face_it = (AMesh_FaceIt *)it;		

	face->m_vertexData[0] = face_it->pos->VertexProps(0).Data();
	face->m_vertexData[1] = face_it->pos->VertexProps(1).Data();
	face->m_vertexData[2] = face_it->pos->VertexProps(2).Data();

	face->m_vertexNumber =3;
	face->m_faceData = face_it->pos->FProp();
};

static
	void
AMesh_FaceIt_Step(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_FaceIt * face_it = (AMesh_FaceIt *)it;		
	face_it->pos++;
};

static
	void
AMesh_FaceIt_Reset(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	AMesh_FaceIt * f_it = (AMesh_FaceIt *)it;		
	f_it->pos = f_it->mesh->Polys().begin();
};

static
	void
AMesh_FaceIt_Construct(
	AMesh * mesh,
	CSG_FaceIteratorDescriptor *output
) {

	output->Done = AMesh_FaceIt_Done;
	output->Fill = AMesh_FaceIt_Fill;
	output->Step = AMesh_FaceIt_Step;
	output->Reset = AMesh_FaceIt_Reset;

	output->num_elements = mesh->Polys().size();
	
	AMesh_FaceIt * f_it = new AMesh_FaceIt;
	f_it->mesh = mesh;
	f_it->pos =  mesh->Polys().begin();

	output->it = f_it;

};


#endif

