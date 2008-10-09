/**
 * $Id: $
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_anim.h"

#include "BSE_view.h"

#include "BIF_gl.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"

#include "blendef.h"
#include "mydevice.h"

typedef enum SK_PType
{
	PT_CONTINUOUS,
	PT_EXACT,
} SK_PType;

typedef struct SK_Point
{
	float p[3];
	SK_PType type;
} SK_Point;

typedef struct SK_Stroke
{
	struct SK_Stroke *next, *prev;

	SK_Point *points;
	int nb_points;
	int buf_size;
} SK_Stroke;

#define SK_Stroke_BUFFER_INIT_SIZE 20

typedef struct SK_DrawData
{
	short mval[2];
	short previous_mval[2];
	SK_PType type;
} SK_DrawData;

typedef struct SK_Sketch
{
	ListBase	strokes;
	SK_Stroke	*active_stroke;
} SK_Sketch;

SK_Sketch *GLOBAL_sketch = NULL;

/******************** PROTOTYPES ******************************/

void sk_freeStroke(SK_Stroke *stk);
void sk_freeSketch(SK_Sketch *sketch);

/******************** PEELING *********************************/

typedef struct SK_DepthPeel
{
	struct SK_DepthPeel *next, *prev;
	
	float depth;
	float p[3];
	float no[3];
} SK_DepthPeel;

int cmpPeel(void *arg1, void *arg2)
{
	SK_DepthPeel *p1 = arg1;
	SK_DepthPeel *p2 = arg2;
	int val = 0;
	
	if (p1->depth < p2->depth)
	{
		val = -1;
	}
	else if (p1->depth > p2->depth)
	{
		val = 1;
	}
	
	return val;
}

void addDepthPeel(ListBase *depth_peels, float depth, float p[3], float no[3])
{
	SK_DepthPeel *peel = MEM_callocN(sizeof(SK_DepthPeel), "DepthPeel");
	
	peel->depth = depth;
	VECCOPY(peel->p, p);
	VECCOPY(peel->no, no);
	
	BLI_addtail(depth_peels, peel);
}

int peelDerivedMesh(Object *ob, DerivedMesh *dm, float obmat[][4], float ray_start[3], float ray_normal[3], short mval[2], ListBase *depth_peels)
{
	int retval = 0;
	int totvert = dm->getNumVerts(dm);
	int totface = dm->getNumFaces(dm);
	
	if (totvert > 0) {
		float imat[4][4];
		float timat[3][3]; /* transpose inverse matrix for normals */
		float ray_start_local[3], ray_normal_local[3];
		int test = 1;

		Mat4Invert(imat, obmat);

		Mat3CpyMat4(timat, imat);
		Mat3Transp(timat);
		
		VECCOPY(ray_start_local, ray_start);
		VECCOPY(ray_normal_local, ray_normal);
		
		Mat4MulVecfl(imat, ray_start_local);
		Mat4Mul3Vecfl(imat, ray_normal_local);
		
		
		/* If number of vert is more than an arbitrary limit, 
		 * test against boundbox first
		 * */
		if (totface > 16) {
			struct BoundBox *bb = object_get_boundbox(ob);
			test = ray_hit_boundbox(bb, ray_start_local, ray_normal_local);
		}
		
		if (test == 1) {
			MVert *verts = dm->getVertArray(dm);
			MFace *faces = dm->getFaceArray(dm);
			int i;
			
			for( i = 0; i < totface; i++) {
				MFace *f = faces + i;
				float lambda;
				int result;
				
				
				result = RayIntersectsTriangle(ray_start_local, ray_normal_local, verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, &lambda, NULL);
				
				if (result) {
					float location[3], normal[3];
					float intersect[3];
					float new_depth;
					
					VECCOPY(intersect, ray_normal_local);
					VecMulf(intersect, lambda);
					VecAddf(intersect, intersect, ray_start_local);
					
					VECCOPY(location, intersect);
					
					if (f->v4)
						CalcNormFloat4(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, verts[f->v4].co, normal);
					else
						CalcNormFloat(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, normal);

					Mat4MulVecfl(obmat, location);
					
					new_depth = VecLenf(location, ray_start);					
					
					Mat3MulVecfl(timat, normal);
					Normalize(normal);

					addDepthPeel(depth_peels, new_depth, location, normal);
				}
		
				if (f->v4 && result == 0)
				{
					result = RayIntersectsTriangle(ray_start_local, ray_normal_local, verts[f->v3].co, verts[f->v4].co, verts[f->v1].co, &lambda, NULL);
					
					if (result) {
						float location[3], normal[3];
						float intersect[3];
						float new_depth;
						
						VECCOPY(intersect, ray_normal_local);
						VecMulf(intersect, lambda);
						VecAddf(intersect, intersect, ray_start_local);
						
						VECCOPY(location, intersect);
						
						if (f->v4)
							CalcNormFloat4(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, verts[f->v4].co, normal);
						else
							CalcNormFloat(verts[f->v1].co, verts[f->v2].co, verts[f->v3].co, normal);

						Mat4MulVecfl(obmat, location);
						
						new_depth = VecLenf(location, ray_start);					
						
						Mat3MulVecfl(timat, normal);
						Normalize(normal);
	
						addDepthPeel(depth_peels, new_depth, location, normal);
					} 
				}
			}
		}
	}

	return retval;
} 

