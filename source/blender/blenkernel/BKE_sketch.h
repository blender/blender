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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_SKETCH_H__
#define __BKE_SKETCH_H__

/** \file BKE_sketch.h
 *  \ingroup bke
 */

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

typedef struct SK_Point {
	float p[3];
	short p2d[2];
	float no[3];
	float size;
	SK_PType type;
	SK_PMode mode;
} SK_Point;

typedef struct SK_Stroke {
	struct SK_Stroke *next, *prev;

	SK_Point *points;
	int nb_points;
	int buf_size;
	int selected;
} SK_Stroke;

#define SK_OVERDRAW_LIMIT   5

typedef struct SK_Overdraw {
	SK_Stroke *target;
	int start, end;
	int count;
} SK_Overdraw;

#define SK_Stroke_BUFFER_INIT_SIZE 20

typedef struct SK_DrawData {
	int mval[2];
	int previous_mval[2];
	SK_PType type;
} SK_DrawData;

typedef struct SK_Intersection {
	struct SK_Intersection *next, *prev;
	SK_Stroke *stroke;
	int        before;
	int        after;
	int        gesture_index;
	float      p[3];
	float      lambda;       /* used for sorting intersection points */
} SK_Intersection;

typedef struct SK_Sketch {
	ListBase   strokes;
	ListBase   depth_peels;
	SK_Stroke *active_stroke;
	SK_Stroke *gesture;
	SK_Point   next_point;
	SK_Overdraw over;
} SK_Sketch;


typedef struct SK_Gesture {
	SK_Stroke   *stk;
	SK_Stroke   *segments;

	ListBase     intersections;
	ListBase     self_intersections;

	int          nb_self_intersections;
	int          nb_intersections;
	int          nb_segments;
} SK_Gesture;


/************************************************/

void freeSketch(SK_Sketch *sketch);
SK_Sketch *createSketch(void);

void sk_removeStroke(SK_Sketch *sketch, SK_Stroke *stk);

void sk_freeStroke(SK_Stroke *stk);
SK_Stroke *sk_createStroke(void);

SK_Point *sk_lastStrokePoint(SK_Stroke *stk);

void sk_allocStrokeBuffer(SK_Stroke *stk);
void sk_shrinkStrokeBuffer(SK_Stroke *stk);
void sk_growStrokeBuffer(SK_Stroke *stk);
void sk_growStrokeBufferN(SK_Stroke *stk, int n);

void sk_replaceStrokePoint(SK_Stroke *stk, SK_Point *pt, int n);
void sk_insertStrokePoint(SK_Stroke *stk, SK_Point *pt, int n);
void sk_appendStrokePoint(SK_Stroke *stk, SK_Point *pt);
void sk_insertStrokePoints(SK_Stroke *stk, SK_Point *pts, int len, int start, int end);

void sk_trimStroke(SK_Stroke *stk, int start, int end);
void sk_straightenStroke(SK_Stroke * stk, int start, int end, float p_start[3], float p_end[3]);
void sk_polygonizeStroke(SK_Stroke *stk, int start, int end);
void sk_flattenStroke(SK_Stroke *stk, int start, int end);
void sk_reverseStroke(SK_Stroke *stk);

void sk_filterLastContinuousStroke(SK_Stroke *stk);
void sk_filterStroke(SK_Stroke *stk, int start, int end);

void sk_initPoint(SK_Point *pt, SK_DrawData *dd, const float no[3]);
void sk_copyPoint(SK_Point *dst, SK_Point *src);

int sk_stroke_filtermval(SK_DrawData *dd);
void sk_endContinuousStroke(SK_Stroke *stk);

void sk_updateNextPoint(SK_Sketch *sketch, SK_Stroke *stk);

void sk_initDrawData(SK_DrawData *dd, const int mval[2]);

void sk_deleteSelectedStrokes(SK_Sketch *sketch);
void sk_selectAllSketch(SK_Sketch *sketch, int mode);

#endif
