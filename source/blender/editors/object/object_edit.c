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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
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
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"
#include "BLI_rand.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_booleanops.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
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
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_property.h"
#include "BKE_report.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_particle.h"
#include "ED_mesh.h"
#include "ED_mball.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_types.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

/* for menu/popup icons etc etc*/
#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"	// own include

/* ************* XXX **************** */
static void error() {}
static void waitcursor(int val) {}
static int pupmenu(const char *msg) {return 0;}

/* port over here */
static bContext *C;
static void error_libdata() {}

/* ********************************** */

/* --------------------------------- */

void ED_object_apply_obmat(Object *ob)
{
	float mat[3][3], imat[3][3], tmat[3][3];
	
	/* from obmat to loc rot size */
	
	if(ob==NULL) return;
	copy_m3_m4(mat, ob->obmat);
	
	VECCOPY(ob->loc, ob->obmat[3]);

	mat3_to_eul( ob->rot,mat);
	eul_to_mat3( tmat,ob->rot);

	invert_m3_m3(imat, tmat);
	
	mul_m3_m3m3(tmat, imat, mat);
	
	ob->size[0]= tmat[0][0];
	ob->size[1]= tmat[1][1];
	ob->size[2]= tmat[2][2];
	
}

/* ********* clear/set restrict view *********/
static int object_restrictview_clear_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	Base *base;
	int changed = 0;
	
	/* XXX need a context loop to handle such cases */
	for(base = FIRSTBASE; base; base=base->next){
		if((base->lay & v3d->lay) && base->object->restrictflag & OB_RESTRICT_VIEW) {
			base->flag |= SELECT;
			base->object->flag = base->flag;
			base->object->restrictflag &= ~OB_RESTRICT_VIEW; 
			changed = 1;
		}
	}
	if (changed) {
		DAG_scene_sort(scene);
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_restrictview_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Clear Restrict View";
	ot->description = "Reveal the object by setting the restrictview flag.";
	ot->idname= "OBJECT_OT_restrictview_clear";
	
	/* api callbacks */
	ot->exec= object_restrictview_clear_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int object_restrictview_set_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	short changed = 0;
	int unselected= RNA_boolean_get(op->ptr, "unselected");
	
	CTX_DATA_BEGIN(C, Base*, base, visible_bases) {
		if(!unselected) {
			if (base->flag & SELECT){
				base->flag &= ~SELECT;
				base->object->flag = base->flag;
				base->object->restrictflag |= OB_RESTRICT_VIEW;
				changed = 1;
				if (base==BASACT) {
					ED_base_object_activate(C, NULL);
				}
			}
		}
		else {
			if (!(base->flag & SELECT)){
				base->object->restrictflag |= OB_RESTRICT_VIEW;
				changed = 1;
			}
		}	
	}
	CTX_DATA_END;

	if (changed) {
		DAG_scene_sort(scene);
		
		WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, CTX_data_scene(C));
		
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_restrictview_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Restrict View";
	ot->description = "Hide the object by setting the restrictview flag.";
	ot->idname= "OBJECT_OT_restrictview_set";
	
	/* api callbacks */
	ot->exec= object_restrictview_set_exec;
	ot->poll= ED_operator_view3d_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects.");
	
}


/* ******************* toggle editmode operator  ***************** */

void ED_object_exit_editmode(bContext *C, int flag)
{
	/* Note! only in exceptional cases should 'EM_DO_UNDO' NOT be in the flag */

	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	int freedata = flag & EM_FREEDATA;
	
	if(obedit==NULL) return;
	
	if(flag & EM_WAITCURSOR) waitcursor(1);
	if(obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		
//		if(EM_texFaceCheck())
		
//		if(retopo_mesh_paint_check())
//			retopo_end_okee();
		
		if(me->edit_mesh->totvert>MESH_MAX_VERTS) {
			error("Too many vertices");
			return;
		}
		load_editMesh(scene, obedit);
		
		if(freedata) {
			free_editMesh(me->edit_mesh);
			MEM_freeN(me->edit_mesh);
			me->edit_mesh= NULL;
		}
		
		if(obedit->restore_mode & OB_MODE_WEIGHT_PAINT)
			mesh_octree_table(obedit, NULL, NULL, 'e');
	}
	else if (obedit->type==OB_ARMATURE) {	
		ED_armature_from_edit(obedit);
		if(freedata)
			ED_armature_edit_free(obedit);
	}
	else if(ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		load_editNurb(obedit);
		if(freedata) free_editNurb(obedit);
	}
	else if(obedit->type==OB_FONT && freedata) {
		load_editText(obedit);
		if(freedata) free_editText(obedit);
	}
	else if(obedit->type==OB_LATTICE) {
		load_editLatt(obedit);
		if(freedata) free_editLatt(obedit);
	}
	else if(obedit->type==OB_MBALL) {
		load_editMball(obedit);
		if(freedata) free_editMball(obedit);
	}

	/* freedata only 0 now on file saves and render */
	if(freedata) {
		ListBase pidlist;
		PTCacheID *pid;

		/* for example; displist make is different in editmode */
		scene->obedit= NULL; // XXX for context

		/* flag object caches as outdated */
		BKE_ptcache_ids_from_object(&pidlist, obedit);
		for(pid=pidlist.first; pid; pid=pid->next) {
			if(pid->type != PTCACHE_TYPE_PARTICLES) /* particles don't need reset on geometry change */
				pid->cache->flag |= PTCACHE_OUTDATED;
		}
		BLI_freelistN(&pidlist);
		
		BKE_ptcache_object_reset(scene, obedit, PTCACHE_RESET_OUTDATED);

		/* also flush ob recalc, doesn't take much overhead, but used for particles */
		DAG_id_flush_update(&obedit->id, OB_RECALC_OB|OB_RECALC_DATA);
	
		if(flag & EM_DO_UNDO)
			ED_undo_push(C, "Editmode");
	
		if(flag & EM_WAITCURSOR) waitcursor(0);
	
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, scene);

		obedit->mode &= ~OB_MODE_EDIT;
		ED_object_toggle_modes(C, obedit->restore_mode);
	}
}


