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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/mesh.c
 *  \ingroup bke
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_ipo_types.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_bpath.h"
#include "BLI_editVert.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "BKE_animsys.h"
#include "BKE_main.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_displist.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_key.h"
/* these 2 are only used by conversion functions */
#include "BKE_curve.h"
/* -- */
#include "BKE_object.h"

#ifdef USE_BMESH_FORWARD_COMPAT
#include "BLI_array.h"
#endif


EditMesh *BKE_mesh_get_editmesh(Mesh *me)
{
	return me->edit_mesh;
}

void BKE_mesh_end_editmesh(Mesh *UNUSED(me), EditMesh *UNUSED(em))
{
}


void mesh_update_customdata_pointers(Mesh *me)
{
	me->mvert = CustomData_get_layer(&me->vdata, CD_MVERT);
	me->dvert = CustomData_get_layer(&me->vdata, CD_MDEFORMVERT);
	me->msticky = CustomData_get_layer(&me->vdata, CD_MSTICKY);

	me->medge = CustomData_get_layer(&me->edata, CD_MEDGE);

	me->mface = CustomData_get_layer(&me->fdata, CD_MFACE);
	me->mcol = CustomData_get_layer(&me->fdata, CD_MCOL);
	me->mtface = CustomData_get_layer(&me->fdata, CD_MTFACE);
}

/* Note: unlinking is called when me->id.us is 0, question remains how
 * much unlinking of Library data in Mesh should be done... probably
 * we need a more generic method, like the expand() functions in
 * readfile.c */

void unlink_mesh(Mesh *me)
{
	int a;
	
	if(me==NULL) return;
	
	for(a=0; a<me->totcol; a++) {
		if(me->mat[a]) me->mat[a]->id.us--;
		me->mat[a]= NULL;
	}

	if(me->key) {
		me->key->id.us--;
	}
	me->key= NULL;
	
	if(me->texcomesh) me->texcomesh= NULL;
}

/* do not free mesh itself */
void free_mesh(Mesh *me)
{
	unlink_mesh(me);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	
	if(me->adt) {
		BKE_free_animdata(&me->id);
		me->adt= NULL;
	}
	
	if(me->mat) MEM_freeN(me->mat);
	
	if(me->bb) MEM_freeN(me->bb);
	if(me->mselect) MEM_freeN(me->mselect);
	if(me->edit_mesh) MEM_freeN(me->edit_mesh);
}

void copy_dverts(MDeformVert *dst, MDeformVert *src, int copycount)
{
	/* Assumes dst is already set up */
	int i;

	if (!src || !dst)
		return;

	memcpy (dst, src, copycount * sizeof(MDeformVert));
	
	for (i=0; i<copycount; i++){
		if (src[i].dw){
			dst[i].dw = MEM_callocN (sizeof(MDeformWeight)*src[i].totweight, "copy_deformWeight");
			memcpy (dst[i].dw, src[i].dw, sizeof (MDeformWeight)*src[i].totweight);
		}
	}

}

void free_dverts(MDeformVert *dvert, int totvert)
{
	/* Instead of freeing the verts directly,
	call this function to delete any special
	vert data */
	int	i;

	if (!dvert)
		return;

	/* Free any special data from the verts */
	for (i=0; i<totvert; i++){
		if (dvert[i].dw) MEM_freeN (dvert[i].dw);
	}
	MEM_freeN (dvert);
}

Mesh *add_mesh(const char *name)
{
	Mesh *me;
	
	me= alloc_libblock(&G.main->mesh, ID_ME, name);
	
	me->size[0]= me->size[1]= me->size[2]= 1.0;
	me->smoothresh= 30;
	me->texflag= AUTOSPACE;
	me->flag= ME_TWOSIDED;
	me->bb= unit_boundbox();
	me->drawflag= ME_DRAWEDGES|ME_DRAWFACES|ME_DRAWCREASES;
	
	return me;
}

Mesh *copy_mesh(Mesh *me)
{
	Mesh *men;
	MTFace *tface;
	int a, i;
	
	men= copy_libblock(&me->id);
	
	men->mat= MEM_dupallocN(me->mat);
	for(a=0; a<men->totcol; a++) {
		id_us_plus((ID *)men->mat[a]);
	}
	id_us_plus((ID *)men->texcomesh);

	CustomData_copy(&me->vdata, &men->vdata, CD_MASK_MESH, CD_DUPLICATE, men->totvert);
	CustomData_copy(&me->edata, &men->edata, CD_MASK_MESH, CD_DUPLICATE, men->totedge);
	CustomData_copy(&me->fdata, &men->fdata, CD_MASK_MESH, CD_DUPLICATE, men->totface);
	mesh_update_customdata_pointers(men);

	/* ensure indirect linked data becomes lib-extern */
	for(i=0; i<me->fdata.totlayer; i++) {
		if(me->fdata.layers[i].type == CD_MTFACE) {
			tface= (MTFace*)me->fdata.layers[i].data;

			for(a=0; a<me->totface; a++, tface++)
				if(tface->tpage)
					id_lib_extern((ID*)tface->tpage);
		}
	}
	
	men->mselect= NULL;
	men->edit_mesh= NULL;

	men->bb= MEM_dupallocN(men->bb);
	
	men->key= copy_key(me->key);
	if(men->key) men->key->from= (ID *)men;

	return men;
}

static void expand_local_mesh(Mesh *me)
{
	id_lib_extern((ID *)me->texcomesh);

	if(me->mtface) {
		MTFace *tface;
		int a, i;

		for(i=0; i<me->fdata.totlayer; i++) {
			if(me->fdata.layers[i].type == CD_MTFACE) {
				tface= (MTFace*)me->fdata.layers[i].data;

				for(a=0; a<me->totface; a++, tface++) {
					if(tface->tpage) {
						id_lib_extern((ID *)tface->tpage);
					}
				}
			}
		}
	}

	if(me->mat) {
		extern_local_matarar(me->mat, me->totcol);
	}
}

