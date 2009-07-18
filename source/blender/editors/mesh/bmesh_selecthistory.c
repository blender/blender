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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "MTC_matrixops.h"

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_render_ext.h"  /* externtex */

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "mesh_intern.h"

#include "BLO_sys_types.h" // for intptr_t support

/*these wrap equivilent bmesh functions.  I'm in two minds of it we should
  just use the bm functions directly; on the one hand, there's no real
  need (at the moment) to wrap them, but on the other hand having these
  wrapped avoids a confusing mess of mixing BM_ and EDBM_ namespaces.*/

void EDBM_editselection_center(BMEditMesh *em, float *center, BMEditSelection *ese)
{
	BM_editselection_center(em->bm, center, ese);
}

void EDBM_editselection_normal(float *normal, BMEditSelection *ese)
{
	BM_editselection_normal(normal, ese);
}

/* Calculate a plane that is rightangles to the edge/vert/faces normal
also make the plane run allong an axis that is related to the geometry,
because this is used for the manipulators Y axis.*/
void EDBM_editselection_plane(BMEditMesh *em, float *plane, BMEditSelection *ese)
{
	BM_editselection_plane(em->bm, plane, ese);
}

void EDBM_remove_selection(BMEditMesh *em, void *data)
{
	BM_remove_selection(em->bm, data);
}

void EDBM_store_selection(BMEditMesh *em, void *data)
{
	BM_store_selection(em->bm, data);
}

void EDBM_validate_selections(BMEditMesh *em)
{
	BM_validate_selections(em->bm);
}
