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

/* vec==NULL: init
   vec is supposed to be local coord, deform happens in local space
*/

void hook_object_deform(Object *ob, int index, float *vec)
{
	float totforce;
	ObHook *hook;
	float vect[3], vectot[3];
	
	if(ob->hooks.first==NULL) return;
	
	/* reinitialize if... */
	if(vec==NULL) {
		totforce= 0.0;
		for(hook= ob->hooks.first; hook; hook= hook->next) {
			if(hook->parent) {
				hook->curindex= 0;
				Mat4Invert(ob->imat, ob->obmat);
				/* apparently this call goes from right to left... */
				Mat4MulSerie(hook->mat, ob->imat, hook->parent->obmat, hook->parentinv, NULL, 
							NULL, NULL, NULL, NULL);
			}
		}
		return;
	}

	totforce= 0.0;
	vectot[0]= vectot[1]= vectot[2]= 0.0;
	
	for(hook= ob->hooks.first; hook; hook= hook->next) {
		if(hook->parent) {
			
			/* is 'index' in hook array? */
			while(hook->curindex < hook->totindex-1) {
				if( hook->indexar[hook->curindex] < index ) hook->curindex++;
				else break;
			}
			
			if( hook->indexar[hook->curindex]==index ) {
				float fac= hook->force, len;
				
				VecMat4MulVecfl(vect, hook->mat, vec);

				if(hook->falloff!=0.0) {
					/* hook->cent is in local coords */
					len= VecLenf(vec, hook->cent);
					if(len > hook->falloff) fac= 0.0;
					else if(len>0.0) fac*= sqrt(1.0 - len/hook->falloff);
				}
				if(fac!=0.0) {
					totforce+= fac;
					vectot[0]+= fac*vect[0];
					vectot[1]+= fac*vect[1];
					vectot[2]+= fac*vect[2];
				}
			}
		}
	}

	/* if totforce < 1.0, we take old position also into account */
	if(totforce<1.0) {
		vectot[0]+= (1.0-totforce)*vec[0];
		vectot[1]+= (1.0-totforce)*vec[1];
		vectot[2]+= (1.0-totforce)*vec[2];
	}
	else VecMulf(vectot, 1.0/totforce);
	
	VECCOPY(vec, vectot);
}


void mesh_modifier(Object *ob, float (**vertexCos_r)[3])
{
	MVert *origMVert=NULL;
	Mesh *me= ob->data;
	float (*vertexCos)[3] = NULL;
	int a;

	do_mesh_key(me);

	/* hooks */
	if(ob->hooks.first) {
		if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
		
		/* NULL signals initialize */
		hook_object_deform(ob, 0, NULL);
		
		for(a=0; a<me->totvert; a++) {
			hook_object_deform(ob, a, vertexCos[a]);
		}
	}
		
	if(ob->effect.first) {
		WaveEff *wav;
		float ctime = bsystem_time(ob, 0, (float)G.scene->r.cfra, 0.0);
		int a;
	
		for (wav= ob->effect.first; wav; wav= wav->next) {
			if(wav->type==EFF_WAVE) {
				if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
				init_wave_deform(wav);

				for(a=0; a<me->totvert; a++) {
					calc_wave_deform(wav, ctime, vertexCos[a]);
				}
			}
		}
	}

	if((ob->softflag & OB_SB_ENABLE) && !(ob->softflag & OB_SB_POSTDEF)) {
		if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
		sbObjectStep(ob, (float)G.scene->r.cfra, vertexCos);
	}

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
				// misleading making displists... very bad
			if (ob->parent!=G.obedit) {
				if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
				armature_deform_verts(ob->parent, ob, vertexCos, me->totvert);
			}
		}
	}

	if((ob->softflag & OB_SB_ENABLE) && (ob->softflag & OB_SB_POSTDEF)) {
		if (!vertexCos) vertexCos = mesh_getVertexCos(me, NULL);
		sbObjectStep(ob, (float)G.scene->r.cfra, vertexCos);
	}

	*vertexCos_r = vertexCos;
}

int curve_modifier(Object *ob, char mode)
{
	static ListBase nurb={NULL, NULL};
	Curve *cu= ob->data;
	Nurb *nu, *newnu;
	BezTriple *bezt;
	BPoint *bp;
	int a, index, done= 0;
	
	do_curve_key(cu);
	
	/* conditions if it's needed */
	if(ob->hooks.first);
	else if(ob->parent && ob->partype==PARSKEL); 
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
		
		/* hooks */
		if(ob->hooks.first) {
			done= 1;
			
			/* NULL signals initialize */
			hook_object_deform(ob, 0, NULL);
			index= 0;
			
			nu= cu->nurb.first;
			while(nu) {
				if((nu->type & 7)==CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						hook_object_deform(ob, index++, bezt->vec[0]);
						hook_object_deform(ob, index++, bezt->vec[1]);
						hook_object_deform(ob, index++, bezt->vec[2]);
						bezt++;
					}
				}
				else {
					bp= nu->bp;
					a= nu->pntsu*nu->pntsv;
					while(a--) {
						hook_object_deform(ob, index++, bp->vec);
						bp++;
					}
				}
					
				nu= nu->next;
			}
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
	static BPoint *bpoint;
	Lattice *lt= ob->data;
	BPoint *bp;
	int a, index, done= 0;
	
	do_latt_key(lt);
	
	/* conditions if it's needed */
	if(ob->hooks.first);
	else if(ob->parent && ob->partype==PARSKEL); 
	else if((ob->softflag & OB_SB_ENABLE));
	else return 0;
	
	if(mode=='s') { // "start"
		/* copy  */
		bpoint= MEM_dupallocN(lt->def);
		
		/* hooks */
		if(ob->hooks.first) {
			done= 1;
			
			/* NULL signals initialize */
			hook_object_deform(ob, 0, NULL);
			index= 0;
			bp= lt->def;
			a= lt->pntsu*lt->pntsv*lt->pntsw;
			while(a--) {
				hook_object_deform(ob, index++, bp->vec);
				bp++;
			}
		}
		
		if((ob->softflag & OB_SB_ENABLE)) {
			sbObjectStep(ob, (float)G.scene->r.cfra, NULL);
		}
		
	}
	else { // end
		MEM_freeN(lt->def);
		lt->def= bpoint;
		bpoint= NULL;
	}
	
	return done;
}