int peelObjects(ListBase *depth_peels, short mval[2])
{
	Base *base;
	int retval = 0;
	float ray_start[3], ray_normal[3];
	
	viewray(mval, ray_start, ray_normal);

	base= FIRSTBASE;
	for ( base = FIRSTBASE; base != NULL; base = base->next ) {
		if ( BASE_SELECTABLE(base) ) {
			Object *ob = base->object;
			
			if (ob->transflag & OB_DUPLI)
			{
				DupliObject *dupli_ob;
				ListBase *lb = object_duplilist(G.scene, ob);
				
				for(dupli_ob = lb->first; dupli_ob; dupli_ob = dupli_ob->next)
				{
					Object *ob = dupli_ob->ob;
					
					if (ob->type == OB_MESH) {
						DerivedMesh *dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
						int val;
						
						val = peelDerivedMesh(ob, dm, dupli_ob->mat, ray_start, ray_normal, mval, depth_peels);
	
						retval = retval || val;
	
						dm->release(dm);
					}
				}
				
				free_object_duplilist(lb);
			}
			
			if (ob->type == OB_MESH) {
				DerivedMesh *dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
				int val;
				
				val = peelDerivedMesh(ob, dm, ob->obmat, ray_start, ray_normal, mval, depth_peels);
				
				retval = retval || val;
				
				dm->release(dm);
			}
		}
	}
	
	BLI_sortlist(depth_peels, cmpPeel);
	
	return retval;
}

/**************************************************************/

void sk_freeSketch(SK_Sketch *sketch)
{
	SK_Stroke *stk, *next;
	
	for (stk = sketch->strokes.first; stk; stk = next)
	{
		next = stk->next;
		
		sk_freeStroke(stk);
	}
	
	MEM_freeN(sketch);
}

SK_Sketch* sk_createSketch()
{
	SK_Sketch *sketch;
	
	sketch = MEM_callocN(sizeof(SK_Sketch), "SK_Sketch");
	
	sketch->active_stroke = NULL;

	sketch->strokes.first = NULL;
	sketch->strokes.last = NULL;
	
	return sketch;
}

void sk_allocStrokeBuffer(SK_Stroke *stk)
{
	stk->points = MEM_callocN(sizeof(SK_Point) * stk->buf_size, "SK_Point buffer");
}

void sk_freeStroke(SK_Stroke *stk)
{
	MEM_freeN(stk->points);
	MEM_freeN(stk);
}

SK_Stroke* sk_createStroke()
{
	SK_Stroke *stk;
	
	stk = MEM_callocN(sizeof(SK_Stroke), "SK_Stroke");
	
	stk->nb_points = 0;
	stk->buf_size = SK_Stroke_BUFFER_INIT_SIZE;
	
	sk_allocStrokeBuffer(stk);
	
	return stk;
}

void sk_shrinkStrokeBuffer(SK_Stroke *stk)
{
	if (stk->nb_points < stk->buf_size)
	{
		SK_Point *old_points = stk->points;
		
		stk->buf_size = stk->nb_points;

		sk_allocStrokeBuffer(stk);		
		
		memcpy(stk->points, old_points, sizeof(SK_Point) * stk->nb_points);
		
		MEM_freeN(old_points);
	}
}

void sk_growStrokeBuffer(SK_Stroke *stk)
{
	if (stk->nb_points == stk->buf_size)
	{
		SK_Point *old_points = stk->points;
		
		stk->buf_size *= 2;
		
		sk_allocStrokeBuffer(stk);
		
		memcpy(stk->points, old_points, sizeof(SK_Point) * stk->nb_points);
		
		MEM_freeN(old_points);
	}
}

