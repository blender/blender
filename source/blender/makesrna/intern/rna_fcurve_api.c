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
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_anim_types.h"

#ifdef RNA_RUNTIME

#include <stddef.h>

#include "BLI_blenlib.h"

#include "BKE_animsys.h"
#include "BKE_fcurve.h"

DriverTarget *rna_Driver_add_target(ChannelDriver *driver, char *name)
{
	DriverTarget *dtar= driver_add_new_target(driver);
	
	/* set the name if given */
	if (name && name[0]) {
		BLI_strncpy(dtar->name, name, 64);
		BLI_uniquename(&driver->targets, dtar, "var", '_', offsetof(DriverTarget, name), 64);
	}
	
	/* return this target for the users to play with */
	return dtar;
}

void rna_Driver_remove_target(ChannelDriver *driver, DriverTarget *dtar)
{
	/* call the API function for this */
	driver_free_target(driver, dtar);
}

#else

void RNA_api_drivers(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	/* add target */
	func= RNA_def_function(srna, "add_target", "rna_Driver_add_target");
	RNA_def_function_ui_description(func, "Add a new target for the driver.");
		/* return type */
	parm= RNA_def_pointer(func, "target", "DriverTarget", "", "Newly created Driver Target.");
		RNA_def_function_return(func, parm);
		/* optional name parameter */
	parm= RNA_def_string(func, "name", "", 64, "Name", "Name to use in scripted expressions/functions. (No spaces or dots are allowed. Also, must not start with a symbol or digit)");
	
	/* remove target */
	func= RNA_def_function(srna, "remove_target", "rna_Driver_remove_target");
		RNA_def_function_ui_description(func, "Remove an existing target from the driver.");
		/* target to remove*/
	parm= RNA_def_pointer(func, "target", "DriverTarget", "", "Target to remove from the driver.");
		RNA_def_property_flag(parm, PROP_REQUIRED);
}

#endif
