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
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_armature_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_graph.h"
#include "BLI_ghash.h"

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
#include "BIF_editarmature.h"
#include "BIF_sketch.h"
#include "BIF_retarget.h"
#include "BIF_generate.h"
#include "BIF_interface.h"

#include "BIF_transform.h"

#include "blendef.h"
#include "mydevice.h"
#include "reeb.h"

typedef enum SK_PType
{
	PT_CONTINUOUS,
	PT_EXACT,
} SK_PType;

typedef enum SK_PMode
{
	PT_SNAP,
	PT_PROJECT,
} SK_PMode;

typedef struct SK_Point
{
	float p[3];
	float no[3];
	SK_PType type;
	SK_PMode mode;
} SK_Point;

typedef struct SK_Stroke
{
	struct SK_Stroke *next, *prev;

	SK_Point *points;
	int nb_points;
	int buf_size;
	int selected;
} SK_Stroke;

#define SK_OVERDRAW_LIMIT	5

typedef struct SK_Overdraw
{
	SK_Stroke *target;
	int	start, end;
	int count;
} SK_Overdraw;

#define SK_Stroke_BUFFER_INIT_SIZE 20

typedef struct SK_DrawData
{
	short mval[2];
	short previous_mval[2];
	SK_PType type;
} SK_DrawData;

typedef struct SK_Intersection
{
	struct SK_Intersection *next, *prev;
	SK_Stroke *stroke;
	int			before;
	int			after;
	int			gesture_index;
	float		p[3];
	float		lambda; /* used for sorting intersection points */
} SK_Intersection;

typedef struct SK_Sketch
{
	ListBase	strokes;
	SK_Stroke	*active_stroke;
	SK_Stroke	*gesture;
	SK_Point	next_point;
	SK_Overdraw over;
} SK_Sketch;

typedef struct SK_StrokeIterator {
	HeadFct		head;
	TailFct		tail;
	PeekFct		peek;
	NextFct		next;
	NextNFct	nextN;
	PreviousFct	previous;
	StoppedFct	stopped;
	
	float *p, *no;
	
	int length;
	int index;
	/*********************************/
	SK_Stroke *stroke;
	int start;
	int end;
	int stride;
} SK_StrokeIterator;

typedef struct SK_Gesture {
	SK_Stroke	*stk;
	SK_Stroke	*segments;

	ListBase	intersections;
	ListBase	self_intersections;

	int			nb_self_intersections;
	int			nb_intersections;
	int			nb_segments;
} SK_Gesture;

typedef int  (*GestureDetectFct)(SK_Gesture*, SK_Sketch *);
typedef void (*GestureApplyFct)(SK_Gesture*, SK_Sketch *);

typedef struct SK_GestureAction {
	char name[64];
	GestureDetectFct	detect;
	GestureApplyFct		apply;
} SK_GestureAction;

SK_Sketch *GLOBAL_sketch = NULL;
SK_Point boneSnap;
int    LAST_SNAP_POINT_VALID = 0;
float  LAST_SNAP_POINT[3];

/******************** PROTOTYPES ******************************/

void initStrokeIterator(BArcIterator *iter, SK_Stroke *stk, int start, int end);

void sk_deleteSelectedStrokes(SK_Sketch *sketch);

void sk_freeStroke(SK_Stroke *stk);
void sk_freeSketch(SK_Sketch *sketch);

SK_Point *sk_lastStrokePoint(SK_Stroke *stk);

