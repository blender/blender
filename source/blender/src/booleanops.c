/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * CSG operations. 
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "CSG_BooleanOps.h"

#include "BKE_booleanops.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_toolbox.h"

#include "BDR_editface.h"

#include <math.h>

/**
 * Here's the vertex iterator structure used to walk through
 * the blender vertex structure.
 */

typedef struct {
	Mesh *mesh;
	Object *ob;
	int pos;
} VertexIt;

/**
 * Implementations of local vertex iterator functions.
 * These describe a blender mesh to the CSG module.
 */

static void VertexIt_Destruct(CSG_VertexIteratorDescriptor * iterator)
{
	if (iterator->it) {
		// deallocate memory for iterator
		MEM_freeN(iterator->it);
		iterator->it = 0;
	}
	iterator->Done = NULL;
	iterator->Fill = NULL;
	iterator->Reset = NULL;
	iterator->Step = NULL;
	iterator->num_elements = 0;

}		

static int VertexIt_Done(CSG_IteratorPtr it)
{
	VertexIt * iterator = (VertexIt *)it;
	return(iterator->pos >= iterator->mesh->totvert);
}

static void VertexIt_Fill(CSG_IteratorPtr it, CSG_IVertex *vert)
{
	VertexIt * iterator = (VertexIt *)it;
	MVert *verts = iterator->mesh->mvert;

	float global_pos[3];

	/* boolean happens in global space, transform both with obmat */
	VecMat4MulVecfl(
		global_pos,
		iterator->ob->obmat, 
		verts[iterator->pos].co
	);

	vert->position[0] = global_pos[0];
	vert->position[1] = global_pos[1];
	vert->position[2] = global_pos[2];
}

static void VertexIt_Step(CSG_IteratorPtr it)
{
	VertexIt * iterator = (VertexIt *)it;
	iterator->pos ++;
} 
 
static void VertexIt_Reset(CSG_IteratorPtr it)
{
	VertexIt * iterator = (VertexIt *)it;
	iterator->pos = 0;
}

static void VertexIt_Construct(CSG_VertexIteratorDescriptor *output, Object *ob)
{

	VertexIt *it;
	if (output == 0) return;

	// allocate some memory for blender iterator
	it = (VertexIt *)(MEM_mallocN(sizeof(VertexIt),"Boolean_VIt"));
	if (it == 0) {
		return;
	}
	// assign blender specific variables
	it->ob = ob;
	it->mesh = ob->data;
	
	it->pos = 0;

 	// assign iterator function pointers.
	output->Step = VertexIt_Step;
	output->Fill = VertexIt_Fill;
	output->Done = VertexIt_Done;
	output->Reset = VertexIt_Reset;
	output->num_elements = it->mesh->totvert;
	output->it = it;
}

/**
 * Blender Face iterator
 */

typedef struct {
	Mesh *mesh;
	int pos;
	int offset;
} FaceIt;

static void FaceIt_Destruct(CSG_FaceIteratorDescriptor * iterator)
{
	MEM_freeN(iterator->it);
	iterator->Done = NULL;
	iterator->Fill = NULL;
	iterator->Reset = NULL;
	iterator->Step = NULL;
	iterator->num_elements = 0;
}

static int FaceIt_Done(CSG_IteratorPtr it)
{
	// assume CSG_IteratorPtr is of the correct type.
	FaceIt * iterator = (FaceIt *)it;
	return(iterator->pos >= iterator->mesh->totface);
}

static void FaceIt_Fill(CSG_IteratorPtr it, CSG_IFace *face)
{
	// assume CSG_IteratorPtr is of the correct type.
	FaceIt *face_it = (FaceIt *)it;
	MFace *mfaces = face_it->mesh->mface;
	MFace *mface = &mfaces[face_it->pos];

	face->vertex_index[0] = mface->v1;
	face->vertex_index[1] = mface->v2;
	face->vertex_index[2] = mface->v3;
	if (mface->v4) {
		face->vertex_index[3] = mface->v4;
		face->vertex_number = 4;
	} else {
		face->vertex_number = 3;
	}

	face->orig_face = face_it->offset + face_it->pos;
}

static void FaceIt_Step(CSG_IteratorPtr it)
{
	FaceIt * face_it = (FaceIt *)it;		
	face_it->pos ++;
}

