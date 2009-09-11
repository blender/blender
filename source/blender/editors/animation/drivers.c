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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung (full recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_dynstr.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_fcurve.h"
#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_key.h"
#include "BKE_material.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

/* ************************************************** */
/* Animation Data Validation */

/* Get (or add relevant data to be able to do so) F-Curve from the driver stack, 
 * for the given Animation Data block. This assumes that all the destinations are valid.
 */
FCurve *verify_driver_fcurve (ID *id, const char rna_path[], const int array_index, short add)
{
	AnimData *adt;
	FCurve *fcu;
	
	/* sanity checks */
	if ELEM(NULL, id, rna_path)
		return NULL;
	
	/* init animdata if none available yet */
	adt= BKE_animdata_from_id(id);
	if ((adt == NULL) && (add))
		adt= BKE_id_add_animdata(id);
	if (adt == NULL) { 
		/* if still none (as not allowed to add, or ID doesn't have animdata for some reason) */
		return NULL;
	}
		
	/* try to find f-curve matching for this setting 
	 *	- add if not found and allowed to add one
	 *		TODO: add auto-grouping support? how this works will need to be resolved
	 */
	fcu= list_find_fcurve(&adt->drivers, rna_path, array_index);
	
	if ((fcu == NULL) && (add)) {
		/* use default settings to make a F-Curve */
		fcu= MEM_callocN(sizeof(FCurve), "FCurve");
		
		fcu->flag = (FCURVE_VISIBLE|FCURVE_AUTO_HANDLES|FCURVE_SELECTED);
		
		/* store path - make copy, and store that */
		fcu->rna_path= BLI_strdupn(rna_path, strlen(rna_path));
		fcu->array_index= array_index;
		
		/* add some new driver data */
		fcu->driver= MEM_callocN(sizeof(ChannelDriver), "ChannelDriver");
		
		/* add simple generator modifier for driver so that there is some visible representation */
		add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR);
		
		/* just add F-Curve to end of driver list */
		BLI_addtail(&adt->drivers, fcu);
	}
	
	/* return the F-Curve */
	return fcu;
}

/* ************************************************** */
/* Driver Management API */

/* Main Driver Management API calls:
 * 	Add a new driver for the specified property on the given ID block
 */
short ANIM_add_driver (ID *id, const char rna_path[], int array_index, short flag, int type)
{	
	PointerRNA id_ptr, ptr;
	PropertyRNA *prop;
	FCurve *fcu;
	
	/* validate pointer first - exit if failure */
	RNA_id_pointer_create(id, &id_ptr);
	if ((RNA_path_resolve(&id_ptr, rna_path, &ptr, &prop) == 0) || (prop == NULL)) {
		printf("Insert Key: Could not add Driver, as RNA Path is invalid for the given ID (ID = %s, Path = %s)\n", id->name, rna_path);
		return 0;
	}
	
	/* create F-Curve with Driver */
	fcu= verify_driver_fcurve(id, rna_path, array_index, 1);

	if (fcu && fcu->driver) {
		fcu->driver->type= type;
		
		/* fill in current value for python */
		if (type == DRIVER_TYPE_PYTHON) {
			PropertyType proptype= RNA_property_type(prop);
			int array= RNA_property_array_length(&ptr, prop);
			char *expression= fcu->driver->expression;
			int val, maxlen= sizeof(fcu->driver->expression);
			float fval;
			
			if (proptype == PROP_BOOLEAN) {
				if(!array) val= RNA_property_boolean_get(&ptr, prop);
				else val= RNA_property_boolean_get_index(&ptr, prop, array_index);
				
				BLI_strncpy(expression, (val)? "True": "False", maxlen);
			}
			else if (proptype == PROP_INT) {
				if (!array) val= RNA_property_int_get(&ptr, prop);
				else val= RNA_property_int_get_index(&ptr, prop, array_index);
				
				BLI_snprintf(expression, maxlen, "%d", val);
			}
			else if (proptype == PROP_FLOAT) {
				if (!array) fval= RNA_property_float_get(&ptr, prop);
				else fval= RNA_property_float_get_index(&ptr, prop, array_index);
				
				BLI_snprintf(expression, maxlen, "%.3f", fval);
			}

		}
	}
	
	/* done */
	return (fcu != NULL);
}

