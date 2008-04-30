/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"

#include "BIF_editmesh.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_multires.h"

#include "blendef.h"
#include "editmesh.h"

#include <math.h>

/* Returns the active multires level (currently applied to the mesh) */
MultiresLevel *current_level(Multires *mr)
{
	return BLI_findlink(&mr->levels, mr->current - 1);
}

/* Returns the nth multires level, starting at 1 */
MultiresLevel *multires_level_n(Multires *mr, int n)
{
	if(mr)
		return BLI_findlink(&mr->levels, n - 1);
	else
		return NULL;
}

/* Free and clear the temporary connectivity data */
static void multires_free_temp_data(MultiresLevel *lvl)
{
	if(lvl) {
		if(lvl->edge_boundary_states) MEM_freeN(lvl->edge_boundary_states);
		if(lvl->vert_edge_map) MEM_freeN(lvl->vert_edge_map);
		if(lvl->vert_face_map) MEM_freeN(lvl->vert_face_map);
		if(lvl->map_mem) MEM_freeN(lvl->map_mem);

		lvl->edge_boundary_states = NULL;
		lvl->vert_edge_map = lvl->vert_face_map = NULL;
		lvl->map_mem = NULL;
	}
}

/* Does not actually free lvl itself */
void multires_free_level(MultiresLevel *lvl)
{
	if(lvl) {
		if(lvl->faces) MEM_freeN(lvl->faces);
		if(lvl->edges) MEM_freeN(lvl->edges);
		if(lvl->colfaces) MEM_freeN(lvl->colfaces);
		
		multires_free_temp_data(lvl);
	}
}

void multires_free(Multires *mr)
{
	if(mr) {
		MultiresLevel* lvl= mr->levels.first;

		/* Free the first-level data */
		if(lvl) {
			CustomData_free(&mr->vdata, lvl->totvert);
			CustomData_free(&mr->fdata, lvl->totface);
			MEM_freeN(mr->edge_flags);
			MEM_freeN(mr->edge_creases);
		}

		while(lvl) {
			multires_free_level(lvl);			
			lvl= lvl->next;
		}

		MEM_freeN(mr->verts);

		BLI_freelistN(&mr->levels);

		MEM_freeN(mr);
	}
}

static MultiresLevel *multires_level_copy(MultiresLevel *orig)
{
	if(orig) {
		MultiresLevel *lvl= MEM_dupallocN(orig);
		
		lvl->next= lvl->prev= NULL;
		lvl->faces= MEM_dupallocN(orig->faces);
		lvl->colfaces= MEM_dupallocN(orig->colfaces);
		lvl->edges= MEM_dupallocN(orig->edges);
		lvl->edge_boundary_states = NULL;
		lvl->vert_edge_map= lvl->vert_face_map= NULL;
		lvl->map_mem= NULL;
		
		return lvl;
	}
	return NULL;
}

Multires *multires_copy(Multires *orig)
{
	const CustomDataMask vdata_mask= CD_MASK_MDEFORMVERT;

	if(orig) {
		Multires *mr= MEM_dupallocN(orig);
		MultiresLevel *lvl;
		
		mr->levels.first= mr->levels.last= NULL;
		
		for(lvl= orig->levels.first; lvl; lvl= lvl->next)
			BLI_addtail(&mr->levels, multires_level_copy(lvl));

		mr->verts= MEM_dupallocN(orig->verts);
		
		lvl= mr->levels.first;
		if(lvl) {
			CustomData_copy(&orig->vdata, &mr->vdata, vdata_mask, CD_DUPLICATE, lvl->totvert);
			CustomData_copy(&orig->fdata, &mr->fdata, CD_MASK_MTFACE, CD_DUPLICATE, lvl->totface);
			mr->edge_flags= MEM_dupallocN(orig->edge_flags);
			mr->edge_creases= MEM_dupallocN(orig->edge_creases);
		}
		
		return mr;
	}
	return NULL;
}

static void multires_get_vert(MVert *out, EditVert *eve, MVert *m, int i)
{
	if(eve) {
		VecCopyf(out->co, eve->co);
		out->flag= 0;
		if(eve->f & SELECT) out->flag |= 1;
		if(eve->h) out->flag |= ME_HIDE;
		eve->tmp.l= i;
	}
	else
		*out= *m;
}

void eed_to_medge_flag(EditEdge *eed, short *flag, char *crease)
{
	if(!eed || !flag) return;

	/* Would be nice if EditMesh edge flags could be unified with Mesh flags! */
	*flag= (eed->f & SELECT) | ME_EDGERENDER;
	if(eed->f2<2) *flag |= ME_EDGEDRAW;
	if(eed->f2==0) *flag |= ME_LOOSEEDGE;
	if(eed->sharp) *flag |= ME_SHARP;
	if(eed->seam) *flag |= ME_SEAM;
	if(eed->h & EM_FGON) *flag |= ME_FGON;
	if(eed->h & 1) *flag |= ME_HIDE;
	
	*crease= (char)(255.0*eed->crease);
}

static void multires_get_edge(MultiresEdge *e, EditEdge *eed, MEdge *m, short *flag, char *crease)
{
	if(eed) {
		e->v[0]= eed->v1->tmp.l;
		e->v[1]= eed->v2->tmp.l;
		eed_to_medge_flag(eed, flag, crease);
	} else {		
		e->v[0]= m->v1;
		e->v[1]= m->v2;
		*flag= m->flag;
		*crease= m->crease;
	}
}

static void multires_get_face(MultiresFace *f, EditFace *efa, MFace *m)
{
	if(efa) {
		MFace tmp;
		int j;
		tmp.v1= efa->v1->tmp.l;
		tmp.v2= efa->v2->tmp.l;
		tmp.v3= efa->v3->tmp.l;
		tmp.v4= 0;
		if(efa->v4) tmp.v4= efa->v4->tmp.l;
		test_index_face(&tmp, NULL, 0, efa->v4?4:3);
		for(j=0; j<4; ++j) f->v[j]= (&tmp.v1)[j];

		/* Flags */
		f->flag= efa->flag;
		if(efa->f & 1) f->flag |= ME_FACE_SEL;
		else f->flag &= ~ME_FACE_SEL;
		if(efa->h) f->flag |= ME_HIDE;
		f->mat_nr= efa->mat_nr;
	} else {		
		f->v[0]= m->v1;
		f->v[1]= m->v2;
		f->v[2]= m->v3;
		f->v[3]= m->v4;
		f->flag= m->flag;
		f->mat_nr= m->mat_nr;
	}
}

