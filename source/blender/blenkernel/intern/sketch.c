/**
 *
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_sketch.h"
#include "BKE_utildefines.h"

#include "DNA_userdef_types.h"

void freeSketch(SK_Sketch *sketch)
{
	SK_Stroke *stk, *next;

	for (stk = sketch->strokes.first; stk; stk = next)
	{
		next = stk->next;

		sk_freeStroke(stk);
	}

	BLI_freelistN(&sketch->depth_peels);

	MEM_freeN(sketch);
}

SK_Sketch* createSketch()
{
	SK_Sketch *sketch;

	sketch = MEM_callocN(sizeof(SK_Sketch), "SK_Sketch");

	sketch->active_stroke = NULL;
	sketch->gesture = NULL;

	sketch->strokes.first = NULL;
	sketch->strokes.last = NULL;

	return sketch;
}

void sk_initPoint(SK_Point *pt, SK_DrawData *dd, float *no)
{
	if (no)
	{
		VECCOPY(pt->no, no);
		normalize_v3(pt->no);
	}
	else
	{
		pt->no[0] = 0;
		pt->no[1] = 0;
		pt->no[2] = 1;
	}
	pt->p2d[0] = dd->mval[0];
	pt->p2d[1] = dd->mval[1];
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

	sub_v3_v3v3(delta_p, p_end, p_start);

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
		mul_v3_fl(p, delta);
		add_v3_v3(p, p_start);
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

	sub_v3_v3v3(distance, stk->points[end].p, stk->points[start].p);
	project_v3_v3v3(normal, distance, normal);
	limit = normalize_v3(normal);

	for (i = 1; i < total - 1; i++)
	{
		float d = limit * i / total;
		float offset[3];
		float *p = stk->points[start + i].p;

		sub_v3_v3v3(distance, p, stk->points[start].p);
		project_v3_v3v3(distance, distance, normal);

		VECCOPY(offset, normal);
		mul_v3_fl(offset, d);

		sub_v3_v3(p, distance);
		add_v3_v3(p, offset);
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


/* Ramer-Douglas-Peucker algorithm for line simplification */
void sk_filterStroke(SK_Stroke *stk, int start, int end)
{
	SK_Point *old_points = stk->points;
	int nb_points = stk->nb_points;
	char *marked = NULL;
	char work;
	int i;

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

	marked = MEM_callocN(nb_points, "marked array");
	marked[start] = 1;
	marked[end] = 1;
	
	work = 1;
	
	/* while still reducing */
	while (work)
	{
		int ls, le;
		work = 0;
		
		ls = start;
		le = start+1;
		
		/* while not over interval */
		while (ls < end)
		{
			int max_i = 0;
			short v1[2];
			float max_dist = 16; /* more than 4 pixels */
			
			/* find the next marked point */
			while(marked[le] == 0)
			{
				le++;
			}
			
			/* perpendicular vector to ls-le */
			v1[1] = old_points[le].p2d[0] - old_points[ls].p2d[0]; 
			v1[0] = old_points[ls].p2d[1] - old_points[le].p2d[1]; 
			

			for( i = ls + 1; i < le; i++ )
			{
				float mul;
				float dist;
				short v2[2];
				
				v2[0] = old_points[i].p2d[0] - old_points[ls].p2d[0]; 
				v2[1] = old_points[i].p2d[1] - old_points[ls].p2d[1];
				
				if (v2[0] == 0 && v2[1] == 0)
				{
					continue;
				}

				mul = (float)(v1[0]*v2[0] + v1[1]*v2[1]) / (float)(v2[0]*v2[0] + v2[1]*v2[1]);
				
				dist = mul * mul * (v2[0]*v2[0] + v2[1]*v2[1]);
				
				if (dist > max_dist)
				{
					max_dist = dist;
					max_i = i;
				}
			}
			
			if (max_i != 0)
			{
				work = 1;
				marked[max_i] = 1;
			}
			
			ls = le;
			le = ls + 1;
		}
	}
	

	/* adding points after range */
	for (i = start; i <= end; i++)
	{
		if (marked[i])
		{
			sk_appendStrokePoint(stk, old_points + i);
		}
	}

	MEM_freeN(marked);

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

void sk_endContinuousStroke(SK_Stroke *stk)
{
	stk->points[stk->nb_points - 1].type = PT_EXACT;
}

void sk_updateNextPoint(SK_Sketch *sketch, SK_Stroke *stk)
{
	if (stk)
	{
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

void sk_initDrawData(SK_DrawData *dd, short mval[2])
{
	dd->mval[0] = mval[0];
	dd->mval[1] = mval[1];
	dd->previous_mval[0] = -1;
	dd->previous_mval[1] = -1;
	dd->type = PT_EXACT;
}


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
