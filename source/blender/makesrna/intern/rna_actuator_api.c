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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_actuator_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "WM_types.h"
#include "RNA_define.h"

#ifdef RNA_RUNTIME

#include "BKE_sca.h"
#include "DNA_controller_types.h"
#include "DNA_actuator_types.h"

static void rna_Actuator_link(bActuator *act, bController *cont)
{
	link_logicbricks((void **)&act, (void ***)&(cont->links), &cont->totlinks, sizeof(bActuator *));
}

static void rna_Actuator_unlink(bActuator *act, bController *cont)
{
	unlink_logicbricks((void **)&act, (void ***)&(cont->links), &cont->totlinks);
}

#else

void RNA_api_actuator(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "link", "rna_Actuator_link");
	RNA_def_function_ui_description(func, "Link the actuator to a controller");
	parm = RNA_def_pointer(func, "controller", "Controller", "", "Controller to link to");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_update(parm, NC_LOGIC, NULL);

	func = RNA_def_function(srna, "unlink", "rna_Actuator_unlink");
	RNA_def_function_ui_description(func, "Unlink the actuator from a controller");
	parm = RNA_def_pointer(func, "controller", "Controller", "", "Controller to unlink from");
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_property_update(parm, NC_LOGIC, NULL);
}

#endif