/* For manipulating vertex colors / uvs */
static void mcol_to_multires(MultiresColFace *mrf, MCol *mcol)
{
	char i;
	for(i=0; i<4; ++i) {
		mrf->col[i].a= mcol[i].a;
		mrf->col[i].r= mcol[i].r;
		mrf->col[i].g= mcol[i].g;
		mrf->col[i].b= mcol[i].b;
	}
}

/* 1 <= count <= 4 */
static void multires_col_avg(MultiresCol *avg, MultiresCol cols[4], char count)
{
	unsigned i;
	avg->a= avg->r= avg->g= avg->b= 0;
	for(i=0; i<count; ++i) {
		avg->a+= cols[i].a;
		avg->r+= cols[i].r;
		avg->g+= cols[i].g;
		avg->b+= cols[i].b;
	}
	avg->a/= count;
	avg->r/= count;
	avg->g/= count;
	avg->b/= count;
}

static void multires_col_avg2(MultiresCol *avg, MultiresCol *c1, MultiresCol *c2)
{
	MultiresCol in[2];
	in[0]= *c1;
	in[1]= *c2;
	multires_col_avg(avg,in,2);
}

void multires_load_cols(Mesh *me)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1), *cur;
	EditMesh *em= G.obedit ? G.editMesh : NULL;
	CustomData *src= em ? &em->fdata : &me->fdata;
	EditFace *efa= NULL;
	unsigned i,j;

	if(!CustomData_has_layer(src, CD_MCOL) && !CustomData_has_layer(src, CD_MTFACE)) return;

	/* Add texcol data */
	for(cur= me->mr->levels.first; cur; cur= cur->next)
		if(!cur->colfaces)
			cur->colfaces= MEM_callocN(sizeof(MultiresColFace)*cur->totface,"ColFaces");

	me->mr->use_col= CustomData_has_layer(src, CD_MCOL);

	if(em) efa= em->faces.first;
	for(i=0; i<lvl->totface; ++i) {
		MultiresColFace *f= &lvl->colfaces[i];

		if(me->mr->use_col)
			mcol_to_multires(f, em ? CustomData_em_get(src, efa->data, CD_MCOL) : &me->mcol[i*4]);
		
		if(em) efa= efa->next;
	}

	/* Update higher levels */
	lvl= lvl->next;
	while(lvl) {
		MultiresColFace *cf= lvl->colfaces;
		for(i=0; i<lvl->prev->totface; ++i) {
			const char sides= lvl->prev->faces[i].v[3]?4:3;
			MultiresCol cntr;
			
			/* Find average color of 4 (or 3 for triangle) verts */
			multires_col_avg(&cntr,lvl->prev->colfaces[i].col,sides);
			
			for(j=0; j<sides; ++j) {
				MultiresColFace *pf= &lvl->prev->colfaces[i];

				multires_col_avg2(&cf->col[0],
						  &pf->col[j],
						  &pf->col[j==0?sides-1:j-1]);
				cf->col[1]= pf->col[j];
				multires_col_avg2(&cf->col[2],
						  &pf->col[j],
						  &pf->col[j==sides-1?0:j+1]);
				cf->col[3]= cntr;
				
				++cf;
			}
		}
		lvl= lvl->next;
	}

	/* Update lower levels */
	lvl= me->mr->levels.last;
	lvl= lvl->prev;
	while(lvl) {
		unsigned curf= 0;
		for(i=0; i<lvl->totface; ++i) {
			MultiresFace *f= &lvl->faces[i];
			for(j=0; j<(f->v[3]?4:3); ++j) {
				lvl->colfaces[i].col[j]= lvl->next->colfaces[curf].col[1];
				++curf;
			}
		}
		lvl= lvl->prev;
	}
}

void multires_create(Object *ob, Mesh *me)
{
	MultiresLevel *lvl;
	EditMesh *em= G.obedit ? G.editMesh : NULL;
	EditVert *eve= NULL;
	EditFace *efa= NULL;
	EditEdge *eed= NULL;
	int i;
	
	lvl= MEM_callocN(sizeof(MultiresLevel), "multires level");

	if(me->pv) mesh_pmv_off(ob, me);

	me->mr= MEM_callocN(sizeof(Multires), "multires data");
	
	BLI_addtail(&me->mr->levels,lvl);
	me->mr->current= 1;
	me->mr->level_count= 1;
	me->mr->edgelvl= 1;
	me->mr->pinlvl= 1;
	me->mr->renderlvl= 1;
	
	/* Load mesh (or editmesh) into multires data */

	/* Load vertices and vdata (MDeformVerts) */
	lvl->totvert= em ? BLI_countlist(&em->verts) : me->totvert;
	me->mr->verts= MEM_callocN(sizeof(MVert)*lvl->totvert,"multires verts");
	multires_update_customdata(me->mr->levels.first, em, em ? &em->vdata : &me->vdata,
	                           &me->mr->vdata, CD_MDEFORMVERT);
	if(em) eve= em->verts.first;
	for(i=0; i<lvl->totvert; ++i) {
		multires_get_vert(&me->mr->verts[i], eve, &me->mvert[i], i);
		if(em) eve= eve->next;
	}

	/* Load faces and fdata (MTFaces) */
	lvl->totface= em ? BLI_countlist(&em->faces) : me->totface;
	lvl->faces= MEM_callocN(sizeof(MultiresFace)*lvl->totface,"multires faces");
	multires_update_customdata(me->mr->levels.first, em, em ? &em->fdata : &me->fdata,
	                           &me->mr->fdata, CD_MTFACE);
	if(em) efa= em->faces.first;
	for(i=0; i<lvl->totface; ++i) {
		multires_get_face(&lvl->faces[i], efa, &me->mface[i]);
		if(em) efa= efa->next;
	}

	/* Load edges and edge_flags */
	lvl->totedge= em ? BLI_countlist(&em->edges) : me->totedge;
	lvl->edges= MEM_callocN(sizeof(MultiresEdge)*lvl->totedge,"multires edges");
	me->mr->edge_flags= MEM_callocN(sizeof(short)*lvl->totedge, "multires edge flags");
	me->mr->edge_creases= MEM_callocN(sizeof(short)*lvl->totedge, "multires edge creases");
	if(em) eed= em->edges.first;
	for(i=0; i<lvl->totedge; ++i) {
		multires_get_edge(&lvl->edges[i], eed, &me->medge[i], &me->mr->edge_flags[i], &me->mr->edge_creases[i]);
		if(em) eed= eed->next;
	}

	multires_load_cols(me);
}