int sk_detectCutGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyCutGesture(SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectTrimGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyTrimGesture(SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectCommandGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyCommandGesture(SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectDeleteGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyDeleteGesture(SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectMergeGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyMergeGesture(SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectReverseGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyReverseGesture(SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectConvertGesture(SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyConvertGesture(SK_Gesture *gest, SK_Sketch *sketch);


void sk_resetOverdraw(SK_Sketch *sketch);
int sk_hasOverdraw(SK_Sketch *sketch, SK_Stroke *stk);

/******************** GESTURE ACTIONS ******************************/

SK_GestureAction GESTURE_ACTIONS[] =
	{
		{"Cut", sk_detectCutGesture, sk_applyCutGesture},
		{"Trim", sk_detectTrimGesture, sk_applyTrimGesture},
		{"Command", sk_detectCommandGesture, sk_applyCommandGesture},
		{"Delete", sk_detectDeleteGesture, sk_applyDeleteGesture},
		{"Merge", sk_detectMergeGesture, sk_applyMergeGesture},
		{"Reverse", sk_detectReverseGesture, sk_applyReverseGesture},
		{"Convert", sk_detectConvertGesture, sk_applyConvertGesture},
		{"", NULL, NULL}
	};

/******************** TEMPLATES UTILS *************************/

char  *TEMPLATES_MENU = NULL;
int    TEMPLATES_CURRENT = 0;
GHash *TEMPLATES_HASH = NULL;
RigGraph *TEMPLATE_RIGG = NULL;

void BIF_makeListTemplates()
{
	Base *base;
	int index = 0;

	if (TEMPLATES_HASH != NULL)
	{
		BLI_ghash_free(TEMPLATES_HASH, NULL, NULL);
	}
	
	TEMPLATES_HASH = BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
	TEMPLATES_CURRENT = 0;

	for ( base = FIRSTBASE; base; base = base->next )
	{
		Object *ob = base->object;
		
		if (ob != G.obedit && ob->type == OB_ARMATURE)
		{
			index++;
			BLI_ghash_insert(TEMPLATES_HASH, SET_INT_IN_POINTER(index), ob);
			
			if (ob == G.scene->toolsettings->skgen_template)
			{
				TEMPLATES_CURRENT = index;
			}
		}
	}
}

char *BIF_listTemplates()
{
	GHashIterator ghi;
	char menu_header[] = "Template%t|None%x0|";
	char *p;
	
	if (TEMPLATES_MENU != NULL)
	{
		MEM_freeN(TEMPLATES_MENU);
	}
	
	TEMPLATES_MENU = MEM_callocN(sizeof(char) * (BLI_ghash_size(TEMPLATES_HASH) * 32 + 30), "skeleton template menu");
	
	p = TEMPLATES_MENU;
	
	p += sprintf(TEMPLATES_MENU, "%s", menu_header);
	
	BLI_ghashIterator_init(&ghi, TEMPLATES_HASH);
	
	while (!BLI_ghashIterator_isDone(&ghi))
	{
		Object *ob = BLI_ghashIterator_getValue(&ghi);
		int key = (int)BLI_ghashIterator_getKey(&ghi);
		
		p += sprintf(p, "|%s%%x%i", ob->id.name+2, key);
		
		BLI_ghashIterator_step(&ghi);
	}
	
	return TEMPLATES_MENU;
}

int   BIF_currentTemplate()
{
	if (TEMPLATES_CURRENT == 0 && G.scene->toolsettings->skgen_template != NULL)
	{
		GHashIterator ghi;
		BLI_ghashIterator_init(&ghi, TEMPLATES_HASH);
		
		while (!BLI_ghashIterator_isDone(&ghi))
		{
			Object *ob = BLI_ghashIterator_getValue(&ghi);
			int key = (int)BLI_ghashIterator_getKey(&ghi);
			
			if (ob == G.scene->toolsettings->skgen_template)
			{
				TEMPLATES_CURRENT = key;
				break;
			}
			
			BLI_ghashIterator_step(&ghi);
		}
	}
	
	return TEMPLATES_CURRENT;
}

RigGraph* sk_makeTemplateGraph(Object *ob)
{
	if (ob == G.obedit)
	{
		return NULL;
	}
	
	if (ob != NULL)
	{
		if (TEMPLATE_RIGG && TEMPLATE_RIGG->ob != ob)
		{
			RIG_freeRigGraph((BGraph*)TEMPLATE_RIGG);
			TEMPLATE_RIGG = NULL;
		}
		
		if (TEMPLATE_RIGG == NULL)
		{
			bArmature *arm;

			arm = ob->data;
			
			TEMPLATE_RIGG = RIG_graphFromArmature(ob, arm);
		}
	}
	
	return TEMPLATE_RIGG;
}

int BIF_nbJointsTemplate()
{
	RigGraph *rg = sk_makeTemplateGraph(G.scene->toolsettings->skgen_template);
	
	if (rg)
	{
		return RIG_nbJoints(rg);
	}
	else
	{
		return -1; 
	}
}

char * BIF_nameBoneTemplate()
{
	SK_Sketch *stk = GLOBAL_sketch;
	RigGraph *rg;
	int index = 0;

	if (stk && stk->active_stroke != NULL)
	{
		index = stk->active_stroke->nb_points;
	}
	
	rg = sk_makeTemplateGraph(G.scene->toolsettings->skgen_template);
	
	if (rg == NULL)
	{
		return "";
	}

	return RIG_nameBone(rg, 0, index);
}

void  BIF_freeTemplates()
{
	if (TEMPLATES_MENU != NULL)
	{
		MEM_freeN(TEMPLATES_MENU);
		TEMPLATES_MENU = NULL;
	}
	
	if (TEMPLATES_HASH != NULL)
	{
		BLI_ghash_free(TEMPLATES_HASH, NULL, NULL);
		TEMPLATES_HASH = NULL;
	}
	
	if (TEMPLATE_RIGG != NULL)
	{
		RIG_freeRigGraph((BGraph*)TEMPLATE_RIGG);
		TEMPLATE_RIGG = NULL;
	}
}

void  BIF_setTemplate(int index)
{
	if (index > 0)
	{
		G.scene->toolsettings->skgen_template = BLI_ghash_lookup(TEMPLATES_HASH, SET_INT_IN_POINTER(index));
	}
	else
	{
		G.scene->toolsettings->skgen_template = NULL;
		
		if (TEMPLATE_RIGG != NULL)
		{
			RIG_freeRigGraph((BGraph*)TEMPLATE_RIGG);
		}
		TEMPLATE_RIGG = NULL;
	}
}	

/*********************** CONVERSION ***************************/

void sk_autoname(ReebArc *arc)
{
	if (G.scene->toolsettings->skgen_retarget_options & SK_RETARGET_AUTONAME)
	{
		if (arc == NULL)
		{
			char *num = G.scene->toolsettings->skgen_num_string;
			int i = atoi(num);
			i++;
			BLI_snprintf(num, 8, "%i", i);
		}
		else
		{
			char *side = G.scene->toolsettings->skgen_side_string;
			int valid = 0;
			int caps = 0;
			
			if (BLI_streq(side, ""))
			{
				valid = 1;
			}
			else if (BLI_streq(side, "R") || BLI_streq(side, "L"))
			{
				valid = 1;
				caps = 1;
			}
			else if (BLI_streq(side, "r") || BLI_streq(side, "l"))
			{
				valid = 1;
				caps = 0;
			}
			
			if (valid)
			{
				if (arc->head->p[0] < 0)
				{
					BLI_snprintf(side, 8, caps?"R":"r");
				}
				else
				{
					BLI_snprintf(side, 8, caps?"L":"l");
				}
			}
		}
	}
}

ReebNode *sk_pointToNode(SK_Point *pt, float imat[][4], float tmat[][3])
{
	ReebNode *node;
	
	node = MEM_callocN(sizeof(ReebNode), "reeb node");
	VECCOPY(node->p, pt->p);
	Mat4MulVecfl(imat, node->p);
	
	VECCOPY(node->no, pt->no);
	Mat3MulVecfl(tmat, node->no);
	
	return node;
}

ReebArc *sk_strokeToArc(SK_Stroke *stk, float imat[][4], float tmat[][3])
{
	ReebArc *arc;
	int i;
	
	arc = MEM_callocN(sizeof(ReebArc), "reeb arc");
	arc->head = sk_pointToNode(stk->points, imat, tmat);
	arc->tail = sk_pointToNode(sk_lastStrokePoint(stk), imat, tmat);
	
	arc->bcount = stk->nb_points - 2; /* first and last are nodes, don't count */
	arc->buckets = MEM_callocN(sizeof(EmbedBucket) * arc->bcount, "Buckets");
	
	for (i = 0; i < arc->bcount; i++)
	{
		VECCOPY(arc->buckets[i].p, stk->points[i + 1].p);
		Mat4MulVecfl(imat, arc->buckets[i].p);

		VECCOPY(arc->buckets[i].no, stk->points[i + 1].no);
		Mat3MulVecfl(tmat, arc->buckets[i].no);
	}
	
	return arc;
}

void sk_retargetStroke(SK_Stroke *stk)
{
	float imat[4][4];
	float tmat[3][3];
	ReebArc *arc;
	RigGraph *rg;
	
	Mat4Invert(imat, G.obedit->obmat);
	
	Mat3CpyMat4(tmat, G.obedit->obmat);
	Mat3Transp(tmat);

	arc = sk_strokeToArc(stk, imat, tmat);
	
	sk_autoname(arc);
	
	rg = sk_makeTemplateGraph(G.scene->toolsettings->skgen_template);

	BIF_retargetArc(arc, rg);
	
	sk_autoname(NULL);
	
	MEM_freeN(arc->head);
	MEM_freeN(arc->tail);
	REEB_freeArc((BArc*)arc);
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
	sketch->gesture = NULL;

	sketch->strokes.first = NULL;
	sketch->strokes.last = NULL;
	
	return sketch;
}

void sk_initPoint(SK_Point *pt)
{
	VECCOPY(pt->no, G.vd->viewinv[2]);
	Normalize(pt->no);
	/* more init code here */
}

void sk_copyPoint(SK_Point *dst, SK_Point *src)
{
	memcpy(dst, src, sizeof(SK_Point));
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
	
	stk->selected = 0;
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

void sk_growStrokeBufferN(SK_Stroke *stk, int n)
{
	if (stk->nb_points + n > stk->buf_size)
	{
		SK_Point *old_points = stk->points;
		
		while (stk->nb_points + n > stk->buf_size)
		{
			stk->buf_size *= 2;
		}
		
		sk_allocStrokeBuffer(stk);
		
		memcpy(stk->points, old_points, sizeof(SK_Point) * stk->nb_points);
		
		MEM_freeN(old_points);
	}
}


void sk_replaceStrokePoint(SK_Stroke *stk, SK_Point *pt, int n)
{
	memcpy(stk->points + n, pt, sizeof(SK_Point));
}

void sk_insertStrokePoint(SK_Stroke *stk, SK_Point *pt, int n)
{
	int size = stk->nb_points - n;
	
	sk_growStrokeBuffer(stk);
	
	memmove(stk->points + n + 1, stk->points + n, size * sizeof(SK_Point));
	
	memcpy(stk->points + n, pt, sizeof(SK_Point));
	
	stk->nb_points++;
}

void sk_appendStrokePoint(SK_Stroke *stk, SK_Point *pt)
{
	sk_growStrokeBuffer(stk);
	
	memcpy(stk->points + stk->nb_points, pt, sizeof(SK_Point));
	
	stk->nb_points++;
}

void sk_insertStrokePoints(SK_Stroke *stk, SK_Point *pts, int len, int start, int end)
{
	int size = end - start + 1;
	
	sk_growStrokeBufferN(stk, len - size);
	
	if (len != size)
	{
		int tail_size = stk->nb_points - end + 1;
		
		memmove(stk->points + start + len, stk->points + end + 1, tail_size * sizeof(SK_Point));
	}
	
	memcpy(stk->points + start, pts, len * sizeof(SK_Point));
	
	stk->nb_points += len - size;
}

void sk_trimStroke(SK_Stroke *stk, int start, int end)
{
	int size = end - start + 1;
	
	if (start > 0)
	{
		memmove(stk->points, stk->points + start, size * sizeof(SK_Point));
	}
	
	stk->nb_points = size;
}

void sk_straightenStroke(SK_Stroke *stk, int start, int end, float p_start[3], float p_end[3])
{
	SK_Point pt1, pt2;
	SK_Point *prev, *next;
	float delta_p[3];
	int i, total;
	
	total = end - start;

	VecSubf(delta_p, p_end, p_start);
	
	prev = stk->points + start;
	next = stk->points + end;
	
	VECCOPY(pt1.p, p_start);
	VECCOPY(pt1.no, prev->no);
	pt1.mode = prev->mode;
	pt1.type = prev->type;
	
	VECCOPY(pt2.p, p_end);
	VECCOPY(pt2.no, next->no);
	pt2.mode = next->mode;
	pt2.type = next->type;
	
	sk_insertStrokePoint(stk, &pt1, start + 1); /* insert after start */
	sk_insertStrokePoint(stk, &pt2, end + 1); /* insert before end (since end was pushed back already) */
	
	for (i = 1; i < total; i++)
	{
		float delta = (float)i / (float)total;
		float *p = stk->points[start + 1 + i].p;

		VECCOPY(p, delta_p);
		VecMulf(p, delta);
		VecAddf(p, p, p_start);
	} 
}

void sk_polygonizeStroke(SK_Stroke *stk, int start, int end)
{
	int offset;
	int i;
	
	/* find first exact points outside of range */
	for (;start > 0; start--)
	{
		if (stk->points[start].type == PT_EXACT)
		{
			break;
		}
	}
	
	for (;end < stk->nb_points - 1; end++)
	{
		if (stk->points[end].type == PT_EXACT)
		{
			break;
		}
	}
	
	offset = start + 1;
	
	for (i = start + 1; i < end; i++)
	{
		if (stk->points[i].type == PT_EXACT)
		{
			if (offset != i)
			{
				memcpy(stk->points + offset, stk->points + i, sizeof(SK_Point));
			}

			offset++;
		}
	}
	
	/* some points were removes, move end of array */
	if (offset < end)
	{
		int size = stk->nb_points - end;
		memmove(stk->points + offset, stk->points + end, size * sizeof(SK_Point));
		stk->nb_points = offset + size;
	}
}

void sk_flattenStroke(SK_Stroke *stk, int start, int end)
{
	float normal[3], distance[3];
	float limit;
	int i, total;
	
	total = end - start + 1;
	
	VECCOPY(normal, stk->points[start].no);
	
	VecSubf(distance, stk->points[end].p, stk->points[start].p);
	Projf(normal, distance, normal);
	limit = Normalize(normal);
	
	for (i = 1; i < total - 1; i++)
	{
		float d = limit * i / total;
		float offset[3];
		float *p = stk->points[start + i].p;

		VecSubf(distance, p, stk->points[start].p);
		Projf(distance, distance, normal);
		
		VECCOPY(offset, normal);
		VecMulf(offset, d);
		
		VecSubf(p, p, distance);
		VecAddf(p, p, offset);
	} 
}

void sk_removeStroke(SK_Sketch *sketch, SK_Stroke *stk)
{
	if (sketch->active_stroke == stk)
	{
		sketch->active_stroke = NULL;
	}
	
	BLI_remlink(&sketch->strokes, stk);
	sk_freeStroke(stk);
}

void sk_reverseStroke(SK_Stroke *stk)
{
	SK_Point *old_points = stk->points;
	int i = 0;
	
	sk_allocStrokeBuffer(stk);
	
	for (i = 0; i < stk->nb_points; i++)
	{
		sk_copyPoint(stk->points + i, old_points + stk->nb_points - 1 - i);
	}
	
	MEM_freeN(old_points);
}


void sk_cancelStroke(SK_Sketch *sketch)
{
	if (sketch->active_stroke != NULL)
	{
		sk_resetOverdraw(sketch);
		sk_removeStroke(sketch, sketch->active_stroke);
	}
}

/* Apply reverse Chaikin filter to simplify the polyline
 * */
void sk_filterStroke(SK_Stroke *stk, int start, int end)
{
	SK_Point *old_points = stk->points;
	int nb_points = stk->nb_points;
	int i, j;
	
	return;
	
	if (start == -1)
	{
		start = 0;
		end = stk->nb_points - 1;
	}

	sk_allocStrokeBuffer(stk);
	stk->nb_points = 0;
	
	/* adding points before range */
	for (i = 0; i < start; i++)
	{
		sk_appendStrokePoint(stk, old_points + i);
	}
	
	for (i = start, j = start; i <= end; i++)
	{
		if (i - j == 3)
		{
			SK_Point pt;
			float vec[3];
			
			sk_copyPoint(&pt, &old_points[j+1]);

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
		
		/* this might be uneeded when filtering last continuous stroke */
		if (old_points[i].type == PT_EXACT)
		{
			sk_appendStrokePoint(stk, old_points + i);
			j = i;
		}
	} 
	
	/* adding points after range */
	for (i = end + 1; i < nb_points; i++)
	{
		sk_appendStrokePoint(stk, old_points + i);
	}

	MEM_freeN(old_points);

	sk_shrinkStrokeBuffer(stk);
}

void sk_filterLastContinuousStroke(SK_Stroke *stk)
{
	int start, end;
	
	end = stk->nb_points -1;
	
	for (start = end - 1; start > 0 && stk->points[start].type == PT_CONTINUOUS; start--)
	{
		/* nothing to do here*/
	}
	
	if (end - start > 1)
	{
		sk_filterStroke(stk, start, end);
	}
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

void sk_drawStroke(SK_Stroke *stk, int id, float color[3], int start, int end)
{
	float rgb[3];
	int i;
	
	if (id != -1)
	{
		glLoadName(id);
		
		glBegin(GL_LINE_STRIP);
		
		for (i = 0; i < stk->nb_points; i++)
		{
			glVertex3fv(stk->points[i].p);
		}
		
		glEnd();
		
	}
	else
	{
		float d_rgb[3] = {1, 1, 1};
		
		VECCOPY(rgb, color);
		VecSubf(d_rgb, d_rgb, rgb);
		VecMulf(d_rgb, 1.0f / (float)stk->nb_points);
		
		glBegin(GL_LINE_STRIP);

		for (i = 0; i < stk->nb_points; i++)
		{
			if (i >= start && i <= end)
			{
				glColor3f(0.3, 0.3, 0.3);
			}
			else
			{
				glColor3fv(rgb);
			}
			glVertex3fv(stk->points[i].p);
			VecAddf(rgb, rgb, d_rgb);
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
	}

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

void drawSubdividedStrokeBy(BArcIterator *iter, NextSubdivisionFunc next_subdividion)
{
	float head[3], tail[3];
	int bone_start = 0;
	int end = iter->length;
	int index;

	iter->head(iter);
	VECCOPY(head, iter->p);
	
	glColor3f(0, 1, 0);
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE) * 2);
	glBegin(GL_POINTS);
	
	index = next_subdividion(iter, bone_start, end, head, tail);
	while (index != -1)
	{
		glVertex3fv(tail);
		
		VECCOPY(head, tail);
		bone_start = index; // start next bone from current index

		index = next_subdividion(iter, bone_start, end, head, tail);
	}
	
	glEnd();
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
}

void sk_drawStrokeSubdivision(SK_Stroke *stk)
{
	int head_index = -1;
	int i;
	
	if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_RETARGET)
	{
		return;
	}

	
	for (i = 0; i < stk->nb_points; i++)
	{
		SK_Point *pt = stk->points + i;
		
		if (pt->type == PT_EXACT || i == stk->nb_points - 1) /* stop on exact or on last point */
		{
			if (head_index == -1)
			{
				head_index = i;
			}
			else
			{
				if (i - head_index > 1)
				{
					SK_StrokeIterator sk_iter;
					BArcIterator *iter = (BArcIterator*)&sk_iter;
					
					initStrokeIterator(iter, stk, head_index, i);

					if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_CUT_ADAPTATIVE)
					{
						drawSubdividedStrokeBy(iter, nextAdaptativeSubdivision);
					}
					else if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_CUT_LENGTH)
					{
						drawSubdividedStrokeBy(iter, nextLengthSubdivision);
					}
					else if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_CUT_FIXED)
					{
						drawSubdividedStrokeBy(iter, nextFixedSubdivision);
					}
					
				}

				head_index = i;
			}
		}
	}	
}

SK_Point *sk_snapPointStroke(SK_Stroke *stk, short mval[2], int *dist, int *index, int all_pts)
{
	SK_Point *pt = NULL;
	int i;
	
	for (i = 0; i < stk->nb_points; i++)
	{
		if (all_pts || stk->points[i].type == PT_EXACT)
		{
			short pval[2];
			int pdist;
			
			project_short_noclip(stk->points[i].p, pval);
			
			pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
			
			if (pdist < *dist)
			{
				*dist = pdist;
				pt = stk->points + i;
				
				if (index != NULL)
				{
					*index = i;
				}
			}
		}
	}
	
	return pt;
}

SK_Point *sk_snapPointArmature(Object *ob, ListBase *ebones, short mval[2], int *dist)
{
	SK_Point *pt = NULL;
	EditBone *bone;
	
	for (bone = ebones->first; bone; bone = bone->next)
	{
		float vec[3];
		short pval[2];
		int pdist;
		
		if ((bone->flag & BONE_CONNECTED) == 0)
		{
			VECCOPY(vec, bone->head);
			Mat4MulVecfl(ob->obmat, vec);
			project_short_noclip(vec, pval);
			
			pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
			
			if (pdist < *dist)
			{
				*dist = pdist;
				pt = &boneSnap;
				VECCOPY(pt->p, vec);
				pt->type = PT_EXACT;
			}
		}
		
		
		VECCOPY(vec, bone->tail);
		Mat4MulVecfl(ob->obmat, vec);
		project_short_noclip(vec, pval);
		
		pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);
		
		if (pdist < *dist)
		{
			*dist = pdist;
			pt = &boneSnap;
			VECCOPY(pt->p, vec);
			pt->type = PT_EXACT;
		}
	}
	
	return pt;
}

void sk_resetOverdraw(SK_Sketch *sketch)
{
	sketch->over.target = NULL;
	sketch->over.start = -1;
	sketch->over.end = -1;
	sketch->over.count = 0;
}

int sk_hasOverdraw(SK_Sketch *sketch, SK_Stroke *stk)
{
	return	sketch->over.target &&
			sketch->over.count >= SK_OVERDRAW_LIMIT &&
			(sketch->over.target == stk || stk == NULL) &&
			(sketch->over.start != -1 || sketch->over.end != -1);
}

void sk_updateOverdraw(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	if (sketch->over.target == NULL)
	{
		SK_Stroke *target;
		int closest_index = -1;
		int dist = SNAP_MIN_DISTANCE * 2;
		
		/* If snapping, don't start overdraw */
		if (sk_lastStrokePoint(stk)->mode == PT_SNAP)
		{
			return;
		}
		
		for (target = sketch->strokes.first; target; target = target->next)
		{
			if (target != stk)
			{
				int index;
				
				SK_Point *spt = sk_snapPointStroke(target, dd->mval, &dist, &index, 1);
				
				if (spt != NULL)
				{
					sketch->over.target = target;
					closest_index = index;
				}
			}
		}
		
		if (sketch->over.target != NULL)
		{
			if (closest_index > -1)
			{
				if (sk_lastStrokePoint(stk)->type == PT_EXACT)
				{
					sketch->over.count = SK_OVERDRAW_LIMIT;
				}
				else
				{
					sketch->over.count++;
				}
			}

			if (stk->nb_points == 1)
			{
				sketch->over.start = closest_index;
			}
			else
			{
				sketch->over.end = closest_index;
			}
		}
	}
	else if (sketch->over.target != NULL)
	{
		SK_Point *closest_pt = NULL;
		int dist = SNAP_MIN_DISTANCE * 2;
		int index;

		closest_pt = sk_snapPointStroke(sketch->over.target, dd->mval, &dist, &index, 1);
		
		if (closest_pt != NULL)
		{
			if (sk_lastStrokePoint(stk)->type == PT_EXACT)
			{
				sketch->over.count = SK_OVERDRAW_LIMIT;
			}
			else
			{
				sketch->over.count++;
			}
			
			sketch->over.end = index;
		}
		else
		{
			sketch->over.end = -1;
		}
	}
}

/* return 1 on reverse needed */
int sk_adjustIndexes(SK_Sketch *sketch, int *start, int *end)
{
	int retval = 0;

	*start = sketch->over.start;
	*end = sketch->over.end;
	
	if (*start == -1)
	{
		*start = 0;
	}
	
	if (*end == -1)
	{
		*end = sketch->over.target->nb_points - 1;
	}
	
	if (*end < *start)
	{
		int tmp = *start;
		*start = *end;
		*end = tmp;
		retval = 1;
	}
	
	return retval;
}

void sk_endOverdraw(SK_Sketch *sketch)
{
	SK_Stroke *stk = sketch->active_stroke;
	
	if (sk_hasOverdraw(sketch, NULL))
	{
		int start;
		int end;
		
		if (sk_adjustIndexes(sketch, &start, &end))
		{
			sk_reverseStroke(stk);
		}
		
		if (stk->nb_points > 1)
		{
			stk->points->type = sketch->over.target->points[start].type;
			sk_lastStrokePoint(stk)->type = sketch->over.target->points[end].type;
		}
		
		sk_insertStrokePoints(sketch->over.target, stk->points, stk->nb_points, start, end);
		
		sk_removeStroke(sketch, stk);
		
		sk_resetOverdraw(sketch);
	}
}


void sk_startStroke(SK_Sketch *sketch)
{
	SK_Stroke *stk = sk_createStroke();
	
	BLI_addtail(&sketch->strokes, stk);
	sketch->active_stroke = stk;

	sk_resetOverdraw(sketch);	
}

void sk_endStroke(SK_Sketch *sketch)
{
	sk_shrinkStrokeBuffer(sketch->active_stroke);

	if (G.scene->toolsettings->bone_sketching & BONE_SKETCHING_ADJUST)
	{
		sk_endOverdraw(sketch);
	}

	sketch->active_stroke = NULL;
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

void sk_interpolateDepth(SK_Stroke *stk, int start, int end, float length, float distance)
{
	float progress = 0;
	int i;
	
	progress = VecLenf(stk->points[start].p, stk->points[start - 1].p);
	
	for (i = start; i <= end; i++)
	{
		float ray_start[3], ray_normal[3];
		float delta = VecLenf(stk->points[i].p, stk->points[i + 1].p);
		short pval[2];
		
		project_short_noclip(stk->points[i].p, pval);
		viewray(pval, ray_start, ray_normal);
		
		VecMulf(ray_normal, distance * progress / length);
		VecAddf(stk->points[i].p, stk->points[i].p, ray_normal);

		progress += delta ;
	}
}

void sk_projectDrawPoint(float vec[3], SK_Stroke *stk, SK_DrawData *dd)
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

int sk_getStrokeDrawPoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	pt->type = dd->type;
	pt->mode = PT_PROJECT;
	sk_projectDrawPoint(pt->p, stk, dd);
	
	return 1;
}

int sk_addStrokeDrawPoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	SK_Point pt;
	
	sk_initPoint(&pt);
	
	sk_getStrokeDrawPoint(&pt, sketch, stk, dd);

	sk_appendStrokePoint(stk, &pt);
	
	return 1;
}

int sk_getStrokeSnapPoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	int point_added = 0;

	if (G.scene->snap_mode == SCE_SNAP_MODE_VOLUME)
	{
		ListBase depth_peels;
		DepthPeel *p1, *p2;
		float *last_p = NULL;
		float dist = FLT_MAX;
		float p[3];
		
		depth_peels.first = depth_peels.last = NULL;
		
		peelObjects(&depth_peels, dd->mval);
		
		if (stk->nb_points > 0 && stk->points[stk->nb_points - 1].type == PT_CONTINUOUS)
		{
			last_p = stk->points[stk->nb_points - 1].p;
		}
		else if (LAST_SNAP_POINT_VALID)
		{
			last_p = LAST_SNAP_POINT;
		}
		
		
		for (p1 = depth_peels.first; p1; p1 = p1->next)
		{
			if (p1->flag == 0)
			{
				float vec[3];
				float new_dist;
				
				p2 = NULL;
				p1->flag = 1;
	
				/* if peeling objects, take the first and last from each object */			
				if (G.scene->snap_flag & SCE_SNAP_PEEL_OBJECT)
				{
					DepthPeel *peel;
					for (peel = p1->next; peel; peel = peel->next)
					{
						if (peel->ob == p1->ob)
						{
							peel->flag = 1;
							p2 = peel;
						}
					}
				}
				/* otherwise, pair first with second and so on */
				else
				{
					for (p2 = p1->next; p2 && p2->ob != p1->ob; p2 = p2->next)
					{
						/* nothing to do here */
					}
				}
				
				if (p2)
				{
					p2->flag = 1;
					
					VecAddf(vec, p1->p, p2->p);
					VecMulf(vec, 0.5f);
				}
				else
				{
					VECCOPY(vec, p1->p);
				}
				
				if (last_p == NULL)
				{
					VECCOPY(p, vec);
					dist = 0;
					break;
				}
				
				new_dist = VecLenf(last_p, vec);
				
				if (new_dist < dist)
				{
					VECCOPY(p, vec);
					dist = new_dist;
				}
			}
		}
		
		if (dist != FLT_MAX)
		{
			pt->type = dd->type;
			pt->mode = PT_SNAP;
			VECCOPY(pt->p, p);
			
			point_added = 1;
		}
		
		BLI_freelistN(&depth_peels);
	}
	else
	{
		SK_Stroke *snap_stk;
		float vec[3];
		float no[3];
		int found = 0;
		int dist = SNAP_MIN_DISTANCE; // Use a user defined value here

		/* snap to strokes */
		// if (G.scene->snap_mode == SCE_SNAP_MODE_VERTEX) /* snap all the time to strokes */
		for (snap_stk = sketch->strokes.first; snap_stk; snap_stk = snap_stk->next)
		{
			SK_Point *spt = NULL;
			if (snap_stk == stk)
			{
				spt = sk_snapPointStroke(snap_stk, dd->mval, &dist, NULL, 0);
			}
			else
			{
				spt = sk_snapPointStroke(snap_stk, dd->mval, &dist, NULL, 1);
			}
				
			if (spt != NULL)
			{
				VECCOPY(pt->p, spt->p);
				point_added = 1;
			}
		}

		/* try to snap to closer object */
		found = snapObjects(&dist, vec, no, NOT_SELECTED);
		if (found == 1)
		{
			pt->type = dd->type;
			pt->mode = PT_SNAP;
			VECCOPY(pt->p, vec);
			
			point_added = 1;
		}
	}
	
	return point_added;
}

int sk_addStrokeSnapPoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	int point_added;
	SK_Point pt;
	
	sk_initPoint(&pt);

	point_added = sk_getStrokeSnapPoint(&pt, sketch, stk, dd);
	
	if (point_added)
	{
		float final_p[3];
		float length, distance;
		int total;
		int i;
		
		VECCOPY(final_p, pt.p);
		
		sk_projectDrawPoint(pt.p, stk, dd);
		sk_appendStrokePoint(stk, &pt);
		
		/* update all previous point to give smooth Z progresion */
		total = 0;
		length = 0;
		for (i = stk->nb_points - 2; i > 0; i--)
		{
			length += VecLenf(stk->points[i].p, stk->points[i + 1].p);
			total++;
			if (stk->points[i].mode == PT_SNAP || stk->points[i].type == PT_EXACT)
			{
				break;
			}
		}
		
		if (total > 1)
		{
			distance = sk_distanceDepth(final_p, stk->points[i].p);
			
			sk_interpolateDepth(stk, i + 1, stk->nb_points - 2, length, distance);
		}
		
		VECCOPY(stk->points[stk->nb_points - 1].p, final_p);
		
		point_added = 1;
	}
	
	return point_added;
}

void sk_addStrokePoint(SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, short qual)
{
	int point_added = 0;
	
	if (qual & LR_CTRLKEY)
	{
		point_added = sk_addStrokeSnapPoint(sketch, stk, dd);
	}
	
	if (point_added == 0)
	{
		point_added = sk_addStrokeDrawPoint(sketch, stk, dd);
	}
	
	if (G.scene->toolsettings->bone_sketching & BONE_SKETCHING_ADJUST)
	{
		sk_updateOverdraw(sketch, stk, dd);
	}
}

void sk_getStrokePoint(SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, short qual)
{
	int point_added = 0;
	
	if (qual & LR_CTRLKEY)
	{
		point_added = sk_getStrokeSnapPoint(pt, sketch, stk, dd);
		LAST_SNAP_POINT_VALID = 1;
		VECCOPY(LAST_SNAP_POINT, pt->p);
	}
	else
	{
		LAST_SNAP_POINT_VALID = 0;
	}
	
	if (point_added == 0)
	{
		point_added = sk_getStrokeDrawPoint(pt, sketch, stk, dd);
	}	
}

void sk_endContinuousStroke(SK_Stroke *stk)
{
	stk->points[stk->nb_points - 1].type = PT_EXACT;
}

void sk_updateNextPoint(SK_Sketch *sketch)
{
	if (sketch->active_stroke)
	{
		SK_Stroke *stk = sketch->active_stroke;
		memcpy(&sketch->next_point, stk->points[stk->nb_points - 1].p, sizeof(SK_Point));
	}
}

int sk_stroke_filtermval(SK_DrawData *dd)
{
	int retval = 0;
	if (ABS(dd->mval[0] - dd->previous_mval[0]) + ABS(dd->mval[1] - dd->previous_mval[1]) > U.gp_manhattendist)
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

static void* headPoint(void *arg);
static void* tailPoint(void *arg);
static void* nextPoint(void *arg);
static void* nextNPoint(void *arg, int n);
static void* peekPoint(void *arg, int n);
static void* previousPoint(void *arg);
static int   iteratorStopped(void *arg);

static void initIteratorFct(SK_StrokeIterator *iter)
{
	iter->head = headPoint;
	iter->tail = tailPoint;
	iter->peek = peekPoint;
	iter->next = nextPoint;
	iter->nextN = nextNPoint;
	iter->previous = previousPoint;
	iter->stopped = iteratorStopped;	
}

static SK_Point* setIteratorValues(SK_StrokeIterator *iter, int index)
{
	SK_Point *pt = NULL;
	
	if (index >= 0 && index < iter->length)
	{
		pt = &(iter->stroke->points[iter->start + (iter->stride * index)]);
		iter->p = pt->p;
		iter->no = pt->no;
	}
	else
	{
		iter->p = NULL;
		iter->no = NULL;
	}
	
	return pt;
}

void initStrokeIterator(BArcIterator *arg, SK_Stroke *stk, int start, int end)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;

	initIteratorFct(iter);
	iter->stroke = stk;
	
	if (start < end)
	{
		iter->start = start + 1;
		iter->end = end - 1;
		iter->stride = 1;
	}
	else
	{
		iter->start = start - 1;
		iter->end = end + 1;
		iter->stride = -1;
	}
	
	iter->length = iter->stride * (iter->end - iter->start + 1);
	
	iter->index = -1;
}


static void* headPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;
	SK_Point *result = NULL;
	
	result = &(iter->stroke->points[iter->start - iter->stride]);
	iter->p = result->p;
	iter->no = result->no;
	
	return result;
}

static void* tailPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;
	SK_Point *result = NULL;
	
	result = &(iter->stroke->points[iter->end + iter->stride]);
	iter->p = result->p;
	iter->no = result->no;
	
	return result;
}

