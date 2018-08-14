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
 * The Original Code is Copyright (C) 2007 by Janne Karhu.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/physics/particle_edit_utildefines.h
 *  \ingroup edphys
 */

#ifndef __PARTICLE_EDIT_UTILDEFNIES_H__
#define __PARTICLE_EDIT_UTILDEFNIES_H__

#define KEY_K                   PTCacheEditKey *key; int k
#define POINT_P                 PTCacheEditPoint *point; int p
#define LOOP_POINTS             for (p = 0, point = edit->points; p < edit->totpoint; p++, point++)
#define LOOP_VISIBLE_POINTS     for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) if (!(point->flag & PEP_HIDE))
#define LOOP_SELECTED_POINTS    for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) if (point_is_selected(point))
#define LOOP_UNSELECTED_POINTS  for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) if (!point_is_selected(point))
#define LOOP_EDITED_POINTS      for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) if (point->flag & PEP_EDIT_RECALC)
#define LOOP_TAGGED_POINTS      for (p = 0, point = edit->points; p < edit->totpoint; p++, point++) if (point->flag & PEP_TAG)
#define LOOP_KEYS               for (k = 0, key = point->keys; k < point->totkey; k++, key++)
#define LOOP_VISIBLE_KEYS       for (k = 0, key = point->keys; k < point->totkey; k++, key++) if (!(key->flag & PEK_HIDE))
#define LOOP_SELECTED_KEYS      for (k = 0, key = point->keys; k < point->totkey; k++, key++) if ((key->flag & PEK_SELECT) && !(key->flag & PEK_HIDE))
#define LOOP_TAGGED_KEYS        for (k = 0, key = point->keys; k < point->totkey; k++, key++) if (key->flag & PEK_TAG)

#define KEY_WCO                 ((key->flag & PEK_USE_WCO) ? key->world_co : key->co)

#endif  /* __PARTICLE_EDIT_UTILDEFNIES_H__ */
