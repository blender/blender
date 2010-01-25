/**
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Arystanbek Dyussenov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_action_types.h"

#ifdef RNA_RUNTIME

#include "BKE_action.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"

/* return frame range of all curves (min, max) or (0, 1) if there are no keys */
void rna_Action_get_frame_range(bAction *act, int **frame_range, int *length_r)
{
	int *ret;
	float start, end;

	calc_action_range(act, &start, &end, 1);

	*length_r= 2;
	ret= MEM_callocN(*length_r * sizeof(int), "rna_Action_get_frame_range");

	ret[0]= (int)start;
	ret[1]= (int)end;

	*frame_range= ret;
}

#else

void RNA_api_action(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "get_frame_range", "rna_Action_get_frame_range");
	RNA_def_function_ui_description(func, "Get action frame range as a (min, max) tuple.");
	parm= RNA_def_int_array(func, "frame_range", 1, NULL, 0, 0, "", "Action frame range.", 0, 0);
	RNA_def_property_flag(parm, PROP_DYNAMIC);
	RNA_def_function_output(func, parm);
}

#endif