void make_local_mesh(Mesh *me)
{
	Main *bmain= G.main;
	Object *ob;
	int is_local= FALSE, is_lib= FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if(me->id.lib==NULL) return;
	if(me->id.us==1) {
		id_clear_lib_data(bmain, &me->id);
		expand_local_mesh(me);
		return;
	}

	for(ob= bmain->object.first; ob && ELEM(0, is_lib, is_local); ob= ob->id.next) {
		if(me == ob->data) {
			if(ob->id.lib) is_lib= TRUE;
			else is_local= TRUE;
		}
	}

	if(is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &me->id);
		expand_local_mesh(me);
	}
	else if(is_local && is_lib) {
		Mesh *me_new= copy_mesh(me);
		me_new->id.us= 0;


		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, me->id.lib, &me_new->id);

		for(ob= bmain->object.first; ob; ob= ob->id.next) {
			if(me == ob->data) {
				if(ob->id.lib==NULL) {
					set_mesh(ob, me_new);
				}
			}
		}
	}
}

void boundbox_mesh(Mesh *me, float *loc, float *size)
{
	BoundBox *bb;
	float min[3], max[3];
	float mloc[3], msize[3];
	
	if(me->bb==NULL) me->bb= MEM_callocN(sizeof(BoundBox), "boundbox");
	bb= me->bb;

	if (!loc) loc= mloc;
	if (!size) size= msize;
	
	INIT_MINMAX(min, max);
	if(!minmax_mesh(me, min, max)) {
		min[0] = min[1] = min[2] = -1.0f;
		max[0] = max[1] = max[2] = 1.0f;
	}

	mid_v3_v3v3(loc, min, max);
		
	size[0]= (max[0]-min[0])/2.0f;
	size[1]= (max[1]-min[1])/2.0f;
	size[2]= (max[2]-min[2])/2.0f;
	
	boundbox_set_from_min_max(bb, min, max);
}

void tex_space_mesh(Mesh *me)
{
	float loc[3], size[3];
	int a;

	boundbox_mesh(me, loc, size);

	if(me->texflag & AUTOSPACE) {
		for (a=0; a<3; a++) {
			if(size[a]==0.0f) size[a]= 1.0f;
			else if(size[a]>0.0f && size[a]<0.00001f) size[a]= 0.00001f;
			else if(size[a]<0.0f && size[a]> -0.00001f) size[a]= -0.00001f;
		}

		copy_v3_v3(me->loc, loc);
		copy_v3_v3(me->size, size);
		zero_v3(me->rot);
	}
}

BoundBox *mesh_get_bb(Object *ob)
{
	Mesh *me= ob->data;

	if(ob->bb)
		return ob->bb;

	if (!me->bb)
		tex_space_mesh(me);

	return me->bb;
}

void mesh_get_texspace(Mesh *me, float *loc_r, float *rot_r, float *size_r)
{
	if (!me->bb) {
		tex_space_mesh(me);
	}

	if (loc_r) copy_v3_v3(loc_r, me->loc);
	if (rot_r) copy_v3_v3(rot_r, me->rot);
	if (size_r) copy_v3_v3(size_r, me->size);
}

float *get_mesh_orco_verts(Object *ob)
{
	Mesh *me = ob->data;
	MVert *mvert = NULL;
	Mesh *tme = me->texcomesh?me->texcomesh:me;
	int a, totvert;
	float (*vcos)[3] = NULL;

	/* Get appropriate vertex coordinates */
	vcos = MEM_callocN(sizeof(*vcos)*me->totvert, "orco mesh");
	mvert = tme->mvert;
	totvert = MIN2(tme->totvert, me->totvert);

	for(a=0; a<totvert; a++, mvert++) {
		copy_v3_v3(vcos[a], mvert->co);
	}

	return (float*)vcos;
}

void transform_mesh_orco_verts(Mesh *me, float (*orco)[3], int totvert, int invert)
{
	float loc[3], size[3];
	int a;

	mesh_get_texspace(me->texcomesh?me->texcomesh:me, loc, NULL, size);

	if(invert) {
		for(a=0; a<totvert; a++) {
			float *co = orco[a];
			madd_v3_v3v3v3(co, loc, co, size);
		}
	}
	else {
		for(a=0; a<totvert; a++) {
			float *co = orco[a];
			co[0] = (co[0]-loc[0])/size[0];
			co[1] = (co[1]-loc[1])/size[1];
			co[2] = (co[2]-loc[2])/size[2];
		}
	}
}

/* rotates the vertices of a face in case v[2] or v[3] (vertex index) is = 0.
   this is necessary to make the if(mface->v4) check for quads work */
int test_index_face(MFace *mface, CustomData *fdata, int mfindex, int nr)
{
	/* first test if the face is legal */
	if((mface->v3 || nr==4) && mface->v3==mface->v4) {
		mface->v4= 0;
		nr--;
	}
	if((mface->v2 || mface->v4) && mface->v2==mface->v3) {
		mface->v3= mface->v4;
		mface->v4= 0;
		nr--;
	}
	if(mface->v1==mface->v2) {
		mface->v2= mface->v3;
		mface->v3= mface->v4;
		mface->v4= 0;
		nr--;
	}

	/* check corrupt cases, bowtie geometry, cant handle these because edge data wont exist so just return 0 */
	if(nr==3) {
		if(
		/* real edges */
			mface->v1==mface->v2 ||
			mface->v2==mface->v3 ||
			mface->v3==mface->v1
		) {
			return 0;
		}
	}
	else if(nr==4) {
		if(
		/* real edges */
			mface->v1==mface->v2 ||
			mface->v2==mface->v3 ||
			mface->v3==mface->v4 ||
			mface->v4==mface->v1 ||
		/* across the face */
			mface->v1==mface->v3 ||
			mface->v2==mface->v4
		) {
			return 0;
		}
	}

	/* prevent a zero at wrong index location */
	if(nr==3) {
		if(mface->v3==0) {
			static int corner_indices[4] = {1, 2, 0, 3};

			SWAP(unsigned int, mface->v1, mface->v2);
			SWAP(unsigned int, mface->v2, mface->v3);

			if(fdata)
				CustomData_swap(fdata, mfindex, corner_indices);
		}
	}
	else if(nr==4) {
		if(mface->v3==0 || mface->v4==0) {
			static int corner_indices[4] = {2, 3, 0, 1};

			SWAP(unsigned int, mface->v1, mface->v3);
			SWAP(unsigned int, mface->v2, mface->v4);

			if(fdata)
				CustomData_swap(fdata, mfindex, corner_indices);
		}
	}

	return nr;
}

