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
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

/************************* state testing ************************/

int ED_uvedit_test(Object *obedit)
{
	EditMesh *em;
	int ret;

	if(!obedit || obedit->type != OB_MESH)
		return 0;

	em = BKE_mesh_get_editmesh(obedit->data);
	ret = EM_texFaceCheck(em);
	BKE_mesh_end_editmesh(obedit->data, em);
	
	return ret;
}

/************************* assign image ************************/

void ED_uvedit_assign_image(Scene *scene, Object *obedit, Image *ima, Image *previma)
{
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	int update= 0;
	
	/* skip assigning these procedural images... */
	if(ima && (ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE))
		return;

	/* verify we have a mesh we can work with */
	if(!obedit || (obedit->type != OB_MESH))
		return;

	em= BKE_mesh_get_editmesh(((Mesh*)obedit->data));
	if(!em || !em->faces.first) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return;
	}
	
	/* ensure we have a uv layer */
	if(!CustomData_has_layer(&em->fdata, CD_MTFACE)) {
		EM_add_data_layer(em, &em->fdata, CD_MTFACE);
		update= 1;
	}

	/* now assign to all visible faces */
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, previma, efa, tf)) {
			if(ima) {
				tf->tpage= ima;
				tf->mode |= TF_TEX;
				
				if(ima->id.us==0) id_us_plus(&ima->id);
				else id_lib_extern(&ima->id);
			}
			else {
				tf->tpage= NULL;
				tf->mode &= ~TF_TEX;
			}

			update = 1;
		}
	}

	/* and update depdency graph */
	if(update)
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);

	BKE_mesh_end_editmesh(obedit->data, em);
}

/* dotile -	1, set the tile flag (from the space image)
 * 			2, set the tile index for the faces. */
void ED_uvedit_set_tile(bContext *C, Scene *scene, Object *obedit, Image *ima, int curtile)
{
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	
	/* verify if we have something to do */
	if(!ima || !ED_uvedit_test(obedit))
		return;
	
	/* skip assigning these procedural images... */
	if(ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE)
		return;
	
	em= BKE_mesh_get_editmesh((Mesh*)obedit->data);

	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(efa->h==0 && efa->f & SELECT)
			tf->tile= curtile; /* set tile index */
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
	BKE_mesh_end_editmesh(obedit->data, em);
}

/*********************** space conversion *********************/

static void uvedit_pixel_to_float(SpaceImage *sima, float *dist, float pixeldist)
{
	int width, height;

	if(sima) {
		ED_space_image_size(sima, &width, &height);
	}
	else {
		width= 256;
		height= 256;
	}

	dist[0]= pixeldist/width;
	dist[1]= pixeldist/height;
}

/*************** visibility and selection utilities **************/

int uvedit_face_visible_nolocal(Scene *scene, EditFace *efa)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		return (efa->h==0);
	else
		return (efa->h==0 && (efa->f & SELECT));
}

int uvedit_face_visible(Scene *scene, Image *ima, EditFace *efa, MTFace *tf)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SHOW_SAME_IMAGE)
		return (tf->tpage==ima)? uvedit_face_visible_nolocal(scene, efa): 0;
	else
		return uvedit_face_visible_nolocal(scene, efa);
}

int uvedit_face_selected(Scene *scene, EditFace *efa, MTFace *tf)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		return (efa->f & SELECT);
	else
		return (!(~tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) &&(!efa->v4 || tf->flag & TF_SEL4));
}

void uvedit_face_select(Scene *scene, EditFace *efa, MTFace *tf)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		EM_select_face(efa, 1);
	else
		tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
}

void uvedit_face_deselect(Scene *scene, EditFace *efa, MTFace *tf)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		EM_select_face(efa, 0);
	else
		tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
}

int uvedit_edge_selected(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	ToolSettings *ts= scene->toolsettings;
	int nvert= (efa->v4)? 4: 3;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			return (efa->f & SELECT);
		else if(ts->selectmode == SCE_SELECT_EDGE)
			return (*(&efa->e1 + i))->f & SELECT;
		else
			return (((efa->v1 + i)->f & SELECT) && ((efa->v1 + (i+1)%nvert)->f & SELECT));
	}
	else
		return (tf->flag & TF_SEL_MASK(i)) && (tf->flag & TF_SEL_MASK((i+1)%nvert));
}

void uvedit_edge_select(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	ToolSettings *ts= scene->toolsettings;
	int nvert= (efa->v4)? 4: 3;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			EM_select_face(efa, 1);
		else if(ts->selectmode == SCE_SELECT_EDGE)
			EM_select_edge((*(&efa->e1 + i)), 1);
		else {
			(efa->v1 + i)->f |= SELECT;
			(efa->v1 + (i+1)%nvert)->f |= SELECT;
		}
	}
	else
		tf->flag |= TF_SEL_MASK(i)|TF_SEL_MASK((i+1)%nvert);
}

void uvedit_edge_deselect(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	ToolSettings *ts= scene->toolsettings;
	int nvert= (efa->v4)? 4: 3;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			EM_select_face(efa, 0);
		else if(ts->selectmode == SCE_SELECT_EDGE)
			EM_select_edge((*(&efa->e1 + i)), 0);
		else {
			(efa->v1 + i)->f &= ~SELECT;
			(efa->v1 + (i+1)%nvert)->f &= ~SELECT;
		}
	}
	else
		tf->flag &= ~(TF_SEL_MASK(i)|TF_SEL_MASK((i+1)%nvert));
}

int uvedit_uv_selected(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			return (efa->f & SELECT);
		else
			return (*(&efa->v1 + i))->f & SELECT;
	}
	else
		return tf->flag & TF_SEL_MASK(i);
}

void uvedit_uv_select(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			EM_select_face(efa, 1);
		else
			(*(&efa->v1 + i))->f |= SELECT;
	}
	else
		tf->flag |= TF_SEL_MASK(i);
}

void uvedit_uv_deselect(Scene *scene, EditFace *efa, MTFace *tf, int i)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			EM_select_face(efa, 0);
		else
			(*(&efa->v1 + i))->f &= ~SELECT;
	}
	else
		tf->flag &= ~TF_SEL_MASK(i);
}

/*********************** geometric utilities ***********************/

void uv_center(float uv[][2], float cent[2], int quad)
{
	if(quad) {
		cent[0] = (uv[0][0] + uv[1][0] + uv[2][0] + uv[3][0]) / 4.0;
		cent[1] = (uv[0][1] + uv[1][1] + uv[2][1] + uv[3][1]) / 4.0;		
	}
	else {
		cent[0] = (uv[0][0] + uv[1][0] + uv[2][0]) / 3.0;
		cent[1] = (uv[0][1] + uv[1][1] + uv[2][1]) / 3.0;		
	}
}

float uv_area(float uv[][2], int quad)
{
	if(quad)
		return area_tri_v2(uv[0], uv[1], uv[2]) + area_tri_v2(uv[0], uv[2], uv[3]); 
	else
		return area_tri_v2(uv[0], uv[1], uv[2]); 
}

void uv_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy)
{
	uv[0][0] = uv_orig[0][0]*aspx;
	uv[0][1] = uv_orig[0][1]*aspy;
	
	uv[1][0] = uv_orig[1][0]*aspx;
	uv[1][1] = uv_orig[1][1]*aspy;
	
	uv[2][0] = uv_orig[2][0]*aspx;
	uv[2][1] = uv_orig[2][1]*aspy;
	
	uv[3][0] = uv_orig[3][0]*aspx;
	uv[3][1] = uv_orig[3][1]*aspy;
}

int ED_uvedit_minmax(Scene *scene, Image *ima, Object *obedit, float *min, float *max)
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	int sel;

	INIT_MINMAX2(min, max);

	sel= 0;
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tf)) {
			if(uvedit_uv_selected(scene, efa, tf, 0))				{ DO_MINMAX2(tf->uv[0], min, max); sel = 1; }
			if(uvedit_uv_selected(scene, efa, tf, 1))				{ DO_MINMAX2(tf->uv[1], min, max); sel = 1; }
			if(uvedit_uv_selected(scene, efa, tf, 2))				{ DO_MINMAX2(tf->uv[2], min, max); sel = 1; }
			if(efa->v4 && (uvedit_uv_selected(scene, efa, tf, 3)))	{ DO_MINMAX2(tf->uv[3], min, max); sel = 1; }
		}
	}
	
	BKE_mesh_end_editmesh(obedit->data, em);
	return sel;
}

int uvedit_center(Scene *scene, Image *ima, Object *obedit, float *cent, int mode)
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	float min[2], max[2];
	int change= 0;
	
	if(mode==0) {
		if(ED_uvedit_minmax(scene, ima, obedit, min, max))
			change = 1;
	}
	else if(mode==1) {
		INIT_MINMAX2(min, max);
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))				{ DO_MINMAX2(tf->uv[0], min, max);	change= 1;}
				if(uvedit_uv_selected(scene, efa, tf, 1))				{ DO_MINMAX2(tf->uv[1], min, max);	change= 1;}
				if(uvedit_uv_selected(scene, efa, tf, 2))				{ DO_MINMAX2(tf->uv[2], min, max);	change= 1;}
				if(efa->v4 && (uvedit_uv_selected(scene, efa, tf, 3)))	{ DO_MINMAX2(tf->uv[3], min, max);	change= 1;}
			}
		}
	}
	
	if(change) {
		cent[0]= (min[0]+max[0])/2.0;
		cent[1]= (min[1]+max[1])/2.0;
		
		BKE_mesh_end_editmesh(obedit->data, em);
		return 1;
	}

	BKE_mesh_end_editmesh(obedit->data, em);
	return 0;
}

/************************** find nearest ****************************/

typedef struct NearestHit {
	EditFace *efa;
	MTFace *tf;

	int vert, uv;
	int edge, vert2;
} NearestHit;