typedef struct MultiresMapNode {
	struct MultiresMapNode *next, *prev;
	unsigned Index;
} MultiresMapNode;

/* Produces temporary connectivity data for the multires lvl */
static void multires_calc_temp_data(MultiresLevel *lvl)
{
	unsigned i, j, emax;
	MultiresMapNode *indexnode= NULL;

	lvl->map_mem= MEM_mallocN(sizeof(MultiresMapNode)*(lvl->totedge*2 + lvl->totface*4), "map_mem");
	indexnode= lvl->map_mem;
	
	/* edge map */
	lvl->vert_edge_map= MEM_callocN(sizeof(ListBase)*lvl->totvert,"vert_edge_map");
	for(i=0; i<lvl->totedge; ++i) {
		for(j=0; j<2; ++j, ++indexnode) {
			indexnode->Index= i;
			BLI_addtail(&lvl->vert_edge_map[lvl->edges[i].v[j]], indexnode);
		}
	}

	/* face map */
       	lvl->vert_face_map= MEM_callocN(sizeof(ListBase)*lvl->totvert,"vert_face_map");
	for(i=0; i<lvl->totface; ++i){
		for(j=0; j<(lvl->faces[i].v[3]?4:3); ++j, ++indexnode) {
			indexnode->Index= i;
			BLI_addtail(&lvl->vert_face_map[lvl->faces[i].v[j]], indexnode);
		}
	}

	/* edge boundaries */
	emax = (lvl->prev ? (lvl->prev->totedge * 2) : lvl->totedge);
	lvl->edge_boundary_states= MEM_callocN(sizeof(char)*lvl->totedge, "edge_boundary_states");
	for(i=0; i<emax; ++i) {
		MultiresMapNode *n1= lvl->vert_face_map[lvl->edges[i].v[0]].first;
		unsigned total= 0;
		
		lvl->edge_boundary_states[i] = 1;
		while(n1 && lvl->edge_boundary_states[i] == 1) {
			MultiresMapNode *n2= lvl->vert_face_map[lvl->edges[i].v[1]].first;
			while(n2) {
				if(n1->Index == n2->Index) {
					++total;
					
					if(total > 1) {
						lvl->edge_boundary_states[i] = 0;
						break;
					}
				}
				
				n2= n2->next;
			}
			n1= n1->next;
		}
	}
}

/* CATMULL-CLARK
   ============= */

typedef struct MultiApplyData {
	/* Smooth faces */
	float *corner1, *corner2, *corner3, *corner4;
	char quad;

	/* Smooth edges */
	char boundary;
	float edge_face_neighbor_midpoints_accum[3];
	unsigned edge_face_neighbor_midpoints_total;
	float *endpoint1, *endpoint2;

	/* Smooth verts */
	/* uses 'char boundary' */
	float *original;
	int edge_count;
	float vert_face_neighbor_midpoints_average[3];
	float vert_edge_neighbor_midpoints_average[3];
	float boundary_edges_average[3];
} MultiApplyData;

/* Simply averages the four corners of a polygon. */
static float catmullclark_smooth_face(MultiApplyData *data, const unsigned i)
{
	const float total= data->corner1[i]+data->corner2[i]+data->corner3[i];
	return data->quad ? (total+data->corner4[i])/4 : total/3;
}

static float catmullclark_smooth_edge(MultiApplyData *data, const unsigned i)
{
	float accum= 0;
	unsigned count= 2;

	accum+= data->endpoint1[i] + data->endpoint2[i];

	if(!data->boundary) {
		accum+= data->edge_face_neighbor_midpoints_accum[i];
		count+= data->edge_face_neighbor_midpoints_total;
	}

	return accum / count;
}

static float catmullclark_smooth_vert(MultiApplyData *data, const unsigned i)
{
	if(data->boundary) {
		return data->original[i]*0.75 + data->boundary_edges_average[i]*0.25;
	} else {
		return (data->vert_face_neighbor_midpoints_average[i] +
			2*data->vert_edge_neighbor_midpoints_average[i] +
			data->original[i]*(data->edge_count-3))/data->edge_count;
	}
}



/* Call func count times, passing in[i] as the input and storing the output in out[i] */
static void multi_apply(float *out, MultiApplyData *data,
		 const unsigned count, float (*func)(MultiApplyData *, const unsigned))
{
	unsigned i;
	for(i=0; i<count; ++i)
		out[i]= func(data,i);
}

static short multires_vert_is_boundary(MultiresLevel *lvl, unsigned v)
{
	MultiresMapNode *node= lvl->vert_edge_map[v].first;
	while(node) {
		if(lvl->edge_boundary_states[node->Index])
			return 1;
		node= node->next;
	}
	return 0;
}

#define GET_FLOAT(array, i, j, stride) (((float*)((char*)(array)+((i)*(stride))))[(j)])

static void edge_face_neighbor_midpoints_accum(MultiApplyData *data, MultiresLevel *lvl,
					      void *array, const char stride, const MultiresEdge *e)
{
	ListBase *neighbors1= &lvl->vert_face_map[e->v[0]];
	ListBase *neighbors2= &lvl->vert_face_map[e->v[1]];
	MultiresMapNode *n1, *n2;
	unsigned j,count= 0;
	float *out= data->edge_face_neighbor_midpoints_accum;
	
	out[0]=out[1]=out[2]= 0;

	for(n1= neighbors1->first; n1; n1= n1->next) {
		for(n2= neighbors2->first; n2; n2= n2->next) {
			if(n1->Index == n2->Index) {
				for(j=0; j<3; ++j)
					out[j]+= GET_FLOAT(array,lvl->faces[n1->Index].mid,j,stride);
				++count;
			}
		}
	}

	data->edge_face_neighbor_midpoints_total= count;
}