Mesh *get_mesh(Object *ob)
{
	
	if(ob==NULL) return NULL;
	if(ob->type==OB_MESH) return ob->data;
	else return NULL;
}

void set_mesh(Object *ob, Mesh *me)
{
	Mesh *old=NULL;

	multires_force_update(ob);
	
	if(ob==NULL) return;
	
	if(ob->type==OB_MESH) {
		old= ob->data;
		if (old)
			old->id.us--;
		ob->data= me;
		id_us_plus((ID *)me);
	}
	
	test_object_materials((ID *)me);

	test_object_modifiers(ob);
}

/* ************** make edges in a Mesh, for outside of editmode */

struct edgesort {
	unsigned int v1, v2;
	short is_loose, is_draw;
};

/* edges have to be added with lowest index first for sorting */
static void to_edgesort(struct edgesort *ed,
                        unsigned int v1, unsigned int v2,
                        short is_loose, short is_draw)
{
	if(v1<v2) {
		ed->v1= v1; ed->v2= v2;
	}
	else {
		ed->v1= v2; ed->v2= v1;
	}
	ed->is_loose= is_loose;
	ed->is_draw= is_draw;
}

static int vergedgesort(const void *v1, const void *v2)
{
	const struct edgesort *x1=v1, *x2=v2;

	if( x1->v1 > x2->v1) return 1;
	else if( x1->v1 < x2->v1) return -1;
	else if( x1->v2 > x2->v2) return 1;
	else if( x1->v2 < x2->v2) return -1;
	
	return 0;
}

static void mfaces_strip_loose(MFace *mface, int *totface)
{
	int a,b;

	for (a=b=0; a<*totface; a++) {
		if (mface[a].v3) {
			if (a!=b) {
				memcpy(&mface[b],&mface[a],sizeof(mface[b]));
			}
			b++;
		}
	}

	*totface= b;
}

/* Create edges based on known verts and faces */
static void make_edges_mdata(MVert *UNUSED(allvert), MFace *allface, int UNUSED(totvert), int totface,
	int old, MEdge **alledge, int *_totedge)
{
	MFace *mface;
	MEdge *medge;
	struct edgesort *edsort, *ed;
	int a, totedge=0, final=0;

	/* we put all edges in array, sort them, and detect doubles that way */

	for(a= totface, mface= allface; a>0; a--, mface++) {
		if(mface->v4) totedge+=4;
		else if(mface->v3) totedge+=3;
		else totedge+=1;
	}

	if(totedge==0) {
		/* flag that mesh has edges */
		(*alledge)= MEM_callocN(0, "make mesh edges");
		(*_totedge) = 0;
		return;
	}

	ed= edsort= MEM_mallocN(totedge*sizeof(struct edgesort), "edgesort");

	for(a= totface, mface= allface; a>0; a--, mface++) {
		to_edgesort(ed++, mface->v1, mface->v2, !mface->v3, mface->edcode & ME_V1V2);
		if(mface->v4) {
			to_edgesort(ed++, mface->v2, mface->v3, 0, mface->edcode & ME_V2V3);
			to_edgesort(ed++, mface->v3, mface->v4, 0, mface->edcode & ME_V3V4);
			to_edgesort(ed++, mface->v4, mface->v1, 0, mface->edcode & ME_V4V1);
		}
		else if(mface->v3) {
			to_edgesort(ed++, mface->v2, mface->v3, 0, mface->edcode & ME_V2V3);
			to_edgesort(ed++, mface->v3, mface->v1, 0, mface->edcode & ME_V3V1);
		}
	}

	qsort(edsort, totedge, sizeof(struct edgesort), vergedgesort);

	/* count final amount */
	for(a=totedge, ed=edsort; a>1; a--, ed++) {
		/* edge is unique when it differs from next edge, or is last */
		if(ed->v1 != (ed+1)->v1 || ed->v2 != (ed+1)->v2) final++;
	}
	final++;

	(*alledge)= medge= MEM_callocN(sizeof (MEdge) * final, "make_edges mdge");
	(*_totedge)= final;

	for(a=totedge, ed=edsort; a>1; a--, ed++) {
		/* edge is unique when it differs from next edge, or is last */
		if(ed->v1 != (ed+1)->v1 || ed->v2 != (ed+1)->v2) {
			medge->v1= ed->v1;
			medge->v2= ed->v2;
			if(old==0 || ed->is_draw) medge->flag= ME_EDGEDRAW|ME_EDGERENDER;
			if(ed->is_loose) medge->flag|= ME_LOOSEEDGE;

			/* order is swapped so extruding this edge as a surface wont flip face normals
			 * with cyclic curves */
			if(ed->v1+1 != ed->v2) {
				SWAP(unsigned int, medge->v1, medge->v2);
			}
			medge++;
		}
		else {
			/* equal edge, we merge the drawflag */
			(ed+1)->is_draw |= ed->is_draw;
		}
	}
	/* last edge */
	medge->v1= ed->v1;
	medge->v2= ed->v2;
	medge->flag= ME_EDGEDRAW;
	if(ed->is_loose) medge->flag|= ME_LOOSEEDGE;
	medge->flag |= ME_EDGERENDER;

	MEM_freeN(edsort);
}

void make_edges(Mesh *me, int old)
{
	MEdge *medge;
	int totedge=0;

	make_edges_mdata(me->mvert, me->mface, me->totvert, me->totface, old, &medge, &totedge);
	if(totedge==0) {
		/* flag that mesh has edges */
		me->medge = medge;
		me->totedge = 0;
		return;
	}

	medge= CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, totedge);
	me->medge= medge;
	me->totedge= totedge;

	mesh_strip_loose_faces(me);
}

