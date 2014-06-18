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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/armature/editarmature_sketch.c
 *  \ingroup edarmature
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_armature_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_sketch.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "BIF_gl.h"
#include "ED_armature.h"
#include "armature_intern.h"
#include "BIF_retarget.h"
#include "BIF_generate.h"

#include "ED_transform.h"

#include "WM_api.h"
#include "WM_types.h"

typedef int (*GestureDetectFct)(bContext *, SK_Gesture *, SK_Sketch *);
typedef void (*GestureApplyFct)(bContext *, SK_Gesture *, SK_Sketch *);

typedef struct SK_GestureAction {
	char name[64];
	GestureDetectFct detect;
	GestureApplyFct apply;
} SK_GestureAction;

#if 0 /* UNUSED 2.5 */
static SK_Point boneSnap;
#endif

static int LAST_SNAP_POINT_VALID = 0;
static float LAST_SNAP_POINT[3];


typedef struct SK_StrokeIterator {
	HeadFct     head;
	TailFct     tail;
	PeekFct     peek;
	NextFct     next;
	NextNFct    nextN;
	PreviousFct previous;
	StoppedFct  stopped;

	float *p, *no;
	float size;

	int length;
	int index;
	/*********************************/
	SK_Stroke *stroke;
	int start;
	int end;
	int stride;
} SK_StrokeIterator;

/******************** PROTOTYPES ******************************/

void initStrokeIterator(BArcIterator *iter, SK_Stroke *stk, int start, int end);

int sk_detectCutGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyCutGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectTrimGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyTrimGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectCommandGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyCommandGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectDeleteGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyDeleteGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectMergeGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyMergeGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectReverseGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyReverseGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
int sk_detectConvertGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);
void sk_applyConvertGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch);

SK_Sketch *contextSketch(const bContext *c, int create);
SK_Sketch *viewcontextSketch(ViewContext *vc, int create);

void sk_resetOverdraw(SK_Sketch *sketch);
int sk_hasOverdraw(SK_Sketch *sketch, SK_Stroke *stk);

/******************** GESTURE ACTIONS ******************************/

static SK_GestureAction GESTURE_ACTIONS[] = {
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

static char  *TEMPLATES_MENU = NULL;
static int TEMPLATES_CURRENT = 0;
static GHash *TEMPLATES_HASH = NULL;
static RigGraph *TEMPLATE_RIGG = NULL;

void BIF_makeListTemplates(const bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Base *base;
	int index = 0;

	if (TEMPLATES_HASH != NULL) {
		BLI_ghash_free(TEMPLATES_HASH, NULL, NULL);
	}

	TEMPLATES_HASH = BLI_ghash_int_new("makeListTemplates gh");
	TEMPLATES_CURRENT = 0;

	for (base = FIRSTBASE; base; base = base->next) {
		Object *ob = base->object;

		if (ob != obedit && ob->type == OB_ARMATURE) {
			index++;
			BLI_ghash_insert(TEMPLATES_HASH, SET_INT_IN_POINTER(index), ob);

			if (ob == ts->skgen_template) {
				TEMPLATES_CURRENT = index;
			}
		}
	}
}

#if 0  /* UNUSED */
const char *BIF_listTemplates(const bContext *UNUSED(C))
{
	GHashIterator ghi;
	const char *menu_header = IFACE_("Template %t|None %x0|");
	char *p;
	const size_t template_size = (BLI_ghash_size(TEMPLATES_HASH) * 32 + 30);

	if (TEMPLATES_MENU != NULL) {
		MEM_freeN(TEMPLATES_MENU);
	}

	TEMPLATES_MENU = MEM_callocN(sizeof(char) * template_size, "skeleton template menu");

	p = TEMPLATES_MENU;
	p += BLI_strncpy_rlen(p, menu_header, template_size);

	BLI_ghashIterator_init(&ghi, TEMPLATES_HASH);

	while (!BLI_ghashIterator_done(&ghi)) {
		Object *ob = BLI_ghashIterator_getValue(&ghi);
		int key = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(&ghi));

		p += sprintf(p, "|%s %%x%i", ob->id.name + 2, key);

		BLI_ghashIterator_step(&ghi);
	}

	return TEMPLATES_MENU;
}
#endif

int   BIF_currentTemplate(const bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);

	if (TEMPLATES_CURRENT == 0 && ts->skgen_template != NULL) {
		GHashIterator ghi;
		BLI_ghashIterator_init(&ghi, TEMPLATES_HASH);

		while (!BLI_ghashIterator_done(&ghi)) {
			Object *ob = BLI_ghashIterator_getValue(&ghi);
			int key = GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(&ghi));

			if (ob == ts->skgen_template) {
				TEMPLATES_CURRENT = key;
				break;
			}

			BLI_ghashIterator_step(&ghi);
		}
	}

	return TEMPLATES_CURRENT;
}

static RigGraph *sk_makeTemplateGraph(const bContext *C, Object *ob)
{
	Object *obedit = CTX_data_edit_object(C);
	if (ob == obedit) {
		return NULL;
	}

	if (ob != NULL) {
		if (TEMPLATE_RIGG && TEMPLATE_RIGG->ob != ob) {
			RIG_freeRigGraph((BGraph *)TEMPLATE_RIGG);
			TEMPLATE_RIGG = NULL;
		}

		if (TEMPLATE_RIGG == NULL) {
			bArmature *arm;

			arm = ob->data;

			TEMPLATE_RIGG = RIG_graphFromArmature(C, ob, arm);
		}
	}

	return TEMPLATE_RIGG;
}

int BIF_nbJointsTemplate(const bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	RigGraph *rg = sk_makeTemplateGraph(C, ts->skgen_template);

	if (rg) {
		return RIG_nbJoints(rg);
	}
	else {
		return -1;
	}
}

const char *BIF_nameBoneTemplate(const bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	SK_Sketch *stk = contextSketch(C, 1);
	RigGraph *rg;
	int index = 0;

	if (stk && stk->active_stroke != NULL) {
		index = stk->active_stroke->nb_points;
	}

	rg = sk_makeTemplateGraph(C, ts->skgen_template);

	if (rg == NULL) {
		return "";
	}

	return RIG_nameBone(rg, 0, index);
}

void  BIF_freeTemplates(bContext *UNUSED(C))
{
	if (TEMPLATES_MENU != NULL) {
		MEM_freeN(TEMPLATES_MENU);
		TEMPLATES_MENU = NULL;
	}

	if (TEMPLATES_HASH != NULL) {
		BLI_ghash_free(TEMPLATES_HASH, NULL, NULL);
		TEMPLATES_HASH = NULL;
	}

	if (TEMPLATE_RIGG != NULL) {
		RIG_freeRigGraph((BGraph *)TEMPLATE_RIGG);
		TEMPLATE_RIGG = NULL;
	}
}

void  BIF_setTemplate(bContext *C, int index)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	if (index > 0) {
		ts->skgen_template = BLI_ghash_lookup(TEMPLATES_HASH, SET_INT_IN_POINTER(index));
	}
	else {
		ts->skgen_template = NULL;

		if (TEMPLATE_RIGG != NULL) {
			RIG_freeRigGraph((BGraph *)TEMPLATE_RIGG);
		}
		TEMPLATE_RIGG = NULL;
	}
}

/*********************** CONVERSION ***************************/

static void sk_autoname(bContext *C, ReebArc *arc)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	if (ts->skgen_retarget_options & SK_RETARGET_AUTONAME) {
		if (arc == NULL) {
			char *num = ts->skgen_num_string;
			int i = atoi(num);
			i++;
			BLI_snprintf(num, 8, "%i", i);
		}
		else {
			char *side = ts->skgen_side_string;
			int valid = 0;
			int caps = 0;

			if (side[0] == '\0') {
				valid = 1;
			}
			else if (strcmp(side, "R") == 0 || strcmp(side, "L") == 0) {
				valid = 1;
				caps = 1;
			}
			else if (strcmp(side, "r") == 0 || strcmp(side, "l") == 0) {
				valid = 1;
				caps = 0;
			}

			if (valid) {
				if (arc->head->p[0] < 0) {
					BLI_snprintf(side, 8, caps ? "R" : "r");
				}
				else {
					BLI_snprintf(side, 8, caps ? "L" : "l");
				}
			}
		}
	}
}

static ReebNode *sk_pointToNode(SK_Point *pt, float imat[4][4], float tmat[3][3])
{
	ReebNode *node;

	node = MEM_callocN(sizeof(ReebNode), "reeb node");
	copy_v3_v3(node->p, pt->p);
	mul_m4_v3(imat, node->p);

	copy_v3_v3(node->no, pt->no);
	mul_m3_v3(tmat, node->no);

	return node;
}

