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
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the multiresolution modeling tools.
 *
 * BIF_multires.h
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_vec_types.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_mesh.h"

#include "BIF_screen.h"
#include "BIF_space.h"

#include "BDR_editobject.h"
#include "BDR_sculptmode.h"

#include "BSE_edit.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "multires.h"
#include "mydevice.h"
#include "parametrizer.h"

#include <math.h>
#include <string.h>

void Vec3fAvg3(float *out, float *v1, float *v2, float *v3)
{
	out[0]= (v1[0]+v2[0]+v3[0])/3;
	out[1]= (v1[1]+v2[1]+v3[1])/3;
	out[2]= (v1[2]+v2[2]+v3[2])/3;
}
void Vec3fAvg4(float *out, float *v1, float *v2, float *v3, float *v4)
{
	out[0]= (v1[0]+v2[0]+v3[0]+v4[0])/4;
	out[1]= (v1[1]+v2[1]+v3[1]+v4[1])/4;
	out[2]= (v1[2]+v2[2]+v3[2]+v4[2])/4;
}

short multires_edge_is_boundary(MultiresLevel *lvl, unsigned e)
{
	MultiresMapNode *n1= lvl->vert_face_map[lvl->edges[e].v[0]].first;
	unsigned total= 0;

	while(n1) {
		MultiresMapNode *n2= lvl->vert_face_map[lvl->edges[e].v[1]].first;
		while(n2) {
			if(n1->Index == n2->Index) {
				++total;

				if(total > 1)
					return 0;
			}

			n2= n2->next;
		}
		n1= n1->next;
	}

	return 1;
}

short multires_vert_is_boundary(MultiresLevel *lvl, unsigned v)
{
	MultiresMapNode *node= lvl->vert_edge_map[v].first;
	while(node) {
		if(multires_edge_is_boundary(lvl,node->Index))
			return 1;
		node= node->next;
	}
	return 0;
}

typedef struct FloatNode {
	struct FloatNode *next, *prev;
	float value;
} FloatNode;
typedef struct FloatArrayNode {
	struct FloatArrayNode *next, *prev;
	float *value;
} FloatArrayNode;

typedef struct MultiApplyData {
	/* Smooth faces */
	float *corner1, *corner2, *corner3, *corner4;
	char quad;

	/* Smooth edges */
	char boundary;
	float *edge_face_neighbor_midpoints_accum;
	unsigned edge_face_neighbor_midpoints_total;
	float *endpoint1, *endpoint2;

	/* Smooth verts */
	/* uses 'char boundary' */
	float *original;
	int edge_count;
	float *vert_face_neighbor_midpoints_average;
	float *vert_edge_neighbor_midpoints_average;
	float *boundary_edges_average;
} MultiApplyData;

/* CATMULL-CLARK
   ============= */

/* Simply averages the four corners of a polygon. */
float catmullclark_smooth_face(MultiApplyData *data, const unsigned i)
{
	const float total= data->corner1[i]+data->corner2[i]+data->corner3[i];
	return data->quad ? (total+data->corner4[i])/4 : total/3;
}

float catmullclark_smooth_edge(MultiApplyData *data, const unsigned i)
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
float catmullclark_smooth_vert(MultiApplyData *data, const unsigned i)
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
void multi_apply(float *out, MultiApplyData *data,
		 const unsigned count, float (*func)(MultiApplyData *, const unsigned))
{
	unsigned i;
	for(i=0; i<count; ++i)
		out[i]= func(data,i);
}

float get_float(void *array, const unsigned i, const unsigned j, const char stride)
{
	return ((float*)((char*)array+(i*stride)))[j];
}

