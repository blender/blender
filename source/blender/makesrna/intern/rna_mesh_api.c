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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#ifdef RNA_RUNTIME

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_material.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_arithb.h"
#include "BLI_edgehash.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"

static void rna_Mesh_calc_edges(Mesh *mesh)
{
	CustomData edata;
	EdgeHashIterator *ehi;
	MFace *mf = mesh->mface;
	MEdge *med;
	EdgeHash *eh = BLI_edgehash_new();
	int i, *index, totedge, totface = mesh->totface;

	for (i = 0; i < totface; i++, mf++) {
		if (!BLI_edgehash_haskey(eh, mf->v1, mf->v2))
			BLI_edgehash_insert(eh, mf->v1, mf->v2, NULL);
		if (!BLI_edgehash_haskey(eh, mf->v2, mf->v3))
			BLI_edgehash_insert(eh, mf->v2, mf->v3, NULL);
		
		if (mf->v4) {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v4))
				BLI_edgehash_insert(eh, mf->v3, mf->v4, NULL);
			if (!BLI_edgehash_haskey(eh, mf->v4, mf->v1))
				BLI_edgehash_insert(eh, mf->v4, mf->v1, NULL);
		} else {
			if (!BLI_edgehash_haskey(eh, mf->v3, mf->v1))
				BLI_edgehash_insert(eh, mf->v3, mf->v1, NULL);
		}
	}

	totedge = BLI_edgehash_size(eh);

	/* write new edges into a temporary CustomData */
	memset(&edata, 0, sizeof(edata));
	CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	ehi = BLI_edgehashIterator_new(eh);
	med = CustomData_get_layer(&edata, CD_MEDGE);
	for(i = 0; !BLI_edgehashIterator_isDone(ehi);
	    BLI_edgehashIterator_step(ehi), ++i, ++med, ++index) {
		BLI_edgehashIterator_getKey(ehi, (int*)&med->v1, (int*)&med->v2);

		med->flag = ME_EDGEDRAW|ME_EDGERENDER;
	}
	BLI_edgehashIterator_free(ehi);

	/* free old CustomData and assign new one */
	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata = edata;
	mesh->totedge = totedge;

	mesh->medge = CustomData_get_layer(&mesh->edata, CD_MEDGE);

	BLI_edgehash_free(eh, NULL);
}

static void rna_Mesh_update(Mesh *mesh, bContext *C)
{
	if(mesh->totface && mesh->totedge == 0)
		rna_Mesh_calc_edges(mesh);

	mesh_calc_normals(mesh->mvert, mesh->totvert, mesh->mface, mesh->totface, NULL);

	DAG_id_flush_update(&mesh->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, mesh);
}

static void rna_Mesh_transform(Mesh *me, float *mat)
{

	/* TODO: old API transform had recalc_normals option */
	int i;
	MVert *mvert= me->mvert;

	for(i= 0; i < me->totvert; i++, mvert++) {
		Mat4MulVecfl((float (*)[4])mat, mvert->co);
	}
}

static void rna_Mesh_add_verts(Mesh *mesh, int len)
{
	CustomData vdata;
	MVert *mvert;
	int i, totvert;

	if(len == 0)
		return;

	totvert= mesh->totvert + len;
	CustomData_copy(&mesh->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, totvert);
	CustomData_copy_data(&mesh->vdata, &vdata, 0, 0, mesh->totvert);

	if(!CustomData_has_layer(&vdata, CD_MVERT))
		CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);

	CustomData_free(&mesh->vdata, mesh->totvert);
	mesh->vdata= vdata;
	mesh_update_customdata_pointers(mesh);

	/* scan the input list and insert the new vertices */

	mvert= &mesh->mvert[mesh->totvert];
	for(i=0; i<len; i++, mvert++)
		mvert->flag |= SELECT;

	/* set final vertex list size */
	mesh->totvert= totvert;
}

Mesh *rna_Mesh_create_copy(Mesh *me)
{
	Mesh *ret= copy_mesh(me);
	ret->id.us--;
	
	return ret;
}

