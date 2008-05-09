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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_particle_types.h"
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
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_customdata.h"
#include "BKE_blender.h"
#include "BKE_booleanops.h"
#include "BKE_cloth.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_ipo.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_multires.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_property.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"

#include "BIF_butspace.h"
#include "BIF_editconstraint.h"
#include "BIF_editdeform.h"
#include "BIF_editfont.h"
#include "BIF_editlattice.h"
#include "BIF_editmesh.h"
#include "BIF_editoops.h"
#include "BIF_editparticle.h"
#include "BIF_editview.h"
#include "BIF_editarmature.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_toets.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_filesel.h"	/* For activate_databrowse() */
#include "BSE_view.h"
#include "BSE_drawview.h"
#include "BSE_trans_types.h"
#include "BSE_editipo_types.h"

#include "BDR_vpaint.h"
#include "BDR_sculptmode.h"
#include "BDR_editface.h"
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


/* --------------------------------- */

void exit_paint_modes(void)
{
	if(G.f & G_VERTEXPAINT) set_vpaint();
	if(G.f & G_TEXTUREPAINT) set_texturepaint();
	if(G.f & G_WEIGHTPAINT) set_wpaint();
	if(G.f & G_SCULPTMODE) set_sculptmode();
	if(G.f & G_PARTICLEEDIT) PE_set_particle_edit();

	G.f &= ~(G_VERTEXPAINT+G_TEXTUREPAINT+G_WEIGHTPAINT+G_SCULPTMODE+G_PARTICLEEDIT);
}

