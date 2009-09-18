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

#include "RNA_define.h"
#include "RNA_types.h"

#ifdef RNA_RUNTIME

#include "BKE_context.h"

#include "WM_api.h"

#else

void RNA_api_wm(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *prop;

	func= RNA_def_function(srna, "add_fileselect", "WM_event_add_fileselect");
	RNA_def_function_flag(func, FUNC_NO_SELF|FUNC_USE_CONTEXT);
	RNA_def_function_ui_description(func, "Show up the file selector.");
	prop= RNA_def_pointer(func, "operator", "Operator", "", "Operator to call.");
	RNA_def_property_flag(prop, PROP_REQUIRED);
}

#endif