static ReebArc *sk_strokeToArc(SK_Stroke *stk, float imat[4][4], float tmat[3][3])
{
	ReebArc *arc;
	int i;

	arc = MEM_callocN(sizeof(ReebArc), "reeb arc");
	arc->head = sk_pointToNode(stk->points, imat, tmat);
	arc->tail = sk_pointToNode(sk_lastStrokePoint(stk), imat, tmat);

	arc->bcount = stk->nb_points - 2; /* first and last are nodes, don't count */
	arc->buckets = MEM_callocN(sizeof(EmbedBucket) * arc->bcount, "Buckets");

	for (i = 0; i < arc->bcount; i++) {
		copy_v3_v3(arc->buckets[i].p, stk->points[i + 1].p);
		mul_m4_v3(imat, arc->buckets[i].p);

		copy_v3_v3(arc->buckets[i].no, stk->points[i + 1].no);
		mul_m3_v3(tmat, arc->buckets[i].no);
	}

	return arc;
}

static void sk_retargetStroke(bContext *C, SK_Stroke *stk)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *obedit = CTX_data_edit_object(C);
	float imat[4][4];
	float tmat[3][3];
	ReebArc *arc;
	RigGraph *rg;

	invert_m4_m4(imat, obedit->obmat);

	copy_m3_m4(tmat, obedit->obmat);
	transpose_m3(tmat);

	arc = sk_strokeToArc(stk, imat, tmat);

	sk_autoname(C, arc);

	rg = sk_makeTemplateGraph(C, ts->skgen_template);

	BIF_retargetArc(C, arc, rg);

	sk_autoname(C, NULL);

	MEM_freeN(arc->head);
	MEM_freeN(arc->tail);
	REEB_freeArc((BArc *)arc);
}

/**************************************************************/

static void sk_cancelStroke(SK_Sketch *sketch)
{
	if (sketch->active_stroke != NULL) {
		sk_resetOverdraw(sketch);
		sk_removeStroke(sketch, sketch->active_stroke);
	}
}


static float sk_clampPointSize(SK_Point *pt, float size)
{
	return max_ff(size * pt->size, size / 2);
}

static void sk_drawPoint(GLUquadric *quad, SK_Point *pt, float size)
{
	glTranslatef(pt->p[0], pt->p[1], pt->p[2]);
	gluSphere(quad, sk_clampPointSize(pt, size), 8, 8);
}

static void sk_drawEdge(GLUquadric *quad, SK_Point *pt0, SK_Point *pt1, float size)
{
	float vec1[3], vec2[3] = {0, 0, 1}, axis[3];
	float angle, length;

	sub_v3_v3v3(vec1, pt1->p, pt0->p);
	length = normalize_v3(vec1);
	cross_v3_v3v3(axis, vec2, vec1);

	if (is_zero_v3(axis)) {
		axis[1] = 1;
	}

	angle = angle_normalized_v3v3(vec2, vec1);

	glRotatef(angle * (float)(180.0 / M_PI) + 180.0f, axis[0], axis[1], axis[2]);

	gluCylinder(quad, sk_clampPointSize(pt1, size), sk_clampPointSize(pt0, size), length, 8, 8);
}

static void sk_drawNormal(GLUquadric *quad, SK_Point *pt, float size, float height)
{
	float vec2[3] = {0, 0, 1}, axis[3];
	float angle;
	
	glPushMatrix();

	cross_v3_v3v3(axis, vec2, pt->no);

	if (is_zero_v3(axis)) {
		axis[1] = 1;
	}

	angle = angle_normalized_v3v3(vec2, pt->no);

	glRotatef(angle * (float)(180.0 / M_PI), axis[0], axis[1], axis[2]);

	glColor3f(0, 1, 1);
	gluCylinder(quad, sk_clampPointSize(pt, size), 0, sk_clampPointSize(pt, height), 10, 2);

	glPopMatrix();
}

static void sk_drawStroke(SK_Stroke *stk, int id, float color[3], int start, int end)
{
	float rgb[3];
	int i;
	GLUquadric *quad = gluNewQuadric();
	gluQuadricNormals(quad, GLU_SMOOTH);

	if (id != -1) {
		glLoadName(id);

		for (i = 0; i < stk->nb_points; i++) {
			glPushMatrix();

			sk_drawPoint(quad, stk->points + i, 0.1);

			if (i > 0) {
				sk_drawEdge(quad, stk->points + i - 1, stk->points + i, 0.1);
			}

			glPopMatrix();
		}

	}
	else {
		float d_rgb[3] = {1, 1, 1};

		copy_v3_v3(rgb, color);
		sub_v3_v3(d_rgb, rgb);
		mul_v3_fl(d_rgb, 1.0f / (float)stk->nb_points);

		for (i = 0; i < stk->nb_points; i++) {
			SK_Point *pt = stk->points + i;

			glPushMatrix();

			if (pt->type == PT_EXACT) {
				glColor3f(0, 0, 0);
				sk_drawPoint(quad, pt, 0.15);
				sk_drawNormal(quad, pt, 0.05, 0.9);
			}

			if (i >= start && i <= end) {
				glColor3f(0.3, 0.3, 0.3);
			}
			else {
				glColor3fv(rgb);
			}

			if (pt->type != PT_EXACT) {

				sk_drawPoint(quad, pt, 0.1);
			}

			if (i > 0) {
				sk_drawEdge(quad, pt - 1, pt, 0.1);
			}

			glPopMatrix();

			add_v3_v3(rgb, d_rgb);
		}
	}

	gluDeleteQuadric(quad);
}

static void drawSubdividedStrokeBy(ToolSettings *toolsettings, BArcIterator *iter, NextSubdivisionFunc next_subdividion)
{
	SK_Stroke *stk = ((SK_StrokeIterator *)iter)->stroke;
	float head[3], tail[3];
	int bone_start = 0;
	int end = iter->length;
	int index;
	GLUquadric *quad = gluNewQuadric();
	gluQuadricNormals(quad, GLU_SMOOTH);

	iter->head(iter);
	copy_v3_v3(head, iter->p);

	index = next_subdividion(toolsettings, iter, bone_start, end, head, tail);
	while (index != -1) {
		SK_Point *pt = stk->points + index;

		glPushMatrix();

		glColor3f(0, 1, 0);
		sk_drawPoint(quad, pt, 0.15);

		sk_drawNormal(quad, pt, 0.05, 0.9);

		glPopMatrix();

		copy_v3_v3(head, tail);
		bone_start = index; // start next bone from current index

		index = next_subdividion(toolsettings, iter, bone_start, end, head, tail);
	}

	gluDeleteQuadric(quad);
}

static void sk_drawStrokeSubdivision(ToolSettings *toolsettings, SK_Stroke *stk)
{
	int head_index = -1;
	int i;

	if (toolsettings->bone_sketching_convert == SK_CONVERT_RETARGET) {
		return;
	}


	for (i = 0; i < stk->nb_points; i++) {
		SK_Point *pt = stk->points + i;

		if (pt->type == PT_EXACT || i == stk->nb_points - 1) /* stop on exact or on last point */ {
			if (head_index == -1) {
				head_index = i;
			}
			else {
				if (i - head_index > 1) {
					SK_StrokeIterator sk_iter;
					BArcIterator *iter = (BArcIterator *)&sk_iter;

					initStrokeIterator(iter, stk, head_index, i);

					if (toolsettings->bone_sketching_convert == SK_CONVERT_CUT_ADAPTATIVE) {
						drawSubdividedStrokeBy(toolsettings, iter, nextAdaptativeSubdivision);
					}
					else if (toolsettings->bone_sketching_convert == SK_CONVERT_CUT_LENGTH) {
						drawSubdividedStrokeBy(toolsettings, iter, nextLengthSubdivision);
					}
					else if (toolsettings->bone_sketching_convert == SK_CONVERT_CUT_FIXED) {
						drawSubdividedStrokeBy(toolsettings, iter, nextFixedSubdivision);
					}

				}

				head_index = i;
			}
		}
	}
}