static void find_nearest_uv_edge(Scene *scene, Image *ima, EditMesh *em, float co[2], NearestHit *hit)
{
	MTFace *tf;
	EditFace *efa;
	EditVert *eve;
	float mindist, dist;
	int i, nverts;

	mindist= 1e10f;
	memset(hit, 0, sizeof(*hit));

	for(i=0, eve=em->verts.first; eve; eve=eve->next, i++)
		eve->tmp.l = i;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			nverts= efa->v4? 4: 3;

			for(i=0; i<nverts; i++) {
				dist= dist_to_line_segment_v2(co, tf->uv[i], tf->uv[(i+1)%nverts]);

				if(dist < mindist) {
					hit->tf= tf;
					hit->efa= efa;
					hit->edge= i;
					mindist= dist;

					hit->vert= (*(&efa->v1 + i))->tmp.l;
					hit->vert2= (*(&efa->v1 + ((i+1)%nverts)))->tmp.l;
				}
			}
		}
	}
}

static void find_nearest_uv_face(Scene *scene, Image *ima, EditMesh *em, float co[2], NearestHit *hit)
{
	MTFace *tf;
	EditFace *efa;
	float mindist, dist, cent[2];
	int i, nverts;

	mindist= 1e10f;
	memset(hit, 0, sizeof(*hit));
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			nverts= efa->v4? 4: 3;
			cent[0]= cent[1]= 0.0f;

			for(i=0; i<nverts; i++) {
				cent[0] += tf->uv[i][0];
				cent[1] += tf->uv[i][1];
			}

			cent[0] /= nverts;
			cent[1] /= nverts;
			dist= fabs(co[0]- cent[0]) + fabs(co[1]- cent[1]);

			if(dist < mindist) {
				hit->tf= tf;
				hit->efa= efa;
				mindist= dist;
			}
		}
	}
}

static int nearest_uv_between(MTFace *tf, int nverts, int id, float co[2], float uv[2])
{
	float m[3], v1[3], v2[3], c1, c2;
	int id1, id2;

	id1= (id+nverts-1)%nverts;
	id2= (id+nverts+1)%nverts;

	m[0]= co[0]-uv[0];
	m[1]= co[1]-uv[1];
	sub_v2_v2v2(v1, tf->uv[id1], tf->uv[id]);
	sub_v2_v2v2(v2, tf->uv[id2], tf->uv[id]);

	/* m and v2 on same side of v-v1? */
	c1= v1[0]*m[1] - v1[1]*m[0];
	c2= v1[0]*v2[1] - v1[1]*v2[0];

	if(c1*c2 < 0.0f)
		return 0;

	/* m and v1 on same side of v-v2? */
	c1= v2[0]*m[1] - v2[1]*m[0];
	c2= v2[0]*v1[1] - v2[1]*v1[0];

	return (c1*c2 >= 0.0f);
}

static void find_nearest_uv_vert(Scene *scene, Image *ima, EditMesh *em, float co[2], float penalty[2], NearestHit *hit)
{
	EditFace *efa;
	EditVert *eve;
	MTFace *tf;
	float mindist, dist;
	int i, nverts;

	mindist= 1e10f;
	memset(hit, 0, sizeof(*hit));
	
	for(i=0, eve=em->verts.first; eve; eve=eve->next, i++)
		eve->tmp.l = i;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			nverts= efa->v4? 4: 3;

			for(i=0; i<nverts; i++) {
				if(penalty && uvedit_uv_selected(scene, efa, tf, i))
					dist= fabs(co[0]-tf->uv[i][0])+penalty[0] + fabs(co[1]-tf->uv[i][1])+penalty[1];
				else
					dist= fabs(co[0]-tf->uv[i][0]) + fabs(co[1]-tf->uv[i][1]);

				if(dist<=mindist) {
					if(dist==mindist)
						if(!nearest_uv_between(tf, nverts, i, co, tf->uv[i]))
							continue;

					mindist= dist;

					hit->uv= i;
					hit->tf= tf;
					hit->efa= efa;

					hit->vert= (*(&efa->v1 + i))->tmp.l;
				}
			}
		}
	}
}

int ED_uvedit_nearest_uv(Scene *scene, Object *obedit, Image *ima, float co[2], float uv[2])
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	float mindist, dist;
	int i, nverts, found= 0;

	mindist= 1e10f;
	uv[0]= co[0];
	uv[1]= co[1];
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			nverts= efa->v4? 4: 3;

			for(i=0; i<nverts; i++) {
				dist= fabs(co[0]-tf->uv[i][0]) + fabs(co[1]-tf->uv[i][1]);

				if(dist<=mindist) {
					mindist= dist;

					uv[0]= tf->uv[i][0];
					uv[1]= tf->uv[i][1];
					found= 1;
				}
			}
		}
	}

	BKE_mesh_end_editmesh(obedit->data, em);
	return found;
}

/*********************** loop select ***********************/

static void uv_vertex_loop_flag(UvMapVert *first)
{
	UvMapVert *iterv;
	int count= 0;

	for(iterv=first; iterv; iterv=iterv->next) {
		if(iterv->separate && iterv!=first)
			break;

		count++;
	}
	
	if(count < 5)
		first->flag= 1;
}

static UvMapVert *uv_vertex_map_get(UvVertMap *vmap, EditFace *efa, int a)
{
	UvMapVert *iterv, *first;
	
	first= EM_get_uv_map_vert(vmap, (*(&efa->v1 + a))->tmp.l);

	for(iterv=first; iterv; iterv=iterv->next) {
		if(iterv->separate)
			first= iterv;
		if(iterv->f == efa->tmp.l)
			return first;
	}
	
	return NULL;
}

static int uv_edge_tag_faces(UvMapVert *first1, UvMapVert *first2, int *totface)
{
	UvMapVert *iterv1, *iterv2;
	EditFace *efa;
	int tot = 0;

	/* count number of faces this edge has */
	for(iterv1=first1; iterv1; iterv1=iterv1->next) {
		if(iterv1->separate && iterv1 != first1)
			break;

		for(iterv2=first2; iterv2; iterv2=iterv2->next) {
			if(iterv2->separate && iterv2 != first2)
				break;

			if(iterv1->f == iterv2->f) {
				/* if face already tagged, don't do this edge */
				efa= EM_get_face_for_index(iterv1->f);
				if(efa->f1)
					return 0;

				tot++;
				break;
			}
		}
	}

	if(*totface == 0) /* start edge */
		*totface= tot;
	else if(tot != *totface) /* check for same number of faces as start edge */
		return 0;

	/* tag the faces */
	for(iterv1=first1; iterv1; iterv1=iterv1->next) {
		if(iterv1->separate && iterv1 != first1)
			break;

		for(iterv2=first2; iterv2; iterv2=iterv2->next) {
			if(iterv2->separate && iterv2 != first2)
				break;

			if(iterv1->f == iterv2->f) {
				efa= EM_get_face_for_index(iterv1->f);
				efa->f1= 1;
				break;
			}
		}
	}

	return 1;
}

static int select_edgeloop(Scene *scene, Image *ima, EditMesh *em, NearestHit *hit, float limit[2], int extend)
{
	EditVert *eve;
	EditFace *efa;
	MTFace *tf;
	UvVertMap *vmap;
	UvMapVert *iterv1, *iterv2;
	int a, count, looking, nverts, starttotf, select;

	/* setup */
	EM_init_index_arrays(em, 0, 0, 1);
	vmap= EM_make_uv_vert_map(em, 0, 0, limit);

	for(count=0, eve=em->verts.first; eve; count++, eve= eve->next)
		eve->tmp.l = count;

	for(count=0, efa= em->faces.first; efa; count++, efa= efa->next) {
		if(!extend) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			uvedit_face_deselect(scene, efa, tf);
		}

		efa->tmp.l= count;
		efa->f1= 0;
	}
	
	/* set flags for first face and verts */
	nverts= (hit->efa->v4)? 4: 3;
	iterv1= uv_vertex_map_get(vmap, hit->efa, hit->edge);
	iterv2= uv_vertex_map_get(vmap, hit->efa, (hit->edge+1)%nverts);
	uv_vertex_loop_flag(iterv1);
	uv_vertex_loop_flag(iterv2);

	starttotf= 0;
	uv_edge_tag_faces(iterv1, iterv2, &starttotf);

	/* sorry, first edge isnt even ok */
	if(iterv1->flag==0 && iterv2->flag==0) looking= 0;
	else looking= 1;

	/* iterate */
	while(looking) {
		looking= 0;

		/* find correct valence edges which are not tagged yet, but connect to tagged one */
		for(efa= em->faces.first; efa; efa=efa->next) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(!efa->f1 && uvedit_face_visible(scene, ima, efa, tf)) {
				nverts= (efa->v4)? 4: 3;
				for(a=0; a<nverts; a++) {
					/* check face not hidden and not tagged */
					iterv1= uv_vertex_map_get(vmap, efa, a);
					iterv2= uv_vertex_map_get(vmap, efa, (a+1)%nverts);

					/* check if vertex is tagged and has right valence */
					if(iterv1->flag || iterv2->flag) {
						if(uv_edge_tag_faces(iterv1, iterv2, &starttotf)) {
							looking= 1;
							efa->f1= 1;

							uv_vertex_loop_flag(iterv1);
							uv_vertex_loop_flag(iterv2);
							break;
						}
					}
				}
			}
		}
	}

	/* do the actual select/deselect */
	nverts= (hit->efa->v4)? 4: 3;
	iterv1= uv_vertex_map_get(vmap, hit->efa, hit->edge);
	iterv2= uv_vertex_map_get(vmap, hit->efa, (hit->edge+1)%nverts);
	iterv1->flag= 1;
	iterv2->flag= 1;

	if(extend) {
		tf= CustomData_em_get(&em->fdata, hit->efa->data, CD_MTFACE);

		if(uvedit_uv_selected(scene, hit->efa, tf, hit->edge) && uvedit_uv_selected(scene, hit->efa, tf, hit->edge))
			select= 0;
		else
			select= 1;
	}
	else
		select= 1;
	
	for(efa= em->faces.first; efa; efa=efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		nverts= (efa->v4)? 4: 3;
		for(a=0; a<nverts; a++) {
			iterv1= uv_vertex_map_get(vmap, efa, a);

			if(iterv1->flag) {
				if(select) uvedit_uv_select(scene, efa, tf, a);
				else uvedit_uv_deselect(scene, efa, tf, a);
			}
		}
	}

	/* cleanup */
	EM_free_uv_vert_map(vmap);
	EM_free_index_arrays();

	return (select)? 1: -1;
}

