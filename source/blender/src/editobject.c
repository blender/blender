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
 */

/** 
 * Theorie: (matrices) A x B x C == A x ( B x C x Binv) x B
 * ofwel: OB x PAR x EDIT = OB x (PAR x EDIT x PARinv) x PAR
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include "MEM_guardedalloc.h"
#include "PIL_time.h"

#include "BMF_Api.h"


#include "IMB_imbuf_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_property_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_vfont_types.h"
#include "DNA_world_types.h"
#include "DNA_modifier_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_blender.h"
#include "BKE_booleanops.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_property.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_butspace.h"
#include "BIF_editconstraint.h"
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editlattice.h"
#include "BIF_editmesh.h"
#include "BIF_editoops.h"
#include "BIF_editview.h"
#include "BIF_editarmature.h"
#include "BIF_resources.h"

#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_filesel.h"	/* For activate_databrowse() */
#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_trans_types.h"
#include "BSE_editipo_types.h"

#include "BDR_vpaint.h"
#include "BDR_editmball.h"
#include "BDR_editobject.h"
#include "BDR_drawobject.h"
#include "BDR_editcurve.h"
#include "BDR_unwrapper.h"

#include <time.h>
#include "mydevice.h"
#include "nla.h"

#include "blendef.h"
#include "butspace.h"
#include "BIF_transform.h"

#include "BIF_poseobject.h"

/* used in editipo, editcurve and here */
#define BEZSELECTED(bezt)   (((bezt)->f1 & 1) || ((bezt)->f2 & 1) || ((bezt)->f3 & 1))

/* local prototypes -------------*/

/* --------------------------------- */

void add_object_draw(int type)	/* for toolbox or menus, only non-editmode stuff */
{
	Object *ob;
	
	G.f &= ~(G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT+G_WEIGHTPAINT);
	setcursor_space(SPACE_VIEW3D, CURSOR_STD);

	if ELEM3(curarea->spacetype, SPACE_VIEW3D, SPACE_BUTS, SPACE_INFO) {
		if (G.obedit) exit_editmode(2); // freedata, and undo
		ob= add_object(type);
		base_init_from_view3d(BASACT, G.vd);
		
		/* only undo pushes on objects without editmode... */
		if(type==OB_EMPTY) BIF_undo_push("Add Empty");
		else if(type==OB_LAMP) {
			BIF_undo_push("Add Lamp");
			if(G.vd->drawtype == OB_SHADED) reshadeall_displist();
		}
		else if(type==OB_LATTICE) BIF_undo_push("Add Lattice");
		else if(type==OB_CAMERA) BIF_undo_push("Add Camera");
		
		allqueue(REDRAWVIEW3D, 0);
	}

	redraw_test_buttons(OBACT);

	allqueue(REDRAWALL, 0);

	deselect_all_area_oops();
	set_select_flag_oops();
	
	DAG_scene_sort(G.scene);
	allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
}

void add_objectLamp(short type)
{
	Lamp *la;

	/* this function also comes from an info window */
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;
	
	if(G.obedit==0) {
		add_object_draw(OB_LAMP);
		base_init_from_view3d(BASACT, G.vd);
	}
	
	la = BASACT->object->data;
	la->type = type;	

	allqueue(REDRAWALL, 0);
}

/* note: now unlinks constraints as well */
void free_and_unlink_base(Base *base)
{
	if (base==BASACT)
		BASACT= NULL;
	
	BLI_remlink(&G.scene->base, base);
	free_libblock_us(&G.main->object, base->object);
	MEM_freeN(base);
}

void delete_obj(int ok)
{
	Base *base;
	int islamp= 0;
	
	if(G.obedit) return;
	if(G.scene->id.lib) return;
	
	base= FIRSTBASE;
	while(base) {
		Base *nbase= base->next;

		if TESTBASE(base) {
			if(ok==0 &&  (ok=okee("Erase selected Object(s)"))==0) return;
			if(base->object->type==OB_LAMP) islamp= 1;
			
			free_and_unlink_base(base);
		}
		
		base= nbase;
	}
	countall();

	G.f &= ~(G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT+G_WEIGHTPAINT);
	setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	
	if(islamp && G.vd->drawtype==OB_SHADED) reshadeall_displist();

	redraw_test_buttons(OBACT);
	allqueue(REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	
	DAG_scene_sort(G.scene);
	
	BIF_undo_push("Delete object(s)");
}

static int return_editmesh_indexar(int *tot, int **indexar, float *cent)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int *index, nr, totvert=0;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) totvert++;
	}
	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) {
			*index= nr; index++;
			VecAddf(cent, cent, eve->co);
		}
		nr++;
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static int return_editmesh_vgroup(char *name, float *cent)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int i, totvert=0;
	
	cent[0]= cent[1]= cent[2]= 0.0;
	
	if (G.obedit->actdef) {
		
		/* find the vertices */
		for(eve= em->verts.first; eve; eve= eve->next) {
			for (i=0; i<eve->totweight; i++){
				if(eve->dw[i].def_nr == (G.obedit->actdef-1)) {
					totvert++;
					VecAddf(cent, cent, eve->co);
				}
			}
		}
		if(totvert) {
			bDeformGroup *defGroup = BLI_findlink(&G.obedit->defbase, G.obedit->actdef-1);
			strcpy(name, defGroup->name);
			VecMulf(cent, 1.0f/(float)totvert);
			return 1;
		}
	}
	
	return 0;
}	

static void select_editmesh_hook(HookModifierData *hmd)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int index=0, nr=0;
	
	for(eve= em->verts.first; eve; eve= eve->next, nr++) {
		if(nr==hmd->indexar[index]) {
			eve->f |= SELECT;
			if(index < hmd->totindex-1) index++;
		}
	}
	EM_select_flush();
}

static int return_editlattice_indexar(int *tot, int **indexar, float *cent)
{
	BPoint *bp;
	int *index, nr, totvert=0, a;
	
	// count
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	bp= editLatt->def;
	while(a--) {
		if(bp->f1 & SELECT) {
			if(bp->hide==0) totvert++;
		}
		bp++;
	}

	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	bp= editLatt->def;
	while(a--) {
		if(bp->f1 & SELECT) {
			if(bp->hide==0) {
				*index= nr; index++;
				VecAddf(cent, cent, bp->vec);
			}
		}
		bp++;
		nr++;
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static void select_editlattice_hook(HookModifierData *hmd)
{
	BPoint *bp;
	int index=0, nr=0, a;
	
	// count
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	bp= editLatt->def;
	while(a--) {
		if(hmd->indexar[index]==nr) {
			bp->f1 |= SELECT;
			if(index < hmd->totindex-1) index++;
		}
		nr++;
		bp++;
	}
}

static int return_editcurve_indexar(int *tot, int **indexar, float *cent)
{
	extern ListBase editNurb;
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int *index, a, nr, totvert=0;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->f1 & SELECT) totvert++;
				if(bezt->f2 & SELECT) totvert++;
				if(bezt->f3 & SELECT) totvert++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->f1 & SELECT) totvert++;
				bp++;
			}
		}
	}
	if(totvert==0) return 0;
	
	*indexar= index= MEM_mallocN(4*totvert, "hook indexar");
	*tot= totvert;
	nr= 0;
	cent[0]= cent[1]= cent[2]= 0.0;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(bezt->f1 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[0]);
				}
				nr++;
				if(bezt->f2 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[1]);
				}
				nr++;
				if(bezt->f3 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bezt->vec[2]);
				}
				nr++;
				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(bp->f1 & SELECT) {
					*index= nr; index++;
					VecAddf(cent, cent, bp->vec);
				}
				nr++;
				bp++;
			}
		}
	}
	
	VecMulf(cent, 1.0f/(float)totvert);
	
	return totvert;
}

static void select_editcurve_hook(HookModifierData *hmd)
{
	extern ListBase editNurb;
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	int index=0, a, nr=0;
	
	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			bezt= nu->bezt;
			a= nu->pntsu;
			while(a--) {
				if(nr == hmd->indexar[index]) {
					bezt->f1 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				if(nr == hmd->indexar[index]) {
					bezt->f2 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				if(nr == hmd->indexar[index]) {
					bezt->f3 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;

				bezt++;
			}
		}
		else {
			bp= nu->bp;
			a= nu->pntsu*nu->pntsv;
			while(a--) {
				if(nr == hmd->indexar[index]) {
					bp->f1 |= SELECT;
					if(index<hmd->totindex-1) index++;
				}
				nr++;
				bp++;
			}
		}
	}
}

void hook_select(HookModifierData *hmd) 
{
	if(G.obedit->type==OB_MESH) select_editmesh_hook(hmd);
	else if(G.obedit->type==OB_LATTICE) select_editlattice_hook(hmd);
	else if(G.obedit->type==OB_CURVE) select_editcurve_hook(hmd);
	else if(G.obedit->type==OB_SURF) select_editcurve_hook(hmd);
}

int hook_getIndexArray(int *tot, int **indexar, char *name, float *cent_r)
{
	*indexar= NULL;
	*tot= 0;
	name[0]= 0;

	switch(G.obedit->type) {
	case OB_MESH:
		/* check selected vertices first */
		if( return_editmesh_indexar(tot, indexar, cent_r)) return 1;
		else return return_editmesh_vgroup(name, cent_r);
	case OB_CURVE:
	case OB_SURF:
		return return_editcurve_indexar(tot, indexar, cent_r);
	case OB_LATTICE:
		return return_editlattice_indexar(tot, indexar, cent_r);
	default:
		return 0;
	}
}

void add_hook(void)
{
	ModifierData *md = NULL;
	HookModifierData *hmd = NULL;
	Object *ob=NULL;
	int mode;

	if(G.obedit==NULL) return;
	
	if(modifiers_findByType(G.obedit, eModifierType_Hook))
		mode= pupmenu("Hooks %t|Add, To New Empty %x1|Add, To Selected Object %x2|Remove... %x3|Reassign... %x4|Select... %x5|Clear Offset...%x6");
	else
		mode= pupmenu("Hooks %t|Add, New Empty %x1|Add, To Selected Object %x2");

	if(mode<1) return;
	
	/* preconditions */

	if(mode==2) { // selected object
		Base *base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base!=BASACT) {
					ob= base->object;
					break;
				}
			}
			base= base->next;
		}
		if(ob==NULL) {
			error("Requires selected Object");
			return;
		}
	}
	else if(mode!=1) {
		int maxlen=0, a, nr;
		char *cp;
		
		// make pupmenu with hooks
		for(md=G.obedit->modifiers.first; md; md= md->next) {
			if (md->type==eModifierType_Hook) 
				maxlen+=32;
		}
		
		if(maxlen==0) {
			error("Object has no hooks yet");
			return;
		}
		
		cp= MEM_callocN(maxlen+32, "temp string");
		if(mode==3) strcpy(cp, "Remove %t|");
		else if(mode==4) strcpy(cp, "Reassign %t|");
		else if(mode==5) strcpy(cp, "Select %t|");
		else if(mode==6) strcpy(cp, "Clear Offset %t|");
		
		for(md=G.obedit->modifiers.first; md; md= md->next) {
			if (md->type==eModifierType_Hook) {
				strcat(cp, md->name);
				strcat(cp, " |");
			}
		}
	
		nr= pupmenu(cp);
		MEM_freeN(cp);
		
		if(nr<1) return;
		
		a= 1;
		for(md=G.obedit->modifiers.first; md; md=md->next) {
			if (md->type==eModifierType_Hook) {
				if(a==nr) break;
				a++;
			}
		}

		hmd = (HookModifierData*) md;
		ob= hmd->object;
	}

	/* do it, new hooks or reassign */
	if(mode==1 || mode==2 || mode==4) {
		float cent[3];
		int tot, ok, *indexar;
		char name[32];
		
		ok = hook_getIndexArray(&tot, &indexar, name, cent);

		if(ok==0) {
			error("Requires selected vertices or active Vertex Group");
		}
		else {
			
			if(mode==1) {
				Base *base= BASACT, *newbase;

				ob= add_object(OB_EMPTY);
				/* set layers OK */
				newbase= BASACT;
				newbase->lay= base->lay;
				ob->lay= newbase->lay;
				
				/* transform cent to global coords for loc */
				VecMat4MulVecfl(ob->loc, G.obedit->obmat, cent);
				
				/* restore, add_object sets active */
				BASACT= base;
			}
			/* if mode is 2 or 4, ob has been set */
									
			/* new hook */
			if(mode==1 || mode==2) {
				ModifierData *md = G.obedit->modifiers.first;

				while (md && modifierType_getInfo(md->type)->type==eModifierTypeType_OnlyDeform) {
					md = md->next;
				}

				hmd = (HookModifierData*) modifier_new(eModifierType_Hook);
				BLI_insertlinkbefore(&G.obedit->modifiers, md, hmd);
				sprintf(hmd->modifier.name, "Hook-%s", ob->id.name+2);
			}
			else if (hmd->indexar) MEM_freeN(hmd->indexar); // reassign, hook was set

			hmd->object= ob;
			hmd->indexar= indexar;
			VECCOPY(hmd->cent, cent);
			hmd->totindex= tot;
			BLI_strncpy(hmd->name, name, 32);
			
			if(mode==1 || mode==2) {
				/* matrix calculus */
				/* vert x (obmat x hook->imat) x hook->obmat x ob->imat */
				/*        (parentinv         )                          */
				
				where_is_object(ob);
		
				Mat4Invert(ob->imat, ob->obmat);
				/* apparently this call goes from right to left... */
				Mat4MulSerie(hmd->parentinv, ob->imat, G.obedit->obmat, NULL, 
							NULL, NULL, NULL, NULL, NULL);
			}
		}
	}
	else if(mode==3) { // remove
		BLI_remlink(&G.obedit->modifiers, md);
		modifier_free(md);
	}
	else if(mode==5) { // select
		hook_select(hmd);
	}
	else if(mode==6) { // clear offset
		where_is_object(ob);	// ob is hook->parent

		Mat4Invert(ob->imat, ob->obmat);
		/* this call goes from right to left... */
		Mat4MulSerie(hmd->parentinv, ob->imat, G.obedit->obmat, NULL, 
					NULL, NULL, NULL, NULL, NULL);
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	DAG_scene_sort(G.scene);
	
	BIF_undo_push("Add hook");
}

