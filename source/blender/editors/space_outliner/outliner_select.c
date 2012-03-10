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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_select.c
 *  \ingroup spoutliner
 */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_camera_types.h"
#include "DNA_group_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_sequence_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#if defined WIN32 && !defined _LIBC
# include "BLI_fnmatch.h" /* use fnmatch included in blenlib */
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
# include <fnmatch.h>
#endif


#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "outliner_intern.h"

/* ****************************************************** */
/* Outliner Selection (grey-blue highlight for rows) */

static int outliner_select(SpaceOops *soops, ListBase *lb, int *index, short *selecting)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int change= 0;
	
	for (te= lb->first; te && *index >= 0; te=te->next, (*index)--) {
		tselem= TREESTORE(te);
		
		/* if we've encountered the right item, set its 'Outliner' selection status */
		if (*index == 0) {
			/* this should be the last one, so no need to do anything with index */
			if ((te->flag & TE_ICONROW)==0) {
				/* -1 value means toggle testing for now... */
				if (*selecting == -1) {
					if (tselem->flag & TSE_SELECTED) 
						*selecting= 0;
					else 
						*selecting= 1;
				}
				
				/* set selection */
				if (*selecting) 
					tselem->flag |= TSE_SELECTED;
				else 
					tselem->flag &= ~TSE_SELECTED;

				change |= 1;
			}
		}
		else if (TSELEM_OPEN(tselem,soops)) {
			/* Only try selecting sub-elements if we haven't hit the right element yet
			 *
			 * Hack warning:
			 * 	Index must be reduced before supplying it to the sub-tree to try to do
			 * 	selection, however, we need to increment it again for the next loop to 
			 * 	function correctly
			 */
			(*index)--;
			change |= outliner_select(soops, &te->subtree, index, selecting);
			(*index)++;
		}
	}

	return change;
}

/* ****************************************************** */
/* Outliner Element Selection/Activation on Click */

static int tree_element_active_renderlayer(bContext *C, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Scene *sce;
	
	/* paranoia check */
	if(te->idcode!=ID_SCE)
		return 0;
	sce= (Scene *)tselem->id;
	
	if(set) {
		sce->r.actlay= tselem->nr;
		WM_event_add_notifier(C, NC_SCENE|ND_RENDER_OPTIONS, sce);
	}
	else {
		return sce->r.actlay==tselem->nr;
	}
	return 0;
}

static int  tree_element_set_active_object(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeStoreElem *tselem= TREESTORE(te);
	Scene *sce;
	Base *base;
	Object *ob= NULL;
	
	/* if id is not object, we search back */
	if(te->idcode==ID_OB) ob= (Object *)tselem->id;
	else {
		ob= (Object *)outliner_search_back(soops, te, ID_OB);
		if(ob==OBACT) return 0;
	}
	if(ob==NULL) return 0;
	
	sce= (Scene *)outliner_search_back(soops, te, ID_SCE);
	if(sce && scene != sce) {
		ED_screen_set_scene(C, CTX_wm_screen(C), sce);
	}
	
	/* find associated base in current scene */
	base= object_in_scene(ob, scene);

	if(base) {
		if(set==2) {
			/* swap select */
			if(base->flag & SELECT)
				ED_base_object_select(base, BA_DESELECT);
			else 
				ED_base_object_select(base, BA_SELECT);
		}
		else {
			/* deleselect all */
			scene_deselect_all(scene);
			ED_base_object_select(base, BA_SELECT);
		}
		if(C) {
			ED_base_object_activate(C, base); /* adds notifier */
			WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
		}
	}
	
	if(ob!=scene->obedit) 
		ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
		
	return 1;
}

static int tree_element_active_material(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tes;
	Object *ob;
	
	/* we search for the object parent */
	ob= (Object *)outliner_search_back(soops, te, ID_OB);
	// note: ob->matbits can be NULL when a local object points to a library mesh.
	if(ob==NULL || ob!=OBACT || ob->matbits==NULL) return 0;	// just paranoia
	
	/* searching in ob mat array? */
	tes= te->parent;
	if(tes->idcode==ID_OB) {
		if(set) {
			ob->actcol= te->index+1;
			ob->matbits[te->index]= 1;	// make ob material active too
		}
		else {
			if(ob->actcol == te->index+1) 
				if(ob->matbits[te->index]) return 1;
		}
	}
	/* or we search for obdata material */
	else {
		if(set) {
			ob->actcol= te->index+1;
			ob->matbits[te->index]= 0;	// make obdata material active too
		}
		else {
			if(ob->actcol == te->index+1)
				if(ob->matbits[te->index]==0) return 1;
		}
	}
	if(set) {
		WM_event_add_notifier(C, NC_MATERIAL|ND_SHADING, NULL);
	}
	return 0;
}

