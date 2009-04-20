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

#ifndef BSE_DRAWOOPS_H
#define BSE_DRAWOOPS_H

struct ScrArea;
struct Oops;

void boundbox_oops(short sel);
void give_oopslink_line(struct Oops *oops, struct OopsLink *ol, float *v1, float *v2);
void draw_oopslink(struct Oops *oops);
void draw_icon_oops(float *co, short type);
void mysbox(float x1, float y1, float x2, float y2);
unsigned int give_oops_color(short type, short sel, unsigned int *border);
void calc_oopstext(char *str, float *v1);
void draw_oops(struct Oops *oops);
void drawoopsspace(struct ScrArea *sa, void *spacedata);

#endif /* BSE_DRAWOOPS */