static void vert_face_neighbor_midpoints_average(MultiApplyData *data, MultiresLevel *lvl,
						 void *array, const char stride, const unsigned i)
{
	ListBase *neighbors= &lvl->vert_face_map[i];
	MultiresMapNode *n1;
	unsigned j,count= 0;
	float *out= data->vert_face_neighbor_midpoints_average;

	out[0]=out[1]=out[2]= 0;

	for(n1= neighbors->first; n1; n1= n1->next) {
		for(j=0; j<3; ++j)
			out[j]+= GET_FLOAT(array,lvl->faces[n1->Index].mid,j,stride);
		++count;
	}
	for(j=0; j<3; ++j) out[j]/= count;
}

static void vert_edge_neighbor_midpoints_average(MultiApplyData *data, MultiresLevel *lvl,
						 void *array, const char stride, const unsigned i)
{
	ListBase *neighbors= &lvl->vert_edge_map[i];
	MultiresMapNode *n1;
	unsigned j,count= 0;
	float *out= data->vert_edge_neighbor_midpoints_average;

	out[0]=out[1]=out[2]= 0;

	for(n1= neighbors->first; n1; n1= n1->next) {
		for(j=0; j<3; ++j)
			out[j]+= (GET_FLOAT(array,lvl->edges[n1->Index].v[0],j,stride) +
				  GET_FLOAT(array,lvl->edges[n1->Index].v[1],j,stride)) / 2;
		++count;
	}
	for(j=0; j<3; ++j) out[j]/= count;
}

static void boundary_edges_average(MultiApplyData *data, MultiresLevel *lvl,
				   void *array, const char stride, const unsigned i)
{
	ListBase *neighbors= &lvl->vert_edge_map[i];
	MultiresMapNode *n1;
	unsigned j,count= 0;
	float *out= data->boundary_edges_average;

	out[0]=out[1]=out[2]= 0;
	
	for(n1= neighbors->first; n1; n1= n1->next) {
		const MultiresEdge *e= &lvl->edges[n1->Index];
		const unsigned end= e->v[0]==i ? e->v[1] : e->v[0];
		
		if(lvl->edge_boundary_states[n1->Index]) {
			for(j=0; j<3; ++j)
				out[j]+= GET_FLOAT(array,end,j,stride);
			++count;
		}
	}
	for(j=0; j<3; ++j) out[j]/= count;
}

/* END CATMULL-CLARK
   ================= */

/* Update vertex locations and vertex flags */
static void multires_update_vertices(Mesh *me, EditMesh *em)
{
	MultiresLevel *cr_lvl= current_level(me->mr), *pr_lvl= NULL,
		      *last_lvl= me->mr->levels.last;
	vec3f *pr_deltas= NULL, *cr_deltas= NULL, *swap_deltas= NULL;
	EditVert *eve= NULL;
	MultiApplyData data;
	int i, j;

	/* Prepare deltas */
	pr_deltas= MEM_callocN(sizeof(vec3f)*last_lvl->totvert, "multires deltas 1");
	cr_deltas= MEM_callocN(sizeof(vec3f)*last_lvl->totvert, "multires deltas 2");

	/* Calculate initial deltas -- current mesh subtracted from current level*/
	if(em) eve= em->verts.first;
	for(i=0; i<cr_lvl->totvert; ++i) {
		if(em) {
			VecSubf(&cr_deltas[i].x, eve->co, me->mr->verts[i].co);
			eve= eve->next;
		} else
			VecSubf(&cr_deltas[i].x, me->mvert[i].co, me->mr->verts[i].co);
	}


	/* Copy current level's vertex flags and clear the rest */
	if(em) eve= em->verts.first;	
	for(i=0; i < last_lvl->totvert; ++i) {
		if(i < cr_lvl->totvert) {
			MVert mvflag;
			multires_get_vert(&mvflag, eve, &me->mvert[i], i);
			if(em) eve= eve->next;
			me->mr->verts[i].flag= mvflag.flag;
		}
		else
			me->mr->verts[i].flag= 0;
	}

	/* If already on the highest level, copy current verts (including flags) into current level */
	if(cr_lvl == last_lvl) {
		if(em)
			eve= em->verts.first;
		for(i=0; i<cr_lvl->totvert; ++i) {
			multires_get_vert(&me->mr->verts[i], eve, &me->mvert[i], i);
			if(em) eve= eve->next;
		}
	}

	/* Update higher levels */
	pr_lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	cr_lvl= pr_lvl->next;
	while(cr_lvl) {
		multires_calc_temp_data(pr_lvl);		

		/* Swap the old/new deltas */
		swap_deltas= pr_deltas;
		pr_deltas= cr_deltas;
		cr_deltas= swap_deltas;

		/* Calculate and add new deltas
		   ============================ */
		for(i=0; i<pr_lvl->totface; ++i) {
			const MultiresFace *f= &pr_lvl->faces[i];
			data.corner1= &pr_deltas[f->v[0]].x;
			data.corner2= &pr_deltas[f->v[1]].x;
			data.corner3= &pr_deltas[f->v[2]].x;
			data.corner4= &pr_deltas[f->v[3]].x;
			data.quad= f->v[3] ? 1 : 0;
			multi_apply(&cr_deltas[f->mid].x, &data, 3, catmullclark_smooth_face);
			
			for(j=0; j<(data.quad?4:3); ++j)
				me->mr->verts[f->mid].flag |= me->mr->verts[f->v[j]].flag;
		}

		for(i=0; i<pr_lvl->totedge; ++i) {
			const MultiresEdge *e= &pr_lvl->edges[i];
			data.boundary= pr_lvl->edge_boundary_states[i];
			edge_face_neighbor_midpoints_accum(&data,pr_lvl,cr_deltas,sizeof(vec3f),e);
			data.endpoint1= &pr_deltas[e->v[0]].x;
			data.endpoint2= &pr_deltas[e->v[1]].x;
			multi_apply(&cr_deltas[e->mid].x, &data, 3, catmullclark_smooth_edge);
				
			for(j=0; j<2; ++j)
				me->mr->verts[e->mid].flag |= me->mr->verts[e->v[j]].flag;
		}

		for(i=0; i<pr_lvl->totvert; ++i) {
			data.boundary= multires_vert_is_boundary(pr_lvl,i);
			data.original= &pr_deltas[i].x;
			data.edge_count= BLI_countlist(&pr_lvl->vert_edge_map[i]);
			if(data.boundary)
				boundary_edges_average(&data,pr_lvl,pr_deltas,sizeof(vec3f),i);
			else {
				vert_face_neighbor_midpoints_average(&data,pr_lvl,cr_deltas,sizeof(vec3f),i);
				vert_edge_neighbor_midpoints_average(&data,pr_lvl,pr_deltas,sizeof(vec3f),i);
			}
			multi_apply(&cr_deltas[i].x, &data, 3, catmullclark_smooth_vert);
		}

		/* Apply deltas to vertex locations */
		for(i=0; (cr_lvl == last_lvl) && (i < cr_lvl->totvert); ++i) {
			VecAddf(me->mr->verts[i].co,
				me->mr->verts[i].co,
				&cr_deltas[i].x);			
		}

		multires_free_temp_data(pr_lvl);

		pr_lvl= pr_lvl->next;
		cr_lvl= cr_lvl->next;
	}
	if(pr_deltas) MEM_freeN(pr_deltas);
	if(cr_deltas) MEM_freeN(cr_deltas);

}