static void FaceIt_Reset(CSG_IteratorPtr it)
{
	FaceIt * face_it = (FaceIt *)it;		
	face_it->pos = 0;
}	

static void FaceIt_Construct(
	CSG_FaceIteratorDescriptor *output, Object *ob, int offset)
{
	FaceIt *it;
	if (output == 0) return;

	// allocate some memory for blender iterator
	it = (FaceIt *)(MEM_mallocN(sizeof(FaceIt),"Boolean_FIt"));
	if (it == 0) {
		return ;
	}
	// assign blender specific variables
	it->mesh = ob->data;
	it->offset = offset;
	it->pos = 0;

	// assign iterator function pointers.
	output->Step = FaceIt_Step;
	output->Fill = FaceIt_Fill;
	output->Done = FaceIt_Done;
	output->Reset = FaceIt_Reset;
	output->num_elements = it->mesh->totface;
	output->it = it;
}

static Object *AddNewBlenderMesh(Base *base)
{
	// This little function adds a new mesh object to the blender object list
	// It uses ob to duplicate data as this seems to be easier than creating
	// a new one. This new oject contains no faces nor vertices.
	Mesh *old_me;
	Base *basen;
	Object *ob_new;

	// now create a new blender object.
	// duplicating all the settings from the previous object
	// to the new one.
	ob_new= copy_object(base->object);

	// Ok we don't want to use the actual data from the
	// last object, the above function incremented the 
	// number of users, so decrement it here.
	old_me= ob_new->data;
	old_me->id.us--;

	// Now create a new base to add into the linked list of 
	// vase objects.
	
	basen= MEM_mallocN(sizeof(Base), "duplibase");
	*basen= *base;
	BLI_addhead(&G.scene->base, basen);	/* addhead: anders oneindige lus */
	basen->object= ob_new;
	basen->flag &= ~SELECT;
				
	// Initialize the mesh data associated with this object.						
	ob_new->data= add_mesh("Mesh");
	G.totmesh++;

	// Finally assign the object type.
	ob_new->type= OB_MESH;

	return ob_new;
}

static void InterpCSGFace(
	DerivedMesh *dm, Mesh *orig_me, int index, int orig_index, int nr,
	float mapmat[][4])
{
	float obco[3], *co[4], *orig_co[4], w[4][4];
	MFace *mface, *orig_mface;
	int j;

	mface = CDDM_get_face(dm, index);
	orig_mface = orig_me->mface + orig_index;

	// get the vertex coordinates from the original mesh
	orig_co[0] = (orig_me->mvert + orig_mface->v1)->co;
	orig_co[1] = (orig_me->mvert + orig_mface->v2)->co;
	orig_co[2] = (orig_me->mvert + orig_mface->v3)->co;
	orig_co[3] = (orig_mface->v4)? (orig_me->mvert + orig_mface->v4)->co: NULL;

	// get the vertex coordinates from the new derivedmesh
	co[0] = CDDM_get_vert(dm, mface->v1)->co;
	co[1] = CDDM_get_vert(dm, mface->v2)->co;
	co[2] = CDDM_get_vert(dm, mface->v3)->co;
	co[3] = (nr == 4)? CDDM_get_vert(dm, mface->v4)->co: NULL;

	for (j = 0; j < nr; j++) {
		// get coordinate into the space of the original mesh
		if (mapmat)
			VecMat4MulVecfl(obco, mapmat, co[j]);
		else
			VecCopyf(obco, co[j]);

		InterpWeightsQ3Dfl(orig_co[0], orig_co[1], orig_co[2], orig_co[3], obco, w[j]);
	}

	CustomData_interp(&orig_me->fdata, &dm->faceData, &orig_index, NULL, (float*)w, 1, index);
}

/* Iterate over the CSG Output Descriptors and create a new DerivedMesh
   from them */
