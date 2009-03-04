 /* $Id: bmeshutils.c
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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_types.h"
#include "RNA_define.h"
#include "RNA_access.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_report.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BMF_Api.h"

#include "ED_mesh.h"
#include "ED_view3d.h"
#include "ED_util.h"
#include "ED_screen.h"
#include "BIF_transform.h"

#include "UI_interface.h"

#include "mesh_intern.h"
#include "bmesh.h"

int EDBM_CallOpf(EditMesh *em, wmOperator *op, char *fmt, ...)
{
	BMesh *bm = editmesh_to_bmesh(em);
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, &bmop, fmt, list)) {
		BKE_report(op->reports, RPT_ERROR,
			   "Parse error in EDBM_CallOpf");
		va_end(list);
		return 0;
	}

	BMO_Exec_Op(bm, &bmop);
	BMO_Finish_Op(bm, &bmop);

	va_end(list);

	return EDBM_Finish(bm, em, op, 1);
}

int EDBM_CallOpfSilent(EditMesh *em, char *fmt, ...)
{
	BMesh *bm = editmesh_to_bmesh(em);
	BMOperator bmop;
	va_list list;

	va_start(list, fmt);

	if (!BMO_VInitOpf(bm, &bmop, fmt, list)) {
		va_end(list);
		return 0;
	}

	BMO_Exec_Op(bm, &bmop);
	BMO_Finish_Op(bm, &bmop);

	va_end(list);

	return EDBM_Finish(bm, em, NULL, 0);
}

/*returns 0 on error, 1 on success*/
int EDBM_Finish(BMesh *bm, EditMesh *em, wmOperator *op, int report) {
	EditMesh *em2;
	char *errmsg;

	if (BMO_GetError(bm, &errmsg, NULL)) {
		if (report) BKE_report(op->reports, RPT_ERROR, errmsg);
		BM_Free_Mesh(bm);
		return 0;
	}

	em2 = bmesh_to_editmesh(bm);
	set_editMesh(em, em2);
	MEM_freeN(em2);
	BM_Free_Mesh(bm);

	return 1;
}