void edge_face_neighbor_midpoints_accum(MultiApplyData *data, MultiresLevel *lvl,
					void *array, const char stride, const MultiresEdge *e)
{
	ListBase *neighbors1= &lvl->vert_face_map[e->v[0]];
	ListBase *neighbors2= &lvl->vert_face_map[e->v[1]];
	MultiresMapNode *n1, *n2;
	unsigned j,count= 0;
	float *out= MEM_callocN(sizeof(float)*3, "edge_face_neighbor_midpoints_average");
	
	out[0]=out[1]=out[2]= 0;

	for(n1= neighbors1->first; n1; n1= n1->next) {
		for(n2= neighbors2->first; n2; n2= n2->next) {
			if(n1->Index == n2->Index) {
				for(j=0; j<3; ++j)
					out[j]+= get_float(array,lvl->faces[n1->Index].mid,j,stride);
				++count;
			}
		}
	}

	data->edge_face_neighbor_midpoints_accum= out;
	data->edge_face_neighbor_midpoints_total= count;
}
void vert_face_neighbor_midpoints_average(MultiApplyData *data, MultiresLevel *lvl,
					  void *array, const char stride, const unsigned i)
{
	ListBase *neighbors= &lvl->vert_face_map[i];
	MultiresMapNode *n1;
	unsigned j,count= 0;
	float *out= MEM_callocN(sizeof(float)*3, "vert_face_neighbor_midpoints_average");

	out[0]=out[1]=out[2]= 0;

	for(n1= neighbors->first; n1; n1= n1->next) {
		for(j=0; j<3; ++j)
			out[j]+= get_float(array,lvl->faces[n1->Index].mid,j,stride);
		++count;
	}
	for(j=0; j<3; ++j) out[j]/= count;
	data->vert_face_neighbor_midpoints_average= out;
}
void vert_edge_neighbor_midpoints_average(MultiApplyData *data, MultiresLevel *lvl,
					  void *array, const char stride, const unsigned i)
{
	ListBase *neighbors= &lvl->vert_edge_map[i];
	MultiresMapNode *n1;
	unsigned j,count= 0;
	float *out= MEM_callocN(sizeof(float)*3, "vert_edge_neighbor_midpoints_average");

	out[0]=out[1]=out[2]= 0;

	for(n1= neighbors->first; n1; n1= n1->next) {
		for(j=0; j<3; ++j)
			out[j]+= (get_float(array,lvl->edges[n1->Index].v[0],j,stride) +
				  get_float(array,lvl->edges[n1->Index].v[1],j,stride)) / 2;
		++count;
	}
	for(j=0; j<3; ++j) out[j]/= count;
	data->vert_edge_neighbor_midpoints_average= out;
}
void boundary_edges_average(MultiApplyData *data, MultiresLevel *lvl,
			    void *array, const char stride, const unsigned i)
{
	ListBase *neighbors= &lvl->vert_edge_map[i];
	MultiresMapNode *n1;
	unsigned j,count= 0;
	float *out= MEM_callocN(sizeof(float)*3, "edge_boundary_average");

	out[0]=out[1]=out[2]= 0;
	
	for(n1= neighbors->first; n1; n1= n1->next) {
		const MultiresEdge *e= &lvl->edges[n1->Index];
		const unsigned end= e->v[0]==i ? e->v[1] : e->v[0];
		
		if(multires_edge_is_boundary(lvl,n1->Index)) {
			for(j=0; j<3; ++j)
				out[j]+= get_float(array,end,j,stride);
			++count;
		}
	}
	for(j=0; j<3; ++j) out[j]/= count;
	data->boundary_edges_average= out;
}

/* Five functions for manipulating MultiresColors */
void convert_to_multires_col(MultiresCol *mrc, MCol *mcol)
{
	mrc->a= mcol->a;
	mrc->r= mcol->r;
	mrc->g= mcol->g;
	mrc->b= mcol->b;
}
void convert_to_multires_uvcol(MultiresCol *mrc, TFace *t, const unsigned char j)
{
	convert_to_multires_col(mrc, (MCol*)(&t->col[j]));
	mrc->u= t->uv[j][0];
	mrc->v= t->uv[j][1];
}
float clamp_component(const float c)
{
	if(c<0) return 0;
	else if(c>255) return 255;
	else return c;
}

void multirestexcol_to_mcol(MultiresTexColFace *f, MCol mcol[4])
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

void convert_from_multires_col(MultiresCol *mrc, MCol *mcol)
{
	mcol->a= clamp_component(mrc->a);
	mcol->r= clamp_component(mrc->r);
	mcol->g= clamp_component(mrc->g);
	mcol->b= clamp_component(mrc->b);
}
void texcolface_to_tface(MultiresTexColFace *f, TFace *t)
{
	unsigned i;
	for(i=0; i<4; ++i) {
		convert_from_multires_col(&f->col[i], (MCol*)(&t->col[i]));
		t->uv[i][0]= f->col[i].u;
		t->uv[i][1]= f->col[i].v;
	}
	t->tpage= f->tex_page;
	t->flag= f->tex_flag;
	t->transp= f->tex_transp;
	t->mode= f->tex_mode;
	t->tile= f->tex_tile;
	t->unwrap= f->tex_unwrap;
}

/* 1 <= count <= 4 */
void multires_col_avg(MultiresCol *avg, MultiresCol cols[4], char count)
{
	unsigned i;
	avg->a= avg->r= avg->g= avg->b= avg->u= avg->v= 0;
	for(i=0; i<count; ++i) {
		avg->a+= cols[i].a;
		avg->r+= cols[i].r;
		avg->g+= cols[i].g;
		avg->b+= cols[i].b;
		avg->u+= cols[i].u;
		avg->v+= cols[i].v;
	}
	avg->a/= count;
	avg->r/= count;
	avg->g/= count;
	avg->b/= count;
	avg->u/= count;
	avg->v/= count;
}

void multires_col_avg2(MultiresCol *avg, MultiresCol *c1, MultiresCol *c2)
{
	MultiresCol in[2];
	in[0]= *c1;
	in[1]= *c2;
	multires_col_avg(avg,in,2);
}

void multires_add_dvert(MDeformVert *out, const MDeformVert *in, const float w)
{
	if(out && in) {
		int i, j;
		char found;

		for(i=0; i<in->totweight; ++i) {
			found= 0;
			for(j=0; j<out->totweight; ++j) {
				if(out->dw[j].def_nr==in->dw[i].def_nr) {
					out->dw[j].weight += w;
					found= 1;
				}
			}
			if(!found) {
				MDeformWeight *newdw= MEM_callocN(sizeof(MDeformWeight)*(out->totweight+1), "multires dvert");
				if(out->dw) {
					memcpy(newdw, out->dw, sizeof(MDeformWeight)*out->totweight);
					MEM_freeN(out->dw);
				}

				out->dw= newdw;
				out->dw[out->totweight].weight= w;
				out->dw[out->totweight].def_nr= in->dw[i].def_nr;

				++out->totweight;
			}
		}
	}
}

