/*  BKE_deform.h   June 2001
 *  
 *  support for deformation groups and hooks
 * 
 *	Reevan McKay et al
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_DEFORM_H
#define BKE_DEFORM_H

struct Object;
struct ListBase;
struct bDeformGroup;
struct MDeformVert;

void				 defgroup_copy_list(struct ListBase *lb1, struct ListBase *lb2);
struct bDeformGroup *defgroup_duplicate(struct bDeformGroup *ingroup);
struct bDeformGroup *defgroup_find_name(struct Object *ob, char *name);
int					 defgroup_find_index(struct Object *ob, struct bDeformGroup *dg);
int					*defgroup_flip_map(struct Object *ob, int use_default);
int					 defgroup_flip_index(struct Object *ob, int index, int use_default);
int					 defgroup_name_index(struct Object *ob, const char *name);
void				 defgroup_unique_name(struct bDeformGroup *dg, struct Object *ob);

struct MDeformWeight	*defvert_find_index(const struct MDeformVert *dv, int defgroup);
struct MDeformWeight	*defvert_verify_index(struct MDeformVert *dv, int defgroup);

float  defvert_find_weight(const struct MDeformVert *dvert, int group_num);
float  defvert_array_find_weight_safe(const struct MDeformVert *dvert, int index, int group_num);

void defvert_copy(struct MDeformVert *dvert_r, const struct MDeformVert *dvert);
void defvert_sync(struct MDeformVert *dvert_r, const struct MDeformVert *dvert, int use_verify);
void defvert_sync_mapped(struct MDeformVert *dvert_r, const struct MDeformVert *dvert, int *flip_map, int use_verify);
void defvert_remap (struct MDeformVert *dvert, int *map);
void defvert_flip(struct MDeformVert *dvert, int *flip_map);
void defvert_normalize(struct MDeformVert *dvert);

/* utility function, note that 32 chars is the maximum string length since its only
 * used with defgroups currently */
void flip_side_name(char *name, const char *from_name, int strip_number);

#endif