void ED_object_enter_editmode(bContext *C, int flag)
{
	Scene *scene= CTX_data_scene(C);
	Base *base= NULL;
	Object *ob;
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= NULL;
	int ok= 0;
	
	if(scene->id.lib) return;
	
	if(sa && sa->spacetype==SPACE_VIEW3D)
		v3d= sa->spacedata.first;
	
	if((flag & EM_IGNORE_LAYER)==0) {
		base= CTX_data_active_base(C); /* active layer checked here for view3d */

		if(base==NULL) return;
		else if(v3d && (base->lay & v3d->lay)==0) return;
		else if(!v3d && (base->lay & scene->lay)==0) return;
	}
	else {
		base= scene->basact;
	}

	if (ELEM3(NULL, base, base->object, base->object->data)) return;

	ob = base->object;
	
	if (object_data_is_libdata(ob)) {
		error_libdata();
		return;
	}
	
	if(flag & EM_WAITCURSOR) waitcursor(1);

	ob->restore_mode = ob->mode;
	ED_object_toggle_modes(C, ob->mode);

	ob->mode |= OB_MODE_EDIT;
	
	if(ob->type==OB_MESH) {
		Mesh *me= ob->data;
		
		if(me->pv) mesh_pmv_off(ob, me);
		ok= 1;
		scene->obedit= ob;	// context sees this
		
		make_editMesh(scene, ob);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_MESH, scene);
	}
	else if (ob->type==OB_ARMATURE){
		bArmature *arm= base->object->data;
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
		scene->obedit= ob;
		ED_armature_to_edit(ob);
		/* to ensure all goes in restposition and without striding */
		DAG_id_flush_update(&ob->id, OB_RECALC);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_ARMATURE, scene);
	}
	else if(ob->type==OB_FONT) {
		scene->obedit= ob; // XXX for context
		ok= 1;
 		make_editText(ob);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_TEXT, scene);
	}
	else if(ob->type==OB_MBALL) {
		scene->obedit= ob; // XXX for context
		ok= 1;
		make_editMball(ob);

		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_MBALL, scene);
	}
	else if(ob->type==OB_LATTICE) {
		scene->obedit= ob; // XXX for context
		ok= 1;
		make_editLatt(ob);
		
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_LATTICE, scene);
	}
	else if(ob->type==OB_SURF || ob->type==OB_CURVE) {
		ok= 1;
		scene->obedit= ob; // XXX for context
		make_editNurb(ob);
		
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_EDITMODE_CURVE, scene);
	}
	
	if(ok) {
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		scene->obedit= NULL; // XXX for context
		ob->mode &= ~OB_MODE_EDIT;
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, scene);
	}
	
	if(flag & EM_DO_UNDO) ED_undo_push(C, "Enter Editmode");
	if(flag & EM_WAITCURSOR) waitcursor(0);
}

static int editmode_toggle_exec(bContext *C, wmOperator *op)
{
	if(!CTX_data_edit_object(C))
		ED_object_enter_editmode(C, EM_WAITCURSOR);
	else
		ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
	
	return OPERATOR_FINISHED;
}

static int editmode_toggle_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && (ob->type == OB_MESH || ob->type == OB_ARMATURE ||
		      ob->type == OB_FONT || ob->type == OB_MBALL ||
		      ob->type == OB_LATTICE || ob->type == OB_SURF ||
		      ob->type == OB_CURVE);
}

void OBJECT_OT_editmode_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Toggle Editmode";
	ot->description = "Toggle object's editmode.";
	ot->idname= "OBJECT_OT_editmode_toggle";
	
	/* api callbacks */
	ot->exec= editmode_toggle_exec;
	
	ot->poll= editmode_toggle_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* *************************** */