void make_track(void)
{
	Base *base;
	short mode=0;
	
	if(G.scene->id.lib) return;
	if(G.obedit) {
		return;
	}
	if(BASACT==0) return;

	mode= pupmenu("Make Track %t|TrackTo Constraint %x1|LockTrack Constraint %x2|Old Track %x3");
	if (mode == 0){
		return;
	}
	else if (mode == 1){
		bConstraint *con;
		bTrackToConstraint *data;

		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base!=BASACT) {
					con = add_new_constraint(CONSTRAINT_TYPE_TRACKTO);
					strcpy (con->name, "AutoTrack");

					data = con->data;
					data->tar = BASACT->object;
					base->object->recalc |= OB_RECALC;
					
					/* Lamp and Camera track differently by default */
					if (base->object->type == OB_LAMP || base->object->type == OB_CAMERA) {
						data->reserved1 = TRACK_nZ;
						data->reserved2 = UP_Y;
					}

					add_constraint_to_object(con, base->object);
				}
			}
			base= base->next;
		}

	}
	else if (mode == 2){
		bConstraint *con;
		bLockTrackConstraint *data;

		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base!=BASACT) {
					con = add_new_constraint(CONSTRAINT_TYPE_LOCKTRACK);
					strcpy (con->name, "AutoTrack");

					data = con->data;
					data->tar = BASACT->object;
					base->object->recalc |= OB_RECALC;
					
					/* Lamp and Camera track differently by default */
					if (base->object->type == OB_LAMP || base->object->type == OB_CAMERA) {
						data->trackflag = TRACK_nZ;
						data->lockflag = LOCK_Y;
					}

					add_constraint_to_object(con, base->object);
				}
			}
			base= base->next;
		}

	}
	else if (mode == 3){
		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base!=BASACT) {
					base->object->track= BASACT->object;
					base->object->recalc |= OB_RECALC;
				}
			}
			base= base->next;
		}
	}

	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWVIEW3D, 0);
	DAG_scene_sort(G.scene);
	
	BIF_undo_push("Make Track");
}

void apply_obmat(Object *ob)
{
	float mat[3][3], imat[3][3], tmat[3][3];
	
	/* from obmat to loc rot size */
	
	if(ob==0) return;
	Mat3CpyMat4(mat, ob->obmat);
	
	VECCOPY(ob->loc, ob->obmat[3]);
	
	if(ob->transflag & OB_QUAT) {
		Mat3ToQuat(mat, ob->quat);
		QuatToMat3(ob->quat, tmat);
	}
	else {
		Mat3ToEul(mat, ob->rot);
		EulToMat3(ob->rot, tmat);
	}
	Mat3Inv(imat, tmat);
	
	Mat3MulMat3(tmat, imat, mat);
	
	ob->size[0]= tmat[0][0];
	ob->size[1]= tmat[1][1];
	ob->size[2]= tmat[2][2];

}

void clear_parent(void)
{
	Object *par;
	Base *base;
	int mode;
	
	if(G.obedit) return;
	if(G.scene->id.lib) return;

	mode= pupmenu("OK? %t|Clear Parent %x1|Clear and Keep Transformation (Clear Track) %x2|Clear Parent Inverse %x3");
	
	if(mode<1) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			par= NULL;
			if(mode==1 || mode==2) {
				par= base->object->parent;
				base->object->parent= NULL;
				base->object->recalc |= OB_RECALC;
				
				if(mode==2) {
					base->object->track= NULL;
					apply_obmat(base->object);
				}
			}
			else if(mode==3) {
				Mat4One(base->object->parentinv);
				base->object->recalc |= OB_RECALC;
			}
		}
		base= base->next;
	}

	DAG_scene_sort(G.scene);
	DAG_scene_flush_update(G.scene, screen_view3d_layers());
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Clear Parent");	
}

void clear_track(void)
{
	Base *base;
	int mode;
	
	if(G.obedit) return;
	if(G.scene->id.lib) return;

	mode= pupmenu("OK? %t|Clear Track %x1| Clear Track and Keep Transform %x2");

	if(mode<1) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			base->object->track= NULL;
			base->object->recalc |= OB_RECALC;
			
			if(mode==2) {
				apply_obmat(base->object);
			}			
		}
		base= base->next;
	}

	DAG_scene_sort(G.scene);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Clear Track");	
}

void clear_object(char mode)
{
	Base *base;
	Object *ob;
	float *v1, *v3, mat[3][3];
	char *str=NULL;
	
	if(G.obedit) return;
	if(G.scene->id.lib) return;
	
	if(mode=='r') str= "Clear rotation";
	else if(mode=='g') str= "Clear location";
	else if(mode=='s') str= "Clear size";
	else if(mode=='o') str= "Clear origin";
	else return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			if(ob->flag & OB_POSEMODE) {
				// no test if we got armature; could be in future...
				clear_armature(ob, mode);
			}
			else if((G.f & G_WEIGHTPAINT)==0) {
				
				if(mode=='r') {
					memset(ob->rot, 0, 3*sizeof(float));
					memset(ob->drot, 0, 3*sizeof(float));
					QuatOne(ob->quat);
					QuatOne(ob->dquat);
				}
				else if(mode=='g') {
					memset(ob->loc, 0, 3*sizeof(float));
					memset(ob->dloc, 0, 3*sizeof(float));
				}
				else if(mode=='s') {
					memset(ob->dsize, 0, 3*sizeof(float));
					ob->size[0]= 1.0;
					ob->size[1]= 1.0;
					ob->size[2]= 1.0;
				}
				else if(mode=='o') {
					if(ob->parent) {
						v1= ob->loc;
						v3= ob->parentinv[3];
						
						Mat3CpyMat4(mat, ob->parentinv);
						VECCOPY(v3, v1);
						v3[0]= -v3[0];
						v3[1]= -v3[1];
						v3[2]= -v3[2];
						Mat3MulVecfl(mat, v3);
					}
				}
				
				ob->recalc |= OB_RECALC_OB;
			}			
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_scene_flush_update(G.scene, screen_view3d_layers());
	BIF_undo_push(str);
}

void reset_slowparents(void)
{
	/* back to original locations */
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if(base->object->parent) {
			if(base->object->partype & PARSLOW) {
				base->object->partype -= PARSLOW;
				where_is_object(base->object);
				base->object->partype |= PARSLOW;
			}
		}
		base= base->next;
	}
}

void set_slowparent(void)
{
	Base *base;

	if( okee("Set slow parent")==0 ) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base->object->parent) base->object->partype |= PARSLOW;
		}
		base= base->next;
	}
	BIF_undo_push("Slow parent");
}

void make_vertex_parent(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	Base *base;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	Object *par, *ob;
	int a, v1=0, v2=0, v3=0, nr=1;
	
	/* we need 1 to 3 selected vertices */
	
	if(G.obedit->type==OB_MESH) {
		eve= em->verts.first;
		while(eve) {
			if(eve->f & 1) {
				if(v1==0) v1= nr;
				else if(v2==0) v2= nr;
				else if(v3==0) v3= nr;
				else break;
			}
			nr++;
			eve= eve->next;
		}
	}
	else if ELEM(G.obedit->type, OB_SURF, OB_CURVE) {
		extern ListBase editNurb;
		nu= editNurb.first;
		while(nu) {
			if((nu->type & 7)==CU_BEZIER) {
				bezt= nu->bezt;
				a= nu->pntsu;
				while(a--) {
					if(BEZSELECTED(bezt)) {
						if(v1==0) v1= nr;
						else if(v2==0) v2= nr;
						else if(v3==0) v3= nr;
						else break;
					}
					nr++;
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					if(bp->f1 & SELECT) {
						if(v1==0) v1= nr;
						else if(v2==0) v2= nr;
						else if(v3==0) v3= nr;
						else break;
					}
					nr++;
					bp++;
				}
			}
			nu= nu->next;
		}
		
	}
	
	if( !(v1 && v2==0 && v3==0) && !(v1 && v2 && v3) ) {
		error("Select either 1 or 3 vertices to parent to");
		return;
	}
	
	if(okee("Make vertex parent")==0) return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base!=BASACT) {
				
				ob= base->object;
				ob->recalc |= OB_RECALC;
				par= BASACT->object->parent;
				
				while(par) {
					if(par==ob) break;
					par= par->parent;
				}
				if(par) {
					error("Loop in parents");
				}
				else {
					ob->parent= BASACT->object;
					if(v3) {
						ob->partype= PARVERT3;
						ob->par1= v1-1;
						ob->par2= v2-1;
						ob->par3= v3-1;

						/* inverse parent matrix */
						what_does_parent(ob);
						Mat4Invert(ob->parentinv, workob.obmat);
						clear_workob();
					}
					else {
						ob->partype= PARVERT1;
						ob->par1= v1-1;

						/* inverse parent matrix */
						what_does_parent(ob);
						Mat4Invert(ob->parentinv, workob.obmat);
						clear_workob();
					}
				}
			}
		}
		base= base->next;
	}
	allqueue(REDRAWVIEW3D, 0);
	
	DAG_scene_sort(G.scene);
	// BIF_undo_push(str); not, conflicts with editmode undo...
}

int test_parent_loop(Object *par, Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	
	if(par==0) return 0;
	if(ob == par) return 1;
	
	return test_parent_loop(par->parent, ob);

}

static char *make_bone_menu (Object *ob)
{
	char *menustr=NULL;
	bPoseChannel *pchan;
	int		size;
	int		index=0;
	
	//	Count the bones
	for(size=0, pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next, size++);
	
	size = size*48 + 256;
	menustr = MEM_callocN(size, "bonemenu");
	
	sprintf (menustr, "Select Bone%%t");
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next, index++) {
		sprintf (menustr, "%s|%s%%x%d", menustr, pchan->bone->name, index);
	}
	
	return menustr;
}


void make_parent(void)
{
	Base *base;
	Object *par;
	Bone	*bone=NULL;
	short qual, mode=0;

	if(G.scene->id.lib) return;
	if(G.obedit) {
		if ELEM3(G.obedit->type, OB_MESH, OB_CURVE, OB_SURF) make_vertex_parent();
		else if (G.obedit->type==OB_ARMATURE) make_bone_parent();
		return;
	}
	if(BASACT==0) return;
	
	qual= G.qual;
	par= BASACT->object;

	if(par->type == OB_LATTICE){
		mode= pupmenu("Make Parent %t|Normal Parent %x1|Lattice Deform %x2");
		if(mode<=0){
			return;
		}
		else if(mode==1) {
			mode= PAROBJECT;
		}
		else if(mode==2) {
			mode= PARSKEL;
		}
	}
	else if(par->type == OB_CURVE){
		bConstraint *con;
		bFollowPathConstraint *data;

		mode= pupmenu("Make Parent %t|Normal Parent %x1|Follow Path %x2|Curve Deform %x3|Path Constraint %x4");
		if(mode<=0){
			return;
		}
		else if(mode==1) {
			mode= PAROBJECT;
		}
		else if(mode==2) {
			Curve *cu= par->data;
			
			mode= PAROBJECT;
			if((cu->flag & CU_PATH)==0) {
				cu->flag |= CU_PATH|CU_FOLLOW;
				makeDispListCurveTypes(par, 0);  // force creation of path data
			}
			else cu->flag |= CU_FOLLOW;
		}
		else if(mode==3) {
			mode= PARSKEL;
		}
		else if(mode==4) {

			base= FIRSTBASE;
			while(base) {
				if TESTBASELIB(base) {
					if(base!=BASACT) {
						float cmat[4][4], vec[3], size[3];

						con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
						strcpy (con->name, "AutoPath");

						data = con->data;
						data->tar = BASACT->object;

						add_constraint_to_object(con, base->object);

						get_constraint_target_matrix(con, TARGET_OBJECT, NULL, cmat, size, G.scene->r.cfra - base->object->sf);
						VecSubf(vec, base->object->obmat[3], cmat[3]);

						base->object->loc[0] = vec[0];
						base->object->loc[1] = vec[1];
						base->object->loc[2] = vec[2];
					}
				}
				base= base->next;
			}

			allqueue(REDRAWVIEW3D, 0);
			DAG_scene_sort(G.scene);
			BIF_undo_push("make Parent");
			return;
		}
	}
	else if(par->type == OB_ARMATURE){
		int	bonenr;
		char *bonestr=NULL;
		
		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base!=BASACT) {
					if(base->object->type==OB_MESH) {
						mode= pupmenu("Make Parent To%t|Bone %x1|Armature %x2|Object %x3");
						break;
					}
					else {
						mode= pupmenu("Make Parent To %t|Bone %x1|Object %x3");
						break;
					}
				}
			}
			base= base->next;
		}
	
		switch (mode){
		case 1:
			mode=PARBONE;
			/* Make bone popup menu */

			bonestr = make_bone_menu(par);

			bonenr= pupmenu_col(bonestr, 20);
			if (bonestr)
				MEM_freeN (bonestr);
			
			if (bonenr==-1){
				allqueue(REDRAWVIEW3D, 0);
				return;
			}

			bone= get_indexed_bone(par, bonenr<<16);		// function uses selection codes
			if (!bone){
				allqueue(REDRAWVIEW3D, 0);
				return;
			}

			break;
		case 2:
			mode=PARSKEL;
			break;
		case 3:
			mode=PAROBJECT;
			break;
		default:
			return;
		}
	}
	else {
		if(qual & LR_SHIFTKEY) {
			if(okee("Make parent without inverse")==0) return;
		}
		else {
			if(qual & LR_ALTKEY) {
				if(okee("Make vertex parent")==0) return;
			}
			else if(okee("Make parent")==0) return;

			/* now we'll clearparentandkeeptransform all objects */
			base= FIRSTBASE;
			while(base) {
				if TESTBASELIB(base) {
					if(base!=BASACT && base->object->parent) {
						base->object->parent= NULL;
						apply_obmat(base->object);
					}
				}
				base= base->next;
			}
		}
	}
	
	par->recalc |= OB_RECALC_OB;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base!=BASACT) {
				
				if( test_parent_loop(par, base->object) ) {
					error("Loop in parents");
				}
				else {
					
					base->object->recalc |= OB_RECALC_OB|OB_RECALC_DATA;
					
					/* the ifs below are horrible code (ton) */
					
					if (par->type==OB_ARMATURE) {
						base->object->partype= mode;
						if (bone)
							strcpy (base->object->parsubstr, bone->name);
						else
							base->object->parsubstr[0]=0;
					}
					else {
						if(qual & LR_ALTKEY) {
							base->object->partype= PARVERT1;
						}
						else if(ELEM(par->type, OB_CURVE, OB_LATTICE)) {
							base->object->partype= mode;
						}
						else {
							base->object->partype= PAROBJECT;
						}
					}
					
					base->object->parent= par;
					
					/* calculate inverse parent matrix? */
					if( (qual & LR_SHIFTKEY) ) {
						/* not... */
						Mat4One(base->object->parentinv);
						memset(base->object->loc, 0, 3*sizeof(float));
					}
					else {
						if(mode==PARSKEL && par->type == OB_ARMATURE) {
							/* Prompt the user as to whether he wants to
								* add some vertex groups based on the bones
								* in the parent armature.
								*/
							create_vgroups_from_armature(base->object, 
															par);

							base->object->partype= PAROBJECT;
							what_does_parent(base->object);
							Mat4One (base->object->parentinv);
							base->object->partype= mode;
						}
						else
							what_does_parent(base->object);
						Mat4Invert(base->object->parentinv, workob.obmat);
					}
					
					if(par->type==OB_ARMATURE && mode == PARSKEL){
						verify_defgroups(base->object);
					}
				}
			}
		}
		base= base->next;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	
	DAG_scene_sort(G.scene);
	DAG_scene_flush_update(G.scene, screen_view3d_layers());
	
	BIF_undo_push("make Parent");
}