/*********************** linked select ***********************/

static void select_linked(Scene *scene, Image *ima, EditMesh *em, float limit[2], NearestHit *hit, int extend)
{
	EditFace *efa;
	MTFace *tf;
	UvVertMap *vmap;
	UvMapVert *vlist, *iterv, *startv;
	int a, i, nverts, stacksize= 0, *stack;
	char *flag;

	EM_init_index_arrays(em, 0, 0, 1); /* we can use this too */
	vmap= EM_make_uv_vert_map(em, 1, 0, limit);
	if(vmap == NULL)
		return;

	stack= MEM_mallocN(sizeof(*stack) * em->totface, "UvLinkStack");
	flag= MEM_callocN(sizeof(*flag) * em->totface, "UvLinkFlag");

	if(!hit) {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) {
					stack[stacksize]= a;
					stacksize++;
					flag[a]= 1;
				}
			}
		}
	}
	else {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			if(efa == hit->efa) {
				stack[stacksize]= a;
				stacksize++;
				flag[a]= 1;
				break;
			}
		}
	}

	while(stacksize > 0) {
		stacksize--;
		a= stack[stacksize];

		efa = EM_get_face_for_index(a);

		nverts= efa->v4? 4: 3;

		for(i=0; i<nverts; i++) {
			/* make_uv_vert_map_EM sets verts tmp.l to the indicies */
			vlist= EM_get_uv_map_vert(vmap, (*(&efa->v1 + i))->tmp.l);
			
			startv= vlist;

			for(iterv=vlist; iterv; iterv=iterv->next) {
				if(iterv->separate)
					startv= iterv;
				if(iterv->f == a)
					break;
			}

			for(iterv=startv; iterv; iterv=iterv->next) {
				if((startv != iterv) && (iterv->separate))
					break;
				else if(!flag[iterv->f]) {
					flag[iterv->f]= 1;
					stack[stacksize]= iterv->f;
					stacksize++;
				}
			}
		}
	}

	if(!extend) {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(flag[a])
				tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			else
				tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
		}
	}
	else {
		for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
			if(flag[a]) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(efa->v4) {
					if((tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)))
						break;
				}
				else if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					break;
			}
		}

		if(efa) {
			for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
				if(flag[a]) {
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
		else {
			for(a=0, efa= em->faces.first; efa; efa= efa->next, a++) {
				if(flag[a]) {
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					tf->flag |= (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	}
	
	MEM_freeN(stack);
	MEM_freeN(flag);
	EM_free_uv_vert_map(vmap);
	EM_free_index_arrays();
}

/* ******************** align operator **************** */

static void weld_align_uv(bContext *C, int tool)
{
	Scene *scene;
	Object *obedit;
	Image *ima;
	EditMesh *em;
	EditFace *efa;
	MTFace *tf;
	float cent[2], min[2], max[2];
	
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ima= CTX_data_edit_image(C);

	INIT_MINMAX2(min, max);

	if(tool == 'a') {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))
					DO_MINMAX2(tf->uv[0], min, max)
				if(uvedit_uv_selected(scene, efa, tf, 1))
					DO_MINMAX2(tf->uv[1], min, max)
				if(uvedit_uv_selected(scene, efa, tf, 2))
					DO_MINMAX2(tf->uv[2], min, max)
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))
					DO_MINMAX2(tf->uv[3], min, max)
			}
		}

		tool= (max[0]-min[0] >= max[1]-min[1])? 'y': 'x';
	}

	uvedit_center(scene, ima, obedit, cent, 0);

	if(tool == 'x' || tool == 'w') {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))
					tf->uv[0][0]= cent[0];
				if(uvedit_uv_selected(scene, efa, tf, 1))
					tf->uv[1][0]= cent[0];
				if(uvedit_uv_selected(scene, efa, tf, 2))
					tf->uv[2][0]= cent[0];
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))
					tf->uv[3][0]= cent[0];
			}
		}
	}

	if(tool == 'y' || tool == 'w') {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0))
					tf->uv[0][1]= cent[1];
				if(uvedit_uv_selected(scene, efa, tf, 1))
					tf->uv[1][1]= cent[1];
				if(uvedit_uv_selected(scene, efa, tf, 2))
					tf->uv[2][1]= cent[1];
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))
					tf->uv[3][1]= cent[1];
			}
		}
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
}

static int align_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, RNA_enum_get(op->ptr, "axis"));

	return OPERATOR_FINISHED;
}

void UV_OT_align(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{'a', "ALIGN_AUTO", 0, "Align Auto", "Automatically choose the axis on which there is most alignment already."},
		{'x', "ALIGN_X", 0, "Align X", "Align UVs on X axis."},
		{'y', "ALIGN_Y", 0, "Align Y", "Align UVs on Y axis."},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Align";
	ot->description= "Align selected UV vertices to an axis";
	ot->idname= "UV_OT_align";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= align_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_items, 'a', "Axis", "Axis to align UV locations on.");
}

/* ******************** weld operator **************** */

static int weld_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, 'w');

	return OPERATOR_FINISHED;
}

void UV_OT_weld(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Weld";
	ot->description= "Weld selected UV vertices together";
	ot->idname= "UV_OT_weld";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= weld_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** stitch operator **************** */

/* just for averaging UVs */
typedef struct UVVertAverage {
	float uv[2];
	int count;
} UVVertAverage;

static int stitch_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	Scene *scene;
	Object *obedit;
	EditMesh *em;
	EditFace *efa;
	EditVert *eve;
	Image *ima;
	MTFace *tf;
	
	sima= CTX_wm_space_image(C);
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ima= CTX_data_edit_image(C);
	
	if(RNA_boolean_get(op->ptr, "use_limit")) {
		UvVertMap *vmap;
		UvMapVert *vlist, *iterv;
		float newuv[2], limit[2];
		int a, vtot;

		limit[0]= RNA_float_get(op->ptr, "limit");
		limit[1]= limit[0];

		EM_init_index_arrays(em, 0, 0, 1);
		vmap= EM_make_uv_vert_map(em, 1, 0, limit);

		if(vmap == NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return OPERATOR_CANCELLED;
		}

		for(a=0, eve= em->verts.first; eve; a++, eve= eve->next) {
			vlist= EM_get_uv_map_vert(vmap, a);

			while(vlist) {
				newuv[0]= 0; newuv[1]= 0;
				vtot= 0;

				for(iterv=vlist; iterv; iterv=iterv->next) {
					if((iterv != vlist) && iterv->separate)
						break;

					efa = EM_get_face_for_index(iterv->f);
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					
					if(uvedit_uv_selected(scene, efa, tf, iterv->tfindex)) {
						newuv[0] += tf->uv[iterv->tfindex][0];
						newuv[1] += tf->uv[iterv->tfindex][1];
						vtot++;
					}
				}

				if(vtot > 1) {
					newuv[0] /= vtot; newuv[1] /= vtot;

					for(iterv=vlist; iterv; iterv=iterv->next) {
						if((iterv != vlist) && iterv->separate)
							break;

						efa = EM_get_face_for_index(iterv->f);
						tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

						if(uvedit_uv_selected(scene, efa, tf, iterv->tfindex)) {
							tf->uv[iterv->tfindex][0]= newuv[0];
							tf->uv[iterv->tfindex][1]= newuv[1];
						}
					}
				}

				vlist= iterv;
			}
		}

		EM_free_uv_vert_map(vmap);
		EM_free_index_arrays();
	}
	else {
		UVVertAverage *uv_average, *uvav;
		int count;

		// index and count verts
		for(count=0, eve=em->verts.first; eve; count++, eve= eve->next)
			eve->tmp.l = count;
		
		uv_average= MEM_callocN(sizeof(UVVertAverage)*count, "Stitch");
		
		// gather uv averages per vert
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0)) {
					uvav = uv_average + efa->v1->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[0][0];
					uvav->uv[1] += tf->uv[0][1];
				}

				if(uvedit_uv_selected(scene, efa, tf, 1)) {
					uvav = uv_average + efa->v2->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[1][0];
					uvav->uv[1] += tf->uv[1][1];
				}

				if(uvedit_uv_selected(scene, efa, tf, 2)) {
					uvav = uv_average + efa->v3->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[2][0];
					uvav->uv[1] += tf->uv[2][1];
				}

				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) {
					uvav = uv_average + efa->v4->tmp.l;
					uvav->count++;
					uvav->uv[0] += tf->uv[3][0];
					uvav->uv[1] += tf->uv[3][1];
				}
			}
		}
		
		// apply uv welding
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				if(uvedit_uv_selected(scene, efa, tf, 0)) {
					uvav = uv_average + efa->v1->tmp.l;
					tf->uv[0][0] = uvav->uv[0]/uvav->count;
					tf->uv[0][1] = uvav->uv[1]/uvav->count;
				}

				if(uvedit_uv_selected(scene, efa, tf, 1)) {
					uvav = uv_average + efa->v2->tmp.l;
					tf->uv[1][0] = uvav->uv[0]/uvav->count;
					tf->uv[1][1] = uvav->uv[1]/uvav->count;
				}

				if(uvedit_uv_selected(scene, efa, tf, 2)) {
					uvav = uv_average + efa->v3->tmp.l;
					tf->uv[2][0] = uvav->uv[0]/uvav->count;
					tf->uv[2][1] = uvav->uv[1]/uvav->count;
				}

				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) {
					uvav = uv_average + efa->v4->tmp.l;
					tf->uv[3][0] = uvav->uv[0]/uvav->count;
					tf->uv[3][1] = uvav->uv[1]/uvav->count;
				}
			}
		}

		MEM_freeN(uv_average);
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_stitch(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Stitch";
	ot->description= "Stitch selected UV vertices by proximity";
	ot->idname= "UV_OT_stitch";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= stitch_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "use_limit", 1, "Use Limit", "Stitch UVs within a specified limit distance.");
	RNA_def_float(ot->srna, "limit", 0.01f, 0.0f, FLT_MAX, "Limit", "Limit distance in normalized coordinates.", -FLT_MAX, FLT_MAX);
}

/* ******************** (de)select all operator **************** */