static void* nextPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;
	SK_Point *result = NULL;
	
	iter->index++;
	if (iter->index < iter->length)
	{
		result = setIteratorValues(iter, iter->index);
	}

	return result;
}

static void* nextNPoint(void *arg, int n)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;
	SK_Point *result = NULL;
		
	iter->index += n;

	/* check if passed end */
	if (iter->index < iter->length)
	{
		result = setIteratorValues(iter, iter->index);
	}

	return result;
}

static void* peekPoint(void *arg, int n)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;
	SK_Point *result = NULL;
	int index = iter->index + n;

	/* check if passed end */
	if (index < iter->length)
	{
		result = setIteratorValues(iter, index);
	}

	return result;
}

static void* previousPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;
	SK_Point *result = NULL;
	
	if (iter->index > 0)
	{
		iter->index--;
		result = setIteratorValues(iter, iter->index);
	}

	return result;
}

static int iteratorStopped(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator*)arg;

	if (iter->index >= iter->length)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void sk_convertStroke(SK_Stroke *stk)
{
	bArmature *arm = G.obedit->data;
	SK_Point *head;
	EditBone *parent = NULL;
	float invmat[4][4]; /* move in caller function */
	float tmat[3][3];
	int head_index = 0;
	int i;
	
	head = NULL;
	
	Mat4Invert(invmat, G.obedit->obmat);
	
	Mat3CpyMat4(tmat, G.obedit->obmat);
	Mat3Transp(tmat);
	
	for (i = 0; i < stk->nb_points; i++)
	{
		SK_Point *pt = stk->points + i;
		
		if (pt->type == PT_EXACT)
		{
			if (head == NULL)
			{
				head_index = i;
				head = pt;
			}
			else
			{
				EditBone *bone = NULL;
				EditBone *new_parent;
				
				if (i - head_index > 1)
				{
					SK_StrokeIterator sk_iter;
					BArcIterator *iter = (BArcIterator*)&sk_iter;

					initStrokeIterator(iter, stk, head_index, i);
					
					if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_CUT_ADAPTATIVE)
					{
						bone = subdivideArcBy(arm, &G.edbo, iter, invmat, tmat, nextAdaptativeSubdivision);
					}
					else if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_CUT_LENGTH)
					{
						bone = subdivideArcBy(arm, &G.edbo, iter, invmat, tmat, nextLengthSubdivision);
					}
					else if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_CUT_FIXED)
					{
						bone = subdivideArcBy(arm, &G.edbo, iter, invmat, tmat, nextFixedSubdivision);
					}
				}
				
				if (bone == NULL)
				{
					bone = addEditBone("Bone", &G.edbo, arm);
					
					VECCOPY(bone->head, head->p);
					VECCOPY(bone->tail, pt->p);

					Mat4MulVecfl(invmat, bone->head);
					Mat4MulVecfl(invmat, bone->tail);
					setBoneRollFromNormal(bone, pt->no, invmat, tmat);
				}
				
				new_parent = bone;
				bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				
				/* move to end of chain */
				while (bone->parent != NULL)
				{
					bone = bone->parent;
					bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				}

				if (parent != NULL)
				{
					bone->parent = parent;
					bone->flag |= BONE_CONNECTED;					
				}
				
				parent = new_parent;
				head_index = i;
				head = pt;
			}
		}
	}
}