static DerivedMesh *ConvertCSGDescriptorsToDerivedMesh(
	CSG_FaceIteratorDescriptor *face_it,
	CSG_VertexIteratorDescriptor *vertex_it,
	float parinv[][4],
	float mapmat[][4],
	Material **mat,
	int *totmat,
	Object *ob1,
	Object *ob2)
{
	DerivedMesh *dm;
	GHash *material_hash = NULL;
	Mesh *me1= (Mesh*)ob1->data;
	Mesh *me2= (Mesh*)ob2->data;
	int i;

	// create a new DerivedMesh
	dm = CDDM_new(vertex_it->num_elements, 0, face_it->num_elements);

	CustomData_merge(&me1->fdata, &dm->faceData, CD_MASK_DERIVEDMESH,
	                 CD_DEFAULT, face_it->num_elements); 
	CustomData_merge(&me2->fdata, &dm->faceData, CD_MASK_DERIVEDMESH,
	                 CD_DEFAULT, face_it->num_elements); 

	// step through the vertex iterators:
	for (i = 0; !vertex_it->Done(vertex_it->it); i++) {
		CSG_IVertex csgvert;
		MVert *mvert = CDDM_get_vert(dm, i);

		// retrieve a csg vertex from the boolean module
		vertex_it->Fill(vertex_it->it, &csgvert);
		vertex_it->Step(vertex_it->it);

		// we have to map the vertex coordinates back in the coordinate frame
		// of the resulting object, since it was computed in world space
		VecMat4MulVecfl(mvert->co, parinv, csgvert.position);
	}

	// a hash table to remap materials to indices
	if (mat) {
		material_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
		*totmat = 0;
	}

	// step through the face iterators
	for(i = 0; !face_it->Done(face_it->it); i++) {
		Mesh *orig_me;
		Object *orig_ob;
		Material *orig_mat;
		CSG_IFace csgface;
		MFace *mface;
		int orig_index, mat_nr;

		// retrieve a csg face from the boolean module
		face_it->Fill(face_it->it, &csgface);
		face_it->Step(face_it->it);

		// find the original mesh and data
		orig_ob = (csgface.orig_face < me1->totface)? ob1: ob2;
		orig_me = (orig_ob == ob1)? me1: me2;
		orig_index = (orig_ob == ob1)? csgface.orig_face: csgface.orig_face - me1->totface;

		// copy all face layers, including mface
		CustomData_copy_data(&orig_me->fdata, &dm->faceData, orig_index, i, 1);

		// set mface
		mface = CDDM_get_face(dm, i);
		mface->v1 = csgface.vertex_index[0];
		mface->v2 = csgface.vertex_index[1];
		mface->v3 = csgface.vertex_index[2];
		mface->v4 = (csgface.vertex_number == 4)? csgface.vertex_index[3]: 0;

		// set material, based on lookup in hash table
		orig_mat= give_current_material(orig_ob, mface->mat_nr+1);

		if (mat && orig_mat) {
			if (!BLI_ghash_haskey(material_hash, orig_mat)) {
				mat[*totmat] = orig_mat;
				mat_nr = mface->mat_nr = (*totmat)++;
				BLI_ghash_insert(material_hash, orig_mat, SET_INT_IN_POINTER(mat_nr));
			}
			else
				mface->mat_nr = GET_INT_FROM_POINTER(BLI_ghash_lookup(material_hash, orig_mat));
		}
		else
			mface->mat_nr = 0;

		InterpCSGFace(dm, orig_me, i, orig_index, csgface.vertex_number,
		              (orig_me == me2)? mapmat: NULL);

		test_index_face(mface, &dm->faceData, i, csgface.vertex_number);
	}

	if (material_hash)
		BLI_ghash_free(material_hash, NULL, NULL);

	CDDM_calc_edges(dm);
	CDDM_calc_normals(dm);

	return dm;
}
	
static void BuildMeshDescriptors(
	struct Object *ob,
	int face_offset,
	struct CSG_FaceIteratorDescriptor * face_it,
	struct CSG_VertexIteratorDescriptor * vertex_it)
{
	VertexIt_Construct(vertex_it,ob);
	FaceIt_Construct(face_it,ob,face_offset);
}
	
static void FreeMeshDescriptors(
	struct CSG_FaceIteratorDescriptor *face_it,
	struct CSG_VertexIteratorDescriptor *vertex_it)
{
	VertexIt_Destruct(vertex_it);
	FaceIt_Destruct(face_it);
}