static int select_inverse_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	ToolSettings *ts;
	Object *obedit;
	EditMesh *em;
	EditFace *efa;
	Image *ima;
	MTFace *tf;
	
	scene= CTX_data_scene(C);
	ts= CTX_data_tool_settings(C);
	obedit= CTX_data_edit_object(C);
	em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ima= CTX_data_edit_image(C);

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EM_select_swap(em);
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				tf->flag ^= TF_SEL1;
				tf->flag ^= TF_SEL2;
				tf->flag ^= TF_SEL3;
				if(efa->v4) tf->flag ^= TF_SEL4;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Inverse";
	ot->description= "Select inverse of (un)selected UV vertices";
	ot->idname= "UV_OT_select_inverse";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_inverse_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** (de)select all operator **************** */

static int select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	ToolSettings *ts;
	Object *obedit;
	EditMesh *em;
	EditFace *efa;
	Image *ima;
	MTFace *tf;
	int action = RNA_enum_get(op->ptr, "action");
	
	scene= CTX_data_scene(C);
	ts= CTX_data_tool_settings(C);
	obedit= CTX_data_edit_object(C);
	em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ima= CTX_data_edit_image(C);
	
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		switch (action) {
		case SEL_TOGGLE:
			EM_toggle_select_all(em);
			break;
		case SEL_SELECT:
			EM_select_all(em);
			break;
		case SEL_DESELECT:
			EM_deselect_all(em);
			break;
		case SEL_INVERT:
			EM_select_swap(em);
			break;
		}
	}
	else {

		if (action == SEL_TOGGLE) {
			action = SEL_SELECT;
			for(efa= em->faces.first; efa; efa= efa->next) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				if(uvedit_face_visible(scene, ima, efa, tf)) {
					if(tf->flag & (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4)) {
						action = SEL_DESELECT;
						break;
					}
				}
			}
		}
	
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

			if(uvedit_face_visible(scene, ima, efa, tf)) {
				char select_flag;

				if(efa->v4)
					select_flag = (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				else
					select_flag = (TF_SEL1+TF_SEL2+TF_SEL3);

				switch (action) {
				case SEL_SELECT:
					tf->flag |= select_flag;
					break;
				case SEL_DESELECT:
					tf->flag &= ~select_flag;
					break;
				case SEL_INVERT:
					if ((tf->flag & select_flag) == select_flag) {
						tf->flag &= ~select_flag;
					} else {
						tf->flag &= ~select_flag;
					}
					break;
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->description= "Change selection of all UV vertices";
	ot->idname= "UV_OT_select_all";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_all_exec;
	ot->poll= ED_operator_uvedit;

	WM_operator_properties_select_all(ot);
}

/* ******************** mouse select operator **************** */

static int sticky_select(float *limit, int hitv[4], int v, float *hituv[4], float *uv, int sticky)
{
	int i;

	/* this function test if some vertex needs to selected
	 * in addition to the existing ones due to sticky select */
	if(sticky == SI_STICKY_DISABLE)
		return 0;

	for(i=0; i<4; i++) {
		if(hitv[i] == v) {
			if(sticky == SI_STICKY_LOC) {
				if(fabs(hituv[i][0]-uv[0]) < limit[0] && fabs(hituv[i][1]-uv[1]) < limit[1])
					return 1;
			}
			else if(sticky == SI_STICKY_VERTEX)
				return 1;
		}
	}

	return 0;
}

static int mouse_select(bContext *C, float co[2], int extend, int loop)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	NearestHit hit;
	int a, i, select = 1, selectmode, sticky, sync, hitv[4], nvert;
	int flush = 0; /* 0 == dont flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
	float limit[2], *hituv[4], penalty[2];
	
	uvedit_pixel_to_float(sima, limit, 0.05f);
	uvedit_pixel_to_float(sima, penalty, 5.0f);

	/* retrieve operation mode */
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		sync= 1;

		if(ts->selectmode & SCE_SELECT_FACE)
			selectmode= UV_SELECT_FACE;
		else if(ts->selectmode & SCE_SELECT_EDGE)
			selectmode= UV_SELECT_EDGE;
		else
			selectmode= UV_SELECT_VERTEX;

		sticky= SI_STICKY_DISABLE;
	}
	else {
		sync= 0;
		selectmode= ts->uv_selectmode;
		sticky= (sima)? sima->sticky: 1;
	}

	/* find nearest element */
	if(loop) {
		/* find edge */
		find_nearest_uv_edge(scene, ima, em, co, &hit);
		if(hit.efa == NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return OPERATOR_CANCELLED;
		}
	}
	else if(selectmode == UV_SELECT_VERTEX) {
		/* find vertex */
		find_nearest_uv_vert(scene, ima, em, co, penalty, &hit);
		if(hit.efa == NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return OPERATOR_CANCELLED;
		}

		/* mark 1 vertex as being hit */
		for(i=0; i<4; i++)
			hitv[i]= 0xFFFFFFFF;

		hitv[hit.uv]= hit.vert;
		hituv[hit.uv]= hit.tf->uv[hit.uv];
	}
	else if(selectmode == UV_SELECT_EDGE) {
		/* find edge */
		find_nearest_uv_edge(scene, ima, em, co, &hit);
		if(hit.efa == NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return OPERATOR_CANCELLED;
		}

		/* mark 2 edge vertices as being hit */
		for(i=0; i<4; i++)
			hitv[i]= 0xFFFFFFFF;

		nvert= (hit.efa->v4)? 4: 3;

		hitv[hit.edge]= hit.vert;
		hitv[(hit.edge+1)%nvert]= hit.vert2;
		hituv[hit.edge]= hit.tf->uv[hit.edge];
		hituv[(hit.edge+1)%nvert]= hit.tf->uv[(hit.edge+1)%nvert];
	}
	else if(selectmode == UV_SELECT_FACE) {
		/* find face */
		find_nearest_uv_face(scene, ima, em, co, &hit);
		if(hit.efa == NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return OPERATOR_CANCELLED;
		}
		
		/* make active */
		EM_set_actFace(em, hit.efa);

		/* mark all face vertices as being hit */
		for(i=0; i<4; i++)
			hituv[i]= hit.tf->uv[i];

		hitv[0]= hit.efa->v1->tmp.l;
		hitv[1]= hit.efa->v2->tmp.l;
		hitv[2]= hit.efa->v3->tmp.l;
		
		if(hit.efa->v4)	hitv[3]= hit.efa->v4->tmp.l;
		else hitv[3]= 0xFFFFFFFF;
	}
	else if(selectmode == UV_SELECT_ISLAND) {
		find_nearest_uv_vert(scene, ima, em, co, NULL, &hit);

		if(hit.efa==NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return OPERATOR_CANCELLED;
		}
	}
	else {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	/* do selection */
	if(loop) {
		flush= select_edgeloop(scene, ima, em, &hit, limit, extend);
	}
	else if(selectmode == UV_SELECT_ISLAND) {
		select_linked(scene, ima, em, limit, &hit, extend);
	}
	else if(extend) {
		if(selectmode == UV_SELECT_VERTEX) {
			/* (de)select uv vertex */
			if(uvedit_uv_selected(scene, hit.efa, hit.tf, hit.uv)) {
				uvedit_uv_deselect(scene, hit.efa, hit.tf, hit.uv);
				select= 0;
			}
			else {
				uvedit_uv_select(scene, hit.efa, hit.tf, hit.uv);
				select= 1;
			}
			flush = 1;
		}
		else if(selectmode == UV_SELECT_EDGE) {
			/* (de)select edge */
			if(uvedit_edge_selected(scene, hit.efa, hit.tf, hit.edge)) {
				uvedit_edge_deselect(scene, hit.efa, hit.tf, hit.edge);
				select= 0;
			}
			else {
				uvedit_edge_select(scene, hit.efa, hit.tf, hit.edge);
				select= 1;
			}
			flush = 1;
		}
		else if(selectmode == UV_SELECT_FACE) {
			/* (de)select face */
			if(uvedit_face_selected(scene, hit.efa, hit.tf)) {
				uvedit_face_deselect(scene, hit.efa, hit.tf);
				select= 0;
			}
			else {
				uvedit_face_select(scene, hit.efa, hit.tf);
				select= 1;
			}
			flush = -1;
		}

		/* (de)select sticky uv nodes */
		if(sticky != SI_STICKY_DISABLE) {
			EditVert *ev;
			
			for(a=0, ev=em->verts.first; ev; ev = ev->next, a++)
				ev->tmp.l = a;
			
			/* deselect */
			if(select==0) {
				for(efa= em->faces.first; efa; efa= efa->next) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

					if(uvedit_face_visible(scene, ima, efa, tf)) {
						if(sticky_select(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
							uvedit_uv_deselect(scene, efa, tf, 0);
						if(sticky_select(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
							uvedit_uv_deselect(scene, efa, tf, 1);
						if(sticky_select(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
							uvedit_uv_deselect(scene, efa, tf, 2);
						if(efa->v4)
							if(sticky_select(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
								uvedit_uv_deselect(scene, efa, tf, 3);
					}
				}
				flush = -1;
			}
			/* select */
			else {
				for(efa= em->faces.first; efa; efa= efa->next) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

					if(uvedit_face_visible(scene, ima, efa, tf)) {
						if(sticky_select(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
							uvedit_uv_select(scene, efa, tf, 0);
						if(sticky_select(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
							uvedit_uv_select(scene, efa, tf, 1);
						if(sticky_select(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
							uvedit_uv_select(scene, efa, tf, 2);
						if(efa->v4)
							if(sticky_select(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
								uvedit_uv_select(scene, efa, tf, 3);
					}
				}
				
				flush = 1;
			}			
		}
	}
	else {
		/* deselect all */
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			uvedit_face_deselect(scene, efa, tf);
		}

		if(selectmode == UV_SELECT_VERTEX) {
			/* select vertex */
			uvedit_uv_select(scene, hit.efa, hit.tf, hit.uv);
			flush= 1;
		}
		else if(selectmode == UV_SELECT_EDGE) {
			/* select edge */
			uvedit_edge_select(scene, hit.efa, hit.tf, hit.edge);
			flush= 1;
		}
		else if(selectmode == UV_SELECT_FACE) {
			/* select face */
			uvedit_face_select(scene, hit.efa, hit.tf);
		}

		/* select sticky uvs */
		if(sticky != SI_STICKY_DISABLE) {
			for(efa= em->faces.first; efa; efa= efa->next) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(uvedit_face_visible(scene, ima, efa, tf)) {
					if(sticky == SI_STICKY_DISABLE) continue;

					if(sticky_select(limit, hitv, efa->v1->tmp.l, hituv, tf->uv[0], sticky))
						uvedit_uv_select(scene, efa, tf, 0);
					if(sticky_select(limit, hitv, efa->v2->tmp.l, hituv, tf->uv[1], sticky))
						uvedit_uv_select(scene, efa, tf, 1);
					if(sticky_select(limit, hitv, efa->v3->tmp.l, hituv, tf->uv[2], sticky))
						uvedit_uv_select(scene, efa, tf, 2);
					if(efa->v4)
						if(sticky_select(limit, hitv, efa->v4->tmp.l, hituv, tf->uv[3], sticky))
							uvedit_uv_select(scene, efa, tf, 3);

					flush= 1;
				}
			}
		}
	}
	
	if(sync) {
		/* flush for mesh selection */
		if(ts->selectmode != SCE_SELECT_FACE) {
			if(flush==1)		EM_select_flush(em);
			else if(flush==-1)	EM_deselect_flush(em);
		}
	}
	
	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	
	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_PASS_THROUGH|OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend= RNA_boolean_get(op->ptr, "extend");
	loop= 0;

	return mouse_select(C, co, extend, loop);
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float co[2];
	int x, y;

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;

	UI_view2d_region_to_view(&ar->v2d, x, y, &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void UV_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select";
	ot->description= "Select UV vertice";
	ot->idname= "UV_OT_select";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_exec;
	ot->invoke= select_invoke;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds.", -100.0f, 100.0f);
}

/* ******************** loop select operator **************** */

static int select_loop_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend= RNA_boolean_get(op->ptr, "extend");
	loop= 1;

	return mouse_select(C, co, extend, loop);
}

static int select_loop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float co[2];
	int x, y;

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;

	UI_view2d_region_to_view(&ar->v2d, x, y, &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_loop_exec(C, op);
}

void UV_OT_select_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Loop Select";
	ot->description= "Select a loop of connected UV vertices";
	ot->idname= "UV_OT_select_loop";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_loop_exec;
	ot->invoke= select_loop_invoke;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds.", -100.0f, 100.0f);
}

/* ******************** linked select operator **************** */

static int select_linked_internal(bContext *C, wmOperator *op, wmEvent *event, int pick)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	float limit[2];
	int extend;

	NearestHit hit, *hit_p= NULL;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Can't select linked when sync selection is enabled.");
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	extend= RNA_boolean_get(op->ptr, "extend");
	uvedit_pixel_to_float(sima, limit, 0.05f);

	if(pick) {
		float co[2];

		if(event) {
			/* invoke */
			ARegion *ar= CTX_wm_region(C);
			int x, y;

			x= event->x - ar->winrct.xmin;
			y= event->y - ar->winrct.ymin;

			UI_view2d_region_to_view(&ar->v2d, x, y, &co[0], &co[1]);
			RNA_float_set_array(op->ptr, "location", co);
		}
		else {
			/* exec */
			RNA_float_get_array(op->ptr, "location", co);
		}

		find_nearest_uv_vert(scene, ima, em, co, NULL, &hit);
		hit_p= &hit;
	}

	select_linked(scene, ima, em, limit, hit_p, extend);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

static int select_linked_exec(bContext *C, wmOperator *op)
{
	return select_linked_internal(C, op, NULL, 0);
}

void UV_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->description= "Select all UV vertices linked to the active UV map";
	ot->idname= "UV_OT_select_linked";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_linked_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
}

static int select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return select_linked_internal(C, op, event, 1);
}

static int select_linked_pick_exec(bContext *C, wmOperator *op)
{
	return select_linked_internal(C, op, NULL, 1);
}

void UV_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked Pick";
	ot->description= "Select all UV vertices linked under the mouse";
	ot->idname= "UV_OT_select_linked_pick";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke= select_linked_pick_invoke;
	ot->exec= select_linked_pick_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");

	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds.", -100.0f, 100.0f);
}

/* ******************** unlink selection operator **************** */

static int unlink_selection_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Can't unlink selection when sync selection is enabled.");
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			if(efa->v4) {
				if(~tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4))
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
			}
			else {
				if(~tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3);
			}
		}
	}
	
	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_unlink_selection(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unlink Selection";
	ot->description= "Unlink selected UV vertices from active UV map";
	ot->idname= "UV_OT_unlink_selection";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= unlink_selection_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** border select operator **************** */

/* This function sets the selection on tagged faces, need because settings the
 * selection a face is done in a number of places but it also needs to respect
 * the sticky modes for the UV verts, so dealing with the sticky modes is best
 * done in a separate function.
 * 
 * De-selects faces that have been tagged on efa->tmp.l.  */

static void uv_faces_do_sticky(bContext *C, SpaceImage *sima, Scene *scene, Object *obedit, short select)
{
	/* Selecting UV Faces with some modes requires us to change 
	 * the selection in other faces (depending on the sticky mode).
	 * 
	 * This only needs to be done when the Mesh is not used for
	 * selection (so for sticky modes, vertex or location based). */
	
	ToolSettings *ts= CTX_data_tool_settings(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	int nverts, i;
	
	if((ts->uv_flag & UV_SYNC_SELECTION)==0 && sima->sticky == SI_STICKY_VERTEX) {
		/* Tag all verts as untouched, then touch the ones that have a face center
		 * in the loop and select all MTFace UV's that use a touched vert. */
		EditVert *eve;
		
		for(eve= em->verts.first; eve; eve= eve->next)
			eve->tmp.l = 0;
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->tmp.l) {
				if(efa->v4)
					efa->v1->tmp.l=	efa->v2->tmp.l= efa->v3->tmp.l= efa->v4->tmp.l=1;
				else
					efa->v1->tmp.l= efa->v2->tmp.l= efa->v3->tmp.l= 1;
			}
		}

		/* now select tagged verts */
		for(efa= em->faces.first; efa; efa= efa->next) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);		
			nverts= efa->v4? 4: 3;
			for(i=0; i<nverts; i++) {
				if((*(&efa->v1 + i))->tmp.l) {
					if(select)
						uvedit_uv_select(scene, efa, tf, i);
					else
						uvedit_uv_deselect(scene, efa, tf, i);
				}
			}
		}
	}
	else if((ts->uv_flag & UV_SYNC_SELECTION)==0 && sima->sticky == SI_STICKY_LOC) {
		EditFace *efa_vlist;
		MTFace *tf_vlist;
		UvMapVert *start_vlist=NULL, *vlist_iter;
		struct UvVertMap *vmap;
		float limit[2];
		int efa_index;
		//EditVert *eve; /* removed vert counting for now */ 
		//int a;
		
		uvedit_pixel_to_float(sima, limit, 0.05);
		
		EM_init_index_arrays(em, 0, 0, 1);
		vmap= EM_make_uv_vert_map(em, 0, 0, limit);
		
		/* verts are numbered above in make_uv_vert_map_EM, make sure this stays true! */
		/*for(a=0, eve= em->verts.first; eve; a++, eve= eve->next)
			eve->tmp.l = a; */
		
		if(vmap == NULL) {
			BKE_mesh_end_editmesh(obedit->data, em);
			return;
		}
		
		for(efa_index=0, efa= em->faces.first; efa; efa_index++, efa= efa->next) {
			if(efa->tmp.l) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				nverts= efa->v4? 4: 3;

				for(i=0; i<nverts; i++) {
					if(select)
						uvedit_uv_select(scene, efa, tf, i);
					else
						uvedit_uv_deselect(scene, efa, tf, i);
					
					vlist_iter= EM_get_uv_map_vert(vmap, (*(&efa->v1 + i))->tmp.l);
					
					while (vlist_iter) {
						if(vlist_iter->separate)
							start_vlist = vlist_iter;
						
						if(efa_index == vlist_iter->f)
							break;

						vlist_iter = vlist_iter->next;
					}
				
					vlist_iter = start_vlist;
					while (vlist_iter) {
						
						if(vlist_iter != start_vlist && vlist_iter->separate)
							break;
						
						if(efa_index != vlist_iter->f) {
							efa_vlist = EM_get_face_for_index(vlist_iter->f);
							tf_vlist = CustomData_em_get(&em->fdata, efa_vlist->data, CD_MTFACE);
							
							if(select)
								uvedit_uv_select(scene, efa_vlist, tf_vlist, vlist_iter->tfindex);
							else
								uvedit_uv_deselect(scene, efa_vlist, tf_vlist, vlist_iter->tfindex);
						}
						vlist_iter = vlist_iter->next;
					}
				}
			}
		}
		EM_free_index_arrays();
		EM_free_uv_vert_map(vmap);
		
	}
	else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->tmp.l) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(select)
					uvedit_face_select(scene, efa, tf);
				else
					uvedit_face_deselect(scene, efa, tf);
			}
		}
	}
	BKE_mesh_end_editmesh(obedit->data, em);
}

