/**
 * antialias.c
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

#include "imbuf.h"

#include "BLI_blenlib.h"
#include "DNA_listBase.h"

#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"

/* how it works:

1 - seek for a transistion in a collumn
2 - check the relationship with left and right, 

Is pixel above transition to the left or right equal to the top color, seek down

Is pixel below transition to the left or right equal to the bottom color, seek up 
		
*/

/* there should be a funcion * to indicate if two colors are
 * equal or not.
 * For now we use a define
 */


static unsigned int anti_mask = 0xffffffff;
static int anti_a, anti_b, anti_g, anti_r;

#define compare(x, y) ((x ^ y) & anti_mask)

typedef struct Edge
{
	struct Edge * next, * prev;
	short position;
	int col1, col2;
}Edge;

static void anti_free_listarray(int count, ListBase * listarray)
{
	int i;
	
	if (listarray == 0) return;
	
	for (i = 0; i < count; i++) BLI_freelistN(listarray + i);
	MEM_freeN(listarray);	
}

static ListBase * scanimage(struct ImBuf * ibuf, int dir)
{
	int step, pixels, lines, nextline, x, y, col1, col2;
	unsigned int * rect;
	ListBase * listarray, * curlist;
	Edge * edge;
	int count;
	
	switch (dir) {
	case 'h':
		step = 1; nextline = ibuf->x;
		pixels = ibuf->x; lines = ibuf->y;
		break;
/*	case 'v':  changed so assured values for step etc.. */
	default:
		step = ibuf->x; nextline = 1;
		pixels = ibuf->y; lines = ibuf->x;
	}
	
	listarray = (ListBase*)MEM_callocN((lines)* sizeof(ListBase), "listarray");
	for (y = 0; y < lines; y++){
		rect = ibuf->rect;
		rect += y * nextline;
		curlist = listarray + y;
		
		col1 = rect[0];
		count = 0;
		
		for (x = 0; x < pixels; x++) {
			col2 = rect[0];
			if (compare(col1, col2)) {
				edge = NEW(Edge);

				if (edge == NULL) return(0);

				edge->position = x;
				edge->col1 = col1;
				edge->col2 = col2;
				BLI_addtail(curlist, edge);
				col1 = col2;
				count++;
				if (count > 100) {
					printf("\n\n%s: Aborting antialias !\n", ibuf->name);
					printf("To many transitions.\nIs this a natural image ?\n\n"), 
					anti_free_listarray(lines, listarray);
					return(0);
				}
			}
			rect += step;
		}
	}
	
	return(listarray);
}


static Edge * findmatch(Edge * first, Edge * edge)
{
	Edge * match = 0;
	int in = 0, out = 65535;
	
	if (edge->prev) in = edge->prev->position;
	if (edge->next) out = edge->next->position;
	
	while (first) {
		if (first->position < edge->position) {
			if (first->col1 == edge->col1) {
				if (first->position >= in) match = first;
			} else if (first->col2 == edge->col2) {
				if (first->next == 0) match = first;
				else if (first->next->position >= edge->position) match = first;
			} else if (first->col2 == edge->col1) {
				match = 0; /* at 'sig saw' situations this one can be wrongly set */
			}
		} else if (first->position == edge->position) {
			if (first->col1 == edge->col1 || first->col2 == edge->col2) match = first;
		} else {
			if (match) break;	/* there is one */
			
			if (first->col1 == edge->col1) {
				if (first->prev == 0) match = first;
				else if (first->prev->position <= edge->position) match = first;
			} else if (first->col2 == edge->col2) {
				if (first->position <= out) match = first;
			}
		}
		
		first = first->next;
	}
	
	return(match);
}