void sk_convert(SK_Sketch *sketch)
{
	SK_Stroke *stk;
	
	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		if (stk->selected == 1)
		{
			if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_RETARGET)
			{
				sk_retargetStroke(stk);
			}
			else
			{
				sk_convertStroke(stk);
			}
			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
}
/******************* GESTURE *************************/


/* returns the number of self intersections */
int sk_getSelfIntersections(ListBase *list, SK_Stroke *gesture)
{
	int added = 0;
	int s_i;

	for (s_i = 0; s_i < gesture->nb_points - 1; s_i++)
	{
		float s_p1[3] = {0, 0, 0};
		float s_p2[3] = {0, 0, 0};
		int g_i;
		
		project_float(gesture->points[s_i].p, s_p1);
		project_float(gesture->points[s_i + 1].p, s_p2);

		/* start checking from second next, because two consecutive cannot intersect */
		for (g_i = s_i + 2; g_i < gesture->nb_points - 1; g_i++)
		{
			float g_p1[3] = {0, 0, 0};
			float g_p2[3] = {0, 0, 0};
			float vi[3];
			float lambda;
			
			project_float(gesture->points[g_i].p, g_p1);
			project_float(gesture->points[g_i + 1].p, g_p2);
			
			if (LineIntersectLineStrict(s_p1, s_p2, g_p1, g_p2, vi, &lambda))
			{
				SK_Intersection *isect = MEM_callocN(sizeof(SK_Intersection), "Intersection");
				
				isect->gesture_index = g_i;
				isect->before = s_i;
				isect->after = s_i + 1;
				isect->stroke = gesture;
				
				VecSubf(isect->p, gesture->points[s_i + 1].p, gesture->points[s_i].p);
				VecMulf(isect->p, lambda);
				VecAddf(isect->p, isect->p, gesture->points[s_i].p);
				
				BLI_addtail(list, isect);

				added++;
			}
		}
	}
	
	return added;
}