void multires_load_cols(Mesh *me)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1), *cur;
	unsigned i,j;

	if(!me->mcol && !me->tface) return;

	/* Add texcol data */
	for(cur= me->mr->levels.first; cur; cur= cur->next)
		if(!cur->texcolfaces)
			cur->texcolfaces= MEM_callocN(sizeof(MultiresTexColFace)*cur->totface,"TexColFaces");

	if(me->mcol) {
		me->mr->use_col= 1;
		for(i=0; i<me->totface; ++i)
			for(j=0; j<4; ++j)
				convert_to_multires_col(&lvl->texcolfaces[i].col[j],&me->mcol[i*4+j]);
	}

	if(me->tface) {
		me->mr->use_tex= 1;
		for(i=0; i<me->totface; ++i) {
			MultiresTexColFace *f= &lvl->texcolfaces[i];
			TFace *t= &me->tface[i];
			for(j=0; j<4; ++j)
				convert_to_multires_uvcol(&f->col[j],t,j);

			f->tex_page= t->tpage;
			f->tex_transp= t->transp;
			f->tex_mode= t->mode;
			f->tex_tile= t->tile;
			f->tex_unwrap= t->unwrap;
		}
	}

	/* Update higher levels */
	lvl= lvl->next;
	while(lvl) {
		MultiresTexColFace *cf= lvl->texcolfaces;
		for(i=0; i<lvl->prev->totface; ++i) {
			const char sides= lvl->prev->faces[i].v[3]?4:3;
			MultiresCol cntr;
			
			/* Find average color of 4 (or 3 for triangle) verts */
			multires_col_avg(&cntr,lvl->prev->texcolfaces[i].col,sides);
			
			for(j=0; j<sides; ++j) {
				MultiresTexColFace *pf= &lvl->prev->texcolfaces[i];

				multires_col_avg2(&cf->col[0],
						  &pf->col[j],
						  &pf->col[j==0?sides-1:j-1]);
				cf->col[1]= pf->col[j];
				multires_col_avg2(&cf->col[2],
						  &pf->col[j],
						  &pf->col[j==sides-1?0:j+1]);
				cf->col[3]= cntr;

				cf->tex_page= pf->tex_page;
				cf->tex_flag= pf->tex_flag;
				cf->tex_transp= pf->tex_transp;
				cf->tex_mode= pf->tex_mode;
				cf->tex_tile= pf->tex_tile;
				cf->tex_unwrap= pf->tex_unwrap;
				
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
				lvl->texcolfaces[i].col[j]= lvl->next->texcolfaces[curf].col[1];
				++curf;
			}
		}
		lvl= lvl->prev;
	}
}

void multires_make(void *ob, void *me_v)
{
	Mesh *me= me_v;
	MultiresLevel *lvl= MEM_callocN(sizeof(MultiresLevel), "multires level");
	int em= G.obedit!=NULL;
	int i;

	waitcursor(1);

	if(me->pv) sculptmode_pmv_off(me);

	me->mr= MEM_callocN(sizeof(Multires), "multires data");
	
	BLI_addtail(&me->mr->levels,lvl);
	me->mr->current= 1;
	me->mr->level_count= 1;
	me->mr->edgelvl= 1;
	me->mr->pinlvl= 1;
	me->mr->renderlvl= 1;

	/* Load mesh into modifier */
	if(em) exit_editmode(2);

	/* Load vertices */
	lvl->verts= MEM_callocN(sizeof(MVert)*me->totvert,"multires verts");
	lvl->totvert= me->totvert;
	for(i=0; i<me->totvert; ++i) {
		lvl->verts[i]= me->mvert[i];
	}

	/* Load faces */
	lvl->faces= MEM_callocN(sizeof(MultiresFace)*me->totface,"multires faces");
	lvl->totface= me->totface;
	for(i=0; i<me->totface; ++i) {
		MultiresFace* f= &lvl->faces[i];
		f->v[0]= me->mface[i].v1;
		f->v[1]= me->mface[i].v2;
		f->v[2]= me->mface[i].v3;
		f->v[3]= me->mface[i].v4;
		f->mid= 0;
		f->childrenstart= 0;
		f->flag= me->mface[i].flag;
	}

	/* Load edges */
	lvl->edges= MEM_callocN(sizeof(MultiresEdge)*me->totedge,"multires edges");
	lvl->totedge= me->totedge;
	for(i=0; i<me->totedge; ++i) {
		lvl->edges[i].v[0]= me->medge[i].v1;
		lvl->edges[i].v[1]= me->medge[i].v2;
		lvl->edges[i].mid= 0;
	}

	/* Load dverts */
	if(me->dvert) {
		me->mr->dverts= MEM_mallocN(sizeof(MDeformVert)*me->totvert, "MDeformVert");
		copy_dverts(me->mr->dverts, me->dvert, me->totvert);
	}

	multires_load_cols(me);

	multires_calc_level_maps(lvl);

	if(em) enter_editmode(0);
	
	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Make multires");

	waitcursor(0);
}

