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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_mesh.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"


#include "ED_image.h"

#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"



#include "image_intern.h"

/********************** toolbox operator *********************/

static int toolbox_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Object *obedit= CTX_data_edit_object(C);
	uiPopupMenu *pup;
	uiLayout *layout;
	int show_uvedit;

	show_uvedit= ED_space_image_show_uvedit(sima, obedit);

	pup= uiPupMenuBegin(C, "Toolbox", 0);
	layout= uiPupMenuLayout(pup);

	uiItemM(layout, C, "IMAGE_MT_view", NULL, 0);
	if(show_uvedit) uiItemM(layout, C, "IMAGE_MT_select", NULL, 0);
	uiItemM(layout, C, "IMAGE_MT_image", NULL, 0);
	if(show_uvedit) uiItemM(layout, C, "IMAGE_MT_uvs", NULL, 0);

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void IMAGE_OT_toolbox(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Toolbox";
	ot->idname= "IMAGE_OT_toolbox";
	
	/* api callbacks */
	ot->invoke= toolbox_invoke;
	ot->poll= space_image_main_area_poll;
}