void sk_appendStrokePoint(SK_Stroke *stk, SK_Point *pt)
{
	sk_growStrokeBuffer(stk);
	
	memcpy(stk->points + stk->nb_points, pt, sizeof(SK_Point));
	
	stk->nb_points++;
}

/* Apply reverse Chaikin filter to simplify the polyline
 * */
void sk_filterStroke(SK_Stroke *stk)
{
	SK_Point *old_points = stk->points;
	int nb_points = stk->nb_points;
	int i, j;
	
	sk_allocStrokeBuffer(stk);
	stk->nb_points = 0;
	
	for (i = 0, j = 0; i < nb_points; i++)
	{
		if (i - j == 3)
		{
			SK_Point pt;
			float vec[3];
			
			pt.type = PT_CONTINUOUS;
			pt.p[0] = 0;
			pt.p[1] = 0;
			pt.p[2] = 0;
			
			VECCOPY(vec, old_points[j].p);
			VecMulf(vec, -0.25);
			VecAddf(pt.p, pt.p, vec);
			
			VECCOPY(vec, old_points[j+1].p);
			VecMulf(vec,  0.75);
			VecAddf(pt.p, pt.p, vec);

			VECCOPY(vec, old_points[j+2].p);
			VecMulf(vec,  0.75);
			VecAddf(pt.p, pt.p, vec);

			VECCOPY(vec, old_points[j+3].p);
			VecMulf(vec, -0.25);
			VecAddf(pt.p, pt.p, vec);
			
			sk_appendStrokePoint(stk, &pt);

			j += 2;
		}
		
		if (old_points[i].type == PT_EXACT)
		{
			sk_appendStrokePoint(stk, old_points + i);
			j = i;
		}
	} 
	
	MEM_freeN(old_points);

	sk_shrinkStrokeBuffer(stk);
}

SK_Point *sk_lastStrokePoint(SK_Stroke *stk)
{
	SK_Point *pt = NULL;
	
	if (stk->nb_points > 0)
	{
		pt = stk->points + (stk->nb_points - 1);
	}
	
	return pt;
}

void sk_drawStroke(SK_Stroke *stk)
{
	int i;
	
	glColor3f(1, 0.5, 0);
	glBegin(GL_LINE_STRIP);
	
	for (i = 0; i < stk->nb_points; i++)
	{
		glVertex3fv(stk->points[i].p);
	}
	
	glEnd();
	
	glColor3f(0, 0, 0);
	glBegin(GL_POINTS);

	for (i = 0; i < stk->nb_points; i++)
	{
		if (stk->points[i].type == PT_EXACT)
		{
			glVertex3fv(stk->points[i].p);
		}
	}

	glEnd();

//	glColor3f(1, 1, 1);
//	glBegin(GL_POINTS);
//
//	for (i = 0; i < stk->nb_points; i++)
//	{
//		if (stk->points[i].type == PT_CONTINUOUS)
//		{
//			glVertex3fv(stk->points[i].p);
//		}
//	}
//
//	glEnd();
}

SK_Point *sk_snapPointStroke(SK_Stroke *stk, short mval[2], int *dist)
{
	SK_Point *pt = NULL;
	int i;
	
	for (i = 0; i < stk->nb_points; i++)
	{
		if (stk->points[i].type == PT_EXACT)
		{
			short pval[2];
			int pdist;
			
			project_short_noclip(stk->points[i].p, pval);
			
			pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
			
			if (pdist < *dist)
			{
				*dist = pdist;
				pt = stk->points + i;
			}
		}
	}
	
	return pt;
}


SK_Point *sk_snapPoint(SK_Sketch *sketch, short mval[2], int min_dist)
{
	SK_Point *pt = NULL;
	SK_Stroke *stk;
	int dist = min_dist;
	
	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		SK_Point *spt = sk_snapPointStroke(stk, mval, &dist);
		
		if (spt != NULL)
		{
			pt = spt;
		}
	}
	
	return pt;
}

void sk_startStroke(SK_Sketch *sketch)
{
	SK_Stroke *stk = sk_createStroke();
	
	BLI_addtail(&sketch->strokes, stk);
	sketch->active_stroke = stk;
}