static int posemode_exec(bContext *C, wmOperator *op)
{
	Base *base= CTX_data_active_base(C);
	
	if(base->object->type==OB_ARMATURE) {
		if(base->object==CTX_data_edit_object(C)) {
			ED_object_exit_editmode(C, EM_FREEDATA|EM_DO_UNDO);
			ED_armature_enter_posemode(C, base);
		}
		else if(base->object->mode & OB_MODE_POSE)
			ED_armature_exit_posemode(C, base);
		else
			ED_armature_enter_posemode(C, base);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_PASS_THROUGH;
}

void OBJECT_OT_posemode_toggle(wmOperatorType *ot) 
{
	/* identifiers */
	ot->name= "Toggle Pose Mode";
	ot->idname= "OBJECT_OT_posemode_toggle";
	ot->description= "Enables or disables posing/selecting bones";
	
	/* api callbacks */
	ot->exec= posemode_exec;
	ot->poll= ED_operator_object_active;
	
	/* flag */
	ot->flag= OPTYPE_REGISTER;
}

/* *********************** */

void check_editmode(int type)
{
	Object *obedit= NULL; // XXX
	
	if (obedit==NULL || obedit->type==type) return;

// XXX	ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO); /* freedata, and undo */
}

#if 0
// XXX should be in view3d?

/* context: ob = lamp */
/* code should be replaced with proper (custom) transform handles for lamp properties */
static void spot_interactive(Object *ob, int mode)
{
	Lamp *la= ob->data;
	float transfac, dx, dy, ratio, origval;
	int keep_running= 1, center2d[2];
	short mval[2], mvalo[2];
	
//	getmouseco_areawin(mval);
//	getmouseco_areawin(mvalo);
	
	project_int(ob->obmat[3], center2d);
	if( center2d[0] > 100000 ) {		/* behind camera */
//		center2d[0]= curarea->winx/2;
//		center2d[1]= curarea->winy/2;
	}

//	helpline(mval, center2d);
	
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
		
//		getmouseco_areawin(mval);
		
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
// XXX			shade_buttons_change_3d();

			/* DRAW */	
			headerprint(str);
			force_draw_plus(SPACE_BUTS, 0);

//			helpline(mval, center2d);
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

}
#endif

void special_editmenu(Scene *scene, View3D *v3d)
{
// XXX	static short numcuts= 2;
	Object *ob= OBACT;
	Object *obedit= NULL; // XXX
	int nr,ret=0;
	
	if(ob==NULL) return;
	
	if(obedit==NULL) {
		
		if(ob->mode & OB_MODE_POSE) {
// XXX			pose_special_editmenu();
		}
		else if(paint_facesel_test(ob)) {
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
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
		else if(ob->mode & OB_MODE_VERTEX_PAINT) {
			Mesh *me= get_mesh(ob);
			
			if(me==0 || (me->mcol==NULL && me->mtface==NULL) ) return;
			
			nr= pupmenu("Specials%t|Shared VertexCol%x1");
			if(nr==1) {
				
// XXX				do_shared_vertexcol(me);
				
				DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			}
		}
		else if(ob->mode & OB_MODE_WEIGHT_PAINT) {
			Object *par= modifiers_isDeformedByArmature(ob);

			if(par && (par->mode & OB_MODE_POSE)) {
				nr= pupmenu("Specials%t|Apply Bone Envelopes to Vertex Groups %x1|Apply Bone Heat Weights to Vertex Groups %x2");

// XXX				if(nr==1 || nr==2)
// XXX					pose_adds_vgroups(ob, (nr == 2));
			}
		}
		else if(ob->mode & OB_MODE_PARTICLE_EDIT) {
#if 0
			// XXX
			ParticleSystem *psys = PE_get_current(ob);
			ParticleEditSettings *pset = PE_settings();

			if(!psys)
				return;

			if(pset->selectmode & SCE_SELECT_POINT)
				nr= pupmenu("Specials%t|Rekey%x1|Subdivide%x2|Select First%x3|Select Last%x4|Remove Doubles%x5");
			else
				nr= pupmenu("Specials%t|Rekey%x1|Remove Doubles%x5");
			
			switch(nr) {
			case 1:
// XXX				if(button(&pset->totrekey, 2, 100, "Number of Keys:")==0) return;
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
			
			DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);
			
			if(nr>0) waitcursor(0);
#endif
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
						if(TESTBASELIB(v3d, base)) {
							if(base->object != ob) base_select= base;
						}
					}

					if (base_select) {
						if (get_mesh(base_select->object)) {
							if(nr <= 3){
								waitcursor(1);
// XXX								ret = NewBooleanMesh(BASACT,base_select,nr);
								if (ret==0) {
									error("An internal error occurred");
								} else if(ret==-1) {
									error("Selected meshes must have faces to perform boolean operations");
								} else if (ret==-2) {
									error("Both meshes must be a closed mesh");
								}
								waitcursor(0);
							} else {
								BooleanModifierData *bmd = NULL;
								bmd = (BooleanModifierData *)modifier_new(eModifierType_Boolean);
								BLI_addtail(&ob->modifiers, bmd);
								modifier_unique_name(&ob->modifiers, (ModifierData*)bmd);
								bmd->object = base_select->object;
								bmd->modifier.mode |= eModifierMode_Realtime;
								switch(nr){
									case 4: bmd->operation = eBooleanModifierOp_Intersect; break;
									case 5: bmd->operation = eBooleanModifierOp_Union; break;
									case 6:	bmd->operation = eBooleanModifierOp_Difference; break;
								}
// XXX								do_common_editbuts(B_CHANGEDEP);
							}								
						} else {
							error("Please select 2 meshes");
						}
					} else {
						error("Please select 2 meshes");
					}
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
	else if(obedit->type==OB_MESH) {
	}
	else if(ELEM(obedit->type, OB_CURVE, OB_SURF)) {
	}
	else if(obedit->type==OB_ARMATURE) {
		nr= pupmenu("Specials%t|Subdivide %x1|Subdivide Multi%x2|Switch Direction%x7|Flip Left-Right Names%x3|%l|AutoName Left-Right%x4|AutoName Front-Back%x5|AutoName Top-Bottom%x6");
//		if(nr==1)
// XXX			subdivide_armature(1);
		if(nr==2) {
// XXX			if(button(&numcuts, 1, 128, "Number of Cuts:")==0) return;
			waitcursor(1);
// XXX			subdivide_armature(numcuts);
		}
//		else if(nr==3)
// XXX			armature_flip_names();
		else if(ELEM3(nr, 4, 5, 6)) {
// XXX			armature_autoside_names(nr-4);
		}
//		else if(nr == 7)
// XXX			switch_direction_armature();
	}
	else if(obedit->type==OB_LATTICE) {
		Lattice *lt= obedit->data;
		static float weight= 1.0f;
		{ // XXX
// XXX		if(fbutton(&weight, 0.0f, 1.0f, 10, 10, "Set Weight")) {
			int a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
			BPoint *bp= lt->editlatt->def;
			
			while(a--) {
				if(bp->f1 & SELECT)
					bp->weight= weight;
				bp++;
			}	
		}
	}

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

static void object_flip_subdivison_particles(Scene *scene, Object *ob, int *set, int level, int mode, int particles, int depth)
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
				modifier_unique_name(&ob->modifiers, (ModifierData*)smd);
				
				if (level!=-1) {
					smd->levels = level;
				}
				
				if(*set == -1)
					*set= 1;
			}
		}

		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}

	if(ob->dup_group && depth<=4) {
		GroupObject *go;

		for(go= ob->dup_group->gobject.first; go; go= go->next)
			object_flip_subdivison_particles(scene, go->ob, set, level, mode, particles, depth+1);
	}
}

