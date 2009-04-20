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

#ifndef BKE_NLA_H
#define BKE_NLA_H

struct bActionStrip;
struct ListBase;
struct Object;

void free_actionstrip (struct bActionStrip* strip);
void free_nlastrips (struct ListBase *nlalist);
void copy_nlastrips (struct ListBase *dst, struct ListBase *src);
void copy_actionstrip (struct bActionStrip **dst, struct bActionStrip **src);
void find_stridechannel(struct Object *ob, struct bActionStrip *strip);
struct bActionStrip *convert_action_to_strip (struct Object *ob);
#endif