static int tree_element_active_texture(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tep;
	TreeStoreElem /* *tselem,*/ *tselemp;
	Object *ob=OBACT;
	SpaceButs *sbuts=NULL;
	
	if(ob==NULL) return 0; // no active object
	
	/*tselem= TREESTORE(te);*/ /*UNUSED*/
	
	/* find buttons area (note, this is undefined really still, needs recode in blender) */
	/* XXX removed finding sbuts */
	
	/* where is texture linked to? */
	tep= te->parent;
	tselemp= TREESTORE(tep);
	
	if(tep->idcode==ID_WO) {
		World *wrld= (World *)tselemp->id;

		if(set) {
			if(sbuts) {
				// XXX sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				// XXX sbuts->texfrom= 1;
			}
// XXX			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			wrld->texact= te->index;
		}
		else if(tselemp->id == (ID *)(scene->world)) {
			if(wrld->texact==te->index) return 1;
		}
	}
	else if(tep->idcode==ID_LA) {
		Lamp *la= (Lamp *)tselemp->id;
		if(set) {
			if(sbuts) {
				// XXX sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				// XXX sbuts->texfrom= 2;
			}
// XXX			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			la->texact= te->index;
		}
		else {
			if(tselemp->id == ob->data) {
				if(la->texact==te->index) return 1;
			}
		}
	}
	else if(tep->idcode==ID_MA) {
		Material *ma= (Material *)tselemp->id;
		if(set) {
			if(sbuts) {
				//sbuts->tabo= TAB_SHADING_TEX;	// hack from header_buttonswin.c
				// XXX sbuts->texfrom= 0;
			}
// XXX			extern_set_butspace(F6KEY, 0);	// force shading buttons texture
			ma->texact= (char)te->index;
			
			/* also set active material */
			ob->actcol= tep->index+1;
		}
		else if(tep->flag & TE_ACTIVE) {	// this is active material
			if(ma->texact==te->index) return 1;
		}
	}
	
	if(set)
		WM_event_add_notifier(C, NC_TEXTURE, NULL);

	return 0;
}


static int tree_element_active_lamp(bContext *UNUSED(C), Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	Object *ob;
	
	/* we search for the object parent */
	ob= (Object *)outliner_search_back(soops, te, ID_OB);
	if(ob==NULL || ob!=OBACT) return 0;	// just paranoia
	
	if(set) {
// XXX		extern_set_butspace(F5KEY, 0);
	}
	else return 1;
	
	return 0;
}

static int tree_element_active_camera(bContext *UNUSED(C), Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	Object *ob= (Object *)outliner_search_back(soops, te, ID_OB);

	if(set)
		return 0;

	return scene->camera == ob;
}