/* Change subdivision properties of mesh object ob, if
* level==-1 then toggle subsurf, else set to level.
*/

void flip_subdivison(Scene *scene, View3D *v3d, int level)
{
	Base *base;
	int set= -1;
	int mode, pupmode, particles= 0, havesubdiv= 0, havepart= 0;
	int alt= 0; // XXX
	
	if(alt)
		mode= eModifierMode_Realtime;
	else
		mode= eModifierMode_Render|eModifierMode_Realtime;
	
	if(level == -1) {
		if (scene->obedit) { // XXX get from context
			object_has_subdivision_particles(scene->obedit, &havesubdiv, &havepart, 0);			
		} else {
			for(base= scene->base.first; base; base= base->next) {
				if(((level==-1) && (TESTBASE(v3d, base))) || (TESTBASELIB(v3d, base))) {
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

	if (scene->obedit) {	 // XXX get from context
		object_flip_subdivison_particles(scene, scene->obedit, &set, level, mode, particles, 0);
	} else {
		for(base= scene->base.first; base; base= base->next) {
			if(((level==-1) && (TESTBASE(v3d, base))) || (TESTBASELIB(v3d, base))) {
				object_flip_subdivison_particles(scene, base->object, &set, level, mode, particles, 0);
			}
		}
	}
	
	DAG_ids_flush_update(0);
}
 
static void copymenu_properties(Scene *scene, View3D *v3d, Object *ob)
{	
	bProperty *prop;
	Base *base;
	int nr, tot=0;
	char *str;
	
	prop= ob->prop.first;
	while(prop) {
		tot++;
		prop= prop->next;
	}
	
	str= MEM_callocN(50 + 33*tot, "copymenu prop");
	
	if (tot)
		strcpy(str, "Copy Property %t|Replace All|Merge All|%l");
	else
		strcpy(str, "Copy Property %t|Clear All (no properties on active)");
	
	tot= 0;	
	prop= ob->prop.first;
	while(prop) {
		tot++;
		strcat(str, "|");
		strcat(str, prop->name);
		prop= prop->next;
	}

	nr= pupmenu(str);
	
	if ( nr==1 || nr==2 ) {
		for(base= FIRSTBASE; base; base= base->next) {
			if((base != BASACT) &&(TESTBASELIB(v3d, base))) {
				if (nr==1) { /* replace */
					copy_properties( &base->object->prop, &ob->prop );
				} else {
					for(prop = ob->prop.first; prop; prop= prop->next ) {
						set_ob_property(base->object, prop);
					}
				}
			}
		}
	} else if(nr>0) {
		prop = BLI_findlink(&ob->prop, nr-4); /* account for first 3 menu items & menu index starting at 1*/
		
		if(prop) {
			for(base= FIRSTBASE; base; base= base->next) {
				if((base != BASACT) &&(TESTBASELIB(v3d, base))) {
					set_ob_property(base->object, prop);
				}
			}
		}
	}
	MEM_freeN(str);
	
}

static void copymenu_logicbricks(Scene *scene, View3D *v3d, Object *ob)
{
	Base *base;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(base->object != ob) {
			if(TESTBASELIB(v3d, base)) {
				
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
				
				/* set the initial state */
				base->object->state= ob->state;
				base->object->init_state= ob->init_state;
			}
		}
	}
}

static void copymenu_modifiers(Scene *scene, View3D *v3d, Object *ob)
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
		
		if(i == eModifierType_Collision)
			continue;

		if (	(mti->flags&eModifierTypeFlag_AcceptsCVs) || 
				(ob->type==OB_MESH && (mti->flags&eModifierTypeFlag_AcceptsMesh))) {
			sprintf(str+strlen(str), "|%s%%x%d", mti->name, i);
		}
	}

	event = pupmenu(str);
	if(event<=0) return;

	for (base= FIRSTBASE; base; base= base->next) {
		if(base->object != ob) {
			if(TESTBASELIB(v3d, base)) {
				ModifierData *md;

				base->object->recalc |= OB_RECALC_OB|OB_RECALC_DATA;

				if (base->object->type==ob->type) {
					/* copy all */
					if (event==NUM_MODIFIER_TYPES) {
						object_free_modifiers(base->object);

						for (md=ob->modifiers.first; md; md=md->next) {
							ModifierData *nmd = NULL;
							
							if(ELEM3(md->type, eModifierType_Hook, eModifierType_Softbody, eModifierType_ParticleInstance)) continue;
		
							if(md->type == eModifierType_Collision)
								continue;
							
							nmd = modifier_new(md->type);
							modifier_copyData(md, nmd);
							BLI_addtail(&base->object->modifiers, nmd);
							modifier_unique_name(&base->object->modifiers, nmd);
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
								modifier_unique_name(&base->object->modifiers, mdn);

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
	
//	if(errorstr) notice(errorstr);
	
	DAG_scene_sort(scene);
	
}

/* both pointers should exist */
static void copy_texture_space(Object *to, Object *ob)
{
	float *poin1= NULL, *poin2= NULL;
	short texflag= 0;
	
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

void copy_attr(Scene *scene, View3D *v3d, short event)
{
	Object *ob;
	Base *base;
	Curve *cu, *cu1;
	Nurb *nu;
	int do_scene_sort= 0;
	
	if(scene->id.lib) return;

	if(!(ob=OBACT)) return;
	
	if(scene->obedit) { // XXX get from context
		/* obedit_copymenu(); */
		return;
	}
	if(event==9) {
		copymenu_properties(scene, v3d, ob);
		return;
	}
	else if(event==10) {
		copymenu_logicbricks(scene, v3d, ob);
		return;
	}
	else if(event==24) {
		copymenu_modifiers(scene, v3d, ob);
		return;
	}

	for(base= FIRSTBASE; base; base= base->next) {
		if(base != BASACT) {
			if(TESTBASELIB(v3d, base)) {
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
				else if(event==11) {	/* all physical attributes */
					base->object->gameflag = ob->gameflag;
					base->object->inertia = ob->inertia;
					base->object->formfactor = ob->formfactor;
					base->object->damping= ob->damping;
					base->object->rdamping= ob->rdamping;
					base->object->min_vel= ob->min_vel;
					base->object->max_vel= ob->max_vel;
					if (ob->gameflag & OB_BOUNDS) {
						base->object->boundtype = ob->boundtype;
					}
					base->object->margin= ob->margin;
					base->object->bsoft= copy_bulletsoftbody(ob->bsoft);

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

						BKE_text_to_curve(scene, base->object, 0);		/* needed? */

						
						strcpy(cu1->family, cu->family);
						
						base->object->recalc |= OB_RECALC_DATA;
					}
				}
				else if(event==19) {	/* bevel settings */
					
					if(ELEM(base->object->type, OB_CURVE, OB_FONT)) {
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

					if(ELEM(base->object->type, OB_CURVE, OB_FONT)) {
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
					/* Copy the constraint channels over */
					copy_constraints(&base->object->constraints, &ob->constraints);
					
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
#if 0 // XXX old animation system
					copy_nlastrips(&base->object->nlastrips, &ob->nlastrips);
#endif // XXX old animation system
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
					if(ELEM(base->object->type, OB_CURVE, OB_SURF)) {
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
				else if(event==31) { /* object color */
					QUATCOPY(base->object->col, ob->col);
				}
			}
		}
	}
	
	if(do_scene_sort)
		DAG_scene_sort(scene);

	DAG_ids_flush_update(0);
}

void copy_attr_menu(Scene *scene, View3D *v3d)
{
	Object *ob;
	short event;
	char str[512];
	
	if(!(ob=OBACT)) return;
	
	if (scene->obedit) { // XXX get from context
//		if (ob->type == OB_MESH)
// XXX			mesh_copy_menu();
		return;
	}
	
	/* Object Mode */
	
	/* If you change this menu, don't forget to update the menu in header_view3d.c
	 * view3d_edit_object_copyattrmenu() and in toolbox.c
	 */
	
	strcpy(str, "Copy Attributes %t|Location%x1|Rotation%x2|Size%x3|Draw Options%x4|Time Offset%x5|Dupli%x6|Object Color%x31|%l|Mass%x7|Damping%x8|All Physical Attributes%x11|Properties%x9|Logic Bricks%x10|Protected Transform%x29|%l");
	
	strcat (str, "|Object Constraints%x22");
	strcat (str, "|NLA Strips%x26");
	
// XXX	if (OB_SUPPORT_MATERIAL(ob)) {
//		strcat(str, "|Texture Space%x17");
//	}	
	
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
	
	copy_attr(scene, v3d, event);
}

/********************** Smooth/Flat *********************/

static int shade_smooth_exec(bContext *C, wmOperator *op)
{
	Curve *cu;
	Nurb *nu;
	int clear= (strcmp(op->idname, "OBJECT_OT_shade_flat") == 0);
	int done= 0;

	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {

		if(ob->type==OB_MESH) {
			mesh_set_smooth_flag(ob, !clear);

			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

			done= 1;
		}
		else if ELEM(ob->type, OB_SURF, OB_CURVE) {
			cu= ob->data;

			for(nu=cu->nurb.first; nu; nu=nu->next) {
				if(!clear) nu->flag |= ME_SMOOTH;
				else nu->flag &= ~ME_SMOOTH;
			}

			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);

			done= 1;
		}
	}
	CTX_DATA_END;

	return (done)? OPERATOR_FINISHED: OPERATOR_CANCELLED;
}

void OBJECT_OT_shade_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Shade Flat";
	ot->idname= "OBJECT_OT_shade_flat";
	
	/* api callbacks */
	ot->exec= shade_smooth_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void OBJECT_OT_shade_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Shade Smooth";
	ot->idname= "OBJECT_OT_shade_smooth";
	
	/* api callbacks */
	ot->exec= shade_smooth_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



/* ********************** */

void image_aspect(Scene *scene, View3D *v3d)
{
	/* all selected objects with an image map: scale in image aspect */
	Base *base;
	Object *ob;
	Material *ma;
	Tex *tex;
	float x, y, space;
	int a, b, done;
	
	if(scene->obedit) return; // XXX get from context
	if(scene->id.lib) return;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
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
								else if(ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
									Curve *cu= ob->data;
									space= cu->size[0]/cu->size[1];
								}
							
								x= ibuf->x/space;
								y= ibuf->y;
								
								if(x>y) ob->size[0]= ob->size[1]*x/y;
								else ob->size[1]= ob->size[0]*y/x;
								
								done= 1;
								DAG_id_flush_update(&ob->id, OB_RECALC_OB);								
							}
						}
						if(done) break;
					}
				}
				if(done) break;
			}
		}
	}
	
}

void set_ob_ipoflags(Scene *scene, View3D *v3d)
{
#if 0 // XXX old animation system
	Base *base;
	int set= 1;
	
	if (!v3d) {
		error("Can't do this! Open a 3D window");
		return;
	}
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			if(base->object->ipoflag & OB_DRAWKEY) {
				set= 0;
				break;
			}
		}
	}
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			if(set) {
				base->object->ipoflag |= OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 1;
			}
			else {
				base->object->ipoflag &= ~OB_DRAWKEY;
				if(base->object->ipo) base->object->ipo->showkey= 0;
			}
		}
	}