void mesh_strip_loose_faces(Mesh *me)
{
	int a,b;

	for (a=b=0; a<me->totface; a++) {
		if (me->mface[a].v3) {
			if (a!=b) {
				memcpy(&me->mface[b],&me->mface[a],sizeof(me->mface[b]));
				CustomData_copy_data(&me->fdata, &me->fdata, a, b, 1);
				CustomData_free_elem(&me->fdata, a, 1);
			}
			b++;
		}
	}
	me->totface = b;
}

void mesh_strip_loose_edges(Mesh *me)
{
	int a,b;

	for (a=b=0; a<me->totedge; a++) {
		if (me->medge[a].v1!=me->medge[a].v2) {
			if (a!=b) {
				memcpy(&me->medge[b],&me->medge[a],sizeof(me->medge[b]));
				CustomData_copy_data(&me->edata, &me->edata, a, b, 1);
				CustomData_free_elem(&me->edata, a, 1);
			}
			b++;
		}
	}
	me->totedge = b;
}

void mball_to_mesh(ListBase *lb, Mesh *me)
{
	DispList *dl;
	MVert *mvert;
	MFace *mface;
	float *nors, *verts;
	int a, *index;
	
	dl= lb->first;
	if(dl==NULL) return;

	if(dl->type==DL_INDEX4) {
		me->totvert= dl->nr;
		me->totface= dl->parts;
		
		mvert= CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, dl->nr);
		mface= CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, dl->parts);
		me->mvert= mvert;
		me->mface= mface;

		a= dl->nr;
		nors= dl->nors;
		verts= dl->verts;
		while(a--) {
			copy_v3_v3(mvert->co, verts);
			normal_float_to_short_v3(mvert->no, nors);
			mvert++;
			nors+= 3;
			verts+= 3;
		}
		
		a= dl->parts;
		index= dl->index;
		while(a--) {
			mface->v1= index[0];
			mface->v2= index[1];
			mface->v3= index[2];
			mface->v4= index[3];
			mface->flag= ME_SMOOTH;

			test_index_face(mface, NULL, 0, (mface->v3==mface->v4)? 3: 4);

			mface++;
			index+= 4;
		}

		make_edges(me, 0);	// all edges
	}	
}

/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* return non-zero on error */
int nurbs_to_mdata(Object *ob, MVert **allvert, int *totvert,
	MEdge **alledge, int *totedge, MFace **allface, int *totface)
{
	return nurbs_to_mdata_customdb(ob, &ob->disp,
		allvert, totvert, alledge, totedge, allface, totface);
}

/* Initialize mverts, medges and, faces for converting nurbs to mesh and derived mesh */
/* use specified dispbase  */
int nurbs_to_mdata_customdb(Object *ob, ListBase *dispbase, MVert **allvert, int *_totvert,
	MEdge **alledge, int *_totedge, MFace **allface, int *_totface)
{
	DispList *dl;
	Curve *cu;
	MVert *mvert;
	MFace *mface;
	float *data;
	int a, b, ofs, vertcount, startvert, totvert=0, totvlak=0;
	int p1, p2, p3, p4, *index;
	int conv_polys= 0;

	cu= ob->data;

	conv_polys|= cu->flag & CU_3D;		/* 2d polys are filled with DL_INDEX3 displists */
	conv_polys|= ob->type == OB_SURF;	/* surf polys are never filled */

	/* count */
	dl= dispbase->first;
	while(dl) {
		if(dl->type==DL_SEGM) {
			totvert+= dl->parts*dl->nr;
			totvlak+= dl->parts*(dl->nr-1);
		}
		else if(dl->type==DL_POLY) {
			if(conv_polys) {
				totvert+= dl->parts*dl->nr;
				totvlak+= dl->parts*dl->nr;
			}
		}
		else if(dl->type==DL_SURF) {
			totvert+= dl->parts*dl->nr;
			totvlak+= (dl->parts-1+((dl->flag & DL_CYCL_V)==2))*(dl->nr-1+(dl->flag & DL_CYCL_U));
		}
		else if(dl->type==DL_INDEX3) {
			totvert+= dl->nr;
			totvlak+= dl->parts;
		}
		dl= dl->next;
	}

	if(totvert==0) {
		/* error("can't convert"); */
		/* Make Sure you check ob->data is a curve */
		return -1;
	}

	*allvert= mvert= MEM_callocN(sizeof (MVert) * totvert, "nurbs_init mvert");
	*allface= mface= MEM_callocN(sizeof (MFace) * totvlak, "nurbs_init mface");

	/* verts and faces */
	vertcount= 0;

	dl= dispbase->first;
	while(dl) {
		int smooth= dl->rt & CU_SMOOTH ? 1 : 0;

		if(dl->type==DL_SEGM) {
			startvert= vertcount;
			a= dl->parts*dl->nr;
			data= dl->verts;
			while(a--) {
				copy_v3_v3(mvert->co, data);
				data+=3;
				vertcount++;
				mvert++;
			}

			for(a=0; a<dl->parts; a++) {
				ofs= a*dl->nr;
				for(b=1; b<dl->nr; b++) {
					mface->v1= startvert+ofs+b-1;
					mface->v2= startvert+ofs+b;
					if(smooth) mface->flag |= ME_SMOOTH;
					mface++;
				}
			}

		}
		else if(dl->type==DL_POLY) {
			if(conv_polys) {
				startvert= vertcount;
				a= dl->parts*dl->nr;
				data= dl->verts;
				while(a--) {
					copy_v3_v3(mvert->co, data);
					data+=3;
					vertcount++;
					mvert++;
				}

				for(a=0; a<dl->parts; a++) {
					ofs= a*dl->nr;
					for(b=0; b<dl->nr; b++) {
						mface->v1= startvert+ofs+b;
						if(b==dl->nr-1) mface->v2= startvert+ofs;
						else mface->v2= startvert+ofs+b+1;
						if(smooth) mface->flag |= ME_SMOOTH;
						mface++;
					}
				}
			}
		}
		else if(dl->type==DL_INDEX3) {
			startvert= vertcount;
			a= dl->nr;
			data= dl->verts;
			while(a--) {
				copy_v3_v3(mvert->co, data);
				data+=3;
				vertcount++;
				mvert++;
			}

			a= dl->parts;
			index= dl->index;
			while(a--) {
				mface->v1= startvert+index[0];
				mface->v2= startvert+index[2];
				mface->v3= startvert+index[1];
				mface->v4= 0;
				mface->mat_nr= dl->col;
				test_index_face(mface, NULL, 0, 3);

				if(smooth) mface->flag |= ME_SMOOTH;
				mface++;
				index+= 3;
			}


		}
		else if(dl->type==DL_SURF) {
			startvert= vertcount;
			a= dl->parts*dl->nr;
			data= dl->verts;
			while(a--) {
				copy_v3_v3(mvert->co, data);
				data+=3;
				vertcount++;
				mvert++;
			}

			for(a=0; a<dl->parts; a++) {

				if( (dl->flag & DL_CYCL_V)==0 && a==dl->parts-1) break;

				if(dl->flag & DL_CYCL_U) {			/* p2 -> p1 -> */
					p1= startvert+ dl->nr*a;	/* p4 -> p3 -> */
					p2= p1+ dl->nr-1;		/* -----> next row */
					p3= p1+ dl->nr;
					p4= p2+ dl->nr;
					b= 0;
				}
				else {
					p2= startvert+ dl->nr*a;
					p1= p2+1;
					p4= p2+ dl->nr;
					p3= p1+ dl->nr;
					b= 1;
				}
				if( (dl->flag & DL_CYCL_V) && a==dl->parts-1) {
					p3-= dl->parts*dl->nr;
					p4-= dl->parts*dl->nr;
				}

				for(; b<dl->nr; b++) {
					mface->v1= p1;
					mface->v2= p3;
					mface->v3= p4;
					mface->v4= p2;
					mface->mat_nr= dl->col;
					test_index_face(mface, NULL, 0, 4);

					if(smooth) mface->flag |= ME_SMOOTH;
					mface++;

					p4= p3;
					p3++;
					p2= p1;
					p1++;
				}
			}

		}

		dl= dl->next;
	}

	*_totvert= totvert;
	*_totface= totvlak;

	make_edges_mdata(*allvert, *allface, totvert, totvlak, 0, alledge, _totedge);
	mfaces_strip_loose(*allface, _totface);

	return 0;
}

