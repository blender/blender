/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009, Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_SEQUENCER_H
#define ED_SEQUENCER_H

#define SEQ_ZOOM_FAC(szoom) (szoom > 0)? (szoom) : (szoom == 0)? (1.0) : (-1.0/szoom)


/* in space_sequencer.c, for rna update function */
void ED_sequencer_update_view(bContext *C, int view);

#endif /*  ED_SEQUENCER_H */