static int tree_element_active_world(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{
	TreeElement *tep;
	TreeStoreElem *tselem=NULL;
	Scene *sce=NULL;
	
	tep= te->parent;
	if(tep) {
		tselem= TREESTORE(tep);
		sce= (Scene *)tselem->id;
	}
	
	if(set) {	// make new scene active
		if(sce && scene != sce) {
			ED_screen_set_scene(C, CTX_wm_screen(C), sce);
		}
	}
	
	if(tep==NULL || tselem->id == (ID *)scene) {
		if(set) {
// XXX			extern_set_butspace(F8KEY, 0);
		}
		else {
			return 1;
		}
	}
	return 0;
}

static int tree_element_active_defgroup(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob;
	
	/* id in tselem is object */
	ob= (Object *)tselem->id;
	if(set) {
		BLI_assert(te->index+1 >= 0);
		ob->actdef= te->index+1;

		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	}
	else {
		if(ob==OBACT)
			if(ob->actdef== te->index+1) return 1;
	}
	return 0;
}

static int tree_element_active_posegroup(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	
	if(set) {
		if (ob->pose) {
			ob->pose->active_group= te->index+1;
			WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
		}
	}
	else {
		if(ob==OBACT && ob->pose) {
			if (ob->pose->active_group== te->index+1) return 1;
		}
	}
	return 0;
}

static int tree_element_active_posechannel(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	bArmature *arm= ob->data;
	bPoseChannel *pchan= te->directdata;
	
	if(set) {
		if(!(pchan->bone->flag & BONE_HIDDEN_P)) {
			
			if(set==2) ED_pose_deselectall(ob, 2);	// 2 = clear active tag
			else ED_pose_deselectall(ob, 0);	// 0 = deselect 
			
			if(set==2 && (pchan->bone->flag & BONE_SELECTED)) {
				pchan->bone->flag &= ~BONE_SELECTED;
			} else {
				pchan->bone->flag |= BONE_SELECTED;
				arm->act_bone= pchan->bone;
			}
			
			WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, ob);

		}
	}
	else {
		if(ob==OBACT && ob->pose) {
			if (pchan->bone->flag & BONE_SELECTED) return 1;
		}
	}
	return 0;
}

static int tree_element_active_bone(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *tselem, int set)
{
	bArmature *arm= (bArmature *)tselem->id;
	Bone *bone= te->directdata;
	
	if(set) {
		if(!(bone->flag & BONE_HIDDEN_P)) {
			if(set==2) ED_pose_deselectall(OBACT, 2);	// 2 is clear active tag
			else ED_pose_deselectall(OBACT, 0);
			
			if(set==2 && (bone->flag & BONE_SELECTED)) {
				bone->flag &= ~BONE_SELECTED;
			} else {
				bone->flag |= BONE_SELECTED;
				arm->act_bone= bone;
			}
			
			WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, OBACT);
		}
	}
	else {
		Object *ob= OBACT;
		
		if(ob && ob->data==arm) {
			if (bone->flag & BONE_SELECTED) return 1;
		}
	}
	return 0;
}


/* ebones only draw in editmode armature */
static void tree_element_active_ebone__sel(bContext *C, Scene *scene, bArmature *arm, EditBone *ebone, short sel)
{
	if(sel) {
		ebone->flag |= BONE_SELECTED|BONE_ROOTSEL|BONE_TIPSEL;
		arm->act_edbone= ebone;
		// flush to parent?
		if(ebone->parent && (ebone->flag & BONE_CONNECTED)) ebone->parent->flag |= BONE_TIPSEL;
	}
	else {
		ebone->flag &= ~(BONE_SELECTED|BONE_ROOTSEL|BONE_TIPSEL);
		// flush to parent?
		if(ebone->parent && (ebone->flag & BONE_CONNECTED)) ebone->parent->flag &= ~BONE_TIPSEL;
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_ACTIVE, scene->obedit);
}
static int tree_element_active_ebone(bContext *C, Scene *scene, TreeElement *te, TreeStoreElem *UNUSED(tselem), int set)
{
	bArmature *arm= scene->obedit->data;
	EditBone *ebone= te->directdata;

	if(set==1) {
		if(!(ebone->flag & BONE_HIDDEN_A)) {
			ED_armature_deselect_all(scene->obedit, 0);	// deselect
			tree_element_active_ebone__sel(C, scene, arm, ebone, TRUE);
			return 1;
		}
	}
	else if (set==2) {
		if(!(ebone->flag & BONE_HIDDEN_A)) {
			if(!(ebone->flag & BONE_SELECTED)) {
				tree_element_active_ebone__sel(C, scene, arm, ebone, TRUE);
				return 1;
			}
			else {
				/* entirely selected, so de-select */
				tree_element_active_ebone__sel(C, scene, arm, ebone, FALSE);
				return 0;
			}
		}
	}
	else if (ebone->flag & BONE_SELECTED) {
		return 1;
	}
	return 0;
}