static int border_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tface;
	rcti rect;
	rctf rectf;
	int change, pinned, select, faces;

	/* get rectangle from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
		
	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	/* figure out what to select/deselect */
	select= (RNA_int_get(op->ptr, "gesture_mode") == GESTURE_MODAL_SELECT);
	pinned= RNA_boolean_get(op->ptr, "pinned");
	
	if(ts->uv_flag & UV_SYNC_SELECTION)
		faces= (ts->selectmode == SCE_SELECT_FACE);
	else
		faces= (ts->uv_selectmode == UV_SELECT_FACE);

	/* do actual selection */
	if(faces && !pinned) {
		/* handle face selection mode */
		float cent[2];

		change= 0;

		for(efa= em->faces.first; efa; efa= efa->next) {
			/* assume not touched */
			efa->tmp.l = 0;
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tface)) {
				uv_center(tface->uv, cent, efa->v4 != NULL);
				if(BLI_in_rctf(&rectf, cent[0], cent[1])) {
					efa->tmp.l = change = 1;
				}
			}
		}

		/* (de)selects all tagged faces and deals with sticky modes */
		if(change)
			uv_faces_do_sticky(C, sima, scene, obedit, select);
	}
	else {
		/* other selection modes */
		change= 1;

		for(efa= em->faces.first; efa; efa= efa->next) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if(uvedit_face_visible(scene, ima, efa, tface)) {
				if(!pinned || (ts->uv_flag & UV_SYNC_SELECTION) ) {
					/* UV_SYNC_SELECTION - can't do pinned selection */
					if(BLI_in_rctf(&rectf, tface->uv[0][0], tface->uv[0][1])) {
						if(select)	uvedit_uv_select(scene, efa, tface, 0);
						else		uvedit_uv_deselect(scene, efa, tface, 0);
					}
					if(BLI_in_rctf(&rectf, tface->uv[1][0], tface->uv[1][1])) {
						if(select)	uvedit_uv_select(scene, efa, tface, 1);
						else		uvedit_uv_deselect(scene, efa, tface, 1);
					}
					if(BLI_in_rctf(&rectf, tface->uv[2][0], tface->uv[2][1])) {
						if(select)	uvedit_uv_select(scene, efa, tface, 2);
						else		uvedit_uv_deselect(scene, efa, tface, 2);
					}
					if(efa->v4 && BLI_in_rctf(&rectf, tface->uv[3][0], tface->uv[3][1])) {
						if(select)	uvedit_uv_select(scene, efa, tface, 3);
						else		uvedit_uv_deselect(scene, efa, tface, 3);
					}
				}
				else if(pinned) {
					if((tface->unwrap & TF_PIN1) && 
						BLI_in_rctf(&rectf, tface->uv[0][0], tface->uv[0][1])) {
						
						if(select)	uvedit_uv_select(scene, efa, tface, 0);
						else		uvedit_uv_deselect(scene, efa, tface, 0);
					}
					if((tface->unwrap & TF_PIN2) && 
						BLI_in_rctf(&rectf, tface->uv[1][0], tface->uv[1][1])) {
						
						if(select)	uvedit_uv_select(scene, efa, tface, 1);
						else		uvedit_uv_deselect(scene, efa, tface, 1);
					}
					if((tface->unwrap & TF_PIN3) && 
						BLI_in_rctf(&rectf, tface->uv[2][0], tface->uv[2][1])) {
						
						if(select)	uvedit_uv_select(scene, efa, tface, 2);
						else		uvedit_uv_deselect(scene, efa, tface, 2);
					}
					if((efa->v4) && (tface->unwrap & TF_PIN4) && BLI_in_rctf(&rectf, tface->uv[3][0], tface->uv[3][1])) {
						if(select)	uvedit_uv_select(scene, efa, tface, 3);
						else		uvedit_uv_deselect(scene, efa, tface, 3);
					}
				}
			}
		}
	}

	if(change) {
		/* make sure newly selected vert selection is updated*/
		if(ts->uv_flag & UV_SYNC_SELECTION) {
			if(ts->selectmode != SCE_SELECT_FACE) {
				if(select)	EM_select_flush(em);
				else		EM_deselect_flush(em);
			}
		}

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
		
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_FINISHED;
	}
	
	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_CANCELLED;
} 