/* this may fail replacing ob->data, be sure to check ob->type */
void nurbs_to_mesh(Object *ob)
{
	Main *bmain= G.main;
	Object *ob1;
	DerivedMesh *dm= ob->derivedFinal;
	Mesh *me;
	Curve *cu;
	MVert *allvert= NULL;
	MEdge *alledge= NULL;
	MFace *allface= NULL;
	int totvert, totedge, totface;

	cu= ob->data;

	if (dm == NULL) {
		if (nurbs_to_mdata (ob, &allvert, &totvert, &alledge, &totedge, &allface, &totface) != 0) {
			/* Error initializing */
			return;
		}

		/* make mesh */
		me= add_mesh("Mesh");
		me->totvert= totvert;
		me->totface= totface;
		me->totedge= totedge;

		me->mvert= CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, allvert, me->totvert);
		me->medge= CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, alledge, me->totedge);
		me->mface= CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, allface, me->totface);

		mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
	} else {
		me= add_mesh("Mesh");
		DM_to_mesh(dm, me);
	}

	me->totcol= cu->totcol;
	me->mat= cu->mat;

	tex_space_mesh(me);

	cu->mat= NULL;
	cu->totcol= 0;

	if(ob->data) {
		free_libblock(&bmain->curve, ob->data);
	}
	ob->data= me;
	ob->type= OB_MESH;

	/* other users */
	ob1= bmain->object.first;
	while(ob1) {
		if(ob1->data==cu) {
			ob1->type= OB_MESH;
		
			ob1->data= ob->data;
			id_us_plus((ID *)ob->data);
		}
		ob1= ob1->id.next;
	}
}

typedef struct EdgeLink {
	Link *next, *prev;
	void *edge;
} EdgeLink;

typedef struct VertLink {
	Link *next, *prev;
	unsigned int index;
} VertLink;

static void prependPolyLineVert(ListBase *lb, unsigned int index)
{
	VertLink *vl= MEM_callocN(sizeof(VertLink), "VertLink");
	vl->index = index;
	BLI_addhead(lb, vl);
}

static void appendPolyLineVert(ListBase *lb, unsigned int index)
{
	VertLink *vl= MEM_callocN(sizeof(VertLink), "VertLink");
	vl->index = index;
	BLI_addtail(lb, vl);
}

