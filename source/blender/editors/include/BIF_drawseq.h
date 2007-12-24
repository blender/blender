/**
 * $Id: BIF_drawseq.h 12177 2007-10-01 08:03:11Z schlaile $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_DRAWSEQ_H
#define BIF_DRAWSEQ_H

struct ScrArea;
struct Sequence;

void drawprefetchseqspace(struct ScrArea *sa, void *spacedata);
void drawseqspace(struct ScrArea *sa, void *spacedata);
void set_special_seq_update(int val);

void seq_viewmove(SpaceSeq *sseq);
void seq_reset_imageofs(SpaceSeq *sseq);
#endif

