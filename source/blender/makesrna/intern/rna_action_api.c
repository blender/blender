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
 * Contributor(s): Blender Foundation
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

int *rna_Action_get_frames(bAction *act, int *ret_length)
{
	*ret_length= 3;
	int *ret= MEM_callocN(*ret_length * sizeof(int), "action frames");
	ret[0] = 1;
	ret[1] = 2;
	ret[2] = 3;
	return ret;
}

#else

void RNA_api_action(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "get_frames", "rna_Action_get_frames");
	RNA_def_function_ui_description(func, "Get action frames."); /* XXX describe better */
	parm= RNA_def_int_array(func, "frames", 1, NULL, 0, 0, "", "", 0, 0);
	RNA_def_property_flag(parm, PROP_DYNAMIC_ARRAY);
	RNA_def_function_return(func, parm);
}

#endif