static void filterdraw(unsigned int * ldest, unsigned int * lsrce, int zero, int half, int step)
{
	uchar * src, * dst;
	int count;
	double weight, add;
	
	/* we filter the pixels at ldest between in and out with pixels from lsrce
	 * weight values go from 0 to 1
	 */
	

	count = half - zero;
	if (count < 0) count = -count;
	if (count <= 1) return;
	
	if (zero < half) {
		src = (uchar *) (lsrce + (step * zero));
		dst = (uchar *) (ldest + (step * zero));
	} else {
		zero--;
		src = (uchar *) (lsrce + (step * zero));
		dst = (uchar *) (ldest + (step * zero));
		step = -step;
	}
	
	step = 4 * step;
	
	dst += step * (count >> 1);
	src += step * (count >> 1);
	
	count = (count + 1) >> 1;
	add = 0.5 / count;
	weight = 0.5 * add;
	
	/* this of course gamma corrected */
	
	for(; count > 0; count --) {
		if (anti_a) dst[0] += weight * (src[0] - dst[0]);
		if (anti_b) dst[1] += weight * (src[1] - dst[1]);
		if (anti_g) dst[2] += weight * (src[2] - dst[2]);
		if (anti_r) dst[3] += weight * (src[3] - dst[3]);
		dst += step;
		src += step;
		weight += add;
	}
}

static void filterimage(struct ImBuf * ibuf, struct ImBuf * cbuf, ListBase * listarray, int dir)
{
	int step, pixels, lines, nextline, y, pos, drawboth;
	unsigned int * irect, * crect;
	Edge * left, * middle, * right, temp, * any;
	
	switch (dir) {
	case 'h':
		step = 1; nextline = ibuf->x;
		pixels = ibuf->x; lines = ibuf->y;
		break;
/*	case 'v': changed so have values */
	default:
		step = ibuf->x; nextline = 1;
		pixels = ibuf->y; lines = ibuf->x;
	}
	
	for (y = 1; y < lines - 1; y++){
		irect = ibuf->rect;
		irect += y * nextline;
		crect = cbuf->rect;
		crect += y * nextline;
		
		middle = listarray[y].first;
		while (middle) {
			left = findmatch(listarray[y - 1].first, middle);
			right = findmatch(listarray[y + 1].first, middle);
			drawboth = FALSE;
			
			if (left == 0 || right == 0) {
				/* edge */
				any = left;
				if (right) any = right;
				if (any) {
					/* mirroring */
					pos = 2 * middle->position - any->position;

					if (any->position < middle->position) {
						if (pos > pixels - 1) pos = pixels - 1;
						if (middle->next) {
							if (pos > middle->next->position) pos = middle->next->position;
						}
/*						if (any->next) {
							if (pos > any->next->position) pos = any->next->position;
						}
*/					} else {
						if (pos < 0) pos = 0;
						if (middle->prev) {
							if (pos < middle->prev->position) pos = middle->prev->position;
						}
/*						if (any->prev) {
							if (pos < any->prev->position) pos = any->prev->position;
						}
*/					}
					temp.position = pos;
					if (left) right = &temp;
					else left = &temp;
					drawboth = TRUE;
				}
			} else if (left->position == middle->position || right->position == middle->position) {
				/* straight piece */
				/* small corner, with one of the two at distance 2 (the other is at dist 0) ? */
				
				if (abs(left->position - right->position) == 2) drawboth = TRUE;
			} else if (left->position < middle->position && right->position > middle->position){
				/* stair 1 */
				drawboth = TRUE;
			} else if (left->position > middle->position && right->position < middle->position){
				/* stair 2 */
				drawboth = TRUE;
			} else {
				/* a peek */
				drawboth = TRUE;
			}
			
			if (drawboth) {
				filterdraw(irect, crect - nextline, left->position, middle->position, step);
				filterdraw(irect, crect + nextline, right->position, middle->position, step);
			}

			middle = middle->next;
		}
	}
}


void IMB_antialias(struct ImBuf * ibuf)
{
	struct ImBuf * cbuf;
	ListBase * listarray;
	
	if (ibuf == 0) return;
	cbuf = IMB_dupImBuf(ibuf);
	if (cbuf == 0) return;
	
	anti_a = (anti_mask >> 24) & 0xff;
	anti_b = (anti_mask >> 16) & 0xff;
	anti_g = (anti_mask >>  8) & 0xff;
	anti_r = (anti_mask >>  0) & 0xff;
	
	listarray = scanimage(cbuf, 'h');
	if (listarray) {
		filterimage(ibuf, cbuf, listarray, 'h');
		anti_free_listarray(ibuf->y, listarray);
		
		listarray = scanimage(cbuf, 'v');
		if (listarray) {
			filterimage(ibuf, cbuf, listarray, 'v');
			anti_free_listarray(ibuf->x, listarray);
		}
	}
			
	IMB_freeImBuf(cbuf);
}