/* Main Driver Management API calls:
 * 	Remove the driver for the specified property on the given ID block (if available)
 */
short ANIM_remove_driver (struct ID *id, const char rna_path[], int array_index, short flag)
{
	AnimData *adt;
	FCurve *fcu;
	
	/* get F-Curve
	 * Note: here is one of the places where we don't want new F-Curve + Driver added!
	 * 		so 'add' var must be 0
	 */
	/* we don't check the validity of the path here yet, but it should be ok... */
	fcu= verify_driver_fcurve(id, rna_path, array_index, 0);
	adt= BKE_animdata_from_id(id);
	
	/* only continue if we have an driver to remove */
	if (adt && fcu) {
		/* remove F-Curve from driver stack, then free it */
		BLI_remlink(&adt->drivers, fcu);
		free_fcurve(fcu);
		
		/* done successfully */
		return 1;
	}
	
	/* failed */
	return 0;
}


/* ************************************************** */
/* UI-Button Interface */

/* Add Driver Button Operator ------------------------ */

static int add_driver_button_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop= NULL;
	char *path;
	short success= 0;
	int a, index, length, all= RNA_boolean_get(op->ptr, "all");
	
	/* try to create driver using property retrieved from UI */
	memset(&ptr, 0, sizeof(PointerRNA));
	uiAnimContextProperty(C, &ptr, &prop, &index);
	
	if (ptr.data && prop && RNA_property_animateable(ptr.data, prop)) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
		
		if (path) {
			if (all) {
				length= RNA_property_array_length(&ptr, prop);
				
				if (length) index= 0;
				else length= 1;
			}
			else
				length= 1;
			
			for (a=0; a<length; a++)
				success+= ANIM_add_driver(ptr.id.data, path, index+a, 0, DRIVER_TYPE_PYTHON);
			
			MEM_freeN(path);
		}
	}
	
	if (success) {
		/* send updates */
		ED_anim_dag_flush_update(C);	
		
		/* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, ND_KEYS, NULL); // XXX
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void ANIM_OT_add_driver_button (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Driver";
	ot->idname= "ANIM_OT_add_driver_button";
	
	/* callbacks */
	ot->exec= add_driver_button_exec; 
	//op->poll= ???
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Create drivers for all elements of the array.");
}

/* Remove Driver Button Operator ------------------------ */

static int remove_driver_button_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	PropertyRNA *prop= NULL;
	char *path;
	short success= 0;
	int a, index, length, all= RNA_boolean_get(op->ptr, "all");
	
	/* try to find driver using property retrieved from UI */
	memset(&ptr, 0, sizeof(PointerRNA));
	uiAnimContextProperty(C, &ptr, &prop, &index);

	if (ptr.data && prop) {
		path= RNA_path_from_ID_to_property(&ptr, prop);
		
		if (path) {
			if (all) {
				length= RNA_property_array_length(&ptr, prop);
				
				if(length) index= 0;
				else length= 1;
			}
			else
				length= 1;
			
			for (a=0; a<length; a++)
				success+= ANIM_remove_driver(ptr.id.data, path, index+a, 0);
			
			MEM_freeN(path);
		}
	}
	
	
	if (success) {
		/* send updates */
		ED_anim_dag_flush_update(C);	
		
		/* for now, only send ND_KEYS for KeyingSets */
		WM_event_add_notifier(C, ND_KEYS, NULL);  // XXX
	}
	
	return (success)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void ANIM_OT_remove_driver_button (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Driver";
	ot->idname= "ANIM_OT_remove_driver_button";
	
	/* callbacks */
	ot->exec= remove_driver_button_exec; 
	//op->poll= ???
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "all", 1, "All", "Delete drivers for all elements of the array.");
}

/* ************************************************** */