#endif // XXX old animation system
}


void select_select_keys(Scene *scene, View3D *v3d)
{
#if 0 // XXX old animation system
	Base *base;
	IpoCurve *icu;
	BezTriple *bezt;
	int a;
	
	if (!v3d) {
		error("Can't do this! Open a 3D window");
		return;
	}
	
	if(scene->id.lib) return;

	if(okee("Show and select all keys")==0) return;

	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
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
	}


#endif  // XXX old animation system
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


void auto_timeoffs(Scene *scene, View3D *v3d)
{
	Base *base, **basesort, **bs;
	float start, delta;
	int tot=0, a;
	short offset=25;

	if(BASACT==0 || v3d==NULL) return;
// XXX	if(button(&offset, 0, 1000,"Total time")==0) return;

	/* make array of all bases, xco yco (screen) */
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			tot++;
		}
	}

	delta= (float)offset/(float)tot;
	start= OBACT->sf;

	bs= basesort= MEM_mallocN(sizeof(void *)*tot,"autotimeoffs");
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			*bs= base;
			bs++;
		}
	}
	qsort(basesort, tot, sizeof(void *), vergbaseco);

	bs= basesort;
	for(a=0; a<tot; a++) {
		
		(*bs)->object->sf= start;
		start+= delta;

		bs++;
	}
	MEM_freeN(basesort);

}