int cmpIntersections(void *i1, void *i2)
{
	SK_Intersection *isect1 = i1, *isect2 = i2;
	
	if (isect1->stroke == isect2->stroke)
	{
		if (isect1->before < isect2->before)
		{
			return -1;
		}
		else if (isect1->before > isect2->before)
		{
			return 1;
		}
		else
		{
			if (isect1->lambda < isect2->lambda)
			{
				return -1;
			}
			else if (isect1->lambda > isect2->lambda)
			{
				return 1;
			}
		}
	}
	
	return 0;
}


/* returns the maximum number of intersections per stroke */
int sk_getIntersections(ListBase *list, SK_Sketch *sketch, SK_Stroke *gesture)
{
	SK_Stroke *stk;
	int added = 0;

	for (stk = sketch->strokes.first; stk; stk = stk->next)
	{
		int s_added = 0;
		int s_i;
		
		for (s_i = 0; s_i < stk->nb_points - 1; s_i++)
		{
			float s_p1[3] = {0, 0, 0};
			float s_p2[3] = {0, 0, 0};
			int g_i;
			
			project_float(stk->points[s_i].p, s_p1);
			project_float(stk->points[s_i + 1].p, s_p2);

			for (g_i = 0; g_i < gesture->nb_points - 1; g_i++)
			{
				float g_p1[3] = {0, 0, 0};
				float g_p2[3] = {0, 0, 0};
				float vi[3];
				float lambda;
				
				project_float(gesture->points[g_i].p, g_p1);
				project_float(gesture->points[g_i + 1].p, g_p2);
				
				if (LineIntersectLineStrict(s_p1, s_p2, g_p1, g_p2, vi, &lambda))
				{
					SK_Intersection *isect = MEM_callocN(sizeof(SK_Intersection), "Intersection");
					float ray_start[3], ray_end[3];
					short mval[2];
					
					isect->gesture_index = g_i;
					isect->before = s_i;
					isect->after = s_i + 1;
					isect->stroke = stk;
					isect->lambda = lambda;
					
					mval[0] = (short)(vi[0]);
					mval[1] = (short)(vi[1]);
					viewline(mval, ray_start, ray_end);
					
					LineIntersectLine(	stk->points[s_i].p,
										stk->points[s_i + 1].p,
										ray_start,
										ray_end,
										isect->p,
										vi);
					
					BLI_addtail(list, isect);

					s_added++;
				}
			}
		}
		
		added = MAX2(s_added, added);
	}
	
	BLI_sortlist(list, cmpIntersections);
	
	return added;
}