void UV_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->description= "Select UV vertices using border selection";
	ot->idname= "UV_OT_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= border_select_exec;
	ot->modal= WM_border_select_modal;
	ot->poll= ED_operator_uvedit;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "pinned", 0, "Pinned", "Border select pinned UVs only.");

	WM_operator_properties_gesture_border(ot, FALSE);
}

/* ******************** circle select operator **************** */

static void select_uv_inside_ellipse(SpaceImage *sima, Scene *scene, int select, EditFace *efa, MTFace *tface, int index, float *offset, float *ell, int select_index)
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y, r2, *uv;
	
	uv= tface->uv[index];

	x= (uv[0] - offset[0])*ell[0];
	y= (uv[1] - offset[1])*ell[1];

	r2 = x*x + y*y;
	if(r2 < 1.0) {
		if(select)	uvedit_uv_select(scene, efa, tface, select_index);
		else uvedit_uv_deselect(scene, efa, tface, select_index);
	}
}

int circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	ARegion *ar= CTX_wm_region(C);
	EditFace *efa;
	MTFace *tface;
	int x, y, radius, width, height, select;
	float zoomx, zoomy, offset[2], ellipse[2];
    int gesture_mode= RNA_int_get(op->ptr, "gesture_mode");
    
	/* get operator properties */
	select= (gesture_mode == GESTURE_MODAL_SELECT);
	x= RNA_int_get(op->ptr, "x");
	y= RNA_int_get(op->ptr, "y");
	radius= RNA_int_get(op->ptr, "radius");

	/* compute ellipse size and location, not a circle since we deal
	 * with non square image. ellipse is normalized, r = 1.0. */
	ED_space_image_size(sima, &width, &height);
	ED_space_image_zoom(sima, ar, &zoomx, &zoomy);

	ellipse[0]= width*zoomx/radius;
	ellipse[1]= height*zoomy/radius;

	UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);
	
	/* do selection */
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		select_uv_inside_ellipse(sima, scene, select, efa, tface, 0, offset, ellipse, 0);
		select_uv_inside_ellipse(sima, scene, select, efa, tface, 1, offset, ellipse, 1);
		select_uv_inside_ellipse(sima, scene, select, efa, tface, 2, offset, ellipse, 2);
		if(efa->v4)
			select_uv_inside_ellipse(sima, scene, select, efa, tface, 3, offset, ellipse, 3);
	}

	if(select) EM_select_flush(em);
	else EM_deselect_flush(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_circle_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Circle Select";
	ot->description= "Select UV vertices using circle selection";
	ot->idname= "UV_OT_circle_select";
	
	/* api callbacks */
	ot->invoke= WM_gesture_circle_invoke;
	ot->modal= WM_gesture_circle_modal;
	ot->exec= circle_select_exec;
	ot->poll= ED_operator_uvedit;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "gesture_mode", 0, INT_MIN, INT_MAX, "Gesture Mode", "", INT_MIN, INT_MAX);
}

/* ******************** snap cursor operator **************** */

static void snap_uv_to_pixel(float *uvco, float w, float h)
{
	uvco[0] = ((float)((int)((uvco[0]*w) + 0.5f)))/w;
	uvco[1] = ((float)((int)((uvco[1]*h) + 0.5f)))/h;
}

static void snap_cursor_to_pixels(SpaceImage *sima, View2D *v2d)
{
	int width= 0, height= 0;

	ED_space_image_size(sima, &width, &height);
	snap_uv_to_pixel(v2d->cursor, width, height);
}

static int snap_cursor_to_selection(Scene *scene, Image *ima, Object *obedit, View2D *v2d)
{
	return uvedit_center(scene, ima, obedit, v2d->cursor, 0);
}

static int snap_cursor_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	int change= 0;

	switch(RNA_boolean_get(op->ptr, "target")) {
		case 0:
			snap_cursor_to_pixels(sima, &ar->v2d);
			change= 1;
			break;
		case 1:
			change= snap_cursor_to_selection(scene, ima, obedit, &ar->v2d);
			break;
	}

	if(!change)
		return OPERATOR_CANCELLED;
	
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void UV_OT_snap_cursor(wmOperatorType *ot)
{
	static EnumPropertyItem target_items[] = {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "SELECTION", 0, "Selection", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Snap Cursor";
	ot->description= "Snap cursor to target type";
	ot->idname= "UV_OT_snap_cursor";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= snap_cursor_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UV's to.");
}

/* ******************** snap selection operator **************** */

static int snap_uvs_to_cursor(Scene *scene, Image *ima, Object *obedit, View2D *v2d)
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tface;
	short change= 0;

	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(uvedit_uv_selected(scene, efa, tface, 0))		VECCOPY2D(tface->uv[0], v2d->cursor);
			if(uvedit_uv_selected(scene, efa, tface, 1))		VECCOPY2D(tface->uv[1], v2d->cursor);
			if(uvedit_uv_selected(scene, efa, tface, 2))		VECCOPY2D(tface->uv[2], v2d->cursor);
			if(efa->v4)
				if(uvedit_uv_selected(scene, efa, tface, 3))	VECCOPY2D(tface->uv[3], v2d->cursor);

			change= 1;
		}
	}

	BKE_mesh_end_editmesh(obedit->data, em);
	return change;
}