static void multires_update_faces(Mesh *me, EditMesh *em)
{
	MultiresLevel *cr_lvl= current_level(me->mr), *pr_lvl= NULL,
		      *last_lvl= me->mr->levels.last;
	char *pr_flag_damaged= NULL, *cr_flag_damaged= NULL, *or_flag_damaged= NULL,
	     *pr_mat_damaged= NULL, *cr_mat_damaged= NULL, *or_mat_damaged= NULL, *swap= NULL;
	EditFace *efa= NULL;
	unsigned i,j,curf;

	/* Find for each face whether flag/mat has changed */
	pr_flag_damaged= MEM_callocN(sizeof(char) * last_lvl->totface, "flag_damaged 1");
	cr_flag_damaged= MEM_callocN(sizeof(char) * last_lvl->totface, "flag_damaged 1");
	pr_mat_damaged= MEM_callocN(sizeof(char) * last_lvl->totface, "mat_damaged 1");
	cr_mat_damaged= MEM_callocN(sizeof(char) * last_lvl->totface, "mat_damaged 1");
	if(em) efa= em->faces.first;
	for(i=0; i<cr_lvl->totface; ++i) {
		MultiresFace mftmp;
		multires_get_face(&mftmp, efa, &me->mface[i]);
		if(cr_lvl->faces[i].flag != mftmp.flag)
			cr_flag_damaged[i]= 1;
		if(cr_lvl->faces[i].mat_nr != mftmp.mat_nr)
			cr_mat_damaged[i]= 1;

		/* Update current level */
		cr_lvl->faces[i].flag= mftmp.flag;
		cr_lvl->faces[i].mat_nr= mftmp.mat_nr;

		if(em) efa= efa->next;
	}
	or_flag_damaged= MEM_dupallocN(cr_flag_damaged);
	or_mat_damaged= MEM_dupallocN(cr_mat_damaged);

	/* Update lower levels */
	cr_lvl= cr_lvl->prev;
	while(cr_lvl) {
		swap= pr_flag_damaged;
		pr_flag_damaged= cr_flag_damaged;
		cr_flag_damaged= swap;

		swap= pr_mat_damaged;
		pr_mat_damaged= cr_mat_damaged;
		cr_mat_damaged= swap;

		curf= 0;
		for(i=0; i<cr_lvl->totface; ++i) {
			const int sides= cr_lvl->faces[i].v[3] ? 4 : 3;
			
			/* Check damages */
			for(j=0; j<sides; ++j, ++curf) {
				if(pr_flag_damaged[curf]) {
					cr_lvl->faces[i].flag= cr_lvl->next->faces[curf].flag;
					cr_flag_damaged[i]= 1;
				}
				if(pr_mat_damaged[curf]) {
					cr_lvl->faces[i].mat_nr= cr_lvl->next->faces[curf].mat_nr;
					cr_mat_damaged[i]= 1;
				}
			}
		}

		cr_lvl= cr_lvl->prev;
	}
	
	/* Clear to original damages */
	if(cr_flag_damaged) MEM_freeN(cr_flag_damaged);
	if(cr_mat_damaged) MEM_freeN(cr_mat_damaged);
	cr_flag_damaged= or_flag_damaged;
	cr_mat_damaged= or_mat_damaged;
	
	/* Update higher levels */
	pr_lvl= current_level(me->mr);
	cr_lvl= pr_lvl->next;
	while(cr_lvl) {
		swap= pr_flag_damaged;
		pr_flag_damaged= cr_flag_damaged;
		cr_flag_damaged= swap;

		swap= pr_mat_damaged;
		pr_mat_damaged= cr_mat_damaged;
		cr_mat_damaged= swap;

		/* Update faces */
		for(i=0, curf= 0; i<pr_lvl->totface; ++i) {
			const int sides= cr_lvl->prev->faces[i].v[3] ? 4 : 3;
			for(j=0; j<sides; ++j, ++curf) {
				if(pr_flag_damaged[i]) {
					cr_lvl->faces[curf].flag= pr_lvl->faces[i].flag;
					cr_flag_damaged[curf]= 1;
				}
				if(pr_mat_damaged[i]) {
					cr_lvl->faces[curf].mat_nr= pr_lvl->faces[i].mat_nr;
					cr_mat_damaged[curf]= 1;
				}
			}
		}

		pr_lvl= pr_lvl->next;
		cr_lvl= cr_lvl->next;
	}

	if(pr_flag_damaged) MEM_freeN(pr_flag_damaged);
	if(cr_flag_damaged) MEM_freeN(cr_flag_damaged);
	if(pr_mat_damaged) MEM_freeN(pr_mat_damaged);
	if(cr_mat_damaged) MEM_freeN(cr_mat_damaged);
}