void multires_delete(void *ob, void *me_v)
{
	multires_free(me_v);

	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Delete multires");
}

void multires_free(Mesh *me)
{
	if(me->mr) {
		MultiresLevel* lvl= me->mr->levels.first;

		/* Free the first-level data */
		free_dverts(me->mr->dverts, lvl->totvert);

		while(lvl) {
			multires_free_level(lvl);			
			lvl= lvl->next;
		}

		BLI_freelistN(&me->mr->levels);

		MEM_freeN(me->mr);
		me->mr= NULL;
	}
}

/* Does not actually free lvl itself! */
void multires_free_level(MultiresLevel *lvl)
{
	if(lvl) {
		unsigned i;

		if(lvl->verts) MEM_freeN(lvl->verts);
		if(lvl->faces) MEM_freeN(lvl->faces);
		if(lvl->edges) MEM_freeN(lvl->edges);
		if(lvl->texcolfaces) MEM_freeN(lvl->texcolfaces);
		
		/* Free all vertex maps */
		for(i=0; i<lvl->totvert; ++i)
			BLI_freelistN(&lvl->vert_edge_map[i]);
		for(i=0; i<lvl->totvert; ++i)
			BLI_freelistN(&lvl->vert_face_map[i]);
		MEM_freeN(lvl->vert_edge_map);
		MEM_freeN(lvl->vert_face_map);
	}
}

void multires_del_lower(void *ob, void *me)
{
	Multires *mr= ((Mesh*)me)->mr;
	MultiresLevel *lvl= BLI_findlink(&mr->levels,mr->current-1);

	lvl= lvl->prev;
	while(lvl) {
		multires_free_level(lvl);
		BLI_freelinkN(&mr->levels,lvl);
		lvl= lvl->prev;
		mr->current-= 1;
		mr->level_count-= 1;
	}
	mr->newlvl= mr->current;

	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Multires delete lower");
}

void multires_del_higher(void *ob, void *me)
{
	Multires *mr= ((Mesh*)me)->mr;
	MultiresLevel *lvl= BLI_findlink(&mr->levels,mr->current-1);

	lvl= lvl->next;
	while(lvl) {
		multires_free_level(lvl);
		BLI_freelinkN(&mr->levels,lvl);
		lvl= lvl->next;
		mr->level_count-= 1;
	}

	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Multires delete higher");
}