void mesh_to_curve(Scene *scene, Object *ob)
{
	/* make new mesh data from the original copy */
	DerivedMesh *dm= mesh_get_derived_final(scene, ob, CD_MASK_MESH);

	MVert *mverts= dm->getVertArray(dm);
	MEdge *med, *medge= dm->getEdgeArray(dm);
	MFace *mf,  *mface= dm->getFaceArray(dm);

	int totedge = dm->getNumEdges(dm);
	int totface = dm->getNumFaces(dm);
	int totedges = 0;
	int i, needsFree = 0;

	/* only to detect edge polylines */
	EdgeHash *eh = BLI_edgehash_new();
	EdgeHash *eh_edge = BLI_edgehash_new();


	ListBase edges = {NULL, NULL};

	/* create edges from all faces (so as to find edges not in any faces) */
	mf= mface;
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

	med= medge;
	for(i=0; i<totedge; i++, med++) {
		if (!BLI_edgehash_haskey(eh, med->v1, med->v2)) {
			EdgeLink *edl= MEM_callocN(sizeof(EdgeLink), "EdgeLink");

			BLI_edgehash_insert(eh_edge, med->v1, med->v2, NULL);
			edl->edge= med;

			BLI_addtail(&edges, edl);	totedges++;
		}
	}
	BLI_edgehash_free(eh_edge, NULL);
	BLI_edgehash_free(eh, NULL);

	if(edges.first) {
		Curve *cu = add_curve(ob->id.name+2, OB_CURVE);
		cu->flag |= CU_3D;

		while(edges.first) {
			/* each iteration find a polyline and add this as a nurbs poly spline */

			ListBase polyline = {NULL, NULL}; /* store a list of VertLink's */
			int closed = FALSE;
			int totpoly= 0;
			MEdge *med_current= ((EdgeLink *)edges.last)->edge;
			unsigned int startVert= med_current->v1;
			unsigned int endVert= med_current->v2;
			int ok= TRUE;

			appendPolyLineVert(&polyline, startVert);	totpoly++;
			appendPolyLineVert(&polyline, endVert);		totpoly++;
			BLI_freelinkN(&edges, edges.last);			totedges--;

			while(ok) { /* while connected edges are found... */
				ok = FALSE;
				i= totedges;
				while(i) {
					EdgeLink *edl;

					i-=1;
					edl= BLI_findlink(&edges, i);
					med= edl->edge;

					if(med->v1==endVert) {
						endVert = med->v2;
						appendPolyLineVert(&polyline, med->v2);	totpoly++;
						BLI_freelinkN(&edges, edl);				totedges--;
						ok= TRUE;
					}
					else if(med->v2==endVert) {
						endVert = med->v1;
						appendPolyLineVert(&polyline, endVert);	totpoly++;
						BLI_freelinkN(&edges, edl);				totedges--;
						ok= TRUE;
					}
					else if(med->v1==startVert) {
						startVert = med->v2;
						prependPolyLineVert(&polyline, startVert);	totpoly++;
						BLI_freelinkN(&edges, edl);					totedges--;
						ok= TRUE;
					}
					else if(med->v2==startVert) {
						startVert = med->v1;
						prependPolyLineVert(&polyline, startVert);	totpoly++;
						BLI_freelinkN(&edges, edl);					totedges--;
						ok= TRUE;
					}
				}
			}

			/* Now we have a polyline, make into a curve */
			if(startVert==endVert) {
				BLI_freelinkN(&polyline, polyline.last);
				totpoly--;
				closed = TRUE;
			}

			/* --- nurbs --- */
			{
				Nurb *nu;
				BPoint *bp;
				VertLink *vl;

				/* create new 'nurb' within the curve */
				nu = (Nurb *)MEM_callocN(sizeof(Nurb), "MeshNurb");

				nu->pntsu= totpoly;
				nu->pntsv= 1;
				nu->orderu= 4;
				nu->flagu= CU_NURB_ENDPOINT | (closed ? CU_NURB_CYCLIC:0);	/* endpoint */
				nu->resolu= 12;

				nu->bp= (BPoint *)MEM_callocN(sizeof(BPoint)*totpoly, "bpoints");

				/* add points */
				vl= polyline.first;
				for (i=0, bp=nu->bp; i < totpoly; i++, bp++, vl=(VertLink *)vl->next) {
					copy_v3_v3(bp->vec, mverts[vl->index].co);
					bp->f1= SELECT;
					bp->radius = bp->weight = 1.0;
				}
				BLI_freelistN(&polyline);

				/* add nurb to curve */
				BLI_addtail(&cu->nurb, nu);
			}
			/* --- done with nurbs --- */
		}

		((Mesh *)ob->data)->id.us--;
		ob->data= cu;
		ob->type= OB_CURVE;

		/* curve objects can't contain DM in usual cases, we could free memory */
		needsFree= 1;
	}

	dm->needsFree = needsFree;
	dm->release(dm);

	if (needsFree) {
		ob->derivedFinal = NULL;

		/* curve object could have got bounding box only in special cases */
		if(ob->bb) {
			MEM_freeN(ob->bb);
			ob->bb= NULL;
		}
	}
}

void mesh_delete_material_index(Mesh *me, short index)
{
	MFace *mf;
	int i;

	for (i=0, mf=me->mface; i<me->totface; i++, mf++) {
		if (mf->mat_nr && mf->mat_nr>=index) 
			mf->mat_nr--;
	}
}

void mesh_set_smooth_flag(Object *meshOb, int enableSmooth) 
{
	Mesh *me = meshOb->data;
	int i;

	for (i=0; i<me->totface; i++) {
		MFace *mf = &((MFace*) me->mface)[i];

		if (enableSmooth) {
			mf->flag |= ME_SMOOTH;
		} else {
			mf->flag &= ~ME_SMOOTH;
		}
	}
}

void mesh_calc_normals(MVert *mverts, int numVerts, MFace *mfaces, int numFaces, float (*faceNors_r)[3]) 
{
	float (*tnorms)[3]= MEM_callocN(numVerts*sizeof(*tnorms), "tnorms");
	float (*fnors)[3]= (faceNors_r)? faceNors_r: MEM_callocN(sizeof(*fnors)*numFaces, "meshnormals");
	int i;

	for(i=0; i<numFaces; i++) {
		MFace *mf= &mfaces[i];
		float *f_no= fnors[i];
		float *n4 = (mf->v4)? tnorms[mf->v4]: NULL;
		float *c4 = (mf->v4)? mverts[mf->v4].co: NULL;

		if(mf->v4)
			normal_quad_v3(f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, mverts[mf->v4].co);
		else
			normal_tri_v3(f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co);

		accumulate_vertex_normals(tnorms[mf->v1], tnorms[mf->v2], tnorms[mf->v3], n4,
			f_no, mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, c4);
	}

	/* following Mesh convention; we use vertex coordinate itself for normal in this case */
	for(i=0; i<numVerts; i++) {
		MVert *mv= &mverts[i];
		float *no= tnorms[i];
		
		if(normalize_v3(no) == 0.0f)
			normalize_v3_v3(no, mv->co);

		normal_float_to_short_v3(mv->no, no);
	}
	
	MEM_freeN(tnorms);

	if(fnors != faceNors_r)
		MEM_freeN(fnors);
}

float (*mesh_getVertexCos(Mesh *me, int *numVerts_r))[3]
{
	int i, numVerts = me->totvert;
	float (*cos)[3] = MEM_mallocN(sizeof(*cos)*numVerts, "vertexcos1");

	if (numVerts_r) *numVerts_r = numVerts;
	for (i=0; i<numVerts; i++)
		copy_v3_v3(cos[i], me->mvert[i].co);

	return cos;
}