void enter_editmode(void)
{
	Base *base;
	Object *ob;
	ID *id;
	Mesh *me;
	int ok= 0;
	bArmature *arm;
	
	if(G.scene->id.lib) return;
	base= BASACT;
	if(base==0) return;
	if((G.vd==NULL || (base->lay & G.vd->lay))==0) return;
	
	strcpy(G.editModeTitleExtra, "");

	ob= base->object;
	if(ob->data==0) return;
	
	id= ob->data;
	if(id->lib) {
		error("Can't edit library data");
		return;
	}
	
	if(ob->type==OB_MESH) {
		me= get_mesh(ob);
		if( me==0 ) return;
		if(me->id.lib) {
			error("Can't edit library data");
			return;
		}
		ok= 1;
		G.obedit= ob;
		make_editMesh();
		allqueue(REDRAWBUTSLOGIC, 0);
		if(G.f & G_FACESELECT) allqueue(REDRAWIMAGE, 0);
	}
	if (ob->type==OB_ARMATURE){
		arm=base->object->data;
		if (!arm) return;
		if (arm->id.lib){
			error("Can't edit library data");
			return;
		}
		ok=1;
		G.obedit=ob;
		make_editArmature();
		allqueue (REDRAWVIEW3D,0);
	}
	else if(ob->type==OB_FONT) {
		G.obedit= ob;
		ok= 1;
		make_editText();
	}
	else if(ob->type==OB_MBALL) {
		G.obedit= ob;
		ok= 1;
		make_editMball();
	}
	else if(ob->type==OB_LATTICE) {
		G.obedit= ob;
		ok= 1;
		make_editLatt();
	}
	else if(ob->type==OB_SURF || ob->type==OB_CURVE) {
		ok= 1;
		G.obedit= ob;
		make_editNurb();
	}
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWOOPS, 0);
	countall();
	
	if(ok) {
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
	
		allqueue(REDRAWVIEW3D, 1);
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		
	}
	else G.obedit= NULL;

	scrarea_queue_headredraw(curarea);
}

void exit_editmode(int freedata)	/* freedata==0 at render, 1= freedata, 2= do undo buffer too */
{
	Object *ob;

	if(G.obedit==NULL) return;

	if(G.obedit->type==OB_MESH) {

		/* temporal */
		countall();

		if(G.totvert>MESH_MAX_VERTS) {
			error("Too many vertices");
			return;
		}
		load_editMesh();

		if(freedata) free_editMesh(G.editMesh);

		if(G.f & G_FACESELECT) {
			set_seamtface();
			allqueue(REDRAWIMAGE, 0);
		}
	}
	else if (G.obedit->type==OB_ARMATURE){	
		load_editArmature();
		if (freedata) free_editArmature();
	}
	else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
		extern ListBase editNurb;
		load_editNurb();
		if(freedata) freeNurblist(&editNurb);
	}
	else if(G.obedit->type==OB_FONT && freedata) {
		load_editText();
	}
	else if(G.obedit->type==OB_LATTICE) {
		load_editLatt();
		if(freedata) free_editLatt();
	}
	else if(G.obedit->type==OB_MBALL) {
		extern ListBase editelems;
		load_editMball();
		if(freedata) BLI_freelistN(&editelems);
	}

	ob= G.obedit;
	
	/* for example; displist make is different in editmode */
	if(freedata) G.obedit= NULL;

	/* total remake of softbody data */
	if(modifiers_isSoftbodyEnabled(ob)) {
		if (ob->soft && ob->soft->keys) {
			notice("Erased Baked SoftBody");
		}

		sbObjectToSoftbody(ob);
	}
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	if(freedata) {
		setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	
		countall();
		allqueue(REDRAWVIEW3D, 1);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWBUTSLOGIC, 0);
		allqueue(REDRAWOOPS, 0);
	}
	scrarea_queue_headredraw(curarea);
	
	if(G.obedit==NULL && freedata==2) 
		BIF_undo_push("Editmode");
}

void check_editmode(int type)
{
	
	if (G.obedit==0 || G.obedit->type==type) return;

	exit_editmode(2); // freedata, and undo
}

/* 0 == do centre, 1 == centre new, 2 == centre cursor */

void docentre(int centremode)
{
	EditMesh *em = G.editMesh;
	Base *base;
	Object *ob;
	Mesh *me, *tme;
	Curve *cu;
//	BezTriple *bezt;
//	BPoint *bp;
	Nurb *nu, *nu1;
	EditVert *eve;
	float cent[3], centn[3], min[3], max[3], omat[3][3];
	int a, total= 0;
	MVert *mvert;

	if(G.scene->id.lib) return;
	
	cent[0]= cent[1]= cent[2]= 0.0;
	
	if(G.obedit) {

		INIT_MINMAX(min, max);
	
		if(G.obedit->type==OB_MESH) {
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(G.vd->around==V3D_CENTROID) {
					total++;
					VECADD(cent, cent, eve->co);
				}
				else {
					DO_MINMAX(eve->co, min, max);
				}
			}
			
			if(G.vd->around==V3D_CENTROID) {
				VecMulf(cent, 1.0f/(float)total);
			}
			else {
				cent[0]= (min[0]+max[0])/2.0f;
				cent[1]= (min[1]+max[1])/2.0f;
				cent[2]= (min[2]+max[2])/2.0f;
			}
			
			for(eve= em->verts.first; eve; eve= eve->next) {
				VecSubf(eve->co, eve->co, cent);			
			}
			
			recalc_editnormals();
		}
	}
	
	/* reset flags */
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			base->object->flag &= ~OB_DONE;
		}
		base= base->next;
	}
	me= G.main->mesh.first;
	while(me) {
		me->flag &= ~ME_ISDONE;
		me= me->id.next;
	}
	
	base= FIRSTBASE;
	while(base) {
		
		if TESTBASELIB(base) {
			if((base->object->flag & OB_DONE)==0) {
				
				base->object->flag |= OB_DONE;
				
				if(G.obedit==0 && (me=get_mesh(base->object)) ) {
					
					if(me->key) {
						error("Can't change the center of a mesh with vertex keys");
						return;
					}
					
					if(centremode==2) {
						VECCOPY(cent, give_cursor());
						Mat4Invert(base->object->imat, base->object->obmat);
						Mat4MulVecfl(base->object->imat, cent);
					} else {
						INIT_MINMAX(min, max);
		
						mvert= me->mvert;
						for(a=0; a<me->totvert; a++, mvert++) {
							DO_MINMAX(mvert->co, min, max);
						}
				
						cent[0]= (min[0]+max[0])/2.0f;
						cent[1]= (min[1]+max[1])/2.0f;
						cent[2]= (min[2]+max[2])/2.0f;
					}
						
					mvert= me->mvert;
					for(a=0; a<me->totvert; a++, mvert++) {
						VecSubf(mvert->co, mvert->co, cent);
					}
					me->flag |= ME_ISDONE;
					
					if(centremode) {
						Mat3CpyMat4(omat, base->object->obmat);
						
						VECCOPY(centn, cent);
						Mat3MulVecfl(omat, centn);
						base->object->loc[0]+= centn[0];
						base->object->loc[1]+= centn[1];
						base->object->loc[2]+= centn[2];
						
						/* other users? */
						ob= G.main->object.first;
						while(ob) {
							if((ob->flag & OB_DONE)==0) {
								tme= get_mesh(ob);
								
								if(tme==me) {
									
									ob->flag |= OB_DONE;

									Mat3CpyMat4(omat, ob->obmat);
									VECCOPY(centn, cent);
									Mat3MulVecfl(omat, centn);
									ob->loc[0]+= centn[0];
									ob->loc[1]+= centn[1];
									ob->loc[2]+= centn[2];
									
									if(tme && (tme->flag & ME_ISDONE)==0) {
										mvert= tme->mvert;
										for(a=0; a<tme->totvert; a++, mvert++) {
											VecSubf(mvert->co, mvert->co, cent);
										}
										tme->flag |= ME_ISDONE;
									}
								}
							}
							
							ob= ob->id.next;
						}
					}
				}
				else if ELEM(base->object->type, OB_CURVE, OB_SURF) {
									
					if(G.obedit) {
						extern ListBase editNurb;
						nu1= editNurb.first;
					}
					else {
						cu= base->object->data;
						nu1= cu->nurb.first;
					}
					
					if(centremode==2) {
						VECCOPY(cent, give_cursor());
						Mat4Invert(base->object->imat, base->object->obmat);
						Mat4MulVecfl(base->object->imat, cent);
							
							/* Curves need to be 2d, never offset in
							 * Z. Is a somewhat arbitrary restriction, 
							 * would probably be nice to remove.
							 */
						cent[2]= 0.0;
					} else {
						INIT_MINMAX(min, max);
	
						nu= nu1;
						while(nu) {
							minmaxNurb(nu, min, max);
							nu= nu->next;
						}
						
						cent[0]= (min[0]+max[0])/2.0f;
						cent[1]= (min[1]+max[1])/2.0f;
						cent[2]= (min[2]+max[2])/2.0f;
					}
					
					nu= nu1;
					while(nu) {
						if( (nu->type & 7)==1) {
							a= nu->pntsu;
							while (a--) {
								VecSubf(nu->bezt[a].vec[0], nu->bezt[a].vec[0], cent);
								VecSubf(nu->bezt[a].vec[1], nu->bezt[a].vec[1], cent);
								VecSubf(nu->bezt[a].vec[2], nu->bezt[a].vec[2], cent);
							}
						}
						else {
							a= nu->pntsu*nu->pntsv;
							while (a--)
								VecSubf(nu->bp[a].vec, nu->bp[a].vec, cent);
						}
						nu= nu->next;
					}
			
					if(centremode && G.obedit==0) {
						Mat3CpyMat4(omat, base->object->obmat);
						
						Mat3MulVecfl(omat, cent);
						base->object->loc[0]+= cent[0];
						base->object->loc[1]+= cent[1];
						base->object->loc[2]+= cent[2];
					}
			
					if(G.obedit) {
						break;
					}
	
				}
				else if(base->object->type==OB_FONT) {
					/* get from bb */
					
					cu= base->object->data;
					if(cu->bb==0) return;
					
					cu->xof= -0.5f*( cu->bb->vec[4][0] - cu->bb->vec[0][0]);
					cu->yof= -0.5f -0.5f*( cu->bb->vec[0][1] - cu->bb->vec[2][1]);	/* extra 0.5 is the height of above line */
					
					/* not really ok, do this better once! */
					cu->xof /= cu->fsize;
					cu->yof /= cu->fsize;

					allqueue(REDRAWBUTSEDIT, 0);
				}
				DAG_object_flush_update(G.scene, base->object, OB_RECALC_OB|OB_RECALC_DATA);
			}
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Do Centre");	
}

void docentre_new(void)
{
	if(G.scene->id.lib) return;

	if(G.obedit) {
		error("Unable to center new in Edit Mode");
	}
	else {
		docentre(1);
	}
}

void docentre_cursor(void)
{
	if(G.scene->id.lib) return;

	if(G.obedit) {
		error("Unable to center cursor in Edit Mode");
	}
	else {
		docentre(2);
	}
}

void movetolayer(void)
{
	Base *base;
	unsigned int lay= 0, local;
	int islamp= 0;
	
	if(G.scene->id.lib) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) lay |= base->lay;
		base= base->next;
	}
	if(lay==0) return;
	lay &= 0xFFFFFF;
	
	if( movetolayer_buts(&lay)==0 ) return;
	if(lay==0) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			/* upper byte is used for local view */
			local= base->lay & 0xFF000000;  
			base->lay= lay + local;
			base->object->lay= lay;
			if(base->object->type==OB_LAMP) islamp= 1;
		}
		base= base->next;
	}
	
	if(islamp && G.vd->drawtype == OB_SHADED) reshadeall_displist();

	countall();
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWINFO, 0);
	
	BIF_undo_push("Move to layer");
}

void split_font()
{
	Object *ob = OBACT;
	Base *oldbase = BASACT;
	Curve *cu= ob->data;
	char *p= cu->str;
	int slen= strlen(p);
	int i;

	for (i = 0; i<=slen; p++, i++) {
		adduplicate(1);
		cu= OBACT->data;
		cu->sepchar = i+1;
		text_to_curve(OBACT, 0);	// pass 1: only one letter, adapt position
		text_to_curve(OBACT, 0);	// pass 2: remake
		freedisplist(&OBACT->disp);
		makeDispListCurveTypes(OBACT, 0);
		
		OBACT->flag &= ~SELECT;
		BASACT->flag &= ~SELECT;
		oldbase->flag |= SELECT;
		oldbase->object->flag |= SELECT;
		set_active_base(oldbase);		
	}
}

