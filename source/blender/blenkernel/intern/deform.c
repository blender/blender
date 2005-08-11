/*  deform.c   June 2001
 *  
 *  support for deformation groups
 * 
 *	Reevan McKay
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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


void copy_defgroups(ListBase *outbase, ListBase *inbase)
{
	bDeformGroup *defgroup, *defgroupn;

	outbase->first= outbase->last= 0;

	for (defgroup = inbase->first; defgroup; defgroup=defgroup->next){
		defgroupn= copy_defgroup(defgroup);
		BLI_addtail(outbase, defgroupn);
	}
}

bDeformGroup* copy_defgroup (bDeformGroup *ingroup)
{
	bDeformGroup *outgroup;

	if (!ingroup)
		return NULL;

	outgroup=MEM_callocN(sizeof(bDeformGroup), "deformGroup");
	
	/* For now, just copy everything over. */
	memcpy (outgroup, ingroup, sizeof(bDeformGroup));

	outgroup->next=outgroup->prev=NULL;

	return outgroup;
}

bDeformGroup *get_named_vertexgroup(Object *ob, char *name)
{
	/* return a pointer to the deform group with this name
	 * or return NULL otherwise.
	 */
	bDeformGroup *curdef;

	for (curdef = ob->defbase.first; curdef; curdef=curdef->next){
		if (!strcmp(curdef->name, name)){
			return curdef;
		}
	}
	return NULL;
}

int  get_defgroup_num (Object *ob, bDeformGroup	*dg)
{
	/* Fetch the location of this deform group
	 * within the linked list of deform groups.
	 * (this number is stored in the deform
	 * weights of the deform verts to link them
	 * to this deform group) deform deform
	 * deform blah blah deform
	 */

	bDeformGroup	*eg;
	int def_nr;

	eg = ob->defbase.first;
	def_nr = 0;

	/* loop through all deform groups
	 */
	while (eg != NULL){

		/* if the current deform group is
		 * the one we are after, return
		 * def_nr
		 */
		if (eg == dg){
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

/* *************** HOOK ****************** */

void mesh_modifier(Object *ob, float (**vertexCos_r)[3])
{
	Mesh *me= ob->data;
	float (*vertexCos)[3] = NULL;

	do_mesh_key(me);

	if (ob->parent && me->totvert) {
		if(ob->parent->type==OB_CURVE && ob->partype==PARSKEL) {
			if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
			curve_deform_verts(ob->parent, ob, vertexCos, me->totvert);
		}
		else if(ob->parent->type==OB_LATTICE) {
			if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
			lattice_deform_verts(ob->parent, ob, vertexCos, me->totvert);
		}
		else if(ob->parent->type==OB_ARMATURE && ob->partype==PARSKEL) {
			if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
			armature_deform_verts(ob->parent, ob, vertexCos, me->totvert);
		}
	}

	*vertexCos_r = vertexCos;
}

int curve_modifier(Object *ob, char mode)
{
	static ListBase nurb={NULL, NULL};
	Curve *cu= ob->data;
	Nurb *nu, *newnu;
	int done= 0;
	
	do_curve_key(cu);
	
	/* conditions if it's needed */
	if(ob->parent && ob->partype==PARSKEL); 
	else if(ob->parent && ob->parent->type==OB_LATTICE);
	else return 0;
	
	if(mode=='s') { // "start"
		/* copy  */
		nurb.first= nurb.last= NULL;	
		nu= cu->nurb.first;
		while(nu) {
			newnu= duplicateNurb(nu);
			BLI_addtail(&nurb, newnu);
			nu= nu->next;
		}
	}
	else if(mode=='e') {
		/* paste */
		freeNurblist(&cu->nurb);
		cu->nurb= nurb;
	}
	else if(mode=='a') {
		freeNurblist(&nurb);
	}
	
	return done;
}

int lattice_modifier(Object *ob, char mode)
{
	Lattice *lt = ob->data;

	do_latt_key(lt);
	
	return 0;
}