void ofs_timeoffs(Scene *scene, View3D *v3d)
{
	float offset=0.0f;

	if(BASACT==0 || v3d==NULL) return;
	
// XXX	if(fbutton(&offset, -10000.0f, 10000.0f, 10, 10, "Offset")==0) return;

	/* make array of all bases, xco yco (screen) */
	CTX_DATA_BEGIN(C, Object*, ob, selected_editable_objects) {
		ob->sf += offset;
		if (ob->sf < -MAXFRAMEF)		ob->sf = -MAXFRAMEF;
		else if (ob->sf > MAXFRAMEF)	ob->sf = MAXFRAMEF;
	}
	CTX_DATA_END;

}


void rand_timeoffs(Scene *scene, View3D *v3d)
{
	Base *base;
	float rand=0.0f;

	if(BASACT==0 || v3d==NULL) return;
	
// XXX	if(fbutton(&rand, 0.0f, 10000.0f, 10, 10, "Randomize")==0) return;
	
	rand *= 2;
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(TESTBASELIB(v3d, base)) {
			base->object->sf += (BLI_drand()-0.5) * rand;
			if (base->object->sf < -MAXFRAMEF)		base->object->sf = -MAXFRAMEF;
			else if (base->object->sf > MAXFRAMEF)	base->object->sf = MAXFRAMEF;
		}
	}

}