static int snap_uvs_to_adjacent_unselected(Scene *scene, Image *ima, Object *obedit)
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	EditVert *eve;
	MTFace *tface;
	short change = 0;
	int count = 0;
	float *coords;
	short *usercount, users;
	
	/* set all verts to -1 : an unused index*/
	for(eve= em->verts.first; eve; eve= eve->next)
		eve->tmp.l=-1;
	
	/* index every vert that has a selected UV using it, but only once so as to
	 * get unique indicies and to count how much to malloc */
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(uvedit_uv_selected(scene, efa, tface, 0) && efa->v1->tmp.l==-1)		efa->v1->tmp.l= count++;
			if(uvedit_uv_selected(scene, efa, tface, 1) && efa->v2->tmp.l==-1)		efa->v2->tmp.l= count++;
			if(uvedit_uv_selected(scene, efa, tface, 2) && efa->v3->tmp.l==-1)		efa->v3->tmp.l= count++;
			if(efa->v4)
				if(uvedit_uv_selected(scene, efa, tface, 3) && efa->v4->tmp.l==-1)	efa->v4->tmp.l= count++;

			change = 1;
			
			/* optional speedup */
			efa->tmp.p = tface;
		}
		else
			efa->tmp.p = NULL;
	}
	
	coords = MEM_callocN(sizeof(float)*count*2, "snap to adjacent coords");
	usercount = MEM_callocN(sizeof(short)*count, "snap to adjacent counts");
	
	/* add all UV coords from visible, unselected UV coords as well as counting them to average later */
	for(efa= em->faces.first; efa; efa= efa->next) {
		if((tface=(MTFace *)efa->tmp.p)) {
			/* is this an unselected UV we can snap to? */
			if(efa->v1->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 0))) {
				coords[efa->v1->tmp.l*2] +=		tface->uv[0][0];
				coords[(efa->v1->tmp.l*2)+1] +=	tface->uv[0][1];
				usercount[efa->v1->tmp.l]++;
				change = 1;
			}
			if(efa->v2->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 1))) {
				coords[efa->v2->tmp.l*2] +=		tface->uv[1][0];
				coords[(efa->v2->tmp.l*2)+1] +=	tface->uv[1][1];
				usercount[efa->v2->tmp.l]++;
				change = 1;
			}
			if(efa->v3->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 2))) {
				coords[efa->v3->tmp.l*2] +=		tface->uv[2][0];
				coords[(efa->v3->tmp.l*2)+1] +=	tface->uv[2][1];
				usercount[efa->v3->tmp.l]++;
				change = 1;
			}
			
			if(efa->v4) {
				if(efa->v4->tmp.l >= 0 && (!uvedit_uv_selected(scene, efa, tface, 3))) {
					coords[efa->v4->tmp.l*2] +=		tface->uv[3][0];
					coords[(efa->v4->tmp.l*2)+1] +=	tface->uv[3][1];
					usercount[efa->v4->tmp.l]++;
					change = 1;
				}
			}
		}
	}
	
	/* no other verts selected, bail out */
	if(!change) {
		MEM_freeN(coords);
		MEM_freeN(usercount);
		BKE_mesh_end_editmesh(obedit->data, em);
		return change;
	}
	
	/* copy the averaged unselected UVs back to the selected UVs */
	for(efa= em->faces.first; efa; efa= efa->next) {
		if((tface=(MTFace *)efa->tmp.p)) {
			
			if(	uvedit_uv_selected(scene, efa, tface, 0) &&
					efa->v1->tmp.l >= 0 &&
					(users = usercount[efa->v1->tmp.l])
			) {
				tface->uv[0][0] = coords[efa->v1->tmp.l*2]		/ users;
				tface->uv[0][1] = coords[(efa->v1->tmp.l*2)+1]	/ users;
			}

			if(	uvedit_uv_selected(scene, efa, tface, 1) &&
					efa->v2->tmp.l >= 0 &&
					(users = usercount[efa->v2->tmp.l])
			) {
				tface->uv[1][0] = coords[efa->v2->tmp.l*2]		/ users;
				tface->uv[1][1] = coords[(efa->v2->tmp.l*2)+1]	/ users;
			}
			
			if(	uvedit_uv_selected(scene, efa, tface, 2) &&
					efa->v3->tmp.l >= 0 &&
					(users = usercount[efa->v3->tmp.l])
			) {
				tface->uv[2][0] = coords[efa->v3->tmp.l*2]		/ users;
				tface->uv[2][1] = coords[(efa->v3->tmp.l*2)+1]	/ users;
			}
			
			if(efa->v4) {
				if(	uvedit_uv_selected(scene, efa, tface, 3) &&
						efa->v4->tmp.l >= 0 &&
						(users = usercount[efa->v4->tmp.l])
				) {
					tface->uv[3][0] = coords[efa->v4->tmp.l*2]		/ users;
					tface->uv[3][1] = coords[(efa->v4->tmp.l*2)+1]	/ users;
				}
			}
		}
	}
	
	MEM_freeN(coords);
	MEM_freeN(usercount);

	BKE_mesh_end_editmesh(obedit->data, em);
	return change;
}

static int snap_uvs_to_pixels(SpaceImage *sima, Scene *scene, Object *obedit)
{
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	Image *ima;
	EditFace *efa;
	MTFace *tface;
	int width= 0, height= 0;
	float w, h;
	short change = 0;

	if(!sima)
		return 0;
	
	ima= sima->image;
	
	ED_space_image_size(sima, &width, &height);
	w = (float)width;
	h = (float)height;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(uvedit_uv_selected(scene, efa, tface, 0)) snap_uv_to_pixel(tface->uv[0], w, h);
			if(uvedit_uv_selected(scene, efa, tface, 1)) snap_uv_to_pixel(tface->uv[1], w, h);
			if(uvedit_uv_selected(scene, efa, tface, 2)) snap_uv_to_pixel(tface->uv[2], w, h);
			if(efa->v4)
				if(uvedit_uv_selected(scene, efa, tface, 3)) snap_uv_to_pixel(tface->uv[3], w, h);

			change = 1;
		}
	}

	BKE_mesh_end_editmesh(obedit->data, em);
	return change;
}

static int snap_selection_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	int change= 0;

	switch(RNA_boolean_get(op->ptr, "target")) {
		case 0:
			change= snap_uvs_to_pixels(sima, scene, obedit);
			break;
		case 1:
			change= snap_uvs_to_cursor(scene, ima, obedit, &ar->v2d);
			break;
		case 2:
			change= snap_uvs_to_adjacent_unselected(scene, ima, obedit);
			break;
	}

	if(!change)
		return OPERATOR_CANCELLED;
	
	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_snap_selection(wmOperatorType *ot)
{
	static EnumPropertyItem target_items[] = {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "CURSOR", 0, "Cursor", ""},
		{2, "ADJACENT_UNSELECTED", 0, "Adjacent Unselected", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Snap Selection";
	ot->description= "Snap selected UV vertices to target type";
	ot->idname= "UV_OT_snap_selection";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= snap_selection_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UV's to.");
}

/* ******************** pin operator **************** */

static int pin_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tface;
	int clear= RNA_boolean_get(op->ptr, "clear");
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(!clear) {
				if(uvedit_uv_selected(scene, efa, tface, 0)) tface->unwrap |= TF_PIN1;
				if(uvedit_uv_selected(scene, efa, tface, 1)) tface->unwrap |= TF_PIN2;
				if(uvedit_uv_selected(scene, efa, tface, 2)) tface->unwrap |= TF_PIN3;
				if(efa->v4)
					if(uvedit_uv_selected(scene, efa, tface, 3)) tface->unwrap |= TF_PIN4;
			}
			else {
				if(uvedit_uv_selected(scene, efa, tface, 0)) tface->unwrap &= ~TF_PIN1;
				if(uvedit_uv_selected(scene, efa, tface, 1)) tface->unwrap &= ~TF_PIN2;
				if(uvedit_uv_selected(scene, efa, tface, 2)) tface->unwrap &= ~TF_PIN3;
				if(efa->v4)
					if(uvedit_uv_selected(scene, efa, tface, 3)) tface->unwrap &= ~TF_PIN4;
			}
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_pin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pin";
	ot->description= "Set/clear selected UV vertices as anchored between multiple unwrap operations";
	ot->idname= "UV_OT_pin";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= pin_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "Clear pinning for the selection instead of setting it.");
}

/******************* select pinned operator ***************/