unsigned int find_mid_edge(ListBase *vert_edge_map,
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

void check_colors(Mesh *me)
{
	/* Check if vertex colors have been deleted or added */
	if(me->mr->use_col && !me->mcol)
		me->mr->use_col= 0;
	else if(!me->mr->use_col && me->mcol) {
		me->mr->use_col= 1;
		multires_load_cols(me);
	}

	/* Check if texfaces have been deleted or added */
	if(me->mr->use_tex && !me->tface)
		me->mr->use_tex= 0;
	else if(!me->mr->use_tex && me->tface) {
		me->mr->use_tex= 1;
		multires_load_cols(me);
	}
}

void multires_add_level(void *ob, void *me_v)
{
	int i,j, curf, cure;
	Mesh *me= me_v;
	MultiresLevel *lvl= MEM_callocN(sizeof(MultiresLevel), "multireslevel");
	int em= G.obedit!=NULL;
	MultiApplyData data;

	waitcursor(1);

	if(me->pv) sculptmode_pmv_off(me);
		
	if(em) exit_editmode(2);

	check_colors(me);

	++me->mr->level_count;
	BLI_addtail(&me->mr->levels,lvl);

	/* Create vertices
	   =============== */
	lvl->totvert= lvl->prev->totvert + lvl->prev->totedge + lvl->prev->totface;
	lvl->verts= MEM_callocN(sizeof(MVert)*lvl->totvert,"multires verts");
	/* Copy previous level's verts */
	for(i=0; i<lvl->prev->totvert; ++i)
		VecCopyf(lvl->verts[i].co,lvl->prev->verts[i].co);
	/* Create new edge verts */
	for(i=0; i<lvl->prev->totedge; ++i) {
		VecMidf(lvl->verts[lvl->prev->totvert + i].co,
			lvl->prev->verts[lvl->prev->edges[i].v[0]].co,
			lvl->prev->verts[lvl->prev->edges[i].v[1]].co);
		lvl->prev->edges[i].mid= lvl->prev->totvert + i;
	}
	/* Create new face verts */
	for(i=0; i<lvl->prev->totface; ++i) {
		lvl->prev->faces[i].mid= lvl->prev->totvert + lvl->prev->totedge + i;
	}

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
		
		lvl->prev->faces[i].childrenstart= curf;
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

	multires_calc_level_maps(lvl);
	
	/* Smooth vertices
	   =============== */
	for(i=0; i<lvl->prev->totface; ++i) {
		const MultiresFace *f= &lvl->prev->faces[i];
		data.corner1= lvl->prev->verts[f->v[0]].co;
		data.corner2= lvl->prev->verts[f->v[1]].co;
		data.corner3= lvl->prev->verts[f->v[2]].co;
		data.corner4= lvl->prev->verts[f->v[3]].co;
		data.quad= f->v[3] ? 1 : 0;
		multi_apply(lvl->verts[f->mid].co, &data, 3, catmullclark_smooth_face);
	}

	for(i=0; i<lvl->prev->totedge; ++i) {
		const MultiresEdge *e= &lvl->prev->edges[i];
		data.boundary= multires_edge_is_boundary(lvl->prev,i);
		edge_face_neighbor_midpoints_accum(&data,lvl->prev,lvl->verts,sizeof(MVert),e);
		data.endpoint1= lvl->prev->verts[e->v[0]].co;
		data.endpoint2= lvl->prev->verts[e->v[1]].co;
		multi_apply(lvl->verts[e->mid].co, &data, 3, catmullclark_smooth_edge);
		MEM_freeN(data.edge_face_neighbor_midpoints_accum);
	}
	
	for(i=0; i<lvl->prev->totvert; ++i) {
		data.boundary= multires_vert_is_boundary(lvl->prev,i);
		data.original= lvl->verts[i].co;
		data.edge_count= BLI_countlist(&lvl->prev->vert_edge_map[i]);
		if(data.boundary)
			boundary_edges_average(&data,lvl->prev,lvl->prev->verts,sizeof(MVert),i);
		else {
			vert_face_neighbor_midpoints_average(&data,lvl->prev,lvl->verts,sizeof(MVert),i);
			vert_edge_neighbor_midpoints_average(&data,lvl->prev,lvl->prev->verts,sizeof(MVert),i);
		}
		multi_apply(lvl->verts[i].co, &data, 3, catmullclark_smooth_vert);
		if(data.boundary)
			MEM_freeN(data.boundary_edges_average);
		else {
			MEM_freeN(data.vert_face_neighbor_midpoints_average);
			MEM_freeN(data.vert_edge_neighbor_midpoints_average);
		}
	}

	/* Vertex Colors
	   ============= */
	curf= 0;
	if(me->mr->use_col || me->mr->use_tex) {
		MultiresTexColFace *cf= MEM_callocN(sizeof(MultiresTexColFace)*lvl->totface,"MultiresTexColFaces");
		lvl->texcolfaces= cf;
		for(i=0; i<lvl->prev->totface; ++i) {
			const char sides= lvl->prev->faces[i].v[3]?4:3;
			MultiresCol cntr;

			/* Find average color of 4 (or 3 for triangle) verts */
			multires_col_avg(&cntr,lvl->prev->texcolfaces[i].col,sides);

			for(j=0; j<sides; ++j) {
				multires_col_avg2(&cf->col[0],
						  &lvl->prev->texcolfaces[i].col[j],
						  &lvl->prev->texcolfaces[i].col[j==0?sides-1:j-1]);
				cf->col[1]= lvl->prev->texcolfaces[i].col[j];
				multires_col_avg2(&cf->col[2],
						  &lvl->prev->texcolfaces[i].col[j],
						  &lvl->prev->texcolfaces[i].col[j==sides-1?0:j+1]);
				cf->col[3]= cntr;
				
				cf->tex_page= lvl->prev->texcolfaces[i].tex_page;
				cf->tex_flag= lvl->prev->texcolfaces[i].tex_flag;
				cf->tex_transp= lvl->prev->texcolfaces[i].tex_transp;
				cf->tex_mode= lvl->prev->texcolfaces[i].tex_mode;
				cf->tex_tile= lvl->prev->texcolfaces[i].tex_tile;
				cf->tex_unwrap= lvl->prev->texcolfaces[i].tex_unwrap;

				++cf;
			}
		}
	}

	multires_update_levels(me);
	me->mr->newlvl= me->mr->level_count;
	me->mr->current= me->mr->newlvl;
	multires_level_to_mesh(ob,me);
	if(em) enter_editmode(0);
	
	allqueue(REDRAWBUTSEDIT, 0);

	BIF_undo_push("Add multires level");

	waitcursor(0);
}

void multires_set_level(void *ob, void *me_v)
{
	Mesh *me= me_v;
	int em= G.obedit!=NULL;

	waitcursor(1);

	if(me->pv) sculptmode_pmv_off(me);

	if(em) exit_editmode(2);

	check_colors(me);
	multires_update_levels(me);

	me->mr->current= me->mr->newlvl;
	if(me->mr->current<1) me->mr->current= 1;
	else if(me->mr->current>me->mr->level_count) me->mr->current= me->mr->level_count;

	multires_level_to_mesh(ob,me);

	if(em) enter_editmode(0);

	allqueue(REDRAWBUTSEDIT, 0);
	
	waitcursor(0);
}

void multires_level_to_mesh(Object *ob, Mesh *me)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	int i,sm= G.f & G_SCULPTMODE;
	if(sm) set_sculptmode();

	if(me->mvert) MEM_freeN(me->mvert);
	if(me->mface) MEM_freeN(me->mface);
	if(me->medge) MEM_freeN(me->medge);
	free_dverts(me->dvert, me->totvert);
	
	me->totvert= lvl->totvert;
	me->totface= lvl->totface;
	me->totedge= lvl->totedge;

	me->mvert= MEM_callocN(sizeof(MVert)*me->totvert, "multires dlm mverts");
	me->mface= MEM_callocN(sizeof(MFace)*me->totface, "multires dlm mfaces");
	me->medge= MEM_callocN(sizeof(MEdge)*me->totedge, "multires dlm medges");

	/* Vertices/Edges/Faces */
	for(i=0; i<lvl->totvert; ++i)
		me->mvert[i]= lvl->verts[i];
	for(i=0; i<lvl->totface; ++i) {
		me->mface[i].v1= lvl->faces[i].v[0];
		me->mface[i].v2= lvl->faces[i].v[1];
		me->mface[i].v3= lvl->faces[i].v[2];
		me->mface[i].v4= lvl->faces[i].v[3];
		me->mface[i].flag= lvl->faces[i].flag;
	}
	for(i=0; i<lvl->totedge; ++i) {
		me->medge[i].v1= lvl->edges[i].v[0];
		me->medge[i].v2= lvl->edges[i].v[1];
	}

	/* Vertex groups */
	if(me->mr->dverts && lvl==me->mr->levels.first) {
		me->dvert= MEM_mallocN(sizeof(MDeformVert)*me->totvert, "MDeformVert");
		copy_dverts(me->dvert, me->mr->dverts, lvl->totvert);
	} else if(me->mr->dverts) {
		MultiresLevel *dlvl, *lvl1= me->mr->levels.first;
		MDeformVert **lvl_dverts;
		MDeformVert *source;
		int dlvl_ndx= 0;
		int j;

		lvl_dverts= MEM_callocN(sizeof(MDeformVert*) * (me->mr->current-1), "dvert prop array");
		
		/* dverts are not (yet?) propagated with catmull-clark  */
		for(dlvl= lvl1->next; dlvl && dlvl != lvl->next; dlvl= dlvl->next) {
			lvl_dverts[dlvl_ndx]= MEM_callocN(sizeof(MDeformVert)*dlvl->totvert, "dvert prop data");

			source= dlvl->prev==lvl1 ? me->mr->dverts : lvl_dverts[dlvl_ndx-1];

			/* Copy lower level */
			for(i=0; i<dlvl->prev->totvert; ++i)
				multires_add_dvert(&lvl_dverts[dlvl_ndx][i],
						   &source[i], 1);
			/* Edge verts */
			for(i=0; i<dlvl->prev->totedge; ++i) {
				multires_add_dvert(&lvl_dverts[dlvl_ndx][dlvl->prev->totvert+i],
						   &source[dlvl->prev->edges[i].v[0]],0.5);
				multires_add_dvert(&lvl_dverts[dlvl_ndx][dlvl->prev->totvert+i],
						   &source[dlvl->prev->edges[i].v[1]],0.5);
			}
			/* Face verts */
			for(i=0; i<dlvl->prev->totface; ++i) {
				for(j=0; j<(dlvl->prev->faces[i].v[3]?4:3); ++j)
					multires_add_dvert(&lvl_dverts[dlvl_ndx][dlvl->prev->totvert+dlvl->prev->totedge+i],
							   &source[dlvl->prev->faces[i].v[j]],
							   dlvl->prev->faces[i].v[3]?0.25:(1/3));
			}

			++dlvl_ndx;
		}

		dlvl= lvl1->next;
		for(i=0; i<(dlvl_ndx-1); ++i) {
			for(j=0; j<dlvl->totvert; ++j)
				if(lvl_dverts[i][j].dw) MEM_freeN(lvl_dverts[i][j].dw);
			MEM_freeN(lvl_dverts[i]);
		}

		me->dvert= lvl_dverts[dlvl_ndx-1];

		MEM_freeN(lvl_dverts);
	}

	if(me->mr->use_tex) {
		if(me->tface) MEM_freeN(me->tface);
		me->tface= MEM_callocN(sizeof(TFace)*me->totface, "multires dlm tface");
		
		for(i=0; i<lvl->totface; ++i)
			texcolface_to_tface(&lvl->texcolfaces[i],&me->tface[i]);
			
	} else if(me->mr->use_col) {
		if(me->mcol) MEM_freeN(me->mcol);
		me->mcol= MEM_callocN(sizeof(MCol)*me->totface*4, "multires dlm mcol");

		for(i=0; i<lvl->totface; ++i)
			multirestexcol_to_mcol(&lvl->texcolfaces[i], &me->mcol[i*4]);
	}
	multires_edge_level_update(ob,me);
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);

	if(sm) set_sculptmode();

	countall();

	allqueue(REDRAWVIEW3D, 0);
}