int sk_getSegments(SK_Stroke *segments, SK_Stroke *gesture)
{
	SK_StrokeIterator sk_iter;
	BArcIterator *iter = (BArcIterator*)&sk_iter;
	
	float CORRELATION_THRESHOLD = 0.99f;
	float *vec;
	int i, j;
	
	sk_appendStrokePoint(segments, &gesture->points[0]);
	vec = segments->points[segments->nb_points - 1].p;

	initStrokeIterator(iter, gesture, 0, gesture->nb_points - 1);

	for (i = 1, j = 0; i < gesture->nb_points; i++)
	{ 
		float n[3];
		
		/* Calculate normal */
		VecSubf(n, gesture->points[i].p, vec);
		
		if (calcArcCorrelation(iter, j, i, vec, n) < CORRELATION_THRESHOLD)
		{
			j = i - 1;
			sk_appendStrokePoint(segments, &gesture->points[j]);
			vec = segments->points[segments->nb_points - 1].p;
			segments->points[segments->nb_points - 1].type = PT_EXACT;
		}
	}

	sk_appendStrokePoint(segments, &gesture->points[gesture->nb_points - 1]);
	
	return segments->nb_points - 1;
}

int sk_detectCutGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments == 1 && gest->nb_intersections == 1)
	{
		return 1;
	}

	return 0;
}

void sk_applyCutGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;
	
	for (isect = gest->intersections.first; isect; isect = isect->next)
	{
		SK_Point pt;
		
		pt.type = PT_EXACT;
		pt.mode = PT_PROJECT; /* take mode from neighbouring points */
		VECCOPY(pt.p, isect->p);
		
		sk_insertStrokePoint(isect->stroke, &pt, isect->after);
	}
}

int sk_detectTrimGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments == 2 && gest->nb_intersections == 1 && gest->nb_self_intersections == 0)
	{
		float s1[3], s2[3];
		float angle;
		
		VecSubf(s1, gest->segments->points[1].p, gest->segments->points[0].p);
		VecSubf(s2, gest->segments->points[2].p, gest->segments->points[1].p);
		
		angle = VecAngle2(s1, s2);
	
		if (angle > 60 && angle < 120)
		{
			return 1;
		}
	}

	return 0;
}

