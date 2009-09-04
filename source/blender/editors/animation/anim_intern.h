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
 * Inc., 59 Temple Place * Suite 330, Boston, MA  02111*1307, USA.
 *
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender (with some old code)
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef ANIM_INTERN_H
#define ANIM_INTERN_H

/* KeyingSets/Keyframing Interface ------------- */

/* list of builtin KeyingSets (defined in keyingsets.c) */
extern ListBase builtin_keyingsets;

/* for builtin keyingsets - context poll */
short keyingset_context_ok_poll(bContext *C, KeyingSet *ks);

/* Main KeyingSet operations API call */
short modifykey_get_context_data (bContext *C, ListBase *dsources, KeyingSet *ks);

#endif // ANIM_INTERN_H
