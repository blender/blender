/*  deform.c   June 2001
 *  
 *  support for deformation groups
 * 
 *	Reevan McKay
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"

#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_mesh.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


void copy_defgroups (ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	outbase->first= outbase->last= 0;

	for (defgroup = inbase->first; defgroup; defgroup=defgroup->next){
		defgroupn= copy_defgroup(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup *copy_defgroup (bDeformGroup *ingroup)
{
	bDeformGroup *outgroup;

	if (!ingroup)
		return NULL;

	outgroup=MEM_callocN(sizeof(bDeformGroup), "copy deformGroup");
	
	/* For now, just copy everything over. */
	memcpy (outgroup, ingroup, sizeof(bDeformGroup));

	outgroup->next=outgroup->prev=NULL;

	return outgroup;
}

bDeformGroup *get_named_vertexgroup (Object *ob, char *name)
{
	/* return a pointer to the deform group with this name
	 * or return NULL otherwise.
	 */
	bDeformGroup *curdef;

	for (curdef = ob->defbase.first; curdef; curdef=curdef->next) {
		if (!strcmp(curdef->name, name)) {
			return curdef;
		}
	}
	return NULL;
}

int get_named_vertexgroup_num (Object *ob, char *name)
{
	/* Return the location of the named deform group within the list of
	 * deform groups. This function is a combination of get_defgroup_num and
	 * get_named_vertexgroup. The other two could be called instead, but that
	 * require looping over the vertexgroups twice.
	 */
	bDeformGroup *curdef;
	int def_nr;
	
	for (curdef=ob->defbase.first, def_nr=0; curdef; curdef=curdef->next, def_nr++) {
		if (!strcmp(curdef->name, name))
			return def_nr;
	}
	
	return -1;
}

int get_defgroup_num (Object *ob, bDeformGroup *dg)
{
	/* Fetch the location of this deform group
	 * within the linked list of deform groups.
	 * (this number is stored in the deform
	 * weights of the deform verts to link them
	 * to this deform group).
	 */

	bDeformGroup *eg;
	int def_nr;

	eg = ob->defbase.first;
	def_nr = 0;

	/* loop through all deform groups */
	while (eg != NULL) {

		/* if the current deform group is
		 * the one we are after, return
		 * def_nr
		 */
		if (eg == dg) {
			break;
		}
		++def_nr;
		eg = eg->next;
	}

	/* if there was no deform group found then
	 * return -1 (should set up a nice symbolic
	 * constant for this)
	 */
	if (eg == NULL) return -1;
	
	return def_nr;
    
}

void unique_vertexgroup_name (bDeformGroup *dg, Object *ob)
{
	bDeformGroup *curdef;
	int number;
	int exists = 0;
	char tempname[64];
	char *dot;
	
	if (!ob)
		return;
		
	/* See if we are given an empty string */
	if (dg->name[0] == '\0') {
		/* give it default name first */
		strcpy (dg->name, "Group");
	}	
		
	/* See if we even need to do this */
	for (curdef = ob->defbase.first; curdef; curdef=curdef->next) {
		if (dg!=curdef) {
			if (!strcmp(curdef->name, dg->name)) {
				exists = 1;
				break;
			}
		}
	}
	
	if (!exists)
		return;

	/*	Strip off the suffix */
	dot=strchr(dg->name, '.');
	if (dot)
		*dot=0;
	
	for (number = 1; number <=999; number++) {
		sprintf (tempname, "%s.%03d", dg->name, number);
		
		exists = 0;
		for (curdef=ob->defbase.first; curdef; curdef=curdef->next) {
			if (dg!=curdef) {
				if (!strcmp (curdef->name, tempname)) {
					exists = 1;
					break;
				}
			}
		}
		if (!exists) {
			BLI_strncpy (dg->name, tempname, 32);
			return;
		}
	}	
}

float deformvert_get_weight(const struct MDeformVert *dvert, int group_num)
{
	if(dvert)
	{
		const MDeformWeight *dw = dvert->dw;
		int i;

		for(i=dvert->totweight; i>0; i--, dw++)
			if(dw->def_nr == group_num)
				return dw->weight;
	}

	/* Not found */
	return 0.0;
}

float vertexgroup_get_vertex_weight(const struct MDeformVert *dvert, int index, int group_num)
{
	if(group_num == -1)
		return 1.0;

	if(dvert == 0)
		return 0.0;

	return deformvert_get_weight(dvert+index, group_num);
}