void special_editmenu(void)
{
	Object *ob= OBACT;
	float fac;
	int nr,ret;
	short randfac,numcuts;
	
	if(ob==NULL) return;
	
	if(G.obedit==NULL) {
		
		if(ob->flag & OB_POSEMODE) {
			pose_special_editmenu();
		}
		else if(G.f & G_FACESELECT) {
			Mesh *me= get_mesh(ob);
			TFace *tface;
			int a;
			
			if(me==0 || me->tface==0) return;
			
			nr= pupmenu("Specials%t|Set     Tex%x1|         Shared%x2|         Light%x3|         Invisible%x4|         Collision%x5|Clr     Tex%x6|         Shared%x7|         Light%x8|         Invisible%x9|         Collision%x10");
	
			for(a=me->totface, tface= me->tface; a>0; a--, tface++) {
				if(tface->flag & SELECT) {
					switch(nr) {
					case 1:
						tface->mode |= TF_TEX; break;
					case 2:
						tface->mode |= TF_SHAREDCOL; break;
					case 3:
						tface->mode |= TF_LIGHT; break; 
					case 4:
						tface->mode |= TF_INVISIBLE; break;
					case 5:
						tface->mode |= TF_DYNAMIC; break;
					case 6:
						tface->mode &= ~TF_TEX;
						tface->tpage= 0;
						break;
					case 7:
						tface->mode &= ~TF_SHAREDCOL; break;
					case 8:
						tface->mode &= ~TF_LIGHT; break;
					case 9:
						tface->mode &= ~TF_INVISIBLE; break;
					case 10:
						tface->mode &= ~TF_DYNAMIC; break;
					}
				}
			}
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSEDIT, 0);
			BIF_undo_push("Change texture face");
		}
		else if(G.f & G_VERTEXPAINT) {
			Mesh *me= get_mesh(ob);
			
			if(me==0 || (me->mcol==NULL && me->tface==NULL) ) return;
			
			nr= pupmenu("Specials%t|Shared VertexCol%x1");
			if(nr==1) {
				
				if(me->tface) tface_to_mcol(me);
				
				copy_vpaint_undo( (unsigned int *)me->mcol, me->totface);
				do_shared_vertexcol(me);
				
				if(me->tface) mcol_to_tface(me, 1);
				BIF_undo_push("Shared VertexCol");

				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
		}
		else if(G.f & G_WEIGHTPAINT) {
			if(ob->parent && (ob->parent->flag & OB_POSEMODE)) {
				nr= pupmenu("Specials%t|Apply Bone Envelopes to VertexGroups %x1");
				if(nr==1) {
					Mesh *me= ob->data;
					if(me->dvert) copy_wpaint_undo(me->dvert, me->totvert);
					pose_adds_vgroups(ob);
				}
			}
		}
		else {
			Base *base, *base_select= NULL;
			
			// Get the active object mesh.
			Mesh *me= get_mesh(ob);

			// Booleans, if the active object is a mesh...
			if (me && ob->id.lib==NULL) {
				
				// Bring up a little menu with the boolean operation choices on.
				nr= pupmenu("Boolean Tools%t|Intersect%x1|Union%x2|Difference%x3|Add Intersect Modifier%x4|Add Union Modifier%x5|Add Difference Modifier%x6");

				if (nr > 0) {
					// user has made a choice of a menu element.
					// All of the boolean functions require 2 mesh objects 
					// we search through the object list to find the other 
					// selected item and make sure it is distinct and a mesh.

					for(base= FIRSTBASE; base; base= base->next) {
						if TESTBASELIB(base) {
							if(base->object != ob) base_select= base;
						}
					}

					if (base_select) {
						if (get_mesh(base_select->object)) {
							if(nr <= 3){
								waitcursor(1);
								ret = NewBooleanMesh(BASACT,base_select,nr);
								if (ret==0) {
									error("An internal error occurred -- sorry!");
								} else if(ret==-1) {
									error("Selected meshes must have faces to perform boolean operations");
								}
								else BIF_undo_push("Boolean");
								waitcursor(0);
							} else {
								BooleanModifierData *bmd = NULL;
								bmd = (BooleanModifierData *)modifier_new(eModifierType_Boolean);
								BLI_addtail(&ob->modifiers, bmd);
								bmd->object = base_select->object;
								bmd->modifier.mode |= eModifierMode_Realtime;
								switch(nr){
									case 4: bmd->operation = eBooleanModifierOp_Intersect; break;
									case 5: bmd->operation = eBooleanModifierOp_Union; break;
									case 6:	bmd->operation = eBooleanModifierOp_Difference; break;
								}
								do_common_editbuts(B_CHANGEDEP);
								BIF_undo_push("Add Boolean modifier");								
							}								
						} else {
							error("Please select 2 meshes");
						}
					} else {
						error("Please select 2 meshes");
					}
				}

				allqueue(REDRAWVIEW3D, 0);
			}
			else if (ob->type == OB_FONT) {
				nr= pupmenu("Split %t|Characters%x1");
				if (nr > 0) {
					switch(nr) {
						case 1: split_font();
					}
				}
			}			
		}
	}
	else if(G.obedit->type==OB_MESH) {

		nr= pupmenu("Specials%t|Subdivide%x1|Subdivide Multi%x2|Subdivide Multi Fractal%x3|Subdivide Multi Smooth - WIP%x12|Subdivide Smooth Old%x13|Merge%x4|Remove Doubles%x5|Hide%x6|Reveal%x7|Select Swap%x8|Flip Normals %x9|Smooth %x10|Bevel %x11|Set Smooth %x14|Set Solid %x15");
		
		switch(nr) {
		case 1:
			numcuts = 1;
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag,numcuts,0);
			
			BIF_undo_push("ESubdivide Single");            
			break;
		case 2:
		numcuts = 2;
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag,numcuts,0);
			BIF_undo_push("ESubdivide");
			break;
		case 3:
			numcuts = 2;
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
			randfac= 10;
			if(button(&randfac, 1, 100, "Rand fac:")==0) return;
			fac= -( (float)randfac )/100;
			esubdivideflag(1, fac, G.scene->toolsettings->editbutflag,numcuts,0);
			BIF_undo_push("Subdivide Fractal");
			break;

		case 4:
			mergemenu();
			break;
		case 5:
			notice("Removed %d Vertices", removedoublesflag(1, G.scene->toolsettings->doublimit));
			BIF_undo_push("Remove Doubles");
			break;
		case 6:
			hide_mesh(0);
			break;
		case 7:
			reveal_mesh();
			break;
		case 8:
			selectswap_mesh();
			break;
		case 9:
			flip_editnormals();
			BIF_undo_push("Flip Normals");
			break;
		case 10:
			vertexsmooth();
			break;
		case 11:
			bevel_menu();
			break;
		case 12:
			numcuts = 2;
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag | B_SMOOTH,numcuts,0);
			BIF_undo_push("Subdivide Smooth");
			break;		
		case 13:
			waitcursor(1);
			subdivideflag(1, 0.0, G.scene->toolsettings->editbutflag | B_SMOOTH);
			BIF_undo_push("Subdivide Smooth");
			break;
		case 14:
			mesh_set_smooth_faces(1);
			break;
		case 15: 
			mesh_set_smooth_faces(0);
			break;
		}
		
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		
		if(nr>0) waitcursor(0);
		
	}
	else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {

		nr= pupmenu("Specials%t|Subdivide%x1|Switch Direction%x2");
		
		switch(nr) {
		case 1:
			subdivideNurb();
			break;
		case 2:
			switchdirectionNurb2();
			break;
		}
		
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	}
	else if(G.obedit->type==OB_ARMATURE) {
		nr= pupmenu("Specials%t|Subdivide %x1|Flip Left-Right Names%x2");
		if(nr==1)
			subdivide_armature();
		else if(nr==2)
			armature_flip_names();
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	
}

void convertmenu(void)
{
	Base *base, *basen, *basact, *basedel=NULL;
	Object *ob, *ob1;
	Curve *cu;
	MetaBall *mb;
	Mesh *me;
	DispList *dl;
	int ok=0, nr = 0, a;
	
	if(G.scene->id.lib) return;

	ob= OBACT;
	if(ob==0) return;
	if(G.obedit) return;
	
	basact= BASACT;	/* will be restored */
		
	if(ob->type==OB_FONT) {
		nr= pupmenu("Convert Font to%t|Curve");
		if(nr>0) ok= 1;
	}
	else if(ob->type==OB_MBALL) {
		nr= pupmenu("Convert Metaball to%t|Mesh (keep original)%x1|Mesh (Delete Original)%x2");
		if(nr>0) ok= 1;
	}
	else if(ob->type==OB_CURVE) {
		nr= pupmenu("Convert Curve to%t|Mesh");
		if(nr>0) ok= 1;
	}
	else if(ob->type==OB_SURF) {
		nr= pupmenu("Convert Nurbs Surface to%t|Mesh");
		if(nr>0) ok= 1;
	}
	else if(ob->type==OB_MESH) {
		nr= pupmenu("Convert Modifiers to%t|Mesh (Keep Original)%x1|Mesh (Delete Original)%x2");
		if(nr>0) ok= 1;
	}
	if(ok==0) return;

	/* don't forget multiple users! */

	/* reset flags */
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			base->object->flag &= ~OB_DONE;
		}
		base= base->next;
	}

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			
			ob= base->object;
			
			if(ob->flag & OB_DONE);
			else if(ob->type==OB_MESH) {
				Mesh *oldme= ob->data;
				DispListMesh *dlm;
				DerivedMesh *dm;

				basedel = base;

				ob->flag |= OB_DONE;

				ob1= copy_object(ob);
				ob1->recalc |= OB_RECALC;
				object_free_modifiers(ob1);

				basen= MEM_mallocN(sizeof(Base), "duplibase");
				*basen= *base;
				BLI_addhead(&G.scene->base, basen);	/* addhead: otherwise eternal loop */
				basen->object= ob1;
				basen->flag &= ~SELECT;
					
				me= ob1->data;
				me->id.us--;
					
				ob1->data= add_mesh();
				G.totmesh++;
				ob1->type= OB_MESH;

				me= ob1->data;
				me->totcol= oldme->totcol;
				if(ob1->totcol) {
					me->mat= MEM_dupallocN(oldme->mat);
					for(a=0; a<ob1->totcol; a++) id_us_plus((ID *)me->mat[a]);
				}

				dm= mesh_create_derived_no_deform(ob, NULL);
				dlm= dm->convertToDispListMesh(dm, 0);
				displistmesh_to_mesh(dlm, ob1->data);
				dm->release(dm);
			}
			else if(ob->type==OB_FONT) {
				if(nr==1) {
				
					ob->flag |= OB_DONE;
				
					ob->type= OB_CURVE;
					cu= ob->data;
					
					if(cu->vfont) {
						cu->vfont->id.us--;
						cu->vfont= 0;
					}
					if(cu->vfontb) {
						cu->vfontb->id.us--;
						cu->vfontb= 0;
					}
					if(cu->vfonti) {
						cu->vfonti->id.us--;
						cu->vfonti= 0;
					}
					if(cu->vfontbi) {
						cu->vfontbi->id.us--;
						cu->vfontbi= 0;
					}					
					/* other users */
					if(cu->id.us>1) {
						ob1= G.main->object.first;
						while(ob1) {
							if(ob1->data==cu) {
								ob1->type= OB_CURVE;
								ob1->recalc |= OB_RECALC;
							}
							ob1= ob1->id.next;
						}
					}
				}
			}
			else if ELEM(ob->type, OB_CURVE, OB_SURF) {
				if(nr==1) {

					ob->flag |= OB_DONE;
					cu= ob->data;
					
					dl= cu->disp.first;
					if(dl==0) makeDispListCurveTypes(ob, 0);		// force creation

					nurbs_to_mesh(ob); /* also does users */

					/* texspace and normals */
					BASACT= base;
					enter_editmode();
					exit_editmode(1); // freedata, but no undo
					BASACT= basact;
 				}
			}
			else if(ob->type==OB_MBALL) {
			
				if(nr==1 || nr == 2) {
					ob= find_basis_mball(ob);
					
					if(ob->disp.first && !(ob->flag&OB_DONE)) {
						basedel = base;
					
						ob->flag |= OB_DONE;

						ob1= copy_object(ob);
						ob1->recalc |= OB_RECALC;

						basen= MEM_mallocN(sizeof(Base), "duplibase");
						*basen= *base;
						BLI_addhead(&G.scene->base, basen);	/* addhead: othwise eternal loop */
						basen->object= ob1;
						basen->flag &= ~SELECT;
						
						mb= ob1->data;
						mb->id.us--;
						
						ob1->data= add_mesh();
						G.totmesh++;
						ob1->type= OB_MESH;
						
						me= ob1->data;
						me->totcol= mb->totcol;
						if(ob1->totcol) {
							me->mat= MEM_dupallocN(mb->mat);
							for(a=0; a<ob1->totcol; a++) id_us_plus((ID *)me->mat[a]);
						}
						
						mball_to_mesh(&ob->disp, ob1->data);
					}
				}
			}
		}
		base= base->next;
		if(basedel != NULL && nr == 2)
			free_and_unlink_base(basedel);	
		basedel = NULL;				
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Convert Object");

	DAG_scene_sort(G.scene);
}

	/* Change subdivision properties of mesh object ob, if
	 * level==-1 then toggle subsurf, else set to level.
	 */
void flip_subdivison(Object *ob, int level)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Subsurf);

	if (md) {
		SubsurfModifierData *smd = (SubsurfModifierData*) md;

		if (level == -1) {
			if (smd->modifier.mode&(eModifierMode_Render|eModifierMode_Realtime)) {
				smd->modifier.mode &= ~(eModifierMode_Render|eModifierMode_Realtime);
			} else {
				smd->modifier.mode |= (eModifierMode_Render|eModifierMode_Realtime);
			}
		} else {
			smd->levels = level;
		}
	} else {
		SubsurfModifierData *smd = (SubsurfModifierData*) modifier_new(eModifierType_Subsurf);

		BLI_addtail(&ob->modifiers, smd);

		if (level!=-1) {
			smd->levels = level;
		}
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	BIF_undo_push("Switch subsurf on/off");
}
 
static void copymenu_properties(Object *ob)
{	
	bProperty *prop, *propn, *propc;
	Base *base;
	int nr, tot=0;
	char *str;
	
	prop= ob->prop.first;
	while(prop) {
		tot++;
		prop= prop->next;
	}
	
	if(tot==0) {
		error("No properties in the active object to copy");
		return;
	}
	
	str= MEM_callocN(24+32*tot, "copymenu prop");
	
	strcpy(str, "Copy Property %t");
	
	tot= 0;	
	prop= ob->prop.first;
	while(prop) {
		tot++;
		strcat(str, " |");
		strcat(str, prop->name);
		prop= prop->next;
	}

	nr= pupmenu(str);
	if(nr>0) {
		tot= 0;
		prop= ob->prop.first;
		while(prop) {
			tot++;
			if(tot==nr) break;
			prop= prop->next;
		}
		if(prop) {
			propc= prop;
			
			base= FIRSTBASE;
			while(base) {
				if(base != BASACT) {
					if(TESTBASELIB(base)) {
						prop= get_property(base->object, propc->name);
						if(prop) {
							free_property(prop);
							BLI_remlink(&base->object->prop, prop);
						}
						propn= copy_property(propc);
						BLI_addtail(&base->object->prop, propn);
					}
				}
				base= base->next;
			}
		}
	}
	MEM_freeN(str);
	allqueue(REDRAWVIEW3D, 0);
	
	BIF_undo_push("Copy properties");
}

static void copymenu_logicbricks(Object *ob)
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if(base->object != ob) {
			if(TESTBASELIB(base)) {
				
				/* first: free all logic */
				free_sensors(&base->object->sensors);				
				unlink_controllers(&base->object->controllers);
				free_controllers(&base->object->controllers);
				unlink_actuators(&base->object->actuators);
				free_actuators(&base->object->actuators);
				
				/* now copy it, this also works without logicbricks! */
				clear_sca_new_poins_ob(ob);
				copy_sensors(&base->object->sensors, &ob->sensors);
				copy_controllers(&base->object->controllers, &ob->controllers);
				copy_actuators(&base->object->actuators, &ob->actuators);
				set_sca_new_poins_ob(base->object);
				
				/* some menu settings */
				base->object->scavisflag= ob->scavisflag;
				base->object->scaflag= ob->scaflag;
				
			}
		}
		base= base->next;
	}
	BIF_undo_push("Copy logic");
}

