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
#ifndef BKE_BOOLEANOPS_H
#define BKE_BOOLEANOPS_H

struct Object;
struct Base;
struct DerivedMesh;

/* Performs a boolean between two mesh objects, it is assumed that both objects
   are in fact a mesh object. On success returns 1 and creates a new mesh object
   into blender data structures. On failure returns 0 and reports an error. */
int NewBooleanMesh(struct Base *base, struct Base *base_select, int op);


/* Performs a boolean between two mesh objects, it is assumed that both objects
   are in fact mesh object. On success returns a DerivedMesh. On failure
   returns NULL and reports an error. */
struct DerivedMesh *NewBooleanDerivedMesh(struct Object *ob,
                                          struct Object *ob_select,
                                          int op);
#endif

