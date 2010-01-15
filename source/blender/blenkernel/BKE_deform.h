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

#ifndef BKE_DEFORM_H
#define BKE_DEFORM_H

struct Object;
struct ListBase;
struct bDeformGroup;
struct MDeformVert;

void copy_defgroups (struct ListBase *lb1, struct ListBase *lb2);
struct bDeformGroup *copy_defgroup (struct bDeformGroup *ingroup);
struct bDeformGroup *get_named_vertexgroup (Object *ob, char *name);
int get_defgroup_num (struct Object *ob, struct bDeformGroup *dg);
int *get_defgroup_flip_map(struct Object *ob);
int get_named_vertexgroup_num (Object *ob, const char *name);
void unique_vertexgroup_name (struct bDeformGroup *dg, struct Object *ob);
void flip_vertexgroup_name (char *name_r, const char *name, int strip_number);

float deformvert_get_weight(const struct MDeformVert *dvert, int group_num);
float vertexgroup_get_vertex_weight(const struct MDeformVert *dvert, int index, int group_num);

void copy_defvert (struct MDeformVert *dvert_r, const struct MDeformVert *dvert);
void flip_defvert (struct MDeformVert *dvert, int *flip_map);

#endif