UvVertMap *make_uv_vert_map(struct MFace *mface, struct MTFace *tface, unsigned int totface, unsigned int totvert, int selected, float *limit)
{
	UvVertMap *vmap;
	UvMapVert *buf;
	MFace *mf;
	unsigned int a;
	int	i, totuv, nverts;

	totuv = 0;

	/* generate UvMapVert array */
	mf= mface;
	for(a=0; a<totface; a++, mf++)
		if(!selected || (!(mf->flag & ME_HIDE) && (mf->flag & ME_FACE_SEL)))
			totuv += (mf->v4)? 4: 3;
		
	if(totuv==0)
		return NULL;
	
	vmap= (UvVertMap*)MEM_callocN(sizeof(*vmap), "UvVertMap");
	if (!vmap)
		return NULL;

	vmap->vert= (UvMapVert**)MEM_callocN(sizeof(*vmap->vert)*totvert, "UvMapVert*");
	buf= vmap->buf= (UvMapVert*)MEM_callocN(sizeof(*vmap->buf)*totuv, "UvMapVert");

	if (!vmap->vert || !vmap->buf) {
		free_uv_vert_map(vmap);
		return NULL;
	}

	mf= mface;
	for(a=0; a<totface; a++, mf++) {
		if(!selected || (!(mf->flag & ME_HIDE) && (mf->flag & ME_FACE_SEL))) {
			nverts= (mf->v4)? 4: 3;

			for(i=0; i<nverts; i++) {
				buf->tfindex= i;
				buf->f= a;
				buf->separate = 0;
				buf->next= vmap->vert[*(&mf->v1 + i)];
				vmap->vert[*(&mf->v1 + i)]= buf;
				buf++;
			}
		}
	}
	
	/* sort individual uvs for each vert */
	for(a=0; a<totvert; a++) {
		UvMapVert *newvlist= NULL, *vlist=vmap->vert[a];
		UvMapVert *iterv, *v, *lastv, *next;
		float *uv, *uv2, uvdiff[2];

		while(vlist) {
			v= vlist;
			vlist= vlist->next;
			v->next= newvlist;
			newvlist= v;

			uv= tface[v->f].uv[v->tfindex];
			lastv= NULL;
			iterv= vlist;

			while(iterv) {
				next= iterv->next;

				uv2= tface[iterv->f].uv[iterv->tfindex];
				sub_v2_v2v2(uvdiff, uv2, uv);


				if(fabsf(uv[0]-uv2[0]) < limit[0] && fabsf(uv[1]-uv2[1]) < limit[1]) {
					if(lastv) lastv->next= next;
					else vlist= next;
					iterv->next= newvlist;
					newvlist= iterv;
				}
				else
					lastv=iterv;

				iterv= next;
			}

			newvlist->separate = 1;
		}

		vmap->vert[a]= newvlist;
	}
	
	return vmap;
}

UvMapVert *get_uv_map_vert(UvVertMap *vmap, unsigned int v)
{
	return vmap->vert[v];
}

void free_uv_vert_map(UvVertMap *vmap)
{
	if (vmap) {
		if (vmap->vert) MEM_freeN(vmap->vert);
		if (vmap->buf) MEM_freeN(vmap->buf);
		MEM_freeN(vmap);
	}
}

/* Generates a map where the key is the vertex and the value is a list
   of faces that use that vertex as a corner. The lists are allocated
   from one memory pool. */
void create_vert_face_map(ListBase **map, IndexNode **mem, const MFace *mface, const int totvert, const int totface)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert face map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totface*4, "vert face map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totface; ++i){
		for(j = 0; j < (mface[i].v4?4:3); ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[((unsigned int*)(&mface[i]))[j]], node);
		}
	}
}

/* Generates a map where the key is the vertex and the value is a list
   of edges that use that vertex as an endpoint. The lists are allocated
   from one memory pool. */
void create_vert_edge_map(ListBase **map, IndexNode **mem, const MEdge *medge, const int totvert, const int totedge)
{
	int i, j;
	IndexNode *node = NULL;
 
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert edge map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totedge * 2, "vert edge map mem");
	node = *mem;

	/* Find the users */
	for(i = 0; i < totedge; ++i){
		for(j = 0; j < 2; ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[((unsigned int*)(&medge[i].v1))[j]], node);
		}
	}
}

#ifdef USE_BMESH_FORWARD_COMPAT

