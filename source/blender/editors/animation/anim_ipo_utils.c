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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2005. Full recode
 *				   Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_ipo_utils.c
 *  \ingroup edanimation
 */


/* This file contains code for presenting F-Curves and other animation data
 * in the UI (especially for use in the Animation Editors).
 *
 * -- Joshua Leung, Dec 2008
 */


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"

#include "RNA_access.h"

#include "ED_anim_api.h"

/* ----------------------- Getter functions ----------------------- */

/* Write into "name" buffer, the name of the property (retrieved using RNA from the curve's settings),
 * and return the icon used for the struct that this property refers to 
 * WARNING: name buffer we're writing to cannot exceed 256 chars (check anim_channels_defines.c for details)
 */
int getname_anim_fcurve(char *name, ID *id, FCurve *fcu)
{
	int icon = 0;
	
	/* sanity checks */
	if (name == NULL)
		return icon;
	else if (ELEM3(NULL, id, fcu, fcu->rna_path)) {
		if (fcu == NULL)
			strcpy(name, "<invalid>");
		else if (fcu->rna_path == NULL)
			strcpy(name, "<no path>");
		else /* id == NULL */
			BLI_snprintf(name, 256, "%s[%d]", fcu->rna_path, fcu->array_index);
	}
	else {
		PointerRNA id_ptr, ptr;
		PropertyRNA *prop;
		
		/* get RNA pointer, and resolve the path */
		RNA_id_pointer_create(id, &id_ptr);
		
		/* try to resolve the path */
		if (RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop)) {
			const char *structname=NULL, *propname=NULL;
			char arrayindbuf[16];
			const char *arrayname=NULL;
			short free_structname = 0;
			
			/* For now, name will consist of 3 parts: struct-name, property name, array index
			 * There are several options possible:
			 *	1) <struct-name>.<property-name>.<array-index>
			 *		i.e. Bone1.Location.X, or Object.Location.X
			 *	2) <array-index> <property-name> (<struct name>)
			 *		i.e. X Location (Bone1), or X Location (Object)
			 *	
			 * Currently, option 2 is in use, to try and make it easier to quickly identify F-Curves (it does have
			 * problems with looking rather odd though). Option 1 is better in terms of revealing a consistent sense of 
			 * hierarchy though, which isn't so clear with option 2.
			 */
			
			/* for structname
			 *	- as base, we use a custom name from the structs if one is available 
			 *	- however, if we're showing subdata of bones (probably there will be other exceptions later)
			 *	  need to include that info too since it gets confusing otherwise
			 *	- if a pointer just refers to the ID-block, then don't repeat this info
			 *	  since this just introduces clutter
			 */
			if (strstr(fcu->rna_path, "bones") && strstr(fcu->rna_path, "constraints")) {
				/* perform string 'chopping' to get "Bone Name : Constraint Name" */
				char *pchanName= BLI_getQuotedStr(fcu->rna_path, "bones[");
				char *constName= BLI_getQuotedStr(fcu->rna_path, "constraints[");
				
				/* assemble the string to display in the UI... */
				structname= BLI_sprintfN("%s : %s", pchanName, constName);
				free_structname= 1;
				
				/* free the temp names */
				if (pchanName) MEM_freeN(pchanName);
				if (constName) MEM_freeN(constName);
			}
			else if (ptr.data != ptr.id.data) {
				PropertyRNA *nameprop= RNA_struct_name_property(ptr.type);
				if (nameprop) {
					/* this gets a string which will need to be freed */
					structname= RNA_property_string_get_alloc(&ptr, nameprop, NULL, 0, NULL);
					free_structname= 1;
				}
				else
					structname= RNA_struct_ui_name(ptr.type);
			}
			
			/* Property Name is straightforward */
			propname= RNA_property_ui_name(prop);
			
			/* Array Index - only if applicable */
			if (RNA_property_array_length(&ptr, prop)) {
				char c= RNA_property_array_item_char(prop, fcu->array_index);
				
				/* we need to write the index to a temp buffer (in py syntax) */
				if (c) BLI_snprintf(arrayindbuf, sizeof(arrayindbuf), "%c ", c);
				else BLI_snprintf(arrayindbuf, sizeof(arrayindbuf), "[%d]", fcu->array_index);
				
				arrayname= &arrayindbuf[0];
			}
			else {
				/* no array index */
				arrayname= "";
			}
			
			/* putting this all together into the buffer */
			// XXX we need to check for invalid names...
			// XXX the name length limit needs to be passed in or as some define
			if (structname)
				BLI_snprintf(name, 256, "%s%s (%s)", arrayname, propname, structname); 
			else
				BLI_snprintf(name, 256, "%s%s", arrayname, propname); 
			
			/* free temp name if nameprop is set */
			if (free_structname)
				MEM_freeN((void *)structname);
			
			
			/* Icon for this property's owner:
			 *	use the struct's icon if it is set
			 */
			icon= RNA_struct_ui_icon(ptr.type);
			
			/* valid path - remove the invalid tag since we now know how to use it saving
			 * users manual effort to reenable using "Revive Disabled FCurves" [#29629]
			 */
			fcu->flag &= ~FCURVE_DISABLED;
		}
		else {
			/* invalid path */
			BLI_snprintf(name, 256, "\"%s[%d]\"", fcu->rna_path, fcu->array_index);
			
			/* icon for this should be the icon for the base ID */
			// TODO: or should we just use the error icon?
			icon= RNA_struct_ui_icon(id_ptr.type);
			
			/* tag F-Curve as disabled - as not usable path */
			fcu->flag |= FCURVE_DISABLED;
		}
	}
	
	/* return the icon that the active data had */
	return icon;
}

/* ------------------------------- Color Codes for F-Curve Channels ---------------------------- */

/* step between the major distinguishable color bands of the primary colors */
#define HSV_BANDWIDTH	0.3f

/* used to determine the color of F-Curves with FCURVE_COLOR_AUTO_RAINBOW set */
//void fcurve_rainbow (unsigned int cur, unsigned int tot, float *out)
void getcolor_fcurve_rainbow(int cur, int tot, float *out)
{
	float hue, val, sat, fac;
	int grouping;
	
	/* we try to divide the color into groupings of n colors,
	 * where n is:
	 *	3 - for 'odd' numbers of curves - there should be a majority of triplets of curves
	 *	4 - for 'even' numbers of curves - there should be a majority of quartets of curves
	 * so the base color is simply one of the three primary colors
	 */
	grouping= (4 - (tot % 2));
	hue= HSV_BANDWIDTH * (float)(cur % grouping);
	
	/* 'Value' (i.e. darkness) needs to vary so that larger sets of three will be 
	 * 'darker' (i.e. smaller value), so that they don't look that similar to previous ones.
	 * However, only a range of 0.3 to 1.0 is really usable to avoid clashing
	 * with some other stuff 
	 */
	fac = ((float)cur / (float)tot) * 0.7f;
	
	/* the base color can get offset a bit so that the colors aren't so identical */
	hue += fac * HSV_BANDWIDTH; 
	if (hue > 1.0f) hue= fmod(hue, 1.0f);
	
	/* saturation adjustments for more visible range */
	if ((hue > 0.5f) && (hue < 0.8f)) sat= 0.5f;
	else sat= 0.6f;
	
	/* value is fixed at 1.0f, otherwise we cannot clearly see the curves... */
	val= 1.0f;
	
	/* finally, conver this to RGB colors */
	hsv_to_rgb(hue, sat, val, out, out+1, out+2); 
}