void sk_applyTrimGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;
	float trim_dir[3];
	
	VecSubf(trim_dir, gest->segments->points[2].p, gest->segments->points[1].p);
	
	for (isect = gest->intersections.first; isect; isect = isect->next)
	{
		SK_Point pt;
		float stroke_dir[3];
		
		pt.type = PT_EXACT;
		pt.mode = PT_PROJECT; /* take mode from neighbouring points */
		VECCOPY(pt.p, isect->p);
		
		VecSubf(stroke_dir, isect->stroke->points[isect->after].p, isect->stroke->points[isect->before].p);
		
		/* same direction, trim end */
		if (Inpf(stroke_dir, trim_dir) > 0)
		{
			sk_replaceStrokePoint(isect->stroke, &pt, isect->after);
			sk_trimStroke(isect->stroke, 0, isect->after);
		}
		/* else, trim start */
		else
		{
			sk_replaceStrokePoint(isect->stroke, &pt, isect->before);
			sk_trimStroke(isect->stroke, isect->before, isect->stroke->nb_points - 1);
		}
	
	}
}

int sk_detectCommandGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments > 2 && gest->nb_intersections == 2 && gest->nb_self_intersections == 1)
	{
		SK_Intersection *isect, *self_isect;
		
		/* get the the last intersection of the first pair */
		for( isect = gest->intersections.first; isect; isect = isect->next )
		{
			if (isect->stroke == isect->next->stroke)
			{
				isect = isect->next;
				break;
			}
		}
		
		self_isect = gest->self_intersections.first;
		
		if (isect && isect->gesture_index < self_isect->gesture_index)
		{
			return 1;
		}
	}
	
	return 0;
}

void sk_applyCommandGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;
	int command;
	
	command = pupmenu("Action %t|Flatten %x1|Straighten %x2|Polygonize %x3");
	if(command < 1) return;

	for (isect = gest->intersections.first; isect; isect = isect->next)
	{
		SK_Intersection *i2;
		
		i2 = isect->next;
		
		if (i2 && i2->stroke == isect->stroke)
		{
			switch (command)
			{
				case 1:
					sk_flattenStroke(isect->stroke, isect->before, i2->after);
					break;
				case 2:
					sk_straightenStroke(isect->stroke, isect->before, i2->after, isect->p, i2->p);
					break;
				case 3:
					sk_polygonizeStroke(isect->stroke, isect->before, i2->after);
					break;
			}

			isect = i2;
		}
	}
}

int sk_detectDeleteGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments == 2 && gest->nb_intersections == 2)
	{
		float s1[3], s2[3];
		float angle;
		
		VecSubf(s1, gest->segments->points[1].p, gest->segments->points[0].p);
		VecSubf(s2, gest->segments->points[2].p, gest->segments->points[1].p);
		
		angle = VecAngle2(s1, s2);
		
		if (angle > 120)
		{
			return 1;
		}
	}
	
	return 0;
}

void sk_applyDeleteGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;
	
	for (isect = gest->intersections.first; isect; isect = isect->next)
	{
		/* only delete strokes that are crossed twice */
		if (isect->next && isect->next->stroke == isect->stroke)
		{
			isect = isect->next;
			
			sk_removeStroke(sketch, isect->stroke);
		}
	}
}

int sk_detectMergeGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments > 2 && gest->nb_intersections == 2)
	{
		short start_val[2], end_val[2];
		short dist;
		
		project_short_noclip(gest->stk->points[0].p, start_val);
		project_short_noclip(sk_lastStrokePoint(gest->stk)->p, end_val);
		
		dist = MAX2(ABS(start_val[0] - end_val[0]), ABS(start_val[1] - end_val[1]));
		
		/* if gesture is a circle */
		if ( dist <= 20 )
		{
			SK_Intersection *isect;
			
			/* check if it circled around an exact point */
			for (isect = gest->intersections.first; isect; isect = isect->next)
			{
				/* only delete strokes that are crossed twice */
				if (isect->next && isect->next->stroke == isect->stroke)
				{
					int start_index, end_index;
					int i;
					
					start_index = MIN2(isect->after, isect->next->after);
					end_index = MAX2(isect->before, isect->next->before);
	
					for (i = start_index; i <= end_index; i++)
					{
						if (isect->stroke->points[i].type == PT_EXACT)
						{
							return 1; /* at least one exact point found, stop detect here */
						}
					}
	
					/* skip next */				
					isect = isect->next;
				}
			}
		}
	}
				
	return 0;
}

void sk_applyMergeGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;
	
	/* check if it circled around an exact point */
	for (isect = gest->intersections.first; isect; isect = isect->next)
	{
		/* only merge strokes that are crossed twice */
		if (isect->next && isect->next->stroke == isect->stroke)
		{
			int start_index, end_index;
			int i;
			
			start_index = MIN2(isect->after, isect->next->after);
			end_index = MAX2(isect->before, isect->next->before);

			for (i = start_index; i <= end_index; i++)
			{
				/* if exact, switch to continuous */
				if (isect->stroke->points[i].type == PT_EXACT)
				{
					isect->stroke->points[i].type = PT_CONTINUOUS;
				}
			}

			/* skip next */				
			isect = isect->next;
		}
	}
}

int sk_detectReverseGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments > 2 && gest->nb_intersections == 2 && gest->nb_self_intersections == 0)
	{
		SK_Intersection *isect;
		
		/* check if it circled around an exact point */
		for (isect = gest->intersections.first; isect; isect = isect->next)
		{
			/* only delete strokes that are crossed twice */
			if (isect->next && isect->next->stroke == isect->stroke)
			{
				float start_v[3], end_v[3];
				float angle;
				
				if (isect->gesture_index < isect->next->gesture_index)
				{
					VecSubf(start_v, isect->p, gest->stk->points[0].p);
					VecSubf(end_v, sk_lastStrokePoint(gest->stk)->p, isect->next->p);
				}
				else
				{
					VecSubf(start_v, isect->next->p, gest->stk->points[0].p);
					VecSubf(end_v, sk_lastStrokePoint(gest->stk)->p, isect->p);
				}
				
				angle = VecAngle2(start_v, end_v);
				
				if (angle > 120)
				{
					return 1;
				}
	
				/* skip next */				
				isect = isect->next;
			}
		}
	}
		
	return 0;
}

void sk_applyReverseGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;
	
	for (isect = gest->intersections.first; isect; isect = isect->next)
	{
		/* only reverse strokes that are crossed twice */
		if (isect->next && isect->next->stroke == isect->stroke)
		{
			sk_reverseStroke(isect->stroke);

			/* skip next */				
			isect = isect->next;
		}
	}
}

int sk_detectConvertGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	if (gest->nb_segments == 3 && gest->nb_self_intersections == 1)
	{
		return 1;
	}
	return 0;
}

void sk_applyConvertGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	sk_convert(sketch);
}

static void sk_initGesture(SK_Gesture *gest, SK_Sketch *sketch)
{
	gest->intersections.first = gest->intersections.last = NULL;
	gest->self_intersections.first = gest->self_intersections.last = NULL;
	
	gest->segments = sk_createStroke();
	gest->stk = sketch->gesture;

	gest->nb_self_intersections = sk_getSelfIntersections(&gest->self_intersections, gest->stk);
	gest->nb_intersections = sk_getIntersections(&gest->intersections, sketch, gest->stk);
	gest->nb_segments = sk_getSegments(gest->segments, gest->stk);
}

static void sk_freeGesture(SK_Gesture *gest)
{
	sk_freeStroke(gest->segments);
	BLI_freelistN(&gest->intersections);
	BLI_freelistN(&gest->self_intersections);
}

void sk_applyGesture(SK_Sketch *sketch)
{
	SK_Gesture gest;
	SK_GestureAction *act;
	
	sk_initGesture(&gest, sketch);
	
	/* detect and apply */
	for (act = GESTURE_ACTIONS; act->apply != NULL; act++)
	{
		if (act->detect(&gest, sketch))
		{
			act->apply(&gest, sketch);
			break;
		}
	}
	
	sk_freeGesture(&gest);
}

/********************************************/

void sk_deleteSelectedStrokes(SK_Sketch *sketch)
{
	SK_Stroke *stk, *next;
	
	for (stk = sketch->strokes.first; stk; stk = next)
	{
		next = stk->next;
		
		if (stk->selected == 1)
		{
			sk_removeStroke(sketch, stk);
		}
	}
}

void sk_selectAllSketch(SK_Sketch *sketch, int mode)
{
	SK_Stroke *stk = NULL;
	
	if (mode == -1)
	{
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			stk->selected = 0;
		}
	}
	else if (mode == 0)
	{
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			stk->selected = 1;
		}
	}
	else if (mode == 1)
	{
		int selected = 1;
		
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			selected &= stk->selected;
		}
		
		selected ^= 1;

		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			stk->selected = selected;
		}
	}
}