static EnumPropertyItem *object_mode_set_itemsf(bContext *C, PointerRNA *ptr, int *free)
{	
	EnumPropertyItem *input = object_mode_items;
	EnumPropertyItem *item= NULL;
	Object *ob;
	int totitem= 0;
	
	if(!C) /* needed for docs */
		return object_mode_items;

	ob = CTX_data_active_object(C);
	while(ob && input->identifier) {
		if((input->value == OB_MODE_EDIT && ((ob->type == OB_MESH) || (ob->type == OB_ARMATURE) ||
						    (ob->type == OB_CURVE) || (ob->type == OB_SURF) ||
						     (ob->type == OB_FONT) || (ob->type == OB_MBALL) || (ob->type == OB_LATTICE))) ||
		   (input->value == OB_MODE_POSE && (ob->type == OB_ARMATURE)) ||
		   (input->value == OB_MODE_PARTICLE_EDIT && ob->particlesystem.first) ||
		   ((input->value == OB_MODE_SCULPT || input->value == OB_MODE_VERTEX_PAINT ||
		     input->value == OB_MODE_WEIGHT_PAINT || input->value == OB_MODE_TEXTURE_PAINT) && (ob->type == OB_MESH)) ||
		   (input->value == OB_MODE_OBJECT))
			RNA_enum_item_add(&item, &totitem, input);
		++input;
	}

	RNA_enum_item_end(&item, &totitem);

	*free= 1;

	return item;
}

static const char *object_mode_op_string(int mode)
{
	if(mode & OB_MODE_EDIT)
		return "OBJECT_OT_editmode_toggle";
	if(mode == OB_MODE_SCULPT)
		return "SCULPT_OT_sculptmode_toggle";
	if(mode == OB_MODE_VERTEX_PAINT)
		return "PAINT_OT_vertex_paint_toggle";
	if(mode == OB_MODE_WEIGHT_PAINT)
		return "PAINT_OT_weight_paint_toggle";
	if(mode == OB_MODE_TEXTURE_PAINT)
		return "PAINT_OT_texture_paint_toggle";
	if(mode == OB_MODE_PARTICLE_EDIT)
		return "PARTICLE_OT_particle_edit_toggle";
	if(mode == OB_MODE_POSE)
		return "OBJECT_OT_posemode_toggle";
	return NULL;
}