static void copymenu_modifiers(Object *ob)
{
	char str[512];
	int i, event;
	Base *base;

	strcpy(str, "Copy Modifiers %t");

	sprintf(str+strlen(str), "|All%%x%d|%%l", NUM_MODIFIER_TYPES);

	for (i=eModifierType_None+1; i<NUM_MODIFIER_TYPES; i++) {
		ModifierTypeInfo *mti = modifierType_getInfo(i);

		if (ELEM(i, eModifierType_Hook, eModifierType_Softbody)) continue;

		if (	(mti->flags&eModifierTypeFlag_AcceptsCVs) || 
				(ob->type==OB_MESH && (mti->flags&eModifierTypeFlag_AcceptsMesh))) {
			sprintf(str+strlen(str), "|%s%%x%d", mti->name, i);
		}
	}

	event = pupmenu(str);
	if(event<=0) return;

	for (base= FIRSTBASE; base; base= base->next) {
		if(base->object != ob) {
			if(TESTBASELIB(base)) {
				ModifierData *md;

				base->object->recalc |= OB_RECALC_OB|OB_RECALC_DATA;

				if (base->object->type==OB_MESH) {
					if (event==NUM_MODIFIER_TYPES) {
						object_free_modifiers(base->object);

						for (md=ob->modifiers.first; md; md=md->next) {
							if (md->type!=eModifierType_Hook) {
								ModifierData *nmd = modifier_new(md->type);
								modifier_copyData(md, nmd);
								BLI_addtail(&base->object->modifiers, nmd);
							}
						}
					} else {
						ModifierData *md = modifiers_findByType(ob, event);

						if (md) {
							ModifierData *tmd = modifiers_findByType(base->object, event);

							if (!tmd) {
								tmd = modifier_new(event);
								BLI_addtail(&base->object->modifiers, tmd);
							}

							modifier_copyData(md, tmd);
						}
					}
				}
			}
		}
	}
				
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	DAG_scene_sort(G.scene);
	
	BIF_undo_push("Copy modifiers");
}

void copy_attr_menu()
{
	Object *ob;
	short event;
	char str[512];

	/* If you change this menu, don't forget to update the menu in header_view3d.c
	 * view3d_edit_object_copyattrmenu() and in toolbox.c
	 */
	strcpy(str, "Copy Attributes %t|Location%x1|Rotation%x2|Size%x3|Drawtype%x4|Time Offset%x5|Dupli%x6|%l|Mass%x7|Damping%x8|Properties%x9|Logic Bricks%x10|%l");

	if(!(ob=OBACT)) return;
	
	strcat (str, "|Object Constraints%x22");
	
	if ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL) {
		strcat(str, "|Texture Space%x17");
	}	
	
	if(ob->type == OB_FONT) strcat(str, "|Font Settings%x18|Bevel Settings%x19");
	if(ob->type == OB_CURVE) strcat(str, "|Bevel Settings%x19");
	
	if((ob->type == OB_FONT) || (ob->type == OB_CURVE)) {
			strcat(str, "|Curve Resolution%x25");
	}

	if(ob->type==OB_MESH){
		strcat(str, "|Subdiv%x21");
	}

	if( give_parteff(ob) ) strcat(str, "|Particle Settings%x20");

	if(ob->soft) strcat(str, "|Soft Body Settings%x23");
	
	if(ob->type==OB_MESH){
		strcat(str, "|Modifiers ...%x24");
	}

	event= pupmenu(str);
	if(event<= 0) return;
	
	copy_attr(event);
}