static SK_Point *sk_snapPointStroke(bContext *C, SK_Stroke *stk, int mval[2], float *r_dist_px, int *index, int all_pts)
{
	ARegion *ar = CTX_wm_region(C);
	SK_Point *pt = NULL;
	int i;

	for (i = 0; i < stk->nb_points; i++) {
		if (all_pts || stk->points[i].type == PT_EXACT) {
			short pval[2];
			int pdist;

			if (ED_view3d_project_short_global(ar, stk->points[i].p, pval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {

				pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);

				if (pdist < *r_dist_px) {
					*r_dist_px = pdist;
					pt = stk->points + i;

					if (index != NULL) {
						*index = i;
					}
				}
			}
		}
	}

	return pt;
}

#if 0 /* UNUSED 2.5 */
static SK_Point *sk_snapPointArmature(bContext *C, Object *ob, ListBase *ebones, int mval[2], int *dist)
{
	ARegion *ar = CTX_wm_region(C);
	SK_Point *pt = NULL;
	EditBone *bone;

	for (bone = ebones->first; bone; bone = bone->next)
	{
		float vec[3];
		short pval[2];
		int pdist;

		if ((bone->flag & BONE_CONNECTED) == 0)
		{
			copy_v3_v3(vec, bone->head);
			mul_m4_v3(ob->obmat, vec);
			if (ED_view3d_project_short_noclip(ar, vec, pval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {

				pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);

				if (pdist < *dist)
				{
					*dist = pdist;
					pt = &boneSnap;
					copy_v3_v3(pt->p, vec);
					pt->type = PT_EXACT;
				}
			}
		}


		copy_v3_v3(vec, bone->tail);
		mul_m4_v3(ob->obmat, vec);
		if (ED_view3d_project_short_noclip(ar, vec, pval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {

			pdist = ABS(pval[0] - mval[0]) + ABS(pval[1] - mval[1]);

			if (pdist < *dist)
			{
				*dist = pdist;
				pt = &boneSnap;
				copy_v3_v3(pt->p, vec);
				pt->type = PT_EXACT;
			}
		}
	}

	return pt;
}
#endif

void sk_resetOverdraw(SK_Sketch *sketch)
{
	sketch->over.target = NULL;
	sketch->over.start = -1;
	sketch->over.end = -1;
	sketch->over.count = 0;
}

int sk_hasOverdraw(SK_Sketch *sketch, SK_Stroke *stk)
{
	return sketch->over.target &&
	       sketch->over.count >= SK_OVERDRAW_LIMIT &&
	       (sketch->over.target == stk || stk == NULL) &&
	       (sketch->over.start != -1 || sketch->over.end != -1);
}

static void sk_updateOverdraw(bContext *C, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	if (sketch->over.target == NULL) {
		SK_Stroke *target;
		int closest_index = -1;
		float dist_px = SNAP_MIN_DISTANCE * 2;

		for (target = sketch->strokes.first; target; target = target->next) {
			if (target != stk) {
				int index;

				SK_Point *spt = sk_snapPointStroke(C, target, dd->mval, &dist_px, &index, 1);

				if (spt != NULL) {
					sketch->over.target = target;
					closest_index = index;
				}
			}
		}

		if (sketch->over.target != NULL) {
			if (closest_index > -1) {
				if (sk_lastStrokePoint(stk)->type == PT_EXACT) {
					sketch->over.count = SK_OVERDRAW_LIMIT;
				}
				else {
					sketch->over.count++;
				}
			}

			if (stk->nb_points == 1) {
				sketch->over.start = closest_index;
			}
			else {
				sketch->over.end = closest_index;
			}
		}
	}
	else if (sketch->over.target != NULL) {
		SK_Point *closest_pt = NULL;
		float dist_px = SNAP_MIN_DISTANCE * 2;
		int index;

		closest_pt = sk_snapPointStroke(C, sketch->over.target, dd->mval, &dist_px, &index, 1);

		if (closest_pt != NULL) {
			if (sk_lastStrokePoint(stk)->type == PT_EXACT) {
				sketch->over.count = SK_OVERDRAW_LIMIT;
			}
			else {
				sketch->over.count++;
			}

			sketch->over.end = index;
		}
		else {
			sketch->over.end = -1;
		}
	}
}

/* return 1 on reverse needed */
static int sk_adjustIndexes(SK_Sketch *sketch, int *start, int *end)
{
	int retval = 0;

	*start = sketch->over.start;
	*end = sketch->over.end;

	if (*start == -1) {
		*start = 0;
	}

	if (*end == -1) {
		*end = sketch->over.target->nb_points - 1;
	}

	if (*end < *start) {
		int tmp = *start;
		*start = *end;
		*end = tmp;
		retval = 1;
	}

	return retval;
}

static void sk_endOverdraw(SK_Sketch *sketch)
{
	SK_Stroke *stk = sketch->active_stroke;

	if (sk_hasOverdraw(sketch, NULL)) {
		int start;
		int end;

		if (sk_adjustIndexes(sketch, &start, &end)) {
			sk_reverseStroke(stk);
		}

		if (stk->nb_points > 1) {
			stk->points->type = sketch->over.target->points[start].type;
			sk_lastStrokePoint(stk)->type = sketch->over.target->points[end].type;
		}

		sk_insertStrokePoints(sketch->over.target, stk->points, stk->nb_points, start, end);

		sk_removeStroke(sketch, stk);

		sk_resetOverdraw(sketch);
	}
}


static void sk_startStroke(SK_Sketch *sketch)
{
	SK_Stroke *stk = sk_createStroke();

	BLI_addtail(&sketch->strokes, stk);
	sketch->active_stroke = stk;

	sk_resetOverdraw(sketch);
}

static void sk_endStroke(bContext *C, SK_Sketch *sketch)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	sk_shrinkStrokeBuffer(sketch->active_stroke);

	if (ts->bone_sketching & BONE_SKETCHING_ADJUST) {
		sk_endOverdraw(sketch);
	}

	sketch->active_stroke = NULL;
}

static void sk_updateDrawData(SK_DrawData *dd)
{
	dd->type = PT_CONTINUOUS;

	dd->previous_mval[0] = dd->mval[0];
	dd->previous_mval[1] = dd->mval[1];
}

static float sk_distanceDepth(bContext *C, float p1[3], float p2[3])
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	float vec[3];
	float distance;

	sub_v3_v3v3(vec, p1, p2);

	project_v3_v3v3(vec, vec, rv3d->viewinv[2]);

	distance = len_v3(vec);

	if (dot_v3v3(rv3d->viewinv[2], vec) > 0) {
		distance *= -1;
	}

	return distance;
}

static void sk_interpolateDepth(bContext *C, SK_Stroke *stk, int start, int end, float length, float distance)
{
	ARegion *ar = CTX_wm_region(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;

	float progress = 0;
	int i;

	progress = len_v3v3(stk->points[start].p, stk->points[start - 1].p);

	for (i = start; i <= end; i++) {
		float ray_start[3], ray_normal[3];
		float delta = len_v3v3(stk->points[i].p, stk->points[i + 1].p);
		float pval[2] = {0, 0};

		ED_view3d_project_float_global(ar, stk->points[i].p, pval, V3D_PROJ_TEST_NOP);
		ED_view3d_win_to_ray(ar, v3d, pval, ray_start, ray_normal, false);

		mul_v3_fl(ray_normal, distance * progress / length);
		add_v3_v3(stk->points[i].p, ray_normal);

		progress += delta;
	}
}

static void sk_projectDrawPoint(bContext *C, float vec[3], SK_Stroke *stk, SK_DrawData *dd)
{
	ARegion *ar = CTX_wm_region(C);
	/* copied from grease pencil, need fixing */
	SK_Point *last = sk_lastStrokePoint(stk);
	short cval[2];
	float fp[3] = {0, 0, 0};
	float dvec[3];
	float mval_f[2];
	float zfac;

	if (last != NULL) {
		copy_v3_v3(fp, last->p);
	}

	zfac = ED_view3d_calc_zfac(ar->regiondata, fp, NULL);

	if (ED_view3d_project_short_global(ar, fp, cval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
		VECSUB2D(mval_f, cval, dd->mval);
		ED_view3d_win_to_delta(ar, mval_f, dvec, zfac);
		sub_v3_v3v3(vec, fp, dvec);
	}
	else {
		zero_v3(vec);
	}
}

static int sk_getStrokeDrawPoint(bContext *C, SK_Point *pt, SK_Sketch *UNUSED(sketch), SK_Stroke *stk, SK_DrawData *dd)
{
	pt->type = dd->type;
	pt->mode = PT_PROJECT;
	sk_projectDrawPoint(C, pt->p, stk, dd);

	return 1;
}

static int sk_addStrokeDrawPoint(bContext *C, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	SK_Point pt;

	sk_initPoint(&pt, dd, rv3d->viewinv[2]);

	sk_getStrokeDrawPoint(C, &pt, sketch, stk, dd);

	sk_appendStrokePoint(stk, &pt);

	return 1;
}

static int sk_getStrokeSnapPoint(bContext *C, SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	int point_added = 0;

	if (ts->snap_mode == SCE_SNAP_MODE_VOLUME) {
		DepthPeel *p1, *p2;
		float *last_p = NULL;
		float dist = FLT_MAX;
		float p[3] = {0};
		float size = 0;
		float mvalf[2];

		BLI_freelistN(&sketch->depth_peels);
		BLI_listbase_clear(&sketch->depth_peels);

		mvalf[0] = dd->mval[0];
		mvalf[1] = dd->mval[1];
		peelObjectsContext(C, &sketch->depth_peels, mvalf, SNAP_ALL);

		if (stk->nb_points > 0 && stk->points[stk->nb_points - 1].type == PT_CONTINUOUS) {
			last_p = stk->points[stk->nb_points - 1].p;
		}
		else if (LAST_SNAP_POINT_VALID) {
			last_p = LAST_SNAP_POINT;
		}


		for (p1 = sketch->depth_peels.first; p1; p1 = p1->next) {
			if (p1->flag == 0) {
				float vec[3];
				float new_dist;
				float new_size = 0;

				p2 = NULL;
				p1->flag = 1;

				/* if peeling objects, take the first and last from each object */
				if (ts->snap_flag & SCE_SNAP_PEEL_OBJECT) {
					DepthPeel *peel;
					for (peel = p1->next; peel; peel = peel->next) {
						if (peel->ob == p1->ob) {
							peel->flag = 1;
							p2 = peel;
						}
					}
				}
				/* otherwise, pair first with second and so on */
				else {
					for (p2 = p1->next; p2 && p2->ob != p1->ob; p2 = p2->next) {
						/* nothing to do here */
					}
				}

				if (p2) {
					p2->flag = 1;

					add_v3_v3v3(vec, p1->p, p2->p);
					mul_v3_fl(vec, 0.5f);
					new_size = len_v3v3(p1->p, p2->p);
				}
				else {
					copy_v3_v3(vec, p1->p);
				}

				if (last_p == NULL) {
					copy_v3_v3(p, vec);
					size = new_size;
					dist = 0;
					break;
				}

				new_dist = len_v3v3(last_p, vec);

				if (new_dist < dist) {
					copy_v3_v3(p, vec);
					dist = new_dist;
					size = new_size;
				}
			}
		}

		if (dist != FLT_MAX) {
			pt->type = dd->type;
			pt->mode = PT_SNAP;
			pt->size = size / 2;
			copy_v3_v3(pt->p, p);

			point_added = 1;
		}

		//BLI_freelistN(&depth_peels);
	}
	else {
		SK_Stroke *snap_stk;
		float vec[3];
		float no[3];
		float mval[2];
		int found = 0;
		float dist_px = SNAP_MIN_DISTANCE; // Use a user defined value here

		/* snap to strokes */
		// if (ts->snap_mode == SCE_SNAP_MODE_VERTEX) /* snap all the time to strokes */
		for (snap_stk = sketch->strokes.first; snap_stk; snap_stk = snap_stk->next) {
			SK_Point *spt = NULL;
			if (snap_stk == stk) {
				spt = sk_snapPointStroke(C, snap_stk, dd->mval, &dist_px, NULL, 0);
			}
			else {
				spt = sk_snapPointStroke(C, snap_stk, dd->mval, &dist_px, NULL, 1);
			}

			if (spt != NULL) {
				copy_v3_v3(pt->p, spt->p);
				point_added = 1;
			}
		}
		
		mval[0] = dd->mval[0];
		mval[1] = dd->mval[1];

		/* try to snap to closer object */
		found = snapObjectsContext(C, mval, &dist_px, vec, no, SNAP_NOT_SELECTED);
		if (found == 1) {
			pt->type = dd->type;
			pt->mode = PT_SNAP;
			copy_v3_v3(pt->p, vec);

			point_added = 1;
		}
	}

	return point_added;
}

static int sk_addStrokeSnapPoint(bContext *C, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd)
{
	int point_added;
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;
	SK_Point pt;

	sk_initPoint(&pt, dd, rv3d->viewinv[2]);

	point_added = sk_getStrokeSnapPoint(C, &pt, sketch, stk, dd);

	if (point_added) {
		float final_p[3];
		float length, distance;
		int total;
		int i;

		copy_v3_v3(final_p, pt.p);

		sk_projectDrawPoint(C, pt.p, stk, dd);
		sk_appendStrokePoint(stk, &pt);

		/* update all previous point to give smooth Z progresion */
		total = 0;
		length = 0;
		for (i = stk->nb_points - 2; i > 0; i--) {
			length += len_v3v3(stk->points[i].p, stk->points[i + 1].p);
			total++;
			if (stk->points[i].mode == PT_SNAP || stk->points[i].type == PT_EXACT) {
				break;
			}
		}

		if (total > 1) {
			distance = sk_distanceDepth(C, final_p, stk->points[i].p);

			sk_interpolateDepth(C, stk, i + 1, stk->nb_points - 2, length, distance);
		}

		copy_v3_v3(stk->points[stk->nb_points - 1].p, final_p);

		point_added = 1;
	}

	return point_added;
}

static void sk_addStrokePoint(bContext *C, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, const bool snap)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	int point_added = 0;

	if (snap) {
		point_added = sk_addStrokeSnapPoint(C, sketch, stk, dd);
	}

	if (point_added == 0) {
		point_added = sk_addStrokeDrawPoint(C, sketch, stk, dd);
	}

	if (stk == sketch->active_stroke && ts->bone_sketching & BONE_SKETCHING_ADJUST) {
		sk_updateOverdraw(C, sketch, stk, dd);
	}
}

static void sk_getStrokePoint(bContext *C, SK_Point *pt, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, const bool snap)
{
	int point_added = 0;

	if (snap) {
		point_added = sk_getStrokeSnapPoint(C, pt, sketch, stk, dd);
		LAST_SNAP_POINT_VALID = 1;
		copy_v3_v3(LAST_SNAP_POINT, pt->p);
	}
	else {
		LAST_SNAP_POINT_VALID = 0;
	}

	if (point_added == 0) {
		point_added = sk_getStrokeDrawPoint(C, pt, sketch, stk, dd);
	}
}

/********************************************/

static void *headPoint(void *arg);
static void *tailPoint(void *arg);
static void *nextPoint(void *arg);
static void *nextNPoint(void *arg, int n);
static void *peekPoint(void *arg, int n);
static void *previousPoint(void *arg);
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

static SK_Point *setIteratorValues(SK_StrokeIterator *iter, int index)
{
	SK_Point *pt = NULL;

	if (index >= 0 && index < iter->length) {
		pt = &(iter->stroke->points[iter->start + (iter->stride * index)]);
		iter->p = pt->p;
		iter->no = pt->no;
		iter->size = pt->size;
	}
	else {
		iter->p = NULL;
		iter->no = NULL;
		iter->size = 0;
	}

	return pt;
}

void initStrokeIterator(BArcIterator *arg, SK_Stroke *stk, int start, int end)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;

	initIteratorFct(iter);
	iter->stroke = stk;

	if (start < end) {
		iter->start = start + 1;
		iter->end = end - 1;
		iter->stride = 1;
	}
	else {
		iter->start = start - 1;
		iter->end = end + 1;
		iter->stride = -1;
	}

	iter->length = iter->stride * (iter->end - iter->start + 1);

	iter->index = -1;
}


static void *headPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;
	SK_Point *result = NULL;

	result = &(iter->stroke->points[iter->start - iter->stride]);
	iter->p = result->p;
	iter->no = result->no;
	iter->size = result->size;

	return result;
}

static void *tailPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;
	SK_Point *result = NULL;

	result = &(iter->stroke->points[iter->end + iter->stride]);
	iter->p = result->p;
	iter->no = result->no;
	iter->size = result->size;

	return result;
}