/* checks the mode to be set is compatible with the object
 * should be made into a generic function */
static int object_mode_set_compat(bContext *C, wmOperator *op, Object *ob)
{
	ObjectMode mode = RNA_enum_get(op->ptr, "mode");

	if(ob) {
		if(mode == OB_MODE_OBJECT)
			return 1;

		switch(ob->type) {
		case OB_MESH:
			if(mode & (OB_MODE_EDIT|OB_MODE_SCULPT|OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT|OB_MODE_PARTICLE_EDIT))
				return 1;
			return 0;
		case OB_CURVE:
		case OB_SURF:
		case OB_FONT:
		case OB_MBALL:
			if(mode & (OB_MODE_EDIT))
				return 1;
			return 0;
		case OB_LATTICE:
			if(mode & (OB_MODE_EDIT|OB_MODE_WEIGHT_PAINT))
				return 1;
		case OB_ARMATURE:
			if(mode & (OB_MODE_EDIT|OB_MODE_POSE))
				return 1;
		}
	}

	return 0;
}

static int object_mode_set_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	ObjectMode mode = RNA_enum_get(op->ptr, "mode");
	ObjectMode restore_mode = ob->mode;
	int toggle = RNA_boolean_get(op->ptr, "toggle");

	if(!ob || !object_mode_set_compat(C, op, ob))
		return OPERATOR_PASS_THROUGH;

	/* Exit current mode if it's not the mode we're setting */
	if(ob->mode != OB_MODE_OBJECT && ob->mode != mode)
		WM_operator_name_call(C, object_mode_op_string(ob->mode), WM_OP_EXEC_REGION_WIN, NULL);

	if(mode != OB_MODE_OBJECT) {
		/* Enter new mode */
		if(ob->mode != mode || toggle)
			WM_operator_name_call(C, object_mode_op_string(mode), WM_OP_EXEC_REGION_WIN, NULL);

		if(toggle) {
			if(ob->mode == mode)
				/* For toggling, store old mode so we know what to go back to */
				ob->restore_mode = restore_mode;
			else if(ob->restore_mode != OB_MODE_OBJECT && ob->restore_mode != mode) {
				WM_operator_name_call(C, object_mode_op_string(ob->restore_mode), WM_OP_EXEC_REGION_WIN, NULL);
			}
		}
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_mode_set(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Set Object Mode";
	ot->description = "Sets the object interaction mode.";
	ot->idname= "OBJECT_OT_mode_set";
	
	/* api callbacks */
	ot->exec= object_mode_set_exec;
	
	ot->poll= ED_operator_object_active;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	prop= RNA_def_enum(ot->srna, "mode", object_mode_items, OB_MODE_OBJECT, "Mode", "");
	RNA_def_enum_funcs(prop, object_mode_set_itemsf);

	RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "");
}



void ED_object_toggle_modes(bContext *C, int mode)
{
	if(mode & OB_MODE_SCULPT)
		WM_operator_name_call(C, "SCULPT_OT_sculptmode_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_VERTEX_PAINT)
		WM_operator_name_call(C, "PAINT_OT_vertex_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_WEIGHT_PAINT)
		WM_operator_name_call(C, "PAINT_OT_weight_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_TEXTURE_PAINT)
		WM_operator_name_call(C, "PAINT_OT_texture_paint_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_PARTICLE_EDIT)
		WM_operator_name_call(C, "PARTICLE_OT_particle_edit_toggle", WM_OP_EXEC_REGION_WIN, NULL);
	if(mode & OB_MODE_POSE)
		WM_operator_name_call(C, "OBJECT_OT_posemode_toggle", WM_OP_EXEC_REGION_WIN, NULL);
}

/************************ Game Properties ***********************/

static int game_property_new(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bProperty *prop;

	if(!ob)
		return OPERATOR_CANCELLED;

	prop= new_property(PROP_FLOAT);
	BLI_addtail(&ob->prop, prop);
	unique_property(NULL, prop, 0); // make_unique_prop_names(prop->name);

	return OPERATOR_FINISHED;
}


void OBJECT_OT_game_property_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Game Property";
	ot->idname= "OBJECT_OT_game_property_new";

	/* api callbacks */
	ot->exec= game_property_new;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int game_property_remove(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bProperty *prop;
	int index;

	if(!ob)
		return OPERATOR_CANCELLED;

	index = RNA_int_get(op->ptr, "index");

    prop= BLI_findlink(&ob->prop, index);

    if(prop) {
		BLI_remlink(&ob->prop, prop);
		free_property(prop);
		return OPERATOR_FINISHED;
    }
    else {
    	return OPERATOR_CANCELLED;
    }
}

void OBJECT_OT_game_property_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Game Property";
	ot->idname= "OBJECT_OT_game_property_remove";

	/* api callbacks */
	ot->exec= game_property_remove;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Property index to remove ", 0, INT_MAX);
}
