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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_OBJECT_DEFORM_H__
#define __BKE_OBJECT_DEFORM_H__

/** \file BKE_object_deform.h
 * \ingroup bke
 * \brief Functions for dealing with objects and deform verts,
 *        used by painting and tools.
 */

struct Object;

bool *BKE_objdef_lock_flags_get(struct Object *ob, const int defbase_tot);
bool *BKE_objdef_validmap_get(struct Object *ob, const int defbase_tot);
bool *BKE_objdef_selected_get(struct Object *ob, int defbase_tot, int *r_dg_flags_sel_tot);

#endif  /* __BKE_OBJECT_DEFORM_H__ */