void multires_update_colors(Mesh *me)
{
	MultiresLevel *lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	MultiresCol *pr_deltas= NULL, *cr_deltas= NULL;
	unsigned i,j,curf= 0;

	if(me->mr->use_col || me->mr->use_tex) {
		/* Calc initial deltas */
		cr_deltas= MEM_callocN(sizeof(MultiresCol)*lvl->totface*4,"initial color/uv deltas");
		if(me->mr->use_tex) {
			for(i=0; i<lvl->totface; ++i) {
				for(j=0; j<4; ++j) {
					MultiresCol col;
					convert_to_multires_uvcol(&col,&me->tface[i],j);
					cr_deltas[i*4+j].a= col.a - lvl->texcolfaces[i].col[j].a;
					cr_deltas[i*4+j].r= col.r - lvl->texcolfaces[i].col[j].r;
					cr_deltas[i*4+j].g= col.g - lvl->texcolfaces[i].col[j].g;
					cr_deltas[i*4+j].b= col.b - lvl->texcolfaces[i].col[j].b;
					cr_deltas[i*4+j].u= col.u - lvl->texcolfaces[i].col[j].u;
					cr_deltas[i*4+j].v= col.v - lvl->texcolfaces[i].col[j].v;
				}
			}
		} else if(me->mr->use_col) {
			for(i=0; i<lvl->totface; ++i) {
				for(j=0; j<4; ++j) {
					cr_deltas[i*4+j].a= me->mcol[i*4+j].a - lvl->texcolfaces[i].col[j].a;
					cr_deltas[i*4+j].r= me->mcol[i*4+j].r - lvl->texcolfaces[i].col[j].r;
					cr_deltas[i*4+j].g= me->mcol[i*4+j].g - lvl->texcolfaces[i].col[j].g;
					cr_deltas[i*4+j].b= me->mcol[i*4+j].b - lvl->texcolfaces[i].col[j].b;
				}
			}
		}
		
		/* Update current level */
		for(i=0; i<lvl->totface; ++i) {
			for(j=0; j<4; ++j) {
				if(me->mr->use_tex)
					convert_to_multires_uvcol(&lvl->texcolfaces[i].col[j],&me->tface[i],j);
				else
					convert_to_multires_col(&lvl->texcolfaces[i].col[j],&me->mcol[i*4+j]);
			}
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
					lvl->texcolfaces[i].col[j].a+= cr_deltas[i*4+j].a;
					lvl->texcolfaces[i].col[j].r+= cr_deltas[i*4+j].r;
					lvl->texcolfaces[i].col[j].g+= cr_deltas[i*4+j].g;
					lvl->texcolfaces[i].col[j].b+= cr_deltas[i*4+j].b;
					lvl->texcolfaces[i].col[j].u+= cr_deltas[i*4+j].u;
					lvl->texcolfaces[i].col[j].v+= cr_deltas[i*4+j].v;
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
			MultiresTexColFace *nf= lvl->next->texcolfaces;
			for(i=0; i<lvl->totface; ++i) {
				MultiresFace *f= &lvl->faces[i];
				for(j=0; j<(f->v[3]?4:3); ++j) {
					lvl->texcolfaces[i].col[j]= nf->col[1];
					++nf;
				}
			}
			lvl= lvl->prev;
		}
	}
}