/* intelligent scaling */

static void _intel_scale(struct ImBuf * ibuf, ListBase * listarray, int dir)
{
	int step, lines, nextline, x, y, col;
	unsigned int * irect, * trect;
	int start, end;
	Edge * left, * right;
	struct ImBuf * tbuf;
	
	switch (dir) {
	case 'h':
		step = 1; nextline = ibuf->x;
		lines = ibuf->y;
		tbuf = IMB_double_fast_y(ibuf);
		break;
	case 'v':
		step = 2 * ibuf->x; nextline = 1;
		lines = ibuf->x;
		tbuf = IMB_double_fast_x(ibuf);
		break;
	default:
		return;
	}
	
	if (tbuf == NULL) return;

	imb_freerectImBuf(ibuf);

	ibuf->rect = tbuf->rect;
	ibuf->mall |= IB_rect;
	
	ibuf->x = tbuf->x;
	ibuf->y = tbuf->y;
	tbuf->rect = 0;
	IMB_freeImBuf(tbuf);
	
	for (y = 0; y < lines - 2; y++){
		irect = ibuf->rect;
		irect += ((2 * y) + 1) * nextline;
		
		left = listarray[y].first;
		while (left) {
			right = findmatch(listarray[y + 1].first, left);
			if (right) {
				if (left->col2 == right->col2) {
					if (left->next && right->next) {
						if (left->next->position >= right->position) {
							start = ((left->position + right->position) >> 1);
							end = ((left->next->position + right->next->position) >> 1);
							col = left->col2;
							trect = irect + (start * step);
							for (x = start; x < end; x++) {
								*trect = col;
								trect += step;
							}
						}
					}
				}

				if (left->col1 == right->col1) {
					if (left->prev && right->prev) {
						if (left->prev->position <= right->position) {
							end = ((left->position + right->position) >> 1);
							start = ((left->prev->position + right->prev->position) >> 1);
							col = left->col1;
							trect = irect + (start * step);
							for (x = start; x < end; x++) {
								*trect = col;
								trect += step;
							}
						}
					}
				}

			}
			left = left->next;
		}
	}
}


void IMB_clever_double(struct ImBuf * ibuf)
{
	ListBase * listarray, * curlist;
	Edge * new;
	int size;
	int i;
	
	if (ibuf == 0) return;
	
	size = ibuf->x;
	listarray = scanimage(ibuf, 'v');
	if (listarray) {
		for (i = 0; i < size; i++) {
			curlist = listarray + i;
			new = (Edge*)MEM_callocN(sizeof(Edge),"Edge");
			new->col2 = ibuf->rect[i]; /* upper pixel */
			new->col1 = new->col2 - 1;
			BLI_addhead(curlist, new);
			new = (Edge*)MEM_callocN(sizeof(Edge),"Edge");
			new->position = ibuf->y - 1;
			new->col1 = ibuf->rect[i + ((ibuf->y -1) * ibuf->x)]; /* bottom pixel */
			new->col2 = new->col1 - 1;
			BLI_addtail(curlist, new);
		}
		_intel_scale(ibuf, listarray, 'v');
		anti_free_listarray(size, listarray);

		size = ibuf->y;
		listarray = scanimage(ibuf, 'h');
		if (listarray) {
			for (i = 0; i < size; i++) {
				curlist = listarray + i;
				new =  (Edge*)MEM_callocN(sizeof(Edge),"Edge");
				new->col2 = ibuf->rect[i * ibuf->x]; /* left pixel */
				new->col1 = new->col2 - 1;
				BLI_addhead(curlist, new);
				new =  (Edge*)MEM_callocN(sizeof(Edge),"Edge");
				new->position = ibuf->x - 1;
				new->col1 = ibuf->rect[((i + 1) * ibuf->x) - 1]; /* right pixel */
				new->col2 = new->col1 - 1;
				BLI_addtail(curlist, new);
			}
			_intel_scale(ibuf, listarray, 'h');
			anti_free_listarray(size, listarray);
		}
	}
}