void mesh_loops_to_mface_corners(CustomData *fdata, CustomData *ldata,
                                 CustomData *pdata, int lindex[4], int findex,
                                 const int polyindex,
                                 const int mf_len, /* 3 or 4 */

                                 /* cache values to avoid lookups every time */
                                 const int numTex, /* CustomData_number_of_layers(pdata, CD_MTEXPOLY) */
                                 const int numCol, /* CustomData_number_of_layers(ldata, CD_MLOOPCOL) */
                                 const int hasWCol /* CustomData_has_layer(ldata, CD_WEIGHT_MLOOPCOL) */
                                 )
{
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;
	
	for(i=0; i < numTex; i++){
		texface = CustomData_get_n(fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_get_n(pdata, CD_MTEXPOLY, polyindex, i);
		
		texface->tpage = texpoly->tpage;
		texface->flag = texpoly->flag;
		texface->transp = texpoly->transp;
		texface->mode = texpoly->mode;
		texface->tile = texpoly->tile;
		texface->unwrap = texpoly->unwrap;

		for (j=0; j < mf_len; j++) {
			mloopuv = CustomData_get_n(ldata, CD_MLOOPUV, lindex[j], i);
			texface->uv[j][0] = mloopuv->uv[0];
			texface->uv[j][1] = mloopuv->uv[1];
		}
	}

	for(i=0; i < numCol; i++){
		mcol = CustomData_get_n(fdata, CD_MCOL, findex, i);

		for (j=0; j < mf_len; j++) {
			mloopcol = CustomData_get_n(ldata, CD_MLOOPCOL, lindex[j], i);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;
		}
	}

	if (hasWCol) {
		mcol = CustomData_get(fdata,  findex, CD_WEIGHT_MCOL);

		for (j=0; j < mf_len; j++) {
			mloopcol = CustomData_get(ldata, lindex[j], CD_WEIGHT_MLOOPCOL);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;
		}
	}
}


/*
 * this function recreates a tesselation.
 * returns number of tesselation faces.
 */
int mesh_mpoly_to_mface(struct CustomData *fdata, struct CustomData *ldata,
	struct CustomData *pdata, int totface, int UNUSED(totloop), int totpoly)
{
	MLoop *mloop;

	int lindex[4];
	int i;
	int k;

	MPoly *mp, *mpoly;
	MFace *mface = NULL, *mf;
	BLI_array_declare(mface);

	const int numTex = CustomData_number_of_layers(pdata, CD_MTEXPOLY);
	const int numCol = CustomData_number_of_layers(ldata, CD_MLOOPCOL);
	const int hasWCol = CustomData_has_layer(ldata, CD_WEIGHT_MLOOPCOL);

	mpoly = CustomData_get_layer(pdata, CD_MPOLY);
	mloop = CustomData_get_layer(ldata, CD_MLOOP);

	mp = mpoly;
	k = 0;
	for (i = 0; i<totpoly; i++, mp++) {
		if (ELEM(mp->totloop, 3, 4)) {
			BLI_array_growone(mface);
			mf = &mface[k];

			mf->mat_nr = mp->mat_nr;
			mf->flag = mp->flag;

			mf->v1 = mp->loopstart + 0;
			mf->v2 = mp->loopstart + 1;
			mf->v3 = mp->loopstart + 2;
			mf->v4 = (mp->totloop == 4) ? (mp->loopstart + 3) : 0;

			/* abuse edcode for temp storage and clear next loop */
			mf->edcode = (char)mp->totloop; /* only ever 3 or 4 */

			k++;
		}
	}

	CustomData_free(fdata, totface);
	memset(fdata, 0, sizeof(CustomData));

	totface= k;

	CustomData_add_layer(fdata, CD_MFACE, CD_ASSIGN, mface, totface);

	CustomData_from_bmeshpoly(fdata, pdata, ldata, totface);

	mp = mpoly;
	k = 0;
	for (i = 0; i<totpoly; i++, mp++) {
		if (ELEM(mp->totloop, 3, 4)) {
			mf = &mface[k];

			if (mf->edcode == 3) {
				/*sort loop indices to ensure winding is correct*/
				/* NO SORT - looks like we can skip this */

				lindex[0] = mf->v1;
				lindex[1] = mf->v2;
				lindex[2] = mf->v3;
				lindex[3] = 0; /* unused */

				/*transform loop indices to vert indices*/
				mf->v1 = mloop[mf->v1].v;
				mf->v2 = mloop[mf->v2].v;
				mf->v3 = mloop[mf->v3].v;

				mesh_loops_to_mface_corners(fdata, ldata, pdata,
				                            lindex, k, i, 3,
				                            numTex, numCol, hasWCol);
				test_index_face(mf, fdata, k, 3);
			}
			else {
				/*sort loop indices to ensure winding is correct*/
				/* NO SORT - looks like we can skip this */

				lindex[0] = mf->v1;
				lindex[1] = mf->v2;
				lindex[2] = mf->v3;
				lindex[3] = mf->v4;

				/*transform loop indices to vert indices*/
				mf->v1 = mloop[mf->v1].v;
				mf->v2 = mloop[mf->v2].v;
				mf->v3 = mloop[mf->v3].v;
				mf->v4 = mloop[mf->v4].v;

				mesh_loops_to_mface_corners(fdata, ldata, pdata,
				                            lindex, k, i, 4,
				                            numTex, numCol, hasWCol);
				test_index_face(mf, fdata, k, 4);
			}

			mf->edcode= 0;

			k++;
		}
	}

	return k;
}

#endif /* USE_BMESH_FORWARD_COMPAT */



/* basic vertex data functions */
int minmax_mesh(Mesh *me, float min[3], float max[3])
{
	int i= me->totvert;
	MVert *mvert;
	for(mvert= me->mvert; i--; mvert++) {
		DO_MINMAX(mvert->co, min, max);
	}
	
	return (me->totvert != 0);
}

int mesh_center_median(Mesh *me, float cent[3])
{
	int i= me->totvert;
	MVert *mvert;
	zero_v3(cent);
	for(mvert= me->mvert; i--; mvert++) {
		add_v3_v3(cent, mvert->co);
	}
	/* otherwise we get NAN for 0 verts */
	if(me->totvert) {
		mul_v3_fl(cent, 1.0f/(float)me->totvert);
	}

	return (me->totvert != 0);
}

int mesh_center_bounds(Mesh *me, float cent[3])
{
	float min[3], max[3];
	INIT_MINMAX(min, max);
	if(minmax_mesh(me, min, max)) {
		mid_v3_v3v3(cent, min, max);
		return 1;
	}

	return 0;
}

void mesh_translate(Mesh *me, float offset[3], int do_keys)
{
	int i= me->totvert;
	MVert *mvert;
	for(mvert= me->mvert; i--; mvert++) {
		add_v3_v3(mvert->co, offset);
	}
	
	if (do_keys && me->key) {
		KeyBlock *kb;
		for (kb=me->key->block.first; kb; kb=kb->next) {
			float *fp= kb->data;
			for (i= kb->totelem; i--; fp+=3) {
				add_v3_v3(fp, offset);
			}
		}
	}
}


void BKE_mesh_ensure_navmesh(Mesh *me)
{
	if (!CustomData_has_layer(&me->fdata, CD_RECAST)) {
		int i;
		int numFaces = me->totface;
		int* recastData;
		CustomData_add_layer_named(&me->fdata, CD_RECAST, CD_CALLOC, NULL, numFaces, "recastData");
		recastData = (int*)CustomData_get_layer(&me->fdata, CD_RECAST);
		for (i=0; i<numFaces; i++) {
			recastData[i] = i+1;
		}
		CustomData_add_layer_named(&me->fdata, CD_RECAST, CD_REFERENCE, recastData, numFaces, "recastData");
	}
}