static void multires_update_colors(Mesh *me, EditMesh *em)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	MultiresCol *pr_deltas= NULL, *cr_deltas= NULL;
	CustomData *src= em ? &em->fdata : &me->fdata;
	EditFace *efa= NULL;
	unsigned i,j,curf= 0;
	
	if(me->mr->use_col) {
		/* Calc initial deltas */
		cr_deltas= MEM_callocN(sizeof(MultiresCol)*lvl->totface*4,"initial color/uv deltas");

		if(em) efa= em->faces.first;
		for(i=0; i<lvl->totface; ++i) {
			MCol *col= em ? CustomData_em_get(src, efa->data, CD_MCOL) : &me->mcol[i*4];
			for(j=0; j<4; ++j) {
				if(me->mr->use_col) {
					cr_deltas[i*4+j].a= col[j].a - lvl->colfaces[i].col[j].a;
					cr_deltas[i*4+j].r= col[j].r - lvl->colfaces[i].col[j].r;
					cr_deltas[i*4+j].g= col[j].g - lvl->colfaces[i].col[j].g;
					cr_deltas[i*4+j].b= col[j].b - lvl->colfaces[i].col[j].b;
				}
			}
			if(em) efa= efa->next;
		}
		
		/* Update current level */
		if(em) efa= em->faces.first;
		for(i=0; i<lvl->totface; ++i) {
			MultiresColFace *f= &lvl->colfaces[i];

			if(me->mr->use_col)
				mcol_to_multires(f, em ? CustomData_em_get(src, efa->data, CD_MCOL) : &me->mcol[i*4]);
			
			if(em) efa= efa->next;
		}
		
		/* Update higher levels */
		lvl= lvl->next;
		while(lvl) {
			/* Set up new deltas, but keep the ones from the previous level */
			if(pr_deltas) MEM_freeN(pr_deltas);
			pr_deltas= cr_deltas;
			cr_deltas= MEM_callocN(sizeof(MultiresCol)*lvl->totface*4,"color deltas");

			curf= 0;
			for(i=0; i<lvl->prev->totface; ++i) {
				const char sides= lvl->prev->faces[i].v[3]?4:3;
				MultiresCol cntr;
				
				/* Find average color of 4 (or 3 for triangle) verts */
				multires_col_avg(&cntr,&pr_deltas[i*4],sides);
				
				for(j=0; j<sides; ++j) {
					multires_col_avg2(&cr_deltas[curf*4],
							  &pr_deltas[i*4+j],
							  &pr_deltas[i*4+(j==0?sides-1:j-1)]);
					cr_deltas[curf*4+1]= pr_deltas[i*4+j];
					multires_col_avg2(&cr_deltas[curf*4+2],
							  &pr_deltas[i*4+j],
							  &pr_deltas[i*4+(j==sides-1?0:j+1)]);
					cr_deltas[curf*4+3]= cntr;
					++curf;
				}
			}

			for(i=0; i<lvl->totface; ++i) {
				for(j=0; j<4; ++j) {
					lvl->colfaces[i].col[j].a+= cr_deltas[i*4+j].a;
					lvl->colfaces[i].col[j].r+= cr_deltas[i*4+j].r;
					lvl->colfaces[i].col[j].g+= cr_deltas[i*4+j].g;
					lvl->colfaces[i].col[j].b+= cr_deltas[i*4+j].b;
				}
			}

			lvl= lvl->next;
		}
		if(pr_deltas) MEM_freeN(pr_deltas);
		if(cr_deltas) MEM_freeN(cr_deltas);
		
		/* Update lower levels */
		lvl= me->mr->levels.last;
		lvl= lvl->prev;
		while(lvl) {
			MultiresColFace *nf= lvl->next->colfaces;
			for(i=0; i<lvl->totface; ++i) {
				MultiresFace *f= &lvl->faces[i];
				for(j=0; j<(f->v[3]?4:3); ++j) {
					lvl->colfaces[i].col[j]= nf->col[1];
					++nf;
				}
			}
			lvl= lvl->prev;
		}
	}
}

void multires_update_levels(Mesh *me, const int render)
{
	EditMesh *em= (!render && G.obedit) ? G.editMesh : NULL;

	multires_update_first_level(me, em);
	multires_update_vertices(me, em);
	multires_update_faces(me, em);
	multires_update_colors(me, em);
}

static void check_colors(Mesh *me)
{
	CustomData *src= G.obedit ? &G.editMesh->fdata : &me->fdata;
	const char col= CustomData_has_layer(src, CD_MCOL);

	/* Check if vertex colors have been deleted or added */
	if(me->mr->use_col && !col)
		me->mr->use_col= 0;
	else if(!me->mr->use_col && col) {
		me->mr->use_col= 1;
		multires_load_cols(me);
	}
}

static unsigned int find_mid_edge(ListBase *vert_edge_map,
				  MultiresLevel *lvl,
				  const unsigned int v1,
				  const unsigned int v2 )
{
	MultiresMapNode *n= vert_edge_map[v1].first;
	while(n) {
		if(lvl->edges[n->Index].v[0]==v2 ||
		   lvl->edges[n->Index].v[1]==v2)
			return lvl->edges[n->Index].mid;

		n= n->next;
	}
	return -1;
}

static float clamp_component(const float c)
{
	if(c<0) return 0;
	else if(c>255) return 255;
	else return c;
}

void multires_to_mcol(MultiresColFace *f, MCol mcol[4])
{
	unsigned char j;
	for(j=0; j<4; ++j) {
		mcol->a= clamp_component(f->col[j].a);
		mcol->r= clamp_component(f->col[j].r);
		mcol->g= clamp_component(f->col[j].g);
		mcol->b= clamp_component(f->col[j].b);
		++mcol;
	}
}