void sk_selectStroke(SK_Sketch *sketch)
{
	unsigned int buffer[MAXPICKBUF];
	short hits, mval[2];

	persp(PERSP_VIEW);

	getmouseco_areawin(mval);
	hits = view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-5, mval[1]-5, mval[0]+5, mval[1]+5);
	if(hits==0)
		hits = view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-12, mval[1]-12, mval[0]+12, mval[1]+12);
		
	if (hits>0)
	{
		int besthitresult = -1;
			
		if(hits == 1) {
			besthitresult = buffer[3];
		}
		else {
			besthitresult = buffer[3];
			/* loop and get best hit */
		}
		
		if (besthitresult > 0)
		{
			SK_Stroke *selected_stk = BLI_findlink(&sketch->strokes, besthitresult - 1);
			
			if ((G.qual & LR_SHIFTKEY) == 0)
			{
				sk_selectAllSketch(sketch, -1);
				
				selected_stk->selected = 1;
			}
			else
			{
				selected_stk->selected ^= 1;
			}
			
			
		}
	}
}

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

void sk_drawSketch(SK_Sketch *sketch, int with_names)
{
	SK_Stroke *stk;
	
	glDisable(GL_DEPTH_TEST);

	glLineWidth(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
	
	if (with_names)
	{
		int id;
		for (id = 1, stk = sketch->strokes.first; stk; id++, stk = stk->next)
		{
			sk_drawStroke(stk, id, NULL, -1, -1);
		}
		
		glLoadName(-1);
	}
	else
	{
		float selected_rgb[3] = {1, 0, 0};
		float unselected_rgb[3] = {1, 0.5, 0};
		
		for (stk = sketch->strokes.first; stk; stk = stk->next)
		{
			int start = -1;
			int end = -1;
			
			if (sk_hasOverdraw(sketch, stk))
			{
				sk_adjustIndexes(sketch, &start, &end);
			}
			
			sk_drawStroke(stk, -1, (stk->selected==1?selected_rgb:unselected_rgb), start, end);
		
			if (stk->selected == 1)
			{
				sk_drawStrokeSubdivision(stk);
			}
		}
	
		/* only draw gesture in active area */
		if (sketch->gesture != NULL && area_is_active_area(G.vd->area))
		{
			float gesture_rgb[3] = {0, 0.5, 1};
			sk_drawStroke(sketch->gesture, -1, gesture_rgb, -1, -1);
		}
		
		if (sketch->active_stroke != NULL)
		{
			SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);
			
			if (G.scene->toolsettings->bone_sketching & BONE_SKETCHING_QUICK)
			{
				sk_drawStrokeSubdivision(sketch->active_stroke);
			}
			
			if (last != NULL)
			{
				/* update point if in active area */
				if (area_is_active_area(G.vd->area))
				{
					SK_DrawData dd;
					
					sk_initDrawData(&dd);
					sk_getStrokePoint(&sketch->next_point, sketch, sketch->active_stroke, &dd, G.qual);
				}
				
				glEnable(GL_LINE_STIPPLE);
				glColor3fv(selected_rgb);
				glBegin(GL_LINE_STRIP);
				
					glVertex3fv(last->p);
					glVertex3fv(sketch->next_point.p);
				
				glEnd();
				
				glDisable(GL_LINE_STIPPLE);
	
				switch (sketch->next_point.mode)
				{
					case PT_SNAP:
						glColor3f(0, 1, 0);
						break;
					case PT_PROJECT:
						glColor3f(0, 0, 0);
						break;
				}
				
				glBegin(GL_POINTS);
				
					glVertex3fv(sketch->next_point.p);
				
				glEnd();
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
			sk_selectAllSketch(sketch, -1);
			
			sketch->active_stroke->selected = 1;
		}

		sk_initDrawData(&dd);
		
		/* paint loop */
		do {
			/* get current user input */
			getmouseco_areawin(dd.mval);
			
			/* only add current point to buffer if mouse moved (otherwise wait until it does) */
			if (sk_stroke_filtermval(&dd)) {

				sk_addStrokePoint(sketch, sketch->active_stroke, &dd, G.qual);
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
		} while (get_mbut() & L_MOUSE);
		
		sk_endContinuousStroke(sketch->active_stroke);
		sk_filterLastContinuousStroke(sketch->active_stroke);
		sk_updateNextPoint(sketch);
	}
	else if (mbut == RIGHTMOUSE)
	{
		if (sketch->active_stroke != NULL)
		{
			SK_Stroke *stk = sketch->active_stroke;
			
			sk_endStroke(sketch);
			
			if (G.scene->toolsettings->bone_sketching & BONE_SKETCHING_QUICK)
			{
				if (G.scene->toolsettings->bone_sketching_convert == SK_CONVERT_RETARGET)
				{
					sk_retargetStroke(stk);
				}
				else
				{
					sk_convertStroke(stk);
				}
				BIF_undo_push("Convert Sketch");
				sk_removeStroke(sketch, stk);
				allqueue(REDRAWBUTSEDIT, 0);
			}
			
			allqueue(REDRAWVIEW3D, 0);
		}
		/* no gestures in quick mode */
		else if (G.scene->toolsettings->bone_sketching & BONE_SKETCHING_QUICK)
		{
			retval = 0; /* return zero for default click behavior */
		}
		else
		{
			SK_DrawData dd;
			sketch->gesture = sk_createStroke();
	
			sk_initDrawData(&dd);
			
			/* paint loop */
			do {
				/* get current user input */
				getmouseco_areawin(dd.mval);
				
				/* only add current point to buffer if mouse moved (otherwise wait until it does) */
				if (sk_stroke_filtermval(&dd)) {

					sk_addStrokeDrawPoint(sketch, sketch->gesture, &dd);
					sk_updateDrawData(&dd);
					
					/* draw only if mouse has moved */
					if (sketch->gesture->nb_points > 1)
					{
						force_draw(0);
					}
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
			} while (get_mbut() & R_MOUSE);
			
			sk_endContinuousStroke(sketch->gesture);
			sk_filterLastContinuousStroke(sketch->gesture);
			sk_filterLastContinuousStroke(sketch->gesture);
			sk_filterLastContinuousStroke(sketch->gesture);
	
			if (sketch->gesture->nb_points == 1)		
			{
				sk_selectStroke(sketch);
			}
			else
			{
				/* apply gesture here */
				sk_applyGesture(sketch);
			}
	
			sk_freeStroke(sketch->gesture);
			sketch->gesture = NULL;
			
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	
	return retval;
}

void BDR_drawSketchNames()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_drawSketch(GLOBAL_sketch, 1);
		}
	}
}

void BDR_drawSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_drawSketch(GLOBAL_sketch, 0);
		}
	}
}

void BIF_endStrokeSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_endStroke(GLOBAL_sketch);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

void BIF_cancelStrokeSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_cancelStroke(GLOBAL_sketch);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

void BIF_deleteSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_deleteSelectedStrokes(GLOBAL_sketch);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

void BIF_convertSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_convert(GLOBAL_sketch);
			BIF_undo_push("Convert Sketch");
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
}

int BIF_paintSketch(short mbut)
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch == NULL)
		{
			GLOBAL_sketch = sk_createSketch();
		}
		
		return sk_paint(GLOBAL_sketch, mbut);
	}
	else
	{
		return 0;
	}
}
	

void BDR_queueDrawSketch()
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_queueRedrawSketch(GLOBAL_sketch);
		}
	}
}

void BIF_selectAllSketch(int mode)
{
	if (BIF_validSketchMode())
	{
		if (GLOBAL_sketch != NULL)
		{
			sk_selectAllSketch(GLOBAL_sketch, mode);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

int BIF_validSketchMode()
{
	if (G.obedit && 
		G.obedit->type == OB_ARMATURE && 
		G.scene->toolsettings->bone_sketching & BONE_SKETCHING)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

int BIF_fullSketchMode()
{
	if (G.obedit && 
		G.obedit->type == OB_ARMATURE && 
		G.scene->toolsettings->bone_sketching & BONE_SKETCHING && 
		(G.scene->toolsettings->bone_sketching & BONE_SKETCHING_QUICK) == 0)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void BIF_freeSketch()
{
	if (GLOBAL_sketch != NULL)
	{
		sk_freeSketch(GLOBAL_sketch);
		GLOBAL_sketch = NULL;
	}
}