void copy_attr(short event)
{
	Object *ob, *obt;
	Base *base;
	Curve *cu, *cu1;
	Nurb *nu;
	void *poin1, *poin2=0;
	
	if(G.scene->id.lib) return;

	if(!(ob=OBACT)) return;
	
	if(G.obedit) {
		/* obedit_copymenu(); */
		return;
	}
	
	if ELEM5(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL) {
		if(ob->type==OB_MESH) poin2= &(((Mesh *)ob->data)->texflag);
		else if ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT) poin2= &(((Curve *)ob->data)->texflag);
		else if(ob->type==OB_MBALL) poin2= &(((MetaBall *)ob->data)->texflag);
	}	

	if(event==9) {
		copymenu_properties(ob);
		return;
	}
	else if(event==10) {
		copymenu_logicbricks(ob);
		return;
	}
	else if(event==24) {
		copymenu_modifiers(ob);
		return;
	}

	base= FIRSTBASE;
	while(base) {
		if(base != BASACT) {
			if(TESTBASELIB(base)) {
				base->object->recalc |= OB_RECALC_OB;
				
				if(event==1) {  /* loc */
					VECCOPY(base->object->loc, ob->loc);
					VECCOPY(base->object->dloc, ob->dloc);
				}
				else if(event==2) {  /* rot */
					VECCOPY(base->object->rot, ob->rot);
					VECCOPY(base->object->drot, ob->drot);
					VECCOPY(base->object->quat, ob->quat);
					VECCOPY(base->object->dquat, ob->dquat);
				}
				else if(event==3) {  /* size */
					VECCOPY(base->object->size, ob->size);
					VECCOPY(base->object->dsize, ob->dsize);
				}
				else if(event==4) {  /* drawtype */
					base->object->dt= ob->dt;
					base->object->dtx= ob->dtx;
				}
				else if(event==5) {  /* time offs */
					base->object->sf= ob->sf;
				}
				else if(event==6) {  /* dupli */
					base->object->dupon= ob->dupon;
					base->object->dupoff= ob->dupoff;
					base->object->dupsta= ob->dupsta;
					base->object->dupend= ob->dupend;
					
					base->object->transflag &= ~OB_DUPLI;
					base->object->transflag |= (ob->transflag & OB_DUPLI);
				}
				else if(event==7) {	/* mass */
					base->object->mass= ob->mass;
				}
				else if(event==8) {	/* damping */
					base->object->damping= ob->damping;
					base->object->rdamping= ob->rdamping;
				}
				else if(event==17) {	/* tex space */
					obt= base->object;
					poin1= 0;
					if(obt->type==OB_MESH) poin1= &(((Mesh *)obt->data)->texflag);
					else if ELEM3(obt->type, OB_CURVE, OB_SURF, OB_FONT) poin1= &(((Curve *)obt->data)->texflag);
					else if(obt->type==OB_MBALL) poin1= &(((MetaBall *)obt->data)->texflag);					
					
					if(poin1) {
						memcpy(poin1, poin2, 4+12+12+12);
					
						if(obt->type==OB_MESH) ;
						else if(obt->type==OB_MBALL) tex_space_mball(obt);
						else tex_space_curve(obt->data);
					}
				}
				else if(event==18) {	/* font settings */
					
					if(base->object->type==ob->type) {
						cu= ob->data;
						cu1= base->object->data;
						
						cu1->spacemode= cu->spacemode;
						cu1->spacing= cu->spacing;
						cu1->linedist= cu->linedist;
						cu1->shear= cu->shear;
						cu1->fsize= cu->fsize;
						cu1->xof= cu->xof;
						cu1->yof= cu->yof;
						cu1->textoncurve= cu->textoncurve;
						if(cu1->vfont) cu1->vfont->id.us--;
						cu1->vfont= cu->vfont;
						id_us_plus((ID *)cu1->vfont);
						if(cu1->vfontb) cu1->vfontb->id.us--;
						cu1->vfontb= cu->vfontb;
						id_us_plus((ID *)cu1->vfontb);
						if(cu1->vfonti) cu1->vfonti->id.us--;
						cu1->vfonti= cu->vfonti;
						id_us_plus((ID *)cu1->vfonti);
						if(cu1->vfontbi) cu1->vfontbi->id.us--;
						cu1->vfontbi= cu->vfontbi;
						id_us_plus((ID *)cu1->vfontbi);						

						text_to_curve(base->object, 0);		// needed?

						
						strcpy(cu1->family, cu->family);
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==19) {	/* bevel settings */
					
					if ELEM(base->object->type, OB_CURVE, OB_FONT) {
						cu= ob->data;
						cu1= base->object->data;
						
						cu1->bevobj= cu->bevobj;
						cu1->taperobj= cu->taperobj;
						cu1->width= cu->width;
						cu1->bevresol= cu->bevresol;
						cu1->ext1= cu->ext1;
						cu1->ext2= cu->ext2;
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==25) {	/* curve resolution */

					if ELEM(base->object->type, OB_CURVE, OB_FONT) {
						cu= ob->data;
						cu1= base->object->data;
						
						cu1->resolu= cu->resolu;
						
						nu= cu1->nurb.first;
						
						while(nu) {
							nu->resolu= cu1->resolu;
							nu= nu->next;
						}
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==20) {	/* particle settings */
					PartEff *pa1, *pa2;
					char *p1, *p2;
					
					pa1= give_parteff(ob);
					pa2= give_parteff(base->object);

					if(pa1==0 && pa2) {
						BLI_remlink( &(base->object->effect), pa2);
						free_effect( (Effect *) pa2);
					}
					else if(pa1 && pa2==0) {
						free_effects(&(base->object->effect));
						copy_effects(&(base->object->effect), &ob->effect);
						build_particle_system(base->object);
					}
					else if(pa1 && pa2) {
						if(pa2->keys) MEM_freeN(pa2->keys);
						
						p1= (char *)pa1; p2= (char *)pa2;
						memcpy( p2+8, p1+8, sizeof(PartEff) - 8);
						pa2->keys= 0;
						
						build_particle_system(base->object);
					}
				}
				else if(event==21){
					if (base->object->type==OB_MESH) {
						ModifierData *md = modifiers_findByType(ob, eModifierType_Subsurf);

						if (md) {
							ModifierData *tmd = modifiers_findByType(base->object, eModifierType_Subsurf);

							if (!tmd) {
								tmd = modifier_new(eModifierType_Subsurf);
								BLI_addtail(&base->object->modifiers, tmd);
							}

							modifier_copyData(md, tmd);
							base->object->recalc |= OB_RECALC_DATA;
						}
					}
				}
				else if(event==22) {
					/* Clear the constraints on the target */
					free_constraints(&base->object->constraints);
					free_constraint_channels(&base->object->constraintChannels);

					/* Copy the constraint channels over */
					copy_constraints(&base->object->constraints, &ob->constraints);
					if (U.dupflag& USER_DUP_IPO)
						copy_constraint_channels(&base->object->constraintChannels, &ob->constraintChannels);
					else
						clone_constraint_channels (&base->object->constraintChannels, &ob->constraintChannels);
				}
				else if(event==23) {
					base->object->softflag= ob->softflag;
					if(base->object->soft) sbFree(base->object->soft);
					
					base->object->soft= copy_softbody(ob->soft);

					if (!modifiers_findByType(base->object, eModifierType_Softbody)) {
						BLI_addhead(&base->object->modifiers, modifier_new(eModifierType_Softbody));
					}
				}
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_scene_flush_update(G.scene, screen_view3d_layers());

	if(event==20) {
		allqueue(REDRAWBUTSOBJECT, 0);
	}
	
	BIF_undo_push("Copy attributes");
}

void link_to_scene(unsigned short nr)
{	
	Scene *sce= (Scene*) BLI_findlink(&G.main->scene, G.curscreen->scenenr-1);
	Base *base, *nbase;
	
	if(sce==0) return;
	if(sce->id.lib) return;
	
	base= FIRSTBASE;
	while(base) {
		if(TESTBASE(base)) {
			
			nbase= MEM_mallocN( sizeof(Base), "newbase");
			*nbase= *base;
			BLI_addhead( &(sce->base), nbase);
			id_us_plus((ID *)base->object);
		}
		base= base->next;
	}
}

void make_links_menu()
{
	Object *ob;
	short event=0;
	char str[140];
	
	if(!(ob=OBACT)) return;
	
	strcpy(str, "Make Links %t|To Scene...%x1|%l|Object Ipo%x4");
	
	if(ob->type==OB_MESH)
		strcat(str, "|Mesh Data%x2|Materials%x3");
	else if(ob->type==OB_CURVE)
		strcat(str, "|Curve Data%x2|Materials%x3");
	else if(ob->type==OB_FONT)
		strcat(str, "|Text Data%x2|Materials%x3");
	else if(ob->type==OB_SURF)
		strcat(str, "|Surface Data%x2|Materials%x3");
	else if(ob->type==OB_MBALL)
		strcat(str, "|Materials%x3");
	else if(ob->type==OB_CAMERA)
		strcat(str, "|Camera Data%x2");
	else if(ob->type==OB_LAMP)
		strcat(str, "|Lamp Data%x2");
	else if(ob->type==OB_LATTICE)
		strcat(str, "|Lattice Data%x2");
	else if(ob->type==OB_ARMATURE)
		strcat(str, "|Armature Data%x2");

	event= pupmenu(str);

	if(event<= 0) return;
	
	make_links(event);
}

void make_links(short event)
{
	Object *ob, *obt;
	Base *base, *nbase, *sbase;
	Scene *sce = NULL;
	ID *id;
	Material ***matarar, ***obmatarar, **matar1, **matar2;
	int a;
	short *totcolp, nr;
	char *strp;

	if(!(ob=OBACT)) return;

	if(event==1) {
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), 0, &nr);
		
		if(strncmp(strp, "DataBrow", 8)==0) {
			MEM_freeN(strp);

			activate_databrowse((ID *)G.scene, ID_SCE, 0, B_INFOSCE, &(G.curscreen->scenenr), link_to_scene );
			
			return;			
		}
		else {
			event= pupmenu(strp);
			MEM_freeN(strp);
		
			if(event<= 0) return;
		
			nr= 1;
			sce= G.main->scene.first;
			while(sce) {
				if(nr==event) break;
				nr++;
				sce= sce->id.next;
			}
			if(sce==G.scene) {
				error("This is the current scene");
				return;
			}
			if(sce==0 || sce->id.lib) return;
			
			/* remember: is needed below */
			event= 1;
		}
	}

	base= FIRSTBASE;
	while(base) {
		if(event==1 || base != BASACT) {
			
			obt= base->object;

			if(TESTBASE(base)) {
				
				if(event==1) {		/* to scene */
					
					/* test if already linked */
					sbase= sce->base.first;
					while(sbase) {
						if(sbase->object==base->object) break;
						sbase= sbase->next;
					}
					if(sbase) {	/* remove */
						base= base->next;
						continue;
					}
					
					nbase= MEM_mallocN( sizeof(Base), "newbase");
					*nbase= *base;
					BLI_addhead( &(sce->base), nbase);
					id_us_plus((ID *)base->object);
				}
			}
			if(TESTBASELIB(base)) {
				if(event==2 || event==5) {  /* obdata */
					if(ob->type==obt->type) {
						
							id= obt->data;
							id->us--;
							
							id= ob->data;
							id_us_plus(id);
							obt->data= id;
							
							/* if amount of material indices changed: */
							test_object_materials(obt->data);

							obt->recalc |= OB_RECALC_DATA;
						}
					}
				else if(event==4) {  /* ob ipo */
					if(obt->ipo) obt->ipo->id.us--;
					obt->ipo= ob->ipo;
					if(obt->ipo) {
						id_us_plus((ID *)obt->ipo);
						do_ob_ipo(obt);
					}
				}
				else if(event==3) {  /* materials */
					
					/* only if obt has no material: make arrays */
					/* from ob to obt! */
					
					obmatarar= give_matarar(ob);
					matarar= give_matarar(obt);
					totcolp= give_totcolp(obt);

					/* if one of the two is zero: no render-able object */						
					if( matarar && obmatarar) {
						
						/* take care of users! so first a copy of original: */

						if(ob->totcol) {
							matar1= MEM_dupallocN(ob->mat);
							matar2= MEM_dupallocN(*obmatarar);
						}
						else {
							matar1= matar2= 0;
						}
						
						/* remove links from obt */
						for(a=0; a<obt->totcol; a++) {
							if(obt->mat[a]) obt->mat[a]->id.us--;
							if( (*matarar)[a]) (*matarar)[a]->id.us--;
						}
						
						/* free */
						if(obt->mat) MEM_freeN(obt->mat);
						if(*matarar) MEM_freeN(*matarar);
						
						/* connect a copy */
						obt->mat= matar1;
						*matarar= matar2;
						obt->totcol= ob->totcol;
						*totcolp= ob->totcol;
					
						/* increase users */
						for(a=0; a<obt->totcol; a++) {
							if(obt->mat[a]) id_us_plus((ID *)obt->mat[a]);
							if( (*matarar)[a]) id_us_plus((ID *)(*matarar)[a]);
						}

						obt->colbits= ob->colbits;
						
						/* if amount of material indices changed: */
						test_object_materials(obt->data);
					}
				}
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSHEAD, 0);

	DAG_scene_flush_update(G.scene, screen_view3d_layers());

	BIF_undo_push("Create links");
}

void make_duplilist_real()
{
	Base *base, *basen;
	Object *ob;
	extern ListBase duplilist;
	
	if(okee("Make dupli objects real")==0) return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {

			if(base->object->transflag & OB_DUPLI) {
				
				make_duplilist(G.scene, base->object);
				ob= duplilist.first;
				while(ob) {
					
					/* font duplis can have a totcol without material, we get them from parent
					 * should be implemented better...
					 */
					if(ob->mat==0) ob->totcol= 0;
					
					basen= MEM_dupallocN(base);
					basen->flag &= ~OB_FROMDUPLI;
					BLI_addhead(&G.scene->base, basen);	/* addhead: othwise eternal loop */
					ob->ipo= 0;		/* make sure apply works */
					ob->parent= ob->track= 0;
					ob->disp.first= ob->disp.last= 0;
					ob->transflag &= ~OB_DUPLI;
					basen->object= copy_object(ob);
					
					apply_obmat(basen->object);
					ob= ob->id.next;
				}
				
				free_duplilist();
				
				base->object->transflag &= ~OB_DUPLI;	
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Make duplicates real");
}

void apply_object()
{
	Base *base, *basact;
	Object *ob;
	Mesh *me;
	Curve *cu;
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	MVert *mvert;
	float mat[3][3];
	int a;

	if(G.scene->id.lib) return;
	if(G.obedit) return;
	basact= BASACT;
	
	if(G.qual & LR_SHIFTKEY) {
		ob= OBACT;
		if(ob==0) return;
		
		if(ob->transflag & OB_DUPLI) {
			make_duplilist_real();
		}
		else {
			if(okee("Apply deformation")) {
				object_apply_deform(ob);
				BIF_undo_push("Apply deformation");
			}
		}
		allqueue(REDRAWVIEW3D, 0);

		return;
	}

	if(okee("Apply size and rotation")==0) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			ob= base->object;
	
			if(ob->type==OB_MESH) {
				object_to_mat3(ob, mat);
				me= ob->data;
				
				if(me->id.us>1) {
					error("Can't apply to a multi user mesh");
					return;
				}
				if(me->key) {
					error("Can't apply to a mesh with vertex keys");
					return;
				}
				
				mvert= me->mvert;
				for(a=0; a<me->totvert; a++, mvert++) {
					Mat3MulVecfl(mat, mvert->co);
				}
				ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				QuatOne(ob->quat);
				
				where_is_object(ob);
				
				/* texspace and normals */
				BASACT= base;
				enter_editmode();
				BIF_undo_push("Applied object");	// editmode undo itself
				exit_editmode(1); // freedata, but no undo
				BASACT= basact;				
				
			}
			else if (ob->type==OB_ARMATURE){
				bArmature *arm;

				object_to_mat3(ob, mat);
				arm= ob->data;
				if(arm->id.us>1) {
					error("Can't apply to a multi user armature");
					return;
				}

				apply_rot_armature (ob, mat);
				/* Reset the object's transforms */
				ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				QuatOne(ob->quat);
				
				where_is_object(ob);
			}
			else if ELEM(ob->type, OB_CURVE, OB_SURF) {
				object_to_mat3(ob, mat);
				cu= ob->data;
				
				if(cu->id.us>1) {
					error("Can't apply to a multi user curve");
					return;
				}
				if(cu->key) {
					error("Can't apply to a curve with vertex keys");
					return;
				}
				
				nu= cu->nurb.first;
				while(nu) {
					if( (nu->type & 7)==1) {
						a= nu->pntsu;
						bezt= nu->bezt;
						while(a--) {
							Mat3MulVecfl(mat, bezt->vec[0]);
							Mat3MulVecfl(mat, bezt->vec[1]);
							Mat3MulVecfl(mat, bezt->vec[2]);
							bezt++;
						}
					}
					else {
						a= nu->pntsu*nu->pntsv;
						bp= nu->bp;
						while(a--) {
							Mat3MulVecfl(mat, bp->vec);
							bp++;
						}
					}
					nu= nu->next;
				}
			
				ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				QuatOne(ob->quat);
				
				where_is_object(ob);
				
				/* texspace and normals */
				BASACT= base;
				enter_editmode();
				BIF_undo_push("Applied object");	// editmode undo itself
				exit_editmode(1); // freedata, but no undo
				BASACT= basact;
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Apply object");
}



/* ************ GENERAL  *************** */


/* now only used in 2d spaces, like ipo, nla, sima... */
void apply_keyb_grid(float *val, float fac1, float fac2, float fac3, int invert)
{
	/* fac1 is for 'nothing', fac2 for CTRL, fac3 for SHIFT */
	int ctrl;

	if(invert) {
		if(G.qual & LR_CTRLKEY) ctrl= 0;
		else ctrl= 1;
	}
	else ctrl= (G.qual & LR_CTRLKEY);

	if(ctrl && (G.qual & LR_SHIFTKEY)) {
		if(fac3!= 0.0) *val= fac3*floor(*val/fac3 +.5);
	}
	else if(ctrl) {
		if(fac2!= 0.0) *val= fac2*floor(*val/fac2 +.5);
	}
	else {
		if(fac1!= 0.0) *val= fac1*floor(*val/fac1 +.5);
	}
}

/* exported to transform.c */
void compatible_eul(float *eul, float *oldrot)
{
	float dx, dy, dz;
	
	/* correct differences of about 360 degrees first */

	dx= eul[0] - oldrot[0];
	dy= eul[1] - oldrot[1];
	dz= eul[2] - oldrot[2];

	while( fabs(dx) > 5.1) {
		if(dx > 0.0) eul[0] -= 2.0*M_PI; else eul[0]+= 2.0*M_PI;
		dx= eul[0] - oldrot[0];
	}
	while( fabs(dy) > 5.1) {
		if(dy > 0.0) eul[1] -= 2.0*M_PI; else eul[1]+= 2.0*M_PI;
		dy= eul[1] - oldrot[1];
	}
	while( fabs(dz) > 5.1 ) {
		if(dz > 0.0) eul[2] -= 2.0*M_PI; else eul[2]+= 2.0*M_PI;
		dz= eul[2] - oldrot[2];
	}
	
	/* is 1 of the axis rotations larger than 180 degrees and the other small? NO ELSE IF!! */	
	if( fabs(dx) > 3.2 && fabs(dy)<1.6 && fabs(dz)<1.6 ) {
		if(dx > 0.0) eul[0] -= 2.0*M_PI; else eul[0]+= 2.0*M_PI;
	}
	if( fabs(dy) > 3.2 && fabs(dz)<1.6 && fabs(dx)<1.6 ) {
		if(dy > 0.0) eul[1] -= 2.0*M_PI; else eul[1]+= 2.0*M_PI;
	}
	if( fabs(dz) > 3.2 && fabs(dx)<1.6 && fabs(dy)<1.6 ) {
		if(dz > 0.0) eul[2] -= 2.0*M_PI; else eul[2]+= 2.0*M_PI;
	}

	return; /* <- intersting to find out who did that! */
	
	/* calc again */
	dx= eul[0] - oldrot[0];
	dy= eul[1] - oldrot[1];
	dz= eul[2] - oldrot[2];

	/* special case, tested for x-z  */
	
	if( (fabs(dx) > 3.1 && fabs(dz) > 1.5 ) || ( fabs(dx) > 1.5 && fabs(dz) > 3.1 ) ) {
		if(dx > 0.0) eul[0] -= M_PI; else eul[0]+= M_PI;
		if(eul[1] > 0.0) eul[1]= M_PI - eul[1]; else eul[1]= -M_PI - eul[1];
		if(dz > 0.0) eul[2] -= M_PI; else eul[2]+= M_PI;
		
	}
	else if( (fabs(dx) > 3.1 && fabs(dy) > 1.5 ) || ( fabs(dx) > 1.5 && fabs(dy) > 3.1 ) ) {
		if(dx > 0.0) eul[0] -= M_PI; else eul[0]+= M_PI;
		if(dy > 0.0) eul[1] -= M_PI; else eul[1]+= M_PI;
		if(eul[2] > 0.0) eul[2]= M_PI - eul[2]; else eul[2]= -M_PI - eul[2];
	}
	else if( (fabs(dy) > 3.1 && fabs(dz) > 1.5 ) || ( fabs(dy) > 1.5 && fabs(dz) > 3.1 ) ) {
		if(eul[0] > 0.0) eul[0]= M_PI - eul[0]; else eul[0]= -M_PI - eul[0];
		if(dy > 0.0) eul[1] -= M_PI; else eul[1]+= M_PI;
		if(dz > 0.0) eul[2] -= M_PI; else eul[2]+= M_PI;
	}

}


int cylinder_intersect_test(void)
{
	EditMesh *em = G.editMesh;
	float *oldloc, speed[3], s, t, labda, labdacor, dist, len, len2, axis[3], *base, rc[3], n[3], o[3];
	EditVert *v1;
	
	v1= em->verts.first;

	base= v1->co;
	v1= v1->next;
	VecSubf(axis, v1->co, base);
	
	v1= v1->next;
	oldloc= v1->co;
	v1= v1->next;
	VecSubf(speed, v1->co, oldloc);
	
	VecSubf(rc, oldloc, base);
	
	/* the axis */
	len2= Normalise(axis);
	
	Crossf(n, speed, axis);
	len= Normalise(n);
	if(len==0.0) return 0;
	
	dist= fabs( rc[0]*n[0] + rc[1]*n[1] + rc[2]*n[2] );
	
	if( dist>=G.scene->editbutsize ) return 0;
	
	Crossf(o, rc, axis);
	t= -(o[0]*n[0] + o[1]*n[1] + o[2]*n[2])/len;
	
	Crossf(o, n, axis);
	s=  fabs(sqrt(G.scene->editbutsize*G.scene->editbutsize-dist*dist) / (o[0]*speed[0] + o[1]*speed[1] + o[2]*speed[2]));
	
	labdacor= t-s;
	labda= t+s;

	/* two cases with no intersection point */
	if(labdacor>=1.0 && labda>=1.0) return 0;
	if(labdacor<=0.0 && labda<=0.0) return 0;
	
	/* calc normal */
	/* intersection: */
	
	rc[0]= oldloc[0] + labdacor*speed[0] - base[0];
	rc[1]= oldloc[1] + labdacor*speed[1] - base[1];
	rc[2]= oldloc[2] + labdacor*speed[2] - base[2];
	
	s= (rc[0]*axis[0] + rc[1]*axis[1] + rc[2]*axis[2]) ;
	
	if(s<0.0 || s>len2) return 0;
	
	n[0]= (rc[0] - s*axis[0]);
	n[1]= (rc[1] - s*axis[1]);
	n[2]= (rc[2] - s*axis[2]);

	printf("var1: %f, var2: %f, var3: %f\n", labdacor, len2, s);	
	printf("var1: %f, var2: %f, var3: %f\n", rc[0], rc[1], rc[2]);	
	printf("var1: %f, var2: %f, var3: %f\n", n[0], n[1], n[2]);	

	return 1;
}

int sphere_intersect_test(void)
{
	EditMesh *em = G.editMesh;
	float *oldloc, speed[3], labda, labdacor, len, bsq, u, disc, *base, rc[3];
	EditVert *v1;
	
	v1= em->verts.first;
	base= v1->co;
	
	v1= v1->next;
	oldloc= v1->co;
	
	v1= v1->next;
	VecSubf(speed, v1->co, oldloc);
	len= Normalise(speed);
	if(len==0.0) return 0;
	
	VecSubf(rc, oldloc, base);
	bsq= rc[0]*speed[0] + rc[1]*speed[1] + rc[2]*speed[2]; 
	u= rc[0]*rc[0] + rc[1]*rc[1] + rc[2]*rc[2] - G.scene->editbutsize*G.scene->editbutsize;

	disc= bsq*bsq - u;
	
	if(disc>=0.0) {
		disc= sqrt(disc);
		labdacor= (-bsq - disc)/len;	/* entry point */
		labda= (-bsq + disc)/len;
		
		printf("var1: %f, var2: %f, var3: %f\n", labdacor, labda, G.scene->editbutsize);
	}
	else return 0;

	/* intersection and normal */
	rc[0]= oldloc[0] + labdacor*speed[0] - base[0];
	rc[1]= oldloc[1] + labdacor*speed[1] - base[1];
	rc[2]= oldloc[2] + labdacor*speed[2] - base[2];


	return 1;
}


void std_rmouse_transform(void (*xf_func)(int, int))
{
	short mval[2];
	short xo, yo;
	short timer=0;
	short mousebut;
	
	/* check for left mouse select/right mouse select user pref */
	if (U.flag & USER_LMOUSESELECT) mousebut = L_MOUSE;
		else mousebut = R_MOUSE;
	
	getmouseco_areawin(mval);
	xo= mval[0]; 
	yo= mval[1];
	
	while(get_mbut() & mousebut) {
		getmouseco_areawin(mval);
		if(abs(mval[0]-xo)+abs(mval[1]-yo) > 10) {
			if(curarea->spacetype==SPACE_VIEW3D) {
#ifdef TWEAK_MODE
				initTransform(TFM_TRANSLATION, CTX_TWEAK);
#else
				initTransform(TFM_TRANSLATION, CTX_NONE);
#endif
				Transform();
			}
			else if(curarea->spacetype==SPACE_IMAGE) {
				initTransform(TFM_TRANSLATION, CTX_NONE);
				Transform();
			}
			else if(xf_func)
				xf_func('g', 0);

			while(get_mbut() & mousebut) BIF_wait_for_statechange();
			return;
		}
		else {
			PIL_sleep_ms(10);
			timer++;
			if(timer>=10*U.tb_rightmouse) {
				toolbox_n();
				return;
			}
		}
	}
	/* if gets here it's a select */
	BIF_undo_push("Select");
}

void rightmouse_transform(void)
{
	std_rmouse_transform(NULL);
}


/* ************************************** */


static void single_object_users__forwardModifierLinks(void *userData, Object *ob, Object **obpoin)
{
	ID_NEW(*obpoin);
}
void single_object_users(int flag)	
{
	Base *base;
	Object *ob, *obn;
	
	clear_sca_new_poins();	/* sensor/contr/act */

	/* duplicate */
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		
		if( (base->flag & flag)==flag) {

			if(ob->id.lib==NULL && ob->id.us>1) {
			
				obn= copy_object(ob);
				ob->id.us--;
				base->object= obn;
			}
		}
		base= base->next;
	}
	
	ID_NEW(G.scene->camera);
	if(G.vd) ID_NEW(G.vd->camera);
	
	/* object pointers */
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(ob->id.lib==NULL) {
			if( (base->flag & flag)==flag) {
				
				relink_constraints(&base->object->constraints);
				if (base->object->pose){
					bPoseChannel *chan;
					for (chan = base->object->pose->chanbase.first; chan; chan=chan->next){
						relink_constraints(&chan->constraints);
					}
				}
				modifiers_foreachObjectLink(base->object, single_object_users__forwardModifierLinks, NULL);
				
				ID_NEW(ob->parent);
				ID_NEW(ob->track);
			}
		}
		base= base->next;
	}

	set_sca_new_poins();
}

void new_id_matar(Material **matar, int totcol)
{
	ID *id;
	int a;
	
	for(a=0; a<totcol; a++) {
		id= (ID *)matar[a];
		if(id && id->lib==0) {
			if(id->newid) {
				matar[a]= (Material *)id->newid;
				id_us_plus(id->newid);
				id->us--;
			}
			else if(id->us>1) {
				matar[a]= copy_material(matar[a]);
				id->us--;
				id->newid= (ID *)matar[a];
			}
		}
	}
}

void single_obdata_users(int flag)
{
	Object *ob;
	Lamp *la;
	Curve *cu;
	Camera *cam;
	Base *base;
	Mesh *me;
	ID *id;
	int a;
	
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(ob->id.lib==NULL && (base->flag & flag)==flag ) {
			id= ob->data;
			
			if(id && id->us>1 && id->lib==0) {
				
				switch(ob->type) {
				case OB_LAMP:
					if(id && id->us>1 && id->lib==0) {
						ob->data= la= copy_lamp(ob->data);
						for(a=0; a<MAX_MTEX; a++) {
							if(la->mtex[a]) {
								ID_NEW(la->mtex[a]->object);
							}
						}
					}
					break;
				case OB_CAMERA:
					ob->data= copy_camera(ob->data);
					break;
				case OB_MESH:
					ob->data= copy_mesh(ob->data);
					break;
				case OB_MBALL:
					ob->data= copy_mball(ob->data);
					break;
				case OB_CURVE:
				case OB_SURF:
				case OB_FONT:
					ob->data= cu= copy_curve(ob->data);
					ID_NEW(cu->bevobj);
					ID_NEW(cu->taperobj);
					break;
				case OB_LATTICE:
					ob->data= copy_lattice(ob->data);
					break;
				case OB_ARMATURE:
					ob->recalc |= OB_RECALC_DATA;
					ob->data= copy_armature(ob->data);
					armature_rebuild_pose(ob, ob->data);
					break;
				default:
					printf("ERROR single_obdata_users: %s\n", id->name);
					error("Read console");
					return;
				}
				
				id->us--;
				id->newid= ob->data;
				
			}
			
			id= (ID *)ob->action;
			if (id && id->us>1 && id->lib==0){
				if(id->newid){
					ob->action= (bAction *)id->newid;
					id_us_plus(id->newid);
				}
				else {
					ob->action= copy_action(ob->action);
					id->us--;
					id->newid=(ID *)ob->action;
				}
			}
			id= (ID *)ob->ipo;
			if(id && id->us>1 && id->lib==0) {
				if(id->newid) {
					ob->ipo= (Ipo *)id->newid;
					id_us_plus(id->newid);
				}
				else {
					ob->ipo= copy_ipo(ob->ipo);
					id->us--;
					id->newid= (ID *)ob->ipo;
				}
			}
			/* other ipos */
			switch(ob->type) {
			case OB_LAMP:
				la= ob->data;
				if(la->ipo && la->ipo->id.us>1) {
					la->ipo->id.us--;
					la->ipo= copy_ipo(la->ipo);
				}
				break;
			case OB_CAMERA:
				cam= ob->data;
				if(cam->ipo && cam->ipo->id.us>1) {
					cam->ipo->id.us--;
					cam->ipo= copy_ipo(cam->ipo);
				}
				break;
			}
			
		}
		base= base->next;
	}
	
	me= G.main->mesh.first;
	while(me) {
		ID_NEW(me->texcomesh);
		me= me->id.next;
	}
}


void single_mat_users(int flag)
{
	Object *ob;
	Base *base;
	Material *ma, *man;
	Tex *tex;
	int a, b;
	
	
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(ob->id.lib==NULL && (flag==0 || (base->flag & SELECT)) ) {
	
			for(a=1; a<=ob->totcol; a++) {
				ma= give_current_material(ob, a);
				if(ma) {
					/* do not test for LIB_NEW: this functions guaranteed delivers single_users! */
					
					if(ma->id.us>1) {
						man= copy_material(ma);
					
						man->id.us= 0;
						assign_material(ob, man, a);
						
						if(ma->ipo) {
							man->ipo= copy_ipo(ma->ipo);
							ma->ipo->id.us--;
						}
						
						for(b=0; b<MAX_MTEX; b++) {
							if(ma->mtex[b] && ma->mtex[b]->tex) {
								tex= ma->mtex[b]->tex;
								if(tex->id.us>1) {
									ma->mtex[b]->tex= copy_texture(tex);
									tex->id.us--;
								}
							}
						}
						
					}
				}
			}
		}
		base= base->next;
	}
}

void do_single_tex_user(Tex **from)
{
	Tex *tex, *texn;
	
	tex= *from;
	if(tex==0) return;
	
	if(tex->id.newid) {
		*from= (Tex *)tex->id.newid;
		id_us_plus(tex->id.newid);
		tex->id.us--;
	}
	else if(tex->id.us>1) {
		texn= copy_texture(tex);
		tex->id.newid= (ID *)texn;
		tex->id.us--;
		*from= texn;
	}
	
}

void single_tex_users_expand()
{
	/* only when 'parent' blocks are LIB_NEW */
	Material *ma;
	Lamp *la;
	World *wo;
	int b;
		
	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(ma->mtex[b] && ma->mtex[b]->tex) {
					do_single_tex_user( &(ma->mtex[b]->tex) );
				}
			}
		}
		ma= ma->id.next;
	}

	la= G.main->lamp.first;
	while(la) {
		if(la->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(la->mtex[b] && la->mtex[b]->tex) {
					do_single_tex_user( &(la->mtex[b]->tex) );
				}
			}
		}
		la= la->id.next;
	}
	wo= G.main->world.first;
	while(wo) {
		if(wo->id.flag & LIB_NEW) {
			for(b=0; b<MAX_MTEX; b++) {
				if(wo->mtex[b] && wo->mtex[b]->tex) {
					do_single_tex_user( &(wo->mtex[b]->tex) );
				}
			}
		}
		wo= wo->id.next;
	}
}