static int tree_element_active_modifier(bContext *C, TreeElement *UNUSED(te), TreeStoreElem *tselem, int set)
{
	if(set) {
		Object *ob= (Object *)tselem->id;
		
		WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, ob);

// XXX		extern_set_butspace(F9KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_psys(bContext *C, Scene *UNUSED(scene), TreeElement *UNUSED(te), TreeStoreElem *tselem, int set)
{
	if(set) {
		Object *ob= (Object *)tselem->id;
		
		WM_event_add_notifier(C, NC_OBJECT|ND_PARTICLE|NA_EDITED, ob);
		
// XXX		extern_set_butspace(F7KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_constraint(bContext *C, TreeElement *UNUSED(te), TreeStoreElem *tselem, int set)
{
	if(set) {
		Object *ob= (Object *)tselem->id;
		
		WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
// XXX		extern_set_butspace(F7KEY, 0);
	}
	
	return 0;
}

static int tree_element_active_text(bContext *UNUSED(C), Scene *UNUSED(scene), SpaceOops *UNUSED(soops), TreeElement *UNUSED(te), int UNUSED(set))
{
	// XXX removed
	return 0;
}

static int tree_element_active_pose(bContext *C, Scene *scene, TreeElement *UNUSED(te), TreeStoreElem *tselem, int set)
{
	Object *ob= (Object *)tselem->id;
	Base *base= object_in_scene(ob, scene);
	
	if(set) {
		if(scene->obedit) 
			ED_object_exit_editmode(C, EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR|EM_DO_UNDO);
		
		if(ob->mode & OB_MODE_POSE) 
			ED_armature_exit_posemode(C, base);
		else 
			ED_armature_enter_posemode(C, base);
	}
	else {
		if(ob->mode & OB_MODE_POSE) return 1;
	}
	return 0;
}

static int tree_element_active_sequence(TreeElement *te, TreeStoreElem *UNUSED(tselem), int set)
{
	Sequence *seq= (Sequence*) te->directdata;

	if(set) {
// XXX		select_single_seq(seq, 1);
	}
	else {
		if(seq->flag & SELECT)
			return(1);
	}
	return(0);
}

static int tree_element_active_sequence_dup(Scene *scene, TreeElement *te, TreeStoreElem *UNUSED(tselem), int set)
{
	Sequence *seq, *p;
	Editing *ed= seq_give_editing(scene, FALSE);

	seq= (Sequence*)te->directdata;
	if(set==0) {
		if(seq->flag & SELECT)
			return(1);
		return(0);
	}

// XXX	select_single_seq(seq, 1);
	p= ed->seqbasep->first;
	while(p) {
		if((!p->strip) || (!p->strip->stripdata) || (!p->strip->stripdata->name)) {
			p= p->next;
			continue;
		}

//		if(!strcmp(p->strip->stripdata->name, seq->strip->stripdata->name))
// XXX			select_single_seq(p, 0);
		p= p->next;
	}
	return(0);
}

static int tree_element_active_keymap_item(bContext *UNUSED(C), TreeElement *te, TreeStoreElem *UNUSED(tselem), int set)
{
	wmKeyMapItem *kmi= te->directdata;
	
	if(set==0) {
		if(kmi->flag & KMI_INACTIVE) return 0;
		return 1;
	}
	else {
		kmi->flag ^= KMI_INACTIVE;
	}
	return 0;
}

/* ---------------------------------------------- */

/* generic call for ID data check or make/check active in UI */
int tree_element_active(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, int set)
{

	switch(te->idcode) {
		/* Note: no ID_OB: objects are handled specially to allow multiple
		 * selection. See do_outliner_item_activate. */
		case ID_MA:
			return tree_element_active_material(C, scene, soops, te, set);
		case ID_WO:
			return tree_element_active_world(C, scene, soops, te, set);
		case ID_LA:
			return tree_element_active_lamp(C, scene, soops, te, set);
		case ID_TE:
			return tree_element_active_texture(C, scene, soops, te, set);
		case ID_TXT:
			return tree_element_active_text(C, scene, soops, te, set);
		case ID_CA:
			return tree_element_active_camera(C, scene, soops, te, set);
	}
	return 0;
}

/* generic call for non-id data to make/check active in UI */
/* Context can be NULL when set==0 */
int tree_element_type_active(bContext *C, Scene *scene, SpaceOops *soops, TreeElement *te, TreeStoreElem *tselem, int set)
{
	switch(tselem->type) {
		case TSE_DEFGROUP:
			return tree_element_active_defgroup(C, scene, te, tselem, set);
		case TSE_BONE:
			return tree_element_active_bone(C, scene, te, tselem, set);
		case TSE_EBONE:
			return tree_element_active_ebone(C, scene, te, tselem, set);
		case TSE_MODIFIER:
			return tree_element_active_modifier(C, te, tselem, set);
		case TSE_LINKED_OB:
			if(set) tree_element_set_active_object(C, scene, soops, te, set);
			else if(tselem->id==(ID *)OBACT) return 1;
			break;
		case TSE_LINKED_PSYS:
			return tree_element_active_psys(C, scene, te, tselem, set);
		case TSE_POSE_BASE:
			return tree_element_active_pose(C, scene, te, tselem, set);
		case TSE_POSE_CHANNEL:
			return tree_element_active_posechannel(C, scene, te, tselem, set);
		case TSE_CONSTRAINT:
			return tree_element_active_constraint(C, te, tselem, set);
		case TSE_R_LAYER:
			return tree_element_active_renderlayer(C, te, tselem, set);
		case TSE_POSEGRP:
			return tree_element_active_posegroup(C, scene, te, tselem, set);
		case TSE_SEQUENCE:
			return tree_element_active_sequence(te, tselem, set);
		case TSE_SEQUENCE_DUP:
			return tree_element_active_sequence_dup(scene, te, tselem, set);
		case TSE_KEYMAP_ITEM:
			return tree_element_active_keymap_item(C, te, tselem, set);
			
	}
	return 0;
}

/* ================================================ */

static int do_outliner_item_activate(bContext *C, Scene *scene, ARegion *ar, SpaceOops *soops, TreeElement *te, int extend, const float mval[2])
{
	
	if(mval[1]>te->ys && mval[1]<te->ys+UI_UNIT_Y) {
		TreeStoreElem *tselem= TREESTORE(te);
		int openclose= 0;
		
		/* open close icon */
		if((te->flag & TE_ICONROW)==0) {				// hidden icon, no open/close
			if( mval[0]>te->xs && mval[0]<te->xs+UI_UNIT_X) 
				openclose= 1;
		}
		
		if(openclose) {
			/* all below close/open? */
			if(extend) {
				tselem->flag &= ~TSE_CLOSED;
				outliner_set_flag(soops, &te->subtree, TSE_CLOSED, !outliner_has_one_flag(soops, &te->subtree, TSE_CLOSED, 1));
			}
			else {
				if(tselem->flag & TSE_CLOSED) tselem->flag &= ~TSE_CLOSED;
				else tselem->flag |= TSE_CLOSED;
				
			}
			
			return 1;
		}
		/* name and first icon */
		else if(mval[0]>te->xs+UI_UNIT_X && mval[0]<te->xend) {
			
			/* always makes active object */
			if(tselem->type!=TSE_SEQUENCE && tselem->type!=TSE_SEQ_STRIP && tselem->type!=TSE_SEQUENCE_DUP)
				tree_element_set_active_object(C, scene, soops, te, 1 + (extend!=0 && tselem->type==0));
			
			if(tselem->type==0) { // the lib blocks
				/* editmode? */
				if(te->idcode==ID_SCE) {
					if(scene!=(Scene *)tselem->id) {
						ED_screen_set_scene(C, CTX_wm_screen(C), (Scene *)tselem->id);
					}
				}
				else if(te->idcode==ID_GR) {
					Group *gr= (Group *)tselem->id;
					GroupObject *gob;
					
					if(extend) {
						int sel= BA_SELECT;
						for(gob= gr->gobject.first; gob; gob= gob->next) {
							if(gob->ob->flag & SELECT) {
								sel= BA_DESELECT;
								break;
							}
						}
						
						for(gob= gr->gobject.first; gob; gob= gob->next) {
							ED_base_object_select(object_in_scene(gob->ob, scene), sel);
						}
					}
					else {
						scene_deselect_all(scene);
						
						for(gob= gr->gobject.first; gob; gob= gob->next) {
							if((gob->ob->flag & SELECT) == 0)
								ED_base_object_select(object_in_scene(gob->ob, scene), BA_SELECT);
						}
					}
					
					WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
				}
				else if(ELEM5(te->idcode, ID_ME, ID_CU, ID_MB, ID_LT, ID_AR)) {
					WM_operator_name_call(C, "OBJECT_OT_editmode_toggle", WM_OP_INVOKE_REGION_WIN, NULL);
				} else {	// rest of types
					tree_element_active(C, scene, soops, te, 1);
				}
				
			}
			else tree_element_type_active(C, scene, soops, te, tselem, 1+(extend!=0));
			
			return 1;
		}
	}
	
	for(te= te->subtree.first; te; te= te->next) {
		if(do_outliner_item_activate(C, scene, ar, soops, te, extend, mval)) return 1;
	}
	return 0;
}

/* event can enterkey, then it opens/closes */
static int outliner_item_activate(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar= CTX_wm_region(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	int extend= RNA_boolean_get(op->ptr, "extend");

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], fmval, fmval+1);

	if ( !ELEM3(soops->outlinevis, SO_DATABLOCKS, SO_USERDEF, SO_KEYMAP) &&
	     !(soops->flag & SO_HIDE_RESTRICTCOLS) &&
	     (fmval[0] > ar->v2d.cur.xmax - OL_TOG_RESTRICT_VIEWX))
	{
		return OPERATOR_CANCELLED;
	}

	for(te= soops->tree.first; te; te= te->next) {
		if(do_outliner_item_activate(C, scene, ar, soops, te, extend, fmval)) break;
	}
	
	if(te) {
		ED_undo_push(C, "Outliner click event");
	}
	else {
		short selecting= -1;
		int row;
		
		/* get row number - 100 here is just a dummy value since we don't need the column */
		UI_view2d_listview_view_to_cell(&ar->v2d, 1000, UI_UNIT_Y, 0.0f, OL_Y_OFFSET, 
						fmval[0], fmval[1], NULL, &row);
		
		/* select relevant row */
		if(outliner_select(soops, &soops->tree, &row, &selecting)) {
		
			soops->storeflag |= SO_TREESTORE_REDRAW;
		
			/* no need for undo push here, only changing outliner data which is
			 * scene level - campbell */
			/* ED_undo_push(C, "Outliner selection event"); */
		}
	}
	
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_activate(wmOperatorType *ot)
{
	ot->name= "Activate Item";
	ot->idname= "OUTLINER_OT_item_activate";
	ot->description= "Handle mouse clicks to activate/select items";
	
	ot->invoke= outliner_item_activate;
	
	ot->poll= ED_operator_outliner_active;
	
	RNA_def_boolean(ot->srna, "extend", 1, "Extend", "Extend selection for activation");
}

/* ****************************************************** */

/* **************** Border Select Tool ****************** */
static void outliner_item_border_select(Scene *scene, SpaceOops *soops, rctf *rectf, TreeElement *te, int gesture_mode)
{
	TreeStoreElem *tselem= TREESTORE(te);

	if (te->ys <= rectf->ymax && te->ys + UI_UNIT_Y >= rectf->ymin) {
		if (gesture_mode == GESTURE_MODAL_SELECT) {
			tselem->flag |= TSE_SELECTED;
		}
		else {
			tselem->flag &= ~TSE_SELECTED;
		}
	}

	/* Look at its children. */
	if ((tselem->flag & TSE_CLOSED) == 0) {
		for (te = te->subtree.first; te; te = te->next) {
			outliner_item_border_select(scene, soops, rectf, te, gesture_mode);
		}
	}
	return;
}

static int outliner_border_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	SpaceOops *soops= CTX_wm_space_outliner(C);
	ARegion *ar= CTX_wm_region(C);
	TreeElement *te;
	rcti rect;
	rctf rectf;
	int gesture_mode= RNA_int_get(op->ptr, "gesture_mode");

	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);

	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	for(te= soops->tree.first; te; te= te->next) {
		outliner_item_border_select(scene, soops, &rectf, te, gesture_mode);
	}

	WM_event_add_notifier(C, NC_SCENE|ND_OB_SELECT, scene);
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "OUTLINER_OT_select_border";
	ot->description= "Use box selection to select tree elements";

	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= outliner_border_select_exec;
	ot->modal= WM_border_select_modal;
	ot->cancel= WM_border_select_cancel;

	ot->poll= ED_operator_outliner_active;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* rna */
	WM_operator_properties_gesture_border(ot, FALSE);
}

/* ****************************************************** */