void add_object_draw(int type)	/* for toolbox or menus, only non-editmode stuff */
{
	Object *ob;
	
	exit_paint_modes();
	setcursor_space(SPACE_VIEW3D, CURSOR_STD);

	if ELEM3(curarea->spacetype, SPACE_VIEW3D, SPACE_BUTS, SPACE_INFO) {
		if (G.obedit) exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); /* freedata, and undo */
		ob= add_object(type);
		set_active_base(BASACT);
		base_init_from_view3d(BASACT, G.vd);
		
		/* only undo pushes on objects without editmode... */
		if(type==OB_EMPTY) BIF_undo_push("Add Empty");
		else if(type==OB_LAMP) {
			BIF_undo_push("Add Lamp");
			reshadeall_displist();	/* only frees */
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

/* remove base from a specific scene */
/* note: now unlinks constraints as well */
void free_and_unlink_base_from_scene(Scene *scene, Base *base)
{
	BLI_remlink(&scene->base, base);
	free_libblock_us(&G.main->object, base->object);
	MEM_freeN(base);
}

/* remove base from the current scene */
void free_and_unlink_base(Base *base)
{
	if (base==BASACT)
		BASACT= NULL;
	free_and_unlink_base_from_scene(G.scene, base);
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
			if(ok==0) {
				/* Shift Del is global delete */
				if (G.qual & LR_SHIFTKEY) {
					if(!okee("Erase selected Object(s) Globally")) return;
					ok= 2;
				} else {
					if(!okee("Erase selected Object(s)")) return;
					ok= 1;
				}
			}
			
			exit_paint_modes();

			if(base->object->type==OB_LAMP) islamp= 1;
#ifdef WITH_VERSE
			if(base->object->vnode) b_verse_delete_object(base->object);
#endif
			if (ok==2) {
				Scene *scene; 
				Base *base_other;
				
				for (scene= G.main->scene.first; scene; scene= scene->id.next) {
					if (scene != G.scene && !(scene->id.lib)) {
						base_other= object_in_scene( base->object, scene );
						if (base_other) {
							if (base_other == scene->basact) scene->basact= NULL;	/* in case the object was active */
							free_and_unlink_base_from_scene( scene, base_other );
						}
					}
				}
			}
			
			/* remove from current scene only */
			free_and_unlink_base(base);
		}
		
		base= nbase;
	}
	countall();

	setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	
	if(islamp) reshadeall_displist();	/* only frees displist */

	redraw_test_buttons(OBACT);
	allqueue(REDRAWVIEW3D, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWDATASELECT, 0);
	allspace(OOPS_TEST, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	
	DAG_scene_sort(G.scene);
	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);

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
	MDeformVert *dvert;
	EditVert *eve;
	int i, totvert=0;
	
	cent[0]= cent[1]= cent[2]= 0.0;
	
	if(G.obedit->actdef) {
		
		/* find the vertices */
		for(eve= em->verts.first; eve; eve= eve->next) {
			dvert= CustomData_em_get(&em->vdata, eve->data, CD_MDEFORMVERT);

			if(dvert) {
				for(i=0; i<dvert->totweight; i++){
					if(dvert->dw[i].def_nr == (G.obedit->actdef-1)) {
						totvert++;
						VecAddf(cent, cent, eve->co);
					}
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
	
	/* count */
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
	
	/* count */
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

/* use this when the loc/size/rot of the parent has changed but the children should stay in the same place
 * apply-size-rot or object center for eg */
static void ignore_parent_tx( Object *ob ) {
	Object *ob_child;
	/* a change was made, adjust the children to compensate */
	for (ob_child=G.main->object.first; ob_child; ob_child=ob_child->id.next) {
		if (ob_child->parent == ob) {
			apply_obmat(ob_child);
			what_does_parent(ob_child);
			Mat4Invert(ob_child->parentinv, workob.obmat);
		}
	}
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

void add_hook_menu(void)
{
	int mode;
	
	if(G.obedit==NULL) return;
	
	if(modifiers_findByType(G.obedit, eModifierType_Hook))
		mode= pupmenu("Hooks %t|Add, To New Empty %x1|Add, To Selected Object %x2|Remove... %x3|Reassign... %x4|Select... %x5|Clear Offset...%x6");
	else
		mode= pupmenu("Hooks %t|Add, New Empty %x1|Add, To Selected Object %x2");

	if(mode<1) return;
		
	/* do operations */
	add_hook(mode);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	
	BIF_undo_push("Add hook");
}

void add_hook(int mode)
{
	ModifierData *md = NULL;
	HookModifierData *hmd = NULL;
	Object *ob=NULL;
	
	if(G.obedit==NULL) return;
	
	/* preconditions */
	if(mode==2) { /* selected object */
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
		
		/* make pupmenu with hooks */
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
			else if (hmd->indexar) MEM_freeN(hmd->indexar); /* reassign, hook was set */

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
	else if(mode==3) { /* remove */
		BLI_remlink(&G.obedit->modifiers, md);
		modifier_free(md);
	}
	else if(mode==5) { /* select */
		hook_select(hmd);
	}
	else if(mode==6) { /* clear offset */
		where_is_object(ob);	/* ob is hook->parent */
		
		Mat4Invert(ob->imat, ob->obmat);
		/* this call goes from right to left... */
		Mat4MulSerie(hmd->parentinv, ob->imat, G.obedit->obmat, NULL, 
					NULL, NULL, NULL, NULL, NULL);
	}

	DAG_scene_sort(G.scene);
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
	/* Quats arnt used yet */
	/*if(ob->transflag & OB_QUAT) {
		Mat3ToQuat(mat, ob->quat);
		QuatToMat3(ob->quat, tmat);
	}
	else {*/
		Mat3ToEul(mat, ob->rot);
		EulToMat3(ob->rot, tmat);
	/*}*/
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
	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
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
	int armature_clear= 0;
	char *str=NULL;
	
	if(G.obedit) return;
	if(G.scene->id.lib) return;
	
	if(mode=='r') str= "Clear rotation";
	else if(mode=='g') str= "Clear location";
	else if(mode=='s') str= "Clear scale";
	else if(mode=='o') str= "Clear origin";
	else return;
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			if ((ob->flag & OB_POSEMODE)) {
				/* only clear pose transforms if:
				 *	- with a mesh in weightpaint mode, it's related armature needs to be cleared
				 *	- with clearing transform of object being edited at the time
				 */
				if ((G.f & G_WEIGHTPAINT) || (ob==OBACT)) {
					clear_armature(ob, mode);
					armature_clear= 1;	/* silly system to prevent another dag update, so no action applied */
				}
			}
			else if((G.f & G_WEIGHTPAINT)==0) {
				/* only clear transforms of 'normal' (not armature) object if:
				 *	- not in weightpaint mode or editmode
				 *	- if that object's transform locks are not enabled (this is done on a per-channel basis)
				 */
				if (mode=='r') {
					/* eulers can only get cleared if they are not protected */
					if ((ob->protectflag & OB_LOCK_ROTX)==0)
						ob->rot[0]= ob->drot[0]= 0.0f;
					if ((ob->protectflag & OB_LOCK_ROTY)==0)
						ob->rot[1]= ob->drot[1]= 0.0f;
					if ((ob->protectflag & OB_LOCK_ROTZ)==0)
						ob->rot[2]= ob->drot[2]= 0.0f;
					
					/* quats here are not really used anymore anywhere, so it probably doesn't 
					 * matter to not clear them whether the euler-based rotation is used
					 */
					/*QuatOne(ob->quat);
					QuatOne(ob->dquat);*/
					
#ifdef WITH_VERSE
					if(ob->vnode) {
						struct VNode *vnode = (VNode*)ob->vnode;
						((VObjectData*)vnode->data)->flag |= ROT_SEND_READY;
						b_verse_send_transformation(ob);
					}
#endif

				}
				else if (mode=='g') {
					if ((ob->protectflag & OB_LOCK_LOCX)==0)
						ob->loc[0]= ob->dloc[0]= 0.0f;
					if ((ob->protectflag & OB_LOCK_LOCY)==0)
						ob->loc[1]= ob->dloc[1]= 0.0f;
					if ((ob->protectflag & OB_LOCK_LOCZ)==0)
						ob->loc[2]= ob->dloc[2]= 0.0f;
					
#ifdef WITH_VERSE
					if(ob->vnode) {
						struct VNode *vnode = (VNode*)ob->vnode;
						((VObjectData*)vnode->data)->flag |= POS_SEND_READY;
						b_verse_send_transformation(ob);
					}
#endif

				}
				else if (mode=='s') {
					if ((ob->protectflag & OB_LOCK_SCALEX)==0) {
						ob->dsize[0]= 0.0f;
						ob->size[0]= 1.0f;
					}
					if ((ob->protectflag & OB_LOCK_SCALEY)==0) {
						ob->dsize[1]= 0.0f;
						ob->size[1]= 1.0f;
					}
					if ((ob->protectflag & OB_LOCK_SCALEZ)==0) {
						ob->dsize[2]= 0.0f;
						ob->size[2]= 1.0f;
					}
#ifdef WITH_VERSE
					if(ob->vnode) {
						struct VNode *vnode = (VNode*)ob->vnode;
						((VObjectData*)vnode->data)->flag |= SCALE_SEND_READY;
						b_verse_send_transformation(ob);
					}
#endif

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
	if(armature_clear==0) /* in this case flush was done */
		DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
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
	int a, v1=0, v2=0, v3=0, v4=0, nr=1;
	
	/* we need 1 to 3 selected vertices */
	
	if(G.obedit->type==OB_MESH) {
		eve= em->verts.first;
		while(eve) {
			if(eve->f & 1) {
				if(v1==0) v1= nr;
				else if(v2==0) v2= nr;
				else if(v3==0) v3= nr;
				else if(v4==0) v4= nr;
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
					if(BEZSELECTED_HIDDENHANDLES(bezt)) {
						if(v1==0) v1= nr;
						else if(v2==0) v2= nr;
						else if(v3==0) v3= nr;
						else if(v4==0) v4= nr;
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
						else if(v4==0) v4= nr;
						else break;
					}
					nr++;
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(G.obedit->type==OB_LATTICE) {
		
		a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		bp= editLatt->def;
		while(a--) {
			if(bp->f1 & SELECT) {
				if(v1==0) v1= nr;
				else if(v2==0) v2= nr;
				else if(v3==0) v3= nr;
				else if(v4==0) v4= nr;
				else break;
			}
			nr++;
			bp++;
		}
	}
	
	if(v4 || !((v1 && v2==0 && v3==0) || (v1 && v2 && v3)) ) {
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
	/* BIF_undo_push(str); not, conflicts with editmode undo... */
}

static Object *group_objects_menu(Group *group)
{
	GroupObject *go;
	int len= 0;
	short a, nr;
	char *str;
		
	for(go= group->gobject.first; go; go= go->next) {
		if(go->ob)
			len++;
	}
	if(len==0) return NULL;
	
	str= MEM_callocN(40+32*len, "menu");
	
	strcpy(str, "Make Proxy for: %t");
	a= strlen(str);
	for(nr=1, go= group->gobject.first; go; go= go->next, nr++) {
		a+= sprintf(str+a, "|%s %%x%d", go->ob->id.name+2, nr);
	}
	
	a= pupmenu_col(str, 20);
	MEM_freeN(str);
	if(a>0) {
		go= BLI_findlink(&group->gobject, a-1);
		return go->ob;
	}
	return NULL;
}


/* adds empty object to become local replacement data of a library-linked object */
void make_proxy(void)
{
	Object *ob= OBACT;
	Object *gob= NULL;
	
	if(G.scene->id.lib) return;
	if(ob==NULL) return;
	
	
	if(ob->dup_group && ob->dup_group->id.lib) {
		gob= ob;
		/* gives menu with list of objects in group */
		ob= group_objects_menu(ob->dup_group);
	}
	else if(ob->id.lib) {
		if(okee("Make Proxy Object")==0)
		return;
	}
	else {
		error("Can only make proxy for a referenced object or group");
		return;
	}
	
	if(ob) {
		Object *newob;
		Base *newbase, *oldbase= BASACT;
		char name[32];
		
		newob= add_object(OB_EMPTY);
		if(gob)
			strcpy(name, gob->id.name+2);
		else
			strcpy(name, ob->id.name+2);
		strcat(name, "_proxy");
		rename_id(&newob->id, name);
		
		/* set layers OK */
		newbase= BASACT;	/* add_object sets active... */
		newbase->lay= oldbase->lay;
		newob->lay= newbase->lay;
		
		/* remove base, leave user count of object, it gets linked in object_make_proxy */
		if(gob==NULL) {
			BLI_remlink(&G.scene->base, oldbase);
			MEM_freeN(oldbase);
		}		
		object_make_proxy(newob, ob, gob);
		
		DAG_scene_sort(G.scene);
		DAG_object_flush_update(G.scene, newob, OB_RECALC);
		allqueue(REDRAWALL, 0);
		BIF_undo_push("Make Proxy Object");
	}
}

int test_parent_loop(Object *par, Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	
	if(par==0) return 0;
	if(ob == par) return 1;
	
	return test_parent_loop(par->parent, ob);

}

void make_parent(void)
{
	Base *base;
	Object *par;
	bPoseChannel *pchan= NULL;
	short qual, mode=0;

	if(G.scene->id.lib) return;
	if(G.obedit) {
		if ELEM4(G.obedit->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE) make_vertex_parent();
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
				makeDispListCurveTypes(par, 0);  /* force creation of path data */
			}
			else cu->flag |= CU_FOLLOW;
		}
		else if(mode==3) {
			mode= PARSKEL;
		}
		else if(mode==4) {
			bConstraint *con;
			bFollowPathConstraint *data;
				
			base= FIRSTBASE;
			while(base) {
				if TESTBASELIB(base) {
					if(base!=BASACT) {
						float cmat[4][4], vec[3];
						
						con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
						strcpy (con->name, "AutoPath");
						
						data = con->data;
						data->tar = BASACT->object;
						
						add_constraint_to_object(con, base->object);
						
						get_constraint_target_matrix(con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, G.scene->r.cfra - give_timeoffset(base->object));
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
			BIF_undo_push("Make Parent");
			return;
		}
	}
	else if(par->type == OB_ARMATURE){
		
		base= FIRSTBASE;
		while(base) {
			if TESTBASELIB(base) {
				if(base!=BASACT) {
					if(ELEM(base->object->type, OB_MESH, OB_LATTICE)) {
						if(par->flag & OB_POSEMODE)
							mode= pupmenu("Make Parent To%t|Bone %x1|Armature %x2|Object %x3");
						else
							mode= pupmenu("Make Parent To%t|Armature %x2|Object %x3");
						break;
					}
					else {
						if(par->flag & OB_POSEMODE)
							mode= pupmenu("Make Parent To %t|Bone %x1|Object %x3");
						else
							mode= pupmenu("Make Parent To %t|Object %x3");
						break;
					}
				}
			}
			base= base->next;
		}
	
		switch (mode){
		case 1:
			mode=PARBONE;
			pchan= get_active_posechannel(par);

			if(pchan==NULL) {
				error("No active Bone");
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
						if (pchan)
							strcpy (base->object->parsubstr, pchan->name);
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
						if(mode==PARSKEL && base->object->type==OB_MESH && par->type == OB_ARMATURE) {
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
				}
			}
		}
		base= base->next;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	
	DAG_scene_sort(G.scene);
	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
	
	BIF_undo_push("make Parent");
}


void enter_editmode(int wc)
{
	Base *base;
	Object *ob;
	Mesh *me;
	bArmature *arm;
	int ok= 0;
	
	if(G.scene->id.lib) return;
	base= BASACT;
	if(base==0) return;
	if((G.vd==NULL || (base->lay & G.vd->lay))==0) return;
	
	strcpy(G.editModeTitleExtra, "");

	ob= base->object;
	if(ob->data==0) return;
	
	if (object_data_is_libdata(ob)) {
		error_libdata();
		return;
	}
	
	if(wc) waitcursor(1);
	
	if(ob->type==OB_MESH) {
		me= get_mesh(ob);
		if( me==0 ) return;
		if(me->pv) mesh_pmv_off(ob, me);
		ok= 1;
		G.obedit= ob;
		make_editMesh();
		allqueue(REDRAWBUTSLOGIC, 0);
		/*if(G.f & G_FACESELECT) allqueue(REDRAWIMAGE, 0);*/
		if (EM_texFaceCheck())
			allqueue(REDRAWIMAGE, 0);
		
	}
	if (ob->type==OB_ARMATURE){
		arm= base->object->data;
		if (!arm) return;
		/*
		 * The function object_data_is_libdata make a problem here, the
		 * check for ob->proxy return 0 and let blender enter to edit mode
		 * this causa a crash when you try leave the edit mode.
		 * The problem is that i can't remove the ob->proxy check from
		 * object_data_is_libdata that prevent the bugfix #6614, so
		 * i add this little hack here.
		 */
		if(arm->id.lib) {
			error_libdata();
			return;
		}
		ok=1;
		G.obedit=ob;
		make_editArmature();
		/* to ensure all goes in restposition and without striding */
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC);

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
	
	if(wc) waitcursor(0);
	
	scrarea_queue_headredraw(curarea);
}

void exit_editmode(int flag)	/* freedata==0 at render, 1= freedata, 2= do undo buffer too */
{
	Object *ob;
	int freedata = flag & EM_FREEDATA;
	
	if(G.obedit==NULL) return;

	if(flag & EM_WAITCURSOR) waitcursor(1);
	if(G.obedit->type==OB_MESH) {

		/* temporal */
		countall();
		
		if(EM_texFaceCheck())
			allqueue(REDRAWIMAGE, 0);
		
		if(retopo_mesh_paint_check())
			retopo_end_okee();

		if(G.totvert>MESH_MAX_VERTS) {
			error("Too many vertices");
			return;
		}
		load_editMesh();

		if(freedata) free_editMesh(G.editMesh);
		
		if(G.f & G_WEIGHTPAINT)
			mesh_octree_table(G.obedit, NULL, 'e');
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

	if(ob->type==OB_MESH && get_mesh(ob)->mr)
		multires_edge_level_update(ob, get_mesh(ob));
	
	/* also flush ob recalc, doesn't take much overhead, but used for particles */
	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB|OB_RECALC_DATA);

	if(freedata) {
		setcursor_space(SPACE_VIEW3D, CURSOR_STD);
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 1);
	allqueue(REDRAWBUTSALL, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWOOPS, 0);

	scrarea_queue_headredraw(curarea);
	
	if(G.obedit==NULL && (flag & EM_FREEUNDO)) 
		BIF_undo_push("Editmode");
	
	if(flag & EM_WAITCURSOR) waitcursor(0);
}

void check_editmode(int type)
{
	
	if (G.obedit==0 || G.obedit->type==type) return;

	exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); /* freedata, and undo */
}

/* 0 == do center, 1 == center new, 2 == center cursor */

void docenter(int centermode)
{
	EditMesh *em = G.editMesh;
	Base *base;
	Object *ob;
	Mesh *me, *tme;
	Curve *cu;
/*	BezTriple *bezt;
	BPoint *bp; */
	Nurb *nu, *nu1;
	EditVert *eve;
	float cent[3], centn[3], min[3], max[3], omat[3][3];
	int a, total= 0;
	
	/* keep track of what is changed */
	int tot_change=0, tot_lib_error=0, tot_key_error=0, tot_multiuser_arm_error=0;
	MVert *mvert;

	if(G.scene->id.lib || G.vd==NULL) return;
	
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
			tot_change++;
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		}
	}
	
	/* reset flags */
	for (base=FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base)
			base->object->flag &= ~OB_DONE;
	}
	
	for (me= G.main->mesh.first; me; me= me->id.next) {
		me->flag &= ~ME_ISDONE;
	}
	
	base= FIRSTBASE;
	while(base) {
		
		if TESTBASELIB(base) {
			if((base->object->flag & OB_DONE)==0) {
				base->object->flag |= OB_DONE;
				
				if(base->object->id.lib) {
					tot_lib_error++;
				}
				else if(G.obedit==0 && (me=get_mesh(base->object)) ) {
					
					if(me->key) {
						/*error("Can't change the center of a mesh with vertex keys");
						return;*/
						tot_key_error++;
					} else if (me->id.lib) {
						tot_lib_error++;
					} else {
						if(centermode==2) {
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
						
						if(centermode) {
							Mat3CpyMat4(omat, base->object->obmat);
							
							VECCOPY(centn, cent);
							Mat3MulVecfl(omat, centn);
							base->object->loc[0]+= centn[0];
							base->object->loc[1]+= centn[1];
							base->object->loc[2]+= centn[2];
							
							where_is_object(base->object);
							ignore_parent_tx(base->object);
							
							/* other users? */
							ob= G.main->object.first;
							while(ob) {
								if((ob->flag & OB_DONE)==0) {
									tme= get_mesh(ob);
									
									if(tme==me) {
										
										ob->flag |= OB_DONE;
										ob->recalc= OB_RECALC_OB|OB_RECALC_DATA;

										Mat3CpyMat4(omat, ob->obmat);
										VECCOPY(centn, cent);
										Mat3MulVecfl(omat, centn);
										ob->loc[0]+= centn[0];
										ob->loc[1]+= centn[1];
										ob->loc[2]+= centn[2];
										
										where_is_object(ob);
										ignore_parent_tx(ob);
										
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
						tot_change++;
					}
				}
				else if ELEM(base->object->type, OB_CURVE, OB_SURF) {
					
					/* totally weak code here... (ton) */
					if(G.obedit==base->object) {
						extern ListBase editNurb;
						nu1= editNurb.first;
						cu= G.obedit->data;
					}
					else {
						cu= base->object->data;
						nu1= cu->nurb.first;
					}
					
					if (cu->id.lib) {
						tot_lib_error++;
					} else {
						if(centermode==2) {
							VECCOPY(cent, give_cursor());
							Mat4Invert(base->object->imat, base->object->obmat);
							Mat4MulVecfl(base->object->imat, cent);

							/* don't allow Z change if curve is 2D */
							if( !( cu->flag & CU_3D ) )
								cent[2] = 0.0;
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
				
						if(centermode && G.obedit==0) {
							Mat3CpyMat4(omat, base->object->obmat);
							
							Mat3MulVecfl(omat, cent);
							base->object->loc[0]+= cent[0];
							base->object->loc[1]+= cent[1];
							base->object->loc[2]+= cent[2];
							
							where_is_object(base->object);
							ignore_parent_tx(base->object);
						}
						
						tot_change++;
						if(G.obedit) {
							if (centermode==0) {
								DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
							}
							break;
						}
					}
				}
				else if(base->object->type==OB_FONT) {
					/* get from bb */
					
					cu= base->object->data;
					
					if(cu->bb==0) {
						/* do nothing*/
					} else if (cu->id.lib) {
						tot_lib_error++;
					} else {
						cu->xof= -0.5f*( cu->bb->vec[4][0] - cu->bb->vec[0][0]);
						cu->yof= -0.5f -0.5f*( cu->bb->vec[0][1] - cu->bb->vec[2][1]);	/* extra 0.5 is the height of above line */
						
						/* not really ok, do this better once! */
						cu->xof /= cu->fsize;
						cu->yof /= cu->fsize;

						allqueue(REDRAWBUTSEDIT, 0);
						tot_change++;
					}
				}
				else if(base->object->type==OB_ARMATURE) {
					bArmature *arm = base->object->data;
					
					if (arm->id.lib) {
						tot_lib_error++;
					} else if(arm->id.us>1) {
						/*error("Can't apply to a multi user armature");
						return;*/
						tot_multiuser_arm_error++;
					} else {
						/* Function to recenter armatures in editarmature.c 
						 * Bone + object locations are handled there.
						 */
						docenter_armature(base->object, centermode);
						tot_change++;
						
						where_is_object(base->object);
						ignore_parent_tx(base->object);
						
						if(G.obedit) 
							break;
					}
				}
				base->object->recalc= OB_RECALC_OB|OB_RECALC_DATA;
			}
		}
		base= base->next;
	}
	if (tot_change) {
		DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Do Center");	
	}
	
	/* Warn if any errors occured */
	if (tot_lib_error+tot_key_error+tot_multiuser_arm_error) {
		char err[512];
		sprintf(err, "Warning %i Object(s) Not Centered, %i Changed:", tot_lib_error+tot_key_error+tot_multiuser_arm_error, tot_change);
		
		if (tot_lib_error)
			sprintf(err+strlen(err), "|%i linked library objects", tot_lib_error);
		if (tot_key_error)
			sprintf(err+strlen(err), "|%i mesh key object(s)", tot_key_error);
		if (tot_multiuser_arm_error)
			sprintf(err+strlen(err), "|%i multiuser armature object(s)", tot_multiuser_arm_error);
		
		error(err);
	}
}

void docenter_new(void)
{
	if(G.scene->id.lib) return;

	if(G.obedit) {
		error("Unable to center new in Edit Mode");
	}
	else {
		docenter(1);
	}
}

void docenter_cursor(void)
{
	if(G.scene->id.lib) return;

	if(G.obedit) {
		error("Unable to center cursor in Edit Mode");
	}
	else {
		docenter(2);
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
	
	if(lay==0) return;
	
	if(G.vd->localview) {
		/* now we can move out of localview. */
		if (!okee("Move from localview")) return;
		base= FIRSTBASE;
		while(base) {
			if TESTBASE(base) {
				lay= base->lay & ~G.vd->lay;
				base->lay= lay;
				base->object->lay= lay;
				base->object->flag &= ~SELECT;
				base->flag &= ~SELECT;
				if(base->object->type==OB_LAMP) islamp= 1;
			}
			base= base->next;
		}
	} else {
		if( movetolayer_buts(&lay, NULL)==0 ) return;
		
		/* normal non localview operation */
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
	}
	if(islamp) reshadeall_displist();	/* only frees */
	
	/* warning, active object may be hidden now */
	
	countall();
	DAG_scene_sort(G.scene);
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWINFO, 0);
	
	BIF_undo_push("Move to layer");
}

/* THIS IS BAD CODE! do not bring back before it has a real implementation (ton) */
void split_font()
{
	Object *ob = OBACT;
	Base *oldbase = BASACT;
	Curve *cu= ob->data;
	char *p= cu->str;
	int slen= strlen(p);
	int i;

	for (i = 0; i<=slen; p++, i++) {
		adduplicate(1, U.dupflag);
		cu= OBACT->data;
		cu->sepchar = i+1;
		text_to_curve(OBACT, 0);	/* pass 1: only one letter, adapt position */
		text_to_curve(OBACT, 0);	/* pass 2: remake */
		freedisplist(&OBACT->disp);
		makeDispListCurveTypes(OBACT, 0);
		
		OBACT->flag &= ~SELECT;
		BASACT->flag &= ~SELECT;
		oldbase->flag |= SELECT;
		oldbase->object->flag |= SELECT;
		set_active_base(oldbase);		
	}
}

static void helpline(short *mval, int *center2d)
{
	
	/* helpline, copied from transform.c actually */
	persp(PERSP_WIN);
	glDrawBuffer(GL_FRONT);
	
	BIF_ThemeColor(TH_WIRE);
	
	setlinestyle(3);
	glBegin(GL_LINE_STRIP); 
	glVertex2sv(mval); 
	glVertex2iv((GLint *)center2d); 
	glEnd();
	setlinestyle(0);
	
	persp(PERSP_VIEW);
	bglFlush(); // flush display for frontbuffer
	glDrawBuffer(GL_BACK);
	
	
}

/* context: ob = lamp */
/* code should be replaced with proper (custom) transform handles for lamp properties */
static void spot_interactive(Object *ob, int mode)
{
	Lamp *la= ob->data;
	float transfac, dx, dy, ratio, origval;
	int keep_running= 1, center2d[2];
	short mval[2], mvalo[2];
	
	getmouseco_areawin(mval);
	getmouseco_areawin(mvalo);
	
	project_int(ob->obmat[3], center2d);
	if( center2d[0] > 100000 ) {		/* behind camera */
		center2d[0]= curarea->winx/2;
		center2d[1]= curarea->winy/2;
	}

	helpline(mval, center2d);
	
	/* ratio is like scaling */
	dx = (float)(center2d[0] - mval[0]);
	dy = (float)(center2d[1] - mval[1]);
	transfac = (float)sqrt( dx*dx + dy*dy);
	if(transfac==0.0f) transfac= 1.0f;
	
	if(mode==1)	
		origval= la->spotsize;
	else if(mode==2)	
		origval= la->dist;
	else if(mode==3)	
		origval= la->clipsta;
	else	
		origval= la->clipend;
	
	while (keep_running>0) {
		
		getmouseco_areawin(mval);
		
		/* essential for idling subloop */
		if(mval[0]==mvalo[0] && mval[1]==mvalo[1]) {
			PIL_sleep_ms(2);
		}
		else {
			char str[32];
			
			dx = (float)(center2d[0] - mval[0]);
			dy = (float)(center2d[1] - mval[1]);
			ratio = (float)(sqrt( dx*dx + dy*dy))/transfac;
			
			/* do the trick */
			
			if(mode==1) {	/* spot */
				la->spotsize = ratio*origval;
				CLAMP(la->spotsize, 1.0f, 180.0f);
				sprintf(str, "Spot size %.2f\n", la->spotsize);
			}
			else if(mode==2) {	/* dist */
				la->dist = ratio*origval;
				CLAMP(la->dist, 0.01f, 5000.0f);
				sprintf(str, "Distance %.2f\n", la->dist);
			}
			else if(mode==3) {	/* sta */
				la->clipsta = ratio*origval;
				CLAMP(la->clipsta, 0.001f, 5000.0f);
				sprintf(str, "Distance %.2f\n", la->clipsta);
			}
			else if(mode==4) {	/* end */
				la->clipend = ratio*origval;
				CLAMP(la->clipend, 0.1f, 5000.0f);
				sprintf(str, "Clip End %.2f\n", la->clipend);
			}

			/* cleanup */
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			
			/* handle shaded mode */
			shade_buttons_change_3d();

			/* DRAW */	
			headerprint(str);
			force_draw_plus(SPACE_BUTS, 0);

			helpline(mval, center2d);
		}
		
		while( qtest() ) {
			short val;
			unsigned short event= extern_qread(&val);
			
			switch (event){
				case ESCKEY:
				case RIGHTMOUSE:
					keep_running= 0;
					break;
				case LEFTMOUSE:
				case SPACEKEY:
				case PADENTER:
				case RETKEY:
					if(val)
						keep_running= -1;
					break;
			}
		}
	}

	if(keep_running==0) {
		if(mode==1)	
			la->spotsize= origval;
		else if(mode==2)	
			la->dist= origval;
		else if(mode==3)	
			la->clipsta= origval;
		else	
			la->clipend= origval;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSSHADING, 0);
	BIF_preview_changed(ID_LA);
}


void special_editmenu(void)
{
	static short numcuts= 2;
	Object *ob= OBACT;
	float fac;
	int nr,ret;
	short randfac;
	
	if(ob==NULL) return;
	
	if(G.obedit==NULL) {
		
		if(ob->flag & OB_POSEMODE) {
			pose_special_editmenu();
		}
		else if(FACESEL_PAINT_TEST) {
			Mesh *me= get_mesh(ob);
			MTFace *tface;
			MFace *mface;
			int a;
			
			if(me==0 || me->mtface==0) return;
			
			nr= pupmenu("Specials%t|Set     Tex%x1|         Shared%x2|         Light%x3|         Invisible%x4|         Collision%x5|         TwoSide%x6|Clr     Tex%x7|         Shared%x8|         Light%x9|         Invisible%x10|         Collision%x11|         TwoSide%x12");
			
			tface= me->mtface;
			mface= me->mface;
			for(a=me->totface; a>0; a--, tface++, mface++) {
				if(mface->flag & ME_FACE_SEL) {
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
						tface->mode |= TF_TWOSIDE; break;
					case 7:
						tface->mode &= ~TF_TEX;
						tface->tpage= 0;
						break;
					case 8:
						tface->mode &= ~TF_SHAREDCOL; break;
					case 9:
						tface->mode &= ~TF_LIGHT; break;
					case 10:
						tface->mode &= ~TF_INVISIBLE; break;
					case 11:
						tface->mode &= ~TF_DYNAMIC; break;
					case 12:
						tface->mode &= ~TF_TWOSIDE; break;
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
			
			if(me==0 || (me->mcol==NULL && me->mtface==NULL) ) return;
			
			nr= pupmenu("Specials%t|Shared VertexCol%x1");
			if(nr==1) {
				
				do_shared_vertexcol(me);
				
				BIF_undo_push("Shared VertexCol");

				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
		}
		else if(G.f & G_WEIGHTPAINT) {
			Object *par= modifiers_isDeformedByArmature(ob);

			if(par && (par->flag & OB_POSEMODE)) {
				nr= pupmenu("Specials%t|Apply Bone Envelopes to Vertex Groups %x1|Apply Bone Heat Weights to Vertex Groups %x2");

				if(nr==1 || nr==2)
					pose_adds_vgroups(ob, (nr == 2));
			}
		}
		else if(G.f & G_PARTICLEEDIT) {
			ParticleSystem *psys = PE_get_current(ob);
			ParticleEditSettings *pset = PE_settings();

			if(!psys)
				return;

			if(G.scene->selectmode & SCE_SELECT_POINT)
				nr= pupmenu("Specials%t|Rekey%x1|Subdivide%x2|Select First%x3|Select Last%x4|Remove Doubles%x5");
			else
				nr= pupmenu("Specials%t|Rekey%x1|Remove Doubles%x5");
			
			switch(nr) {
			case 1:
				if(button(&pset->totrekey, 2, 100, "Number of Keys:")==0) return;
				waitcursor(1);
				PE_rekey();
				break;
			case 2:
				PE_subdivide();
				break;
			case 3:
				PE_select_root();
				break;
			case 4:
				PE_select_tip();
				break;
			case 5:
				PE_remove_doubles();
				break;
			}
			
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			
			if(nr>0) waitcursor(0);
		}
		else {
			Base *base, *base_select= NULL;
			
			/* Get the active object mesh. */
			Mesh *me= get_mesh(ob);

			/* Booleans, if the active object is a mesh... */
			if (me && ob->id.lib==NULL) {
				
				/* Bring up a little menu with the boolean operation choices on. */
				nr= pupmenu("Boolean Tools%t|Intersect%x1|Union%x2|Difference%x3|Add Intersect Modifier%x4|Add Union Modifier%x5|Add Difference Modifier%x6");

				if (nr > 0) {
					/* user has made a choice of a menu element.
					   All of the boolean functions require 2 mesh objects 
					   we search through the object list to find the other 
					   selected item and make sure it is distinct and a mesh. */

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
									error("An internal error occurred");
								} else if(ret==-1) {
									error("Selected meshes must have faces to perform boolean operations");
								} else if (ret==-2) {
									error("Both meshes must be a closed mesh");
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
			else if (ob->type == OB_LAMP) {
				Lamp *la= ob->data;
				if(la->type==LA_SPOT) {
					short nr= pupmenu("Lamp Tools%t|Spot Size%x1|Distance%x2|Clip Start%x3|Clip End%x4");
					if(nr>0)
						spot_interactive(ob, nr);
				}
			}
			else if (ob->type == OB_FONT) {
				/* removed until this gets a decent implementation (ton) */
/*				nr= pupmenu("Split %t|Characters%x1");
				if (nr > 0) {
					switch(nr) {
						case 1: split_font();
					}
				}
*/
			}			
		}
	}
	else if(G.obedit->type==OB_MESH) {
		/* This is all that is needed, since all other functionality is in Ctrl+ V/E/F but some users didnt like, so for now have the old/big menu */
		/*
		nr= pupmenu("Subdivide Mesh%t|Subdivide%x1|Subdivide Multi%x2|Subdivide Multi Fractal%x3|Subdivide Smooth%x4");
		switch(nr) {
		case 1:
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag, 1, 0);
			
			BIF_undo_push("ESubdivide Single");            
			break;
		case 2:
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag, numcuts, 0);
			BIF_undo_push("ESubdivide");
			break;
		case 3:
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			randfac= 10;
			if(button(&randfac, 1, 100, "Rand fac:")==0) return;
			waitcursor(1);			
			fac= -( (float)randfac )/100;
			esubdivideflag(1, fac, G.scene->toolsettings->editbutflag, numcuts, 0);
			BIF_undo_push("Subdivide Fractal");
			break;
			
		case 4:
			fac= 1.0f;
			if(fbutton(&fac, 0.0f, 5.0f, 10, 10, "Smooth:")==0) return;
				fac= 0.292f*fac;
			
			waitcursor(1);
			esubdivideflag(1, fac, G.scene->toolsettings->editbutflag | B_SMOOTH, 1, 0);
			BIF_undo_push("Subdivide Smooth");
			break;		
		}
		*/
		
		nr= pupmenu("Specials%t|Subdivide%x1|Subdivide Multi%x2|Subdivide Multi Fractal%x3|Subdivide Smooth%x12|Merge%x4|Remove Doubles%x5|Hide%x6|Reveal%x7|Select Swap%x8|Flip Normals %x9|Smooth %x10|Bevel %x11|Set Smooth %x14|Set Solid %x15|Blend From Shape%x16|Propagate To All Shapes%x17|Select Vertex Path%x18");
		
		switch(nr) {
		case 1:
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag, 1, 0);
			
			BIF_undo_push("ESubdivide Single");            
			break;
		case 2:
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
			esubdivideflag(1, 0.0, G.scene->toolsettings->editbutflag, numcuts, 0);
			BIF_undo_push("ESubdivide");
			break;
		case 3:
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			randfac= 10;
			if(button(&randfac, 1, 100, "Rand fac:")==0) return;
			waitcursor(1);			
			fac= -( (float)randfac )/100;
			esubdivideflag(1, fac, G.scene->toolsettings->editbutflag, numcuts, 0);
			BIF_undo_push("Subdivide Fractal");
			break;
			
		case 12:	/* smooth */
			/* if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return; */
			fac= 1.0f;
			if(fbutton(&fac, 0.0f, 5.0f, 10, 10, "Smooth:")==0) return;
				fac= 0.292f*fac;
			
			waitcursor(1);
			esubdivideflag(1, fac, G.scene->toolsettings->editbutflag | B_SMOOTH, 1, 0);
			BIF_undo_push("Subdivide Smooth");
			break;		

		case 4:
			mergemenu();
			break;
		case 5:
			notice("Removed %d Vertices", removedoublesflag(1, 0, G.scene->toolsettings->doublimit));
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
		case 14:
			mesh_set_smooth_faces(1);
			break;
		case 15: 
			mesh_set_smooth_faces(0);
			break;
		case 16: 
			shape_copy_select_from();
			break;
		case 17: 
			shape_propagate();
			break;
		case 18:
			pathselect();
			BIF_undo_push("Select Vertex Path");
			break;
		}
		
		
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		
		if(nr>0) waitcursor(0);
		
	}
	else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {

		nr= pupmenu("Specials%t|Subdivide%x1|Switch Direction%x2|Set Goal Weight %x3|Set Radius %x4|Smooth Radius %x5");
		
		switch(nr) {
		case 1:
			subdivideNurb();
			break;
		case 2:
			switchdirectionNurb2();
			break;
		case 3:
			setweightNurb();
			break;
		case 4:
			setradiusNurb();
			break;
		case 5:
			smoothradiusNurb();
			break;
		}
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	}
	else if(G.obedit->type==OB_ARMATURE) {
		nr= pupmenu("Specials%t|Subdivide %x1|Subdivide Multi%x2|Flip Left-Right Names%x3|%l|AutoName Left-Right%x4|AutoName Front-Back%x5|AutoName Top-Bottom%x6");
		if(nr==1)
			subdivide_armature(1);
		if(nr==2) {
			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
			subdivide_armature(numcuts);
		}
		else if(nr==3)
			armature_flip_names();
		else if(ELEM3(nr, 4, 5, 6)) {
			armature_autoside_names(nr-4);
		}
	}
	else if(G.obedit->type==OB_LATTICE) {
		static float weight= 1.0f;
		if(fbutton(&weight, 0.0f, 1.0f, 10, 10, "Set Weight")) {
			int a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			BPoint *bp= editLatt->def;
			
			while(a--) {
				if(bp->f1 & SELECT)
					bp->weight= weight;
				bp++;
			}	
		}
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	
}

static void curvetomesh(Object *ob) 
{
	Curve *cu;
	DispList *dl;
	
	ob->flag |= OB_DONE;
	cu= ob->data;
	
	dl= cu->disp.first;
	if(dl==0) makeDispListCurveTypes(ob, 0);		/* force creation */

	nurbs_to_mesh(ob); /* also does users */
	if (ob->type != OB_MESH) {
		error("can't convert curve to mesh");
	} else {
		object_free_modifiers(ob);
	}
}

void convertmenu(void)
{
	Base *base, *basen=NULL, *basact, *basedel=NULL;
	Object *obact, *ob, *ob1;
	Curve *cu;
	Nurb *nu;
	MetaBall *mb;
	Mesh *me;
	int ok=0, nr = 0, a;
	
	if(G.scene->id.lib) return;

	obact= OBACT;
	if(obact==0) return;
	if(!obact->flag & SELECT) return;
	if(G.obedit) return;
	
	basact= BASACT;	/* will be restored */
		
	if(obact->type==OB_FONT) {
		nr= pupmenu("Convert Font to%t|Curve%x1|Curve (Single filling group)%x2|Mesh%x3");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_MBALL) {
		nr= pupmenu("Convert Metaball to%t|Mesh (keep original)%x1|Mesh (Delete Original)%x2");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_CURVE) {
		nr= pupmenu("Convert Curve to%t|Mesh");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_SURF) {
		nr= pupmenu("Convert Nurbs Surface to%t|Mesh");
		if(nr>0) ok= 1;
	}
	else if(obact->type==OB_MESH) {
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
			else if(ob->type==OB_MESH && ob->modifiers.first) { /* converting a mesh with no modifiers causes a segfault */
				DerivedMesh *dm;
				
				basedel = base;

				ob->flag |= OB_DONE;

				ob1= copy_object(ob);
				ob1->recalc |= OB_RECALC;

				basen= MEM_mallocN(sizeof(Base), "duplibase");
				*basen= *base;
				BLI_addhead(&G.scene->base, basen);	/* addhead: otherwise eternal loop */
				basen->object= ob1;
				basen->flag |= SELECT;
				base->flag &= ~SELECT;
				ob->flag &= ~SELECT;

				/* decrement original mesh's usage count  */
				me= ob1->data;
				me->id.us--;

				/* make a new copy of the mesh */
				ob1->data= copy_mesh(me);
				G.totmesh++;

				/* make new mesh data from the original copy */
				dm= mesh_get_derived_final(ob1, CD_MASK_MESH);
				/* dm= mesh_create_derived_no_deform(ob1, NULL);	this was called original (instead of get_derived). man o man why! (ton) */
				
				DM_to_mesh(dm, ob1->data);

				dm->release(dm);
				object_free_modifiers(ob1);	/* after derivedmesh calls! */
				
				/* If the original object is active then make this object active */
				if (ob == obact) {
					set_active_base( basen );
					basact = basen;
				}
			}
			else if(ob->type==OB_FONT) {
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
				if (nr==2 || nr==3) {
					nu= cu->nurb.first;
					while(nu) {
						nu->charidx= 0;
						nu= nu->next;
					}					
				}
				if (nr==3) {
					curvetomesh(ob);
				}
			}
			else if ELEM(ob->type, OB_CURVE, OB_SURF) {
				if(nr==1) {
					curvetomesh(ob);
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
						basen->flag |= SELECT;
						basedel->flag &= ~SELECT;
						ob->flag &= ~SELECT;
						
						mb= ob1->data;
						mb->id.us--;
						
						ob1->data= add_mesh("Mesh");
						G.totmesh++;
						ob1->type= OB_MESH;
						
						me= ob1->data;
						me->totcol= mb->totcol;
						if(ob1->totcol) {
							me->mat= MEM_dupallocN(mb->mat);
							for(a=0; a<ob1->totcol; a++) id_us_plus((ID *)me->mat[a]);
						}
						
						mball_to_mesh(&ob->disp, ob1->data);
						
						/* So we can see the wireframe */
						BASACT= basen;
						
						/* If the original object is active then make this object active */
						if (ob == obact) {
							set_active_base( basen );
							basact = basen;
						}
						
					}
				}
			}
		}
		base= base->next;
		if(basedel != NULL && nr == 2) {
			if(basedel==basact)
				basact= NULL;
			free_and_unlink_base(basedel);	
		}
		basedel = NULL;				
	}
	
	/* texspace and normals */
	if(!basen) BASACT= base;

	enter_editmode(EM_WAITCURSOR);
	exit_editmode(EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */
	BASACT= basact;

	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allspace(OOPS_TEST, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Convert Object");

	DAG_scene_sort(G.scene);
}

/* Change subdivision or particle properties of mesh object ob, if level==-1
 * then toggle subsurf, else set to level set allows to toggle multiple
 * selections */

static void object_has_subdivision_particles(Object *ob, int *havesubdiv, int *havepart, int depth)
{
	if(ob->type==OB_MESH) {
		if(modifiers_findByType(ob, eModifierType_Subsurf))
			*havesubdiv= 1;
		if(modifiers_findByType(ob, eModifierType_ParticleSystem))
			*havepart= 1;
	}

	if(ob->dup_group && depth <= 4) {
		GroupObject *go;

		for(go= ob->dup_group->gobject.first; go; go= go->next)
			object_has_subdivision_particles(go->ob, havesubdiv, havepart, depth+1);
	}
}

static void object_flip_subdivison_particles(Object *ob, int *set, int level, int mode, int particles, int depth)
{
	ModifierData *md;

	if(ob->type==OB_MESH) {
		if(particles) {
			for(md=ob->modifiers.first; md; md=md->next) {
				if(md->type == eModifierType_ParticleSystem) {
					ParticleSystemModifierData *psmd = (ParticleSystemModifierData*)md;

					if(*set == -1)
						*set= psmd->modifier.mode&(mode);

					if (*set)
						psmd->modifier.mode &= ~(mode);
					else
						psmd->modifier.mode |= (mode);
				}
			}
		}
		else {
			md = modifiers_findByType(ob, eModifierType_Subsurf);

			if (md) {
				SubsurfModifierData *smd = (SubsurfModifierData*) md;

				if (level == -1) {
					if(*set == -1) 
						*set= smd->modifier.mode&(mode);

					if (*set)
						smd->modifier.mode &= ~(mode);
					else
						smd->modifier.mode |= (mode);
				} else {
					smd->levels = level;
				}
			} 
			else if(depth == 0 && *set != 0) {
				SubsurfModifierData *smd = (SubsurfModifierData*) modifier_new(eModifierType_Subsurf);

				BLI_addtail(&ob->modifiers, smd);

				if (level!=-1) {
					smd->levels = level;
				}
				
				if(*set == -1)
					*set= 1;
			}
		}

		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}

	if(ob->dup_group && depth<=4) {
		GroupObject *go;

		for(go= ob->dup_group->gobject.first; go; go= go->next)
			object_flip_subdivison_particles(go->ob, set, level, mode, particles, depth+1);
	}
}

/* Change subdivision properties of mesh object ob, if
* level==-1 then toggle subsurf, else set to level.
*/

void flip_subdivison(int level)
{
	Base *base;
	int set= -1;
	int mode, pupmode, particles= 0, havesubdiv= 0, havepart= 0;
	
	if(G.qual & LR_ALTKEY)
		mode= eModifierMode_Realtime;
	else
		mode= eModifierMode_Render|eModifierMode_Realtime;
	
	if(level == -1) {
		if (G.obedit) {
			object_has_subdivision_particles(G.obedit, &havesubdiv, &havepart, 0);			
		} else {
			for(base= G.scene->base.first; base; base= base->next) {
				if(((level==-1) && (TESTBASE(base))) || (TESTBASELIB(base))) {
					object_has_subdivision_particles(base->object, &havesubdiv, &havepart, 0);
				}
			}
		}
	}
	else
		havesubdiv= 1;
	
	if(havesubdiv && havepart) {
		pupmode= pupmenu("Switch%t|Subsurf %x1|Particle Systems %x2");
		if(pupmode <= 0)
			return;
		else if(pupmode == 2)
			particles= 1;
	}
	else if(havepart)
		particles= 1;

	if (G.obedit) {	
		object_flip_subdivison_particles(G.obedit, &set, level, mode, particles, 0);
	} else {
		for(base= G.scene->base.first; base; base= base->next) {
			if(((level==-1) && (TESTBASE(base))) || (TESTBASELIB(base))) {
				object_flip_subdivison_particles(base->object, &set, level, mode, particles, 0);
			}
		}
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
	
	if(particles)
		BIF_undo_push("Switch particles on/off");
	else
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
	Base *base;
	int i, event;
	char str[512];
	char *errorstr= NULL;

	strcpy(str, "Copy Modifiers %t");

	sprintf(str+strlen(str), "|All%%x%d|%%l", NUM_MODIFIER_TYPES);

	for (i=eModifierType_None+1; i<NUM_MODIFIER_TYPES; i++) {
		ModifierTypeInfo *mti = modifierType_getInfo(i);

		if(ELEM3(i, eModifierType_Hook, eModifierType_Softbody, eModifierType_ParticleInstance)) continue;

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

				if (base->object->type==ob->type) {
					/* copy all */
					if (event==NUM_MODIFIER_TYPES) {
						object_free_modifiers(base->object);

						for (md=ob->modifiers.first; md; md=md->next) {
							if (md->type!=eModifierType_Hook) {
								ModifierData *nmd = modifier_new(md->type);
								modifier_copyData(md, nmd);
								BLI_addtail(&base->object->modifiers, nmd);
							}
						}

						copy_object_particlesystems(base->object, ob);
						copy_object_softbody(base->object, ob);
					} else {
						/* copy specific types */
						ModifierData *md, *mdn;
						
						/* remove all with type 'event' */
						for (md=base->object->modifiers.first; md; md=mdn) {
							mdn= md->next;
							if(md->type==event) {
								BLI_remlink(&base->object->modifiers, md);
								modifier_free(md);
							}
						}
						
						/* copy all with type 'event' */
						for (md=ob->modifiers.first; md; md=md->next) {
							if (md->type==event) {
								
								mdn = modifier_new(event);
								BLI_addtail(&base->object->modifiers, mdn);

								modifier_copyData(md, mdn);
							}
						}

						if(event == eModifierType_ParticleSystem) {
							object_free_particlesystems(base->object);
							copy_object_particlesystems(base->object, ob);
						}
						else if(event == eModifierType_Softbody) {
							object_free_softbody(base->object);
							copy_object_softbody(base->object, ob);
						}
					}
				}
				else
					errorstr= "Did not copy modifiers to other Object types";
			}
		}
	}
	
	if(errorstr) notice(errorstr);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	DAG_scene_sort(G.scene);
	
	BIF_undo_push("Copy modifiers");
}

/* both pointers should exist */
static void copy_texture_space(Object *to, Object *ob)
{
	float *poin1= NULL, *poin2= NULL;
	int texflag= 0;
	
	if(ob->type==OB_MESH) {
		texflag= ((Mesh *)ob->data)->texflag;
		poin2= ((Mesh *)ob->data)->loc;
	}
	else if (ELEM3(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		texflag= ((Curve *)ob->data)->texflag;
		poin2= ((Curve *)ob->data)->loc;
	}
	else if(ob->type==OB_MBALL) {
		texflag= ((MetaBall *)ob->data)->texflag;
		poin2= ((MetaBall *)ob->data)->loc;
	}
	else
		return;
		
	if(to->type==OB_MESH) {
		((Mesh *)to->data)->texflag= texflag;
		poin1= ((Mesh *)to->data)->loc;
	}
	else if (ELEM3(to->type, OB_CURVE, OB_SURF, OB_FONT)) {
		((Curve *)to->data)->texflag= texflag;
		poin1= ((Curve *)to->data)->loc;
	}
	else if(to->type==OB_MBALL) {
		((MetaBall *)to->data)->texflag= texflag;
		poin1= ((MetaBall *)to->data)->loc;
	}
	else
		return;
	
	memcpy(poin1, poin2, 9*sizeof(float));	/* this was noted in DNA_mesh, curve, mball */
	
	if(to->type==OB_MESH) ;
	else if(to->type==OB_MBALL) tex_space_mball(to);
	else tex_space_curve(to->data);
	
}

void copy_attr(short event)
{
	Object *ob;
	Base *base;
	Curve *cu, *cu1;
	Nurb *nu;
	int do_scene_sort= 0;
	
	if(G.scene->id.lib) return;

	if(!(ob=OBACT)) return;
	
	if(G.obedit) {
		/* obedit_copymenu(); */
		return;
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
					/* Quats arnt used yet */
					/*VECCOPY(base->object->quat, ob->quat);
					VECCOPY(base->object->dquat, ob->dquat);*/
				}
				else if(event==3) {  /* size */
					VECCOPY(base->object->size, ob->size);
					VECCOPY(base->object->dsize, ob->dsize);
				}
				else if(event==4) {  /* drawtype */
					base->object->dt= ob->dt;
					base->object->dtx= ob->dtx;
					base->object->empty_drawtype= ob->empty_drawtype;
					base->object->empty_drawsize= ob->empty_drawsize;
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

					base->object->dup_group= ob->dup_group;
					if(ob->dup_group)
						id_us_plus((ID *)ob->dup_group);
				}
				else if(event==7) {	/* mass */
					base->object->mass= ob->mass;
				}
				else if(event==8) {	/* damping */
					base->object->damping= ob->damping;
					base->object->rdamping= ob->rdamping;
				}
				else if(event==17) {	/* tex space */
					copy_texture_space(base->object, ob);
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
						cu1->wordspace= cu->wordspace;
						cu1->ulpos= cu->ulpos;
						cu1->ulheight= cu->ulheight;
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

						text_to_curve(base->object, 0);		/* needed? */

						
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
						cu1->resolu_ren= cu->resolu_ren;
						
						nu= cu1->nurb.first;
						
						while(nu) {
							nu->resolu= cu1->resolu;
							nu= nu->next;
						}
						
						base->object->recalc |= OB_RECALC_DATA;
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
					
					do_scene_sort= 1;
				}
				else if(event==23) {
					base->object->softflag= ob->softflag;
					if(base->object->soft) sbFree(base->object->soft);
					
					base->object->soft= copy_softbody(ob->soft);

					if (!modifiers_findByType(base->object, eModifierType_Softbody)) {
						BLI_addhead(&base->object->modifiers, modifier_new(eModifierType_Softbody));
					}
				}
				else if(event==26) {
					copy_nlastrips(&base->object->nlastrips, &ob->nlastrips);
				}
				else if(event==27) {	/* autosmooth */
					if (base->object->type==OB_MESH) {
						Mesh *me= ob->data;
						Mesh *cme= base->object->data;
						cme->smoothresh= me->smoothresh;
						if(me->flag & ME_AUTOSMOOTH)
							cme->flag |= ME_AUTOSMOOTH;
						else
							cme->flag &= ~ME_AUTOSMOOTH;
					}
				}
				else if(event==28) { /* UV orco */
					if ELEM(base->object->type, OB_CURVE, OB_SURF) {
						cu= ob->data;
						cu1= base->object->data;
						
						if(cu->flag & CU_UV_ORCO)
							cu1->flag |= CU_UV_ORCO;
						else
							cu1->flag &= ~CU_UV_ORCO;
					}		
				}
				else if(event==29) { /* protected bits */
					base->object->protectflag= ob->protectflag;
				}
				else if(event==30) { /* index object */
					base->object->index= ob->index;
				}
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	if(do_scene_sort)
		DAG_scene_sort(G.scene);

	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);

	if(event==20) {
		allqueue(REDRAWBUTSOBJECT, 0);
	}
	
	BIF_undo_push("Copy Attributes");
}

void copy_attr_menu()
{
	Object *ob;
	short event;
	char str[512];
	
	if(!(ob=OBACT)) return;
	
	if (G.obedit) {
		if (ob->type == OB_MESH)
			mesh_copy_menu();
		return;
	}
	
	/* Object Mode */
	
	/* If you change this menu, don't forget to update the menu in header_view3d.c
	 * view3d_edit_object_copyattrmenu() and in toolbox.c
	 */
	
	strcpy(str, "Copy Attributes %t|Location%x1|Rotation%x2|Size%x3|Draw Options%x4|Time Offset%x5|Dupli%x6|%l|Mass%x7|Damping%x8|Properties%x9|Logic Bricks%x10|Protected Transform%x29|%l");
	
	strcat (str, "|Object Constraints%x22");
	strcat (str, "|NLA Strips%x26");
	
	if (OB_SUPPORT_MATERIAL(ob)) {
		strcat(str, "|Texture Space%x17");
	}	
	
	if(ob->type == OB_FONT) strcat(str, "|Font Settings%x18|Bevel Settings%x19");
	if(ob->type == OB_CURVE) strcat(str, "|Bevel Settings%x19|UV Orco%x28");
	
	if((ob->type == OB_FONT) || (ob->type == OB_CURVE)) {
			strcat(str, "|Curve Resolution%x25");
	}

	if(ob->type==OB_MESH){
		strcat(str, "|Subsurf Settings%x21|AutoSmooth%x27");
	}

	if(ob->soft) strcat(str, "|Soft Body Settings%x23");
	
	strcat(str, "|Pass Index%x30");
	
	if(ob->type==OB_MESH || ob->type==OB_CURVE || ob->type==OB_LATTICE || ob->type==OB_SURF){
		strcat(str, "|Modifiers ...%x24");
	}

	event= pupmenu(str);
	if(event<= 0) return;
	
	copy_attr(event);
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
	int a;
	short nr=0;
	char *strp;

	if(!(ob=OBACT)) return;

	if(event==1) {
		IDnames_to_pupstring(&strp, NULL, NULL, &(G.main->scene), 0, &nr);
		
		if(nr == -2) {
			MEM_freeN(strp);

			activate_databrowse((ID *)G.scene, ID_SCE, 0, B_INFOSCE, &(G.curscreen->scenenr), link_to_scene );
			
			return;			
		}
		else {
			event= pupmenu_col(strp, 20);
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

	/* All non group linking */
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
				else if(event==6) {
					if(ob->dup_group) ob->dup_group->id.us--;
					obt->dup_group= ob->dup_group;
					if(obt->dup_group) {
						id_us_plus((ID *)obt->dup_group);
						obt->transflag |= OB_DUPLIGROUP;
					}
				}
				else if(event==3) {  /* materials */
					
					/* new approach, using functions from kernel */
					for(a=0; a<ob->totcol; a++) {
						Material *ma= give_current_material(ob, a+1);
						assign_material(obt, ma, a+1);	/* also works with ma==NULL */
					}
				}
			}
		}
		base= base->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSHEAD, 0);

	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);

	BIF_undo_push("Create links");
}

void apply_objects_locrot( void )
{
	Base *base, *basact;
	Object *ob;
	bArmature *arm;
	Mesh *me;
	Curve *cu;
	Nurb *nu;
	BPoint *bp;
	BezTriple *bezt;
	MVert *mvert;
	float mat[3][3];
	int a, change = 0;
	
	
	/* first check if we can execute */
	for (base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			if(ob->type==OB_MESH) {
				me= ob->data;
				
				if(me->id.us>1) {
					error("Can't apply to a multi user mesh, doing nothing.");
					return;
				}
				if(me->key) {
					error("Can't apply to a mesh with vertex keys, doing nothing.");
					return;
				}
			}
			else if (ob->type==OB_ARMATURE) {
				arm= ob->data;
				
				if(arm->id.us>1) {
					error("Can't apply to a multi user armature, doing nothing.");
					return;
				}
			}
			else if ELEM(ob->type, OB_CURVE, OB_SURF) {
				cu= ob->data;
				
				if(cu->id.us>1) {
					error("Can't apply to a multi user curve, doing nothing.");
					return;
				}
				if(cu->key) {
					error("Can't apply to a curve with vertex keys, doing nothing.");
					return;
				}
			}
		}
	}
	
	/* now execute */
	basact= BASACT;
	base= FIRSTBASE;
	for (base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			if(ob->type==OB_MESH) {
				object_to_mat3(ob, mat);
				me= ob->data;
				
				/* see checks above */
				
				mvert= me->mvert;
				for(a=0; a<me->totvert; a++, mvert++) {
					Mat3MulVecfl(mat, mvert->co);
				}
				ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				/*QuatOne(ob->quat);*/ /* Quats arnt used yet */
				
				where_is_object(ob);
				
				/* texspace and normals */
				BASACT= base;
				enter_editmode(EM_WAITCURSOR);
				BIF_undo_push("Applied object");	/* editmode undo itself */
				exit_editmode(EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */
				BASACT= basact;
				
				change = 1;
			}
			else if (ob->type==OB_ARMATURE) {
				object_to_mat3(ob, mat);
				arm= ob->data;
				
				/* see checks above */
				apply_rot_armature(ob, mat);
				
				/* Reset the object's transforms */
				ob->size[0]= ob->size[1]= ob->size[2]= 1.0;
				ob->rot[0]= ob->rot[1]= ob->rot[2]= 0.0;
				/*QuatOne(ob->quat); (not used anymore)*/
				
				where_is_object(ob);
				
				change = 1;
			}
			else if ELEM(ob->type, OB_CURVE, OB_SURF) {
				float scale;
				object_to_mat3(ob, mat);
				scale = Mat3ToScalef(mat);
				cu= ob->data;
				
				/* see checks above */
				
				nu= cu->nurb.first;
				while(nu) {
					if( (nu->type & 7)==1) {
						a= nu->pntsu;
						bezt= nu->bezt;
						while(a--) {
							Mat3MulVecfl(mat, bezt->vec[0]);
							Mat3MulVecfl(mat, bezt->vec[1]);
							Mat3MulVecfl(mat, bezt->vec[2]);
							bezt->radius *= scale;
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
				/*QuatOne(ob->quat); (quats arnt used anymore)*/
				
				where_is_object(ob);
				
				/* texspace and normals */
				BASACT= base;
				enter_editmode(EM_WAITCURSOR);
				BIF_undo_push("Applied object");	/* editmode undo itself */
				exit_editmode(EM_FREEDATA|EM_WAITCURSOR); /* freedata, but no undo */
				BASACT= basact;
				
				change = 1;
			} else {
				continue;
			}
			
			ignore_parent_tx(ob);
		}
	}
	if (change) {
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Apply Objects Scale & Rotation");
	}
}

void apply_objects_visual_tx( void )
{
	Base *base;
	Object *ob;
	int change = 0;
	
	for (base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			where_is_object(ob);
			VECCOPY(ob->loc, ob->obmat[3]);
			Mat4ToSize(ob->obmat, ob->size);
			Mat4ToEul(ob->obmat, ob->rot);
			
			where_is_object(ob);
			
			change = 1;
		}
	}
	if (change) {
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Apply Objects Visual Transform");
	}
}

void apply_object( void )
{
	Object *ob;
	int evt;
	if(G.scene->id.lib) return;
	if(G.obedit) return;
	
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
		
	} else {
		
		evt = pupmenu("Apply Object%t|Scale and Rotation to ObData|Visual Transform to Objects Loc/Scale/Rot");
		if (evt==-1) return;
		
		if (evt==1) {
			apply_objects_locrot();
		} else if (evt==2) {
			apply_objects_visual_tx();
		}
	}
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
	len2= Normalize(axis);
	
	Crossf(n, speed, axis);
	len= Normalize(n);
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
	len= Normalize(speed);
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
	short context = (U.flag & USER_DRAGIMMEDIATE)?CTX_TWEAK:CTX_NONE;
	
	/* check for left mouse select/right mouse select */
	
	if(curarea->spacetype==SPACE_NODE)
		mousebut = L_MOUSE|R_MOUSE;
	else if (U.flag & USER_LMOUSESELECT) 
		mousebut = L_MOUSE;
	else 
		mousebut = R_MOUSE;
	
	getmouseco_areawin(mval);
	xo= mval[0]; 
	yo= mval[1];
	 
	while(get_mbut() & mousebut) {
		getmouseco_areawin(mval);
		if(abs(mval[0]-xo)+abs(mval[1]-yo) > 10) {
			if(curarea->spacetype==SPACE_VIEW3D) {
				initTransform(TFM_TRANSLATION, context);
				Transform();
			}
			else if(curarea->spacetype==SPACE_IMAGE) {
				initTransform(TFM_TRANSLATION, context);
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
				if(curarea->spacetype==SPACE_VIEW3D) {
					toolbox_n();
					return;
				}
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

#ifdef WITH_VERSE
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(ob->vnode) {
			error("Can't make data single user, when data are shared at verse server");
			return;
		}
		base = base->next;
	}
#endif

	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(ob->id.lib==NULL && (base->flag & flag)==flag ) {
			id= ob->data;
			
			if(id && id->us>1 && id->lib==0) {
				ob->recalc= OB_RECALC_DATA;
				
				switch(ob->type) {
				case OB_LAMP:
					if(id && id->us>1 && id->lib==NULL) {
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
					me= ob->data= copy_mesh(ob->data);
					if(me && me->key)
						ipo_idnew(me->key->ipo);	/* drivers */
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
			if (id && id->us>1 && id->lib==NULL){
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
			if(id && id->us>1 && id->lib==NULL) {
				if(id->newid) {
					ob->ipo= (Ipo *)id->newid;
					id_us_plus(id->newid);
				}
				else {
					ob->ipo= copy_ipo(ob->ipo);
					id->us--;
					id->newid= (ID *)ob->ipo;
				}
				ipo_idnew(ob->ipo);	/* drivers */
			}
			/* other ipos */
			switch(ob->type) {
			case OB_LAMP:
				la= ob->data;
				if(la->ipo && la->ipo->id.us>1) {
					la->ipo->id.us--;
					la->ipo= copy_ipo(la->ipo);
					ipo_idnew(la->ipo);	/* drivers */
				}
				break;
			case OB_CAMERA:
				cam= ob->data;
				if(cam->ipo && cam->ipo->id.us>1) {
					cam->ipo->id.us--;
					cam->ipo= copy_ipo(cam->ipo);
					ipo_idnew(cam->ipo);	/* drivers */
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

void single_ipo_users(int flag)
{
	Object *ob;
	Base *base;
	ID *id;
	
	base= FIRSTBASE;
	while(base) {
		ob= base->object;
		if(ob->id.lib==NULL && (flag==0 || (base->flag & SELECT)) ) {
			ob->recalc= OB_RECALC_DATA;
			
			id= (ID *)ob->ipo;
			if(id && id->us>1 && id->lib==NULL) {
				ob->ipo= copy_ipo(ob->ipo);
				id->us--;
				ipo_idnew(ob->ipo);	/* drivers */
			}
		}
		base= base->next;
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
							ipo_idnew(ma->ipo);	/* drivers */
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
	
	nr= pupmenu("Make Single User%t|Object|Object & ObData|Object & ObData & Materials+Tex|Materials+Tex|Ipos");
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
		else if(nr==5) {
			single_ipo_users(1);
		}
		
		
		clear_id_newpoins();

		countall();
		allqueue(REDRAWALL, 0);
		BIF_undo_push("Single user");
	}
}

/* ************************************************************* */

/* helper for below, ma was checked to be not NULL */
static void make_local_makelocalmaterial(Material *ma)
{
	ID *id;
	int b;
	
	make_local_material(ma);
	
	for(b=0; b<MAX_MTEX; b++) {
		if(ma->mtex[b] && ma->mtex[b]->tex) {
			make_local_texture(ma->mtex[b]->tex);
		}
	}
	
	id= (ID *)ma->ipo;
	if(id && id->lib) make_local_ipo(ma->ipo);	
	
	/* nodetree? XXX */
}

void make_local_menu(void)
{
	int mode;
	
	/* If you modify this menu, please remember to update view3d_edit_object_makelocalmenu
	* in header_view3d.c and the menu in toolbox.c
	*/
	
	if(G.scene->id.lib) return;
	
	mode = pupmenu("Make Local%t|Selected Objects %x1|Selected Objects and Data %x2|All %x3");
	
	if (mode <= 0) return;
	
	make_local(mode);
}

void make_local(int mode)
{
	Base *base;
	Object *ob;
	bActionStrip *strip;
	ParticleSystem *psys;
	Material *ma, ***matarar;
	Lamp *la;
	Curve *cu;
	ID *id;
	int a, b;
	
	/* WATCH: the function new_id(..) re-inserts the id block!!! */
	if(G.scene->id.lib) return;
	
	if(mode==3) {
		all_local(NULL, 0);	/* NULL is all libs */
		allqueue(REDRAWALL, 0);
		return;
	}
	else if(mode<1) return;

	clear_id_newpoins();
	
	base= FIRSTBASE;
	while(base) {
		if( TESTBASE(base) ) {
			ob= base->object;
			if(ob->id.lib) {
				make_local_object(ob);
			}
		}
		base= base->next;
	}
	
	/* maybe object pointers */
	base= FIRSTBASE;
	while(base) {
		if( TESTBASE(base) ) {
			ob= base->object;
			if(ob->id.lib==NULL) {
				ID_NEW(ob->parent);
				ID_NEW(ob->track);
			}
		}
		base= base->next;
	}

	base= FIRSTBASE;
	while(base) {
		if( TESTBASE(base) ) {
			ob= base->object;
			id= ob->data;
			
			if(id && mode>1) {
				
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

				for(psys=ob->particlesystem.first; psys; psys=psys->next)
					make_local_particlesettings(psys->part);
			}
			id= (ID *)ob->ipo;
			if(id && id->lib) make_local_ipo(ob->ipo);

			id= (ID *)ob->action;
			if(id && id->lib) make_local_action(ob->action);
			
			for(strip=ob->nlastrips.first; strip; strip=strip->next) {
				if(strip->act && strip->act->id.lib)
					make_local_action(strip->act);
			}
		}
		base= base->next;		
	}

	if(mode>1) {
		base= FIRSTBASE;
		while(base) {
			if( TESTBASE(base) ) {
				ob= base->object;
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
						if(ma)
							make_local_makelocalmaterial(ma);
					}
					
					matarar= (Material ***)give_matarar(ob);
					if (matarar) {
						for(a=0; a<ob->totcol; a++) {
							ma= (*matarar)[a];
							if(ma)
								make_local_makelocalmaterial(ma);
						}
					}
				}
			}
			base= base->next;
		}
	}

	allqueue(REDRAWALL, 0);
	BIF_undo_push("Make local");
}

static void copy_object__forwardModifierLinks(void *userData, Object *ob,
                                              ID **idpoin)
{
	/* this is copied from ID_NEW; it might be better to have a macro */
	if(*idpoin && (*idpoin)->newid) *idpoin = (*idpoin)->newid;
}


/* after copying objects, copied data should get new pointers */
static void copy_object_set_idnew(int dupflag)
{
	Base *base;
	Object *ob;
	Material *ma, *mao;
	ID *id;
	Ipo *ipo;
	bActionStrip *strip;
	int a;
	
	/* check object pointers */
	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			relink_constraints(&ob->constraints);
			if (ob->pose){
				bPoseChannel *chan;
				for (chan = ob->pose->chanbase.first; chan; chan=chan->next){
					relink_constraints(&chan->constraints);
				}
			}
			modifiers_foreachIDLink(ob, copy_object__forwardModifierLinks, NULL);
			ID_NEW(ob->parent);
			ID_NEW(ob->track);
			ID_NEW(ob->proxy);
			ID_NEW(ob->proxy_group);
			
			for(strip= ob->nlastrips.first; strip; strip= strip->next) {
				bActionModifier *amod;
				for(amod= strip->modifiers.first; amod; amod= amod->next)
					ID_NEW(amod->ob);
			}
		}
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
	
	/* lamps */
	if( dupflag & USER_DUP_IPO) {
		Lamp *la= G.main->lamp.first;
		while(la) {
			if(la->id.newid) {
				Lamp *lan= (Lamp *)la->id.newid;
				id= (ID *)lan->ipo;
				if(id) {
					ID_NEW_US(lan->ipo)
					else lan->ipo= copy_ipo(lan->ipo);
					id->us--;
				}
			}
			la= la->id.next;
		}
	}
	
	/* ipos */
	ipo= G.main->ipo.first;
	while(ipo) {
		if(ipo->id.lib==NULL && ipo->id.newid) {
			Ipo *ipon= (Ipo *)ipo->id.newid;
			IpoCurve *icu;
			for(icu= ipon->curve.first; icu; icu= icu->next) {
				if(icu->driver) {
					ID_NEW(icu->driver->ob);
				}
			}
		}
		ipo= ipo->id.next;
	}
	
	set_sca_new_poins();
	
	clear_id_newpoins();

}

/* This function duplicated the current visible selection, its used by Duplicate and Linked Duplicate
Alt+D/Shift+D as well as Pythons Object.Duplicate(), it takes
mode: 
	0: Duplicate with transform, Redraw.
	1: Duplicate, no transform, Redraw
	2: Duplicate, no transform, no redraw (Only used by python)
if true the user will not be dropped into grab mode directly after and..
dupflag: a flag made from constants declared in DNA_userdef_types.h
	The flag tells adduplicate() weather to copy data linked to the object, or to reference the existing data.
	U.dupflag for default operations or you can construct a flag as python does
	if the dupflag is 0 then no data will be copied (linked duplicate) */

void adduplicate(int mode, int dupflag)
{
	Base *base, *basen;
	Material ***matarar;
	Object *ob, *obn;
	ID *id;
	int a, didit;
	
	if(G.scene->id.lib) return;
	clear_id_newpoins();
	clear_sca_new_poins();	/* sensor/contr/act */
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
		
			ob= base->object;
			if(ob->flag & OB_POSEMODE) {
				; /* nothing? */
			}
			else {
				obn= copy_object(ob);
				obn->recalc |= OB_RECALC;
				
				basen= MEM_mallocN(sizeof(Base), "duplibase");
				*basen= *base;
				BLI_addhead(&G.scene->base, basen);	/* addhead: prevent eternal loop */
				basen->object= obn;
				base->flag &= ~SELECT;
				
				if(basen->flag & OB_FROMGROUP) {
					Group *group;
					for(group= G.main->group.first; group; group= group->id.next) {
						if(object_in_group(ob, group))
							add_to_group(group, obn);
					}
					obn->flag |= OB_FROMGROUP; /* this flag is unset with copy_object() */
				}
				
				if(BASACT==base) BASACT= basen;

				/* duplicates using userflags */
				
				if(dupflag & USER_DUP_IPO) {
					bConstraintChannel *chan;
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
				if(dupflag & USER_DUP_ACT){ /* Not buttons in the UI to modify this, add later? */
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
							
							if(obn->fluidsimSettings) {
							obn->fluidsimSettings->orgMesh = (Mesh *)obn->data;
							}
							
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
					
				case OB_LATTICE:
					if(dupflag!=0) {
						ID_NEW_US2(obn->data )
						else obn->data= copy_lattice(obn->data);
						id->us--;
					}
					break;
				case OB_CAMERA:
					if(dupflag!=0) {
						ID_NEW_US2(obn->data )
						else obn->data= copy_camera(obn->data);
						id->us--;
					}
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
#ifdef WITH_VERSE
				/* send new created object to verse server,
				 * when original object was linked with object node */
				if(ob->vnode) {
					b_verse_duplicate_object(((VNode*)ob->vnode)->session, ob, obn);
				}
#endif
			}
						
		}
		base= base->next;
	}

	copy_object_set_idnew(dupflag);

	DAG_scene_sort(G.scene);
	DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);

	countall();
	if(mode==0) {
		BIF_TransformSetUndo("Add Duplicate");
		initTransform(TFM_TRANSLATION, CTX_NONE);
		Transform();
	}
	set_active_base(BASACT);
	if(mode!=2) { /* mode of 2 is used by python to avoid unrequested redraws */
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWACTION, 0);	/* also oops */
		allqueue(REDRAWIPO, 0);	/* also oops */
	}
}

void make_duplilist_real()
{
	Base *base, *basen;
	Object *ob;
	/*	extern ListBase duplilist; */
	
	if(okee("Make dupli objects real")==0) return;
	
	clear_id_newpoins();
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			
			if(base->object->transflag & OB_DUPLI) {
				ListBase *lb= object_duplilist(G.scene, base->object);
				DupliObject *dob;
				
				for(dob= lb->first; dob; dob= dob->next) {
					ob= copy_object(dob->ob);
					/* font duplis can have a totcol without material, we get them from parent
					* should be implemented better...
					*/
					if(ob->mat==NULL) ob->totcol= 0;
					
					basen= MEM_dupallocN(base);
					basen->flag &= ~OB_FROMDUPLI;
					BLI_addhead(&G.scene->base, basen);	/* addhead: othwise eternal loop */
					basen->object= ob;
					ob->ipo= NULL;		/* make sure apply works */
					ob->parent= ob->track= NULL;
					ob->disp.first= ob->disp.last= NULL;
					ob->transflag &= ~OB_DUPLI;	
					
					Mat4CpyMat4(ob->obmat, dob->mat);
					apply_obmat(ob);
				}
				
				copy_object_set_idnew(0);
				
				free_object_duplilist(lb);
				
				base->object->transflag &= ~OB_DUPLI;	
			}
		}
		base= base->next;
	}
	
	DAG_scene_sort(G.scene);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Make duplicates real");
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
	nr= pupmenu("Select Linked%t|Object Ipo%x1|ObData%x2|Material%x3|Texture%x4|DupliGroup%x5|ParticleSystem%x6");
	
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
	short changed = 0;
	/* events (nr):
	 * Object Ipo: 1
	 * ObData: 2
	 * Current Material: 3
	 * Current Texture: 4
	 * DupliGroup: 5
	 * PSys: 6
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
			if(mat->mtex[ (int)mat->texact ]) tex= mat->mtex[ (int)mat->texact ]->tex;
			if(tex==0) return;
		}
	}
	else if(nr==5) {
		if(ob->dup_group==NULL) return;
	}
	else if(nr==6) {
		if(ob->particlesystem.first==NULL) return;
	}
	else return;
	
	base= FIRSTBASE;
	while(base) {
		if (BASE_SELECTABLE(base) && !(base->flag & SELECT)) {
			if(nr==1) {
				if(base->object->ipo==ipo) base->flag |= SELECT;
				changed = 1;
			}
			else if(nr==2) {
				if(base->object->data==obdata) base->flag |= SELECT;
				changed = 1;
			}
			else if(nr==3 || nr==4) {
				ob= base->object;
				
				for(a=1; a<=ob->totcol; a++) {
					mat1= give_current_material(ob, a);
					if(nr==3) {
						if(mat1==mat) base->flag |= SELECT;
						changed = 1;
					}
					else if(mat1 && nr==4) {
						for(b=0; b<MAX_MTEX; b++) {
							if(mat1->mtex[b]) {
								if(tex==mat1->mtex[b]->tex) {
									base->flag |= SELECT;
									changed = 1;
									break;
								}
							}
						}
					}
				}
			}
			else if(nr==5) {
				if(base->object->dup_group==ob->dup_group) {
					 base->flag |= SELECT;
					 changed = 1;
				}
			}
			else if(nr==6) {
				/* loop through other, then actives particles*/
				ParticleSystem *psys;
				ParticleSystem *psys_act;
				
				for(psys=base->object->particlesystem.first; psys; psys=psys->next) {
					for(psys_act=ob->particlesystem.first; psys_act; psys_act=psys_act->next) {
						if (psys->part == psys_act->part) {
							base->flag |= SELECT;
							changed = 1;
							break;
						}
					}
					
					if (base->flag & SELECT) {
						break;
					}
				}
			}
			base->object->flag= base->flag;
		}
		base= base->next;
	}
	
	if (changed) {
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWDATASELECT, 0);
		allqueue(REDRAWOOPS, 0);
		BIF_undo_push("Select linked");
	}
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
							if(tex->type==TEX_IMAGE && tex->ima) {
								ImBuf *ibuf= BKE_image_get_ibuf(tex->ima, NULL);
								
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
							
								x= ibuf->x/space;
								y= ibuf->y;
								
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
	
	if (!G.vd) {
		error("Can't do this! Open a 3D window");
		return;
	}
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base->object->ipoflag & OB_DRAWKEY) {
				set= 0;
				break;
			}
		}
		base= base->next;
	}
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(set) {
				base->object->ipoflag |= OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 1;
			}
			else {
				base->object->ipoflag &= ~OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 0;
			}
		}
		base= base->next;
	}
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWNLA, 0);
	allqueue (REDRAWACTION, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWIPO, 0);
}

void select_select_keys(void)
{
	Base *base;
	IpoCurve *icu;
	BezTriple *bezt;
	int a;
	
	if (!G.vd) {
		error("Can't do this! Open a 3D window");
		return;
	}
	
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

	if(BASACT==0 || G.vd==NULL) return;
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

void ofs_timeoffs(void)
{
	Base *base;
	float offset=0.0f;

	if(BASACT==0 || G.vd==NULL) return;
	
	if(fbutton(&offset, -10000.0f, 10000.0f, 10, 10, "Offset")==0) return;

	/* make array of all bases, xco yco (screen) */
	base= FIRSTBASE;
	while(base) {
		if(TESTBASELIB(base)) {
			base->object->sf += offset;
			if (base->object->sf < -MAXFRAMEF)		base->object->sf = -MAXFRAMEF;
			else if (base->object->sf > MAXFRAMEF)	base->object->sf = MAXFRAMEF;
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}


void rand_timeoffs(void)
{
	Base *base;
	float rand=0.0f;

	if(BASACT==0 || G.vd==NULL) return;
	
	if(fbutton(&rand, 0.0f, 10000.0f, 10, 10, "Randomize")==0) return;
	
	rand *= 2;
	
	base= FIRSTBASE;
	while(base) {
		if(TESTBASELIB(base)) {
			base->object->sf += (BLI_drand()-0.5) * rand;
			if (base->object->sf < -MAXFRAMEF)		base->object->sf = -MAXFRAMEF;
			else if (base->object->sf > MAXFRAMEF)	base->object->sf = MAXFRAMEF;
		}
		base= base->next;
	}

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
	if(G.f & G_PARTICLEEDIT) {
		PE_mirror_x(0);
	}
	else {
		initTransform(TFM_MIRROR, CTX_NO_PET);
		Transform();
	}
}

void hookmenu(void)
{
	/* only called in object mode */
	short event, changed=0;
	Object *ob;
	Base *base;
	ModifierData *md;
	HookModifierData *hmd;
	
	event= pupmenu("Modify Hooks for Selected...%t|Reset Offset%x1|Recenter at Cursor%x2");
	if (event==-1) return;
	if (event==2 && !(G.vd)) {
		error("Cannot perform this operation without a 3d view");
		return;
	}
	
	for (base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			for (md = base->object->modifiers.first; md; md=md->next) {
				if (md->type==eModifierType_Hook) {
					ob = base->object;
					hmd = (HookModifierData*) md;
					
					/*
					 * Copied from modifiers_cursorHookCenter and
					 * modifiers_clearHookOffset, should consolidate
					 * */
					
					if (event==1) {
						if(hmd->object) {
							Mat4Invert(hmd->object->imat, hmd->object->obmat);
							Mat4MulSerie(hmd->parentinv, hmd->object->imat, ob->obmat, NULL, NULL, NULL, NULL, NULL, NULL);
							
							changed= 1;
							DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
						}
					} else {
						float *curs = give_cursor();
						float bmat[3][3], imat[3][3];
						
						where_is_object(ob);
					
						Mat3CpyMat4(bmat, ob->obmat);
						Mat3Inv(imat, bmat);
				
						curs= give_cursor();
						hmd->cent[0]= curs[0]-ob->obmat[3][0];
						hmd->cent[1]= curs[1]-ob->obmat[3][1];
						hmd->cent[2]= curs[2]-ob->obmat[3][2];
						Mat3MulVecfl(imat, hmd->cent);
						
						changed= 1;
						DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
					} 
				}
			}
		}
	}
	
	if (changed) {
		if (event==1)
			BIF_undo_push("Clear hook offset for selected");
		else if (event==2)
			BIF_undo_push("Hook cursor center for selected");
		
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}	
}

/*
 * Returns true if the Object is a from an external blend file (libdata)
 */
int object_is_libdata(Object *ob)
{
	if (!ob) return 0;
	if (ob->proxy) return 0;
	if (ob->id.lib) return 1;
	return 0;
}


/*
 * Returns true if the Object data is a from an external blend file (libdata)
 */
int object_data_is_libdata(Object *ob)
{
	if (!ob) return 0;
	if (ob->proxy) return 0;
	if (ob->id.lib) return 1;
	if (!ob->data) return 0;
	if (((ID *)ob->data)->lib) return 1;
	return 0;
}

void hide_objects(int select)
{
	Base *base;
	short changed = 0, changed_act = 0;
	for(base = FIRSTBASE; base; base=base->next){
		if(TESTBASELIB(base)==select){
			base->flag &= ~SELECT;
			base->object->flag = base->flag;
			base->object->restrictflag |= OB_RESTRICT_VIEW;
			changed = 1;
			if (base==BASACT) {
				BASACT= NULL;
				changed_act = 1;
			}
		}
	}
	if (changed) {
		if(select) BIF_undo_push("Hide Selected Objects");
		else if(select) BIF_undo_push("Hide Unselected Objects");
		DAG_scene_sort(G.scene);
		allqueue(REDRAWVIEW3D,0);
		allqueue(REDRAWOOPS,0);
		allqueue(REDRAWDATASELECT,0);
		if (changed_act) { /* these spaces depend on the active object */
			allqueue(REDRAWBUTSALL,0);
			allqueue(REDRAWIPO,0);
			allqueue(REDRAWACTION,0);
		}
		countall();
	}
}

void show_objects(void)
{
	Base *base;
	int changed = 0;
	for(base = FIRSTBASE; base; base=base->next){
		if((base->lay & G.vd->lay) && base->object->restrictflag & OB_RESTRICT_VIEW) {
			base->flag |= SELECT;
			base->object->flag = base->flag;
			base->object->restrictflag &= ~OB_RESTRICT_VIEW; 
			changed = 1;
		}
	}
	if (changed) {
		BIF_undo_push("Unhide Objects");
		DAG_scene_sort(G.scene);
		allqueue(REDRAWVIEW3D,0);
		allqueue(REDRAWOOPS,0);
		countall();
	}
}