void multires_level_to_mesh(Object *ob, Mesh *me, const int render)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	int i;
	EditMesh *em= (!render && G.obedit) ? G.editMesh : NULL;
	
	if(em)
		return;

	CustomData_free_layer_active(&me->vdata, CD_MVERT, me->totvert);
	CustomData_free_layer_active(&me->edata, CD_MEDGE, me->totedge);
	CustomData_free_layer_active(&me->fdata, CD_MFACE, me->totface);
	CustomData_free_layer_active(&me->vdata, CD_MDEFORMVERT, me->totvert);
	CustomData_free_layers(&me->fdata, CD_MTFACE, me->totface);
	CustomData_free_layers(&me->fdata, CD_MCOL, me->totface);
		
	me->totvert= lvl->totvert;
	me->totface= lvl->totface;
	me->totedge= lvl->totedge;

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, me->totvert);
	CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, me->totedge);
	CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, me->totface);
	mesh_update_customdata_pointers(me);

	/* Vertices/Edges/Faces */
	
	for(i=0; i<lvl->totvert; ++i) {
		me->mvert[i]= me->mr->verts[i];
	}
	for(i=0; i<lvl->totedge; ++i) {
		me->medge[i].v1= lvl->edges[i].v[0];
		me->medge[i].v2= lvl->edges[i].v[1];
		me->medge[i].flag &= ~ME_HIDE;
	}
	for(i=0; i<lvl->totface; ++i) {
		me->mface[i].v1= lvl->faces[i].v[0];
		me->mface[i].v2= lvl->faces[i].v[1];
		me->mface[i].v3= lvl->faces[i].v[2];
		me->mface[i].v4= lvl->faces[i].v[3];
		me->mface[i].flag= lvl->faces[i].flag;
		me->mface[i].flag &= ~ME_HIDE;
		me->mface[i].mat_nr= lvl->faces[i].mat_nr;
	}
	
	/* Edge flags */
	if(lvl==me->mr->levels.first) {
		for(i=0; i<lvl->totedge; ++i) {
			me->medge[i].flag= me->mr->edge_flags[i];
			me->medge[i].crease= me->mr->edge_creases[i];
		}
	} else {
		MultiresLevel *lvl1= me->mr->levels.first;
		const int last= lvl1->totedge * pow(2, me->mr->current-1);
		for(i=0; i<last; ++i) {
			const int ndx= i / pow(2, me->mr->current-1);
			
			me->medge[i].flag= me->mr->edge_flags[ndx];
			me->medge[i].crease= me->mr->edge_creases[ndx];
		}
	}

	multires_customdata_to_mesh(me, em, lvl, &me->mr->vdata, em ? &em->vdata : &me->vdata, CD_MDEFORMVERT);
	multires_customdata_to_mesh(me, em, lvl, &me->mr->fdata, em ? &em->fdata : &me->fdata, CD_MTFACE);

	/* Colors */
	if(me->mr->use_col) {
		CustomData *src= &me->fdata;
		
		if(me->mr->use_col) me->mcol= CustomData_add_layer(src, CD_MCOL, CD_CALLOC, NULL, me->totface);
		
		for(i=0; i<lvl->totface; ++i) {
			if(me->mr->use_col)
				multires_to_mcol(&lvl->colfaces[i], &me->mcol[i*4]);
		}
			
	}
	
	mesh_update_customdata_pointers(me);
	
	multires_edge_level_update(ob,me);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