void multires_update_levels(Mesh *me)
{
	MultiresLevel *cr_lvl= BLI_findlink(&me->mr->levels,me->mr->current-1), *pr_lvl;
	vec3f *pr_deltas= NULL, *cr_deltas= NULL;
	MultiApplyData data;
	unsigned i,j,curf;

	/* Update special first-level data */
	if(cr_lvl==me->mr->levels.first) {
		/* Update deformverts */
		free_dverts(me->mr->dverts, cr_lvl->totvert);
		if(me->dvert) {
			me->mr->dverts= MEM_mallocN(sizeof(MDeformVert)*me->totvert, "MDeformVert");
			copy_dverts(me->mr->dverts, me->dvert, me->totvert);
		}
	}

	/* Prepare deltas */
	cr_deltas= MEM_callocN(sizeof(vec3f)*cr_lvl->totvert,"initial deltas");

	/* Calculate initial deltas -- current mesh subtracted from current level*/
	for(i=0; i<cr_lvl->totvert; ++i)
		VecSubf(&cr_deltas[i].x,me->mvert[i].co,cr_lvl->verts[i].co);

	/* Update current level -- copy current mesh into current level */
	for(i=0; i<cr_lvl->totvert; ++i)
		VecCopyf(cr_lvl->verts[i].co,me->mvert[i].co);
	for(i=0; i<cr_lvl->totface; ++i)
		cr_lvl->faces[i].flag= me->mface[i].flag;

	/* Update higher levels */
	pr_lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	cr_lvl= pr_lvl->next;
	while(cr_lvl) {
		/* Set up new deltas, but keep the ones from the previous level */
		if(pr_deltas) MEM_freeN(pr_deltas);
		pr_deltas= cr_deltas;
		cr_deltas= MEM_callocN(sizeof(vec3f)*cr_lvl->totvert,"deltas");

		/* Calculate and add new deltas
		   ============================*/

		for(i=0; i<pr_lvl->totface; ++i) {
			const MultiresFace *f= &pr_lvl->faces[i];
			data.corner1= &pr_deltas[f->v[0]].x;
			data.corner2= &pr_deltas[f->v[1]].x;
			data.corner3= &pr_deltas[f->v[2]].x;
			data.corner4= &pr_deltas[f->v[3]].x;
			data.quad= f->v[3] ? 1 : 0;
			multi_apply(&cr_deltas[f->mid].x, &data, 3, catmullclark_smooth_face);

			VecAddf(cr_lvl->verts[f->mid].co,
				cr_lvl->verts[f->mid].co,
				&cr_deltas[f->mid].x);
		}

		for(i=0; i<pr_lvl->totedge; ++i) {
			const MultiresEdge *e= &pr_lvl->edges[i];
			data.boundary= multires_edge_is_boundary(pr_lvl,i);
			edge_face_neighbor_midpoints_accum(&data,pr_lvl,cr_deltas,sizeof(vec3f),e);
			data.endpoint1= &pr_deltas[e->v[0]].x;
			data.endpoint2= &pr_deltas[e->v[1]].x;
			multi_apply(&cr_deltas[e->mid].x, &data, 3, catmullclark_smooth_edge);
			MEM_freeN(data.edge_face_neighbor_midpoints_accum);
		}
		for(i=0; i<pr_lvl->totedge; ++i) {
			const unsigned ndx= pr_lvl->edges[i].mid;
			VecAddf(cr_lvl->verts[ndx].co,
				cr_lvl->verts[ndx].co,
				&cr_deltas[ndx].x);
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
			if(data.boundary)
				MEM_freeN(data.boundary_edges_average);
			else {
				MEM_freeN(data.vert_face_neighbor_midpoints_average);
				MEM_freeN(data.vert_edge_neighbor_midpoints_average);
			}
		}
		for(i=0; i<pr_lvl->totvert; ++i) {
			VecAddf(cr_lvl->verts[i].co,
				cr_lvl->verts[i].co,
				&cr_deltas[i].x);
		}

		/* Update faces */
		curf= 0;
		for(i=0; i<cr_lvl->prev->totface; ++i) {
			const int sides= cr_lvl->prev->faces[i].v[3] ? 4 : 3;
			for(j=0; j<sides; ++j) {
				if(pr_lvl->faces[i].flag & ME_SMOOTH)
					cr_lvl->faces[curf].flag |= ME_SMOOTH;
				else
					cr_lvl->faces[curf].flag &= ~ME_SMOOTH;
				++curf;
			}
		}

		pr_lvl= pr_lvl->next;
		cr_lvl= cr_lvl->next;
	}
	if(pr_deltas) MEM_freeN(pr_deltas);
	if(cr_deltas) MEM_freeN(cr_deltas);

	/* Update lower levels */
	cr_lvl= me->mr->levels.last;
	cr_lvl= cr_lvl->prev;
	while(cr_lvl) {
		for(i=0; i<cr_lvl->totvert; ++i)
			cr_lvl->verts[i]= cr_lvl->next->verts[i];

		/* Update faces */
		curf= 0;
		for(i=0; i<cr_lvl->totface; ++i) {
			const int sides= cr_lvl->faces[i].v[3] ? 4 : 3;
			char smooth= 1;
			
			for(j=0; j<sides; ++j) {
				if(!(cr_lvl->next->faces[curf].flag & ME_SMOOTH)) {
					smooth= 0;
					break;
				}
				++curf;
			}
			if(smooth)
				cr_lvl->faces[i].flag |= ME_SMOOTH;
			else
				cr_lvl->faces[i].flag &= ~ME_SMOOTH;
		}

		cr_lvl= cr_lvl->prev;
	}

	multires_update_colors(me);

}