DerivedMesh *NewBooleanDerivedMesh_intern(
	struct Object *ob, struct Object *ob_select,
	int int_op_type, Material **mat, int *totmat)
{

	float inv_mat[4][4];
	float map_mat[4][4];

	DerivedMesh *dm = NULL;
	Mesh *me1 = get_mesh(ob_select);
	Mesh *me2 = get_mesh(ob);

	if (me1 == NULL || me2 == NULL) return 0;
	if (!me1->totface || !me2->totface) return 0;

	// we map the final object back into ob's local coordinate space. For this
	// we need to compute the inverse transform from global to ob (inv_mat),
	// and the transform from ob to ob_select for use in interpolation (map_mat)
	Mat4Invert(inv_mat, ob->obmat);
	Mat4MulMat4(map_mat, ob_select->obmat, inv_mat);
	Mat4Invert(inv_mat, ob_select->obmat);

	{
		// interface with the boolean module:
		//
		// the idea is, we pass the boolean module verts and faces using the
		// provided descriptors. once the boolean operation is performed, we
		// get back output descriptors, from which we then build a DerivedMesh

		CSG_VertexIteratorDescriptor vd_1, vd_2;
		CSG_FaceIteratorDescriptor fd_1, fd_2;
		CSG_OperationType op_type;
		CSG_BooleanOperation *bool_op;

		// work out the operation they chose and pick the appropriate 
		// enum from the csg module.
		switch (int_op_type) {
			case 1 : op_type = e_csg_intersection; break;
			case 2 : op_type = e_csg_union; break;
			case 3 : op_type = e_csg_difference; break;
			case 4 : op_type = e_csg_classify; break;
			default : op_type = e_csg_intersection;
		}
		
		BuildMeshDescriptors(ob_select, 0, &fd_1, &vd_1);
		BuildMeshDescriptors(ob, me1->totface, &fd_2, &vd_2);

		bool_op = CSG_NewBooleanFunction();

		// perform the operation
		if (CSG_PerformBooleanOperation(bool_op, op_type, fd_1, vd_1, fd_2, vd_2)) {
			CSG_VertexIteratorDescriptor vd_o;
			CSG_FaceIteratorDescriptor fd_o;

			CSG_OutputFaceDescriptor(bool_op, &fd_o);
			CSG_OutputVertexDescriptor(bool_op, &vd_o);

			// iterate through results of operation and insert
			// into new object
			dm = ConvertCSGDescriptorsToDerivedMesh(
				&fd_o, &vd_o, inv_mat, map_mat, mat, totmat, ob_select, ob);

			// free up the memory
			CSG_FreeVertexDescriptor(&vd_o);
			CSG_FreeFaceDescriptor(&fd_o);
		}
		else
			error("Unknown internal error in boolean");

		CSG_FreeBooleanOperation(bool_op);

		FreeMeshDescriptors(&fd_1, &vd_1);
		FreeMeshDescriptors(&fd_2, &vd_2);
	}

	return dm;
}

int NewBooleanMesh(Base *base, Base *base_select, int int_op_type)
{
	Mesh *me_new;
	int a, maxmat, totmat= 0;
	Object *ob_new, *ob, *ob_select;
	Material **mat;
	DerivedMesh *dm;

	ob= base->object;
	ob_select= base_select->object;

	maxmat= ob->totcol + ob_select->totcol;
	mat= (Material**)MEM_mallocN(sizeof(Material*)*maxmat, "NewBooleanMeshMat");
	
	/* put some checks in for nice user feedback */
	if((!(get_mesh(ob)->totface)) || (!(get_mesh(ob_select)->totface)))
	{
		MEM_freeN(mat);
		return -1;
	}
	
	dm= NewBooleanDerivedMesh_intern(ob, ob_select, int_op_type, mat, &totmat);

	if (dm == NULL) {
		MEM_freeN(mat);
		return 0;
	}

	/* create a new blender mesh object - using 'base' as  a template */
	ob_new= AddNewBlenderMesh(base_select);
	me_new= ob_new->data;

	DM_to_mesh(dm, me_new);
	dm->release(dm);

	/* add materials to object */
	for (a = 0; a < totmat; a++)
		assign_material(ob_new, mat[a], a+1);

	MEM_freeN(mat);

	/* update dag */
	DAG_object_flush_update(G.scene, ob_new, OB_RECALC_DATA);

	return 1;
}

DerivedMesh *NewBooleanDerivedMesh(struct Object *ob, struct Object *ob_select,
                                   int int_op_type)
{
	return NewBooleanDerivedMesh_intern(ob, ob_select, int_op_type, NULL, NULL);
}