void multires_add_level(Object *ob, Mesh *me, const char subdiv_type)
{
	int i,j, curf, cure;
	MultiresLevel *lvl= NULL;
	MultiApplyData data;
	MVert *oldverts= NULL;
	
	lvl= MEM_callocN(sizeof(MultiresLevel), "multireslevel");
	if(me->pv) mesh_pmv_off(ob, me);

	check_colors(me);
	multires_update_levels(me, 0);

	++me->mr->level_count;
	BLI_addtail(&me->mr->levels,lvl);

	/* Create vertices
	   =============== */
	lvl->totvert= lvl->prev->totvert + lvl->prev->totedge + lvl->prev->totface;
	oldverts= me->mr->verts;
	me->mr->verts= MEM_callocN(sizeof(MVert)*lvl->totvert, "multitres verts");
	/* Copy old verts */
	for(i=0; i<lvl->prev->totvert; ++i)
		me->mr->verts[i]= oldverts[i];
	/* Create new edge verts */
	for(i=0; i<lvl->prev->totedge; ++i) {
		VecMidf(me->mr->verts[lvl->prev->totvert + i].co,
			oldverts[lvl->prev->edges[i].v[0]].co,
			oldverts[lvl->prev->edges[i].v[1]].co);
		lvl->prev->edges[i].mid= lvl->prev->totvert + i;
	}
	/* Create new face verts */
	for(i=0; i<lvl->prev->totface; ++i) {
		lvl->prev->faces[i].mid= lvl->prev->totvert + lvl->prev->totedge + i;
	}

	multires_calc_temp_data(lvl->prev);

	/* Create faces
	   ============ */
	/* Allocate all the new faces (each triangle creates three, and
	   each quad creates four */
	lvl->totface= 0;
	for(i=0; i<lvl->prev->totface; ++i)
		lvl->totface+= lvl->prev->faces[i].v[3] ? 4 : 3;
	lvl->faces= MEM_callocN(sizeof(MultiresFace)*lvl->totface,"multires faces");

	curf= 0;
	for(i=0; i<lvl->prev->totface; ++i) {
		const int max= lvl->prev->faces[i].v[3] ? 3 : 2;
		
		for(j=0; j<max+1; ++j) {
			lvl->faces[curf].v[0]= find_mid_edge(lvl->prev->vert_edge_map,lvl->prev,
							     lvl->prev->faces[i].v[j],
							     lvl->prev->faces[i].v[j==0?max:j-1]);
			lvl->faces[curf].v[1]= lvl->prev->faces[i].v[j];
			lvl->faces[curf].v[2]= find_mid_edge(lvl->prev->vert_edge_map,lvl->prev,
							     lvl->prev->faces[i].v[j],
							     lvl->prev->faces[i].v[j==max?0:j+1]);
			lvl->faces[curf].v[3]= lvl->prev->totvert + lvl->prev->totedge + i;
			lvl->faces[curf].flag= lvl->prev->faces[i].flag;
			lvl->faces[curf].mat_nr= lvl->prev->faces[i].mat_nr;

			++curf;
		}
	}

	/* Create edges
	   ============ */
	/* Figure out how many edges to allocate */
	lvl->totedge= lvl->prev->totedge*2;
	for(i=0; i<lvl->prev->totface; ++i)
		lvl->totedge+= lvl->prev->faces[i].v[3]?4:3;
	lvl->edges= MEM_callocN(sizeof(MultiresEdge)*lvl->totedge,"multires edges");

	for(i=0; i<lvl->prev->totedge; ++i) {
		lvl->edges[i*2].v[0]= lvl->prev->edges[i].v[0];
		lvl->edges[i*2].v[1]= lvl->prev->edges[i].mid;
		lvl->edges[i*2+1].v[0]= lvl->prev->edges[i].mid;
		lvl->edges[i*2+1].v[1]= lvl->prev->edges[i].v[1];
	}
	/* Add edges inside of old polygons */
	curf= 0;
	cure= lvl->prev->totedge*2;
	for(i=0; i<lvl->prev->totface; ++i) {
		for(j=0; j<(lvl->prev->faces[i].v[3]?4:3); ++j) {
			lvl->edges[cure].v[0]= lvl->faces[curf].v[2];
			lvl->edges[cure].v[1]= lvl->faces[curf].v[3];
			++cure;
			++curf;
		}
	}

	/* Smooth vertices
	   =============== */
	for(i=0; i<lvl->prev->totface; ++i) {
		const MultiresFace *f= &lvl->prev->faces[i];
		data.corner1= oldverts[f->v[0]].co;
		data.corner2= oldverts[f->v[1]].co;
		data.corner3= oldverts[f->v[2]].co;
		data.corner4= oldverts[f->v[3]].co;
		data.quad= f->v[3] ? 1 : 0;
		multi_apply(me->mr->verts[f->mid].co, &data, 3, catmullclark_smooth_face);
	}

	if(subdiv_type == 0) {
		for(i=0; i<lvl->prev->totedge; ++i) {
			const MultiresEdge *e= &lvl->prev->edges[i];
			data.boundary= lvl->prev->edge_boundary_states[i];
			edge_face_neighbor_midpoints_accum(&data,lvl->prev, me->mr->verts, sizeof(MVert),e);
			data.endpoint1= oldverts[e->v[0]].co;
			data.endpoint2= oldverts[e->v[1]].co;
			multi_apply(me->mr->verts[e->mid].co, &data, 3, catmullclark_smooth_edge);
		}
		
		for(i=0; i<lvl->prev->totvert; ++i) {
			data.boundary= multires_vert_is_boundary(lvl->prev,i);
			data.original= oldverts[i].co;
			data.edge_count= BLI_countlist(&lvl->prev->vert_edge_map[i]);
			if(data.boundary)
				boundary_edges_average(&data,lvl->prev, oldverts, sizeof(MVert),i);
			else {
				vert_face_neighbor_midpoints_average(&data,lvl->prev, me->mr->verts,
								     sizeof(MVert),i);
				vert_edge_neighbor_midpoints_average(&data,lvl->prev, oldverts,
								     sizeof(MVert),i);
			}
			multi_apply(me->mr->verts[i].co, &data, 3, catmullclark_smooth_vert);
		}
	}

	multires_free_temp_data(lvl->prev);
	MEM_freeN(oldverts);

	/* Vertex Colors
	   ============= */
	curf= 0;
	if(me->mr->use_col) {
		MultiresColFace *cf= MEM_callocN(sizeof(MultiresColFace)*lvl->totface,"Multirescolfaces");
		lvl->colfaces= cf;
		for(i=0; i<lvl->prev->totface; ++i) {
			const char sides= lvl->prev->faces[i].v[3]?4:3;
			MultiresCol cntr;

			/* Find average color of 4 (or 3 for triangle) verts */
			multires_col_avg(&cntr,lvl->prev->colfaces[i].col,sides);

			for(j=0; j<sides; ++j) {
				multires_col_avg2(&cf->col[0],
						  &lvl->prev->colfaces[i].col[j],
						  &lvl->prev->colfaces[i].col[j==0?sides-1:j-1]);
				cf->col[1]= lvl->prev->colfaces[i].col[j];
				multires_col_avg2(&cf->col[2],
						  &lvl->prev->colfaces[i].col[j],
						  &lvl->prev->colfaces[i].col[j==sides-1?0:j+1]);
				cf->col[3]= cntr;

				++cf;
			}
		}
	}

	me->mr->newlvl= me->mr->level_count;
	me->mr->current= me->mr->newlvl;
	/* Unless the render level has been set to something other than the
	   highest level (by the user), increment the render level to match
	   the highest available level */
	if(me->mr->renderlvl == me->mr->level_count - 1) me->mr->renderlvl= me->mr->level_count;

	multires_level_to_mesh(ob, me, 0);
}

void multires_set_level(Object *ob, Mesh *me, const int render)
{
	if(me->pv) mesh_pmv_off(ob, me);

	check_colors(me);
	multires_update_levels(me, render);

	me->mr->current= me->mr->newlvl;
	if(me->mr->current<1) me->mr->current= 1;
	else if(me->mr->current>me->mr->level_count) me->mr->current= me->mr->level_count;

	multires_level_to_mesh(ob, me, render);
}

/* Update the edge visibility flags to only show edges on or below the edgelvl */
void multires_edge_level_update(Object *ob, Mesh *me)
{
	if(!G.obedit) {
		MultiresLevel *cr_lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
		MultiresLevel *edge_lvl= BLI_findlink(&me->mr->levels,me->mr->edgelvl-1);
		const int threshold= edge_lvl->totedge * pow(2, me->mr->current - me->mr->edgelvl);
		unsigned i;

		for(i=0; i<cr_lvl->totedge; ++i) {
			const int ndx= me->pv ? me->pv->edge_map[i] : i;
			if(ndx != -1) { /* -1= hidden edge */
				if(me->mr->edgelvl >= me->mr->current || i<threshold)
					me->medge[ndx].flag |= ME_EDGEDRAW | ME_EDGERENDER;
				else
					me->medge[ndx].flag &= ~ME_EDGEDRAW & ~ME_EDGERENDER;
			}
		}
		
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}
}
