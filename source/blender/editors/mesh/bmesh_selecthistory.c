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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* ********* Selection History ************ */

#include "BKE_tessmesh.h"


/* these wrap equivalent bmesh functions.  I'm in two minds of it we should
 * just use the bm functions directly; on the one hand, there's no real
 * need (at the moment) to wrap them, but on the other hand having these
 * wrapped avoids a confusing mess of mixing BM_ and EDBM_ namespaces. */

void EDBM_editselection_center(BMEditMesh *em, float *center, BMEditSelection *ese)
{
	BM_editselection_center(em->bm, center, ese);
}

void EDBM_editselection_normal(float *normal, BMEditSelection *ese)
{
	BM_editselection_normal(normal, ese);
}

/* Calculate a plane that is rightangles to the edge/vert/faces normal
also make the plane run along an axis that is related to the geometry,
because this is used for the manipulators Y axis.*/
void EDBM_editselection_plane(BMEditMesh *em, float *plane, BMEditSelection *ese)
{
	BM_editselection_plane(em->bm, plane, ese);
}

void EDBM_remove_selection(BMEditMesh *em, void *data)
{
	BM_select_history_remove(em->bm, data);
}

void EDBM_store_selection(BMEditMesh *em, void *data)
{
	BM_select_history_store(em->bm, data);
}

void EDBM_validate_selections(BMEditMesh *em)
{
	BM_select_history_validate(em->bm);
}