void single_mat_users_expand(void)
{
	/* only when 'parent' blocks are LIB_NEW */

	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material *ma;
	int a;
	
	ob= G.main->object.first;
	while(ob) {
		if(ob->id.flag & LIB_NEW) {
			new_id_matar(ob->mat, ob->totcol);
		}
		ob= ob->id.next;
	}

	me= G.main->mesh.first;
	while(me) {
		if(me->id.flag & LIB_NEW) {
			new_id_matar(me->mat, me->totcol);
		}
		me= me->id.next;
	}

	cu= G.main->curve.first;
	while(cu) {
		if(cu->id.flag & LIB_NEW) {
			new_id_matar(cu->mat, cu->totcol);
		}
		cu= cu->id.next;
	}

	mb= G.main->mball.first;
	while(mb) {
		if(mb->id.flag & LIB_NEW) {
			new_id_matar(mb->mat, mb->totcol);
		}
		mb= mb->id.next;
	}

	/* material imats  */
	ma= G.main->mat.first;
	while(ma) {
		if(ma->id.flag & LIB_NEW) {
			for(a=0; a<MAX_MTEX; a++) {
				if(ma->mtex[a]) {
					ID_NEW(ma->mtex[a]->object);
				}
			}
		}
		ma= ma->id.next;
	}
}

void single_user(void)
{
	int nr;
	
	if(G.scene->id.lib) return;

	clear_id_newpoins();
	
	nr= pupmenu("Make Single User%t|Object|Object & ObData|Object & ObData & Materials+Tex|Materials+Tex");
	if(nr>0) {
	
		if(nr==1) single_object_users(1);
	
		else if(nr==2) {
			single_object_users(1);
			single_obdata_users(1);
		}
		else if(nr==3) {
			single_object_users(1);
			single_obdata_users(1);
			single_mat_users(1); /* also tex */
			
		}
		else if(nr==4) {
			single_mat_users(1);
		}
		
		clear_id_newpoins();

		countall();
		allqueue(REDRAWALL, 0);
		BIF_undo_push("Single user");
	}
}

/* ************************************************************* */


void make_local(void)
{
	Base *base;
	Object *ob;
	Material *ma, ***matarar;
	Lamp *la;
	Curve *cu;
	ID *id;
	int a, b, mode;
	
	/* WATCH: the function new_id(..) re-inserts the id block!!! */
	
	if(G.scene->id.lib) return;
	
	mode= pupmenu("Make Local%t|Selected %x1|All %x2");
	
	if(mode==2) {
		all_local(NULL);	// NULL is all libs
		allqueue(REDRAWALL, 0);
		return;
	}
	else if(mode!=1) return;

	clear_id_newpoins();
	
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if( (base->flag & SELECT)) {
			if(ob->id.lib) {
				make_local_object(ob);
			}
		}
		base= base->next;
	}
	
	/* maybe object pointers */
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if( (base->flag & SELECT)) {
			if(ob->id.lib==NULL) {
				ID_NEW(ob->parent);
				ID_NEW(ob->track);
			}
		}
		base= base->next;
	}

	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if( (base->flag & SELECT) ) {
	
			id= ob->data;
			
			if(id) {
				
				switch(ob->type) {
				case OB_LAMP:
					make_local_lamp((Lamp *)id);
					
					la= ob->data;
					id= (ID *)la->ipo;
					if(id && id->lib) make_local_ipo(la->ipo);
					
					break;
				case OB_CAMERA:
					make_local_camera((Camera *)id);
					break;
				case OB_MESH:
					make_local_mesh((Mesh *)id);
					make_local_key( ((Mesh *)id)->key );
					break;
				case OB_MBALL:
					make_local_mball((MetaBall *)id);
					break;
				case OB_CURVE:
				case OB_SURF:
				case OB_FONT:
					cu= (Curve *)id;
					make_local_curve(cu);
					id= (ID *)cu->ipo;
					if(id && id->lib) make_local_ipo(cu->ipo);
					make_local_key( cu->key );
					break;
				case OB_LATTICE:
					make_local_lattice((Lattice *)id);
					make_local_key( ((Lattice *)id)->key );
					break;
				case OB_ARMATURE:
					make_local_armature ((bArmature *)id);
					break;
				}
			}
			id= (ID *)ob->ipo;
			if(id && id->lib) make_local_ipo(ob->ipo);

			id= (ID *)ob->action;
			if(id && id->lib) make_local_action(ob->action);
		}
		base= base->next;		
	}

	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(base->flag & SELECT ) {
			
			if(ob->type==OB_LAMP) {
				la= ob->data;
				for(b=0; b<MAX_MTEX; b++) {
					if(la->mtex[b] && la->mtex[b]->tex) {
						make_local_texture(la->mtex[b]->tex);
					}
				}
			}
			else {
				
				for(a=0; a<ob->totcol; a++) {
					ma= ob->mat[a];
					if(ma) {
						make_local_material(ma);
					
						for(b=0; b<MAX_MTEX; b++) {
							if(ma->mtex[b] && ma->mtex[b]->tex) {
								make_local_texture(ma->mtex[b]->tex);
							}
						}
						id= (ID *)ma->ipo;
						if(id && id->lib) make_local_ipo(ma->ipo);	
					}
				}
				
				matarar= (Material ***)give_matarar(ob);
				
				for(a=0; a<ob->totcol; a++) {
					ma= (*matarar)[a];
					if(ma) {
						make_local_material(ma);
					
						for(b=0; b<MAX_MTEX; b++) {
							if(ma->mtex[b] && ma->mtex[b]->tex) {
								make_local_texture(ma->mtex[b]->tex);
							}
						}
						id= (ID *)ma->ipo;
						if(id && id->lib) make_local_ipo(ma->ipo);	
					}
				}
			}
		}
		base= base->next;
	}


	allqueue(REDRAWALL, 0);
	BIF_undo_push("Make local");
}