void sk_endStroke(SK_Sketch *sketch)
{
	sk_shrinkStrokeBuffer(sketch->active_stroke);
	sketch->active_stroke = NULL;
}

void sk_projectPaintData(SK_Stroke *stk, SK_DrawData *dd, float vec[3])
{
	/* copied from grease pencil, need fixing */	
	SK_Point *last = sk_lastStrokePoint(stk);
	short cval[2];
	//float *fp = give_cursor();
	float fp[3] = {0, 0, 0};
	float dvec[3];
	
	if (last != NULL)
	{
		VECCOPY(fp, last->p);
	}
	
	initgrabz(fp[0], fp[1], fp[2]);
	
	/* method taken from editview.c - mouse_cursor() */
	project_short_noclip(fp, cval);
	window_to_3d(dvec, cval[0] - dd->mval[0], cval[1] - dd->mval[1]);
	VecSubf(vec, fp, dvec);
}

void sk_updateDrawData(SK_DrawData *dd)
{
	dd->type = PT_CONTINUOUS;
	
	dd->previous_mval[0] = dd->mval[0];
	dd->previous_mval[1] = dd->mval[1];
}

float sk_distanceDepth(float p1[3], float p2[3])
{
	float vec[3];
	float distance;
	
	VecSubf(vec, p1, p2);
	
	Projf(vec, vec, G.vd->viewinv[2]);
	
	distance = VecLength(vec);
	
	if (Inpf(G.vd->viewinv[2], vec) > 0)
	{
		distance *= -1;
	}
	
	return distance; 
}

void sk_addStrokeSnapPoint(SK_Stroke *stk, SK_DrawData *dd, SK_Point *snap_pt)
{
	SK_Point pt;
	float distance;
	float length;
	int i, j, total;
	
	pt.type = PT_EXACT;
	
	sk_projectPaintData(stk, dd, pt.p);

	sk_appendStrokePoint(stk, &pt);
	
	/* update all previous point to give smooth Z progresion */
	total = 0;
	length = 0;
	for (i = stk->nb_points - 2; i > 0; i--)
	{
		length += VecLenf(stk->points[i].p, stk->points[i + 1].p);
		total++;
		if (stk->points[i].type == PT_EXACT)
		{
			break;
		}
	}
	
	if (total > 1)
	{
		float progress = length - VecLenf(stk->points[stk->nb_points - 2].p, stk->points[stk->nb_points - 1].p);
		
		distance = sk_distanceDepth(snap_pt->p, stk->points[i].p);
		
		for (j = 1, i = stk->nb_points - 2; j < total; j++, i--)
		{
			float ray_start[3], ray_normal[3];
			float delta = VecLenf(stk->points[i].p, stk->points[i - 1].p);
			short pval[2];
			
			project_short_noclip(stk->points[i].p, pval);
			viewray(pval, ray_start, ray_normal);
			
			VecMulf(ray_normal, distance * progress / length);
			VecAddf(stk->points[i].p, stk->points[i].p, ray_normal);

			progress -= delta ;
		}
	}

	VECCOPY(stk->points[stk->nb_points - 1].p, snap_pt->p);
}

void sk_addStrokeDrawPoint(SK_Stroke *stk, SK_DrawData *dd)
{
	SK_Point pt;
	
	pt.type = dd->type;

	sk_projectPaintData(stk, dd, pt.p);

	sk_appendStrokePoint(stk, &pt);
}

void sk_addStrokeEmbedPoint(SK_Stroke *stk, SK_DrawData *dd)
{
	SK_DepthPeel *p1, *p2;
	ListBase depth_peels;
	
	depth_peels.first = depth_peels.last = NULL;
	
	peelObjects(&depth_peels, dd->mval);
	
	p1 = depth_peels.first;
	
	if (p1)
	{
		SK_Point pt;
		
		pt.type = dd->type;

		p2 = p1->next;
		
		if (p2)
		{
			VecAddf(pt.p, p1->p, p2->p);
			VecMulf(pt.p, 0.5f);
		}
		else
		{
			VECCOPY(pt.p, p1->p);
		}

		sk_appendStrokePoint(stk, &pt);
	}
	else
	{
		sk_addStrokeDrawPoint(stk, dd);
	}
	
	BLI_freelistN(&depth_peels);
}

void sk_endContinuousStroke(SK_Stroke *stk)
{
	stk->points[stk->nb_points - 1].type = PT_EXACT;
}

