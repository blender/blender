
#include <string.h>
/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * CSG operations. 
 */

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "CSG_BooleanOps.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"
#include "BKE_object.h"
#include "BKE_booleanops.h"
#include "BKE_utildefines.h"
#include "BKE_library.h"
#include "BKE_material.h"

#include <math.h>

// TODO check to see how many of these includes are necessary

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"

#include "BIF_editmesh.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/** check if passed mesh has faces, return zero if only edges, 1 if faces have been found */
int has_faces(Mesh *me);

/**
 * Here's the vertex iterator structure used to walk through
 * the blender vertex structure.
 */

typedef struct {
	Object *ob;
	Mesh *mesh;
	int pos;
} VertexIt;

/**
 * Implementations of local vertex iterator functions.
 * These describe a blender mesh to the CSG module.
 */

static
	void
VertexIt_Destruct(
	CSG_VertexIteratorDescriptor * iterator
){
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

static
	int
VertexIt_Done(
	CSG_IteratorPtr it
){
	VertexIt * iterator = (VertexIt *)it;
	return(iterator->pos >= iterator->mesh->totvert);
}
	

static
	void
VertexIt_Fill(
	CSG_IteratorPtr it,
	CSG_IVertex *vert
){
	VertexIt * iterator = (VertexIt *)it;
	MVert *verts = iterator->mesh->mvert;

	float global_pos[3];

	VecMat4MulVecfl(
		global_pos,
		iterator->ob->obmat, 
		verts[iterator->pos].co
	);

	vert->position[0] = global_pos[0];
	vert->position[1] = global_pos[1];
	vert->position[2] = global_pos[2];
}

static
	void
VertexIt_Step(
	CSG_IteratorPtr it
){
	VertexIt * iterator = (VertexIt *)it;
	iterator->pos ++;
} 
 
static
	void
VertexIt_Reset(
	CSG_IteratorPtr it
){
	VertexIt * iterator = (VertexIt *)it;
	iterator->pos = 0;
}

static
	void
VertexIt_Construct(
	CSG_VertexIteratorDescriptor * output,
	Object *ob
){

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
	Object *ob;
	Mesh *mesh;
	int pos;
} FaceIt;


static
	void
FaceIt_Destruct(
	CSG_FaceIteratorDescriptor * iterator
) {
	MEM_freeN(iterator->it);
	iterator->Done = NULL;
	iterator->Fill = NULL;
	iterator->Reset = NULL;
	iterator->Step = NULL;
	iterator->num_elements = 0;
}


static
	int
FaceIt_Done(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	FaceIt * iterator = (FaceIt *)it;
	return(iterator->pos >= iterator->mesh->totface);
}

static
	void
FaceIt_Fill(
	CSG_IteratorPtr it,
	CSG_IFace *face
){
	// assume CSG_IteratorPtr is of the correct type.
	FaceIt * face_it = (FaceIt *)it;
	Object *ob = face_it->ob;
	MFace *mfaces = face_it->mesh->mface;
	TFace *tfaces = face_it->mesh->tface;
	int f_index = face_it->pos;
	MFace *mface = &mfaces[f_index];
	FaceData *fdata = face->user_face_data;
	
	if (mface->v3) {
		// ignore lines (faces with mface->v3==0)
		face->vertex_index[0] = mface->v1;
		face->vertex_index[1] = mface->v2;
		face->vertex_index[2] = mface->v3;
		if (mface->v4) {
			face->vertex_index[3] = mface->v4;
			face->vertex_number = 4;
		} else {
			face->vertex_number = 3;
		}
	}	

	fdata->faceflag = mface->flag;
	fdata->material = give_current_material(ob, mface->mat_nr+1);

		// pack rgba colors.
	if (tfaces) {
		TFace *tface= &tfaces[f_index];
		int i;

		fdata->tpage = tface->tpage;
		fdata->flag = tface->flag;
		fdata->transp = tface->transp;
		fdata->mode = tface->mode;
		fdata->tile = tface->tile;

		for (i=0; i<4; i++) {
			FaceVertexData *fvdata= face->user_face_vertex_data[i];

			fvdata->uv[0] = tface->uv[i][0];
			fvdata->uv[1] = tface->uv[i][1];
			fvdata->color[0] = (float) ((tface->col[i] >> 24) & 0xff);
			fvdata->color[1] = (float) ((tface->col[i] >> 16) & 0xff);
			fvdata->color[2] = (float) ((tface->col[i] >> 8) & 0xff);
			fvdata->color[3] = (float) ((tface->col[i] >> 0) & 0xff);
		}
	}
}


static
	void
FaceIt_Step(
	CSG_IteratorPtr it
) {
	FaceIt * face_it = (FaceIt *)it;		
	face_it->pos ++;
}

static
	void
FaceIt_Reset(
	CSG_IteratorPtr it
) {
	FaceIt * face_it = (FaceIt *)it;		
	face_it->pos = 0;
}	

static
	void
FaceIt_Construct(
	CSG_FaceIteratorDescriptor * output,
	Object * ob
){

	FaceIt *it;
	if (output == 0) return;

	// allocate some memory for blender iterator
	it = (FaceIt *)(MEM_mallocN(sizeof(FaceIt),"Boolean_FIt"));
	if (it == 0) {
		return ;
	}
	// assign blender specific variables
	it->ob = ob;
	it->mesh = ob->data;
	it->pos = 0;

	// assign iterator function pointers.
	output->Step = FaceIt_Step;
	output->Fill = FaceIt_Fill;
	output->Done = FaceIt_Done;
	output->Reset = FaceIt_Reset;
	output->num_elements = it->mesh->totface;
	output->it = it;
}


/**
 * Interpolation functions for various user data types.
 */
	
	int
InterpNoUserData(
	void *d1,
	void *d2,
	void *dnew,
	float epsilon
) {
	// nothing to do of course.
	return 0;
}

	int
InterpFaceVertexData(
	void *d1,
	void *d2,
	void *dnew,
	float epsilon
) {
	/* XXX, passed backwards, should be fixed inside
	 * BSP lib I guess.
	 */
	FaceVertexData *fv1 = d2;
	FaceVertexData *fv2 = d1;
	FaceVertexData *fvO = dnew;

	fvO->uv[0] = (fv2->uv[0] - fv1->uv[0]) * epsilon + fv1->uv[0]; 
	fvO->uv[1] = (fv2->uv[1] - fv1->uv[1]) * epsilon + fv1->uv[1]; 
	fvO->color[0] = (fv2->color[0] - fv1->color[0]) * epsilon + fv1->color[0]; 
	fvO->color[1] = (fv2->color[1] - fv1->color[1]) * epsilon + fv1->color[1]; 
	fvO->color[2] = (fv2->color[2] - fv1->color[2]) * epsilon + fv1->color[2]; 
	fvO->color[3] = (fv2->color[3] - fv1->color[3]) * epsilon + fv1->color[3]; 

	return 0;
}

int has_faces(Mesh *me)
{
	MFace *mface;
	int a;

	mface= me->mface;
	for(a=0; a<me->totface; a++, mface++) {		
		if(mface->v3) return 1;
	}
	return 0;
}

/**
 * Assumes mesh is valid and forms part of a fresh
 * blender object.
 */

	int
NewBooleanMesh(
	struct Base * base,
	struct Base * base_select,
	int int_op_type
){
	Mesh *me2 = get_mesh(base_select->object);
	Mesh *me = get_mesh(base->object);
	Mesh *me_new = NULL;
	Object *ob;
	int free_tface1,free_tface2;

	float inv_mat[4][4];
	int success = 0;
	// build and fill new descriptors for these meshes
	CSG_VertexIteratorDescriptor vd_1;
	CSG_VertexIteratorDescriptor vd_2;
	CSG_FaceIteratorDescriptor fd_1;
	CSG_FaceIteratorDescriptor fd_2;

	CSG_MeshPropertyDescriptor mpd1,mpd2;

	// work out the operation they chose and pick the appropriate 
	// enum from the csg module.

	CSG_OperationType op_type;

	if (me == NULL || me2 == NULL) return 0;

	success = has_faces(me);
	if(success==0) return 0;
	success = has_faces(me2);
	if(success==0) return 0;
	
	success = 0;

	switch (int_op_type) {
		case 1 : op_type = e_csg_intersection; break;
		case 2 : op_type = e_csg_union; break;
		case 3 : op_type = e_csg_difference; break;
		case 4 : op_type = e_csg_classify; break;
		default : op_type = e_csg_intersection;
	}

	// Here is the section where we describe the properties of
	// both meshes to the bsp module.

	if (me->mcol != NULL) {
		// Then this mesh has vertex colors only 
		// well this is awkward because there is no equivalent 
		// test_index_mface just for vertex colors!
		// as a temporary hack we can convert these vertex colors 
		// into tfaces do the operation and turn them back again.

		// create some memory for the tfaces.
		me->tface = (TFace *)MEM_callocN(sizeof(TFace)*me->totface,"BooleanOps_TempTFace");
		mcol_to_tface(me,1);
		free_tface1 = 1;
	} else {
		free_tface1 = 0;
	}

	mpd1.user_face_vertex_data_size = 0;
	mpd1.user_data_size = sizeof(FaceData);

	if (me->tface) {
		mpd1.user_face_vertex_data_size = sizeof(FaceVertexData);
	}
	
	// same for mesh2

	if (me2->mcol != NULL) {
		// create some memory for the tfaces.
		me2->tface = (TFace *)MEM_callocN(sizeof(TFace)*me2->totface,"BooleanOps_TempTFace");
		mcol_to_tface(me2,1);
		free_tface2 = 1;
	} else {
		free_tface2 = 0;
	}

	mpd2.user_face_vertex_data_size = 0;
	mpd2.user_data_size = sizeof(FaceData);

	if (me2->tface) {
		mpd2.user_face_vertex_data_size = sizeof(FaceVertexData);
	}

	ob = base->object;

	// we map the final object back into object 1's (ob)
	// local coordinate space. For this we need to compute
	// the inverse transform from global to local.	
	
	Mat4Invert(inv_mat,ob->obmat);

	// make a boolean operation;
	{
		CSG_BooleanOperation * bool_op = CSG_NewBooleanFunction();
		CSG_MeshPropertyDescriptor output_mpd = CSG_DescibeOperands(bool_op,mpd1,mpd2);
		// analyse the result and choose mesh descriptors accordingly
		int output_type;			
		if (output_mpd.user_face_vertex_data_size) {
			output_type = 1;
		} else {
			output_type = 0;
		}
		
		BuildMeshDescriptors(
			base->object,
			&fd_1,
			&vd_1
		);
		BuildMeshDescriptors(
			base_select->object,
			&fd_2,
			&vd_2
		);

		// perform the operation

		if (output_type == 0) {

			success = 
			CSG_PerformBooleanOperation(
				bool_op,
				op_type,
				fd_1,vd_1,fd_2,vd_2,
				InterpNoUserData	
			);
		} else {
			success = 
			CSG_PerformBooleanOperation(
				bool_op,
				op_type,
				fd_1,vd_1,fd_2,vd_2,
				InterpFaceVertexData	
			);
		}
		
		if (success) {
			// descriptions of the output;
			CSG_VertexIteratorDescriptor vd_o;
			CSG_FaceIteratorDescriptor fd_o;

			// Create a new blender mesh object - using 'base' as 
			// a template for the new object.
			Object * ob_new=  AddNewBlenderMesh(base);

			// get the output descriptors

			CSG_OutputFaceDescriptor(bool_op,&fd_o);
			CSG_OutputVertexDescriptor(bool_op,&vd_o);

			me_new = ob_new->data;
			// iterate through results of operation and insert into new object
			// see subsurf.c 

			ConvertCSGDescriptorsToMeshObject(
				ob_new,
				&output_mpd,
				&fd_o,
				&vd_o,
				inv_mat
			);
		
			// initialize the object
			tex_space_mesh(me_new);

			// free up the memory

			CSG_FreeVertexDescriptor(&vd_o);
			CSG_FreeFaceDescriptor(&fd_o);
		}

		CSG_FreeBooleanOperation(bool_op);
		bool_op = NULL;

	}

	// We may need to map back the tfaces to mcols here.
	if (free_tface1) {
		tface_to_mcol(me);
		MEM_freeN(me->tface);
		me->tface = NULL;
	}
	if (free_tface2) {
		tface_to_mcol(me2);
		MEM_freeN(me2->tface);
		me2->tface = NULL;
	}

	if (free_tface1 && free_tface2) {
		// then we need to map the output tfaces into mcols
		if (me_new) {
			tface_to_mcol(me_new);
			MEM_freeN(me_new->tface);
			me_new->tface = NULL;
		}
	}
	
	FreeMeshDescriptors(&fd_1,&vd_1);
	FreeMeshDescriptors(&fd_2,&vd_2);

	return success;
}


	Object *
AddNewBlenderMesh(
	Base *base
){
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
	ob_new->data= add_mesh();
	G.totmesh++;

	// Finally assign the object type.
	ob_new->type= OB_MESH;

	return ob_new;
}

/**
 *
 * External interface
 *
 * This function builds a blender mesh using the output information from
 * the CSG module. It declares all the necessary blender cruft and 
 * fills in the vertex and face arrays.
 */
	int
ConvertCSGDescriptorsToMeshObject(
	Object *ob,
	CSG_MeshPropertyDescriptor *props,
	CSG_FaceIteratorDescriptor *face_it,
	CSG_VertexIteratorDescriptor *vertex_it,
	float parinv[][4]
){
	Mesh *me = ob->data;
	FaceVertexData *user_face_vertex_data;
	GHash *material_hash;
	CSG_IVertex vert;
	CSG_IFace face;
	MVert *insert_pos;
	MFace *mfaces;
	TFace *tfaces;
	int fi_insert_pos, nmaterials;

	// create some memory for the Iface according to output mesh props.

	if (face_it == NULL || vertex_it == NULL || props == NULL || me == NULL) {
		return 0;
	}
	if (vertex_it->num_elements > MESH_MAX_VERTS) return 0;

	// initialize the face structure for readback
	
	face.user_face_data = MEM_callocN(sizeof(FaceData),"BooleanOp_IFaceData");
	
	if (props->user_face_vertex_data_size) {
		user_face_vertex_data = MEM_callocN(sizeof(FaceVertexData)*4,"BooleanOp_IFaceData");
		face.user_face_vertex_data[0] = &user_face_vertex_data[0];
		face.user_face_vertex_data[1] = &user_face_vertex_data[1];
		face.user_face_vertex_data[2] = &user_face_vertex_data[2];
		face.user_face_vertex_data[3] = &user_face_vertex_data[3];
	} else {
		user_face_vertex_data = NULL;
	}
	
	// create memory for the vertex array.

	me->mvert = MEM_callocN(sizeof(MVert) * vertex_it->num_elements,"BooleanOp_VertexArray");
	me->mface = MEM_callocN(sizeof(MFace) * face_it->num_elements,"BooleanOp_FaceArray");

	if (user_face_vertex_data) {
		me->tface = MEM_callocN(sizeof(TFace) * face_it->num_elements,"BooleanOp_TFaceArray");
		if (me->tface == NULL) return 0;
	} else {
		me->tface = NULL;
	}

	if (me->mvert == NULL || me->mface == NULL) return 0;

	insert_pos = me->mvert;
	mfaces = me->mface;
	tfaces = me->tface;

	fi_insert_pos = 0;

	// step through the iterators.

	while (!vertex_it->Done(vertex_it->it)) {
		vertex_it->Fill(vertex_it->it,&vert);

		// map output vertex into insert_pos 
		// and transform at by parinv at the same time.

		VecMat4MulVecfl(
			insert_pos->co,
			parinv,
			vert.position
		);
		insert_pos ++;
		vertex_it->Step(vertex_it->it);
	}

	me->totvert = vertex_it->num_elements;

	// a hash table to remap materials to indices with
	material_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	nmaterials = 0;

	while (!face_it->Done(face_it->it)) {
		MFace *mface = &mfaces[fi_insert_pos];
		FaceData *fdata;
		
		face_it->Fill(face_it->it,&face);
		fdata = face.user_face_data;

		// cheat CSG never dumps out quads.

		mface->v1 = face.vertex_index[0];
		mface->v2 = face.vertex_index[1];
		mface->v3 = face.vertex_index[2];
		mface->v4 = 0;

		mface->edcode = ME_V1V2|ME_V2V3|ME_V3V4|ME_V4V1;
		mface->puno = 0;
		mface->mat_nr = 0;
		mface->flag = fdata->faceflag;
		
		/* HACK, perform material to index mapping using a general
		 * hash table, just tuck the int into a void *.
		 */
		
		if (!BLI_ghash_haskey(material_hash, fdata->material)) {
			int matnr = nmaterials++;
			BLI_ghash_insert(material_hash, fdata->material, (void*) matnr);
			assign_material(ob, fdata->material, matnr+1);
		}
		mface->mat_nr = (int) BLI_ghash_lookup(material_hash, fdata->material);

		// grab the vertex colors and texture cos and dump them into the tface.

		if (tfaces) {
			TFace *tface= &tfaces[fi_insert_pos];
			int i;
			
			// copy all the tface settings back
			tface->tpage = fdata->tpage;
			tface->flag = fdata->flag;
			tface->transp = fdata->transp;
			tface->mode = fdata->mode;
			tface->tile = fdata->tile;
			
			for (i=0; i<4; i++) {
				FaceVertexData *fvdata = face.user_face_vertex_data[i];
				float *color = fvdata->color;

				tface->uv[i][0] = fvdata->uv[0];
				tface->uv[i][1] = fvdata->uv[1];
				tface->col[i] = 
					((((unsigned int)floor(color[0] + 0.5f)) & 0xff) << 24) |
					((((unsigned int)floor(color[1] + 0.5f)) & 0xff) << 16) |
					((((unsigned int)floor(color[2] + 0.5f)) & 0xff) << 8) |
					((((unsigned int)floor(color[3] + 0.5f)) & 0xff) << 0);
			}

			test_index_face(mface, tface, 3);
		} else {
			test_index_mface(mface, 3);
		}

		fi_insert_pos++;
		face_it->Step(face_it->it);
	}

	BLI_ghash_free(material_hash, NULL, NULL);

	me->totface = face_it->num_elements;

	mesh_calculate_vertex_normals(me);

	// thats it!
	if (user_face_vertex_data) {
		MEM_freeN(user_face_vertex_data);
	}
	MEM_freeN(face.user_face_data);

	return 1;
}	
	
	void
BuildMeshDescriptors(
	struct Object *ob,
	struct CSG_FaceIteratorDescriptor * face_it,
	struct CSG_VertexIteratorDescriptor * vertex_it
){	
	VertexIt_Construct(vertex_it,ob);
	FaceIt_Construct(face_it,ob);
}
	
	void
FreeMeshDescriptors(
	struct CSG_FaceIteratorDescriptor *face_it,
	struct CSG_VertexIteratorDescriptor *vertex_it
){
	VertexIt_Destruct(vertex_it);
	FaceIt_Destruct(face_it);
}