static int select_pinned_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tface;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		tface = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tface)) {
			if(tface->unwrap & TF_PIN1) uvedit_uv_select(scene, efa, tface, 0);
			if(tface->unwrap & TF_PIN2) uvedit_uv_select(scene, efa, tface, 1);
			if(tface->unwrap & TF_PIN3) uvedit_uv_select(scene, efa, tface, 2);
			if(efa->v4) {
				if(tface->unwrap & TF_PIN4) uvedit_uv_select(scene, efa, tface, 3);
			}
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_select_pinned(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Selected Pinned";
	ot->description= "Select all pinned UV vertices";
	ot->idname= "UV_OT_select_pinned";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_pinned_exec;
	ot->poll= ED_operator_uvedit;
}

/********************** hide operator *********************/

static int hide_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	int swap= RNA_boolean_get(op->ptr, "unselected");
	int facemode= sima ? sima->flag & SI_SELACTFACE : 0;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EM_hide_mesh(em, swap);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_FINISHED;
	}
	
	if(swap) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if(facemode) {
					/* Pretend face mode */
					if((	(efa->v4==NULL && 
							(	tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) ==			(TF_SEL1|TF_SEL2|TF_SEL3) )			 ||
							(	tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) ==	(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)	) == 0) {
						
						if(em->selectmode == SCE_SELECT_FACE) {
							efa->f &= ~SELECT;
							/* must re-select after */
							efa->e1->f &= ~SELECT;
							efa->e2->f &= ~SELECT;
							efa->e3->f &= ~SELECT;
							if(efa->e4) efa->e4->f &= ~SELECT;
						}
						else
							EM_select_face(efa, 0);
					}
					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
				else if(em->selectmode == SCE_SELECT_FACE) {
					if((tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))==0) {
						if(!efa->v4)
							EM_select_face(efa, 0);
						else if(!(tf->flag & TF_SEL4))
							EM_select_face(efa, 0);
						tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
					}
				}
				else {
					/* EM_deselect_flush will deselect the face */
					if((tf->flag & TF_SEL1)==0)				efa->v1->f &= ~SELECT;
					if((tf->flag & TF_SEL2)==0)				efa->v2->f &= ~SELECT;
					if((tf->flag & TF_SEL3)==0)				efa->v3->f &= ~SELECT;
					if((efa->v4) && (tf->flag & TF_SEL4)==0)	efa->v4->f &= ~SELECT;			

					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				if(facemode) {
					if(	(efa->v4==NULL && 
							(	tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) ==			(TF_SEL1|TF_SEL2|TF_SEL3) )			 ||
							(	tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) ==	(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)	) {
						
						if(em->selectmode == SCE_SELECT_FACE) {
							efa->f &= ~SELECT;
							/* must re-select after */
							efa->e1->f &= ~SELECT;
							efa->e2->f &= ~SELECT;
							efa->e3->f &= ~SELECT;
							if(efa->e4) efa->e4->f &= ~SELECT;
						}
						else
							EM_select_face(efa, 0);
					}

					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
				else if(em->selectmode == SCE_SELECT_FACE) {
					if(tf->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
						EM_select_face(efa, 0);
					else if(efa->v4 && tf->flag & TF_SEL4)
						EM_select_face(efa, 0);

					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
				else {
					/* EM_deselect_flush will deselect the face */
					if(tf->flag & TF_SEL1)				efa->v1->f &= ~SELECT;
					if(tf->flag & TF_SEL2)				efa->v2->f &= ~SELECT;
					if(tf->flag & TF_SEL3)				efa->v3->f &= ~SELECT;
					if((efa->v4) && tf->flag & TF_SEL4)	efa->v4->f &= ~SELECT;

					tf->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	}
	
	/*deselects too many but ok for now*/
	if(em->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_VERTEX))
		EM_deselect_flush(em);
	
	if(em->selectmode==SCE_SELECT_FACE) {
		/* de-selected all edges from faces that were de-selected.
		 * now make sure all faces that are selected also have selected edges */
		for(efa= em->faces.first; efa; efa= efa->next)
			if(efa->f & SELECT)
				EM_select_face(efa, 1);
	}
	
	EM_validate_selections(em);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Selected";
	ot->description= "Hide (un)selected UV vertices";
	ot->idname= "UV_OT_hide";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= hide_exec;
	ot->poll= ED_operator_uvedit;

	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected.");
}

/****************** reveal operator ******************/

static int reveal_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh*)obedit->data);
	EditFace *efa;
	MTFace *tf;
	int facemode= sima ? sima->flag & SI_SELACTFACE : 0;
	int stickymode= sima ? (sima->sticky != SI_STICKY_DISABLE) : 1;
	
	/* call the mesh function if we are in mesh sync sel */
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EM_reveal_mesh(em);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_FINISHED;
	}
	
	if(facemode) {
		if(em->selectmode == SCE_SELECT_FACE) {
			for(efa= em->faces.first; efa; efa= efa->next) {
				if(!(efa->h) && !(efa->f & SELECT)) {
					tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					EM_select_face(efa, 1);
					tf->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
				}
			}
		}
		else {
			/* enable adjacent faces to have disconnected UV selections if sticky is disabled */
			if(!stickymode) {
				for(efa= em->faces.first; efa; efa= efa->next) {
					if(!(efa->h) && !(efa->f & SELECT)) {
						/* All verts must be unselected for the face to be selected in the UV view */
						if((efa->v1->f&SELECT)==0 && (efa->v2->f&SELECT)==0 && (efa->v3->f&SELECT)==0 && (efa->v4==0 || (efa->v4->f&SELECT)==0)) {
							tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

							tf->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
							/* Cant use EM_select_face here because it unselects the verts
							 * and we cant tell if the face was totally unselected or not */
							/*EM_select_face(efa, 1);
							 * 
							 * See Loop with EM_select_face() below... */
							efa->f |= SELECT;
						}
					}
				}
			}
			else {
				for(efa= em->faces.first; efa; efa= efa->next) {
					if(!(efa->h) && !(efa->f & SELECT)) {
						tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

						if((efa->v1->f & SELECT)==0)				{tf->flag |= TF_SEL1;}
						if((efa->v2->f & SELECT)==0)				{tf->flag |= TF_SEL2;}
						if((efa->v3->f & SELECT)==0)				{tf->flag |= TF_SEL3;}
						if((efa->v4 && (efa->v4->f & SELECT)==0))	{tf->flag |= TF_SEL4;}

						efa->f |= SELECT;
					}
				}
			}
			
			/* Select all edges and verts now */
			for(efa= em->faces.first; efa; efa= efa->next)
				/* we only selected the face flags, and didnt changes edges or verts, fix this now */
				if(!(efa->h) && (efa->f & SELECT))
					EM_select_face(efa, 1);

			EM_select_flush(em);
		}
	}
	else if(em->selectmode == SCE_SELECT_FACE) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(!(efa->h) && !(efa->f & SELECT)) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				efa->f |= SELECT;
				tf->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
			}
		}
		
		/* Select all edges and verts now */
		for(efa= em->faces.first; efa; efa= efa->next)
			/* we only selected the face flags, and didnt changes edges or verts, fix this now */
			if(!(efa->h) && (efa->f & SELECT))
				EM_select_face(efa, 1);
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(!(efa->h) && !(efa->f & SELECT)) {
				tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

				if((efa->v1->f & SELECT)==0)				{tf->flag |= TF_SEL1;}
				if((efa->v2->f & SELECT)==0)				{tf->flag |= TF_SEL2;}
				if((efa->v3->f & SELECT)==0)				{tf->flag |= TF_SEL3;}
				if((efa->v4 && (efa->v4->f & SELECT)==0))	{tf->flag |= TF_SEL4;}

				efa->f |= SELECT;
			}
		}
		
		/* Select all edges and verts now */
		for(efa= em->faces.first; efa; efa= efa->next)
			/* we only selected the face flags, and didnt changes edges or verts, fix this now */
			if(!(efa->h) && (efa->f & SELECT))
				EM_select_face(efa, 1);
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void UV_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal Hidden";
	ot->description= "Reveal all hidden UV vertices";
	ot->idname= "UV_OT_reveal";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= reveal_exec;
	ot->poll= ED_operator_uvedit;
}

/******************** set 3d cursor operator ********************/

static int set_2d_cursor_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);
	ar->v2d.cursor[0]= location[0];
	ar->v2d.cursor[1]= location[1];
	
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

static int set_2d_cursor_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	int x, y;
	float location[2];

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;
	UI_view2d_region_to_view(&ar->v2d, x, y, &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return set_2d_cursor_exec(C, op);
}

void UV_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set 2D Cursor";
	ot->description= "Set 2D cursor location";
	ot->idname= "UV_OT_cursor_set";
	
	/* api callbacks */
	ot->exec= set_2d_cursor_exec;
	ot->invoke= set_2d_cursor_invoke;
	ot->poll= ED_operator_uvedit;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in 0.0-1.0 coordinates.", -10.0f, 10.0f);
}

/********************** set tile operator **********************/

static int set_tile_exec(bContext *C, wmOperator *op)
{
	Image *ima= CTX_data_edit_image(C);
	int tile[2];

	if(!ima || !(ima->tpageflag & IMA_TILES))
		return OPERATOR_CANCELLED;

	RNA_int_get_array(op->ptr, "tile", tile);
	ED_uvedit_set_tile(C, CTX_data_scene(C), CTX_data_edit_object(C), ima, tile[0] + ima->xrep*tile[1]);

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

static int set_tile_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	float fx, fy;
	int x, y, tile[2];

	if(!ima || !(ima->tpageflag & IMA_TILES))
		return OPERATOR_CANCELLED;

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;
	UI_view2d_region_to_view(&ar->v2d, x, y, &fx, &fy);

	if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
		fx= fx*ima->xrep;
		fy= fy*ima->yrep;
		
		tile[0]= fx;
		tile[1]= fy;
		
		sima->curtile= tile[1]*ima->xrep + tile[0];
		RNA_int_set_array(op->ptr, "tile", tile);
	}

	return set_tile_exec(C, op);
}

void UV_OT_tile_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Tile";
	ot->description= "Set UV image tile coordinates";
	ot->idname= "UV_OT_tile_set";
	
	/* api callbacks */
	ot->exec= set_tile_exec;
	ot->invoke= set_tile_invoke;
	ot->poll= ED_operator_uvedit;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "tile", 2, NULL, 0, INT_MAX, "Tile", "Tile coordinate.", 0, 10);
}

/* ************************** registration **********************************/

void ED_operatortypes_uvedit(void)
{
	WM_operatortype_append(UV_OT_select_all);
	WM_operatortype_append(UV_OT_select_inverse);
	WM_operatortype_append(UV_OT_select);
	WM_operatortype_append(UV_OT_select_loop);
	WM_operatortype_append(UV_OT_select_linked);
	WM_operatortype_append(UV_OT_select_linked_pick);
	WM_operatortype_append(UV_OT_unlink_selection);
	WM_operatortype_append(UV_OT_select_pinned);
	WM_operatortype_append(UV_OT_select_border);
	WM_operatortype_append(UV_OT_circle_select);

	WM_operatortype_append(UV_OT_snap_cursor);
	WM_operatortype_append(UV_OT_snap_selection);

	WM_operatortype_append(UV_OT_align);
	WM_operatortype_append(UV_OT_stitch);
	WM_operatortype_append(UV_OT_weld);
	WM_operatortype_append(UV_OT_pin);

	WM_operatortype_append(UV_OT_average_islands_scale);
	WM_operatortype_append(UV_OT_cube_project);
	WM_operatortype_append(UV_OT_cylinder_project);
	WM_operatortype_append(UV_OT_from_view);
	WM_operatortype_append(UV_OT_minimize_stretch);
	WM_operatortype_append(UV_OT_pack_islands);
	WM_operatortype_append(UV_OT_reset);
	WM_operatortype_append(UV_OT_sphere_project);
	WM_operatortype_append(UV_OT_unwrap);

	WM_operatortype_append(UV_OT_reveal);
	WM_operatortype_append(UV_OT_hide);

	WM_operatortype_append(UV_OT_cursor_set);
	WM_operatortype_append(UV_OT_tile_set);
}

void ED_keymap_uvedit(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	
	keymap= WM_keymap_find(keyconf, "UV Editor", 0, 0);
	keymap->poll= ED_operator_uvedit;
	
	/* pick selection */
	WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0)->ptr, "extend", 1);

	/* border/circle selection */
	WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "pinned", 1);
	WM_keymap_add_item(keymap, "UV_OT_circle_select", CKEY, KM_PRESS, 0, 0);

	/* selection manipulation */
	WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_linked_pick", LKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0)->ptr, "extend", TRUE);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_linked_pick", LKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", TRUE);

	WM_keymap_add_item(keymap, "UV_OT_unlink_selection", LKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_all", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_pinned", PKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "UV_OT_weld", WKEY, KM_PRESS, 0, 0);

	/* uv operations */
	WM_keymap_add_item(keymap, "UV_OT_stitch", VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, KM_ALT, 0)->ptr, "clear", 1);

	/* unwrap */
	WM_keymap_add_item(keymap, "UV_OT_unwrap", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_minimize_stretch", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_pack_islands", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_average_islands_scale", AKEY, KM_PRESS, KM_CTRL, 0);

	/* hide */
	WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);
	WM_keymap_add_item(keymap, "UV_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	/* cursor */
	WM_keymap_add_item(keymap, "UV_OT_cursor_set", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_tile_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);
	
	/* menus */
	WM_keymap_add_menu(keymap, "IMAGE_MT_uvs_snap", SKEY, KM_PRESS, KM_SHIFT, 0);

	ED_object_generic_keymap(keyconf, keymap, TRUE);

	transform_keymap_for_space(keyconf, keymap, SPACE_IMAGE);
}