int sk_stroke_filtermval(SK_DrawData *dd)
{
	int retval = 0;
	if (dd->mval[0] != dd->previous_mval[0] || dd->mval[1] != dd->previous_mval[1])
	{
		retval = 1;
	}
	
	return retval;
}

void sk_initDrawData(SK_DrawData *dd)
{
	getmouseco_areawin(dd->mval);
	dd->previous_mval[0] = -1;
	dd->previous_mval[1] = -1;
	dd->type = PT_EXACT;
}

/********************************************/

void sk_queueRedrawSketch(SK_Sketch *sketch)
{
	if (sketch->active_stroke != NULL)
	{
		SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);
		
		if (last != NULL)
		{
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

void sk_drawSketch(SK_Sketch *sketch)
{
	SK_Stroke *stk;
	
	glDisable(GL_DEPTH_TEST);

	glLineWidth(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));

	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		sk_drawStroke(stk);
	}
	
	if (sketch->active_stroke != NULL)
	{
		SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);
		
		if (last != NULL)
		{
			SK_DrawData dd;
			float vec[3];
			
			sk_initDrawData(&dd);
			sk_projectPaintData(sketch->active_stroke, &dd, vec);
			
			glEnable(GL_LINE_STIPPLE);
			glColor3f(1, 0.5, 0);
			glBegin(GL_LINE_STRIP);
			
				glVertex3fv(last->p);
				glVertex3fv(vec);
			
			glEnd();
			
			glDisable(GL_LINE_STIPPLE);
			
			if (G.qual & LR_CTRLKEY)
			{
				SK_Point *snap_pt = sk_snapPoint(sketch, dd.mval, 30);
				
				if (snap_pt != NULL)
				{
					glColor3f(0, 0.5, 1);
					glBegin(GL_POINTS);
					
						glVertex3fv(snap_pt->p);
					
					glEnd();
				}
			}
		}
	}
	
	glLineWidth(1.0);
	glPointSize(1.0);

	glEnable(GL_DEPTH_TEST);
}

int sk_paint(SK_Sketch *sketch, short mbut)
{
	int retval = 1;
	
	if (mbut == LEFTMOUSE)
	{
		SK_DrawData dd;
		if (sketch->active_stroke == NULL)
		{
			sk_startStroke(sketch);
		}

		sk_initDrawData(&dd);
		
		/* paint loop */
		do {
			/* get current user input */
			getmouseco_areawin(dd.mval);
			
			/* only add current point to buffer if mouse moved (otherwise wait until it does) */
			if (sk_stroke_filtermval(&dd)) {
				if (G.qual & LR_CTRLKEY)
				{
					SK_Point *snap_pt = sk_snapPoint(sketch, dd.mval, 30);
					
					if (snap_pt != NULL)
					{
						sk_addStrokeSnapPoint(sketch->active_stroke, &dd, snap_pt);
					}
					else
					{
						sk_addStrokeDrawPoint(sketch->active_stroke, &dd);
					}
				}
				else if (G.qual & LR_SHIFTKEY)
				{
					sk_addStrokeEmbedPoint(sketch->active_stroke, &dd);
				}
				else
				{
					sk_addStrokeDrawPoint(sketch->active_stroke, &dd);
				}
				
				sk_updateDrawData(&dd);
				force_draw(0);
			}
			else
			{
				BIF_wait_for_statechange();
			}
			
			while( qtest() ) {
				short event, val;
				event = extern_qread(&val);
			}
			
			/* do mouse checking at the end, so don't check twice, and potentially
			 * miss a short tap 
			 */
		} while (get_mbut() & LEFTMOUSE);
		
		sk_endContinuousStroke(sketch->active_stroke);
	}
	else if (mbut == RIGHTMOUSE)
	{
		if (sketch->active_stroke != NULL)
		{
			sk_filterStroke(sketch->active_stroke);
			sk_endStroke(sketch);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	
	return retval;
}


void BDR_drawSketch()
{
	if (GLOBAL_sketch != NULL)
	{
		sk_drawSketch(GLOBAL_sketch);
	}
}

int BIF_paintSketch(short mbut)
{
	if (GLOBAL_sketch == NULL)
	{
		GLOBAL_sketch = sk_createSketch();
	}
	
	return sk_paint(GLOBAL_sketch, mbut);
}

void BDR_queueDrawSketch()
{
	if (GLOBAL_sketch != NULL)
	{
		sk_queueRedrawSketch(GLOBAL_sketch);
	}
}
