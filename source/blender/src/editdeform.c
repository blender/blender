/**
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
 * Creator-specific support for vertex deformation groups
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_global.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"

#include "BIF_editdeform.h"
#include "BIF_toolbox.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void sel_verts_defgroup (int select)
{
	EditVert		*eve;
	Object			*ob;
	int				i;

	ob=G.obedit;

	if (!ob)
		return;

	switch (ob->type){
	case OB_MESH:
		for (eve=G.edve.first; eve; eve=eve->next){
			if (eve->totweight){
				for (i=0; i<eve->totweight; i++){
					if (eve->dw[i].def_nr == (ob->actdef-1)){
						if (select) eve->f |= 1;
						else eve->f &= ~1;
						break;
					}
				}
			}
		}
		break;
	default:
		break;
	}

}

MDeformWeight *verify_defweight (MDeformVert *dv, int defgroup)
/* Ensures that mv has a deform weight entry for
the specified defweight group */
{
	int	i;
	MDeformWeight *newdw;

	if (!dv)
		return NULL;

	for (i=0; i<dv->totweight; i++){
		if (dv->dw[i].def_nr == defgroup)
			return dv->dw+i;
	}

	newdw = MEM_callocN (sizeof(MDeformWeight)*(dv->totweight+1), "deformWeight");
	if (dv->dw){
		memcpy (newdw, dv->dw, sizeof(MDeformWeight)*dv->totweight);
		MEM_freeN (dv->dw);
	}
	dv->dw=newdw;
	
	dv->dw[dv->totweight].weight=0;
	dv->dw[dv->totweight].def_nr=defgroup;
	/* Group index */
	
	dv->totweight++;

	return dv->dw+(dv->totweight-1);
}

void add_defgroup (Object *ob)
{
	bDeformGroup	*defgroup;

	if (!ob)
		return;

	defgroup = MEM_callocN (sizeof(bDeformGroup), "deformGroup");
	strcpy (defgroup->name, "Group");

	BLI_addtail(&ob->defbase, defgroup);
	unique_vertexgroup_name(defgroup, ob);

	ob->actdef = BLI_countlist(&ob->defbase);
}

void del_defgroup (Object *ob)
{
	bDeformGroup	*defgroup;
	EditVert		*eve;
	int				i;


	if (!ob)
		return;

	if (!ob->actdef)
		return;

	defgroup = BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!defgroup)
		return;

	/* Make sure that no verts are using this group */
	remove_verts_defgroup(1);

	/* Make sure that any verts with higher indices are adjusted accordingly */
	for (eve=G.edve.first; eve; eve=eve->next){
		for (i=0; i<eve->totweight; i++){
			if (eve->dw[i].def_nr > (ob->actdef-1))
				eve->dw[i].def_nr--;
		}
	}

	/* Update the active material index if necessary */
	if (ob->actdef==BLI_countlist(&ob->defbase))
		ob->actdef--;

	/* Remove the group */
	BLI_freelinkN (&ob->defbase, defgroup);
}

void assign_verts_defgroup (void)
/* Only available in editmode */
{
	Object *ob;
	EditVert *eve;
	bDeformGroup	*dg, *eg;
	extern float editbutvweight;	/* buttons.c */
	int	i, done;
	MDeformWeight *newdw;

	ob=G.obedit;

	if (!ob)
		return;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!dg){
		error ("No vertex group active");
		return;
	}

	switch (ob->type){
	case OB_MESH:
		/* Go through the list of editverts and assign them */
		for (eve=G.edve.first; eve; eve=eve->next){
			if (eve->f & 1){
				done=0;
				/* See if this vert already has a reference to this group */
				/*		If so: Change its weight */
				done=0;
				for (i=0; i<eve->totweight; i++){
					eg = BLI_findlink (&ob->defbase, eve->dw[i].def_nr);
					/* Find the actual group */
					if (eg==dg){
						eve->dw[i].weight=editbutvweight;
						done=1;
						break;
					}
			 	}
				/*		If not: Add the group and set its weight */
				if (!done){
					newdw = MEM_callocN (sizeof(MDeformWeight)*(eve->totweight+1), "deformWeight");
					if (eve->dw){
						memcpy (newdw, eve->dw, sizeof(MDeformWeight)*eve->totweight);
						MEM_freeN (eve->dw);
					}
					eve->dw=newdw;

					eve->dw[eve->totweight].weight=editbutvweight;
					eve->dw[eve->totweight].def_nr=ob->actdef-1;

					eve->totweight++;

				}
			}
		}
		break;
	default:
		printf ("Assigning deformation groups to unknown object type: Warn <reevan@blender.nl>\n");
		break;
	}

}

void remove_verts_defgroup (int allverts)
/* Only available in editmode */
{
	Object *ob;
	EditVert *eve;
	MDeformWeight *newdw;
	bDeformGroup	*dg, *eg;
	int	i;

	ob=G.obedit;

	if (!ob)
		return;

	dg=BLI_findlink(&ob->defbase, ob->actdef-1);
	if (!dg){
		error ("No vertex group active");
		return;
	}

	switch (ob->type){
	case OB_MESH:
		for (eve=G.edve.first; eve; eve=eve->next){
			if (eve->dw && ((eve->f & 1) || allverts)){
				for (i=0; i<eve->totweight; i++){
					/* Find group */
					eg = BLI_findlink (&ob->defbase, eve->dw[i].def_nr);
					if (eg == dg){
						eve->totweight--;
						if (eve->totweight){
							newdw = MEM_mallocN (sizeof(MDeformWeight)*(eve->totweight), "deformWeight");
							
							if (eve->dw){
								memcpy (newdw, eve->dw, sizeof(MDeformWeight)*i);
								memcpy (newdw+i, eve->dw+i+1, sizeof(MDeformWeight)*(eve->totweight-i));
								MEM_freeN (eve->dw);
							}
							eve->dw=newdw;
						}
						else{
							MEM_freeN (eve->dw);
							eve->dw=NULL;
						}
					}
				}
			}
		}
		break;
	default:
		printf ("Removing deformation groups from unknown object type: Warn <reevan@blender.nl>\n");
		break;
	}
}

void verify_defgroups (Object *ob)
{
		/* Ensure the defbase & the dverts match */
	switch (ob->type){
	case OB_MESH:
		if (!ob->defbase.first){
			if (((Mesh*)ob->data)->dvert){
				free_dverts(((Mesh*)ob->data)->dvert, ((Mesh*)ob->data)->totvert);
				((Mesh*)ob->data)->dvert=NULL;
			}
		}
		break;
	default:
		break;
	}
}

void unique_vertexgroup_name (bDeformGroup *dg, Object *ob)
{
	char		tempname[64];
	int			number;
	char		*dot;
	int exists = 0;
	bDeformGroup *curdef;
	
	if (!ob)
		return;
	/* See if we even need to do this */
	for (curdef = ob->defbase.first; curdef; curdef=curdef->next){
		if (dg!=curdef){
			if (!strcmp(curdef->name, dg->name)){
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
	
	for (number = 1; number <=999; number++){
		sprintf (tempname, "%s.%03d", dg->name, number);
		
		exists = 0;
		for (curdef=ob->defbase.first; curdef; curdef=curdef->next){
			if (dg!=curdef){
				if (!strcmp (curdef->name, tempname)){
					exists = 1;
					break;
				}
			}
		}
		if (!exists){
			strcpy (dg->name, tempname);
			return;
		}
	}	
}