static void *nextPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;
	SK_Point *result = NULL;

	iter->index++;
	if (iter->index < iter->length) {
		result = setIteratorValues(iter, iter->index);
	}

	return result;
}

static void *nextNPoint(void *arg, int n)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;
	SK_Point *result = NULL;

	iter->index += n;

	/* check if passed end */
	if (iter->index < iter->length) {
		result = setIteratorValues(iter, iter->index);
	}

	return result;
}

static void *peekPoint(void *arg, int n)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;
	SK_Point *result = NULL;
	int index = iter->index + n;

	/* check if passed end */
	if (index < iter->length) {
		result = setIteratorValues(iter, index);
	}

	return result;
}

static void *previousPoint(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;
	SK_Point *result = NULL;

	if (iter->index > 0) {
		iter->index--;
		result = setIteratorValues(iter, iter->index);
	}

	return result;
}

static int iteratorStopped(void *arg)
{
	SK_StrokeIterator *iter = (SK_StrokeIterator *)arg;

	if (iter->index >= iter->length) {
		return 1;
	}
	else {
		return 0;
	}
}

static void sk_convertStroke(bContext *C, SK_Stroke *stk)
{
	Object *obedit = CTX_data_edit_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	bArmature *arm = obedit->data;
	SK_Point *head;
	EditBone *parent = NULL;
	float invmat[4][4]; /* move in caller function */
	float tmat[3][3];
	int head_index = 0;
	int i;

	head = NULL;

	invert_m4_m4(invmat, obedit->obmat);

	copy_m3_m4(tmat, obedit->obmat);
	transpose_m3(tmat);

	for (i = 0; i < stk->nb_points; i++) {
		SK_Point *pt = stk->points + i;

		if (pt->type == PT_EXACT) {
			if (head == NULL) {
				head_index = i;
				head = pt;
			}
			else {
				EditBone *bone = NULL;
				EditBone *new_parent;

				if (i - head_index > 1) {
					SK_StrokeIterator sk_iter;
					BArcIterator *iter = (BArcIterator *)&sk_iter;

					initStrokeIterator(iter, stk, head_index, i);

					if (ts->bone_sketching_convert == SK_CONVERT_CUT_ADAPTATIVE) {
						bone = subdivideArcBy(ts, arm, arm->edbo, iter, invmat, tmat, nextAdaptativeSubdivision);
					}
					else if (ts->bone_sketching_convert == SK_CONVERT_CUT_LENGTH) {
						bone = subdivideArcBy(ts, arm, arm->edbo, iter, invmat, tmat, nextLengthSubdivision);
					}
					else if (ts->bone_sketching_convert == SK_CONVERT_CUT_FIXED) {
						bone = subdivideArcBy(ts, arm, arm->edbo, iter, invmat, tmat, nextFixedSubdivision);
					}
				}

				if (bone == NULL) {
					bone = ED_armature_edit_bone_add(arm, "Bone");

					copy_v3_v3(bone->head, head->p);
					copy_v3_v3(bone->tail, pt->p);

					mul_m4_v3(invmat, bone->head);
					mul_m4_v3(invmat, bone->tail);
					setBoneRollFromNormal(bone, head->no, invmat, tmat);
				}

				new_parent = bone;
				bone->flag |= BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;

				/* move to end of chain */
				while (bone->parent != NULL) {
					bone = bone->parent;
					bone->flag |= BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
				}

				if (parent != NULL) {
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

static void sk_convert(bContext *C, SK_Sketch *sketch)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	SK_Stroke *stk;

	for (stk = sketch->strokes.first; stk; stk = stk->next) {
		if (stk->selected == 1) {
			if (ts->bone_sketching_convert == SK_CONVERT_RETARGET) {
				sk_retargetStroke(C, stk);
			}
			else {
				sk_convertStroke(C, stk);
			}
//			XXX
//			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
}
/******************* GESTURE *************************/


/* returns the number of self intersections */
static int sk_getSelfIntersections(bContext *C, ListBase *list, SK_Stroke *gesture)
{
	ARegion *ar = CTX_wm_region(C);
	int added = 0;
	int s_i;

	for (s_i = 0; s_i < gesture->nb_points - 1; s_i++) {
		float s_p1[3] = {0, 0, 0};
		float s_p2[3] = {0, 0, 0};
		int g_i;

		ED_view3d_project_float_global(ar, gesture->points[s_i].p, s_p1, V3D_PROJ_TEST_NOP);
		ED_view3d_project_float_global(ar, gesture->points[s_i + 1].p, s_p2, V3D_PROJ_TEST_NOP);

		/* start checking from second next, because two consecutive cannot intersect */
		for (g_i = s_i + 2; g_i < gesture->nb_points - 1; g_i++) {
			float g_p1[3] = {0, 0, 0};
			float g_p2[3] = {0, 0, 0};
			float vi[3];
			float lambda;

			ED_view3d_project_float_global(ar, gesture->points[g_i].p, g_p1, V3D_PROJ_TEST_NOP);
			ED_view3d_project_float_global(ar, gesture->points[g_i + 1].p, g_p2, V3D_PROJ_TEST_NOP);

			if (isect_line_line_strict_v3(s_p1, s_p2, g_p1, g_p2, vi, &lambda)) {
				SK_Intersection *isect = MEM_callocN(sizeof(SK_Intersection), "Intersection");

				isect->gesture_index = g_i;
				isect->before = s_i;
				isect->after = s_i + 1;
				isect->stroke = gesture;

				sub_v3_v3v3(isect->p, gesture->points[s_i + 1].p, gesture->points[s_i].p);
				mul_v3_fl(isect->p, lambda);
				add_v3_v3(isect->p, gesture->points[s_i].p);

				BLI_addtail(list, isect);

				added++;
			}
		}
	}

	return added;
}

static int cmpIntersections(void *i1, void *i2)
{
	SK_Intersection *isect1 = i1, *isect2 = i2;

	if (isect1->stroke == isect2->stroke) {
		if (isect1->before < isect2->before) {
			return -1;
		}
		else if (isect1->before > isect2->before) {
			return 1;
		}
		else {
			if (isect1->lambda < isect2->lambda) {
				return -1;
			}
			else if (isect1->lambda > isect2->lambda) {
				return 1;
			}
		}
	}

	return 0;
}


/* returns the maximum number of intersections per stroke */
static int sk_getIntersections(bContext *C, ListBase *list, SK_Sketch *sketch, SK_Stroke *gesture)
{
	ARegion *ar = CTX_wm_region(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	SK_Stroke *stk;
	int added = 0;

	for (stk = sketch->strokes.first; stk; stk = stk->next) {
		int s_added = 0;
		int s_i;

		for (s_i = 0; s_i < stk->nb_points - 1; s_i++) {
			float s_p1[3] = {0, 0, 0};
			float s_p2[3] = {0, 0, 0};
			int g_i;

			ED_view3d_project_float_global(ar, stk->points[s_i].p, s_p1, V3D_PROJ_TEST_NOP);
			ED_view3d_project_float_global(ar, stk->points[s_i + 1].p, s_p2, V3D_PROJ_TEST_NOP);

			for (g_i = 0; g_i < gesture->nb_points - 1; g_i++) {
				float g_p1[3] = {0, 0, 0};
				float g_p2[3] = {0, 0, 0};
				float vi[3];
				float lambda;

				ED_view3d_project_float_global(ar, gesture->points[g_i].p, g_p1, V3D_PROJ_TEST_NOP);
				ED_view3d_project_float_global(ar, gesture->points[g_i + 1].p, g_p2, V3D_PROJ_TEST_NOP);

				if (isect_line_line_strict_v3(s_p1, s_p2, g_p1, g_p2, vi, &lambda)) {
					SK_Intersection *isect = MEM_callocN(sizeof(SK_Intersection), "Intersection");
					float ray_start[3], ray_end[3];
					float mval[2];

					isect->gesture_index = g_i;
					isect->before = s_i;
					isect->after = s_i + 1;
					isect->stroke = stk;
					isect->lambda = lambda;

					mval[0] = vi[0];
					mval[1] = vi[1];
					ED_view3d_win_to_segment(ar, v3d, mval, ray_start, ray_end, true);

					isect_line_line_v3(stk->points[s_i].p,
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

static int sk_getSegments(SK_Stroke *segments, SK_Stroke *gesture)
{
	SK_StrokeIterator sk_iter;
	BArcIterator *iter = (BArcIterator *)&sk_iter;

	float CORRELATION_THRESHOLD = 0.99f;
	float *vec;
	int i, j;

	sk_appendStrokePoint(segments, &gesture->points[0]);
	vec = segments->points[segments->nb_points - 1].p;

	initStrokeIterator(iter, gesture, 0, gesture->nb_points - 1);

	for (i = 1, j = 0; i < gesture->nb_points; i++) {
		float n[3];

		/* Calculate normal */
		sub_v3_v3v3(n, gesture->points[i].p, vec);

		if (calcArcCorrelation(iter, j, i, vec, n) < CORRELATION_THRESHOLD) {
			j = i - 1;
			sk_appendStrokePoint(segments, &gesture->points[j]);
			vec = segments->points[segments->nb_points - 1].p;
			segments->points[segments->nb_points - 1].type = PT_EXACT;
		}
	}

	sk_appendStrokePoint(segments, &gesture->points[gesture->nb_points - 1]);

	return segments->nb_points - 1;
}

int sk_detectCutGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	if (gest->nb_segments == 1 && gest->nb_intersections == 1) {
		return 1;
	}

	return 0;
}

void sk_applyCutGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	SK_Intersection *isect;

	for (isect = gest->intersections.first; isect; isect = isect->next) {
		SK_Point pt;

		pt.type = PT_EXACT;
		pt.mode = PT_PROJECT; /* take mode from neighboring points */
		copy_v3_v3(pt.p, isect->p);
		copy_v3_v3(pt.no, isect->stroke->points[isect->before].no);

		sk_insertStrokePoint(isect->stroke, &pt, isect->after);
	}
}

int sk_detectTrimGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	if (gest->nb_segments == 2 && gest->nb_intersections == 1 && gest->nb_self_intersections == 0) {
		float s1[3], s2[3];
		float angle;

		sub_v3_v3v3(s1, gest->segments->points[1].p, gest->segments->points[0].p);
		sub_v3_v3v3(s2, gest->segments->points[2].p, gest->segments->points[1].p);

		angle = RAD2DEGF(angle_v2v2(s1, s2));

		if (angle > 60 && angle < 120) {
			return 1;
		}
	}

	return 0;
}

void sk_applyTrimGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	SK_Intersection *isect;
	float trim_dir[3];

	sub_v3_v3v3(trim_dir, gest->segments->points[2].p, gest->segments->points[1].p);

	for (isect = gest->intersections.first; isect; isect = isect->next) {
		SK_Point pt;
		float stroke_dir[3];

		pt.type = PT_EXACT;
		pt.mode = PT_PROJECT; /* take mode from neighboring points */
		copy_v3_v3(pt.p, isect->p);
		copy_v3_v3(pt.no, isect->stroke->points[isect->before].no);

		sub_v3_v3v3(stroke_dir, isect->stroke->points[isect->after].p, isect->stroke->points[isect->before].p);

		/* same direction, trim end */
		if (dot_v3v3(stroke_dir, trim_dir) > 0) {
			sk_replaceStrokePoint(isect->stroke, &pt, isect->after);
			sk_trimStroke(isect->stroke, 0, isect->after);
		}
		/* else, trim start */
		else {
			sk_replaceStrokePoint(isect->stroke, &pt, isect->before);
			sk_trimStroke(isect->stroke, isect->before, isect->stroke->nb_points - 1);
		}

	}
}

int sk_detectCommandGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	if (gest->nb_segments > 2 && gest->nb_intersections == 2 && gest->nb_self_intersections == 1) {
		SK_Intersection *isect, *self_isect;

		/* get the the last intersection of the first pair */
		for (isect = gest->intersections.first; isect; isect = isect->next) {
			if (isect->stroke == isect->next->stroke) {
				isect = isect->next;
				break;
			}
		}

		self_isect = gest->self_intersections.first;

		if (isect && isect->gesture_index < self_isect->gesture_index) {
			return 1;
		}
	}

	return 0;
}

void sk_applyCommandGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	SK_Intersection *isect;
	int command = 1;

/*	XXX */
/*	command = pupmenu("Action %t|Flatten %x1|Straighten %x2|Polygonize %x3"); */
	if (command < 1) return;

	for (isect = gest->intersections.first; isect; isect = isect->next) {
		SK_Intersection *i2;

		i2 = isect->next;

		if (i2 && i2->stroke == isect->stroke) {
			switch (command) {
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

int sk_detectDeleteGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	if (gest->nb_segments == 2 && gest->nb_intersections == 2) {
		float s1[3], s2[3];
		float angle;

		sub_v3_v3v3(s1, gest->segments->points[1].p, gest->segments->points[0].p);
		sub_v3_v3v3(s2, gest->segments->points[2].p, gest->segments->points[1].p);

		angle = RAD2DEGF(angle_v2v2(s1, s2));

		if (angle > 120) {
			return 1;
		}
	}

	return 0;
}

void sk_applyDeleteGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *sketch)
{
	SK_Intersection *isect;

	for (isect = gest->intersections.first; isect; isect = isect->next) {
		/* only delete strokes that are crossed twice */
		if (isect->next && isect->next->stroke == isect->stroke) {
			isect = isect->next;

			sk_removeStroke(sketch, isect->stroke);
		}
	}
}

int sk_detectMergeGesture(bContext *C, SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	ARegion *ar = CTX_wm_region(C);
	if (gest->nb_segments > 2 && gest->nb_intersections == 2) {
		short start_val[2], end_val[2];
		short dist;

		if ((ED_view3d_project_short_global(ar, gest->stk->points[0].p,           start_val, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) &&
		    (ED_view3d_project_short_global(ar, sk_lastStrokePoint(gest->stk)->p, end_val,   V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK))
		{

			dist = MAX2(ABS(start_val[0] - end_val[0]), ABS(start_val[1] - end_val[1]));

			/* if gesture is a circle */
			if (dist <= 20) {
				SK_Intersection *isect;

				/* check if it circled around an exact point */
				for (isect = gest->intersections.first; isect; isect = isect->next) {
					/* only delete strokes that are crossed twice */
					if (isect->next && isect->next->stroke == isect->stroke) {
						int start_index, end_index;
						int i;

						start_index = MIN2(isect->after, isect->next->after);
						end_index = MAX2(isect->before, isect->next->before);

						for (i = start_index; i <= end_index; i++) {
							if (isect->stroke->points[i].type == PT_EXACT) {
								return 1; /* at least one exact point found, stop detect here */
							}
						}

						/* skip next */
						isect = isect->next;
					}
				}
			}
		}
	}

	return 0;
}

void sk_applyMergeGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	SK_Intersection *isect;

	/* check if it circled around an exact point */
	for (isect = gest->intersections.first; isect; isect = isect->next) {
		/* only merge strokes that are crossed twice */
		if (isect->next && isect->next->stroke == isect->stroke) {
			int start_index, end_index;
			int i;

			start_index = MIN2(isect->after, isect->next->after);
			end_index = MAX2(isect->before, isect->next->before);

			for (i = start_index; i <= end_index; i++) {
				/* if exact, switch to continuous */
				if (isect->stroke->points[i].type == PT_EXACT) {
					isect->stroke->points[i].type = PT_CONTINUOUS;
				}
			}

			/* skip next */
			isect = isect->next;
		}
	}
}

int sk_detectReverseGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	if (gest->nb_segments > 2 && gest->nb_intersections == 2 && gest->nb_self_intersections == 0) {
		SK_Intersection *isect;

		/* check if it circled around an exact point */
		for (isect = gest->intersections.first; isect; isect = isect->next) {
			/* only delete strokes that are crossed twice */
			if (isect->next && isect->next->stroke == isect->stroke) {
				float start_v[3], end_v[3];
				float angle;

				if (isect->gesture_index < isect->next->gesture_index) {
					sub_v3_v3v3(start_v, isect->p, gest->stk->points[0].p);
					sub_v3_v3v3(end_v, sk_lastStrokePoint(gest->stk)->p, isect->next->p);
				}
				else {
					sub_v3_v3v3(start_v, isect->next->p, gest->stk->points[0].p);
					sub_v3_v3v3(end_v, sk_lastStrokePoint(gest->stk)->p, isect->p);
				}

				angle = RAD2DEGF(angle_v2v2(start_v, end_v));

				if (angle > 120) {
					return 1;
				}

				/* skip next */
				isect = isect->next;
			}
		}
	}

	return 0;
}

void sk_applyReverseGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	SK_Intersection *isect;

	for (isect = gest->intersections.first; isect; isect = isect->next) {
		/* only reverse strokes that are crossed twice */
		if (isect->next && isect->next->stroke == isect->stroke) {
			sk_reverseStroke(isect->stroke);

			/* skip next */
			isect = isect->next;
		}
	}
}

int sk_detectConvertGesture(bContext *UNUSED(C), SK_Gesture *gest, SK_Sketch *UNUSED(sketch))
{
	if (gest->nb_segments == 3 && gest->nb_self_intersections == 1) {
		return 1;
	}
	return 0;
}

void sk_applyConvertGesture(bContext *C, SK_Gesture *UNUSED(gest), SK_Sketch *sketch)
{
	sk_convert(C, sketch);
}

static void sk_initGesture(bContext *C, SK_Gesture *gest, SK_Sketch *sketch)
{
	BLI_listbase_clear(&gest->intersections);
	BLI_listbase_clear(&gest->self_intersections);

	gest->segments = sk_createStroke();
	gest->stk = sketch->gesture;

	gest->nb_self_intersections = sk_getSelfIntersections(C, &gest->self_intersections, gest->stk);
	gest->nb_intersections = sk_getIntersections(C, &gest->intersections, sketch, gest->stk);
	gest->nb_segments = sk_getSegments(gest->segments, gest->stk);
}

static void sk_freeGesture(SK_Gesture *gest)
{
	sk_freeStroke(gest->segments);
	BLI_freelistN(&gest->intersections);
	BLI_freelistN(&gest->self_intersections);
}

static void sk_applyGesture(bContext *C, SK_Sketch *sketch)
{
	SK_Gesture gest;
	SK_GestureAction *act;

	sk_initGesture(C, &gest, sketch);

	/* detect and apply */
	for (act = GESTURE_ACTIONS; act->apply != NULL; act++) {
		if (act->detect(C, &gest, sketch)) {
			act->apply(C, &gest, sketch);
			break;
		}
	}

	sk_freeGesture(&gest);
}

/********************************************/


static int sk_selectStroke(bContext *C, SK_Sketch *sketch, const int mval[2], int extend)
{
	ViewContext vc;
	rcti rect;
	unsigned int buffer[MAXPICKBUF];
	short hits;

	view3d_set_viewcontext(C, &vc);

	rect.xmin = mval[0] - 5;
	rect.xmax = mval[0] + 5;
	rect.ymin = mval[1] - 5;
	rect.ymax = mval[1] + 5;

	hits = view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect);

	if (hits > 0) {
		int besthitresult = -1;

		if (hits == 1) {
			besthitresult = buffer[3];
		}
		else {
			besthitresult = buffer[3];
			/* loop and get best hit */
		}

		if (besthitresult > 0) {
			SK_Stroke *selected_stk = BLI_findlink(&sketch->strokes, besthitresult - 1);

			if (extend == 0) {
				sk_selectAllSketch(sketch, -1);

				selected_stk->selected = 1;
			}
			else {
				selected_stk->selected ^= 1;
			}


		}
		return 1;
	}

	return 0;
}

#if 0 /* UNUSED 2.5 */
static void sk_queueRedrawSketch(SK_Sketch *sketch)
{
	if (sketch->active_stroke != NULL)
	{
		SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);

		if (last != NULL)
		{
//			XXX
//			allqueue(REDRAWVIEW3D, 0);
		}
	}
}
#endif

static void sk_drawSketch(Scene *scene, View3D *UNUSED(v3d), SK_Sketch *sketch, int with_names)
{
	ToolSettings *ts = scene->toolsettings;
	SK_Stroke *stk;

	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	if (with_names) {
		int id;
		for (id = 1, stk = sketch->strokes.first; stk; id++, stk = stk->next) {
			sk_drawStroke(stk, id, NULL, -1, -1);
		}

		glLoadName(-1);
	}
	else {
		float selected_rgb[3] = {1, 0, 0};
		float unselected_rgb[3] = {1, 0.5, 0};

		for (stk = sketch->strokes.first; stk; stk = stk->next) {
			int start = -1;
			int end = -1;

			if (sk_hasOverdraw(sketch, stk)) {
				sk_adjustIndexes(sketch, &start, &end);
			}

			sk_drawStroke(stk, -1, (stk->selected == 1 ? selected_rgb : unselected_rgb), start, end);

			if (stk->selected == 1) {
				sk_drawStrokeSubdivision(ts, stk);
			}
		}

		if (sketch->active_stroke != NULL) {
			SK_Point *last = sk_lastStrokePoint(sketch->active_stroke);

			if (ts->bone_sketching & BONE_SKETCHING_QUICK) {
				sk_drawStrokeSubdivision(ts, sketch->active_stroke);
			}

			if (last != NULL) {
				GLUquadric *quad = gluNewQuadric();
				gluQuadricNormals(quad, GLU_SMOOTH);

				glPushMatrix();

				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				switch (sketch->next_point.mode) {
					case PT_SNAP:
						glColor3f(0, 1, 0);
						break;
					case PT_PROJECT:
						glColor3f(0, 0, 0);
						break;
				}

				sk_drawPoint(quad, &sketch->next_point, 0.1);

				glColor4f(selected_rgb[0], selected_rgb[1], selected_rgb[2], 0.3);

				sk_drawEdge(quad, last, &sketch->next_point, 0.1);

				glDisable(GL_BLEND);

				glPopMatrix();

				gluDeleteQuadric(quad);
			}
		}
	}

#if 0
	if (BLI_listbase_is_empty(&sketch->depth_peels) == false) {
		float colors[8][3] = {
			{1, 0, 0},
			{0, 1, 0},
			{0, 0, 1},
			{1, 1, 0},
			{1, 0, 1},
			{0, 1, 1},
			{1, 1, 1},
			{0, 0, 0}
		};
		DepthPeel *p;
		GLUquadric *quad = gluNewQuadric();
		gluQuadricNormals(quad, GLU_SMOOTH);

		for (p = sketch->depth_peels.first; p; p = p->next)
		{
			int index = GET_INT_FROM_POINTER(p->ob);
			index = (index >> 5) & 7;

			glColor3fv(colors[index]);
			glPushMatrix();
			glTranslatef(p->p[0], p->p[1], p->p[2]);
			gluSphere(quad, 0.02, 8, 8);
			glPopMatrix();
		}

		gluDeleteQuadric(quad);
	}
#endif

	glDisable(GL_DEPTH_TEST);

	/* only draw gesture in active area */
	if (sketch->gesture != NULL /* && area_is_active_area(G.vd->area) */) {
		float gesture_rgb[3] = {0, 0.5, 1};
		sk_drawStroke(sketch->gesture, -1, gesture_rgb, -1, -1);
	}
}

static int sk_finish_stroke(bContext *C, SK_Sketch *sketch)
{
	ToolSettings *ts = CTX_data_tool_settings(C);

	if (sketch->active_stroke != NULL) {
		SK_Stroke *stk = sketch->active_stroke;

		sk_endStroke(C, sketch);

		if (ts->bone_sketching & BONE_SKETCHING_QUICK) {
			if (ts->bone_sketching_convert == SK_CONVERT_RETARGET) {
				sk_retargetStroke(C, stk);
			}
			else {
				sk_convertStroke(C, stk);
			}
//			XXX
//			BIF_undo_push("Convert Sketch");
			sk_removeStroke(sketch, stk);
//			XXX
//			allqueue(REDRAWBUTSEDIT, 0);
		}

//		XXX
//		allqueue(REDRAWVIEW3D, 0);
		return 1;
	}

	return 0;
}

static void sk_start_draw_stroke(SK_Sketch *sketch)
{
	if (sketch->active_stroke == NULL) {
		sk_startStroke(sketch);
		sk_selectAllSketch(sketch, -1);

		sketch->active_stroke->selected = 1;
	}
}

static void sk_start_draw_gesture(SK_Sketch *sketch)
{
	sketch->gesture = sk_createStroke();
}

static int sk_draw_stroke(bContext *C, SK_Sketch *sketch, SK_Stroke *stk, SK_DrawData *dd, bool snap)
{
	if (sk_stroke_filtermval(dd)) {
		sk_addStrokePoint(C, sketch, stk, dd, snap);
		sk_updateDrawData(dd);
		sk_updateNextPoint(sketch, stk);
		
		return 1;
	}

	return 0;
}

static int ValidSketchViewContext(ViewContext *vc)
{
	Object *obedit = vc->obedit;
	Scene *scene = vc->scene;

	if (obedit &&
	    obedit->type == OB_ARMATURE &&
	    scene->toolsettings->bone_sketching & BONE_SKETCHING)
	{
		return 1;
	}
	else {
		return 0;
	}
}

int BDR_drawSketchNames(ViewContext *vc)
{
	if (ValidSketchViewContext(vc)) {
		SK_Sketch *sketch = viewcontextSketch(vc, 0);
		if (sketch) {
			sk_drawSketch(vc->scene, vc->v3d, sketch, 1);
			return 1;
		}
	}

	return 0;
}

void BDR_drawSketch(const bContext *C)
{
	if (ED_operator_sketch_mode(C)) {
		SK_Sketch *sketch = contextSketch(C, 0);
		if (sketch) {
			sk_drawSketch(CTX_data_scene(C), CTX_wm_view3d(C), sketch, 0);
		}
	}
}

static int sketch_delete(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	SK_Sketch *sketch = contextSketch(C, 0);
	if (sketch) {
		sk_deleteSelectedStrokes(sketch);
//			allqueue(REDRAWVIEW3D, 0);
	}
	WM_event_add_notifier(C, NC_SCREEN | ND_SKETCH | NA_REMOVED, NULL);
	return OPERATOR_FINISHED;
}

void BIF_sk_selectStroke(bContext *C, const int mval[2], short extend)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	SK_Sketch *sketch = contextSketch(C, 0);

	if (sketch != NULL && ts->bone_sketching & BONE_SKETCHING) {
		if (sk_selectStroke(C, sketch, mval, extend))
			ED_area_tag_redraw(CTX_wm_area(C));
	}
}

void BIF_convertSketch(bContext *C)
{
	if (ED_operator_sketch_full_mode(C)) {
		SK_Sketch *sketch = contextSketch(C, 0);
		if (sketch) {
			sk_convert(C, sketch);
//			BIF_undo_push("Convert Sketch");
//			allqueue(REDRAWVIEW3D, 0);
//			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
}

void BIF_deleteSketch(bContext *C)
{
	if (ED_operator_sketch_full_mode(C)) {
		SK_Sketch *sketch = contextSketch(C, 0);
		if (sketch) {
			sk_deleteSelectedStrokes(sketch);
//			BIF_undo_push("Convert Sketch");
//			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

#if 0
void BIF_selectAllSketch(bContext *C, int mode)
{
	if (BIF_validSketchMode(C))
	{
		SK_Sketch *sketch = contextSketch(C, 0);
		if (sketch)
		{
			sk_selectAllSketch(sketch, mode);
//			XXX
//			allqueue(REDRAWVIEW3D, 0);
		}
	}
}
#endif

SK_Sketch *contextSketch(const bContext *C, int create)
{
	Object *obedit = CTX_data_edit_object(C);
	SK_Sketch *sketch = NULL;

	if (obedit && obedit->type == OB_ARMATURE) {
		bArmature *arm = obedit->data;
	
		if (arm->sketch == NULL && create) {
			arm->sketch = createSketch();
		}
		sketch = arm->sketch;
	}

	return sketch;
}

SK_Sketch *viewcontextSketch(ViewContext *vc, int create)
{
	Object *obedit = vc->obedit;
	SK_Sketch *sketch = NULL;

	if (obedit && obedit->type == OB_ARMATURE) {
		bArmature *arm = obedit->data;
	
		if (arm->sketch == NULL && create) {
			arm->sketch = createSketch();
		}
		sketch = arm->sketch;
	}

	return sketch;
}

static int sketch_convert(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	SK_Sketch *sketch = contextSketch(C, 0);
	if (sketch != NULL) {
		sk_convert(C, sketch);
		ED_area_tag_redraw(CTX_wm_area(C));
	}
	return OPERATOR_FINISHED;
}

static int sketch_cancel_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	SK_Sketch *sketch = contextSketch(C, 0);
	if (sketch != NULL) {
		sk_cancelStroke(sketch);
		ED_area_tag_redraw(CTX_wm_area(C));
		return OPERATOR_FINISHED;
	}
	return OPERATOR_PASS_THROUGH;
}

static int sketch_finish(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	SK_Sketch *sketch = contextSketch(C, 0);
	if (sketch != NULL) {
		if (sk_finish_stroke(C, sketch)) {
			ED_area_tag_redraw(CTX_wm_area(C));
			return OPERATOR_FINISHED;
		}
	}
	return OPERATOR_PASS_THROUGH;
}

static int sketch_select(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	SK_Sketch *sketch = contextSketch(C, 0);
	if (sketch) {
		short extend = 0;
		if (sk_selectStroke(C, sketch, event->mval, extend))
			ED_area_tag_redraw(CTX_wm_area(C));
	}

	return OPERATOR_FINISHED;
}

static void sketch_draw_stroke_cancel(bContext *C, wmOperator *op)
{
	SK_Sketch *sketch = contextSketch(C, 1); /* create just to be sure */
	sk_cancelStroke(sketch);
	MEM_freeN(op->customdata);
}

static int sketch_draw_stroke(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool snap = RNA_boolean_get(op->ptr, "snap");
	SK_DrawData *dd;
	SK_Sketch *sketch = contextSketch(C, 1);

	op->customdata = dd = MEM_callocN(sizeof(SK_DrawData), "SketchDrawData");
	sk_initDrawData(dd, event->mval);

	sk_start_draw_stroke(sketch);

	sk_draw_stroke(C, sketch, sketch->active_stroke, dd, snap);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void sketch_draw_gesture_cancel(bContext *C, wmOperator *op)
{
	SK_Sketch *sketch = contextSketch(C, 1); /* create just to be sure */
	sk_cancelStroke(sketch);
	MEM_freeN(op->customdata);
}

static int sketch_draw_gesture(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool snap = RNA_boolean_get(op->ptr, "snap");
	SK_DrawData *dd;
	SK_Sketch *sketch = contextSketch(C, 1); /* create just to be sure */
	sk_cancelStroke(sketch);

	op->customdata = dd = MEM_callocN(sizeof(SK_DrawData), "SketchDrawData");
	sk_initDrawData(dd, event->mval);

	sk_start_draw_gesture(sketch);
	sk_draw_stroke(C, sketch, sketch->gesture, dd, snap);

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static int sketch_draw_modal(bContext *C, wmOperator *op, const wmEvent *event, short gesture, SK_Stroke *stk)
{
	bool snap = RNA_boolean_get(op->ptr, "snap");
	SK_DrawData *dd = op->customdata;
	SK_Sketch *sketch = contextSketch(C, 1); /* create just to be sure */
	int retval = OPERATOR_RUNNING_MODAL;

	switch (event->type) {
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
			snap = event->ctrl != 0;
			RNA_boolean_set(op->ptr, "snap", snap);
			break;
		case MOUSEMOVE:
		case INBETWEEN_MOUSEMOVE:
			dd->mval[0] = event->mval[0];
			dd->mval[1] = event->mval[1];
			sk_draw_stroke(C, sketch, stk, dd, snap);
			ED_area_tag_redraw(CTX_wm_area(C));
			break;
		case ESCKEY:
			op->type->cancel(C, op);
			ED_area_tag_redraw(CTX_wm_area(C));
			retval = OPERATOR_CANCELLED;
			break;
		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				if (gesture == 0) {
					sk_endContinuousStroke(stk);
					sk_filterLastContinuousStroke(stk);
					sk_updateNextPoint(sketch, stk);
					ED_area_tag_redraw(CTX_wm_area(C));
					MEM_freeN(op->customdata);
					retval = OPERATOR_FINISHED;
				}
				else {
					sk_endContinuousStroke(stk);
					sk_filterLastContinuousStroke(stk);

					if (stk->nb_points > 1) {
						/* apply gesture here */
						sk_applyGesture(C, sketch);
					}

					sk_freeStroke(stk);
					sketch->gesture = NULL;

					ED_area_tag_redraw(CTX_wm_area(C));
					MEM_freeN(op->customdata);
					retval = OPERATOR_FINISHED;
				}
			}
			break;
	}

	return retval;
}

static int sketch_draw_stroke_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SK_Sketch *sketch = contextSketch(C, 1); /* create just to be sure */
	return sketch_draw_modal(C, op, event, 0, sketch->active_stroke);
}

static int sketch_draw_gesture_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	SK_Sketch *sketch = contextSketch(C, 1); /* create just to be sure */
	return sketch_draw_modal(C, op, event, 1, sketch->gesture);
}

static int sketch_draw_preview(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool snap = RNA_boolean_get(op->ptr, "snap");
	SK_Sketch *sketch = contextSketch(C, 0);

	if (sketch) {
		SK_DrawData dd;

		sk_initDrawData(&dd, event->mval);
		sk_getStrokePoint(C, &sketch->next_point, sketch, sketch->active_stroke, &dd, snap);
		ED_area_tag_redraw(CTX_wm_area(C));
	}

	return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

/* ============================================== Poll Functions ============================================= */

int ED_operator_sketch_mode_active_stroke(bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	SK_Sketch *sketch = contextSketch(C, 0);

	if (ts->bone_sketching & BONE_SKETCHING &&
	    sketch != NULL &&
	    sketch->active_stroke != NULL)
	{
		return 1;
	}
	else {
		return 0;
	}
}

static int ED_operator_sketch_mode_gesture(bContext *C)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	SK_Sketch *sketch = contextSketch(C, 0);

	if (ts->bone_sketching & BONE_SKETCHING &&
	    (ts->bone_sketching & BONE_SKETCHING_QUICK) == 0 &&
	    sketch != NULL &&
	    sketch->active_stroke == NULL)
	{
		return 1;
	}
	else {
		return 0;
	}
}

int ED_operator_sketch_full_mode(bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);

	if (obedit &&
	    obedit->type == OB_ARMATURE &&
	    ts->bone_sketching & BONE_SKETCHING &&
	    (ts->bone_sketching & BONE_SKETCHING_QUICK) == 0)
	{
		return 1;
	}
	else {
		return 0;
	}
}

int ED_operator_sketch_mode(const bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	ToolSettings *ts = CTX_data_tool_settings(C);

	if (obedit &&
	    obedit->type == OB_ARMATURE &&
	    ts->bone_sketching & BONE_SKETCHING)
	{
		return 1;
	}
	else {
		return 0;
	}
}

/* ================================================ Operators ================================================ */

void SKETCH_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete";
	ot->idname = "SKETCH_OT_delete";
	ot->description = "Delete a sketch stroke";

	/* api callbacks */
	ot->invoke = sketch_delete;

	ot->poll = ED_operator_sketch_full_mode;

	/* flags */
//	ot->flag = OPTYPE_UNDO;
}

void SKETCH_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select";
	ot->idname = "SKETCH_OT_select";
	ot->description = "Select a sketch stroke";

	/* api callbacks */
	ot->invoke = sketch_select;

	ot->poll = ED_operator_sketch_full_mode;

	/* flags */
//	ot->flag = OPTYPE_UNDO;
}

void SKETCH_OT_cancel_stroke(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cancel Stroke";
	ot->idname = "SKETCH_OT_cancel_stroke";
	ot->description = "Cancel the current sketch stroke";

	/* api callbacks */
	ot->invoke = sketch_cancel_invoke;

	ot->poll = ED_operator_sketch_mode_active_stroke;

	/* flags */
//	ot->flag = OPTYPE_UNDO;
}

void SKETCH_OT_convert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert";
	ot->idname = "SKETCH_OT_convert";
	ot->description = "Convert the selected sketch strokes to bone chains";

	/* api callbacks */
	ot->invoke = sketch_convert;

	ot->poll = ED_operator_sketch_full_mode;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}

void SKETCH_OT_finish_stroke(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "End Stroke";
	ot->idname = "SKETCH_OT_finish_stroke";
	ot->description = "End and keep the current sketch stroke";

	/* api callbacks */
	ot->invoke = sketch_finish;

	ot->poll = ED_operator_sketch_mode_active_stroke;

	/* flags */
//	ot->flag = OPTYPE_UNDO;
}

void SKETCH_OT_draw_preview(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Draw Preview";
	ot->idname = "SKETCH_OT_draw_preview";
	ot->description = "Draw preview of current sketch stroke (internal use)";

	/* api callbacks */
	ot->invoke = sketch_draw_preview;

	ot->poll = ED_operator_sketch_mode_active_stroke;

	RNA_def_boolean(ot->srna, "snap", 0, "Snap", "");

	/* flags */
//	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

void SKETCH_OT_draw_stroke(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Draw Stroke";
	ot->idname = "SKETCH_OT_draw_stroke";
	ot->description = "Start to draw a sketch stroke";

	/* api callbacks */
	ot->invoke = sketch_draw_stroke;
	ot->modal  = sketch_draw_stroke_modal;
	ot->cancel = sketch_draw_stroke_cancel;

	ot->poll = (int (*)(bContext *))ED_operator_sketch_mode;

	RNA_def_boolean(ot->srna, "snap", 0, "Snap", "");

	/* flags */
	ot->flag = OPTYPE_BLOCKING; // OPTYPE_REGISTER|OPTYPE_UNDO
}

void SKETCH_OT_gesture(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Gesture";
	ot->idname = "SKETCH_OT_gesture";
	ot->description = "Start to draw a gesture stroke";

	/* api callbacks */
	ot->invoke = sketch_draw_gesture;
	ot->modal  = sketch_draw_gesture_modal;
	ot->cancel = sketch_draw_gesture_cancel;

	ot->poll = ED_operator_sketch_mode_gesture;

	RNA_def_boolean(ot->srna, "snap", 0, "Snap", "");

	/* flags */
	ot->flag = OPTYPE_BLOCKING; // OPTYPE_UNDO
}