static void adduplicate__forwardModifierLinks(void *userData, Object *ob, Object **obpoin)
{
	ID_NEW(*obpoin);
}
void adduplicate(int noTrans)
/* dtrans is 3 x 3xfloat dloc, drot en dsize */
{
	Base *base, *basen;
	Object *ob, *obn;
	Material ***matarar, *ma, *mao;
	ID *id;
	Ipo *ipo;
	bConstraintChannel *chan;
	int a, didit, dupflag;
	
	if(G.scene->id.lib) return;
	clear_id_newpoins();
	clear_sca_new_poins();	/* sensor/contr/act */
	
	if( G.qual & LR_ALTKEY ) dupflag= 0;
	else dupflag= U.dupflag;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
		
			ob= base->object;
			if(ob->flag & OB_POSEMODE) {
				; // nothing?
			}
			else {
				obn= copy_object(ob);
				obn->recalc |= OB_RECALC;
				
				basen= MEM_mallocN(sizeof(Base), "duplibase");
				*basen= *base;
				BLI_addhead(&G.scene->base, basen);	/* addhead: prevent eternal loop */
				basen->object= obn;
				base->flag &= ~SELECT;
				basen->flag &= ~OB_FROMGROUP;
				
				if(BASACT==base) BASACT= basen;

				/* duplicates using userflags */
				
				if(dupflag & USER_DUP_IPO) {
					id= (ID *)obn->ipo;
					if(id) {
						ID_NEW_US( obn->ipo)
						else obn->ipo= copy_ipo(obn->ipo);
						id->us--;
					}
					/* Handle constraint ipos */
					for (chan=obn->constraintChannels.first; chan; chan=chan->next){
						id= (ID *)chan->ipo;
						if(id) {
							ID_NEW_US( chan->ipo)
							else chan->ipo= copy_ipo(chan->ipo);
							id->us--;
						}
					}
				}
				if(dupflag & USER_DUP_ACT){
					id= (ID *)obn->action;
					if (id){
						ID_NEW_US(obn->action)
						else{
							obn->action= copy_action(obn->action);
						}
						id->us--;
					}
				}
				if(dupflag & USER_DUP_MAT) {
					for(a=0; a<obn->totcol; a++) {
						id= (ID *)obn->mat[a];
						if(id) {
							ID_NEW_US(obn->mat[a])
							else obn->mat[a]= copy_material(obn->mat[a]);
							id->us--;
						}
					}
				}
				
				id= obn->data;
				didit= 0;
				
				switch(obn->type) {
				case OB_MESH:
					if(dupflag & USER_DUP_MESH) {
						ID_NEW_US2( obn->data )
						else {
							obn->data= copy_mesh(obn->data);
							didit= 1;
						}
						id->us--;
					}
					break;
				case OB_CURVE:
					if(dupflag & USER_DUP_CURVE) {
						ID_NEW_US2(obn->data )
						else {
							obn->data= copy_curve(obn->data);
							didit= 1;
						}
						id->us--;
					}
					break;
				case OB_SURF:
					if(dupflag & USER_DUP_SURF) {
						ID_NEW_US2( obn->data )
						else {
							obn->data= copy_curve(obn->data);
							didit= 1;
						}
						id->us--;
					}
					break;
				case OB_FONT:
					if(dupflag & USER_DUP_FONT) {
						ID_NEW_US2( obn->data )
						else {
							obn->data= copy_curve(obn->data);
							didit= 1;
						}
						id->us--;
					}
					break;
				case OB_MBALL:
					if(dupflag & USER_DUP_MBALL) {
						ID_NEW_US2(obn->data )
						else {
							obn->data= copy_mball(obn->data);
							didit= 1;
						}
						id->us--;
					}
					break;
				case OB_LAMP:
					if(dupflag & USER_DUP_LAMP) {
						ID_NEW_US2(obn->data )
						else obn->data= copy_lamp(obn->data);
						id->us--;
					}
					break;

				case OB_ARMATURE:
					obn->recalc |= OB_RECALC_DATA;
					if(obn->pose) obn->pose->flag |= POSE_RECALC;
					
					if(dupflag & USER_DUP_ARM) {
						ID_NEW_US2(obn->data )
						else {
							obn->data= copy_armature(obn->data);
							armature_rebuild_pose(obn, obn->data);
							didit= 1;
						}
						id->us--;
					}
					
					break;
					
				/* always dupli's */
				case OB_LATTICE:
					ID_NEW_US2(obn->data )
					else obn->data= copy_lattice(obn->data);
					id->us--;
					break;
				case OB_CAMERA:
					ID_NEW_US2(obn->data )
					else obn->data= copy_camera(obn->data);
					id->us--;
					break;
				}
				
				if(dupflag & USER_DUP_MAT) {
					matarar= give_matarar(obn);
					if(didit && matarar) {
						for(a=0; a<obn->totcol; a++) {
							id= (ID *)(*matarar)[a];
							if(id) {
								ID_NEW_US( (*matarar)[a] )
								else (*matarar)[a]= copy_material((*matarar)[a]);
								
								id->us--;
							}
						}
					}
				}
			}
						
		}
		base= base->next;
	}
	
	/* check object pointers */
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			relink_constraints(&base->object->constraints);
			if (base->object->pose){
				bPoseChannel *chan;
				for (chan = base->object->pose->chanbase.first; chan; chan=chan->next){
					relink_constraints(&chan->constraints);
				}
			}
			modifiers_foreachObjectLink(base->object, adduplicate__forwardModifierLinks, NULL);
			ID_NEW(base->object->parent);
			ID_NEW(base->object->track);
			
		}
		base= base->next;
	}
	
	/* ipos */
	ipo= G.main->ipo.first;
	while(ipo) {
		if(ipo->id.lib==NULL && ipo->id.newid) {
			IpoCurve *icu;
			for(icu= ipo->curve.first; icu; icu= icu->next) {
				if(icu->driver) {
					ID_NEW(icu->driver->ob);
				}
			}
		}
		ipo= ipo->id.next;
	}
	
	/* materials */
	if( dupflag & USER_DUP_MAT) {
		mao= G.main->mat.first;
		while(mao) {
			if(mao->id.newid) {
				
				ma= (Material *)mao->id.newid;
				
				if(dupflag & USER_DUP_TEX) {
					for(a=0; a<MAX_MTEX; a++) {
						if(ma->mtex[a]) {
							id= (ID *)ma->mtex[a]->tex;
							if(id) {
								ID_NEW_US(ma->mtex[a]->tex)
								else ma->mtex[a]->tex= copy_texture(ma->mtex[a]->tex);
								id->us--;
							}
						}
					}
				}
				id= (ID *)ma->ipo;
				if(id) {
					ID_NEW_US(ma->ipo)
					else ma->ipo= copy_ipo(ma->ipo);
					id->us--;
				}
			}
			mao= mao->id.next;
		}
	}

	DAG_scene_sort(G.scene);
	DAG_scene_flush_update(G.scene, screen_view3d_layers());
	set_sca_new_poins();

	clear_id_newpoins();
	
	countall();
	if(!noTrans) {
		BIF_TransformSetUndo("Add Duplicate");
		initTransform(TFM_TRANSLATION, CTX_NONE);
		Transform();
	}
	set_active_base(BASACT);
	
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);	/* also oops */
	allqueue(REDRAWIPO, 0);	/* also oops */
}

void selectlinks_menu(void)
{
	Object *ob;
	int nr;
	
	ob= OBACT;
	if(ob==0) return;
	
	/* If you modify this menu, please remember to update view3d_select_linksmenu
	 * in header_view3d.c and the menu in toolbox.c
	 */
	nr= pupmenu("Select Linked%t|Object Ipo%x1|ObData%x2|Material%x3|Texture%x4");
	
	if (nr <= 0) return;
	
	selectlinks(nr);
}

void selectlinks(int nr)
{
	Object *ob;
	Base *base;
	void *obdata = NULL;
	Ipo *ipo = NULL;
	Material *mat = NULL, *mat1;
	Tex *tex=0;
	int a, b;
	
	/* events (nr):
	 * Object Ipo: 1
	 * ObData: 2
	 * Current Material: 3
	 * Current Texture: 4
	 */
	
	
	ob= OBACT;
	if(ob==0) return;
	
	if(nr==1) {
		ipo= ob->ipo;
		if(ipo==0) return;
	}
	else if(nr==2) {
		if(ob->data==0) return;
		obdata= ob->data;
	}
	else if(nr==3 || nr==4) {
		mat= give_current_material(ob, ob->actcol);
		if(mat==0) return;
		if(nr==4) {
			if(mat->mtex[ mat->texact ]) tex= mat->mtex[ mat->texact ]->tex;
			if(tex==0) return;
		}
	}
	else return;
	
	base= FIRSTBASE;
	while(base) {
		if(base->lay & G.vd->lay) {
			if(nr==1) {
				if(base->object->ipo==ipo) base->flag |= SELECT;
			}
			else if(nr==2) {
				if(base->object->data==obdata) base->flag |= SELECT;
			}
			else if(nr==3 || nr==4) {
				ob= base->object;
				
				for(a=1; a<=ob->totcol; a++) {
					mat1= give_current_material(ob, a);
					if(nr==3) {
						if(mat1==mat) base->flag |= SELECT;
					}
					else if(mat1 && nr==4) {
						for(b=0; b<MAX_MTEX; b++) {
							if(mat1->mtex[b]) {
								if(tex==mat1->mtex[b]->tex) base->flag |= SELECT;
							}
						}
					}
				}
			}
			base->object->flag= base->flag;
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWDATASELECT, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Select links");
}

void image_aspect(void)
{
	/* all selected objects with an image map: scale in image aspect */
	Base *base;
	Object *ob;
	Material *ma;
	Tex *tex;
	float x, y, space;
	int a, b, done;
	
	if(G.obedit) return;
	if(G.scene->id.lib) return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			ob= base->object;
			done= 0;
			
			for(a=1; a<=ob->totcol; a++) {
				ma= give_current_material(ob, a);
				if(ma) {
					for(b=0; b<MAX_MTEX; b++) {
						if(ma->mtex[b] && ma->mtex[b]->tex) {
							tex= ma->mtex[b]->tex;
							if(tex->type==TEX_IMAGE && tex->ima && tex->ima->ibuf) {

								/* texturespace */
								space= 1.0;
								if(ob->type==OB_MESH) {
									float size[3];
									mesh_get_texspace(ob->data, NULL, NULL, size);
									space= size[0]/size[1];
								}
								else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
									Curve *cu= ob->data;
									space= cu->size[0]/cu->size[1];
								}
							
								x= tex->ima->ibuf->x/space;
								y= tex->ima->ibuf->y;
								
								if(x>y) ob->size[0]= ob->size[1]*x/y;
								else ob->size[1]= ob->size[0]*y/x;
								
								done= 1;
								DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);								
							}
						}
						if(done) break;
					}
				}
				if(done) break;
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	BIF_undo_push("Image aspect");
}

void set_ob_ipoflags(void)
{
	Base *base;
	int set= 1;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			if(base->object->ipoflag & OB_DRAWKEY) {
				set= 0;
				break;
			}
		}
		base= base->next;
	}
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			if(set) {
				base->object->ipoflag |= OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 1;
			}
			else {
				base->object->ipoflag &= ~OB_DRAWKEY;
			}
		}
		base= base->next;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	if(set) {
		allqueue(REDRAWNLA, 0);
		allqueue (REDRAWACTION, 0);
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWIPO, 0);
	}
}

void select_select_keys(void)
{
	Base *base;
	IpoCurve *icu;
	BezTriple *bezt;
	int a;
	
	if(G.scene->id.lib) return;

	if(okee("Show and select all keys")==0) return;

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base->object->ipo) {
				base->object->ipoflag |= OB_DRAWKEY;
				base->object->ipo->showkey= 1;
				icu= base->object->ipo->curve.first;
				while(icu) {
					a= icu->totvert;
					bezt= icu->bezt;
					while(a--) {
						bezt->f1 |= SELECT;
						bezt->f2 |= SELECT;
						bezt->f3 |= SELECT;
						bezt++;
					}
					icu= icu->next;
				}
			}
		}
		base= base->next;
	}

	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWVIEW3D, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);

	BIF_undo_push("Select keys");

}


int vergbaseco(const void *a1, const void *a2)
{
	Base **x1, **x2;
	
	x1= (Base **) a1;
	x2= (Base **) a2;
	
	if( (*x1)->sy > (*x2)->sy ) return 1;
	else if( (*x1)->sy < (*x2)->sy) return -1;
	else if( (*x1)->sx > (*x2)->sx ) return 1;
	else if( (*x1)->sx < (*x2)->sx ) return -1;

	return 0;
}


void auto_timeoffs(void)
{
	Base *base, **basesort, **bs;
	float start, delta;
	int tot=0, a;
	short offset=25;

	if(BASACT==0) return;
	if(button(&offset, 0, 1000,"Total time")==0) return;

	/* make array of all bases, xco yco (screen) */
	base= FIRSTBASE;
	while(base) {
		if(TESTBASELIB(base)) {
			tot++;
		}
		base= base->next;
	}

	delta= (float)offset/(float)tot;
	start= OBACT->sf;

	bs= basesort= MEM_mallocN(sizeof(void *)*tot,"autotimeoffs");
	base= FIRSTBASE;

	while(base) {
		if(TESTBASELIB(base)) {
			*bs= base;
			bs++;
		}
		base= base->next;
	}
	qsort(basesort, tot, sizeof(void *), vergbaseco);

	bs= basesort;
	for(a=0; a<tot; a++) {
		
		(*bs)->object->sf= start;
		start+= delta;

		bs++;
	}
	MEM_freeN(basesort);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

void texspace_edit(void)
{
	Base *base;
	int nr=0;
	
	/* first test if from visible and selected objects
	 * texspacedraw is set:
	 */
	
	if(G.obedit) return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			break;
		}
		base= base->next;
	}

	if(base==0) {
		return;
	}
	
	nr= pupmenu("Texture Space %t|Grab/Move%x1|Size%x2");
	if(nr<1) return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			base->object->dtx |= OB_TEXSPACE;
		}
		base= base->next;
	}
	

	if(nr==1) {
		initTransform(TFM_TRANSLATION, CTX_TEXTURE);
		Transform();
	}
	else if(nr==2) {
		initTransform(TFM_RESIZE, CTX_TEXTURE);
		Transform();
	}
	else if(nr==3) {
		initTransform(TFM_ROTATION, CTX_TEXTURE);
		Transform();
	}
}

/* ******************************************************************** */
/* Mirror function in Edit Mode */

void mirrormenu(void)
{
	short mode = 0;


	if (G.obedit==0) {
		mode=pupmenu("Mirror Axis %t|X Local%x4|Y Local%x5|Z Local%x6|");

		if (mode==-1) return; /* return */
		Mirror(mode); /* separating functionality from interface | call*/
	}
	else {
		mode=pupmenu("Mirror Axis %t|X Global%x1|Y Global%x2|Z Global%x3|%l|X Local%x4|Y Local%x5|Z Local%x6|%l|X View%x7|Y View%x8|Z View%x9|");

		if (mode==-1) return; /* return */
		Mirror(mode); /* separating functionality from interface | call*/
	}
}