static void rna_Mesh_add_edges(Mesh *mesh, int len)
{
	CustomData edata;
	MEdge *medge;
	int i, totedge;

	if(len == 0)
		return;

	totedge= mesh->totedge+len;

	/* update customdata  */
	CustomData_copy(&mesh->edata, &edata, CD_MASK_MESH, CD_DEFAULT, totedge);
	CustomData_copy_data(&mesh->edata, &edata, 0, 0, mesh->totedge);

	if(!CustomData_has_layer(&edata, CD_MEDGE))
		CustomData_add_layer(&edata, CD_MEDGE, CD_CALLOC, NULL, totedge);

	CustomData_free(&mesh->edata, mesh->totedge);
	mesh->edata= edata;
	mesh_update_customdata_pointers(mesh);

	/* set default flags */
	medge= &mesh->medge[mesh->totedge];
	for(i=0; i<len; i++, medge++)
		medge->flag= ME_EDGEDRAW|ME_EDGERENDER|SELECT;

	mesh->totedge= totedge;
}

static void rna_Mesh_add_faces(Mesh *mesh, int len)
{
	CustomData fdata;
	MFace *mface;
	int i, totface;

	if(len == 0)
		return;

	totface= mesh->totface + len;	/* new face count */

	/* update customdata */
	CustomData_copy(&mesh->fdata, &fdata, CD_MASK_MESH, CD_DEFAULT, totface);
	CustomData_copy_data(&mesh->fdata, &fdata, 0, 0, mesh->totface);

	if(!CustomData_has_layer(&fdata, CD_MFACE))
		CustomData_add_layer(&fdata, CD_MFACE, CD_CALLOC, NULL, totface);

	CustomData_free(&mesh->fdata, mesh->totface);
	mesh->fdata= fdata;
	mesh_update_customdata_pointers(mesh);

	/* set default flags */
	mface= &mesh->mface[mesh->totface];
	for(i=0; i<len; i++, mface++)
		mface->flag= SELECT;

	mesh->totface= totface;
}

/*
static void rna_Mesh_add_faces(Mesh *mesh)
{
}
*/

static void rna_Mesh_add_geometry(Mesh *mesh, int verts, int edges, int faces)
{
	if(verts)
		rna_Mesh_add_verts(mesh, verts);
	if(edges)
		rna_Mesh_add_edges(mesh, edges);
	if(faces)
		rna_Mesh_add_faces(mesh, faces);
}

static void rna_Mesh_add_uv_texture(Mesh *me)
{
	me->mtface= CustomData_add_layer(&me->fdata, CD_MTFACE, CD_DEFAULT, NULL, me->totface);
}

static void rna_Mesh_calc_normals(Mesh *me)
{
	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

static void rna_Mesh_add_material(Mesh *me, Material *ma)
{
	int i;
	int totcol = me->totcol + 1;
	Material **mat;

	/* don't add if mesh already has it */
	for (i = 0; i < me->totcol; i++)
		if (me->mat[i] == ma)
			return;

	mat= MEM_callocN(sizeof(void*) * totcol, "newmatar");

	if (me->totcol) memcpy(mat, me->mat, sizeof(void*) * me->totcol);
	if (me->mat) MEM_freeN(me->mat);

	me->mat = mat;
	me->mat[me->totcol++] = ma;
	ma->id.us++;

	test_object_materials((ID*)me);
}

#else

void RNA_api_mesh(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "transform", "rna_Mesh_transform");
	RNA_def_function_ui_description(func, "Transform mesh vertices by a matrix.");
	parm= RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix.", 0.0f, 0.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "add_geometry", "rna_Mesh_add_geometry");
	parm= RNA_def_int(func, "verts", 0, 0, INT_MAX, "Number", "Number of vertices to add.", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "edges", 0, 0, INT_MAX, "Number", "Number of edges to add.", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	parm= RNA_def_int(func, "faces", 0, 0, INT_MAX, "Number", "Number of faces to add.", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func= RNA_def_function(srna, "create_copy", "rna_Mesh_create_copy");
	RNA_def_function_ui_description(func, "Create a copy of this Mesh datablock.");
	parm= RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh, remove it if it is only used for export.");
	RNA_def_function_return(func, parm);

	func= RNA_def_function(srna, "add_uv_texture", "rna_Mesh_add_uv_texture");
	RNA_def_function_ui_description(func, "Add a UV texture layer to Mesh.");

	func= RNA_def_function(srna, "calc_normals", "rna_Mesh_calc_normals");
	RNA_def_function_ui_description(func, "Calculate vertex normals.");

	func= RNA_def_function(srna, "update", "rna_Mesh_update");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func= RNA_def_function(srna, "add_material", "rna_Mesh_add_material");
	RNA_def_function_ui_description(func, "Add a new material to Mesh.");
	parm= RNA_def_pointer(func, "material", "Material", "", "Material to add.");
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

#endif