void multires_calc_level_maps(MultiresLevel *lvl)
{
	unsigned i,j;
	MultiresMapNode *indexnode= NULL;
	
	lvl->vert_edge_map= MEM_callocN(sizeof(ListBase)*lvl->totvert,"vert_edge_map");
	for(i=0; i<lvl->totedge; ++i) {
		for(j=0; j<2; ++j) {
			indexnode= MEM_callocN(sizeof(MultiresMapNode),"vert_edge_map indexnode");
			indexnode->Index= i;
			BLI_addtail(&lvl->vert_edge_map[lvl->edges[i].v[j]],indexnode);
		}
	}


       	lvl->vert_face_map= MEM_callocN(sizeof(ListBase)*lvl->totvert,"vert_face_map");
	for(i=0; i<lvl->totface; ++i){
		for(j=0; j<(lvl->faces[i].v[3]?4:3); ++j){
			indexnode= MEM_callocN(sizeof(MultiresMapNode),"vert_face_map indexnode");
			indexnode->Index= i;
			BLI_addtail(&lvl->vert_face_map[lvl->faces[i].v[j]],indexnode);
		}
	}

}

unsigned powi(const unsigned b, const unsigned p)
{
	unsigned i,r= b;

	if(p==0) return 1;

	for(i=1; i<p; ++i)
		r*= b;

	return r;
}

void multires_edge_level_update(void *ob, void *me_v)
{
	Mesh *me= me_v;
	MultiresLevel *cr_lvl= BLI_findlink(&me->mr->levels,me->mr->current-1);
	MultiresLevel *edge_lvl= BLI_findlink(&me->mr->levels,me->mr->edgelvl-1);
	unsigned i;

	for(i=0; i<cr_lvl->totedge; ++i) {
		const int ndx= me->pv ? me->pv->edge_map[i] : i;
		if(ndx != -1) { /* -1= hidden edge */
			if(me->mr->edgelvl >= me->mr->current ||
			   i<edge_lvl->totedge*powi(2,me->mr->current-me->mr->edgelvl))
				me->medge[ndx].flag= ME_EDGEDRAW;
			else
				me->medge[ndx].flag= 0;
		}
	}

	allqueue(REDRAWVIEW3D, 0);
}
