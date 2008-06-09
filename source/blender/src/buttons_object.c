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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_cloth.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "BIF_butspace.h"
#include "BIF_editaction.h"
#include "BIF_editparticle.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_mywindow.h"
#include "BIF_poseobject.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_outliner.h"

#include "BDR_drawobject.h"
#include "BDR_editcurve.h"

#include "mydevice.h"
#include "blendef.h"

/* -----includes for this file specific----- */


#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_cloth_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_text_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_group.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"

#include "LBM_fluidsim.h"
#include "elbeem.h"

#include "BIF_editconstraint.h"
#include "BIF_editdeform.h"
#include "BIF_editparticle.h"

#include "BSE_editipo.h"
#include "BSE_edit.h"

#include "BDR_editobject.h"
#include "BPY_extern.h"

#include "butspace.h" // own module

static float prspeed=0.0;
float prlen=0.0;


/* ********************* CONSTRAINT ***************************** */

static void constraint_active_func(void *ob_v, void *con_v)
{
	Object *ob= ob_v;
	bConstraint *con;
	ListBase *lb;
	
	/* lets be nice and escape if its active already */
	if(con_v) {
		con= con_v;
		if(con->flag & CONSTRAINT_ACTIVE) return;
	}
	
	lb= get_active_constraints(ob);
	
	for(con= lb->first; con; con= con->next) {
		if(con==con_v) con->flag |= CONSTRAINT_ACTIVE;
		else con->flag &= ~CONSTRAINT_ACTIVE;
	}

	/* make sure ipowin and buttons shows it */
	if(ob->ipowin==ID_CO) {
		allqueue(REDRAWIPO, ID_CO);
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWNLA, 0);
	}
	allqueue(REDRAWBUTSOBJECT, 0);
}

static void add_constraint_to_active(Object *ob, bConstraint *con)
{
	ListBase *list= get_active_constraints(ob);
	bPoseChannel *pchan= get_active_posechannel(ob);
	
	if (list) {
		unique_constraint_name(con, list);
		BLI_addtail(list, con);
		
		if (proxylocked_constraints_owner(ob, pchan))
			con->flag |= CONSTRAINT_PROXY_LOCAL;
		
		con->flag |= CONSTRAINT_ACTIVE;
		for (con= con->prev; con; con= con->prev)
			con->flag &= ~CONSTRAINT_ACTIVE;
	}
}

/* returns base ID for Ipo, sets actname to channel if appropriate */
/* should not make action... */
static void get_constraint_ipo_context(void *ob_v, char *actname)
{
	Object *ob= ob_v;
	
	/* todo: check object if it has ob-level action ipo */
	if (ob->flag & OB_POSEMODE) {
		bPoseChannel *pchan;
		
		pchan = get_active_posechannel(ob);
		if (pchan) {
			BLI_strncpy(actname, pchan->name, 32);
		}
	}
	else if(ob->ipoflag & OB_ACTION_OB)
		strcpy(actname, "Object");
}	

/* initialize UI to show Ipo window and make sure channels etc exist */
static void enable_constraint_ipo_func (void *ob_v, void *con_v)
{
	Object *ob= ob_v;
	bConstraint *con = con_v;
	char actname[32]="";
	
	/* verifies if active constraint is set and shown in UI */
	constraint_active_func(ob_v, con_v);
	
	/* the context */
	get_constraint_ipo_context(ob, actname);
	
	/* adds ipo & channels & curve if needed */
	if(con->flag & CONSTRAINT_OWN_IPO)
		verify_ipo((ID *)ob, ID_CO, NULL, con->name, actname);
	else
		verify_ipo((ID *)ob, ID_CO, actname, con->name, NULL);
		
	/* make sure ipowin shows it */
	ob->ipowin= ID_CO;
	allqueue(REDRAWIPO, ID_CO);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWNLA, 0);
}


static void add_influence_key_to_constraint_func (void *ob_v, void *con_v)
{
	Object *ob= ob_v;
	bConstraint *con = con_v;
	IpoCurve *icu;
	char actname[32]="";
	
	/* verifies if active constraint is set and shown in UI */
	constraint_active_func(ob_v, con_v);
	
	/* the context */
	get_constraint_ipo_context(ob, actname);

	/* adds ipo & channels & curve if needed */
	if(con->flag & CONSTRAINT_OWN_IPO)
		icu= verify_ipocurve((ID *)ob, ID_CO, NULL, con->name, actname, CO_ENFORCE);
	else
		icu= verify_ipocurve((ID *)ob, ID_CO, actname, con->name, NULL, CO_ENFORCE);
		
	if (!icu) {
		error("Cannot get a curve from this IPO, may be dealing with linked data");
		return;
	}
	
	if(ob->action)
		insert_vert_icu(icu, get_action_frame(ob, (float)CFRA), con->enforce, 0);
	else
		insert_vert_icu(icu, CFRA, con->enforce, 0);
	
	/* make sure ipowin shows it */
	ob->ipowin= ID_CO;
	allqueue(REDRAWIPO, ID_CO);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWNLA, 0);
	
	BIF_undo_push("Insert Influence Key");
}

void del_constr_func (void *ob_v, void *con_v)
{
	bConstraint *con= con_v;
	bConstraintChannel *chan;
	ListBase *lb;
	
	/* remove ipo channel */
	lb= get_active_constraint_channels(ob_v, 0);
	if(lb) {
		chan = get_constraint_channel(lb, con->name);
		if(chan) {
			if(chan->ipo) chan->ipo->id.us--;
			BLI_freelinkN(lb, chan);
		}
	}
	/* remove constraint itself */
	lb= get_active_constraints(ob_v);
	free_constraint_data (con);
	BLI_freelinkN(lb, con);
	
	constraint_active_func(ob_v, NULL);
}

static void del_constraint_func (void *ob_v, void *con_v)
{
	del_constr_func (ob_v, con_v);
	BIF_undo_push("Delete constraint");
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWIPO, 0); 
}

static void verify_constraint_name_func (void *con_v, void *name_v)
{
	Object *ob= OBACT;
	bConstraint *con= con_v;
	char oldname[32];	
	
	if (!con)
		return;
	
	/* put on the stack */
	BLI_strncpy(oldname, (char *)name_v, 32);
	
	rename_constraint(ob, con, oldname);
	
	constraint_active_func(ob, con);
	allqueue(REDRAWACTION, 0); 
}

void const_moveUp(void *ob_v, void *con_v)
{
	bConstraint *con, *constr= con_v;
	ListBase *conlist;
	
	if(constr->prev) {
		conlist = get_active_constraints(ob_v);
		for(con= conlist->first; con; con= con->next) {
			if(con==constr) {
				BLI_remlink(conlist, con);
				BLI_insertlink(conlist, con->prev->prev, con);
				break;
			}
		}
	}
}

static void constraint_moveUp(void *ob_v, void *con_v)
{
	const_moveUp(ob_v, con_v);
	BIF_undo_push("Move constraint");
}

void const_moveDown(void *ob_v, void *con_v)
{
	bConstraint *con, *constr= con_v;
	ListBase *conlist;
	
	if(constr->next) {
		conlist = get_active_constraints(ob_v);
		for(con= conlist->first; con; con= con->next) {
			if(con==constr) {
				BLI_remlink(conlist, con);
				BLI_insertlink(conlist, con->next, con);
				break;
			}
		}
	}
}

static void constraint_moveDown(void *ob_v, void *con_v)
{
	const_moveDown(ob_v, con_v);
	BIF_undo_push("Move constraint");
}

/* autocomplete callback for  buttons */
void autocomplete_bone(char *str, void *arg_v)
{
	Object *ob= (Object *)arg_v;
	
	if(ob==NULL || ob->pose==NULL) return;
	
	/* search if str matches the beginning of name */
	if(str[0]) {
		AutoComplete *autocpl= autocomplete_begin(str, 32);
		bPoseChannel *pchan;
		
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next)
			autocomplete_do_name(autocpl, pchan->name);

		autocomplete_end(autocpl, str);
	}
}

/* autocomplete callback for buttons */
void autocomplete_vgroup(char *str, void *arg_v)
{
	Object *ob= (Object *)arg_v;
	
	if(ob==NULL) return;
	
	/* search if str matches the beginning of a name */
	if(str[0]) {
		AutoComplete *autocpl= autocomplete_begin(str, 32);
		bDeformGroup *dg;
		
		for(dg= ob->defbase.first; dg; dg= dg->next)
			if(dg->name!=str)
				autocomplete_do_name(autocpl, dg->name);

		autocomplete_end(autocpl, str);
	}
}

/* pole angle callback */
void con_kinematic_set_pole_angle(void *ob_v, void *con_v)
{
	bConstraint *con= con_v;
	bKinematicConstraint *data = con->data;

	if(data->poletar) {
		if(data->flag & CONSTRAINT_IK_SETANGLE) {
			data->flag |= CONSTRAINT_IK_GETANGLE;
			data->flag &= ~CONSTRAINT_IK_SETANGLE;
		}
		else {
			data->flag &= ~CONSTRAINT_IK_GETANGLE;
			data->flag |= CONSTRAINT_IK_SETANGLE;
		}
	}
}

/* some commonly used macros in the constraints drawing code */
#define is_armature_target(target) (target && target->type==OB_ARMATURE)
#define is_armature_owner(ob) ((ob->type == OB_ARMATURE) && (ob->flag & OB_POSEMODE))
#define is_geom_target(target) (target && (ELEM(target->type, OB_MESH, OB_LATTICE)) )

/* Helper function for draw constraint - draws constraint space stuff 
 * This function should not be called if no menus are required 
 * owner/target: -1 = don't draw menu; 0= not posemode, 1 = posemode 
 */
static void draw_constraint_spaceselect (uiBlock *block, bConstraint *con, short xco, short yco, short owner, short target)
{
	short tarx, ownx;
	short bwidth;
	
	/* calculate sizes and placement of menus */
	if (owner == -1) {
		bwidth = 125;
		tarx = 120;
		ownx = 0;
	}
	else if (target == -1) {
		bwidth = 125;
		tarx = 0;
		ownx = 120;
	}
	else {
		bwidth = 100;
		tarx = 95;
		ownx = tarx + bwidth;
	}
	
	
	uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "CSpace:", xco, yco, 80,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
	
	uiBlockBeginAlign(block);
	
	/* Target-Space */
	if (target == 1) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Pose Space %x3|Local with Parent %x4|Local Space %x1", 
												tarx, yco, bwidth, 18, &con->tarspace, 0, 0, 0, 0, "Choose space that target is evaluated in");	
	}
	else if (target == 0) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Local (Without Parent) Space %x1", 
										tarx, yco, bwidth, 18, &con->tarspace, 0, 0, 0, 0, "Choose space that target is evaluated in");	
	}
	
	/* Owner-Space */
	if (owner == 1) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Owner Space %t|World Space %x0|Pose Space %x3|Local with Parent %x4|Local Space %x1", 
												ownx, yco, bwidth, 18, &con->ownspace, 0, 0, 0, 0, "Choose space that owner is evaluated in");	
	}
	else if (owner == 0) {
		uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Owner Space %t|World Space %x0|Local (Without Parent) Space %x1", 
										ownx, yco, bwidth, 18, &con->ownspace, 0, 0, 0, 0, "Choose space that owner is evaluated in");	
	}
	
	uiBlockEndAlign(block);
}

/* draw panel showing settings for a constraint */
static void draw_constraint (uiBlock *block, ListBase *list, bConstraint *con, short *xco, short *yco)
{
	Object *ob= OBACT;
	bPoseChannel *pchan= get_active_posechannel(ob);
	bConstraintTypeInfo *cti;
	uiBut *but;
	char typestr[32];
	short height, width = 265;
	short proxy_protected;
	int rb_col;

	/* get constraint typeinfo */
	cti= constraint_get_typeinfo(con);
	if (cti == NULL) {
		/* exception for 'Null' constraint - it doesn't have constraint typeinfo! */
		if (con->type == CONSTRAINT_TYPE_NULL)
			strcpy(typestr, "Null");
		else
			strcpy(typestr, "Unknown");
	}
	else
		strcpy(typestr, cti->name);
		
	/* determine whether constraint is proxy protected or not */
	if (proxylocked_constraints_owner(ob, pchan)) {
		proxy_protected= (con->flag & CONSTRAINT_PROXY_LOCAL) ? 0 : 1;
	}
	else
		proxy_protected= 0;
		
	/* unless button has own callback, it adds this callback to button */
	uiBlockSetFunc(block, constraint_active_func, ob, con);

	/* Draw constraint header */
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* rounded header */
	rb_col= (con->flag & CONSTRAINT_ACTIVE)?50:20;
	uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-1, width+40, 22, NULL, 5.0, 0.0, 
			 (con->flag & CONSTRAINT_EXPAND)?3:15 , rb_col-20, ""); 
	
	/* open/close */
	uiDefIconButBitS(block, ICONTOG, CONSTRAINT_EXPAND, B_CONSTRAINT_TEST, ICON_DISCLOSURE_TRI_RIGHT, *xco-10, *yco, 20, 20, &con->flag, 0.0, 0.0, 0.0, 0.0, "Collapse/Expand Constraint");
	
	/* name */	
	if ((con->flag & CONSTRAINT_EXPAND) && (proxy_protected==0)) {
		if (con->flag & CONSTRAINT_DISABLE)
			uiBlockSetCol(block, TH_REDALERT);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		
		uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, *xco+10, *yco, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		
		but = uiDefBut(block, TEX, B_CONSTRAINT_TEST, "", *xco+120, *yco, 85, 18, con->name, 0.0, 29.0, 0.0, 0.0, "Constraint name"); 
		uiButSetFunc(but, verify_constraint_name_func, con, NULL);
	}	
	else {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		if (con->flag & CONSTRAINT_DISABLE)
			uiBlockSetCol(block, TH_REDALERT);
		
		uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, *xco+10, *yco, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		
		uiDefBut(block, LABEL, B_CONSTRAINT_TEST, con->name, *xco+120, *yco-1, 135, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
	}

	uiBlockSetCol(block, TH_AUTO);	
	
	/* proxy-protected constraints cannot be edited, so hide up/down + close buttons */
	if (proxy_protected) {
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* draw a ghost icon (for proxy) and also a lock beside it, to show that constraint is "proxy locked" */
		uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, ICON_GHOST, *xco+244, *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Proxy Protected");
		uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, ICON_LOCKED, *xco+262, *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Proxy Protected");
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else {
		short prev_proxylock, show_upbut, show_downbut;
		
		/* Up/Down buttons: 
		 *	Proxy-constraints are not allowed to occur after local (non-proxy) constraints
		 *	as that poses problems when restoring them, so disable the "up" button where
		 *	it may cause this situation. 
		 *
		 * 	Up/Down buttons should only be shown (or not greyed - todo) if they serve some purpose. 
		 */
		if (proxylocked_constraints_owner(ob, pchan)) {
			if (con->prev) {
				prev_proxylock= (con->prev->flag & CONSTRAINT_PROXY_LOCAL) ? 0 : 1;
			}
			else
				prev_proxylock= 0;
		}
		else
			prev_proxylock= 0;
			
		show_upbut= ((prev_proxylock == 0) && (con->prev));
		show_downbut= (con->next) ? 1 : 0;
		
		if (show_upbut || show_downbut) {
			uiBlockBeginAlign(block);
				uiBlockSetEmboss(block, UI_EMBOSS);
				
				if (show_upbut) {
					but = uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, VICON_MOVE_UP, *xco+width-50, *yco, 16, 18, NULL, 0.0, 0.0, 0.0, 0.0, "Move constraint up in constraint stack");
					uiButSetFunc(but, constraint_moveUp, ob, con);
				}
				
				if (show_downbut) {
					but = uiDefIconBut(block, BUT, B_CONSTRAINT_TEST, VICON_MOVE_DOWN, *xco+width-50+18, *yco, 16, 18, NULL, 0.0, 0.0, 0.0, 0.0, "Move constraint down in constraint stack");
					uiButSetFunc(but, constraint_moveDown, ob, con);
				}
			uiBlockEndAlign(block);
		}
		
		
		/* Close 'button' - emboss calls here disable drawing of 'button' behind X */
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
			but = uiDefIconBut(block, BUT, B_CONSTRAINT_CHANGETARGET, ICON_X, *xco+262, *yco, 19, 19, list, 0.0, 0.0, 0.0, 0.0, "Delete constraint");
			uiButSetFunc(but, del_constraint_func, ob, con);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	
	/* Set but-locks for protected settings (magic numbers are used here!) */
	if (proxy_protected)
		uiSetButLock(1, "Cannot edit Proxy-Protected Constraint");
	
	/* Draw constraint data */
	if ((con->flag & CONSTRAINT_EXPAND) == 0) {
		(*yco) -= 21;
	}
	else {
		switch (con->type) {
		case CONSTRAINT_TYPE_PYTHON:
			{
				bPythonConstraint *data = con->data;
				bConstraintTarget *ct;
				uiBut *but2;
				int tarnum, theight;
				static int pyconindex=0;
				char *menustr;
				
				theight = (data->tarnum)? (data->tarnum * 38) : (38);
				height = theight + 78;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40, height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Script:", *xco+60, *yco-24, 55, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* do the scripts menu */
				menustr = buildmenu_pyconstraints(data->text, &pyconindex);
				but2 = uiDefButI(block, MENU, B_CONSTRAINT_TEST, menustr,
				      *xco+120, *yco-24, 150, 20, &pyconindex,
				      0, 0, 0, 0, "Set the Script Constraint to use");
				uiButSetFunc(but2, validate_pyconstraint_cb, data, &pyconindex);
				MEM_freeN(menustr);	
				
				/* draw target(s) */
				if (data->flag & PYCON_USETARGETS) {
					/* Draw target parameters */ 
					for (ct=data->targets.first, tarnum=1; ct; ct=ct->next, tarnum++) {
						char tarstr[32];
						short yoffset= ((tarnum-1) * 38);
						
						/* target label */
						sprintf(tarstr, "Target %d:", tarnum);
						uiDefBut(block, LABEL, B_CONSTRAINT_TEST, tarstr, *xco+45, *yco-(48+yoffset), 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
						
						/* target space-selector - per target */
						if (is_armature_target(ct->tar)) {
							uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Pose Space %x3|Local with Parent %x4|Local Space %x1", 
															*xco+10, *yco-(66+yoffset), 100, 18, &ct->space, 0, 0, 0, 0, "Choose space that target is evaluated in");	
						}
						else {
							uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Target Space %t|World Space %x0|Local (Without Parent) Space %x1", 
															*xco+10, *yco-(66+yoffset), 100, 18, &ct->space, 0, 0, 0, 0, "Choose space that target is evaluated in");	
						}
						
						uiBlockBeginAlign(block);
							/* target object */
							uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-(48+yoffset), 150, 18, &ct->tar, "Target Object"); 
							
							/* subtarget */
							if (is_armature_target(ct->tar)) {
								but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-(66+yoffset),150,18, &ct->subtarget, 0, 24, 0, 0, "Subtarget Bone");
								uiButSetCompleteFunc(but, autocomplete_bone, (void *)ct->tar);
							}
							else if (is_geom_target(ct->tar)) {
								but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-(66+yoffset),150,18, &ct->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
								uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)ct->tar);
							}
							else {
								strcpy(ct->subtarget, "");
							}
						uiBlockEndAlign(block);
					}
				}
				else {
					/* Draw indication that no target needed */
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+60, *yco-48, 55, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Not Applicable", *xco+120, *yco-48, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				}
				
				/* settings */
				uiBlockBeginAlign(block);
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Options", *xco, *yco-(52+theight), (width/2),18, NULL, 0, 24, 0, 0, "Change some of the constraint's settings.");
					uiButSetFunc(but, BPY_pyconstraint_settings, data, NULL);
					
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Refresh", *xco+((width/2)+10), *yco-(52+theight), (width/2),18, NULL, 0, 24, 0, 0, "Force constraint to refresh it's settings");
				uiBlockEndAlign(block);
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-(73+theight), is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data = con->data;
				float minval, maxval;
				
				height = 108;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				
				uiBlockEndAlign(block);
				
				/* Draw action/type buttons */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_actionpoin_but, ID_AC, B_CONSTRAINT_TEST, "AC:",	*xco+((width/2)-117), *yco-64, 78, 18, &data->act, "Action containing the keyed motion for this bone"); 
					uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Key on%t|Loc X%x20|Loc Y%x21|Loc Z%x22|Rot X%x0|Rot Y%x1|Rot Z%x2|Size X%x10|Size Y%x11|Size Z%x12", *xco+((width/2)-117), *yco-84, 78, 18, &data->type, 0, 24, 0, 0, "Specify which transformation channel from the target is used to key the action");
				uiBlockEndAlign(block);
				
				/* Draw start/end frame buttons */
				uiBlockBeginAlign(block);
					uiDefButI(block, NUM, B_CONSTRAINT_TEST, "Start:", *xco+((width/2)-36), *yco-64, 78, 18, &data->start, 1, MAXFRAME, 0.0, 0.0, "Starting frame of the keyed motion"); 
					uiDefButI(block, NUM, B_CONSTRAINT_TEST, "End:", *xco+((width/2)-36), *yco-84, 78, 18, &data->end, 1, MAXFRAME, 0.0, 0.0, "Ending frame of the keyed motion"); 
				uiBlockEndAlign(block);
				
				/* Draw minimum/maximum transform range buttons */
				uiBlockBeginAlign(block);
					if (data->type < 10) { /* rotation */
						minval = -180.0f;
						maxval = 180.0f;
					}
					else if (data->type < 20) { /* scaling */
						minval = 0.0001f;
						maxval = 1000.0f;
					}
					else { /* location */
						minval = -1000.0f;
						maxval = 1000.0f;
					}
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Min:", *xco+((width/2)+45), *yco-64, 78, 18, &data->min, minval, maxval, 0, 0, "Minimum value for target channel range");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Max:", *xco+((width/2)+45), *yco-84, 78, 18, &data->max, minval, maxval, 0, 0, "Maximum value for target channel range");
				uiBlockEndAlign(block);
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-104, -1, is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_CHILDOF:
			{
				bChildOfConstraint *data = con->data;
				short normButWidth = (width/3);
				
				height = 165;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Parent:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object to use as Parent"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone to use as Parent");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw triples of channel toggles */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Use Channel(s):", *xco+65, *yco-64, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiBlockBeginAlign(block); 
					uiDefButBitI(block, TOG, CHILDOF_LOCX, B_CONSTRAINT_TEST, "Loc X", *xco, *yco-84, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects x-location"); 
					uiDefButBitI(block, TOG, CHILDOF_LOCY, B_CONSTRAINT_TEST, "Loc Y", *xco+normButWidth, *yco-84, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects y-location"); 
					uiDefButBitI(block, TOG, CHILDOF_LOCZ, B_CONSTRAINT_TEST, "Loc Z", *xco+(normButWidth * 2), *yco-84, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects z-location"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitI(block, TOG, CHILDOF_ROTX, B_CONSTRAINT_TEST, "Rot X", *xco, *yco-105, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects x-rotation"); 
					uiDefButBitI(block, TOG, CHILDOF_ROTY, B_CONSTRAINT_TEST, "Rot Y", *xco+normButWidth, *yco-105, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects y-rotation"); 
					uiDefButBitI(block, TOG, CHILDOF_ROTZ, B_CONSTRAINT_TEST, "Rot Z", *xco+(normButWidth * 2), *yco-105, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects z-rotation"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitI(block, TOG, CHILDOF_SIZEX, B_CONSTRAINT_TEST, "Scale X", *xco, *yco-126, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects x-scaling"); 
					uiDefButBitI(block, TOG, CHILDOF_SIZEY, B_CONSTRAINT_TEST, "Scale Y", *xco+normButWidth, *yco-126, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects y-scaling"); 
					uiDefButBitI(block, TOG, CHILDOF_SIZEZ, B_CONSTRAINT_TEST, "Scale Z", *xco+(normButWidth * 2), *yco-126, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Parent affects z-scaling"); 
				uiBlockEndAlign(block);
				
				
				/* Inverse options */
				uiBlockBeginAlign(block);
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Set Offset", *xco, *yco-151, (width/2),18, NULL, 0, 24, 0, 0, "Calculate current Parent-Inverse Matrix (i.e. restore offset from parent)");
					uiButSetFunc(but, childof_const_setinv, con, NULL);
					
					but=uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Clear Offset", *xco+((width/2)+10), *yco-151, (width/2),18, NULL, 0, 24, 0, 0, "Clear Parent-Inverse Matrix (i.e. clear offset from parent)");
					uiButSetFunc(but, childof_const_clearinv, con, NULL);
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data = con->data;
				
				height = 111;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefButBitI(block, TOG, LOCLIKE_X, B_CONSTRAINT_TEST, "X", *xco+((width/2)-48), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
					uiDefButBitI(block, TOG, LOCLIKE_X_INVERT, B_CONSTRAINT_TEST, "-", *xco+((width/2)-16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert X component");
					uiDefButBitI(block, TOG, LOCLIKE_Y, B_CONSTRAINT_TEST, "Y", *xco+((width/2)+16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
					uiDefButBitI(block, TOG, LOCLIKE_Y_INVERT, B_CONSTRAINT_TEST, "-", *xco+((width/2)+48), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert Y component");
					uiDefButBitI(block, TOG, LOCLIKE_Z, B_CONSTRAINT_TEST, "Z", *xco+((width/2)+96), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
					uiDefButBitI(block, TOG, LOCLIKE_Z_INVERT, B_CONSTRAINT_TEST, "-", *xco+((width/2)+128), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert Z component");
				uiBlockEndAlign(block);
				
				/* Draw options */
				uiDefButBitI(block, TOG, LOCLIKE_OFFSET, B_CONSTRAINT_TEST, "Offset", *xco, *yco-89, (width/2), 18, &data->flag, 0, 24, 0, 0, "Add original location onto copied location");
				if (is_armature_target(data->tar)) {
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", *xco+(width/2), *yco-89, (width/2), 18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
				}
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-109, is_armature_owner(ob), is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data = con->data;
				
				height = 101;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) { 
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefButBitI(block, TOG, ROTLIKE_X, B_CONSTRAINT_TEST, "X", *xco+((width/2)-48), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
					uiDefButBitI(block, TOG, ROTLIKE_X_INVERT, B_CONSTRAINT_TEST, "-", *xco+((width/2)-16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert X component");
					uiDefButBitI(block, TOG, ROTLIKE_Y, B_CONSTRAINT_TEST, "Y", *xco+((width/2)+16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
					uiDefButBitI(block, TOG, ROTLIKE_Y_INVERT, B_CONSTRAINT_TEST, "-", *xco+((width/2)+48), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert Y component");
					uiDefButBitI(block, TOG, ROTLIKE_Z, B_CONSTRAINT_TEST, "Z", *xco+((width/2)+96), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
					uiDefButBitI(block, TOG, ROTLIKE_Z_INVERT, B_CONSTRAINT_TEST, "-", *xco+((width/2)+128), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Invert Z component");
				uiBlockEndAlign(block);
				
				/* draw offset toggle */
				uiDefButBitI(block, TOG, ROTLIKE_OFFSET, B_CONSTRAINT_TEST, "Offset", *xco, *yco-64, 80, 18, &data->flag, 0, 24, 0, 0, "Add original rotation onto copied rotation");
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-94, is_armature_owner(ob), is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_SIZELIKE:
			{
				bSizeLikeConstraint *data = con->data;
				
				height = 101;
				
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefButBitI(block, TOG, SIZELIKE_X, B_CONSTRAINT_TEST, "X", *xco+((width/2)-48), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
					uiDefButBitI(block, TOG, SIZELIKE_Y, B_CONSTRAINT_TEST, "Y", *xco+((width/2)-16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
					uiDefButBitI(block, TOG, SIZELIKE_Z, B_CONSTRAINT_TEST, "Z", *xco+((width/2)+16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
				uiBlockEndAlign(block);
				
				/* draw offset toggle */
				uiDefButBitI(block, TOG, SIZELIKE_OFFSET, B_CONSTRAINT_TEST, "Offset", *xco, *yco-64, 80, 18, &data->flag, 0, 24, 0, 0, "Add original scaling onto copied scaling");
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-94, is_armature_owner(ob), is_armature_target(data->tar));
			}
 			break;
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data = con->data;
				
				height = 146;
				if(data->poletar) 
					height += 30;

				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				/* IK Target */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco, *yco-24, 80, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco, *yco-44, 137, 19, &data->tar, "Target Object"); 

				if (is_armature_target(data->tar)) {
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco, *yco-62,137,19, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
					uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
				}
				else if (is_geom_target(data->tar)) {
					but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco, *yco-62,137,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
					uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
				}
				else {
					strcpy (data->subtarget, "");
				}
				
				uiBlockEndAlign(block);
				
				/* Settings */
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, CONSTRAINT_IK_TIP, B_CONSTRAINT_TEST, "Use Tail", *xco, *yco-92, 137, 19, &data->flag, 0, 0, 0, 0, "Include Bone's tail also last element in Chain");
				uiDefButS(block, NUM, B_CONSTRAINT_TEST, "ChainLen:", *xco, *yco-112,137,19, &data->rootbone, 0, 255, 0, 0, "If not zero, the amount of bones in this chain");
				
				uiBlockBeginAlign(block);
				uiDefButF(block, NUMSLI, B_CONSTRAINT_TEST, "PosW ", *xco+147, *yco-92, 137, 19, &data->weight, 0.01, 1.0, 2, 2, "For Tree-IK: weight of position control for this target");
				uiDefButBitS(block, TOG, CONSTRAINT_IK_ROT, B_CONSTRAINT_TEST, "Rot", *xco+147, *yco-112, 40,19, &data->flag, 0, 0, 0, 0, "Chain follows rotation of target");
				uiDefButF(block, NUMSLI, B_CONSTRAINT_TEST, "W ", *xco+187, *yco-112, 97, 19, &data->orientweight, 0.01, 1.0, 2, 2, "For Tree-IK: Weight of orientation control for this target");
				
				uiBlockBeginAlign(block);
				
				uiDefButBitS(block, TOG, CONSTRAINT_IK_STRETCH, B_CONSTRAINT_TEST, "Stretch", *xco, *yco-137,137,19, &data->flag, 0, 0, 0, 0, "Enable IK stretching");
				uiBlockBeginAlign(block);
				uiDefButS(block, NUM, B_CONSTRAINT_TEST, "Iterations:", *xco+147, *yco-137, 137, 19, &data->iterations, 1, 10000, 0, 0, "Maximum number of solving iterations"); 
				uiBlockEndAlign(block);
				
				/* Pole Vector */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Pole Target:", *xco+147, *yco-24, 100, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+147, *yco-44, 137, 19, &data->poletar, "Pole Target Object"); 
				if (is_armature_target(data->poletar)) {
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+147, *yco-62,137,19, &data->polesubtarget, 0, 24, 0, 0, "Pole Subtarget Bone");
					uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->poletar);
				}
				else if (is_geom_target(data->poletar)) {
					but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+147, *yco-62,137,18, &data->polesubtarget, 0, 24, 0, 0, "Name of Vertex Group defining pole 'target' points");
					uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->poletar);
				}
				else {
					strcpy(data->polesubtarget, "");
				}
				
				if (data->poletar) {
					uiBlockBeginAlign(block);
#if 0
					but = uiDefBut(block, BUT, B_CONSTRAINT_TEST, (data->flag & CONSTRAINT_IK_SETANGLE)? "Set Pole Offset": "Clear Pole Offset", *xco, *yco-167, 137, 19, 0, 0.0, 1.0, 0.0, 0.0, "Set the pole rotation offset from the current pose");
					uiButSetFunc(but, con_kinematic_set_pole_angle, ob, con);
					if (!(data->flag & CONSTRAINT_IK_SETANGLE))
#endif					
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pole Offset ", *xco, *yco-167, 137, 19, &data->poleangle, -180.0, 180.0, 0, 0, "Pole rotation offset");
				}
			}
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data = con->data;
				
				if (is_armature_target(data->tar)) 
					height = 118;
				else
					height = 96;
					
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Align:", *xco+5, *yco-42, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButBitI(block, TOG, 1, B_CONSTRAINT_TEST, "TargetZ", *xco+60, *yco-42, 50, 18, &data->flags, 0, 1, 0, 0, "Target Z axis, not world Z axis, will constrain up direction");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "To:", *xco+12, *yco-64, 25, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	*xco+39, *yco-64,17,18, &data->reserved1, 12.0, 0.0, 0, 0, "X axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Y",	*xco+56, *yco-64,17,18, &data->reserved1, 12.0, 1.0, 0, 0, "Y axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	*xco+73, *yco-64,17,18, &data->reserved1, 12.0, 2.0, 0, 0, "Z axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"-X",	*xco+90, *yco-64,24,18, &data->reserved1, 12.0, 3.0, 0, 0, "-X axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"-Y",	*xco+114, *yco-64,24,18, &data->reserved1, 12.0, 4.0, 0, 0, "-Y axis points to the target object");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"-Z",	*xco+138, *yco-64,24,18, &data->reserved1, 12.0, 5.0, 0, 0, "-Z axis points to the target object");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Up:", *xco+174, *yco-64, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	*xco+204, *yco-64,17,18, &data->reserved2, 13.0, 0.0, 0, 0, "X axis points upward");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Y",	*xco+221, *yco-64,17,18, &data->reserved2, 13.0, 1.0, 0, 0, "Y axis points upward");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	*xco+238, *yco-64,17,18, &data->reserved2, 13.0, 2.0, 0, 0, "Z axis points upward");
				uiBlockEndAlign(block);
				
				if (is_armature_target(data->tar)) {
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", *xco, *yco-94, 241, 18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
					
					/* constraint space settings */
					draw_constraint_spaceselect(block, con, *xco, *yco-116, is_armature_owner(ob), is_armature_target(data->tar));
				}
				else {
					/* constraint space settings */
					draw_constraint_spaceselect(block, con, *xco, *yco-94, is_armature_owner(ob), is_armature_target(data->tar));
				}
			}
			break;
		case CONSTRAINT_TYPE_MINMAX:
			{
				bMinMaxConstraint *data = con->data;
				
				if (is_armature_target(data->tar)) 
					height = 88;
				else
					height = 66;
					
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Offset:", *xco, *yco-44, 100, 18, &data->offset, -100, 100, 100.0, 0.0, "Offset from the position of the object center"); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiDefButBitI(block, TOG, MINMAX_STICKY, B_CONSTRAINT_TEST, "Sticky", *xco, *yco-24, 44, 18, &data->flag, 0, 24, 0, 0, "Immobilize object while constrained");
				uiDefButBitI(block, TOG, MINMAX_USEROT, B_CONSTRAINT_TEST, "Use Rot", *xco+44, *yco-24, 64, 18, &data->flag, 0, 24, 0, 0, "Use target object rotation");
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Max/Min:", *xco-8, *yco-64, 54, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiBlockBeginAlign(block);			
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	*xco+51, *yco-64,17,18, &data->minmaxflag, 12.0, 0.0, 0, 0, "Will not pass below X of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	*xco+67, *yco-64,17,18, &data->minmaxflag, 12.0, 1.0, 0, 0, "Will not pass below Y of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	*xco+85, *yco-64,17,18, &data->minmaxflag, 12.0, 2.0, 0, 0, "Will not pass below Z of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-X",	*xco+102, *yco-64,24,18, &data->minmaxflag, 12.0, 3.0, 0, 0, "Will not pass above X of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Y",	*xco+126, *yco-64,24,18, &data->minmaxflag, 12.0, 4.0, 0, 0, "Will not pass above Y of target");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Z",	*xco+150, *yco-64,24,18, &data->minmaxflag, 12.0, 5.0, 0, 0, "Will not pass above Z of target");
				uiBlockEndAlign(block);
				
				if (is_armature_target(data->tar)) {
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", *xco, *yco-86, 241, 18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
				}
				
			}
			break;
		case CONSTRAINT_TYPE_LOCKTRACK:
			{
				bLockTrackConstraint *data = con->data;
				height = 66;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "To:", *xco+12, *yco-64, 25, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	*xco+39, *yco-64,17,18, &data->trackflag, 12.0, 0.0, 0, 0, "X axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	*xco+56, *yco-64,17,18, &data->trackflag, 12.0, 1.0, 0, 0, "Y axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	*xco+73, *yco-64,17,18, &data->trackflag, 12.0, 2.0, 0, 0, "Z axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-X",	*xco+90, *yco-64,24,18, &data->trackflag, 12.0, 3.0, 0, 0, "-X axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Y",	*xco+114, *yco-64,24,18, &data->trackflag, 12.0, 4.0, 0, 0, "-Y axis points to the target object");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Z",	*xco+138, *yco-64,24,18, &data->trackflag, 12.0, 5.0, 0, 0, "-Z axis points to the target object");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Lock:", *xco+166, *yco-64, 38, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	*xco+204, *yco-64,17,18, &data->lockflag, 13.0, 0.0, 0, 0, "X axis is locked");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	*xco+221, *yco-64,17,18, &data->lockflag, 13.0, 1.0, 0, 0, "Y axis is locked");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	*xco+238, *yco-64,17,18, &data->lockflag, 13.0, 2.0, 0, 0, "Z axis is locked");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_FOLLOWPATH:
			{
				bFollowPathConstraint *data = con->data;
				
				height = 66;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
				
				/* Draw Curve Follow toggle */
				uiDefButBitI(block, TOG, 1, B_CONSTRAINT_TEST, "CurveFollow", *xco+39, *yco-44, 100, 18, &data->followflag, 0, 24, 0, 0, "Object will follow the heading and banking of the curve");
				
				/* Draw Offset number button */
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Offset:", *xco+155, *yco-44, 100, 18, &data->offset, -MAXFRAMEF, MAXFRAMEF, 100.0, 0.0, "Offset from the position corresponding to the time frame"); 
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Fw:", *xco+12, *yco-64, 27, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	*xco+39, *yco-64,17,18, &data->trackflag, 12.0, 0.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	*xco+56, *yco-64,17,18, &data->trackflag, 12.0, 1.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	*xco+73, *yco-64,17,18, &data->trackflag, 12.0, 2.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-X",	*xco+90, *yco-64,24,18, &data->trackflag, 12.0, 3.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Y",	*xco+114, *yco-64,24,18, &data->trackflag, 12.0, 4.0, 0, 0, "The axis that points forward along the path");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"-Z",	*xco+138, *yco-64,24,18, &data->trackflag, 12.0, 5.0, 0, 0, "The axis that points forward along the path");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Up:", *xco+174, *yco-64, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
					
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"X",	*xco+204, *yco-64,17,18, &data->upflag, 13.0, 0.0, 0, 0, "The axis that points upward");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Y",	*xco+221, *yco-64,17,18, &data->upflag, 13.0, 1.0, 0, 0, "The axis that points upward");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST,"Z",	*xco+238, *yco-64,17,18, &data->upflag, 13.0, 2.0, 0, 0, "The axis that points upward");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_STRETCHTO:
			{
				bStretchToConstraint *data = con->data;
				
				height = 105;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					if (is_armature_target(data->tar)) {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", *xco, *yco-60, 20, 18, &data->orglength, 0.0, 0, 0, 0, "Recalculate RLength");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Rest Length:", *xco+18, *yco-60,139,18, &data->orglength, 0.0, 100, 0.5, 0.5, "Length at Rest Position");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", *xco+155, *yco-60,98,18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
					}
					else {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", *xco, *yco-60, 20, 18, &data->orglength, 0.0, 0, 0, 0, "Recalculate RLength");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Rest Length:", *xco+18, *yco-60, 237, 18, &data->orglength, 0.0, 100, 0.5, 0.5, "Length at Rest Position");
					}
				uiBlockEndAlign(block);
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Volume Variation:", *xco+18, *yco-82, 237, 18, &data->bulge, 0.0, 100, 0.5, 0.5, "Factor between volume variation and stretching");
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Vol:",*xco+14, *yco-104,30,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"XZ",	 *xco+44, *yco-104,30,18, &data->volmode, 12.0, 0.0, 0, 0, "Keep Volume: Scaling X & Z");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	 *xco+74, *yco-104,20,18, &data->volmode, 12.0, 1.0, 0, 0, "Keep Volume: Scaling X");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	 *xco+94, *yco-104,20,18, &data->volmode, 12.0, 2.0, 0, 0, "Keep Volume: Scaling Z");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"NONE", *xco+114, *yco-104,50,18, &data->volmode, 12.0, 3.0, 0, 0, "Ignore Volume");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST,"Plane:",*xco+175, *yco-104,40,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"X",	  *xco+215, *yco-104,20,18, &data->plane, 12.0, 0.0, 0, 0, "Keep X axis");
					uiDefButI(block, ROW,B_CONSTRAINT_TEST,"Z",	  *xco+235, *yco-104,20,18, &data->plane, 12.0, 2.0, 0, 0, "Keep Z axis");
				uiBlockEndAlign(block);
				}
			break;
		case CONSTRAINT_TYPE_LOCLIMIT:
			{
				bLocLimitConstraint *data = con->data;
				
				int togButWidth = 50;
				int textButWidth = ((width/2)-togButWidth);
				
				height = 136; 
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				/* Draw Pairs of LimitToggle+LimitValue */
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMIN, B_CONSTRAINT_TEST, "minX", *xco, *yco-28, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-28, (textButWidth-5), 18, &(data->xmin), -1000, 1000, 0.1,0.5,"Lowest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMAX, B_CONSTRAINT_TEST, "maxX", *xco+(width-(textButWidth-5)-togButWidth), *yco-28, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-28, (textButWidth-5), 18, &(data->xmax), -1000, 1000, 0.1,0.5,"Highest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMIN, B_CONSTRAINT_TEST, "minY", *xco, *yco-50, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-50, (textButWidth-5), 18, &(data->ymin), -1000, 1000, 0.1,0.5,"Lowest y value to allow"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMAX, B_CONSTRAINT_TEST, "maxY", *xco+(width-(textButWidth-5)-togButWidth), *yco-50, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-50, (textButWidth-5), 18, &(data->ymax), -1000, 1000, 0.1,0.5,"Highest y value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMIN, B_CONSTRAINT_TEST, "minZ", *xco, *yco-72, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-72, (textButWidth-5), 18, &(data->zmin), -1000, 1000, 0.1,0.5,"Lowest z value to allow"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMAX, B_CONSTRAINT_TEST, "maxZ", *xco+(width-(textButWidth-5)-togButWidth), *yco-72, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-72, (textButWidth-5), 18, &(data->zmax), -1000, 1000, 0.1,0.5,"Highest z value to allow"); 
				uiBlockEndAlign(block);
				
				/* special option(s) */
				uiDefButBitS(block, TOG, LIMIT_TRANSFORM, B_CONSTRAINT_TEST, "For Transform", *xco+(width/4), *yco-100, (width/2), 18, &data->flag2, 0, 24, 0, 0, "Transforms are affected by this constraint as well"); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-130, is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_ROTLIMIT:
			{
				bRotLimitConstraint *data = con->data;
				int normButWidth = (width/3);
				
				height = 136; 
				
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				/* Draw Pairs of LimitToggle+LimitValue */
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XROT, B_CONSTRAINT_TEST, "LimitX", *xco, *yco-28, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Limit rotation on x-axis"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min:", *xco+normButWidth, *yco-28, normButWidth, 18, &(data->xmin), -360, 360, 0.1,0.5,"Lowest x value to allow"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max:", *xco+(normButWidth * 2), *yco-28, normButWidth, 18, &(data->xmax), -360, 360, 0.1,0.5,"Highest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YROT, B_CONSTRAINT_TEST, "LimitY", *xco, *yco-50, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Limit rotation on y-axis"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min:", *xco+normButWidth, *yco-50, normButWidth, 18, &(data->ymin), -360, 360, 0.1,0.5,"Lowest y value to allow"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max:", *xco+(normButWidth * 2), *yco-50, normButWidth, 18, &(data->ymax), -360, 360, 0.1,0.5,"Highest y value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZROT, B_CONSTRAINT_TEST, "LimitZ", *xco, *yco-72, normButWidth, 18, &data->flag, 0, 24, 0, 0, "Limit rotation on z-axis"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min:", *xco+normButWidth, *yco-72, normButWidth, 18, &(data->zmin), -360, 360, 0.1,0.5,"Lowest z value to allow"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max:", *xco+(normButWidth * 2), *yco-72, normButWidth, 18, &(data->zmax), -360, 360, 0.1,0.5,"Highest z value to allow"); 
				uiBlockEndAlign(block); 
				
				/* special option(s) */
				uiDefButBitS(block, TOG, LIMIT_TRANSFORM, B_CONSTRAINT_TEST, "For Transform", *xco+(width/4), *yco-100, (width/2), 18, &data->flag2, 0, 24, 0, 0, "Transforms are affected by this constraint as well"); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-130, is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_SIZELIMIT:
			{
				bSizeLimitConstraint *data = con->data;
				
				int togButWidth = 50;
				int textButWidth = ((width/2)-togButWidth);
				
				height = 136; 
					
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				/* Draw Pairs of LimitToggle+LimitValue */
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMIN, B_CONSTRAINT_TEST, "minX", *xco, *yco-28, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-28, (textButWidth-5), 18, &(data->xmin), 0.0001, 1000, 0.1,0.5,"Lowest x value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_XMAX, B_CONSTRAINT_TEST, "maxX", *xco+(width-(textButWidth-5)-togButWidth), *yco-28, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum x value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-28, (textButWidth-5), 18, &(data->xmax), 0.0001, 1000, 0.1,0.5,"Highest x value to allow"); 
				uiBlockEndAlign(block); 
				
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMIN, B_CONSTRAINT_TEST, "minY", *xco, *yco-50, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-50, (textButWidth-5), 18, &(data->ymin), 0.0001, 1000, 0.1,0.5,"Lowest y value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_YMAX, B_CONSTRAINT_TEST, "maxY", *xco+(width-(textButWidth-5)-togButWidth), *yco-50, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum y value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-50, (textButWidth-5), 18, &(data->ymax), 0.0001, 1000, 0.1,0.5,"Highest y value to allow"); 
				uiBlockEndAlign(block); 
				
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMIN, B_CONSTRAINT_TEST, "minZ", *xco, *yco-72, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-72, (textButWidth-5), 18, &(data->zmin), 0.0001, 1000, 0.1,0.5,"Lowest z value to allow"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButBitS(block, TOG, LIMIT_ZMAX, B_CONSTRAINT_TEST, "maxZ", *xco+(width-(textButWidth-5)-togButWidth), *yco-72, 50, 18, &data->flag, 0, 24, 0, 0, "Use maximum z value"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-72, (textButWidth-5), 18, &(data->zmax), 0.0001, 1000, 0.1,0.5,"Highest z value to allow"); 
				uiBlockEndAlign(block);
				
				/* special option(s) */
				uiDefButBitS(block, TOG, LIMIT_TRANSFORM, B_CONSTRAINT_TEST, "For Transform", *xco+(width/4), *yco-100, (width/2), 18, &data->flag2, 0, 24, 0, 0, "Transforms are affected by this constraint as well"); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-130, is_armature_owner(ob), -1);
			}
			break;
		case CONSTRAINT_TYPE_DISTLIMIT:
			{
				bDistLimitConstraint *data = con->data;
				
				height = 105;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
					
					if (is_armature_target(data->tar)) {
						but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
					if (is_armature_target(data->tar)) {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", *xco, *yco-60, 20, 18, &data->dist, 0, 0, 0, 0, "Recalculate distance"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Distance:", *xco+18, *yco-60,139,18, &data->dist, 0.0, 100, 0.5, 0.5, "Radius of limiting sphere");
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Head/Tail:", *xco+155, *yco-60,100,18, &con->headtail, 0.0, 1, 0.1, 0.1, "Target along length of bone: Head=0, Tail=1");
					}
					else {
						uiDefButF(block, BUTM, B_CONSTRAINT_TEST, "R", *xco, *yco-60, 20, 18, &data->dist, 0, 0, 0, 0, "Recalculate distance"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Distance:", *xco+18, *yco-60, 237, 18, &data->dist, 0.0, 100, 0.5, 0.5, "Radius of limiting sphere");
					}
					
					/* disabled soft-distance controls... currently it doesn't work yet. It was intended to be used for soft-ik (see xsi-blog for details) */
#if 0
					uiDefButBitS(block, TOG, LIMITDIST_USESOFT, B_CONSTRAINT_TEST, "Soft", *xco, *yco-82, 50, 18, &data->flag, 0, 24, 0, 0, "Enables soft-distance");
					if (data->flag & LIMITDIST_USESOFT)
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Soft-Distance:", *xco+50, *yco-82, 187, 18, &data->soft, 0.0, 100, 0.5, 0.5, "Distance surrounding radius when transforms should get 'delayed'");
#endif
				uiBlockEndAlign(block);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Clamp Region:",*xco+((width/2)-110), *yco-104,100,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Limit Mode%t|Inside %x0|Outside %x1|Surface %x2", *xco+(width/2), *yco-104, 100, 18, &data->mode, 0, 24, 0, 0, "Distances in relation to sphere of influence to allow");
			}
			break;
		case CONSTRAINT_TYPE_RIGIDBODYJOINT:
			{
				bRigidBodyJointConstraint *data = con->data;
				float extremeLin = 999.f;
				float extremeAngX = 180.f;
				float extremeAngY = 45.f;
				float extremeAngZ = 45.f;
				int togButWidth = 70;
				int offsetY = 150;
				int textButWidth = ((width/2)-togButWidth);
				
				uiDefButI(block, MENU, B_CONSTRAINT_TEST, "Joint Types%t|Ball%x1|Hinge%x2|Cone Twist%x4|Generic (experimental)%x12",//|Extra Force%x6",
												*xco, *yco-25, 150, 18, &data->type, 0, 0, 0, 0, "Choose the joint type");
                height = 140;
				if (data->type==CONSTRAINT_RB_GENERIC6DOF)
					height = 270;
				if (data->type==CONSTRAINT_RB_CONETWIST)
					height = 200;
				
                uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, "");
				
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "toObject:", *xco, *yco-50, 130, 18, &data->tar, "Child Object");
				uiDefButBitS(block, TOG, CONSTRAINT_DRAW_PIVOT, B_CONSTRAINT_TEST, "ShowPivot", *xco+135, *yco-50, 130, 18, &data->flag, 0, 24, 0, 0, "Show pivot position and rotation"); 				
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pivot X:", *xco, *yco-75, 130, 18, &data->pivX, -1000, 1000, 100, 0.0, "Offset pivot on X");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pivot Y:", *xco, *yco-100, 130, 18, &data->pivY, -1000, 1000, 100, 0.0, "Offset pivot on Y");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Pivot Z:", *xco, *yco-125, 130, 18, &data->pivZ, -1000, 1000, 100, 0.0, "Offset pivot on z");
				
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Ax X:", *xco+135, *yco-75, 130, 18, &data->axX, -360, 360, 1500, 0.0, "Rotate pivot on X Axis (in degrees)");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Ax Y:", *xco+135, *yco-100, 130, 18, &data->axY, -360, 360, 1500, 0.0, "Rotate pivot on Y Axis (in degrees)");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Ax Z:", *xco+135, *yco-125, 130, 18, &data->axZ, -360, 360, 1500, 0.0, "Rotate pivot on Z Axis (in degrees)");
				
				if (data->type==CONSTRAINT_RB_GENERIC6DOF) {
					/* Draw Pairs of LimitToggle+LimitValue */
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 1, B_CONSTRAINT_TEST, "LinMinX", *xco, *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-offsetY, (textButWidth-5), 18, &(data->minLimit[0]), -extremeLin, extremeLin, 0.1,0.5,"min x limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 1, B_CONSTRAINT_TEST, "LinMaxX", *xco+(width-(textButWidth-5)-togButWidth), *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-offsetY, (textButWidth), 18, &(data->maxLimit[0]), -extremeLin, extremeLin, 0.1,0.5,"max x limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 2, B_CONSTRAINT_TEST, "LinMinY", *xco, *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-offsetY, (textButWidth-5), 18, &(data->minLimit[1]), -extremeLin, extremeLin, 0.1,0.5,"min y limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 2, B_CONSTRAINT_TEST, "LinMaxY", *xco+(width-(textButWidth-5)-togButWidth), *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-offsetY, (textButWidth), 18, &(data->maxLimit[1]), -extremeLin, extremeLin, 0.1,0.5,"max y limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 4, B_CONSTRAINT_TEST, "LinMinZ", *xco, *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-offsetY, (textButWidth-5), 18, &(data->minLimit[2]), -extremeLin, extremeLin, 0.1,0.5,"min z limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 4, B_CONSTRAINT_TEST, "LinMaxZ", *xco+(width-(textButWidth-5)-togButWidth), *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-offsetY, (textButWidth), 18, &(data->maxLimit[2]), -extremeLin, extremeLin, 0.1,0.5,"max z limit"); 
					uiBlockEndAlign(block);
					offsetY += 20;
				}
				if ((data->type==CONSTRAINT_RB_GENERIC6DOF) || (data->type==CONSTRAINT_RB_CONETWIST)) {
					/* Draw Pairs of LimitToggle+LimitValue */
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 8, B_CONSTRAINT_TEST, "AngMinX", *xco, *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-offsetY, (textButWidth-5), 18, &(data->minLimit[3]), -extremeAngX, extremeAngX, 0.1,0.5,"min x limit"); 
					uiBlockEndAlign(block);
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 8, B_CONSTRAINT_TEST, "AngMaxX", *xco+(width-(textButWidth-5)-togButWidth), *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum x limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-offsetY, (textButWidth), 18, &(data->maxLimit[3]), -extremeAngX, extremeAngX, 0.1,0.5,"max x limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 16, B_CONSTRAINT_TEST, "AngMinY", *xco, *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-offsetY, (textButWidth-5), 18, &(data->minLimit[4]), -extremeAngY, extremeAngY, 0.1,0.5,"min y limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 16, B_CONSTRAINT_TEST, "AngMaxY", *xco+(width-(textButWidth-5)-togButWidth), *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum y limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-offsetY, (textButWidth), 18, &(data->maxLimit[4]), -extremeAngY, extremeAngY, 0.1,0.5,"max y limit"); 
					uiBlockEndAlign(block);
					
					offsetY += 20;
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 32, B_CONSTRAINT_TEST, "AngMinZ", *xco, *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use minimum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+togButWidth, *yco-offsetY, (textButWidth-5), 18, &(data->minLimit[5]), -extremeAngZ, extremeAngZ, 0.1,0.5,"min z limit"); 
					uiBlockEndAlign(block);
					
					uiBlockBeginAlign(block); 
						uiDefButBitS(block, TOG, 32, B_CONSTRAINT_TEST, "AngMaxZ", *xco+(width-(textButWidth-5)-togButWidth), *yco-offsetY, togButWidth, 18, &data->flag, 0, 24, 0, 0, "Use maximum z limit"); 
						uiDefButF(block, NUM, B_CONSTRAINT_TEST, "", *xco+(width-textButWidth-5), *yco-offsetY, (textButWidth), 18, &(data->maxLimit[5]), -extremeAngZ, extremeAngZ, 0.1,0.5,"max z limit"); 
					uiBlockEndAlign(block);
				}
			}
			break;
		case CONSTRAINT_TYPE_CLAMPTO:
			{
				bClampToConstraint *data = con->data;
				
				height = 90;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Main Axis:", *xco, *yco-64, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "Auto", *xco+100, *yco-64, 50, 18, &data->flag, 12.0, CLAMPTO_AUTO, 0, 0, "Automatically determine main-axis of movement");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "X", *xco+150, *yco-64, 32, 18, &data->flag, 12.0, CLAMPTO_X, 0, 0, "Main axis of movement is x-axis");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "Y", *xco+182, *yco-64, 32, 18, &data->flag, 12.0, CLAMPTO_Y, 0, 0, "Main axis of movement is y-axis");
					uiDefButI(block, ROW, B_CONSTRAINT_TEST, "Z", *xco+214, *yco-64, 32, 18, &data->flag, 12.0, CLAMPTO_Z, 0, 0, "Main axis of movement is z-axis");
				uiBlockEndAlign(block);
				
				/* Extra Options Controlling Behaviour */
				//uiBlockBeginAlign(block);
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Options:", *xco, *yco-88, 90, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButBitI(block, TOG, CLAMPTO_CYCLIC, B_CONSTRAINT_TEST, "Cyclic", *xco+((width/2)), *yco-88,60,19, &data->flag2, 0, 0, 0, 0, "Treat curve as cyclic curve (no clamping to curve bounding box)");
				//uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_TRANSFORM:
			{
				bTransformConstraint *data = con->data;
				float fmin, fmax, tmin, tmax;
				
				height = 178;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
				
				/* Draw target parameters */			
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* Draw target parameters */
				uiBlockBeginAlign(block);
					uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object to use as Parent"); 
					
					if (is_armature_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone to use as Parent");
						uiButSetCompleteFunc(but, autocomplete_bone, (void *)data->tar);
					}
					else if (is_geom_target(data->tar)) {
						but= uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "VG:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Name of Vertex Group defining 'target' points");
						uiButSetCompleteFunc(but, autocomplete_vgroup, (void *)data->tar);
					}
					else {
						strcpy(data->subtarget, "");
					}
				uiBlockEndAlign(block);
				
				/* Extrapolate Ranges? */
				uiDefButBitC(block, TOG, 1, B_CONSTRAINT_TEST, "Extrapolate", *xco-10, *yco-42,80,19, &data->expo, 0, 0, 0, 0, "Extrapolate ranges");
				
				/* Draw options for source motion */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Source:", *xco-10, *yco-62, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* 	draw Loc/Rot/Size toggles 	*/
				uiBlockBeginAlign(block);
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Loc", *xco-5, *yco-82, 45, 18, &data->from, 12.0, 0, 0, 0, "Use Location transform channels from Target");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Rot", *xco+40, *yco-82, 45, 18, &data->from, 12.0, 1, 0, 0, "Use Rotation transform channels from Target");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Scale", *xco+85, *yco-82, 45, 18, &data->from, 12.0, 2, 0, 0, "Use Scale transform channels from Target");
				uiBlockEndAlign(block);
				
				/* Draw Pairs of Axis: Min/Max Value*/
				if (data->from == 2) {
					fmin= 0.0001;
					fmax= 1000.0;
				}
				else if (data->from == 1) {
					fmin= -360.0;
					fmax= 360.0;
				}
				else {
					fmin = -1000.0;
					fmax= 1000.0;
				}
				
				uiBlockBeginAlign(block); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "X:", *xco-10, *yco-107, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", *xco+20, *yco-107, 55, 18, &data->from_min[0], fmin, fmax, 0, 0, "Bottom of range of x-axis source motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", *xco+75, *yco-107, 55, 18, &data->from_max[0], fmin, fmax, 0, 0, "Top of range of x-axis source motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Y:", *xco-10, *yco-127, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", *xco+20, *yco-127, 55, 18, &data->from_min[1], fmin, fmax, 0, 0, "Bottom of range of y-axis source motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", *xco+75, *yco-127, 55, 18, &data->from_max[1], fmin, fmax, 0, 0, "Top of range of y-axis source motion for source->target mapping"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Z:", *xco-10, *yco-147, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", *xco+20, *yco-147, 55, 18, &data->from_min[2], fmin, fmax, 0, 0, "Bottom of range of z-axis source motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", *xco+75, *yco-147, 55, 18, &data->from_max[2], fmin, fmax, 0, 0, "Top of range of z-axis source motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				
				/* Draw options for target motion */
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Destination:", *xco+150, *yco-62, 150, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				/* 	draw Loc/Rot/Size toggles 	*/
				uiBlockBeginAlign(block);
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Loc", *xco+150, *yco-82, 45, 18, &data->to, 12.0, 0, 0, 0, "Use as Location transform");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Rot", *xco+195, *yco-82, 45, 18, &data->to, 12.0, 1, 0, 0, "Use as Rotation transform");
					uiDefButS(block, ROW, B_CONSTRAINT_TEST, "Scale", *xco+245, *yco-82, 45, 18, &data->to, 12.0, 2, 0, 0, "Use as Scale transform");
				uiBlockEndAlign(block);
				
				/* Draw Pairs of Source-Axis: Min/Max Value*/
				if (data->to == 2) {
					tmin= 0.0001;
					tmax= 1000.0;
				}
				else if (data->to == 1) {
					tmin= -360.0;
					tmax= 360.0;
				}
				else {
					tmin = -1000.0;
					tmax= 1000.0;
				}
				
				uiBlockBeginAlign(block); 
					uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Axis Mapping%t|X->X%x0|Y->X%x1|Z->X%x2", *xco+150, *yco-107, 40, 18, &data->map[0], 0, 24, 0, 0, "Specify which source axis the x-axis destination uses");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", *xco+175, *yco-107, 50, 18, &data->to_min[0], tmin, tmax, 0, 0, "Bottom of range of x-axis destination motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", *xco+240, *yco-107, 50, 18, &data->to_max[0], tmin, tmax, 0, 0, "Top of range of x-axis destination motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				uiBlockBeginAlign(block); 
					uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Axis Mapping%t|X->Y%x0|Y->Y%x1|Z->Y%x2", *xco+150, *yco-127, 40, 18, &data->map[1], 0, 24, 0, 0, "Specify which source axis the y-axis destination uses");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", *xco+175, *yco-127, 50, 18, &data->to_min[1], tmin, tmax, 0, 0, "Bottom of range of y-axis destination motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", *xco+240, *yco-127, 50, 18, &data->to_max[1], tmin, tmax, 0, 0, "Top of range of y-axis destination motion for source->target mapping"); 
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block); 
					uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Axis Mapping%t|X->Z%x0|Y->Z%x1|Z->Z%x2", *xco+150, *yco-147, 40, 18, &data->map[2], 0, 24, 0, 0, "Specify which source axis the z-axis destination uses");
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "min", *xco+175, *yco-147, 50, 18, &data->to_min[2], tmin, tmax, 0, 0, "Bottom of range of z-axis destination motion for source->target mapping"); 
					uiDefButF(block, NUM, B_CONSTRAINT_TEST, "max", *xco+240, *yco-147, 50, 18, &data->to_max[2], tmin, tmax, 0, 0, "Top of range of z-axis destination motion for source->target mapping"); 
				uiBlockEndAlign(block); 
				
				/* constraint space settings */
				draw_constraint_spaceselect(block, con, *xco, *yco-170, is_armature_owner(ob), is_armature_target(data->tar));
			}
			break;
		case CONSTRAINT_TYPE_NULL:
			{
				height = 17;
				uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-10, *yco-height, width+40,height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
			}
			break;
		default:
			height = 0;
			break;
		}
		
		(*yco)-=(24+height);
	}

	if (ELEM(con->type, CONSTRAINT_TYPE_NULL, CONSTRAINT_TYPE_RIGIDBODYJOINT)==0) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_CONSTRAINT_INF, "Influence ", *xco, *yco, 197, 20, &(con->enforce), 0.0, 1.0, 0.0, 0.0, "Amount of influence this constraint will have on the final solution");
		but = uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Show", *xco+200, *yco, 45, 20, 0, 0.0, 1.0, 0.0, 0.0, "Show constraint's ipo in the Ipo window, adds a channel if not there");
		/* If this is on an object or bone, add ipo channel the constraint */
		uiButSetFunc (but, enable_constraint_ipo_func, ob, con);
		but = uiDefBut(block, BUT, B_CONSTRAINT_TEST, "Key", *xco+245, *yco, 40, 20, 0, 0.0, 1.0, 0.0, 0.0, "Add an influence keyframe to the constraint");
		/* Add a keyframe to the influence IPO */
		uiButSetFunc (but, add_influence_key_to_constraint_func, ob, con);
		uiBlockEndAlign(block);
		(*yco)-=24;
	} 
	else {
		(*yco)-=3;
	}
	
	/* clear any locks set up for proxies/lib-linking */
	uiClearButLock();
}

static uiBlock *add_constraintmenu(void *arg_unused)
{
	Object *ob= OBACT;
	uiBlock *block;
	ListBase *conlist;
	short yco= 0;
	
	conlist = get_active_constraints(ob);
	
	block= uiNewBlock(&curarea->uiblocks, "add_constraintmenu", UI_EMBOSSP, UI_HELV, curarea->win);

	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_CHILDOF, "Child Of",			0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_TRANSFORM, "Transformation", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_LOCLIKE, "Copy Location", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_ROTLIKE, "Copy Rotation", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_SIZELIKE, "Copy Scale", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_LOCLIMIT, "Limit Location", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_ROTLIMIT, "Limit Rotation", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_SIZELIMIT, "Limit Scale", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_DISTLIMIT, "Limit Distance", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_TRACKTO, "Track To",	0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_MINMAX, "Floor",	0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_LOCKTRACK, "Locked Track", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_FOLLOWPATH, "Follow Path", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
		
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_CLAMPTO, "Clamp To", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_STRETCHTO, "Stretch To", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");

	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_RIGIDBODYJOINT, "Rigid Body Joint", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");//rcruiz

	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if (ob->flag & OB_POSEMODE) {
		uiDefBut(block, BUTM, B_CONSTRAINT_ADD_KINEMATIC, "IK Solver", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	}
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_ACTION, "Action", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_PYTHON, "Script", 0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_NULL, "Null",	0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	
	uiTextBoundsBlock(block, 50);
	uiBlockSetDirection(block, UI_DOWN);
		
	return block;
}

void do_constraintbuts(unsigned short event)
{
	Object *ob= OBACT;
	bConstraint *con;
	
	switch(event) {
	case B_CONSTRAINT_TEST:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;  // no handling
	case B_CONSTRAINT_INF:
		/* influence; do not execute actions for 1 dag_flush */
		if (ob->pose)
			ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);

	case B_CONSTRAINT_CHANGETARGET:
		if (ob->pose) ob->pose->flag |= POSE_RECALC;	// checks & sorts pose channels
		DAG_scene_sort(G.scene);
		break;
	
	case B_CONSTRAINT_ADD_NULL:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_NULL);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_PYTHON:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_PYTHON);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;	
	case B_CONSTRAINT_ADD_KINEMATIC:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_CHILDOF:
		{
			con= add_new_constraint(CONSTRAINT_TYPE_CHILDOF);
			add_constraint_to_active(ob, con);
			
			/* if this constraint is being added to a posechannel, make sure
			 * the constraint gets evaluated in pose-space
			 */
			if (ob->flag & OB_POSEMODE) {
				con->ownspace = CONSTRAINT_SPACE_POSE;
				con->flag |= CONSTRAINT_SPACEONCE;
			}
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_TRACKTO:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_TRACKTO);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_MINMAX:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_MINMAX);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_ROTLIKE:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_ROTLIKE);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_LOCLIKE:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_LOCLIKE);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
 	case B_CONSTRAINT_ADD_SIZELIKE:
 		{
 			con = add_new_constraint(CONSTRAINT_TYPE_SIZELIKE);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_ACTION:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_ACTION);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_LOCKTRACK:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_LOCKTRACK);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_FOLLOWPATH:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_STRETCHTO:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_STRETCHTO);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_LOCLIMIT:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_LOCLIMIT);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_ROTLIMIT:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_ROTLIMIT);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_SIZELIMIT:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_SIZELIMIT);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_RIGIDBODYJOINT:
		{
			bRigidBodyJointConstraint *data;
			Base *base_iter;
			
			con = add_new_constraint(CONSTRAINT_TYPE_RIGIDBODYJOINT);
			add_constraint_to_active(ob, con);
			
			/* set selected first object as target - moved from new_constraint_data */
			data = (bRigidBodyJointConstraint*)con->data;
			base_iter = G.scene->base.first;
            while ( base_iter && !data->tar ) {
                if( ( ( base_iter->flag & SELECT ) &&
//                     ( base_iter->lay & G.vd->lay ) ) &&
                    ( base_iter != G.scene->basact ) ))
				{
                        data->tar=base_iter->object;
						break;
				}
                base_iter = base_iter->next;
            }
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_CLAMPTO:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_CLAMPTO);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_TRANSFORM:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_TRANSFORM);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_DISTLIMIT:
		{
			con = add_new_constraint(CONSTRAINT_TYPE_DISTLIMIT);
			add_constraint_to_active(ob, con);
			
			BIF_undo_push("Add constraint");
		}
		break;

	default:
		break;
	}

	object_test_constraints(ob);
	
	if(ob->pose) update_pose_constraint_flags(ob->pose);
	
	if(ob->type==OB_ARMATURE) DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA|OB_RECALC_OB);
	else DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
}

void pointcache_bake(PTCacheID *pid, int startframe)
{
	Base *base;
	ScrArea *sa;
    PointCache *cache;
	ListBase pidlist;
	float frameleno= G.scene->r.framelen;
	int cfrao= CFRA, didbreak =0, endframe, cstart, cend;
	
	G.scene->r.framelen= 1.0;		// baking has to be in uncorrected time
	sbSetInterruptCallBack(blender_test_break); // make softbody module ESC aware
	G.afbreek=0; // init global break system
	
	if(pid) {
	    cache= pid->cache;

		BKE_ptcache_id_time(pid, 0.0f, &cstart, &cend, NULL);

		startframe= startframe;
		endframe= cend;

		cache->flag |= PTCACHE_BAKING;
		cache->flag &= ~PTCACHE_BAKED;
	}
	else {
		startframe= MAXFRAME;
		endframe= 0;

		for(base=G.scene->base.first; base; base= base->next) {
			if(TESTBASELIB(base)) {
				BKE_ptcache_ids_from_object(&pidlist, base->object);

				for(pid=pidlist.first; pid; pid=pid->next) {
					cache= pid->cache;

					BKE_ptcache_id_time(pid, 0.0f, &cstart, &cend, NULL);

					startframe= MIN2(startframe, cstart);
					endframe= MAX2(endframe, cend);

					cache->flag |= PTCACHE_BAKING;
					cache->flag &= ~PTCACHE_BAKED;
				}

				BLI_freelistN(&pidlist);
			}
		}
	}
	
	CFRA= startframe;
	update_for_newframe_muted();	// put everything on this frame
	
	curarea->win_swap= 0;		// clean swapbuffers
	
	for(; CFRA <= endframe; CFRA++) {
		set_timecursor(CFRA);
		
		update_for_newframe_muted();
		
		for(sa= G.curscreen->areabase.first; sa; sa= sa->next)
			if(sa->spacetype == SPACE_VIEW3D)
				scrarea_do_windraw(sa);
		screen_swapbuffers();

		//blender_test_break() has a granularity of 10 ms, who cares .. baking the unit cube is kinda boring
		if(blender_test_break()) {
			didbreak = 1;
			break;
		}
	}

	if(didbreak && G.qual!=LR_SHIFTKEY) {
		/* failed to bake, free the frames we baked */
		if(pid) {
			cache= pid->cache;

			BKE_ptcache_id_time(pid, 0.0f, &cstart, &cend, NULL);

			cache->flag &= ~PTCACHE_BAKING;
			BKE_ptcache_id_reset(pid, PTCACHE_RESET_OUTDATED);
		}
		else {
			for(base=G.scene->base.first; base; base= base->next) {
				if(TESTBASELIB(base)) {
					BKE_ptcache_ids_from_object(&pidlist, base->object);

					for(pid=pidlist.first; pid; pid=pid->next) {
						cache= pid->cache;

						BKE_ptcache_id_time(pid, 0.0f, &cstart, &cend, NULL);

						cache->flag &= ~PTCACHE_BAKING;

						BKE_ptcache_id_reset(pid, PTCACHE_RESET_OUTDATED);
					}

					BLI_freelistN(&pidlist);
				}
			}
		}
	}
	else {
		/* succesfully finished baking */
		if(pid) {
			cache= pid->cache;

			cache->flag &= ~PTCACHE_BAKING;
			cache->flag |= PTCACHE_BAKED;
		}
		else {
			for(base=G.scene->base.first; base; base= base->next) {
				if(TESTBASELIB(base)) {
					BKE_ptcache_ids_from_object(&pidlist, base->object);

					for(pid=pidlist.first; pid; pid=pid->next) {
						cache= pid->cache;

						cache->flag &= ~PTCACHE_BAKING;
						cache->flag |= PTCACHE_BAKED;
					}

					BLI_freelistN(&pidlist);
				}
			}
		}
	}
	
	/* restore */
	waitcursor(0);
	sbSetInterruptCallBack(NULL); // softbody module won't ESC
	G.afbreek=0; // reset global break system

	CFRA= cfrao;
	G.scene->r.framelen= frameleno;
	update_for_newframe_muted();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

void pointcache_free(PTCacheID *pid, int cacheonly)
{
	Base *base;
	ListBase pidlist;
	
	if(pid) {
		if(cacheonly) {
			BKE_ptcache_id_reset(pid, PTCACHE_RESET_DEPSGRAPH);
		}
		else {
			BKE_ptcache_id_reset(pid, PTCACHE_RESET_BAKED);
			pid->cache->flag &= ~PTCACHE_BAKED;
		}

		DAG_object_flush_update(G.scene, pid->ob, OB_RECALC_DATA);
	}
	else {
		for(base=G.scene->base.first; base; base= base->next) {
			if(TESTBASELIB(base)) {
				BKE_ptcache_ids_from_object(&pidlist, base->object);

				for(pid=pidlist.first; pid; pid=pid->next) {
					if(cacheonly) {
						BKE_ptcache_id_reset(pid, PTCACHE_RESET_DEPSGRAPH);
					}
					else {
						BKE_ptcache_id_reset(pid, PTCACHE_RESET_BAKED);
						pid->cache->flag &= ~PTCACHE_BAKED;
					}

					DAG_object_flush_update(G.scene, pid->ob, OB_RECALC_DATA);
				}

				BLI_freelistN(&pidlist);
			}
		}
	}

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}

// store processed path & file prefix for fluidsim bake directory
void fluidsimFilesel(char *selection)
{
	Object *ob = OBACT;
	char srcDir[FILE_MAXDIR+FILE_MAXFILE], srcFile[FILE_MAXFILE];
	char prefix[FILE_MAXFILE];
	char *srch, *srchSub, *srchExt, *lastFound;
	int isElbeemSurf = 0;

	// make prefix
	strcpy(srcDir, selection);
	BLI_splitdirstring(srcDir, srcFile);
	strcpy(prefix, srcFile);
	// check if this is a previously generated surface mesh file
	srch = strstr(prefix, "fluidsurface_");
	// TODO search from back...
	if(srch) {
		srchSub = strstr(prefix,"_preview_");
		if(!srchSub) srchSub = strstr(prefix,"_final_");
		srchExt = strstr(prefix,".gz.bobj");
		if(!srchExt) srchExt = strstr(prefix,".bobj");
		if(srchSub && srchExt) {
			*srch = '\0';
			isElbeemSurf = 1;
		}
	}
	if(!isElbeemSurf) {
		// try to remove suffix
		lastFound = NULL;
		srch = strchr(prefix, '.'); // search last . from extension
		while(srch) {
			lastFound = srch;
			if(srch) {
				srch++;
				srch = strchr(srch, '.');
			}
		}
		if(lastFound) {
			*lastFound = '\0';
		} 
	}

	if(ob->fluidsimSettings) {
		strcpy(ob->fluidsimSettings->surfdataPath, srcDir);
		//not necessary? strcat(ob->fluidsimSettings->surfdataPath, "/");
		strcat(ob->fluidsimSettings->surfdataPath, prefix);

		// redraw view & buttons...
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	}
}

void do_object_panels(unsigned short event)
{
	Object *ob;
	
	ob= OBACT;
	if(ob==NULL)
		return;
	
	switch(event) {
	case B_TRACKBUTS:
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RECALCPATH:
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_DUPLI_FRAME:
		ob->transflag &= ~(OB_DUPLIVERTS|OB_DUPLIFACES|OB_DUPLIGROUP);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_DUPLI_VERTS:
		ob->transflag &= ~(OB_DUPLIFRAMES|OB_DUPLIFACES|OB_DUPLIGROUP);
		DAG_scene_sort(G.scene);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA|OB_RECALC_OB);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_DUPLI_FACES:
		ob->transflag &= ~(OB_DUPLIVERTS|OB_DUPLIFRAMES|OB_DUPLIGROUP);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_DUPLI_GROUP:
		ob->transflag &= ~(OB_DUPLIVERTS|OB_DUPLIFRAMES|OB_DUPLIFACES);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
		
	case B_PRINTSPEED:
		{
			float vec[3];
			CFRA++;
			do_ob_ipo(ob);
			where_is_object(ob);
			VECCOPY(vec, ob->obmat[3]);
			CFRA--;
			do_ob_ipo(ob);
			where_is_object(ob);
			VecSubf(vec, vec, ob->obmat[3]);
			prspeed= Normalize(vec);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_PRINTLEN:
		if(ob->type==OB_CURVE) {
			Curve *cu=ob->data;
			
			if(cu->path) prlen= cu->path->totdist; else prlen= -1.0;
			scrarea_queue_winredraw(curarea);
		} 
		break;
	case B_RELKEY:
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWIPO, 0);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		break;
	case B_CURVECHECK:
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;
	
	case B_SOFTBODY_DEL_VG:
		if(ob->soft) {
			ob->soft->vertgroup= 0;
			//ob->softflag |= OB_SB_REDO;
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA); 
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_FLUIDSIM_BAKE:
		/* write config files (currently no simulation) */
		fluidsimBake(ob);
		break;
	case B_FLUIDSIM_MAKEPART:
		if(ob==NULL || ob->type!=OB_MESH) break;
		else {
			ParticleSettings *part = psys_new_settings("PSys", G.main);
			ParticleSystem *psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
			ModifierData *md;
			ParticleSystemModifierData *psmd;

			part->type = PART_FLUID;
			psys->part = part;
			psys->pointcache = BKE_ptcache_add();
			psys->flag |= PSYS_ENABLED;

			ob->fluidsimSettings->type = OB_FLUIDSIM_PARTICLE;

			BLI_addtail(&ob->particlesystem,psys);

			md= modifier_new(eModifierType_ParticleSystem);
			sprintf(md->name, "FluidParticleSystem" );
			psmd= (ParticleSystemModifierData*) md;
			psmd->psys=psys;
			BLI_addtail(&ob->modifiers, md);
		}
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_FLUIDSIM_CHANGETYPE:
		if(ob && ob->particlesystem.first && ob->fluidsimSettings->type!=OB_FLUIDSIM_PARTICLE){
			ParticleSystem *psys;
			for(psys=ob->particlesystem.first; psys; psys=psys->next) {
				if(psys->part->type==PART_FLUID) {
					/* clear modifier */
					ParticleSystemModifierData *psmd= psys_get_modifier(ob,psys);
					BLI_remlink(&ob->modifiers, psmd);
					modifier_free((ModifierData *)psmd);

					/* clear particle system */
					BLI_remlink(&ob->particlesystem,psys);
					psys_free(ob,psys);

					BIF_undo_push("Delete particle system");

					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

					allqueue(REDRAWVIEW3D, 0);
					allqueue(REDRAWOOPS, 0);
					break;
				}
			}
		}
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_FLUIDSIM_SELDIR: 
		{
			ScrArea *sa = closest_bigger_area();
			/* choose dir for surface files */
			areawinset(sa->win);
			activate_fileselect(FILE_SPECIAL, "Select Directory", ob->fluidsimSettings->surfdataPath, fluidsimFilesel);
		}
		break;
	case B_FLUIDSIM_FORCEREDRAW:
		/* force redraw */
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		break;
	case B_GROUP_RELINK:
		group_relink_nla_objects(ob);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_OBJECT_IPOFLAG:
		if(ob->ipo) ob->ipo->showkey= (ob->ipoflag & OB_DRAWKEY)?1:0;
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_CLOTH_CHANGEPREROLL:
	{
		ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
		if(clmd)
		{
			PTCacheID pid;

			BKE_ptcache_id_from_cloth(&pid, ob, clmd);

			// do nothing in editmode
			if(pid.cache->flag & PTCACHE_BAKE_EDIT_ACTIVE)
				break;
			
			CFRA= 1;
			update_for_newframe_muted();
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA); 
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	break;	
	case B_BAKE_CACHE_CHANGE:
	{
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA); 
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	break;
	}

}

static void do_add_groupmenu(void *arg, int event)
{
	Object *ob= OBACT;
	
	if(ob) {
		
		if(event== -1) {
			Group *group= add_group( "Group" );
			add_to_group(group, ob);
		}
		else
			add_to_group(BLI_findlink(&G.main->group, event), ob);
			
		ob->flag |= OB_FROMGROUP;
		BASACT->flag |= OB_FROMGROUP;
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
	}		
}

static uiBlock *add_groupmenu(void *arg_unused)
{
	uiBlock *block;
	Group *group;
	short xco=0, yco= 0, index=0;
	char str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "add_constraintmenu", UI_EMBOSSP, UI_HELV, curarea->win);
	uiBlockSetButmFunc(block, do_add_groupmenu, NULL);

	uiDefBut(block, BUTM, B_NOP, "ADD NEW",		0, 20, 160, 19, NULL, 0.0, 0.0, 1, -1, "");
	for(group= G.main->group.first; group; group= group->id.next, index++) {
		
		/*if(group->id.lib) strcpy(str, "L  ");*/ /* we cant allow adding objects inside linked groups, it wont be saved anyway */
		if(group->id.lib==0) {
			strcpy(str, "   ");
			strcat(str, group->id.name+2);
			uiDefBut(block, BUTM, B_NOP, str,	xco*160, -20*yco, 160, 19, NULL, 0.0, 0.0, 1, index, "");
			
			yco++;
			if(yco>24) {
				yco= 0;
				xco++;
			}
		}
	}
	
	uiTextBoundsBlock(block, 50);
	uiBlockSetDirection(block, UI_DOWN);	
	
	return block;
}

static void group_ob_rem(void *gr_v, void *ob_v)
{
	Object *ob= OBACT;
	
	if(rem_from_group(gr_v, ob) && find_group(ob, NULL)==NULL) {
		ob->flag &= ~OB_FROMGROUP;
		BASACT->flag &= ~OB_FROMGROUP;
	}
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWVIEW3D, 0);

}

static void group_local(void *gr_v, void *unused)
{
	Group *group= gr_v;
	
	group->id.lib= NULL;
	
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWVIEW3D, 0);
	
}

static void object_panel_object(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	Group *group;
	int a, xco, yco=0;
	short dx= 33, dy= 30;
	int is_libdata = object_is_libdata(ob);
	block= uiNewBlock(&curarea->uiblocks, "object_panel_object", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Object and Links", "Object", 0, 0, 318, 204)==0) return;
	
	
	
	/* object name */
	uiBlockSetCol(block, TH_BUT_SETTING2);
	uiSetButLock(is_libdata, ERROR_LIBDATA_MESSAGE);
	xco= std_libbuttons(block, 10, 180, 0, NULL, 0, ID_OB, 0, &ob->id, NULL, &(G.buts->menunr), B_OBALONE, B_OBLOCAL, 0, 0, B_KEEPDATA);
	uiBlockSetCol(block, TH_AUTO);
	
	/* parent */
	uiSetButLock(is_libdata, ERROR_LIBDATA_MESSAGE);
	uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_OBJECTPANELPARENT, "Par:", xco+5, 180, 305-xco, 20, &ob->parent, "Parent Object"); 
	
	uiSetButLock(is_libdata, ERROR_LIBDATA_MESSAGE);
	but = uiDefButS(block, NUM, B_NOP, "PassIndex:",		xco+5, 150, 305-xco, 20, &ob->index, 0.0, 1000.0, 0, 0, "Index # for the IndexOB render pass.");
	
	uiSetButLock(1, NULL);
	uiDefBlockBut(block, add_groupmenu, NULL, "Add to Group", 10,150,150,20, "Add Object to a new Group");

	/* all groups */
	for(group= G.main->group.first; group; group= group->id.next) {
		if(object_in_group(ob, group)) {
			xco= 160;
			
			uiBlockBeginAlign(block);
			uiSetButLock(GET_INT_FROM_POINTER(group->id.lib), ERROR_LIBDATA_MESSAGE); /* We cant actually use this button */
			but = uiDefBut(block, TEX, B_IDNAME, "GR:",	10, 120-yco, 150, 20, group->id.name+2, 0.0, 21.0, 0, 0, "Displays Group name. Click to change.");
			uiButSetFunc(but, test_idbutton_cb, group->id.name, NULL);
			uiClearButLock();
			
			if(group->id.lib) {
				but= uiDefIconBut(block, BUT, B_NOP, ICON_PARLIB, 160, 120-yco, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Make Group local");
				uiButSetFunc(but, group_local, group, NULL);
				xco= 180;
			} else { /* cant remove objects from linked groups */
				but = uiDefIconBut(block, BUT, B_NOP, VICON_X, xco, 120-yco, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Remove Group membership");
				uiButSetFunc(but, group_ob_rem, group, ob);
			}
			
			yco+= 20;
			xco= 10;
			
			/* layers */
			uiSetButLock(GET_INT_FROM_POINTER(group->id.lib), ERROR_LIBDATA_MESSAGE);
			uiBlockBeginAlign(block);
			for(a=0; a<5; a++)
				uiDefButBitI(block, TOG, 1<<a, REDRAWVIEW3D, "",	(short)(xco+a*(dx/2)), 120-yco, (short)(dx/2), (short)(dy/2), (int *)&(group->layer), 0, 0, 0, 0, "");
			for(a=0; a<5; a++)
				uiDefButBitI(block, TOG, 1<<(a+10), REDRAWVIEW3D, "",	(short)(xco+a*(dx/2)), 105-yco, (short)(dx/2), (short)(dy/2), (int *)&(group->layer), 0, 0, 0, 0, "");
			
			xco+= 7;
			uiBlockBeginAlign(block);
			for(a=5; a<10; a++)
				uiDefButBitI(block, TOG, 1<<a, REDRAWVIEW3D, "",	(short)(xco+a*(dx/2)), 120-yco, (short)(dx/2), (short)(dy/2), (int *)&(group->layer), 0, 0, 0, 0, "");
			for(a=5; a<10; a++)
				uiDefButBitI(block, TOG, 1<<(a+10), REDRAWVIEW3D, "",	(short)(xco+a*(dx/2)), 105-yco, (short)(dx/2), (short)(dy/2), (int *)&(group->layer), 0, 0, 0, 0, "");
			
			uiBlockEndAlign(block);
			uiClearButLock();

			yco+= 40;
		}
	}
	
	if(120-yco < -10)
		uiNewPanelHeight(block, 204 - (120-yco));
}

static void object_panel_anim_timeoffset_callback( void *data, void *timeoffset_ui) {
	Object *ob = (Object *)data;
	ob->sf = (*(float *)timeoffset_ui) - (give_timeoffset(ob) - ob->sf);
}

static void object_panel_anim(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	static float timeoffset_ui;
	char str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_anim", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim settings", "Object", 320, 0, 318, 204)==0) return;
	
	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,B_TRACKBUTS,"TrackX",	24,180,59,19, &ob->trackflag, 12.0, 0.0, 0, 0, "Specify the axis that points to another object");
	uiDefButS(block, ROW,B_TRACKBUTS,"Y",		85,180,19,19, &ob->trackflag, 12.0, 1.0, 0, 0, "Specify the axis that points to another object");
	uiDefButS(block, ROW,B_TRACKBUTS,"Z",		104,180,19,19, &ob->trackflag, 12.0, 2.0, 0, 0, "Specify the axis that points to another object");
	uiDefButS(block, ROW,B_TRACKBUTS,"-X",		124,180,24,19, &ob->trackflag, 12.0, 3.0, 0, 0, "Specify the axis that points to another object");
	uiDefButS(block, ROW,B_TRACKBUTS,"-Y",		150,180,24,19, &ob->trackflag, 12.0, 4.0, 0, 0, "Specify the axis that points to another object");
	uiDefButS(block, ROW,B_TRACKBUTS,"-Z",		178,180,24,19, &ob->trackflag, 12.0, 5.0, 0, 0, "Specify the axis that points to another object");
	uiBlockBeginAlign(block);
	uiDefButS(block, ROW,REDRAWVIEW3D,"UpX",	226,180,45,19, &ob->upflag, 13.0, 0.0, 0, 0, "Specify the axis that points up");
	uiDefButS(block, ROW,REDRAWVIEW3D,"Y",		274,180,20,19, &ob->upflag, 13.0, 1.0, 0, 0, "Specify the axis that points up");
	uiDefButS(block, ROW,REDRAWVIEW3D,"Z",		298,180,19,19, &ob->upflag, 13.0, 2.0, 0, 0, "Specify the axis that points up");
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, OB_DRAWKEY, B_OBJECT_IPOFLAG, "Draw Key",		24,155,71,19, &ob->ipoflag, 0, 0, 0, 0, "Draw object as key position");
	uiDefButBitS(block, TOG, OB_DRAWKEYSEL, REDRAWVIEW3D, "Draw Key Sel",	97,155,81,19, &ob->ipoflag, 0, 0, 0, 0, "Limit the drawing of object keys");
	uiDefButBitS(block, TOG, OB_POWERTRACK, REDRAWVIEW3D, "Powertrack",		180,155,78,19, &ob->transflag, 0, 0, 0, 0, "Switch objects rotation off");
	uiDefButBitS(block, TOG, PARSLOW, 0, "SlowPar",					260,155,56,19, &ob->partype, 0, 0, 0, 0, "Create a delay in the parent relationship");
	uiBlockBeginAlign(block);
	
	uiDefButBitS(block, TOG, OB_DUPLIFRAMES, B_DUPLI_FRAME, "DupliFrames",	24,130,95,20, &ob->transflag, 0, 0, 0, 0, "Make copy of object for every frame");
	uiDefButBitS(block, TOG, OB_DUPLIVERTS, B_DUPLI_VERTS, "DupliVerts",		119,130,95,20, &ob->transflag, 0, 0, 0, 0, "Duplicate child objects on all vertices");
	uiDefButBitS(block, TOG, OB_DUPLIFACES, B_DUPLI_FACES, "DupliFaces",		214,130,102,20, &ob->transflag, 0, 0, 0, 0, "Duplicate child objects on all faces");
	uiDefButBitS(block, TOG, OB_DUPLIGROUP, B_DUPLI_GROUP, "DupliGroup",		24,110,150,20, &ob->transflag, 0, 0, 0, 0, "Enable group instancing");
	if(ob->transflag & OB_DUPLIFRAMES) {
		uiDefButBitS(block, TOG, OB_DUPLINOSPEED, REDRAWVIEW3D, "No Speed",		174,110,142,20, &ob->transflag, 0, 0, 0, 0, "Set dupliframes to still, regardless of frame");
	} else if(ob->transflag & OB_DUPLIVERTS) {
		uiDefButBitS(block, TOG, OB_DUPLIROT, REDRAWVIEW3D, "Rot",				174,110,142,20, &ob->transflag, 0, 0, 0, 0, "Rotate dupli according to vertex normal");
	} else if(ob->transflag & OB_DUPLIFACES) {
		uiDefButBitS(block, TOG, OB_DUPLIFACES_SCALE, REDRAWVIEW3D, "Scale",			174,110,80,20, &ob->transflag, 0, 0, 0, 0, "Scale dupli based on face size");
		uiDefButF(block, NUM, REDRAWVIEW3D, "",		254,110,62,20, &ob->dupfacesca, 0.001, 10000.0, 0, 0, "Scale the DupliFace objects");
	} else {
		uiDefIDPoinBut(block, test_grouppoin_but, ID_GR, B_GROUP_RELINK, "GR:",	174,110,142,20, &ob->dup_group, "Instance an existing group");
	}
	
	uiBlockBeginAlign(block);
	/* DupSta and DupEnd are both shorts, so the maxframe is greater then their range
	just limit the buttons to the max short */
	uiDefButI(block, NUM, REDRAWVIEW3D, "DupSta:",		24,85,141,19, &ob->dupsta, 1.0, 32767, 0, 0, "Specify startframe for Dupliframes");
	uiDefButI(block, NUM, REDRAWVIEW3D, "DupOn:",		170,85,146,19, &ob->dupon, 1.0, 1500.0, 0, 0, "Specify the number of frames to use between DupOff frames");
	uiDefButI(block, NUM, REDRAWVIEW3D, "DupEnd",		24,65,140,19, &ob->dupend, 1.0, 32767, 0, 0, "Specify endframe for Dupliframes");
	uiDefButI(block, NUM, REDRAWVIEW3D, "DupOff",		171,65,145,19, &ob->dupoff, 0.0, 1500.0, 0, 0, "Specify recurring frames to exclude from the Dupliframes");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	
	timeoffset_ui = give_timeoffset(ob);
	but = uiDefButF(block, NUM, REDRAWALL, "TimeOffset:",			24,35,115,20, &timeoffset_ui, -MAXFRAMEF, MAXFRAMEF, 100, 0, "Animation offset in frames for ipo's and dupligroup instances");
	uiButSetFunc(but, object_panel_anim_timeoffset_callback, ob, &timeoffset_ui);
	
	uiDefBut(block, BUT, B_AUTOTIMEOFS, "Auto",	139,35,34,20, 0, 0, 0, 0, 0, "Assign selected objects a timeoffset within a range, starting from the active object");
	uiDefBut(block, BUT, B_OFSTIMEOFS, "Ofs",	173,35,34,20, 0, 0, 0, 0, 0, "Offset selected objects timeoffset");
	uiDefBut(block, BUT, B_RANDTIMEOFS, "Rand",	207,35,34,20, 0, 0, 0, 0, 0, "Randomize selected objects timeoffset");
	uiDefBut(block, BUT, B_PRINTSPEED,	"PrSpeed",			250,35,65,20, 0, 0, 0, 0, 0, "Print objectspeed");
	uiBlockEndAlign(block);
	
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, OB_OFFS_OB, REDRAWALL, "OfsEdit",			24,10,56,20, &ob->ipoflag, 0, 0, 0, 0, "Use timeoffset when inserting keys and display timeoffset for ipo and action views");
	uiDefButBitS(block, TOG, OB_OFFS_PARENT, REDRAWALL, "OfsParent",			82,10,56,20 , &ob->ipoflag, 0, 0, 0, 0, "Apply the timeoffset to this objects parent relationship");
	uiDefButBitS(block, TOG, OB_OFFS_PARTICLE, REDRAWALL, "OfsParticle",		140,10,56,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the particle effect");
	uiDefButBitS(block, TOG, OB_OFFS_PARENTADD, REDRAWALL, "AddParent",		196,10,56,20, &ob->ipoflag, 0, 0, 0, 0, "Add the parents timeoffset value");
	uiBlockEndAlign(block);
	
	sprintf(str, "%.4f", prspeed);
	uiDefBut(block, LABEL, 0, str,							260,10,63,31, NULL, 1.0, 0, 0, 0, "");
	
}

static void object_panel_draw(Object *ob)
{
	uiBlock *block;
	int xco, a, dx, dy;
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_draw", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Draw", "Object", 640, 0, 318, 204)==0) return;
	
	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	/* LAYERS */
	xco= 65;
	dx= 35;
	dy= 30;
	
	uiDefBut(block, LABEL, 0, "Layers",				10,170,100,20, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	for(a=0; a<5; a++)
		uiDefButBitI(block, TOG, 1<<a, B_OBLAY+a, "",	(short)(xco+a*(dx/2)), 180, (short)(dx/2), (short)(dy/2), (int *)&(BASACT->lay), 0, 0, 0, 0, "");
	for(a=0; a<5; a++)
		uiDefButBitI(block, TOG, 1<<(a+10), B_OBLAY+a+10, "",	(short)(xco+a*(dx/2)), 165, (short)(dx/2), (short)(dy/2), (int *)&(BASACT->lay), 0, 0, 0, 0, "");
	
	xco+= 7;
	uiBlockBeginAlign(block);
	for(a=5; a<10; a++)
		uiDefButBitI(block, TOG, 1<<a, B_OBLAY+a, "",	(short)(xco+a*(dx/2)), 180, (short)(dx/2), (short)(dy/2), (int *)&(BASACT->lay), 0, 0, 0, 0, "");
	for(a=5; a<10; a++)
		uiDefButBitI(block, TOG, 1<<(a+10), B_OBLAY+a+10, "",	(short)(xco+a*(dx/2)), 165, (short)(dx/2), (short)(dy/2), (int *)&(BASACT->lay), 0, 0, 0, 0, "");
	
	uiBlockEndAlign(block);
	
	/* Object Color */
	uiBlockBeginAlign(block);
	uiDefButF(block, COL, REDRAWVIEW3D, "",	250, 180, 50, 15, ob->col, 0, 0, 0, 0, "Object color, used when faces have the ObCol mode enabled");
	uiDefButF(block, NUM, REDRAWVIEW3D, "A:", 250, 165, 50, 15, &ob->col[3], 0.0f, 1.0f, 10, 2, "Object alpha, used when faces have the ObCol mode enabled");
	uiBlockEndAlign(block);	
	
	uiDefBut(block, LABEL, 0, "Drawtype",						10,120,100,20, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW, REDRAWVIEW3D, "Shaded",	10,100,100, 20, &ob->dt, 0, OB_SHADED, 0, 0, "Draw active object shaded or textured");
	uiDefButC(block, ROW, REDRAWVIEW3D, "Solid",	10,80,100, 20, &ob->dt, 0, OB_SOLID, 0, 0, "Draw active object in solid");
	uiDefButC(block, ROW, REDRAWVIEW3D, "Wire",		10,60, 100, 20, &ob->dt, 0, OB_WIRE, 0, 0, "Draw active object in wireframe");
	uiDefButC(block, ROW, REDRAWVIEW3D, "Bounds",	10,40, 100, 20, &ob->dt, 0, OB_BOUNDBOX, 0, 0, "Only draw object with bounding box");
	uiBlockEndAlign(block);
	
	uiDefBut(block, LABEL, 0, "Draw Extra",							120,120,90,20, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, OB_BOUNDBOX, REDRAWVIEW3D, "Bounds",				120, 100, 90, 20, &ob->dtx, 0, 0, 0, 0, "Displays the active object's bounds");
	uiDefButBitC(block, TOG, OB_DRAWNAME, REDRAWVIEW3D, "Name",		210, 100, 90, 20, &ob->dtx, 0, 0, 0, 0, "Displays the active object's name");
	
	uiDefButS(block, MENU, REDRAWVIEW3D, "Boundary Display%t|Box%x0|Sphere%x1|Cylinder%x2|Cone%x3|Polyheder%x4",
			  120, 80, 90, 20, &ob->boundtype, 0, 0, 0, 0, "Selects the boundary display type");
	uiDefButBitC(block, TOG, OB_AXIS, REDRAWVIEW3D, "Axis",			210, 80, 90, 20, &ob->dtx, 0, 0, 0, 0, "Displays the active object's center and axis");
	
	uiDefButBitC(block, TOG, OB_TEXSPACE, REDRAWVIEW3D, "TexSpace",	120, 60, 90, 20, &ob->dtx, 0, 0, 0, 0, "Displays the active object's texture space");
	uiDefButBitC(block, TOG, OB_DRAWWIRE, REDRAWVIEW3D, "Wire",		210, 60, 90, 20, &ob->dtx, 0, 0, 0, 0, "Adds the active object's wireframe over solid drawing");
	
	uiDefButBitC(block, TOG, OB_DRAWTRANSP, REDRAWVIEW3D, "Transp",	120, 40, 90, 20, &ob->dtx, 0, 0, 0, 0, "Enables transparent materials for the active object (Mesh only)");
	uiDefButBitC(block, TOG, OB_DRAWXRAY, REDRAWVIEW3D, "X-ray",	210, 40, 90, 20, &ob->dtx, 0, 0, 0, 0, "Makes the active object draw in front of others");
}

void object_panel_constraint(char *context)
{
	uiBlock *block;
	Object *ob= OBACT;
	ListBase *conlist;
	bConstraint *curcon;
	short xco, yco;
	char str[64];
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_constraint", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Constraints", context, 960, 0, 318, 204)==0) return;
	
	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);
	
	/* do not allow this panel to draw in editmode - why?*/
	if(G.obedit==OBACT) return;	// ??
	
	conlist = get_active_constraints(OBACT);
	
	if (conlist) {
		uiDefBlockBut(block, add_constraintmenu, NULL, "Add Constraint", 0, 190, 130, 20, "Add a new constraint");
		
		/* print active object or bone */
		str[0]= 0;
		if (ob->flag & OB_POSEMODE) {
			bPoseChannel *pchan= get_active_posechannel(ob);
			if(pchan) sprintf(str, "To Bone: %s", pchan->name);
		}
		else {
			sprintf(str, "To Object: %s", ob->id.name+2);
		}
		uiDefBut(block, LABEL, 1, str,	150, 190, 150, 20, NULL, 0.0, 0.0, 0, 0, "Displays Active Object or Bone name");
		
		/* Go through the list of constraints and draw them */
		xco = 10;
		yco = 160;
		
		for (curcon = conlist->first; curcon; curcon=curcon->next) {
			/* hrms, the temporal constraint should not draw! */
			if(curcon->type==CONSTRAINT_TYPE_KINEMATIC) {
				bKinematicConstraint *data= curcon->data;
				if(data->flag & CONSTRAINT_IK_TEMP)
					continue;
			}
			/* Draw default constraint header */
			draw_constraint(block, conlist, curcon, &xco, &yco);	
		}
		
		if(yco < 0) uiNewPanelHeight(block, 204-yco);
		
	}
}

void do_effects_panels(unsigned short event)
{
	Object *ob;
	ModifierData *md;
	ParticleSystemModifierData *psmd;
	ParticleSystem *psys;
	ParticleSettings *part;
	LinkNode *node, *firstnode;
	ID *id,*idtest;
	int nr;
	
	ob= OBACT;

	psys=psys_get_current(ob);

	switch(event) {

    case B_AUTOTIMEOFS:
		auto_timeoffs();
		break;
    case B_OFSTIMEOFS:
		ofs_timeoffs();
		break;
    case B_RANDTIMEOFS:
		rand_timeoffs();
		break;
	case B_FRAMEMAP:
		G.scene->r.framelen= G.scene->r.framapto;
		G.scene->r.framelen/= G.scene->r.images;
		allqueue(REDRAWALL, 0);
		break;
	case B_PARTBROWSE:
		if(G.buts->menunr== -2) {
			activate_databrowse((ID *)G.buts->lockpoin, ID_PA, 0, B_PARTBROWSE, &G.buts->menunr, do_effects_panels);
			return;
		}
		
		if(G.buts->menunr < 0) return;
		
		if(G.buts->pin) {
			
		}
		else {
			psys= psys_get_current(ob);
			if(psys)
				part=psys->part;
			else
				part=NULL;

			nr= 1;
			
			id= (ID *)part;
			
			idtest= G.main->particle.first;
			while(idtest) {
				if(nr==G.buts->menunr) {
					break;
				}
				nr++;
				idtest= idtest->next;
			}

			if(idtest==0) { /* new particle system */
				if(id){
					idtest= (ID *)psys_copy_settings((ParticleSettings *)id);
				}
				else {
					idtest= (ID *)psys_new_settings("PSys", G.main);
				}
				idtest->us--;
			}
			else if(((ParticleSettings*)idtest)->type==PART_FLUID) {
				error("Can't select fluid particles");
				break;
			}

			if(idtest!=id) {
				short nr=0;
				if(id==0){ /* no psys previously -> no modifier -> need to create that also */
					psys = MEM_callocN(sizeof(ParticleSystem), "particle_system");
					psys->pointcache = BKE_ptcache_add();
					psys->flag |= PSYS_ENABLED;
					BLI_addtail(&ob->particlesystem,psys);

					md= modifier_new(eModifierType_ParticleSystem);
					sprintf(md->name, "ParticleSystem %i", BLI_countlist(&ob->particlesystem));
					psmd= (ParticleSystemModifierData*) md;
					psmd->psys=psys;
					BLI_addtail(&ob->modifiers, md);
				}

				idtest->us++;
				psys->part=(ParticleSettings*)idtest;
				psys->totpart=0;
				psys->flag= PSYS_ENABLED|PSYS_CURRENT;
				psys->cfra=bsystem_time(ob,G.scene->r.cfra+1,0.0);

				/* check need for dupliobjects */
				nr=0;
				for(psys=ob->particlesystem.first; psys; psys=psys->next){
					if(ELEM(psys->part->draw_as,PART_DRAW_OB,PART_DRAW_GR))
						nr++;
				}
				if(nr)
					ob->transflag |= OB_DUPLIPARTS;
				else
					ob->transflag &= ~OB_DUPLIPARTS;

				BIF_undo_push("Browse Particle System");

				DAG_scene_sort(G.scene);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSOBJECT, 0);
				allqueue(REDRAWOOPS, 0);
			}
			
		}
		break;
	case B_PARTDELETE:
		if(ob && ob->particlesystem.first){
			psys= psys_get_current(ob);
			if(psys) {
				/* clear modifier */
				psmd= psys_get_modifier(ob,psys);
				BLI_remlink(&ob->modifiers, psmd);
				modifier_free((ModifierData *)psmd);

				/* clear particle system */
				BLI_remlink(&ob->particlesystem,psys);
				psys_free(ob,psys);

				BIF_undo_push("Delete particle system");

				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWBUTSOBJECT, 0);
				allqueue(REDRAWOOPS, 0);
			}
		}
		break;
	case B_PARTALONE: /* TODO: not too sure of how this works so someone check please, jahka */
		if(ob && (psys=psys_get_current(ob))){
			if(psys->part) {
				if(psys->part->id.us>1){
					if(okee("Make local")){
						part=psys_copy_settings(psys->part);
						part->id.us=1;
						psys->part->id.us--;
						psys->part=part;

						allqueue(REDRAWVIEW3D, 0);
						allqueue(REDRAWBUTSOBJECT, 0);
						allqueue(REDRAWOOPS, 0);

						BIF_undo_push("Make single user or local");
					}
				}
			}
		}
		break;

	case B_PART_ALLOC:
	case B_PART_DISTR:
	case B_PART_INIT:
	case B_PART_RECALC:
	case B_PART_ALLOC_CHILD:
	case B_PART_DISTR_CHILD:
	case B_PART_INIT_CHILD:
	case B_PART_RECALC_CHILD:
		if(psys) {
			Base *base;
			Object *bob;
			ParticleSystem *bpsys;
			int flush;

			nr=0;
			for(bpsys=ob->particlesystem.first; bpsys; bpsys=bpsys->next){
				if(ELEM(bpsys->part->draw_as,PART_DRAW_OB,PART_DRAW_GR))
					nr++;
			}
			if(nr)
				ob->transflag |= OB_DUPLIPARTS;
			else
				ob->transflag &= ~OB_DUPLIPARTS;

			if(psys->part->type==PART_REACTOR)
				if(psys->target_ob)
					DAG_object_flush_update(G.scene, psys->target_ob, OB_RECALC_DATA);

			for(base = G.scene->base.first; base; base= base->next) {
				bob= base->object;
				flush= 0;
				for(bpsys=bob->particlesystem.first; bpsys; bpsys=bpsys->next)
					if(bpsys->part==psys->part)
						flush= 1;

				if(flush)
					DAG_object_flush_update(G.scene, bob, OB_RECALC_DATA);
			}

			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
		}
		break;
	
	/* there were separate update events before the pointcache refactor,
	 * now it only does a flush update which means it is recalculating
	 * more than strictly needed, but how to restore such partial updates
	 * i'm not sure - brecht. */
#if 0
	case B_PART_ALLOC:
	case B_PART_ALLOC_CHILD:
		if(psys){
			psys_flush_settings(psys->part,PSYS_ALLOC,event==B_PART_ALLOC);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWOOPS, 0);
		}
		break;

	case B_PART_DISTR:
	case B_PART_DISTR_CHILD:
		if(psys){
			psys_flush_settings(psys->part,PSYS_DISTR,event==B_PART_DISTR);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWOOPS, 0);
		}
		break;
	case B_PART_INIT:
	case B_PART_INIT_CHILD:
		if(psys){
			psys_flush_settings(psys->part,PSYS_INIT,event==B_PART_INIT);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWOOPS, 0);
		}
		break;
	case B_PART_ENABLE:
		if(psys) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWBUTSEDIT, 0);
		}
		break;
		/* no break! */
#endif
	case B_PART_REDRAW_DEPS:
		if(event == B_PART_REDRAW_DEPS)
			DAG_scene_sort(G.scene);
		/* no break! */
	case B_PART_REDRAW:
		nr=0;
		for(psys=ob->particlesystem.first; psys; psys=psys->next){
			if(ELEM(psys->part->draw_as,PART_DRAW_OB,PART_DRAW_GR))
				nr++;
		}
		if(nr)
			ob->transflag |= OB_DUPLIPARTS;
		else
			ob->transflag &= ~OB_DUPLIPARTS;

		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_PARTTYPE:
		if(psys) {
		 	/* 1 = do DAG_object_flush_update */
			firstnode= psys_using_settings(psys->part, 1);

			for(node=firstnode; node; node=node->next)
				psys_changed_type(node->link);
			
			BLI_linklist_free(firstnode, NULL);
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
		}
		break;
	case B_PARTACT:
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWIPO, 0);
		break;
	case B_PARTTARGET:
		if((psys=psys_get_current(ob))){
			if(psys->keyed_ob==ob || psys->target_ob==ob){
				if(psys->keyed_ob==ob)
					psys->keyed_ob=NULL;
				else
					psys->target_ob=NULL;
			}
			else{
				DAG_scene_sort(G.scene);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWBUTSOBJECT, 0);
		}
		break;
	case B_PART_REKEY:
		PE_rekey();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		break;
	case B_PART_EDITABLE:
		if((psys = psys_get_current(ob))) {
			if(psys->flag & PSYS_EDITED){
				if(okee("Lose changes done in particle mode?")){
					if(psys->edit)
						PE_free_particle_edit(psys);

					psys->flag &= ~PSYS_EDITED;
					psys->recalc |= PSYS_RECALC_HAIR;

					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				}
			}
			else {
				if(psys_check_enabled(ob, psys)) {
					psys->flag |= PSYS_EDITED;
					if(G.f & G_PARTICLEEDIT)
						PE_create_particle_edit(ob, psys);
				}
				else
					error("Particle system not enabled, skipping set editable");
			}
		}
	case B_FIELD_DEP:
		/* do this before scene sort, that one checks for CU_PATH */
		if(ob->type==OB_CURVE && ob->pd->forcefield==PFIELD_GUIDE) {
			Curve *cu= ob->data;
			
			cu->flag |= (CU_PATH|CU_3D);
			do_curvebuts(B_CU3D);	/* all curves too */
		}
		DAG_scene_sort(G.scene);

		if(ob->type==OB_CURVE && ob->pd->forcefield==PFIELD_GUIDE)
			DAG_object_flush_update(G.scene, ob, OB_RECALC);
		else
			DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);

		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_FIELD_CHANGE:
		if(ob->pd->forcefield != PFIELD_TEXTURE && ob->pd->tex){
			ob->pd->tex->id.us--;
			ob->pd->tex=0;
		}
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
		allqueue(REDRAWVIEW3D, 0);
		break;
	}

}

/* copy from buttons_editing.c */
static void field_testTexture(char *name, ID **idpp)
{
	ID *id;

	for(id = G.main->tex.first; id; id = id->next) {
		if(strcmp(name, id->name + 2) == 0) {
			*idpp = id;
			/* texture gets user, objects not: delete object = clear modifier */
			id_us_plus(id);
			return;
		}
	}
	*idpp = 0;
}

/* Panel for collision */
static void object_collision__enabletoggle ( void *ob_v, void *arg2 )
{
	Object *ob = ob_v;
	PartDeflect *pd= ob->pd;
	ModifierData *md = modifiers_findByType ( ob, eModifierType_Collision );
	
	if ( !md )
	{
		if(pd && (pd->deflect))
		{
			md = modifier_new ( eModifierType_Collision );
			BLI_addtail ( &ob->modifiers, md );
			DAG_scene_sort(G.scene);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
	}
	else
	{
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		BLI_remlink ( &ob->modifiers, md );
		modifier_free ( md );
		DAG_scene_sort(G.scene);
		allqueue(REDRAWBUTSEDIT, 0);
	}
}

/* Panels for particle interaction settings */
static void object_panel_collision(Object *ob)
{
	uiBlock *block;
	uiBut *but;

	block= uiNewBlock(&curarea->uiblocks, "object_panel_deflection", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Fields", "Physics");
	if(uiNewPanel(curarea, block, "Collision", "Physics", 0, 0, 318, 204)==0) return;

	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	/* should become button, option? */
	if(ob->pd==NULL) {
		ob->pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");
		/* and if needed, init here */
		ob->pd->pdef_sbdamp = 0.1f;
		ob->pd->pdef_sbift  = 0.2f;
		ob->pd->pdef_sboft  = 0.02f;
	}
	
	/* only meshes collide now */
	if(ob->pd && ob->type==OB_MESH) {
		PartDeflect *pd= ob->pd;
		
		but = uiDefButBitS(block, TOG, 1, B_REDR, "Collision",10,160,150,20, &pd->deflect, 0, 0, 0, 0, "Enable this objects as a collider for physics systems");
		uiButSetFunc(but, object_collision__enabletoggle, ob, NULL);

		uiDefBut(block, LABEL, 0, "",160,160,150,2, NULL, 0.0, 0, 0, 0, "");
		
		if(pd->deflect) {
			uiDefBut(block, LABEL, 0, "Particle Interaction",			10,135,310,20, NULL, 0.0, 0, 0, 0, "");

			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Damping: ",		10,115,105,20, &pd->pdef_damp, 0.0, 1.0, 10, 2, "Amount of damping during particle collision");
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Rnd: ",	115,115,75,20, &pd->pdef_rdamp, 0.0, 1.0, 10, 2, "Random variation of damping");
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Friction: ",		10,95,105,20, &pd->pdef_frict, 0.0, 1.0, 10, 2, "Amount of friction during particle collision");
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Rnd: ",	115,95,75,20, &pd->pdef_rfrict, 0.0, 1.0, 10, 2, "Random variation of friction");
			uiBlockEndAlign(block);

			uiDefButBitS(block, TOG, PDEFLE_KILL_PART, B_FIELD_CHANGE, "Kill",200,115,120,20, &pd->flag, 0, 0, 0, 0, "Kill collided particles");
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Permeability: ",	200,90,120,20, &pd->pdef_perm, 0.0, 1.0, 10, 2, "Chance that the particle will pass through the mesh");
			
			uiDefBut(block, LABEL, 0, "Soft Body and Cloth Interaction",			10,65,310,20, NULL, 0.0, 0, 0, 0, "");

			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Damping:",	10,45,150,20, &pd->pdef_sbdamp, 0.0, 1.0, 10, 0, "Amount of damping during collision");
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Inner:",	10,25,150,20, &pd->pdef_sbift, 0.001, 1.0, 10, 0, "Inner face thickness");
			uiDefButF(block, NUM, B_FIELD_CHANGE, "Outer:",	10, 5,150,20, &pd->pdef_sboft, 0.001, 1.0, 10, 0, "Outer face thickness");
			uiBlockEndAlign(block);

			uiDefButBitS(block, TOG, OB_SB_COLLFINAL, B_FIELD_CHANGE, "Ev.M.Stack", 170,45,150,20, &ob->softflag, 0, 0, 0, 0, "Pick collision object from modifier stack (softbody only)");
		}
	}
}
static void object_panel_fields(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	int particles=0;
	static short actpsys=-1;

	block= uiNewBlock(&curarea->uiblocks, "object_panel_fields", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Fields", "Physics", 0, 0, 318, 204)==0) return;

	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	/* should become button, option? */
	if(ob->pd==NULL) {
		ob->pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");
		/* and if needed, init here */
		ob->pd->pdef_sbdamp = 0.1f;
		ob->pd->pdef_sbift  = 0.2f;
		ob->pd->pdef_sboft  = 0.02f;
	}
	
	if(ob->pd) {
		PartDeflect *pd= ob->pd;
		char *menustr= MEM_mallocN(256, "temp string");
		char *tipstr="Choose field type";

		uiBlockBeginAlign(block);
		
		if(ob->particlesystem.first) {
			ParticleSystem *psys;
			char *menustr2= psys_menu_string(ob,1);
			
			psys= psys_get_current(ob);
			if(psys && actpsys >= 0) {
				actpsys= psys_get_current_num(ob)+1;

				if(psys->part->pd==NULL)
					psys->part->pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");

				pd= psys->part->pd;
				particles=1;
			}
			else
				actpsys= -1; /* -1 = object */

			but=uiDefButS(block, MENU, B_BAKE_REDRAWEDIT, menustr2, 10,180,70,20, &actpsys, 14.0, 0.0, 0, 0, "Browse systems");
			uiButSetFunc(but, PE_change_act, ob, &actpsys);

			MEM_freeN(menustr2);
		}

		/* setup menu button */
		if(particles){
			sprintf(menustr, "Field Type%%t|None%%x0|Spherical%%x%d|Wind%%x%d|Vortex%%x%d|Magnetic%%x%d|Harmonic%%x%d", 
					PFIELD_FORCE, PFIELD_WIND, PFIELD_VORTEX, PFIELD_MAGNET, PFIELD_HARMONIC);

			if(pd->forcefield==PFIELD_FORCE) tipstr= "Particle attracts or repels particles (On shared object layers)";
			else if(pd->forcefield==PFIELD_WIND) tipstr= "Constant force applied in direction of particle Z axis (On shared object layers)";
			else if(pd->forcefield==PFIELD_VORTEX) tipstr= "Particles swirl around Z-axis of the particle (On shared object layers)";
		}
		else{
			if(ob->type==OB_CURVE)
				sprintf(menustr, "Field Type%%t|None%%x0|Spherical%%x%d|Wind%%x%d|Vortex%%x%d|Curve Guide%%x%d|Magnetic%%x%d|Harmonic%%x%d|Texture%%x%d", 
						PFIELD_FORCE, PFIELD_WIND, PFIELD_VORTEX, PFIELD_GUIDE, PFIELD_MAGNET, PFIELD_HARMONIC, PFIELD_TEXTURE);
			else
				sprintf(menustr, "Field Type%%t|None%%x0|Spherical%%x%d|Wind%%x%d|Vortex%%x%d|Magnetic%%x%d|Harmonic%%x%d|Texture%%x%d", 
						PFIELD_FORCE, PFIELD_WIND, PFIELD_VORTEX, PFIELD_MAGNET, PFIELD_HARMONIC, PFIELD_TEXTURE);

			if(pd->forcefield==PFIELD_FORCE) tipstr= "Object center attracts or repels particles (On shared object layers)";
			else if(pd->forcefield==PFIELD_WIND) tipstr= "Constant force applied in direction of Object Z axis (On shared object layers)";
			else if(pd->forcefield==PFIELD_VORTEX) tipstr= "Particles swirl around Z-axis of the Object (On shared object layers)";
			else if(pd->forcefield==PFIELD_GUIDE) tipstr= "Use a Curve Path to guide particles (On shared object layers)";
		}
		
		if(ob->particlesystem.first)
			uiDefButS(block, MENU, B_FIELD_DEP, menustr,			80,180,70,20, &pd->forcefield, 0.0, 0.0, 0, 0, tipstr);
		else
			uiDefButS(block, MENU, B_FIELD_DEP, menustr,			10,180,140,20, &pd->forcefield, 0.0, 0.0, 0, 0, tipstr);

		uiBlockEndAlign(block);
		uiDefBut(block, LABEL, 0, "",160,180,150,2, NULL, 0.0, 0, 0, 0, "");

		MEM_freeN(menustr);
		
		if(pd->forcefield) {
			uiBlockBeginAlign(block);
			if(pd->forcefield == PFIELD_GUIDE) {
				uiDefButF(block, NUM, B_FIELD_CHANGE, "MinDist: ",	10,140,140,20, &pd->f_strength, 0.0, 1000.0, 10, 0, "The distance from which particles are affected fully.");
				uiDefButF(block, NUM, B_FIELD_CHANGE, "Fall-off: ",	10,120,140,20, &pd->f_power, 0.0, 10.0, 10, 0, "Falloff factor, between mindist and maxdist");
				uiDefButF(block, NUM, B_FIELD_CHANGE, "Free: ",	10,100,140,20, &pd->free_end, 0.0, 0.99, 10, 0, "Guide-free time from particle life's end");
				uiBlockEndAlign(block);
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, PFIELD_USEMAX, B_FIELD_CHANGE, "Use",	10,80,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a maximum distance for the field to work");
				uiDefButF(block, NUM, B_FIELD_CHANGE, "MaxDist: ",	50,80,100,20, &pd->maxdist, 0, 1000.0, 10, 0, "Maximum distance for the field to work");
			}
			else {
				uiDefButF(block, NUM, B_FIELD_CHANGE, "Strength: ",	10,140,140,20, &pd->f_strength, -1000, 1000, 10, 3, "Strength of force field");
				
				if(pd->forcefield == PFIELD_TEXTURE){
					uiDefIDPoinBut(block, field_testTexture, ID_TE, B_FIELD_CHANGE, "Texture: ", 10,120,140,20, &pd->tex, "Texture to use as force");
					uiBlockEndAlign(block);
					uiBlockBeginAlign(block);
					uiDefButBitS(block, TOG, PFIELD_TEX_OBJECT, B_FIELD_CHANGE, "Use Object Co",	10,95,140,20, &pd->flag, 0.0, 0, 0, 0, "Use object/global coordinates for texture");
					uiDefButBitS(block, TOG, PFIELD_TEX_ROOTCO, B_FIELD_CHANGE, "Root TexCo",	10,75,100,20, &pd->flag, 0.0, 0, 0, 0, "Texture coords from root particle locations");
					uiDefButBitS(block, TOG, PFIELD_TEX_2D, B_FIELD_CHANGE, "2D",	120,75,30,20, &pd->flag, 0.0, 0, 0, 0, "Apply force only in 2d");
				}
				else if(pd->forcefield == PFIELD_HARMONIC) 
					uiDefButF(block, NUM, B_FIELD_CHANGE, "Damp: ",	10,120,140,20, &pd->f_damp, 0, 10, 10, 0, "Damping of the harmonic force");	
			}
			uiBlockEndAlign(block);
			
			uiBlockBeginAlign(block);
			if(pd->forcefield == PFIELD_GUIDE){
				uiDefButBitS(block, TOG, PFIELD_GUIDE_PATH_ADD, B_FIELD_CHANGE, "Additive",	10,40,140,20, &pd->flag, 0.0, 0, 0, 0, "Based on distance/falloff it adds a portion of the entire path");
			}
			else if(pd->forcefield==PFIELD_TEXTURE){
				uiDefButS(block, MENU, B_FIELD_CHANGE, "Texture mode%t|RGB%x0|Gradient%x1|Curl%x2",	10,50,140,20, &pd->tex_mode, 0.0, 0.0, 0, 0, "How the texture effect is calculated (RGB & Curl need a RGB texture else Gradient will be used instead)");
	
				uiDefButF(block, NUM, B_FIELD_CHANGE, "Nabla:",	10,30,140,20, &pd->tex_nabla, 0.0001f, 1.0, 1, 0, "Specify the dimension of the area for gradient and curl calculation");
			}
			else if(particles==0 && ELEM(pd->forcefield,PFIELD_VORTEX,PFIELD_WIND)==0){
				//uiDefButF(block, NUM, B_FIELD_CHANGE, "Distance: ",	10,20,140,20, &pd->f_dist, 0, 1000.0, 10, 0, "Falloff power (real gravitational fallof = 2)");
				uiDefButBitS(block, TOG, PFIELD_PLANAR, B_FIELD_CHANGE, "Planar",	10,15,140,20, &pd->flag, 0.0, 0, 0, 0, "Create planar field");
			}
			uiBlockEndAlign(block);
			
			if(pd->forcefield==PFIELD_GUIDE){
				uiBlockBeginAlign(block);
				uiDefButF(block, NUMSLI, B_FIELD_CHANGE, "Clump:",		160,180,140,20, &pd->clump_fac, -1.0, 1.0, 1, 3, "Amount of clumpimg");
				uiDefButF(block, NUMSLI, B_FIELD_CHANGE, "Shape:",		160,160,140,20, &pd->clump_pow, -0.999, 0.999, 1, 3, "Shape of clumpimg");
				uiBlockEndAlign(block);

				uiBlockBeginAlign(block);
				if(pd->kink){
					uiDefButS(block, MENU, B_FIELD_CHANGE, "Kink:%t|Roll%x6|Rotation%x5|Braid%x4|Wave%x3|Radial%x2|Curl%x1|Nothing%x0", 160,120,70,20, &pd->kink, 14.0, 0.0, 0, 0, "Type of periodic offset on the curve");
					uiDefButS(block, MENU, B_FIELD_CHANGE, "Axis %t|Z %x2|Y %x1|X %x0", 230,120,70,20, &pd->kink_axis, 14.0, 0.0, 0, 0, "Which axis to use for offset");
					uiDefButF(block, NUM, B_FIELD_CHANGE, "Freq:",			160,100,140,20, &pd->kink_freq, 0.0, 10.0, 1, 3, "The frequency of the offset (1/total length)");
					uiDefButF(block, NUMSLI, B_FIELD_CHANGE, "Shape:",		160,80,140,20, &pd->kink_shape, -0.999, 0.999, 1, 3, "Adjust the offset to the beginning/end");
					uiDefButF(block, NUM, B_FIELD_CHANGE, "Amplitude:",	160,60,140,20, &pd->kink_amp, 0.0, 10.0, 1, 3, "The amplitude of the offset");
				}
				else{
					uiDefButS(block, MENU, B_FIELD_CHANGE, "Kink:%t|Roll%x6|Rotation%x5|Braid%x4|Wave%x3|Radial%x2|Curl%x1|Nothing%x0", 160,120,140,20, &pd->kink, 14.0, 0.0, 0, 0, "Type of periodic offset on the curve");
				}
				uiBlockEndAlign(block);
			}
			else{
				uiDefButS(block, MENU, B_FIELD_DEP, "Fall-off%t|Cone%x2|Tube%x1|Sphere%x0",	160,180,140,20, &pd->falloff, 0.0, 0.0, 0, 0, "Fall-off shape");
				if(pd->falloff==PFIELD_FALL_TUBE)
					uiDefBut(block, LABEL, 0, "Longitudinal",		160,160,140,20, NULL, 0.0, 0, 0, 0, "");
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, PFIELD_POSZ, B_FIELD_CHANGE, "Pos",	160,140,40,20, &pd->flag, 0.0, 0, 0, 0, "Effect only in direction of positive Z axis");
				uiDefButF(block, NUM, B_FIELD_CHANGE, "Fall-off: ",	200,140,100,20, &pd->f_power, 0, 10, 10, 0, "Falloff power (real gravitational falloff = 2)");
				uiDefButBitS(block, TOG, PFIELD_USEMAX, B_FIELD_CHANGE, "Use",	160,120,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a maximum distance for the field to work");
				uiDefButF(block, NUM, B_FIELD_CHANGE, "MaxDist: ",	200,120,100,20, &pd->maxdist, 0, 1000.0, 10, 0, "Maximum distance for the field to work");
				uiDefButBitS(block, TOG, PFIELD_USEMIN, B_FIELD_CHANGE, "Use",	160,100,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a minimum distance for the field's fall-off");
				uiDefButF(block, NUM, B_FIELD_CHANGE, "MinDist: ",	200,100,100,20, &pd->mindist, 0, 1000.0, 10, 0, "Minimum distance for the field's fall-off");
				uiBlockEndAlign(block);

				if(pd->falloff==PFIELD_FALL_TUBE){
					uiDefBut(block, LABEL, 0, "Radial",		160,80,70,20, NULL, 0.0, 0, 0, 0, "");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, B_FIELD_CHANGE, "Fall-off: ",	160,60,140,20, &pd->f_power_r, 0, 10, 10, 0, "Radial falloff power (real gravitational falloff = 2)");
					uiDefButBitS(block, TOG, PFIELD_USEMAXR, B_FIELD_CHANGE, "Use",	160,40,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a maximum radial distance for the field to work");
					uiDefButF(block, NUM, B_FIELD_CHANGE, "MaxDist: ",	200,40,100,20, &pd->maxrad, 0, 1000.0, 10, 0, "Maximum radial distance for the field to work");
					uiDefButBitS(block, TOG, PFIELD_USEMINR, B_FIELD_CHANGE, "Use",	160,20,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a minimum radial distance for the field's fall-off");
					uiDefButF(block, NUM, B_FIELD_CHANGE, "MinDist: ",	200,20,100,20, &pd->minrad, 0, 1000.0, 10, 0, "Minimum radial distance for the field's fall-off");
					uiBlockEndAlign(block);
				}
				else if(pd->falloff==PFIELD_FALL_CONE){
					uiDefBut(block, LABEL, 0, "Angular",		160,80,70,20, NULL, 0.0, 0, 0, 0, "");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, B_FIELD_CHANGE, "Fall-off: ",	160,60,140,20, &pd->f_power_r, 0, 10, 10, 0, "Radial falloff power (real gravitational falloff = 2)");
					uiDefButBitS(block, TOG, PFIELD_USEMAXR, B_FIELD_CHANGE, "Use",	160,40,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a maximum angle for the field to work");
					uiDefButF(block, NUM, B_FIELD_CHANGE, "MaxAngle: ",	200,40,100,20, &pd->maxrad, 0, 89.0, 10, 0, "Maximum angle for the field to work (in radians)");
					uiDefButBitS(block, TOG, PFIELD_USEMINR, B_FIELD_CHANGE, "Use",	160,20,40,20, &pd->flag, 0.0, 0, 0, 0, "Use a minimum angle for the field's fall-off");
					uiDefButF(block, NUM, B_FIELD_CHANGE, "MinAngle: ",	200,20,100,20, &pd->minrad, 0, 89.0, 10, 0, "Minimum angle for the field's fall-off (in radians)");
					uiBlockEndAlign(block);
				}
			}
		}	
	}
}

/* Generic physics baking buttons */

static void object_physics__baketoggle(void *pid_v, void *unused_v)
{
	PTCacheID *pid = pid_v;
	Object *ob = pid->ob;
	PointCache *cache = pid->cache;
	ClothModifierData *clmd;
	int cageIndex, stack_index, startframe, endframe;

	// automatically enable modifier in editmode when we have a protected cache
	if(!(cache->flag & PTCACHE_BAKED)) {
		BKE_ptcache_id_time(pid, 0.0f, &startframe, &endframe, NULL);
		pointcache_bake(pid, startframe);

		if(pid->type == PTCACHE_TYPE_CLOTH) {
			clmd= (ClothModifierData*)pid->data;
			cageIndex = modifiers_getCageIndex(ob, NULL );
			stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
			if(stack_index >= cageIndex)
				((ModifierData *)clmd)->mode ^= eModifierMode_OnCage;
		}
	}
	else {
		if(cache->flag & PTCACHE_BAKE_EDIT_ACTIVE) {
			notice("Can't free bake in editmode");
		}
		else {
			if(pid->type == PTCACHE_TYPE_CLOTH) {
				clmd= (ClothModifierData*)pid->data;
				((ModifierData *)clmd)->mode ^= eModifierMode_OnCage;
			}

			cache->flag &= ~PTCACHE_BAKED;
			BKE_ptcache_id_reset(pid, PTCACHE_RESET_OUTDATED);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
	}
}

static void object_physics__rebake(void *pid_v, void *unused_v)
{
	PTCacheID *pid = pid_v;
	int curframe = (int)G.scene->r.cfra;

	BKE_ptcache_id_clear(pid, PTCACHE_CLEAR_AFTER, curframe);
	pointcache_bake(pid, curframe);
}

static void object_physics__clearcache(void *pid_v, void *unused_v)
{
	PTCacheID *pid = pid_v;
	Object *ob = pid->ob;
	PointCache *cache = pid->cache;

	if(cache->flag & PTCACHE_BAKE_EDIT_ACTIVE)
		return;

	BKE_ptcache_id_reset(pid, PTCACHE_RESET_BAKED);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA); 

	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

static void object_physics_bake_buttons(uiBlock *block, PTCacheID *pid, int y, int libdata)
{
	uiBut *but;
	PointCache *cache;
	
	cache= pid->cache;

	if(!libdata && G.obedit)
		uiSetButLock(1, "Can't change bake settings in editmode");

	if(cache->flag & PTCACHE_BAKED)
		but = uiDefBut(block, BUT, REDRAWBUTSOBJECT, "Free Bake", 10,y+25,85,20, NULL, 0.0, 0.0, 0, 0, "Free baked simulation");
	else
		but = uiDefBut(block, BUT, REDRAWBUTSOBJECT, "Bake", 10,y+25,85,20, NULL, 0.0, 0.0, 0, 0, "Bake specified frame range");
	uiButSetFunc(but, object_physics__baketoggle, pid, NULL);

	if(!libdata && !G.obedit && (cache->flag & PTCACHE_BAKED))
		uiSetButLock(1, "Simulation frames are baked");

	uiBlockBeginAlign(block);
	uiDefButI(block, NUM, B_BAKE_CACHE_CHANGE, "Start:", 100,y+25,105,20, &cache->startframe, 1, MAXFRAME, 1, 0, "Frame on which the simulation starts");
	uiDefButI(block, NUM, B_BAKE_CACHE_CHANGE, "End:", 205,y+25,105,20, &cache->endframe, 1, MAXFRAME, 1, 0, "Frame on which the simulation stops");
	uiBlockEndAlign(block);
			
	if(cache->flag & PTCACHE_BAKED) {
		if(pid->type == PTCACHE_TYPE_CLOTH ||
		   (pid->type == PTCACHE_TYPE_SOFTBODY && !((SoftBody*)pid->data)->particles)) {
			if(!libdata && !G.obedit)
				uiClearButLock();

			uiBlockBeginAlign(block);
			uiDefButBitI(block, TOG, PTCACHE_BAKE_EDIT, REDRAWVIEW3D, "Bake Editing",	10,y,100,20, &cache->flag, 0, 0, 0, 0, "Enable editing of the baked results in editmode.");
			but= uiDefBut(block, BUT, REDRAWBUTSOBJECT, "Rebake From Current Frame", 110,y,200,20, NULL, 0.0, 0.0, 0, 0, "Bake again from current frame");
			uiButSetFunc(but, object_physics__rebake, pid, NULL);
			uiBlockEndAlign(block);
		}

		if(!libdata)
			uiClearButLock();
	}
	else {
		char str[512];
		int exist, startframe, endframe;

		if(!libdata)
			uiClearButLock();
		
		BKE_ptcache_id_time(pid, 0.0f, &startframe, &endframe, NULL);
		exist= BKE_ptcache_id_exist(pid, startframe);

		sprintf(str, "%simulation frames in disk cache.", (exist)? "S": "No s");
		uiDefBut(block, LABEL, 0, str, 10,y,200,20, NULL, 0.0, 0, 0, 0, "");

		if(exist) {
			but= uiDefBut(block, BUT, REDRAWBUTSOBJECT, "Free Cache", 210,y,100,20, NULL, 0.0, 0.0, 0, 0, "Free cached simulation results");
			uiButSetFunc(but, object_physics__clearcache, pid, NULL);
		}
	}
}

/* Panel for softbodies */
static void object_softbodies__enable(void *ob_v, void *arg2)
{
	Object *ob = ob_v;
	ModifierData *md = modifiers_findByType(ob, eModifierType_Softbody);
	PTCacheID pid;

	if(md) {
		BLI_remlink(&ob->modifiers, md);
		modifier_free(md);
		BIF_undo_push("Del modifier");

		ob->softflag &= ~OB_SB_ENABLE;
	} else {
		md = modifier_new(eModifierType_Softbody);
		BLI_addhead(&ob->modifiers, md);

		if (!ob->soft) {
			ob->soft= sbNew();
			ob->softflag |= OB_SB_GOAL|OB_SB_EDGES;

			BKE_ptcache_id_from_softbody(&pid, ob, ob->soft);
			BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_ALL, 0);
		}

		ob->softflag |= OB_SB_ENABLE;
	}

	/* needed so that initial state is cached correctly */
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

static int _can_softbodies_at_all(Object *ob)
{
	// list of Yes
    if ((ob->type==OB_MESH)
		|| (ob->type==OB_CURVE)
		|| (ob->type==OB_LATTICE)
		|| (ob->type==OB_SURF)
		) return 1;
	// else deny
	return 0;
}
static void object_softbodies__enable_psys(void *ob_v, void *psys_v)
{
	ParticleSystem *psys = psys_v;
	Object *ob = ob_v;

	if(psys->softflag & OB_SB_ENABLE) {
		psys->softflag &= ~OB_SB_ENABLE;
	}
	else{
		if (!psys->soft) {
			psys->soft= sbNew();
			psys->softflag |= OB_SB_GOAL|OB_SB_EDGES;
			psys->soft->particles=psys;
		}
		psys->softflag |= OB_SB_ENABLE;
	}

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	allqueue(REDRAWBUTSEDIT, 0);
}


#ifdef _work_on_sb_solver
static char sbsolvers[] = "Solver %t|RKP almost SOFT not usable but for some german teachers %x1|STU ip semi implicit euler%x3|SI1  (half step)adaptive semi implict euler %x2|SI2  (use dv)adaptive semi implict euler %x4|SOFT  step size controlled midpoint(1rst choice for real softbodies)%x0";
/* SIF would have been candidate  .. well lack of time .. brecht is busy .. better make a stable version for peach now :) */
static char sbsolvers[] = "SIF  semi implicit euler with fixed step size (worth a try with real stiff egdes)%x3|SOFT  step size controlled midpoint(1rst choice for real softbodies)%x0";
#else
static char sbsolvers[] = "RKCP correct physics (harder to get stable but usefull for education :)%x1|SOFT  step size controlled midpoint(1rst choice for real softbodies)%x0";
#endif

static void object_softbodies_collision(Object *ob)
{
	SoftBody *sb=ob->soft;
	uiBlock *block;
	static int val;
	short *softflag=&ob->softflag, psys_cur=0;
	int ob_has_hair=psys_ob_has_hair(ob);
	static PTCacheID staticpid;
	int libdata;

    if(!_can_softbodies_at_all(ob)) return;
	/*bah that is ugly! creating missing data members in UI code*/
	if(ob->pd == NULL){
		ob->pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");
		ob->pd->pdef_sbdamp = 0.1f;
		ob->pd->pdef_sbift  = 0.2f;
		ob->pd->pdef_sboft  = 0.02f;
	}
	block= uiNewBlock(&curarea->uiblocks, "object_softbodies_collision", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Soft Body", "Physics"); 
	if(uiNewPanel(curarea, block, "Soft Body Collision", "Physics", 651, 0, 318, 204)==0) return;

	libdata= object_is_libdata(ob);
	uiSetButLock(libdata, ERROR_LIBDATA_MESSAGE);

	if(ob_has_hair) {
		if(PE_get_current_num(ob) >= 0) {
			ParticleSystem *psys = PE_get_current(ob);
			if(psys) {
				sb = psys->soft;
				softflag = &psys->softflag;
				psys_cur = 1;
			}
		}
	}

	if(psys_cur) {
		if(*softflag & OB_SB_ENABLE)
			val = 1;
		else
			val = 0;
	}
	else
		val = modifiers_isSoftbodyEnabled(ob);

	if(!val) {
		uiDefBut(block, LABEL, 0, "",10,10,1,2, NULL, 0.0, 0, 0, 0, ""); /* tell UI we go to 10,10*/
		uiBlockBeginAlign(block);
		if(psys_cur){
			uiDefBut(block, LABEL, 0, "Hair is not a softbody.",10,190,300,20, NULL, 0.0, 0, 0, 0, ""); 
		}
		else {
			uiDefBut(block, LABEL, 0, "Object is not a softbody.",10,190,300,20, NULL, 0.0, 0, 0, 0, ""); 
		}
		uiBlockEndAlign(block);
	}
	else{
		BKE_ptcache_id_from_softbody(&staticpid, ob, sb);
		object_physics_bake_buttons(block, &staticpid, 125, libdata);

		/* SELF COLLISION STUFF */
		if ((ob->type==OB_MESH)||(ob->type==OB_CURVE) ) {
			uiBlockBeginAlign(block);
			if (*softflag & OB_SB_EDGES){
				uiDefButBitS(block, TOG, OB_SB_SELF, B_BAKE_CACHE_CHANGE, "Self Collision",		10,80,150,20, softflag, 0, 0, 0, 0, "enable naive vertex ball self collision");
				if(*softflag & OB_SB_SELF){
					uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Ball Size:", 160,80,150,20, &sb->colball, -10.0,  10.0, 10, 0, "Absolute ball size or factor if not manual adjusted");
					uiDefButS(block, ROW, B_BAKE_CACHE_CHANGE, "Man",10,60,60,20, &sb->sbc_mode, 4.0,SBC_MODE_MANUAL, 0, 0, "Manual adjust");
					uiDefButS(block, ROW, B_BAKE_CACHE_CHANGE, "Av",70,60,60,20, &sb->sbc_mode, 4.0,SBC_MODE_AVG, 0, 0, "Average Spring lenght * Ball Size");
					uiDefButS(block, ROW, B_BAKE_CACHE_CHANGE, "Min",130,60,60,20, &sb->sbc_mode, 4.0,SBC_MODE_MIN, 0, 0, "Minimal Spring lenght * Ball Size");
					uiDefButS(block, ROW, B_BAKE_CACHE_CHANGE, "Max",190,60,60,20, &sb->sbc_mode, 4.0,SBC_MODE_MAX, 0, 0, "Maximal Spring lenght * Ball Size");
					uiDefButS(block, ROW, B_BAKE_CACHE_CHANGE, "AvMiMa",250,60,60,20, &sb->sbc_mode, 4.0,SBC_MODE_AVGMINMAX, 0, 0, "(Min+Max)/2 * Ball Size");
					uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "B Stiff:", 10,40,150,20, &sb->ballstiff, 0.001,  100.0, 10, 0, "Ball inflating presure");
					uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "B Damp:", 160,40,150,20, &sb->balldamp,  0.001,  1.0, 10, 0, "Blending to inelastic collision");
				}
			}
			else{
				uiDefBut(block, LABEL, 0, "<Self Collision> not available because there",10,80,300,20, NULL, 0.0, 0, 0, 0, ""); 
				uiDefBut(block, LABEL, 0, "are no edges, enable <Use Edges>",10,60,300,20, NULL, 0.0, 0, 0, 0, ""); 
			}

			uiBlockEndAlign(block);
			/*SOLVER SETTINGS*/
			/* done in another panel now*/
		}

		uiDefBut(block, LABEL, 0, "",10,10,1,2, NULL, 0.0, 0, 0, 0, ""); /* tell UI we go to 10,10*/
	}
	uiBlockEndAlign(block);
}
static void object_softbodies_solver(Object *ob)
{
	SoftBody *sb=ob->soft;
	uiBlock *block;
	static int val;
	short *softflag=&ob->softflag, psys_cur=0, adaptive_mode=0;
	int ob_has_hair=psys_ob_has_hair(ob);
	if(!_can_softbodies_at_all(ob)) return;
	block= uiNewBlock(&curarea->uiblocks, "object_softbodies_solver", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Soft Body", "Physics"); 
	if(uiNewPanel(curarea, block, "Soft Body Solver", "Physics", 651, 0, 318, 204)==0) return;

	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);

	/* doubt that is really needed here but for now */ 
	if(ob_has_hair) {
		if(PE_get_current_num(ob) >= 0) {
			ParticleSystem *psys = PE_get_current(ob);
			if(psys) {
				sb = psys->soft;
				softflag = &psys->softflag;
				psys_cur = 1;
			}
		}
	}

	if(psys_cur) {
		if(*softflag & OB_SB_ENABLE)
			val = 1;
		else
			val = 0;
	}
	else
		val = modifiers_isSoftbodyEnabled(ob);

	if(!val) { 
		uiDefBut(block, LABEL, 0, "",10,10,1,2, NULL, 0.0, 0, 0, 0, ""); /* tell UI we go to 10,10*/
		if(psys_cur){
			uiDefBut(block, LABEL, 0, "Hair is not a softbody.",10,190,300,20, NULL, 0.0, 0, 0, 0, ""); 
		}
		else {
			uiDefBut(block, LABEL, 0, "Object is not a softbody.",10,190,300,20, NULL, 0.0, 0, 0, 0, ""); 
		}
	}
	else{ 
		if ((ob->type==OB_MESH)||(ob->type==OB_CURVE) ) {
			/*SOLVER SETTINGS*/
			uiBlockBeginAlign(block);
			uiDefBut(block, LABEL, 0, "Solver select",10,200,300,20, NULL, 0.0, 0, 0, 0, ""); 
			uiDefButS(block, MENU, B_BAKE_CACHE_CHANGE, sbsolvers,10,180,300,20, &sb->solver_ID, 14.0, 0.0, 0, 0, "Select Solver");
			uiBlockEndAlign(block);

			/*some have adapive step size - some not*/
			switch (sb->solver_ID) {
			case 0:
			case 1:
				{adaptive_mode = 1; break;}
			case 3:
				{adaptive_mode = 0; break;}
			default: printf("SB_solver?\n"); // should never happen
			}
			if(adaptive_mode){
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, 0, "Step size controls",10,160,300,20, NULL, 0.0, 0, 0, 0, "");
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Error Lim:",	10,140,280,20, &sb->rklimit , 0.001, 10.0, 10, 0, "The Runge-Kutta ODE solver error limit, low value gives more precision, high values speed");
				uiDefButBitS(block, TOG, SBSO_OLDERR, B_BAKE_CACHE_CHANGE,"V", 290,140,20,20, &sb->solverflags,  0,  0, 0, 0, "Use velocities for automagic step sizes");
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "MinS:", 10,120,150,20, &sb->minloops,  0.00,  30000.0, 10, 0, "Minimal # solver steps/frame ");
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "MaxS:", 160,120,150,20, &sb->maxloops,  0.00,  30000.0, 10, 0, "Maximal # solver steps/frame ");
				uiBlockEndAlign(block);

				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, 0, "Collision helpers",10,100,300,20, NULL, 0.0, 0, 0, 0, "");
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Choke:", 10,80,150,20, &sb->choke, 0.00,  100.0, 10, 0, "'Viscosity' inside collision target ");
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Fuzzy:", 160,80,150,20, &sb->fuzzyness,  1.00,  100.0, 10, 0, "Fuzzyness while on collision, high values make collsion handling faster but less stable");
				uiBlockEndAlign(block);

				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, 0, "Diagnosis",10,60,300,20, NULL, 0.0, 0, 0, 0, "");
				uiDefButBitS(block, TOG, SBSO_MONITOR, B_BAKE_CACHE_CHANGE,"Print Performance to Console", 10,40,300,20, &sb->solverflags,  0,  0, 0, 0, "Turn on SB diagnose console prints");				
				uiBlockEndAlign(block);
			} 
			else{
				uiBlockEndAlign(block);
				uiBlockBeginAlign(block);
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Fuzzy:", 210,100,90,20, &sb->fuzzyness,  1.00,  100.0, 10, 0, "Fuzzyness while on collision, high values make collsion handling faster but less stable");
				uiDefButBitS(block, TOG, SBSO_MONITOR, B_BAKE_CACHE_CHANGE,"M", 290,100,20,20, &sb->solverflags,  0,  0, 0, 0, "Turn on SB diagnose console prints");
				uiBlockEndAlign(block);
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Steps:", 10,80,100,20, &sb->minloops,  1.00,  30000.0, 10, 0, "Solver steps/frame ");
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Choke:", 210,80,100,20, &sb->choke, 0.00,  100.0, 10, 0, "'Viscosity' inside collision target ");
			}

			uiBlockEndAlign(block);

		}
	}
	uiBlockEndAlign(block);
}

static void object_softbodies(Object *ob)
{
	SoftBody *sb=ob->soft;
	ParticleSystem *psys=NULL;
	uiBlock *block;
	uiBut *but;
	ModifierData *md;
	static int val;
	short *softflag=&ob->softflag, psys_cur=0;
	int ob_has_hair = psys_ob_has_hair(ob);
	static short actsoft= -1;

    if(!_can_softbodies_at_all(ob)) return;
	block= uiNewBlock(&curarea->uiblocks, "object_softbodies", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Soft Body", "Physics"); 
	if(uiNewPanel(curarea, block, "Soft Body", "Physics", 640, 0, 318, 204)==0) return;
	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);

	if(ob_has_hair) {
		psys= psys_get_current(ob);

		if(psys && actsoft >= 0) {
			actsoft= psys_get_current_num(ob)+1;

			sb=psys->soft;
			softflag=&psys->softflag;
			psys_cur=1;
		}
		else
			actsoft= -1; /* -1 = object */
	}

	if(psys_cur && psys) {
		if(*softflag & OB_SB_ENABLE)
			val = 1;
		else
			val = 0;

		but = uiDefButI(block, TOG, REDRAWBUTSOBJECT, "Soft Body",	10,200,130,20, &val, 0, 0, 0, 0, "Sets hair to become soft body");
		uiButSetFunc(but, object_softbodies__enable_psys, ob, psys);
	}
	else {
		md = modifiers_findByType(ob, eModifierType_Softbody);
		val = (md != NULL);

		if(ob_has_hair)
			but = uiDefButI(block, TOG, REDRAWBUTSOBJECT, "Soft Body",	10,200,130,20, &val, 0, 0, 0, 0, "Sets object to become soft body");
		else
			but = uiDefButI(block, TOG, REDRAWBUTSOBJECT, "Soft Body",	10,200,130,20, &val, 0, 0, 0, 0, "Sets object to become soft body");

		uiButSetFunc(but, object_softbodies__enable, ob, NULL);

		if(md) {
			uiBlockBeginAlign(block);
			uiDefIconButBitI(block, TOG, eModifierMode_Render, B_BAKE_CACHE_CHANGE, ICON_SCENE, 145, 200, 20, 20,&md->mode, 0, 0, 1, 0, "Enable soft body during rendering");
			but= uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_BAKE_CACHE_CHANGE, VICON_VIEW3D, 165, 200, 20, 20,&md->mode, 0, 0, 1, 0, "Enable soft body during interactive display");
			uiBlockEndAlign(block);
		}
	}

	if(ob_has_hair) {
		char *menustr = psys_menu_string(ob,1);

		but=uiDefButS(block, MENU, B_BAKE_REDRAWEDIT, menustr, 210,200,100,20, &actsoft, 14.0, 0.0, 0, 0, "Browse systems");
		uiButSetFunc(but, PE_change_act, ob, &actsoft);
		
		MEM_freeN(menustr);
	}


	
	uiDefBut(block, LABEL, 0, "",10,10,300,0, NULL, 0.0, 0, 0, 0, ""); /* tell UI we go to 10,10*/

	if(val) {
		int defCount;
		char *menustr;
		static char str[128];

		if(sb->pointcache->flag & PTCACHE_BAKED)
			uiSetButLock(1, "Simulation frames are baked");

		//if(ob->softflag & OB_SB_BAKESET) {
		//	uiBlockBeginAlign(block);
		//	uiDefButI(block, NUM, B_DIFF, "Start:",			10, 170,100,20, &sb->sfra, 1.0, 10000.0, 10, 0, "Start frame for baking");
		//	uiDefButI(block, NUM, B_DIFF, "End:",			110, 170,100,20, &sb->efra, 1.0, 10000.0, 10, 0, "End frame for baking");
		//	uiDefButI(block, NUM, B_DIFF, "Interval:",		210, 170,100,20, &sb->interval, 1.0, 10.0, 10, 0, "Interval in frames between baked keys");
		//	uiBlockEndAlign(block);

		//	uiDefButS(block, TOG, B_DIFF, "Local",			10, 145,100,20, &sb->local, 0.0, 0.0, 0, 0, "Use local coordinates for baking");


		//	uiClearButLock();
		//	uiBlockBeginAlign(block);

		//	if(sb->keys) {
		//		char str[128];
		//		uiDefIconTextBut(block, BUT, B_SOFTBODY_BAKE_FREE, ICON_X, "FREE BAKE", 10, 120,300,20, NULL, 0.0, 0.0, 0, 0, "Free baked result");
		//		sprintf(str, "Stored %d vertices %d keys %.3f MB", sb->totpoint, sb->totkey, ((float)16*sb->totpoint*sb->totkey)/(1024.0*1024.0));
		//		uiDefBut(block, LABEL, 0, str, 10, 100,300,20, NULL, 0.0, 0.0, 00, 0, "");
		//	}
		//	else				
		//		uiDefBut(block, BUT, B_SOFTBODY_BAKE, "BAKE",	10, 120,300,20, NULL, 0.0, 0.0, 10, 0, "Start baking. Press ESC to exit without baking");
		//}
		//else {
			/* GENERAL STUFF */
			if (sb->totpoint){
			sprintf(str, "Vertex Mass; Object mass %f [k]",sb->nodemass*sb->totpoint/1000.0f);
			}
			else{
			sprintf(str, "Vertex Mass");
			}
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Friction:", 10, 170,150,20, &sb->mediafrict, 0.0, 50.0, 10, 0, "General media friction for point movements");
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Mass:",	   160, 170,150,20, &sb->nodemass , 0.001, 50000.0, 10, 0, str);
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Grav:",	   10,150,150,20, &sb->grav , -10.0, 10.0, 10, 0, "Apply gravitation to point movement");
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Speed:",	   160,150,150,20, &sb->physics_speed , 0.01, 100.0, 10, 0, "Tweak timing for physics to control frequency and speed");
			uiBlockEndAlign(block);

			/* GOAL STUFF */
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, OB_SB_GOAL, B_BAKE_CACHE_CHANGE, "Use Goal",	10,120,130,20, softflag, 0, 0, 0, 0, "Define forces for vertices to stick to animated position");
			if (*softflag & OB_SB_GOAL){
				if(ob->type==OB_MESH) {
					menustr= get_vertexgroup_menustr(ob);
					defCount=BLI_countlist(&ob->defbase);
					if(defCount==0) sb->vertgroup= 0;
					uiDefButS(block, MENU, B_BAKE_CACHE_CHANGE, menustr,	140,120,20,20, &sb->vertgroup, 0, defCount, 0, 0, "Browses available vertex groups");
					MEM_freeN (menustr);

					if(sb->vertgroup) {
						bDeformGroup *defGroup = BLI_findlink(&ob->defbase, sb->vertgroup-1);
						if(defGroup)
							uiDefBut(block, BUT, B_BAKE_CACHE_CHANGE, defGroup->name,	160,120,130,20, NULL, 0.0, 0.0, 0, 0, "Name of current vertex group");
						else
							uiDefBut(block, BUT, B_BAKE_CACHE_CHANGE, "(no group)",	160,120,130,20, NULL, 0.0, 0.0, 0, 0, "Vertex Group doesn't exist anymore");
						uiDefIconBut(block, BUT, B_SOFTBODY_DEL_VG, ICON_X, 290,120,20,20, 0, 0, 0, 0, 0, "Disable use of vertex group");
					}
					else
						uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Goal:",	160,120,150,20, &sb->defgoal, 0.0, 1.0, 10, 0, "Default Goal (vertex target position) value, when no Vertex Group used");
				}
				else {
					uiDefButS(block, TOG, B_BAKE_CACHE_CHANGE, "W",			140,120,20,20, &sb->vertgroup, 0, 1, 0, 0, "Use control point weight values");
					uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Goal:",	160,120,150,20, &sb->defgoal, 0.0, 1.0, 10, 0, "Default Goal (vertex target position) value, when no Vertex Group used");
				}

				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "G Stiff:",	10,100,150,20, &sb->goalspring, 0.0, 0.999, 10, 0, "Goal (vertex target position) spring stiffness");
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "G Damp:",	160,100,150,20, &sb->goalfrict  , 0.0, 50.0, 10, 0, "Goal (vertex target position) friction");
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "G Min:",		10,80,150,20, &sb->mingoal, 0.0, 1.0, 10, 0, "Goal minimum, vertex group weights are scaled to match this range");
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "G Max:",		160,80,150,20, &sb->maxgoal, 0.0, 1.0, 10, 0, "Goal maximum, vertex group weights are scaled to match this range");
			}
			uiBlockEndAlign(block);

			/* EDGE SPRING STUFF */
			if(ob->type!=OB_SURF) {
				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, OB_SB_EDGES, B_BAKE_CACHE_CHANGE, "Use Edges",		10,50,90,20, softflag, 0, 0, 0, 0, "Use Edges as springs");
			if (*softflag & OB_SB_EDGES){
				uiDefButBitS(block, TOG, OB_SB_QUADS, B_BAKE_CACHE_CHANGE, "Stiff Quads",		110,50,90,20, softflag, 0, 0, 0, 0, "Adds diagonal springs on 4-gons");
				uiDefButBitS(block, TOG, OB_SB_EDGECOLL, B_BAKE_CACHE_CHANGE, "CEdge",		220,50,45,20, softflag, 0, 0, 0, 0, "Edge collide too"); 
				uiDefButBitS(block, TOG, OB_SB_FACECOLL, B_BAKE_CACHE_CHANGE, "CFace",		265,50,45,20, softflag, 0, 0, 0, 0, "Faces collide too SLOOOOOW warning "); 
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Pull:",	10,30,75,20, &sb->inspring, 0.0,  0.999, 10, 0, "Edge spring stiffness when longer than rest length");
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Push:",	85,30,75,20, &sb->inpush, 0.0,  0.999, 10, 0, "Edge spring stiffness when shorter than rest length");
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Damp:",	160,30,70,20, &sb->infrict, 0.0,  50.0, 10, 0, "Edge spring friction");
			    uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "SL:",250 ,30,60,20, &sb->springpreload, 0.0,  200.0, 10, 0, "Alter spring lenght to shrink/blow up (unit %) 0 to disable ");
				
				uiDefButBitS(block, TOG,OB_SB_AERO_ANGLE,B_BAKE_CACHE_CHANGE, "N",10,10,20,20, softflag, 0, 0, 0, 0, "New aero(uses angle and length)");
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Aero:",     30,10,60,20, &sb->aeroedge,  0.00,  30000.0, 10, 0, "Make edges 'sail'");
			    uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Plas:", 90,10,60,20, &sb->plastic, 0.0,  100.0, 10, 0, "Permanent deform");
				if(ob->type==OB_MESH) {
					uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Be:", 150,10,80,20, &sb->secondspring, 0.0,  10.0, 10, 0, "Bending Stiffness");
					if (*softflag & OB_SB_QUADS){ 
					uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Sh:", 230,10,80,20, &sb->shearstiff, 0.0,  1.0, 10, 0, "Shear Stiffness");
					}
				}
				else sb->secondspring = 0;
				uiDefBut(block, LABEL, 0, "",10,10,1,0, NULL, 0.0, 0, 0, 0, ""); /* tell UI we go to 10,10*/
			}
				uiBlockEndAlign(block);
			}
		//}
	}
	uiBlockEndAlign(block);
}

static void object_panel_particle_bake(Object *ob)
{
	uiBlock *block;
	ParticleSystem *psys= psys_get_current(ob);
	static PTCacheID staticpid;
	int libdata;

	if (psys==NULL || psys->part==NULL) return;
	if (ELEM(psys->part->type, PART_HAIR, PART_FLUID)) return;
	if (psys->part->phystype == PART_PHYS_KEYED) return;
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_bake", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Particle System", "Particle");
	if(uiNewPanel(curarea, block, "Bake", "Particle", 320, 0, 318, 204)==0) return;
	
	libdata= object_is_libdata(ob);
	uiSetButLock(libdata, ERROR_LIBDATA_MESSAGE);
	
	BKE_ptcache_id_from_particles(&staticpid, ob, psys);
	object_physics_bake_buttons(block, &staticpid, 10, libdata);
}

 /* Panels for new particles*/
static void object_panel_particle_children(Object *ob)
{
	uiBlock *block;
	ParticleSystem *psys = psys_get_current(ob);
	ParticleSettings *part;
	short butx=0, buty=160, butw=150, buth=20;
	static short kink_ui=0;

	if (psys==NULL) return;
	part=psys->part;
	if(part==NULL) return;
		
	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_child", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Extras", "Particle");
	if(uiNewPanel(curarea, block, "Children", "Particle", 1300, 0, 318, 204)==0) return;

	uiSetButLock((part->id.lib != NULL), ERROR_LIBDATA_MESSAGE);

	if(part->type == PART_FLUID) {
		uiDefBut(block, LABEL, 0, "No settings for fluid particles",					butx,(buty-=2*buth),2*butw,buth, NULL, 0.0, 0, 0, 0, "");
		return;
	}

	uiDefButS(block, MENU, B_PART_ALLOC_CHILD, "Children from:%t|Faces%x2|Particles%x1|None%x0", butx,buty,butw,buth, &part->childtype, 14.0, 0.0, 0, 0, "Create child particles");

	if(part->childtype==0) return;

	if(part->childtype==PART_CHILD_FACES && !(part->phystype==PART_PHYS_KEYED || part->type==PART_HAIR)) {
		uiDefBut(block, LABEL, 0, "Hair or keyed",	butx,(buty-=2*buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiDefBut(block, LABEL, 0, "particles needed!",	butx,(buty-=2*buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		return;
	}

	uiBlockBeginAlign(block);

	buty -= buth/2;
	
	uiDefButI(block, NUM, B_PART_ALLOC_CHILD, "Amount:", butx,(buty-=buth),butw,buth, &part->child_nbr, 0.0, MAX_PART_CHILDREN, 0, 0, "Amount of children/parent");
	uiDefButI(block, NUM, B_DIFF, "Render Amount:", butx,(buty-=buth),butw,buth, &part->ren_child_nbr, 0.0, MAX_PART_CHILDREN, 0, 0, "Amount of children/parent for rendering");

	if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES) {
		uiDefButF(block, NUMSLI, B_PART_DISTR_CHILD, "VParents:",		butx,(buty-=buth),butw,buth, &part->parents, 0.0, 1.0, 1, 3, "Relative amount of virtual parents");
		}
	else {
		uiDefButF(block, NUM, B_PART_RECALC_CHILD, "Rad:",		butx,(buty-=buth),butw,buth, &part->childrad, 0.0, 10.0, 1, 3, "Radius of children around parent");
		uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Round:",		butx,(buty-=buth),butw,buth, &part->childflat, 0.0, 1.0, 1, 3, "Roundness of children around parent");
	}
	uiBlockEndAlign(block);

	buty -= buth/2;

	/* clump */
	uiBlockBeginAlign(block);
	uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Clump:",		butx,(buty-=buth),butw,buth, &part->clumpfac, -1.0, 1.0, 1, 3, "Amount of clumpimg");
	uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Shape:",		butx,(buty-=buth),butw,buth, &part->clumppow, -0.999, 0.999, 1, 3, "Shape of clumpimg");
	uiBlockEndAlign(block);

	buty -= buth/2;

	uiBlockBeginAlign(block);
	if(part->draw_as != PART_DRAW_PATH) {
		uiDefButF(block, NUM, B_PART_REDRAW, "Size:",		butx,(buty-=buth),butw/2,buth, &part->childsize, 0.01, 100, 10, 1, "A multiplier for the child particle size");
		uiDefButF(block, NUM, B_PART_REDRAW, "Rand:",		butx+butw/2,buty,butw/2,buth, &part->childrandsize, 0.0, 1.0, 10, 1, "Random variation to the size of the child particles");
	}
	if(part->childtype == PART_CHILD_FACES) {
		/* only works if children could be emitted from volume, but that option isn't available now */
		/*uiDefButF(block, NUM, B_PART_REDRAW, "Spread:",butx,(buty-=buth),butw/2,buth, &part->childspread, -1.0, 1.0, 10, 1, "Spread children from the faces");*/
		uiDefButBitI(block, TOG, PART_CHILD_SEAMS, B_PART_DISTR_CHILD, "Use Seams",	butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Use seams to determine parents");
	}
	uiBlockEndAlign(block);

	butx=160;
	buty=180;

	if(part->phystype==PART_PHYS_KEYED || part->type==PART_HAIR)
		uiDefButBitS(block, TOG, 1, B_PART_REDRAW, "Kink/Branch",	 butx,(buty-=buth),butw,buth, &kink_ui, 0, 0, 0, 0, "Show kink and branch options");
	else
		buty-=buth;

	if(kink_ui || !(part->phystype==PART_PHYS_KEYED || part->type==PART_HAIR)) {
		buty -= buth/2;

		/* kink */
		uiBlockBeginAlign(block);
		if(part->kink) {
			uiDefButS(block, MENU, B_PART_RECALC_CHILD, "Kink:%t|Braid%x4|Wave%x3|Radial%x2|Curl%x1|Nothing%x0", butx,(buty-=buth),butw/2,buth, &part->kink, 14.0, 0.0, 0, 0, "Type of periodic offset on the path");
			uiDefButS(block, MENU, B_PART_RECALC_CHILD, "Axis %t|Z %x2|Y %x1|X %x0", butx+butw/2,buty,butw/2,buth, &part->kink_axis, 14.0, 0.0, 0, 0, "Which axis to use for offset");
			uiDefButF(block, NUM, B_PART_RECALC_CHILD, "Freq:",			butx,(buty-=buth),butw,buth, &part->kink_freq, 0.0, 10.0, 1, 3, "The frequency of the offset (1/total length)");
			uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Shape:",		butx,(buty-=buth),butw,buth, &part->kink_shape, -0.999, 0.999, 1, 3, "Adjust the offset to the beginning/end");
			uiDefButF(block, NUM, B_PART_RECALC_CHILD, "Amplitude:",	butx,(buty-=buth),butw,buth, &part->kink_amp, 0.0, 10.0, 1, 3, "The amplitude of the offset");
		}
		else {
			uiDefButS(block, MENU, B_PART_RECALC_CHILD, "Kink:%t|Braid%x4|Wave%x3|Radial%x2|Curl%x1|Nothing%x0", butx,(buty-=buth),butw,buth, &part->kink, 14.0, 0.0, 0, 0, "Type of periodic offset on the path");
			buty-=3*buth;
		}
		uiBlockEndAlign(block);

		if(part->childtype==PART_CHILD_PARTICLES && (part->phystype==PART_PHYS_KEYED || part->type==PART_HAIR)) {
			if(part->flag & PART_BRANCHING) {
				uiDefButBitI(block, TOG, PART_BRANCHING, B_PART_RECALC_CHILD, "Branching",	butx,(buty-=2*buth),butw,buth, &part->flag, 0, 0, 0, 0, "Branch child paths from eachother");
				uiDefButBitI(block, TOG, PART_ANIM_BRANCHING, B_PART_RECALC_CHILD, "Animated",	butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Animate branching");
				uiDefButBitI(block, TOG, PART_SYMM_BRANCHING, B_PART_RECALC_CHILD, "Symmetric",	butx+butw/2,buty,butw/2,buth, &part->flag, 0, 0, 0, 0, "Start and end points are the same");
				uiDefButF(block, NUM, B_PART_RECALC_CHILD, "Threshold:",	butx,(buty-=buth),butw,buth, &part->branch_thres, 0.0, 1.0, 1, 3, "Threshold of branching");
			}
			else
				uiDefButBitI(block, TOG, PART_BRANCHING, B_PART_RECALC_CHILD, "Branching",	butx,(buty-=2*buth),butw,buth, &part->flag, 0, 0, 0, 0, "Branch child paths from eachother");
		}
	}
	else {
	/* rough */
		buty -= buth/2;

		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Rough1:",	butx,(buty-=buth),butw,buth, &part->rough1, 0.0, 10.0, 1, 3, "Amount of location dependant rough");
		uiDefButF(block, NUM, B_PART_RECALC_CHILD, "Size1:",	butx,(buty-=buth),butw,buth, &part->rough1_size, 0.01, 10.0, 1, 3, "Size of location dependant rough");
		uiBlockEndAlign(block);
		buty -= buth/2;
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Rough2:",	butx,(buty-=buth),butw,buth, &part->rough2, 0.0, 10.0, 1, 3, "Amount of random rough");
		uiDefButF(block, NUM, B_PART_RECALC_CHILD, "Size2:",	butx,(buty-=buth),butw,buth, &part->rough2_size, 0.01, 10.0, 1, 3, "Size of random rough");
		uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Thresh:",	butx,(buty-=buth),butw,buth, &part->rough2_thres, 0.00, 1.0, 1, 3, "Amount of particles left untouched by random rough");
		uiBlockEndAlign(block);
		buty -= buth/2;
		uiBlockBeginAlign(block);
		uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "RoughE:",	butx,(buty-=buth),butw,buth, &part->rough_end, 0.0, 10.0, 1, 3, "Amount of end point rough");
		uiDefButF(block, NUMSLI, B_PART_RECALC_CHILD, "Shape:",	butx,(buty-=buth),butw,buth, &part->rough_end_shape, 0.0, 10.0, 1, 3, "Shape of end point rough");
		uiBlockEndAlign(block);
	}
}
static void particle_set_vg(void *ob_v, void *vgnum_v)
{
	Object *ob= ob_v;
	ParticleSystem *psys=psys_get_current(ob);
	short vgnum = *((short *)vgnum_v);

	if(vgnum==PSYS_VG_DENSITY)
		psys->recalc|=PSYS_DISTR;
	else if(vgnum!=PSYS_VG_SIZE)
		psys->recalc|=PSYS_INIT;

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
}
static void particle_del_vg(void *ob_v, void *vgnum_v)
{
	Object *ob= ob_v;
	ParticleSystem *psys=psys_get_current(ob);
	short vgnum = *((short *)vgnum_v);

	if(vgnum==PSYS_VG_DENSITY) {
		psys->recalc|=PSYS_DISTR;
	}

	psys->vgroup[vgnum]=0;

	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
}
static void object_panel_particle_extra(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	ParticleSystem *psys=psys_get_current(ob);
	ParticleSettings *part;
	short butx=0, buty=160, butw=150, buth=20;
	static short vgnum=0;

	if (psys==NULL) return;
	part=psys->part;
	if(part==NULL) return;
		
	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_extra", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Extras", "Particle", 980, 0, 318, 204)==0) return;

	uiSetButLock((part->id.lib != NULL), ERROR_LIBDATA_MESSAGE);

	if(part->type == PART_FLUID) {
		uiDefBut(block, LABEL, 0, "No settings for fluid particles",					butx,(buty-=2*buth),2*butw,buth, NULL, 0.0, 0, 0, 0, "");
		return;
	}

	uiDefBut(block, LABEL, 0, "Effectors:",	butx,buty,butw,buth, NULL, 0.0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefIDPoinBut(block, test_grouppoin_but, ID_GR, B_PART_RECALC, "GR:", butx, (buty-=buth), butw/2, buth, &part->eff_group, "Limit effectors to this Group"); 
	uiDefButBitI(block, TOG, PART_SIZE_DEFL, B_PART_RECALC, "Size Deflect",	butx+butw/2,buty,butw/2,buth, &part->flag, 0, 0, 0, 0, "Use particle's size in deflection");
	uiDefButBitI(block, TOG, PART_DIE_ON_COL, B_PART_RECALC, "Die on hit",butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Particles die when they collide with a deflector object");
	uiDefButBitI(block, TOG, PART_STICKY, B_PART_RECALC, "Sticky",	butx+butw/2,buty,butw/2,buth, &part->flag, 0, 0, 0, 0, "Particles stick to collided objects if they die in the collision");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 0, "Time:",	butx,(buty-=buth),butw/3,buth, NULL, 0.0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitI(block, TOG, PART_GLOB_TIME, B_PART_RECALC, "Global",	 butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Set all ipos that work on particles to be calculated in global/object time");
	uiDefButBitI(block, TOG, PART_ABS_TIME, B_PART_RECALC, "Absolute",	 butx+butw/2,buty,butw/2,buth, &part->flag, 0, 0, 0, 0, "Set all ipos that work on particles to be calculated in absolute/relative time");

	//if(part->flag & PART_LOOP){
	//	uiDefButBitI(block, TOG, PART_LOOP, B_PART_RECALC, "Loop",	 butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Loop particle lives");
	//	uiDefButBitI(block, TOG, PART_LOOP_INSTANT, B_PART_RECALC, "Instantly",	 butx+butw/2,buty,butw/2,buth, &part->flag, 0, 0, 0, 0, "Loop particle life at time of death");
	//}
	//else
		uiDefButBitI(block, TOG, PART_LOOP, B_PART_RECALC, "Loop",	 butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Loop particle lives");

	uiDefButF(block, NUM, B_PART_RECALC, "Tweak:",	butx,(buty-=buth),butw,buth, &part->timetweak, 0.0, 10.0, 1, 0, "A multiplier for physics timestep (1.0 means one frame = 1/25 seconds)");
	uiBlockEndAlign(block);

	if(ob->type==OB_MESH) {
		char *menustr= get_vertexgroup_menustr(ob);
		int defCount=BLI_countlist(&ob->defbase);
		if(defCount==0) psys->vgroup[vgnum]= 0;

		uiDefBut(block, LABEL, 0, "Vertex group:",	butx,(buty-=2*buth),butw,buth, NULL, 0.0, 0, 0, 0, "");

		uiBlockBeginAlign(block);
		
		uiDefButS(block, MENU, B_PART_REDRAW, "Attribute%t|Effector%x11|TanRot%x10|TanVel%x9|Size%x8|RoughE%x7|Rough2%x6|Rough1%x5|Kink%x4|Clump%x3|Length%x2|Velocity%x1|Density%x0", butx,(buty-=buth),butw-40,buth, &vgnum, 14.0, 0.0, 0, 0, "Attribute effected by vertex group");
		but=uiDefButBitS(block, TOG, (1<<vgnum), B_PART_RECALC, "Neg",	butx+butw-40,buty,40,buth, &psys->vg_neg, 0, 0, 0, 0, "Negate the effect of the vertex group");
		uiButSetFunc(but, particle_set_vg, (void *)ob, (void *)(&vgnum));
		
		butx+=butw;

		but= uiDefButS(block, MENU, B_PART_RECALC, menustr,	butx,buty,buth,buth, psys->vgroup+vgnum, 0, defCount, 0, 0, "Browses available vertex groups");
		uiButSetFunc(but, particle_set_vg, (void *)ob, (void *)(&vgnum));
		MEM_freeN (menustr);

		if(psys->vgroup[vgnum]) {
			bDeformGroup *defGroup = BLI_findlink(&ob->defbase, psys->vgroup[vgnum]-1);
			if(defGroup)
				uiDefBut(block, BUT, B_PART_REDRAW, defGroup->name,	butx+buth,buty,butw-2*buth,buth, NULL, 0.0, 0.0, 0, 0, "Name of current vertex group");
			else{
				uiDefBut(block, BUT, B_PART_REDRAW, "(no group)",	butx+buth,buty,butw-2*buth,buth, NULL, 0.0, 0.0, 0, 0, "Vertex Group doesn't exist anymore");
			}
			but=uiDefIconBut(block, BUT, B_PART_RECALC, ICON_X, butx+butw-buth,buty,buth,buth, 0, 0, 0, 0, 0, "Disable use of vertex group");
			uiButSetFunc(but, particle_del_vg, (void *)ob, (void *)(&vgnum));
		}

		uiBlockEndAlign(block);
	}
	
	buty=butx=160;

	uiDefButI(block, NUM, B_PART_DISTR, "Seed:",				butx,(buty-=buth),butw,buth, &psys->seed, 0.0, 255.0, 1, 0, "Set an offset in the random table");
	if(part->type == PART_HAIR) {
		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, B_PART_RECALC, "Stiff:",	butx,(buty-=buth),(butw*3)/5,buth, &part->eff_hair, 0.0, 1.0, 0, 0, "Hair stiffness for effectors");
		uiDefButBitI(block, TOG, PART_CHILD_EFFECT, B_PART_RECALC, "Children", butx+(butw*3)/5,buty,(butw*2)/5,buth, &part->flag, 0, 0, 0, 0, "Apply effectors to children");
		uiBlockEndAlign(block);
	}
	else
		buty-=buth;

	/* size changes must create a recalc event always so that sizes are updated properly */
	uiDefButF(block, NUM, B_PART_RECALC, "Size:",	butx,(buty-=buth),butw,buth, &part->size, 0.01, 100, 10, 1, "The size of the particles");
	uiDefButF(block, NUM, B_PART_RECALC, "Rand:",	butx,(buty-=buth),butw,buth, &part->randsize, 0.0, 2.0, 10, 1, "Give the particle size a random variation");

	uiDefButBitI(block, TOG, PART_SIZEMASS, B_PART_RECALC, "Mass from size",	 butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Multiply mass with particle size");
	uiDefButF(block, NUM, B_PART_RECALC, "Mass:",	butx,(buty-=buth),butw,buth, &part->mass, 0.01, 100, 10, 1, "Specify the mass of the particles");
}
/* copy from buttons_shading.c */
static void autocomplete_uv(char *str, void *arg_v)
{
	Mesh *me;
	CustomDataLayer *layer;
	AutoComplete *autocpl;
	int a;

	if(str[0]==0)
		return;

	autocpl= autocomplete_begin(str, 32);
		
	/* search if str matches the beginning of name */
	for(me= G.main->mesh.first; me; me=me->id.next)
		for(a=0, layer= me->fdata.layers; a<me->fdata.totlayer; a++, layer++)
			if(layer->type == CD_MTFACE)
				autocomplete_do_name(autocpl, layer->name);
	
	autocomplete_end(autocpl, str);
}
static void object_panel_particle_visual(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	ParticleSystem *psys=psys_get_current(ob);
	ParticleSettings *part;
	short butx=0, buty=160, butw=150, buth=20;
	static short bbuvnum=0;

	if (psys==NULL) return;
	part=psys->part;
	if(part==NULL) return;
		
	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_visual", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Visualization", "Particle", 640, 0, 318, 204)==0) return;

	uiDefButS(block, MENU, B_PART_RECALC, "Billboard %x9|Group %x8|Object %x7|Path %x6|Line %x5|Axis %x4|Cross %x3|Circle %x2|Point %x1|None %x0", butx,buty,butw,buth, &part->draw_as, 14.0, 0.0, 0, 0, "How particles are visualized");

	if(part->draw_as==PART_DRAW_NOT) {
		uiDefButBitS(block, TOG, PART_DRAW_EMITTER, B_PART_REDRAW, "Render emitter",	butx,(buty-=2*buth),butw,buth, &part->draw, 0, 0, 0, 0, "Render emitter object");
		return;
	}

	uiDefBut(block, LABEL, 0, "Draw:",	butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, PART_DRAW_VEL, B_PART_REDRAW, "Vel",	butx,(buty-=buth),butw/3,buth, &part->draw, 0, 0, 0, 0, "Show particle velocity");
	uiDefButBitS(block, TOG, PART_DRAW_SIZE, B_PART_REDRAW, "Size",	butx+butw/3,buty,butw/3,buth, &part->draw, 0, 0, 0, 0, "Show particle size");
	uiDefButBitS(block, TOG, PART_DRAW_NUM, B_PART_REDRAW, "Num",	butx+2*butw/3,buty,butw/3,buth, &part->draw, 0, 0, 0, 0, "Show particle number");
	uiDefButS(block, NUM, B_PART_REDRAW, "Draw Size:", butx,(buty-=buth),butw,buth, &part->draw_size, 0.0, 10.0, 0, 0, "Size of particles on viewport in pixels (0=default)");
	uiDefButS(block, NUM, B_PART_RECALC_CHILD, "Disp:",		butx,(buty-=buth),butw,buth, &part->disp, 0.0, 100.0, 10, 0, "Percentage of particles to display in 3d view");
	uiBlockEndAlign(block);

	uiDefBut(block, LABEL, 0, "Render:",	butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, B_PART_DISTR, "Material:",					butx,(buty-=buth),butw-30,buth, &part->omat, 1.0, 16.0, 0, 0, "Specify material used for the particles");
	uiDefButBitS(block, TOG, PART_DRAW_MAT_COL, B_PART_RECALC, "Col",	butx+butw-30,buty,30,buth, &part->draw, 0, 0, 0, 0, "Draw particles using material's diffuse color");
	uiDefButBitS(block, TOG, PART_DRAW_EMITTER, B_PART_REDRAW, "Emitter",	butx,(buty-=buth),butw/2,buth, &part->draw, 0, 0, 0, 0, "Render emitter Object also");
	uiDefButBitS(block, TOG, PART_DRAW_PARENT, B_PART_REDRAW, "Parents",				butx+butw/2,buty,butw/2,buth, &part->draw, 0, 0, 0, 0, "Render parent particles");
	uiDefButBitI(block, TOG, PART_UNBORN, B_PART_REDRAW, "Unborn",			butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Show particles before they are emitted");
	uiDefButBitI(block, TOG, PART_DIED, B_PART_REDRAW, "Died",				butx+butw/2,buty,butw/2,buth, &part->flag, 0, 0, 0, 0, "Show particles after they have died");

	uiBlockEndAlign(block);

	butx=160;
	buty=160-buth;

	uiBlockBeginAlign(block);

	switch(part->draw_as) {
		case PART_DRAW_OB:
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_PART_REDRAW_DEPS, "OB:",	butx,(buty-=buth),butw,buth, &part->dup_ob, "Show this Object in place of particles"); 
			break;
		case PART_DRAW_GR:
			uiDefIDPoinBut(block, test_grouppoin_but, ID_GR, B_PART_REDRAW_DEPS, "GR:",	butx,(buty-=buth),butw,buth, &part->dup_group, "Show Objects in this Group in place of particles"); 
			uiDefButBitS(block, TOG, PART_DRAW_WHOLE_GR, B_PART_REDRAW, "Dupli Group",	butx,(buty-=buth),butw,buth, &part->draw, 0, 0, 0, 0, "Use whole group at once");
			if((part->draw & PART_DRAW_WHOLE_GR)==0)
				uiDefButBitS(block, TOG, PART_DRAW_RAND_GR, B_PART_REDRAW, "Pick Random",	butx,(buty-=buth),butw,buth, &part->draw, 0, 0, 0, 0, "Pick objects from group randomly");
			break;
		case PART_DRAW_BB:
			uiDefButBitS(block, TOG, PART_DRAW_BB_LOCK, B_PART_REDRAW, "Lock",	butx,(buty+=buth),butw/2,buth, &part->draw, 0, 0, 0, 0, "Lock the billboards align axis");
			uiDefButS(block, MENU, B_PART_REDRAW, "Align to%t|Velocity%x4|View%x3|Z%x2|Y%x1|X%x0", butx+butw/2,buty,butw/2,buth, &part->bb_align, 14.0, 0.0, 0, 0, "In respect to what the billboards are aligned");
			uiDefButF(block, NUM, B_PART_REDRAW, "Tilt:", butx,(buty-=buth),butw/2,buth, &part->bb_tilt, -1.0, 1.0, 0, 0, "Tilt of the billboards");
			uiDefButF(block, NUM, B_PART_REDRAW, "Rand:", butx+butw/2,buty,butw/2,buth, &part->bb_rand_tilt, 0.0, 1.0, 0, 0, "Random tilt of the billboards");
			uiDefButS(block, NUM, B_PART_REDRAW, "UV Split:", butx,(buty-=buth),butw,buth, &part->bb_uv_split, 1.0, 10.0, 0, 0, "Amount of rows/columns to split uv coordinates for billboards");
			uiDefButS(block, MENU, B_PART_REDRAW, "Animate%t|Angle%x2|Time%x1|None%x0", butx,(buty-=buth),butw/2,buth, &part->bb_anim, 14.0, 0.0, 0, 0, "How to animate billboard textures");
			uiDefButS(block, MENU, B_PART_REDRAW, "Offset%t|Random%x2|Linear%x1|None%x0", butx+butw/2,buty,butw/2,buth, &part->bb_split_offset, 14.0, 0.0, 0, 0, "How to offset billboard textures");
			uiDefButF(block, NUM, B_PART_REDRAW, "OffsetX:", butx,(buty-=buth),butw,buth, part->bb_offset, -1.0, 1.0, 0, 0, "Offset billboards horizontally");
			uiDefButF(block, NUM, B_PART_REDRAW, "OffsetY:", butx,(buty-=buth),butw,buth, part->bb_offset+1, -1.0, 1.0, 0, 0, "Offset billboards vertically");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_PART_REDRAW, "OB:",	butx,(buty-=buth),butw,buth, &part->bb_ob, "Billboards face this object (default is active camera)"); 
			uiDefButS(block, MENU, B_PART_REDRAW, "UV channel%t|Split%x2|Time-Index (X-Y)%x1|Normal%x0", butx,(buty-=buth),butw,buth, &bbuvnum, 14.0, 0.0, 0, 0, "UV channel");
			but=uiDefBut(block, TEX, B_PART_REDRAW, "UV:", butx,(buty-=buth),butw,buth, psys->bb_uvname+bbuvnum, 0, 31, 0, 0, "Set name of UV layer to use with billboards, default is active UV layer");
			uiButSetCompleteFunc(but, autocomplete_uv, NULL);
			break;
		case PART_DRAW_LINE:
			uiDefButBitS(block, TOG, PART_DRAW_VEL_LENGTH, B_PART_REDRAW, "Speed",	butx,(buty-=buth),butw,buth, &part->draw, 0, 0, 0, 0, "Multiply line length by particle speed");
			uiDefButF(block, NUM, B_PART_REDRAW, "Back:", butx,(buty-=buth),butw,buth, &part->draw_line[0], 0.0, 10.0, 0, 0, "Length of the line's tail");
			uiDefButF(block, NUM, B_PART_REDRAW, "Front:", butx,(buty-=buth),butw,buth, &part->draw_line[1], 0.0, 10.0, 0, 0, "Length of the line's head");
			break;
		case PART_DRAW_PATH:
			if(part->phystype==PART_PHYS_KEYED || part->type==PART_HAIR) {
				uiDefButS(block, NUM, B_PART_RECALC, "Steps:",	butx,(buty+=buth),butw,buth, &part->draw_step, 0.0, 7.0, 0, 0, "How many steps paths are drawn with (power of 2)");
				uiDefButS(block, NUM, B_PART_REDRAW, "Render:",	butx,(buty-=buth),butw,buth, &part->ren_step, 0.0, 9.0, 0, 0, "How many steps paths are rendered with (power of 2)");

				uiDefButBitI(block, TOG, PART_ABS_LENGTH, B_PART_RECALC, "Abs Length",	 butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Use maximum length for children");
				uiDefButF(block, NUM, B_PART_RECALC, "Max Length:",		butx,(buty-=buth),butw,buth, &part->abslength, 0.0, 10000.0, 1, 3, "Absolute maximum path length for children, in blender units");
				uiDefButF(block, NUMSLI, B_PART_RECALC, "RLength:",		butx,(buty-=buth),butw,buth, &part->randlength, 0.0, 1.0, 1, 3, "Give path length a random variation");
				uiBlockEndAlign(block);

				uiDefButBitI(block, TOG, PART_HAIR_BSPLINE, B_PART_RECALC, "B-Spline",	 butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Interpolate hair using B-Splines");

				uiBlockBeginAlign(block);
				uiDefButBitS(block, TOG, PART_DRAW_REN_STRAND, B_PART_REDRAW, "Strand render",	 butx,buty-=buth,butw,buth, &part->draw, 0, 0, 0, 0, "Use the strand primitive for rendering");
				if(part->draw & PART_DRAW_REN_STRAND) {
					uiDefButS(block, NUM, B_PART_REDRAW, "Angle:",	butx,(buty-=buth),butw,buth, &part->adapt_angle, 0.0, 45.0, 0, 0, "How many degrees path has to curve to make another render segment");
				}
				else {
					uiDefButBitS(block, TOG, PART_DRAW_REN_ADAPT, B_PART_REDRAW, "Adaptive render",	 butx,buty-=buth,butw,buth, &part->draw, 0, 0, 0, 0, "Draw steps of the particle path");
					if(part->draw & PART_DRAW_REN_ADAPT) {
						uiDefButS(block, NUM, B_PART_REDRAW, "Angle:",	butx,(buty-=buth),butw/2,buth, &part->adapt_angle, 0.0, 45.0, 0, 0, "How many degrees path has to curve to make another render segment");
						uiDefButS(block, NUM, B_PART_REDRAW, "Pixel:",	butx+butw/2,buty,(butw+1)/2,buth, &part->adapt_pix, 0.0, 50.0, 0, 0, "How many pixels path has to cover to make another render segment");
					}
				}
			}
			else {
				uiDefBut(block, LABEL, 0, "Hair or keyed",	butx,(buty-=2*buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
				uiDefBut(block, LABEL, 0, "particles needed!",	butx,(buty-=2*buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
			}
			break;
	}
	uiBlockEndAlign(block);
}
static void object_panel_particle_simplification(Object *ob)
{
	uiBlock *block;
	ParticleSystem *psys=psys_get_current(ob);
	ParticleSettings *part;
	short butx=0, buty=160, butw=150, buth=20;

	if (psys==NULL) return;
	part=psys->part;
	if(part==NULL) return;

	if(part->draw_as!=PART_DRAW_PATH || !(part->draw & PART_DRAW_REN_STRAND))
		return;
	if(part->childtype!=PART_CHILD_FACES)
		return;
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_simplification", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Visualization", "Particle");
	if(uiNewPanel(curarea, block, "Simplification", "Particle", 640, 0, 318, 204)==0) return;

	uiBlockBeginAlign(block);
	uiDefButBitS(block, TOG, PART_SIMPLIFY_ENABLE, B_PART_REDRAW, "Child Simplification", butx,buty-=buth,butw,buth, &part->simplify_flag, 0, 0, 0, 0, "Remove child strands as the object becomes smaller on the screen");
	uiBlockEndAlign(block);
	if(part->simplify_flag & PART_SIMPLIFY_ENABLE) {
		buty -= 10;

		uiBlockBeginAlign(block);
		uiDefButS(block, NUM, B_NOP, "Reference Size:", butx,(buty-=buth),butw,buth, &part->simplify_refsize, 1.0, 32768.0, 0, 0, "Reference size size in pixels, after which simplification begins");
		uiDefButF(block, NUM, B_NOP, "Rate:", butx,(buty-=buth),butw,buth, &part->simplify_rate, 0.0, 1.0, 0, 0, "Speed of simplification");
		uiDefButF(block, NUM, B_NOP, "Transition:", butx,(buty-=buth),butw,buth, &part->simplify_transition, 0.0, 1.0, 0, 0, "Transition period for fading out strands");
		uiBlockEndAlign(block);

		buty -= 10;

		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, PART_SIMPLIFY_VIEWPORT, B_PART_REDRAW, "Viewport", butx,buty-=buth,butw,buth, &part->simplify_flag, 0, 0, 0, 0, "Remove child strands as the object goes outside the viewport");
		uiDefButF(block, NUM, B_NOP, "Rate:", butx,(buty-=buth),butw,buth, &part->simplify_viewport, 0.0, 0.999, 0, 0, "Speed of simplification");
		uiBlockEndAlign(block);
	}
	uiBlockEndAlign(block);
}
static void boidrule_moveDown(void *part_v, void *rule_v)
{
	ParticleSettings *part = part_v;
	char r, *rule = rule_v;

	int n= rule - part->boidrule;

	if(n+1 < BOID_TOT_RULES) {
		r=part->boidrule[n];
		part->boidrule[n]=part->boidrule[n+1];
		part->boidrule[n+1]=r;
	}
}
static void boidrule_moveUp(void *part_v, void *rule_v)
{
	ParticleSettings *part = part_v;
	char r, *rule = rule_v;

	int n= rule - part->boidrule;

	if(n-1 >= 0) {
		r=part->boidrule[n];
		part->boidrule[n]=part->boidrule[n-1];
		part->boidrule[n-1]=r;
	}
}
static void object_panel_particle_physics(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	ParticleSystem *psys=psys_get_current(ob);
	ParticleSettings *part;
	short butx=0, buty=160, butw=150, buth=20;

	if (psys==NULL) return;
	
	part=psys->part;

	if(part==NULL) return;
		
	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_physics", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Physics", "Particle", 320, 0, 318, 204)==0) return;
	
	if(part->type == PART_FLUID) {
		uiDefBut(block, LABEL, 0, "No settings for fluid particles",					butx,(buty-=2*buth),2*butw,buth, NULL, 0.0, 0, 0, 0, "");
		return;
	}

	if(ob->id.lib)
		uiSetButLock(1, "Can't edit library data");
	else if(psys->flag & PSYS_EDITED)
		uiSetButLock(1, "Hair is edited!");
	else if(psys->pointcache->flag & PTCACHE_BAKED)
		uiSetButLock(1, "Simulation frames are baked!");

	if(part->phystype==PART_PHYS_KEYED){
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, PSYS_FIRST_KEYED, B_PART_RECALC, "First",	 butx,buty,45,buth, &psys->flag, 0, 0, 0, 0, "Sets the system to be the starting point of keyed particles");
		uiDefButS(block, MENU, B_PART_RECALC, "Physics %t|Boids%x3|Keyed %x2|Newtonian %x1|None %x0", butx+45,buty,butw-45,buth, &part->phystype, 14.0, 0.0, 0, 0, "Select particle physics type");
		uiBlockEndAlign(block);
	}
	else
		uiDefButS(block, MENU, B_PART_RECALC, "Physics%t|Boids%x3|Keyed%x2|Newtonian%x1|None%x0", butx,buty,butw,buth, &part->phystype, 14.0, 0.0, 0, 0, "Select particle physics type");

	if(part->phystype==PART_PHYS_BOIDS) {
		int i;
		char *rules[BOID_TOT_RULES] = {"Collision", "Avoid", "Crowd", "Center", "AvVel", "Velocity", "Goal", "Level"};
		char *ruletext[BOID_TOT_RULES] = {
			"Avoid deflector objects",
			"Avoid predators",
			"Avoid other boids",
			"Get to flock center",
			"Maintain average velocity",
			"Match velocity of nearby boids",
			"Seek goal",
			"Keep the Z level"
		};
		/* left column */
		uiDefBut(block, LABEL, 0, "Behaviour:",		butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		for(i=0; i<BOID_TOT_RULES; i++) {
			uiBlockSetCol(block, TH_BUT_ACTION);

			but = uiDefIconBut(block, BUT, B_PART_RECALC, VICON_MOVE_UP, butx, (buty-=buth), 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Move rule up");
			uiButSetFunc(but, boidrule_moveUp, part, part->boidrule+i);

			but = uiDefIconBut(block, BUT, B_PART_RECALC, VICON_MOVE_DOWN, butx+20, buty, 20, 20, NULL, 0.0, 0.0, 0.0, 0.0, "Move rule down");
			uiButSetFunc(but, boidrule_moveDown, part, part->boidrule+i);

			uiBlockSetCol(block, TH_BUT_SETTING2);

			uiDefButF(block, NUM, B_PART_RECALC, rules[part->boidrule[i]],		butx+40,buty,butw-40,buth, part->boidfac+part->boidrule[i], -1.0, 2.0, 1, 3, ruletext[part->boidrule[i]]);
		}
		uiBlockSetCol(block, TH_AUTO);
		uiBlockEndAlign(block);

		buty=140;
		butx=160;
		
		uiDefBut(block, LABEL, 0, "Physics:",		butx,buty,butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, PART_BOIDS_2D, B_PART_RECALC, "2D",	 butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Constrain boids to a surface");
		uiDefButF(block, NUM, B_PART_RECALC, "MaxVelocity:",		butx,(buty-=buth),butw,buth, &part->max_vel, 0.0, 200.0, 1, 3, "Maximum velocity");
		uiDefButF(block, NUM, B_PART_RECALC, "AvVelocity:",		butx,(buty-=buth),butw,buth, &part->average_vel, 0.0, 1.0, 1, 3, "The usual speed % of max velocity");
		uiDefButF(block, NUM, B_PART_RECALC, "LatAcc:",		butx,(buty-=buth),butw,buth, &part->max_lat_acc, 0.0, 1.0, 1, 3, "Lateral acceleration % of max velocity");
		uiDefButF(block, NUM, B_PART_RECALC, "TanAcc:",		butx,(buty-=buth),butw,buth, &part->max_tan_acc, 0.0, 1.0, 1, 3, "Tangential acceleration % of max velocity");
		if(part->flag & PART_BOIDS_2D) {
			uiDefButF(block, NUM, B_PART_RECALC, "GroundZ:",		butx,(buty-=buth),butw,buth, &part->groundz, -100.0, 100.0, 1, 3, "Default Z value");
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_PARTTARGET, "OB:",	butx,(buty-=buth),butw,buth, &psys->keyed_ob, "Constrain boids to object's surface"); 
		}
		else {
			uiDefButF(block, NUM, B_PART_RECALC, "Banking:",		butx,(buty-=buth),butw,buth, &part->banking, -10.0, 10.0, 1, 3, "Banking of boids on turns (1.0==natural banking)");
			uiDefButF(block, NUM, B_PART_RECALC, "MaxBank:",		butx,(buty-=buth),butw,buth, &part->max_bank, 0.0, 1.0, 1, 3, "How much a boid can bank at a single step");
		}
		uiBlockEndAlign(block);
		uiDefButS(block, NUM, B_PART_RECALC, "N:",		butx,(buty-=buth),butw,buth, &part->boidneighbours, 1.0, 10.0, 1, 3, "How many neighbours to consider for each boid");
	}
	else {
		/* left column */
		uiDefBut(block, LABEL, 0, "Initial velocity:",		butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiBlockSetCol(block, TH_BUT_SETTING2);
		uiDefButF(block, NUM, B_PART_RECALC, "Object:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->obfac, -1.0, 1.0, 1, 3, "Let the object give the particle a starting speed");
		uiDefButF(block, NUM, B_PART_RECALC, "Normal:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->normfac, -200.0, 200.0, 1, 3, "Let the surface normal give the particle a starting speed");
		uiDefButF(block, NUM, B_PART_RECALC, "Random:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->randfac, 0.0, 200.0, 1, 3, "Give the starting speed a random variation");
		if(part->type==PART_REACTOR) {
			uiDefButF(block, NUM, B_PART_RECALC, "Particle:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->partfac, -10.0, 10.0, 1, 3, "Let the target particle give the particle a starting speed");
			uiDefButF(block, NUM, B_PART_RECALC, "Reactor:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->reactfac, -10.0, 10.0, 1, 3, "Let the vector away from the target particles location give the particle a starting speed");
		}
		else {
			uiDefButF(block, NUM, B_PART_RECALC, "Tan:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->tanfac, -200.0, 200.0, 1, 3, "Let the surface tangent give the particle a starting speed");
			uiDefButF(block, NUM, B_PART_RECALC, "Rot:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->tanphase, -1.0, 1.0, 1, 3, "Rotate the surface tangent");
		}
		uiBlockSetCol(block, TH_AUTO);
		uiBlockEndAlign(block);

		buty=160;
		butx=160;
		
		if(part->phystype==PART_PHYS_NEWTON)
			uiDefButS(block, MENU, B_PART_RECALC, "Integration%t|RK4%x2|Midpoint%x1|Euler%x0", butx,buty,butw,buth, &part->integrator, 14.0, 0.0, 0, 0, "Select physics integrator type");
		
		uiDefBut(block, LABEL, 0, "Rotation:",	butx, (buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, PART_ROT_DYN, B_PART_RECALC, "Dynamic",	 butx,(buty-=buth*4/5),butw/2,buth*4/5, &part->flag, 0, 0, 0, 0, "Sets rotation to dynamic/constant");
		uiDefButS(block, MENU, B_PART_RECALC, "Rotation%t|Object Z%x8|Object Y%x7|Object X%x6|Global Z%x5|Global Y%x4|Global X%x3|Velocity%x2|Normal%x1|None%x0", butx+butw/2,buty,butw/2,buth*4/5, &part->rotmode, 14.0, 0.0, 0, 0, "Particles initial rotation");
		uiBlockSetCol(block, TH_BUT_SETTING2);
		uiDefButF(block, NUM, B_PART_RECALC, "Random:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->randrotfac, 0.0, 1.0, 1, 3, "Randomize rotation");
		uiDefButF(block, NUM, B_PART_RECALC, "Phase:",			butx,(buty-=buth*4/5),butw/2,buth*4/5, &part->phasefac, -1.0, 1.0, 1, 3, "Initial rotation phase");
		uiDefButF(block, NUM, B_PART_RECALC, "Rand:",			butx+butw/2,buty,butw/2,buth*4/5, &part->randphasefac, 0.0, 1.0, 1, 3, "Randomize rotation phase");
		uiBlockSetCol(block, TH_AUTO);

		uiDefButS(block, MENU, B_PART_RECALC, "Angular v %t|Random%x2|Spin%x1|None%x0", butx,(buty-=buth*4/5),butw,buth*4/5, &part->avemode, 14.0, 0.0, 0, 0, "Select particle angular velocity mode");
		uiBlockSetCol(block, TH_BUT_SETTING2);
		if(ELEM(part->avemode,PART_AVE_RAND,PART_AVE_SPIN))
			uiDefButF(block, NUM, B_PART_RECALC, "Angular v:",		butx,(buty-=buth*4/5),butw,buth*4/5, &part->avefac, -200.0, 200.0, 1, 3, "Angular velocity amount");
   		uiBlockSetCol(block, TH_AUTO);
		uiBlockEndAlign(block);
		
		if(part->phystype==PART_PHYS_NEWTON) {
			butx=0;
			buty=40;
			uiDefBut(block, LABEL, 0, "Global effects:",	butx,buty,butw,buth, NULL, 0.0, 0, 0, 0, "");

			butw=103;
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_PART_RECALC, "AccX:",		butx,(buty-=buth),butw,buth, part->acc, -200.0, 200.0, 10, 0, "Specify a constant acceleration along the X-axis");
			uiDefButF(block, NUM, B_PART_RECALC, "AccY:",		butx+butw,buty,butw,buth, part->acc+1,-200.0, 200.0, 10, 0, "Specify a constant acceleration along the Y-axis");
			uiDefButF(block, NUM, B_PART_RECALC, "AccZ:",		butx+2*butw,buty,butw+1,buth, part->acc+2, -200.0, 200.0, 10, 0, "Specify a constant acceleration along the Z-axis");

			uiDefButF(block, NUM, B_PART_RECALC, "Drag:",		butx,(buty-=buth),butw,buth, &part->dragfac, 0.0, 1.0, 1, 0, "Specify the amount of air-drag");
			uiDefButF(block, NUM, B_PART_RECALC, "Brown:",		butx+butw,buty,butw,buth, &part->brownfac, 0.0, 200.0, 1, 0, "Specify the amount of brownian motion");
			uiDefButF(block, NUM, B_PART_RECALC, "Damp:",		butx+2*butw,buty,butw+1,buth, &part->dampfac, 0.0, 1.0, 1, 0, "Specify the amount of damping");
			uiBlockEndAlign(block);
		}
		else if(part->phystype==PART_PHYS_KEYED) {
			short totkpsys=1;
			butx=0;
			buty=40;
			uiDefBut(block, LABEL, 0, "Keyed Target:",							butx,buty,butw,buth, NULL, 0.0, 0, 0, 0, "");
			if(psys->keyed_ob){
				if(psys->keyed_ob==ob || BLI_findlink(&psys->keyed_ob->particlesystem,psys->keyed_psys-1)==0)
					uiBlockSetCol(block, TH_REDALERT);
				else
					totkpsys = BLI_countlist(&psys->keyed_ob->particlesystem);
			}
			else
				uiBlockSetCol(block, TH_REDALERT);

			uiBlockBeginAlign(block);
			uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_PARTTARGET, "OB:",	butx,(buty-=buth),butw*2/3,buth, &psys->keyed_ob, "The object that has the target particle system"); 
			uiDefButS(block, NUM, B_PARTTARGET, "Psys:",		butx+butw*2/3,buty,butw/3,buth, &psys->keyed_psys, 1.0, totkpsys, 0, 0, "The target particle system number in the object");
			uiBlockEndAlign(block);
			
			uiBlockSetCol(block, TH_AUTO);

			butx=160;

			if(psys->flag & PSYS_FIRST_KEYED)
				uiDefButBitI(block, TOG, PSYS_KEYED_TIME, B_PART_RECALC, "Timed",	 butx,buty,butw,buth, &psys->flag, 0, 0, 0, 0, "Use intermediate key times");
			else
				uiDefButF(block, NUMSLI, B_PART_RECALC, "Time:",		butx,buty,butw,buth, &part->keyed_time, 0.0, 1.0, 1, 3, "Keyed key time relative to remaining particle life");
		}
	}
}

static void object_panel_particle_system(Object *ob)
{
	uiBlock *block;
	uiBut *but;
	ParticleSystem *psys=NULL;
	ParticleSettings *part;
	ID *id, *idfrom;
	ModifierData *md;
	short butx=0, buty=160, butw=150, buth=20;
	char str[30], *lockmessage= NULL;
	static short partact;
	short totpart, lock= 0;

	block= uiNewBlock(&curarea->uiblocks, "object_panel_particle_system", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Particle System", "Particle", 0, 0, 318, 204)==0) return;
	
	if(ob->id.lib) uiSetButLock(1, "Can't edit library data");

	if(ELEM4(ob->type,OB_MESH,OB_FONT,OB_CURVE,OB_SURF)==0) {
		uiDefBut(block, LABEL, 0, "Only Mesh or Curve Objects can generate particles", 10,180,300,20, NULL, 0.0, 0, 0, 0, "");
		return;
	}
	psys=psys_get_current(ob);

	if(psys)
		id=(ID*)(psys->part);
	else
		id=NULL;
	idfrom=&ob->id;

	if(psys==0 || psys->part->type != PART_FLUID) {
	/* browse buttons */
		uiBlockSetCol(block, TH_BUT_SETTING2);
		butx= std_libbuttons(block, butx, buty, 0, NULL, B_PARTBROWSE, ID_PA, 0, id, idfrom, &(G.buts->menunr), B_PARTALONE, 0, B_PARTDELETE, 0, 0);
	}

	uiBlockSetCol(block, TH_AUTO);
	
	partact=psys_get_current_num(ob)+1;
	totpart=BLI_countlist(&ob->particlesystem);
	sprintf(str, "%d Part", totpart);
	but=uiDefButS(block, NUM, B_PARTACT, str, 230,buty,83,buth, &partact, 1.0, totpart+1, 0, 0, "Shows the number of particle systems in the object and the active particle system");
	uiButSetFunc(but, PE_change_act, ob, &partact);

	if(psys==NULL)
		return;

	part=psys->part;

	if(part==NULL)
		return;

	butx=0;

	if(part->type == PART_FLUID) {
		uiDefBut(block, LABEL, 0, "No settings for fluid particles",					butx,buty,2*butw,buth, NULL, 0.0, 0, 0, 0, "");
		return;
	}

	buty -= (buth+5);

	if(part->type == PART_HAIR){
		if(psys->flag & PSYS_EDITED)
			uiDefBut(block, BUT, B_PART_EDITABLE, "Free Edit",		butx+butw+10,buty,butw,buth, NULL, 0.0, 0.0, 10, 0, "Free editing");
		else
			uiDefBut(block, BUT, B_PART_EDITABLE, "Set Editable",	butx+butw+10,buty,butw,buth, NULL, 0.0, 0.0, 10, 0, "Finalize hair to enable editing in particle mode");

	}

	md= (ModifierData*)psys_get_modifier(ob, psys);
	if(md) {
		uiBlockBeginAlign(block);
		uiDefIconButBitI(block, TOG, eModifierMode_Render, B_PART_RECALC, ICON_SCENE, butx+butw-40, buty, 20, 20,&md->mode, 0, 0, 1, 0, "Enable particle system during rendering");
		but= uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_PART_RECALC, VICON_VIEW3D, butx+butw-20, buty, 20, 20,&md->mode, 0, 0, 1, 0, "Enable particle system during interactive display");
		uiBlockEndAlign(block);
	}

	if(psys->flag & PSYS_EDITED) {
		lockmessage= "Hair is edited!";
		lock= 1;
	}
	else if(psys->pointcache->flag & PTCACHE_BAKED) {
		lockmessage= "Simulation frames are baked!";
		lock= 1;
	}

	if(lock)
		uiSetButLock(1, lockmessage);

	uiDefButS(block, MENU, B_PARTTYPE, "Type%t|Hair%x2|Reactor%x1|Emitter%x0", butx,buty,butw-45,buth, &part->type, 14.0, 0.0, 0, 0, "Type of particle system");

	buty-=5;
	uiDefBut(block, LABEL, 0, "Basic:",					butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");
	uiBlockBeginAlign(block);

	if(part->distr==PART_DISTR_GRID)
		uiDefButI(block, NUM, B_PART_ALLOC, "Resol:",		butx,(buty-=buth),butw,buth, &part->grid_res, 1.0, 100.0, 0, 0, "The resolution of the particle grid");
	else
		uiDefButI(block, NUM, B_PART_ALLOC, "Amount:",		butx,(buty-=buth),butw,buth, &part->totpart, 0.0, 100000.0, 0, 0, "The total number of particles");
	if(part->type==PART_REACTOR) {
		uiDefButBitI(block, TOG, PART_REACT_STA_END, B_PART_INIT, "Sta/End",	 butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Give birth to unreacted particles eventually");
		uiDefButS(block, MENU, B_PART_RECALC, "React on %t|Near %x2|Collision %x1|Death %x0", butx+butw/2,buty,butw/2,buth, &part->reactevent, 14.0, 0.0, 0, 0, "The event of target particles to react");
		if(part->flag&PART_REACT_STA_END) {
			uiDefButF(block, NUM, B_PART_INIT, "Sta:",		butx,(buty-=buth),butw,buth, &part->sta, 1.0, part->end, 100, 1, "Frame # to start emitting particles");
			uiDefButF(block, NUM, B_PART_INIT, "End:",		butx,(buty-=buth),butw,buth, &part->end, part->sta, MAXFRAMEF, 100, 1, "Frame # to stop emitting particles");
		}
		if(part->from!=PART_FROM_PARTICLE) {
			uiDefButBitI(block, TOG, PART_REACT_MULTIPLE, B_PART_RECALC, "Multi React",	 butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "React multiple times");
			uiDefButF(block, NUM, B_PART_RECALC, "Shape:",		butx,(buty-=buth),butw,buth, &part->reactshape, 0.0, 10.0, 100, 1, "Power of reaction strength dependence on distance to target");
		}
	}
	else if(part->type==PART_HAIR) {
		uiDefButS(block, NUM, B_PART_RECALC, "Segments:",	butx,(buty-=buth),butw,buth, &part->hair_step, 2.0, 50.0, 0, 0, "Amount of hair segments");
	}
	else {
		uiDefButF(block, NUM, B_PART_INIT, "Sta:",		butx,(buty-=buth),butw,buth, &part->sta, -MAXFRAMEF, part->end, 100, 1, "Frame # to start emitting particles");
		uiDefButF(block, NUM, B_PART_INIT, "End:",		butx,(buty-=buth),butw,buth, &part->end, part->sta, MAXFRAMEF, 100, 1, "Frame # to stop emitting particles");
	}

	if(part->type!=PART_HAIR) {
		uiDefButF(block, NUM, B_PART_INIT, "Life:",	butx,(buty-=buth),butw,buth, &part->lifetime, 1.0, MAXFRAMEF, 100, 1, "Specify the life span of the particles");
		uiDefButF(block, NUM, B_PART_INIT, "Rand:",	butx,(buty-=buth),butw,buth, &part->randlife, 0.0, 2.0, 10, 1, "Give the particle life a random variation");
	}

	uiBlockEndAlign(block);

	butx=160;
	buty=120;

	buty-=10;

	uiDefBut(block, LABEL, 0, "Emit From:",							butx,buty,butw,buth, NULL, 0.0, 0, 0, 0, "");
	uiBlockBeginAlign(block);

	if(lock) uiClearButLock();
	uiDefButBitI(block, TOG, PART_TRAND, B_PART_DISTR, "Random",	butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Emit in random order of elements");
	if(lock) uiSetButLock(1, lockmessage);

	if(part->type==PART_REACTOR)
		uiDefButS(block, MENU, B_PART_DISTR, "Particle %x3|Volume %x2|Faces %x1|Verts %x0", butx+butw/2,buty,butw/2,buth, &part->from, 14.0, 0.0, 0, 0, "Where to emit particles from");
	else
		uiDefButS(block, MENU, B_PART_DISTR, "Volume %x2|Faces %x1|Verts%x0", butx+butw/2,buty,butw/2,buth, &part->from, 14.0, 0.0, 0, 0, "Where to emit particles from");

	if(ELEM(part->from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		if(lock) uiClearButLock();
		uiDefButBitI(block, TOG, PART_EDISTR, B_PART_DISTR, "Even",butx,(buty-=buth),butw/2,buth, &part->flag, 0, 0, 0, 0, "Use even distribution from faces based on face areas or edge lengths");
		if(lock) uiSetButLock(1, lockmessage);
		uiDefButS(block, MENU, B_PART_DISTR, "Distribution %t|Grid%x2|Random%x1|Jittered%x0", butx+butw/2,buty,butw/2,buth, &part->distr, 14.0, 0.0, 0, 0, "How to distribute particles on selected element");
		if(part->distr==PART_DISTR_JIT) {
			uiDefButF(block, NUM, B_PART_DISTR, "Amount:",		butx,(buty-=buth),butw,buth, &part->jitfac, 0, 2.0, 1, 1, "Amount of jitter applied to the sampling");
			uiDefButI(block, NUM, B_PART_DISTR, "P/F:",		butx,(buty-=buth),butw,buth, &part->userjit, 0, 1000.0, 1, 1, "Emission locations / face (0 = automatic)");
		}
		if(part->distr==PART_DISTR_GRID){
			uiDefButBitI(block, TOG, PART_GRID_INVERT, B_PART_DISTR, "Invert",butx,(buty-=buth),butw,buth, &part->flag, 0, 0, 0, 0, "Invert what is considered object and what is not.");				
		}
	}
	uiBlockEndAlign(block);
	
	buty=30;

	if(part->type==PART_REACTOR) {
		ParticleSystem *tpsys=0;
		Object *tob=0;
		int tottpsys;

		uiDefBut(block, LABEL, 0, "Target:", butx,(buty-=buth),butw,buth, NULL, 0.0, 0, 0, 0, "");

		if(psys->target_ob)
			tob=psys->target_ob;
		else
			tob=ob;

		tottpsys=BLI_countlist(&tob->particlesystem);

		uiBlockBeginAlign(block);
		
		if(tob->particlesystem.first==0)
			uiBlockSetCol(block, TH_REDALERT);
		
		uiDefIDPoinBut(block, test_obpoin_but, ID_OB, B_PARTTARGET, "OB:",	butx,(buty-=buth),butw*2/3,buth, &psys->target_ob, "The object that has the target particle system (empty if same object)"); 

		tpsys=BLI_findlink(&tob->particlesystem,psys->target_psys-1);
		if(tpsys) {
			if(tob==ob && tpsys==psys)
				uiBlockSetCol(block, TH_REDALERT);
		}
		else
			uiBlockSetCol(block, TH_REDALERT);

		uiDefButS(block, NUM, B_PARTTARGET, "Psys:",		butx+butw*2/3,buty,butw/3,buth, &psys->target_psys, 1.0, tottpsys, 0, 0, "The target particle system number in the object");
		uiBlockEndAlign(block);
		
		uiBlockSetCol(block, TH_AUTO);
	}
}

/* NT - Panel for fluidsim settings */
static void object_panel_fluidsim(Object *ob)
{
#ifndef DISABLE_ELBEEM
	uiBlock *block;
	int yline = 160;
	const int lineHeight = 20;
	const int separateHeight = 3;
	const int objHeight = 20;
	char *msg = NULL;
	
	block= uiNewBlock(&curarea->uiblocks, "object_fluidsim", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Fluid", "Physics", 1060, 0, 318, 204)==0) return;

	uiSetButLock(object_is_libdata(ob), ERROR_LIBDATA_MESSAGE);
	
	if(ob->type==OB_MESH) {
		if(((Mesh *)ob->data)->totvert == 0) {
			msg = "Mesh has no vertices.";
			goto errMessage;
		}
		uiDefButBitS(block, TOG, OB_FLUIDSIM_ENABLE, REDRAWBUTSOBJECT, "Enable",	 0,yline, 75,objHeight, 
				&ob->fluidsimFlag, 0, 0, 0, 0, "Sets object to participate in fluid simulation");

		if(ob->fluidsimFlag & OB_FLUIDSIM_ENABLE) {
			FluidsimSettings *fss= ob->fluidsimSettings;
	
			if(fss==NULL) {
				fss = ob->fluidsimSettings = fluidsimSettingsNew(ob);
			}
			
			uiBlockBeginAlign(block);
			uiDefButS(block, ROW, B_FLUIDSIM_CHANGETYPE ,"Domain",	    90, yline, 70,objHeight, &fss->type, 15.0, OB_FLUIDSIM_DOMAIN,  20.0, 1.0, "Bounding box of this object represents the computational domain of the fluid simulation.");
			uiDefButS(block, ROW, B_FLUIDSIM_CHANGETYPE ,"Fluid",	   160, yline, 70,objHeight, &fss->type, 15.0, OB_FLUIDSIM_FLUID,   20.0, 2.0, "Object represents a volume of fluid in the simulation.");
			uiDefButS(block, ROW, B_FLUIDSIM_CHANGETYPE ,"Obstacle",	 230, yline, 70,objHeight, &fss->type, 15.0, OB_FLUIDSIM_OBSTACLE,20.0, 3.0, "Object is a fixed obstacle.");
			yline -= lineHeight;

			uiDefButS(block, ROW, B_FLUIDSIM_CHANGETYPE    ,"Inflow",	    90, yline, 70,objHeight, &fss->type, 15.0, OB_FLUIDSIM_INFLOW,  20.0, 4.0, "Object adds fluid to the simulation.");
			uiDefButS(block, ROW, B_FLUIDSIM_CHANGETYPE    ,"Outflow",   160, yline, 70,objHeight, &fss->type, 15.0, OB_FLUIDSIM_OUTFLOW, 20.0, 5.0, "Object removes fluid from the simulation.");
			uiDefButS(block, ROW, B_FLUIDSIM_MAKEPART ,"Particle",	 230, yline, 70,objHeight, &fss->type, 15.0, OB_FLUIDSIM_PARTICLE,20.0, 3.0, "Object is made a particle system to display particles generated by a fluidsim domain object.");
			uiBlockEndAlign(block);
			yline -= lineHeight;
			yline -= 2*separateHeight;

			/* display specific settings for each type */
			if(fss->type == OB_FLUIDSIM_DOMAIN) {
				const int maxRes = 512;
				char memString[32];

				// use mesh bounding box and object scaling
				// TODO fix redraw issue
				elbeemEstimateMemreq(fss->resolutionxyz, 
						ob->fluidsimSettings->bbSize[0],ob->fluidsimSettings->bbSize[1],ob->fluidsimSettings->bbSize[2], fss->maxRefine, memString);
				
				uiBlockBeginAlign(block);
				uiDefButS(block, ROW, REDRAWBUTSOBJECT, "Std",	 0,yline, 25,objHeight, &fss->show_advancedoptions, 16.0, 0, 20.0, 0, "Show standard domain options.");
				uiDefButS(block, ROW, REDRAWBUTSOBJECT, "Adv",	25,yline, 25,objHeight, &fss->show_advancedoptions, 16.0, 1, 20.0, 1, "Show advanced domain options.");
				uiDefButS(block, ROW, REDRAWBUTSOBJECT, "Bnd",	50,yline, 25,objHeight, &fss->show_advancedoptions, 16.0, 2, 20.0, 2, "Show domain boundary options.");
				uiBlockEndAlign(block);
				
				uiDefBut(block, BUT, B_FLUIDSIM_BAKE, "BAKE",90, yline,210,objHeight, NULL, 0.0, 0.0, 10, 0, "Perform simulation and output and surface&preview meshes for each frame.");
				yline -= lineHeight;
				yline -= 2*separateHeight;

				if(fss->show_advancedoptions == 0) {
					uiDefBut(block, LABEL,   0, "Req. BAKE Memory:",  0,yline,150,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefBut(block, LABEL,   0, memString,  200,yline,100,objHeight, NULL, 0.0, 0, 0, 0, "");
					yline -= lineHeight;

					uiBlockBeginAlign(block);
					uiDefButS(block, NUM, REDRAWBUTSOBJECT, "Resolution:", 0, yline,150,objHeight, &fss->resolutionxyz, 1, maxRes, 10, 0, "Domain resolution in X, Y and Z direction");
					uiDefButS(block, NUM, B_DIFF,           "Preview-Res.:", 150, yline,150,objHeight, &fss->previewresxyz, 1, 100, 10, 0, "Resolution of the preview meshes to generate, also in X, Y and Z direction");
					uiBlockEndAlign(block);
					yline -= lineHeight;
					yline -= 1*separateHeight;

					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, B_DIFF, "Start time:",   0, yline,150,objHeight, &fss->animStart, 0.0, 100.0, 10, 0, "Simulation time of the first blender frame.");
					uiDefButF(block, NUM, B_DIFF, "End time:",   150, yline,150,objHeight, &fss->animEnd  , 0.0, 100.0, 10, 0, "Simulation time of the last blender frame.");
					uiBlockEndAlign(block);
					yline -= lineHeight;
					yline -= 2*separateHeight;

					if((fss->guiDisplayMode<1) || (fss->guiDisplayMode>3)){ fss->guiDisplayMode=2; } // can be changed by particle setting
					uiDefBut(block, LABEL,   0, "Disp.-Qual.:",		 0,yline, 90,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiBlockBeginAlign(block);
					uiDefButS(block, MENU, B_FLUIDSIM_FORCEREDRAW, "GuiDisplayMode%t|Geometry %x1|Preview %x2|Final %x3",	
							 90,yline,105,objHeight, &fss->guiDisplayMode, 0, 0, 0, 0, "How to display the fluid mesh in the Blender GUI.");
					uiDefButS(block, MENU, B_DIFF, "RenderDisplayMode%t|Geometry %x1|Preview %x2|Final %x3",	
							195,yline,105,objHeight, &fss->renderDisplayMode, 0, 0, 0, 0, "How to display the fluid mesh for rendering.");
					uiBlockEndAlign(block);
					yline -= lineHeight;
					yline -= 1*separateHeight;

					uiBlockBeginAlign(block);
					uiDefIconBut(block, BUT, B_FLUIDSIM_SELDIR, ICON_FILESEL,  0, yline,  20, objHeight,                   0, 0, 0, 0, 0,  "Select Directory (and/or filename prefix) to store baked fluid simulation files in");
					uiDefBut(block, TEX,     B_FLUIDSIM_FORCEREDRAW,"",	      20, yline, 280, objHeight, fss->surfdataPath, 0.0,79.0, 0, 0,  "Enter Directory (and/or filename prefix) to store baked fluid simulation files in");
					uiBlockEndAlign(block);
					// FIXME what is the 79.0 above?
				} else if(fss->show_advancedoptions == 1) {
					// advanced options
					uiDefBut(block, LABEL, 0, "Gravity:",		0, yline,  90,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiBlockBeginAlign(block);
					uiDefButF(block, NUM, B_DIFF, "X:",    90, yline,  70,objHeight, &fss->gravx, -1000.1, 1000.1, 10, 0, "Gravity in X direction");
					uiDefButF(block, NUM, B_DIFF, "Y:",   160, yline,  70,objHeight, &fss->gravy, -1000.1, 1000.1, 10, 0, "Gravity in Y direction");
					uiDefButF(block, NUM, B_DIFF, "Z:",   230, yline,  70,objHeight, &fss->gravz, -1000.1, 1000.1, 10, 0, "Gravity in Z direction");
					uiBlockEndAlign(block);
					yline -= lineHeight;
					yline -= 1*separateHeight;

					/* viscosity */
					if (fss->viscosityMode==1) /*manual*/
						uiBlockBeginAlign(block);
					uiDefButS(block, MENU, REDRAWVIEW3D, "Viscosity%t|Manual %x1|Water %x2|Oil %x3|Honey %x4",	
							0,yline, 90,objHeight, &fss->viscosityMode, 0, 0, 0, 0, "Set viscosity of the fluid to a preset value, or use manual input.");
					if(fss->viscosityMode==1) {
						uiDefButF(block, NUM, B_DIFF, "Value:",     90, yline, 105,objHeight, &fss->viscosityValue,       0.0, 10.0, 10, 0, "Viscosity setting: value that is multiplied by 10 to the power of (exponent*-1).");
						uiDefButS(block, NUM, B_DIFF, "Neg-Exp.:", 195, yline, 105,objHeight, &fss->viscosityExponent, 0,   10,  10, 0, "Negative exponent for the viscosity value (to simplify entering small values e.g. 5*10^-6.");
						uiBlockEndAlign(block);
					} else {
						// display preset values
						uiDefBut(block, LABEL,   0, fluidsimViscosityPresetString[fss->viscosityMode],  90,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
					}
					yline -= lineHeight;
					yline -= 1*separateHeight;

					uiDefBut(block, LABEL, 0, "Realworld-size:",		0,yline,150,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButF(block, NUM, B_DIFF, "", 150, yline,150,objHeight, &fss->realsize, 0.001, 10.0, 10, 0, "Size of the simulation domain in meters.");
					yline -= lineHeight;
					yline -= 2*separateHeight;

					uiDefBut(block, LABEL, 0, "Gridlevels:",		0,yline,150,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButI(block, NUM, B_DIFF, "", 150, yline,150,objHeight, &fss->maxRefine, -1, 4, 10, 0, "Number of coarsened Grids to use (set to -1 for automatic selection).");
					yline -= lineHeight;

					uiDefBut(block, LABEL, 0, "Compressibility:",		0,yline,150,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButF(block, NUM, B_DIFF, "", 150, yline,150,objHeight, &fss->gstar, 0.001, 0.10, 10,0, "Allowed compressibility due to gravitational force for standing fluid (directly affects simulation step size).");
					yline -= lineHeight;

				} else if(fss->show_advancedoptions == 2) {
					// copied from obstacle...
					//yline -= lineHeight + 5;
					//uiDefBut(block, LABEL, 0, "Domain boundary type settings:",		0,yline,300,objHeight, NULL, 0.0, 0, 0, 0, "");
					//yline -= lineHeight;

					uiBlockBeginAlign(block); // domain
					uiDefButS(block, ROW, REDRAWBUTSOBJECT ,"Noslip",    0, yline,100,objHeight, &fss->typeFlags, 15.0, OB_FSBND_NOSLIP,   20.0, 1.0, "Obstacle causes zero normal and tangential velocity (=sticky). Default for all. Only option for moving objects.");
					uiDefButS(block, ROW, REDRAWBUTSOBJECT ,"Part",	   100, yline,100,objHeight, &fss->typeFlags, 15.0, OB_FSBND_PARTSLIP, 20.0, 2.0, "Mix between no-slip and free-slip. Non moving objects only!");
					uiDefButS(block, ROW, REDRAWBUTSOBJECT ,"Free",  	 200, yline,100,objHeight, &fss->typeFlags, 15.0, OB_FSBND_FREESLIP, 20.0, 3.0, "Obstacle only causes zero normal velocity (=not sticky). Non moving objects only!");
					uiBlockEndAlign(block);
					yline -= lineHeight;

					if(fss->typeFlags&OB_FSBND_PARTSLIP) {
						uiDefBut(block, LABEL, 0, "PartSlipValue:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
						uiDefButF(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->partSlipValue, 0.0, 1.0, 10,0, ".");
						yline -= lineHeight;
					} else { 
						//uiDefBut(block, LABEL, 0, "-",	200,yline,100,objHeight, NULL, 0.0, 0, 0, 0, ""); 
					}
					// copied from obstacle...

					uiDefBut(block, LABEL, 0, "Tracer Particles:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButI(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->generateTracers, 0.0, 10000.0, 10,0, "Number of tracer particles to generate.");
					yline -= lineHeight;
					uiDefBut(block, LABEL, 0, "Generate Particles:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButF(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->generateParticles, 0.0, 10.0, 10,0, "Amount of particles to generate (0=off, 1=normal, >1=more).");
					yline -= lineHeight;
					uiDefBut(block, LABEL, 0, "Surface Subdiv:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButI(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->surfaceSubdivs, 0.0, 5.0, 10,0, "Number of isosurface subdivisions. This is necessary for the inclusion of particles into the surface generation. Warning - can lead to longer computation times!");
					yline -= lineHeight;

					uiDefBut(block, LABEL, 0, "Surface Smoothing:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
					uiDefButF(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->surfaceSmoothing, 0.0, 5.0, 10,0, "Amount of surface smoothing (0=off, 1=normal, >1=stronger smoothing).");
					yline -= lineHeight;

					// use new variable...
					uiDefBut(block, LABEL, 0, "Generate&Use SpeedVecs:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
				  uiDefButBitC(block, TOG, 1, REDRAWBUTSOBJECT, "Disable",     200, yline,100,objHeight, &fss->domainNovecgen, 0, 0, 0, 0, "Default is to generate and use fluidsim vertex speed vectors, this option switches calculation off during bake, and disables loading.");
					yline -= lineHeight;
				} // domain 3
			}
			else if(
					(fss->type == OB_FLUIDSIM_FLUID) 
					|| (fss->type == OB_FLUIDSIM_INFLOW) 
					) {
				uiBlockBeginAlign(block); // fluid
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Volume",    0, yline,100,objHeight, &fss->volumeInitType, 15.0, 1, 20.0, 1.0, "Type of volume init: use only inner region of mesh.");
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Shell",   100, yline,100,objHeight, &fss->volumeInitType, 15.0, 2, 20.0, 2.0, "Type of volume init: use only the hollow shell defined by the faces of the mesh.");
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Both",    200, yline,100,objHeight, &fss->volumeInitType, 15.0, 3, 20.0, 3.0, "Type of volume init: use both the inner volume and the outer shell.");
				uiBlockEndAlign(block);
				yline -= lineHeight;

				yline -= lineHeight + 5; // fluid + inflow
				if(fss->type == OB_FLUIDSIM_FLUID)  uiDefBut(block, LABEL, 0, "Initial velocity:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
				if(fss->type == OB_FLUIDSIM_INFLOW) uiDefBut(block, LABEL, 0, "Inflow velocity:",		  0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
				yline -= lineHeight;
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_DIFF, "X:",   0, yline, 100,objHeight, &fss->iniVelx, -1000.1, 1000.1, 10, 0, "Fluid velocity in X direction");
				uiDefButF(block, NUM, B_DIFF, "Y:", 100, yline, 100,objHeight, &fss->iniVely, -1000.1, 1000.1, 10, 0, "Fluid velocity in Y direction");
				uiDefButF(block, NUM, B_DIFF, "Z:", 200, yline, 100,objHeight, &fss->iniVelz, -1000.1, 1000.1, 10, 0, "Fluid velocity in Z direction");
				uiBlockEndAlign(block);
				yline -= lineHeight;

				if(fss->type == OB_FLUIDSIM_INFLOW) { // inflow
					uiDefBut(block, LABEL, 0, "Local Inflow Coords",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
				  uiDefButBitS(block, TOG, OB_FSINFLOW_LOCALCOORD, REDRAWBUTSOBJECT, "Enable",     200, yline,100,objHeight, &fss->typeFlags, 0, 0, 0, 0, "Use local coordinates for inflow (e.g. for rotating objects).");
				  yline -= lineHeight;
				} else {
				}

				// domainNovecgen "misused" here
				uiDefBut(block, LABEL, 0, "Animated Mesh:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
			  uiDefButBitC(block, TOG, 1, REDRAWBUTSOBJECT, "Export",     200, yline,100,objHeight, &fss->domainNovecgen, 0, 0, 0, 0, "Export this mesh as an animated one. Slower, only use if really necessary (e.g. armatures or parented objects), animated pos/rot/scale IPOs do not require it.");
				yline -= lineHeight;

			} // fluid inflow
			else if( (fss->type == OB_FLUIDSIM_OUTFLOW) )	{
				yline -= lineHeight + 5;

				uiBlockBeginAlign(block); // outflow
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Volume",    0, yline,100,objHeight, &fss->volumeInitType, 15.0, 1, 20.0, 1.0, "Type of volume init: use only inner region of mesh.");
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Shell",   100, yline,100,objHeight, &fss->volumeInitType, 15.0, 2, 20.0, 2.0, "Type of volume init: use only the hollow shell defined by the faces of the mesh.");
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Both",    200, yline,100,objHeight, &fss->volumeInitType, 15.0, 3, 20.0, 3.0, "Type of volume init: use both the inner volume and the outer shell.");
				uiBlockEndAlign(block);
				yline -= lineHeight;

				// domainNovecgen "misused" here
				uiDefBut(block, LABEL, 0, "Animated Mesh:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
			  uiDefButBitC(block, TOG, 1, REDRAWBUTSOBJECT, "Export",     200, yline,100,objHeight, &fss->domainNovecgen, 0, 0, 0, 0, "Export this mesh as an animated one. Slower, only use if really necessary (e.g. armatures or parented objects), animated pos/rot/scale IPOs do not require it.");
				yline -= lineHeight;

				//uiDefBut(block, LABEL, 0, "No additional settings as of now...",		0,yline,300,objHeight, NULL, 0.0, 0, 0, 0, "");
			}
			else if( (fss->type == OB_FLUIDSIM_OBSTACLE) )	{
				yline -= lineHeight + 5; // obstacle

				uiBlockBeginAlign(block); // obstacle
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Volume",    0, yline,100,objHeight, &fss->volumeInitType, 15.0, 1, 20.0, 1.0, "Type of volume init: use only inner region of mesh.");
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Shell",   100, yline,100,objHeight, &fss->volumeInitType, 15.0, 2, 20.0, 2.0, "Type of volume init: use only the hollow shell defined by the faces of the mesh.");
				uiDefButC(block, ROW, REDRAWBUTSOBJECT ,"Init Both",    200, yline,100,objHeight, &fss->volumeInitType, 15.0, 3, 20.0, 3.0, "Type of volume init: use both the inner volume and the outer shell.");
				uiBlockEndAlign(block);
				yline -= lineHeight;

				uiBlockBeginAlign(block); // obstacle
				uiDefButS(block, ROW, REDRAWBUTSOBJECT ,"Noslip",    0, yline,100,objHeight, &fss->typeFlags, 15.0, OB_FSBND_NOSLIP,   20.0, 1.0, "Obstacle causes zero normal and tangential velocity (=sticky). Default for all. Only option for moving objects.");
				uiDefButS(block, ROW, REDRAWBUTSOBJECT ,"Part",	   100, yline,100,objHeight, &fss->typeFlags, 15.0, OB_FSBND_PARTSLIP, 20.0, 2.0, "Mix between no-slip and free-slip. Non moving objects only!");
				uiDefButS(block, ROW, REDRAWBUTSOBJECT ,"Free",  	 200, yline,100,objHeight, &fss->typeFlags, 15.0, OB_FSBND_FREESLIP, 20.0, 3.0, "Obstacle only causes zero normal velocity (=not sticky). Non moving objects only!");
				uiBlockEndAlign(block);
				yline -= lineHeight;

				// domainNovecgen "misused" here
				uiDefBut(block, LABEL, 0, "Animated Mesh:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
			  uiDefButBitC(block, TOG, 1, REDRAWBUTSOBJECT, "Export",     200, yline,100,objHeight, &fss->domainNovecgen, 0, 0, 0, 0, "Export this mesh as an animated one. Slower, only use if really necessary (e.g. armatures or parented objects), animated loc/rot/scale IPOs do not require it.");
				yline -= lineHeight;

				uiDefBut(block, LABEL, 0, "PartSlip Amount:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
				if(fss->typeFlags&OB_FSBND_PARTSLIP) {
					uiDefButF(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->partSlipValue, 0.0, 1.0, 10,0, "Amount of mixing between no- and free-slip, 0=stickier, 1=same as free slip.");
				} else { uiDefBut(block, LABEL, 0, "-",	200,yline,100,objHeight, NULL, 0.0, 0, 0, 0, ""); }
				yline -= lineHeight;

				// generateParticles "misused" here
				uiDefBut(block, LABEL, 0, "Impact Factor:",		0,yline,200,objHeight, NULL, 0.0, 0, 0, 0, "");
				uiDefButF(block, NUM, B_DIFF, "", 200, yline,100,objHeight, &fss->surfaceSmoothing, -2.0, 10.0, 10,0, "This is an unphysical value for moving objects - it controls the impact an obstacle has on the fluid, =0 behaves a bit like outflow (deleting fluid), =1 is default, while >1 results in high forces. Can be used to tweak total mass.");
				yline -= lineHeight;

				yline -= lineHeight; // obstacle
			}
			else if(fss->type == OB_FLUIDSIM_PARTICLE) {
				
				//fss->type == 0; // off, broken...
				if(1) {
				// limited selection, old fixed:	fss->typeFlags = (1<<5)|(1<<1); 
#				define PARTBUT_WIDTH (300/3)
				uiDefButBitS(block, TOG, (1<<2) , REDRAWBUTSOBJECT, "Drops",     0*PARTBUT_WIDTH, yline, PARTBUT_WIDTH,objHeight, &fss->typeFlags, 0, 0, 0, 0, "Show drop particles.");
				uiDefButBitS(block, TOG, (1<<4) , REDRAWBUTSOBJECT, "Floats",    1*PARTBUT_WIDTH, yline, PARTBUT_WIDTH,objHeight, &fss->typeFlags, 0, 0, 0, 0, "Show floating foam particles.");
				uiDefButBitS(block, TOG, (1<<5) , REDRAWBUTSOBJECT, "Tracer",    2*PARTBUT_WIDTH, yline, PARTBUT_WIDTH,objHeight, &fss->typeFlags, 0, 0, 0, 0, "Show tracer particles.");
				yline -= lineHeight;
#				undef PARTBUT_WIDTH


				uiDefBut(block, LABEL, 0, "Size Influence:",		0,yline,150,objHeight, NULL, 0.0, 0, 0, 0, "");
				uiDefButF(block, NUM, B_DIFF, "", 150, yline,150,objHeight, &fss->particleInfSize, 0.0, 2.0,   10,0, "Amount of particle size scaling: 0=off (all same size), 1=full (range 0.2-2.0), >1=stronger.");
				yline -= lineHeight;
				uiDefBut(block, LABEL, 0, "Alpha Influence:",		0,yline,150,objHeight, NULL, 0.0, 0, 0, 0, "");
				uiDefButF(block, NUM, B_DIFF, "", 150, yline,150,objHeight, &fss->particleInfAlpha, 0.0, 2.0,   10,0, "Amount of particle alpha change, inverse of size influence: 0=off (all same alpha), 1=full (large particles get lower alphas, smaller ones higher values).");
				yline -= lineHeight;

				yline -= 1*separateHeight;

				// FSPARTICLE also select input files
				uiBlockBeginAlign(block);
				uiDefIconBut(block, BUT, B_FLUIDSIM_SELDIR, ICON_FILESEL,  0, yline,  20, objHeight,                   0, 0, 0, 0, 0,  "Select fluid simulation bake directory/prefix to load particles from, same as for domain object.");
				uiDefBut(block, TEX,     B_FLUIDSIM_FORCEREDRAW,"",	      20, yline, 280, objHeight, fss->surfdataPath, 0.0,79.0, 0, 0,  "Enter fluid simulation bake directory/prefix to load particles from, same as for domain object.");
				uiBlockEndAlign(block);
				yline -= lineHeight;
			} // disabled for now...

			}
			else {
				yline -= lineHeight + 5;
				/* not yet set */
				uiDefBut(block, LABEL, 0, "Select object type for simulation",		0,yline,300,objHeight, NULL, 0.0, 0, 0, 0, "");
				yline -= lineHeight;
			}
			return;

		} else {
			msg = "Object not enabled for fluid simulation.";
		}
	} else {
		msg = "Only mesh objects can do fluid simulation.";
	}
errMessage:
	yline -= lineHeight + 5;
	uiDefBut(block, LABEL, 0, msg, 0,yline,300,objHeight, NULL, 0.0, 0, 0, 0, "");
	yline -= lineHeight;

#endif // DISABLE_ELBEEM
}

/* Panel for cloth */
static void object_cloth__enabletoggle(void *ob_v, void *arg2)
{
	Object *ob = ob_v;
	ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);

	if (!md) {
		md = modifier_new(eModifierType_Cloth);
		BLI_addtail(&ob->modifiers, md);
		
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
	else {
		Object *ob = ob_v;
		ModifierData *md = modifiers_findByType(ob, eModifierType_Cloth);
	
		if (!md)
			return;

		BLI_remlink(&ob->modifiers, md);

		modifier_free(md);

		BIF_undo_push("Del modifier");
		
		//ob->softflag |= OB_SB_RESET;
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWOOPS, 0);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		object_handle_update(ob);
		countall();
	}
}

static void cloth_presets_material(void *ob_v, void *arg2)
{
	Object *ob = ob_v;
	ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
	
	if(!clmd) return;
	if(clmd->sim_parms->presets==0) return;
	
	if(clmd->sim_parms->presets==1) /* SILK */
	{
		clmd->sim_parms->structural = clmd->sim_parms->shear = 5.0;
		clmd->sim_parms->bending = 0.05;
		clmd->sim_parms->Cdis = 0.0;
		clmd->sim_parms->mass = 0.15;
	}
	else if(clmd->sim_parms->presets==2) /* COTTON */
	{
		clmd->sim_parms->structural = clmd->sim_parms->shear = 15.0;
		clmd->sim_parms->bending = 0.5;
		clmd->sim_parms->Cdis = 5.0;
		clmd->sim_parms->mass = 0.3;
	}
	else if(clmd->sim_parms->presets==3) /* RUBBER */
	{
		clmd->sim_parms->structural = clmd->sim_parms->shear = 15.0;
		clmd->sim_parms->bending = 25.0;
		clmd->sim_parms->Cdis = 25.0;
		clmd->sim_parms->stepsPerFrame = MAX2(clmd->sim_parms->stepsPerFrame, 7.0);
		clmd->sim_parms->mass = 3.0;
	}
	else if(clmd->sim_parms->presets==4) /* DENIM */
	{
		clmd->sim_parms->structural = clmd->sim_parms->shear = 40.0;
		clmd->sim_parms->bending = 10.0;
		clmd->sim_parms->Cdis = 25.0;
		clmd->sim_parms->stepsPerFrame = MAX2(clmd->sim_parms->stepsPerFrame, 12.0);
		clmd->sim_parms->mass = 1.0;
	}
	else if(clmd->sim_parms->presets==5) /* LEATHER */
	{
		clmd->sim_parms->structural = clmd->sim_parms->shear = 80.0;
		clmd->sim_parms->bending = 150.0;
		clmd->sim_parms->Cdis = 25.0;
		clmd->sim_parms->stepsPerFrame = MAX2(clmd->sim_parms->stepsPerFrame, 15.0);
		clmd->sim_parms->mass = 0.4;
	}
}

static void cloth_presets_custom_material(void *ob_v, void *arg2)
{
	Object *ob = ob_v;
	ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
	
	if(!clmd) return;
	
	clmd->sim_parms->presets = 0;
}

static int _can_cloth_at_all(Object *ob)
{
	// list of Yes
	if ((ob->type==OB_MESH)) return 1;
	// else deny
	return 0;
}

static void object_panel_cloth(Object *ob)
{
	uiBlock *block=NULL;
	uiBut *but=NULL;
	static int val, val2;
	ClothModifierData *clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
	PointCache *cache;
	ModifierData *md;
	int libdata = 0;
	
	block= uiNewBlock(&curarea->uiblocks, "object_cloth", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Cloth ", "Physics", 640, 0, 318, 204)==0) return;

	libdata= object_is_libdata(ob);
	uiSetButLock(libdata, ERROR_LIBDATA_MESSAGE);
	
	val = (clmd ? 1:0);
	
	if(!_can_cloth_at_all(ob))
	{
		uiDefBut(block, LABEL, 0, "Cloth can be activated on mesh only.",  10,200,300,20, NULL, 0.0, 0, 0, 0, "");
	}
	else	
	{
		but = uiDefButI(block, TOG, REDRAWBUTSOBJECT, "Cloth",	10,200,130,20, &val, 0, 0, 0, 0, "Sets object to become cloth");
		uiButSetFunc(but, object_cloth__enabletoggle, ob, NULL);

		md = (ModifierData*)clmd;
		if(md) {
			uiBlockBeginAlign(block);
			uiDefIconButBitI(block, TOG, eModifierMode_Render, B_BAKE_CACHE_CHANGE, ICON_SCENE, 145, 200, 20, 20,&md->mode, 0, 0, 1, 0, "Enable cloth during rendering");
			but= uiDefIconButBitI(block, TOG, eModifierMode_Realtime, B_BAKE_CACHE_CHANGE, VICON_VIEW3D, 165, 200, 20, 20,&md->mode, 0, 0, 1, 0, "Enable cloth during interactive display");
			uiBlockEndAlign(block);
		}
	}

	uiDefBut(block, LABEL, 0, "",10,10,300,0, NULL, 0.0, 0, 0, 0, ""); /* tell UI we go to 10,10*/
	
	if(clmd)
	{
		int defCount;
		char *clvg1, *clvg2;
		char clmvg [] = "Vertex Groups%t|";

		val2=0;
		cache= clmd->point_cache;
		
		/* GENERAL STUFF */
		if(!libdata) {
			uiClearButLock();
			if(cache->flag & PTCACHE_BAKE_EDIT_ACTIVE)
				uiSetButLock(1, "Please leave editmode.");
			else if(cache->flag & PTCACHE_BAKED)
				uiSetButLock(1, "Simulation frames are baked");
		}
		
		uiDefBut(block, LABEL, 0, "Material Preset:",  10,170,150,20, NULL, 0.0, 0, 0, 0, "");
		but=uiDefButS(block, MENU, B_BAKE_CACHE_CHANGE, "Silk %x1|Cotton %x2|Rubber %x3|Denim %x4|Leather %x5|Custom %x0",
			     160,170,150,20, &clmd->sim_parms->presets, 0, 0, 0, 0, "");
		uiButSetFunc(but, cloth_presets_material, ob, NULL);
		
		uiBlockBeginAlign(block);
		but = uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "StructStiff:", 10,150,150,20, &clmd->sim_parms->structural, 1.0, 10000.0, 100, 0, "Overall stiffness of structure");
		uiButSetFunc(but, cloth_presets_custom_material, ob, NULL);
		
		but = uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "BendStiff:", 160,150,150,20, &clmd->sim_parms->bending, 0.0, 10000.0, 1000, 0, "Wrinkle coefficient (higher = less smaller but more big wrinkles)");
		uiButSetFunc(but, cloth_presets_custom_material, ob, NULL);
		
		but = uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Spring Damp:", 10,130,150,20, &clmd->sim_parms->Cdis, 0.0, 50.0, 100, 0, "Damping of cloth velocity (higher = more smooth, less jiggling)");
		uiButSetFunc(but, cloth_presets_custom_material, ob, NULL);

		uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Air Damp:", 160,130,150,20, &clmd->sim_parms->Cvi, 0.0, 10.0, 10, 0, "Air has normaly some thickness which slows falling things down");
		
		uiDefButI(block, NUM, B_BAKE_CACHE_CHANGE, "Quality:", 10,110,150,20, &clmd->sim_parms->stepsPerFrame, 4.0, 80.0, 5, 0, "Quality of the simulation (higher=better=slower)");
		
		uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Mass:", 160,110,150,20, &clmd->sim_parms->mass, 0.0, 10.0, 1000, 0, "Mass of cloth material.");
		
		uiDefBut(block, LABEL, 0, "Gravity:",  10,90,60,20, NULL, 0.0, 0, 0, 0, "");
		
		uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "X:", 70,90,80,20, &clmd->sim_parms->gravity[0], -100.0, 100.0, 10, 0, "Apply gravitation to point movement");
		uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Y:", 150,90,80,20, &clmd->sim_parms->gravity[1], -100.0, 100.0, 10, 0, "Apply gravitation to point movement");
		uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Z:", 230,90,80,20, &clmd->sim_parms->gravity[2], -100.0, 100.0, 10, 0, "Apply gravitation to point movement");
		uiBlockEndAlign(block);
		
		/* GOAL STUFF */
		uiBlockBeginAlign(block);
		
		
		uiDefButBitI(block, TOG, CLOTH_SIMSETTINGS_FLAG_GOAL, B_BAKE_CACHE_CHANGE, "Pinning of cloth",	10,60,150,20, &clmd->sim_parms->flags, 0, 0, 0, 0, "Define forces for vertices to stick to animated position");
		
		if ((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL) && (BLI_countlist (&ob->defbase) > 0))
		{
			if(ob->type==OB_MESH) 
			{
				
				defCount = sizeof (clmvg);
				clvg1 = get_vertexgroup_menustr (ob);
				clvg2 = MEM_callocN (strlen (clvg1) + 1 + defCount, "clothVgMS");
				if (! clvg2) {
					printf ("draw_modifier: error allocating memory for cloth vertex group menu string.\n");
					return;
				}
				defCount = BLI_countlist (&ob->defbase);
				if (defCount == 0) 
				{
					clmd->sim_parms->vgroup_mass = 0;
				}
				else
				{
					if(!clmd->sim_parms->vgroup_mass)
						clmd->sim_parms->vgroup_mass = 1;
					else if(clmd->sim_parms->vgroup_mass > defCount)
						clmd->sim_parms->vgroup_mass = defCount;
				}
							
				sprintf (clvg2, "%s%s", clmvg, clvg1);
				
				uiDefButS(block, MENU, B_BAKE_CACHE_CHANGE, clvg2, 160,60,150,20, &clmd->sim_parms->vgroup_mass, 0, defCount, 0, 0, "Browses available vertex groups");
				MEM_freeN (clvg1);
				MEM_freeN (clvg2);
			}
			
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Pin Stiff:", 10,40,150,20, &clmd->sim_parms->goalspring, 0.0, 50.0, 50, 0, "Pin (vertex target position) spring stiffness");
			uiDefBut(block, LABEL, 0, "",160,40,150,20, NULL, 0.0, 0, 0, 0, "");	
			// uiDefButI(block, NUM, B_BAKE_CACHE_CHANGE, "Pin Damp:", 160,50,150,20, &clmd->sim_parms->goalfrict, 1.0, 100.0, 10, 0, "Pined damping (higher = doesn't oszilate so much)");
			/*
			// nobody is changing these ones anyway
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "G Min:",		10,30,150,20, &clmd->sim_parms->mingoal, 0.0, 1.0, 10, 0, "Goal minimum, vertex group weights are scaled to match this range");
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "G Max:",		160,30,150,20, &clmd->sim_parms->maxgoal, 0.0, 1.0, 10, 0, "Goal maximum, vertex group weights are scaled to match this range");
			*/
		}
		else if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL)
		{
			uiDefBut(block, LABEL, 0, " ",  160,60,150,20, NULL, 0.0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "No vertex group for pinning available.",  10,30,300,20, NULL, 0.0, 0, 0, 0, "");
		}
		
		uiBlockEndAlign(block);	
		
		/*
		// no tearing supported anymore since modifier stack restrictions 
		uiBlockBeginAlign(block);
		uiDefButBitI(block, TOG, CSIMSETT_FLAG_TEARING_ENABLED, B_EFFECT_DEP, "Tearing",	10,0,150,20, &clmd->sim_parms->flags, 0, 0, 0, 0, "Sets object to become a cloth collision object");
		
		if (clmd->sim_parms->flags & CSIMSETT_FLAG_TEARING_ENABLED)
		{
		uiDefButI(block, NUM, B_DIFF, "Max extent:",	   160,0,150,20, &clmd->sim_parms->maxspringlen, 1.0, 1000.0, 10, 0, "Maximum extension before spring gets cut");
	}
		
		uiBlockEndAlign(block);	
		*/
	}
	
	uiBlockEndAlign(block);
	
	uiBlockEndAlign(block);
}

static void object_panel_cloth_II(Object *ob)
{
	uiBlock *block;
	ClothModifierData *clmd = NULL;
	PointCache *cache;
	static PTCacheID staticpid;
	int libdata;

	block= uiNewBlock(&curarea->uiblocks, "object_cloth_II", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Cloth ", "Physics");
	if(uiNewPanel(curarea, block, "Cloth Collision", "Physics", 651, 0, 318, 204)==0) return;

	libdata= object_is_libdata(ob);
	uiSetButLock(libdata, ERROR_LIBDATA_MESSAGE);
	
	clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
	
	if(clmd)
	{
		BKE_ptcache_id_from_cloth(&staticpid, ob, clmd);
		cache= staticpid.cache;

		if(!libdata) {
			uiClearButLock();
			if(cache->flag & PTCACHE_BAKE_EDIT_ACTIVE)
				uiSetButLock(1, "Please leave editmode.");
		}

		object_physics_bake_buttons(block, &staticpid, 135, libdata);

		uiDefBut(block, LABEL, 0, "",10,140,300,20, NULL, 0.0, 0, 0, 0, "");

		if(!libdata) {
			if(!(cache->flag & PTCACHE_BAKE_EDIT_ACTIVE))
				if(cache->flag & PTCACHE_BAKED)
					uiSetButLock(1, "Simulation frames are baked");
		}
		
		/*
		TODO: implement this again in cloth!
		if(length>1) // B_CLOTH_CHANGEPREROLL
		uiDefButI(block, NUM, B_CLOTH_CHANGEPREROLL, "Preroll:", 10,80,145,20, &clmd->sim_parms->preroll, 0, length-1, 1, 0, "Simulation starts on this frame");	
		else
		uiDefBut(block, LABEL, 0, " ",  10,80,145,20, NULL, 0.0, 0, 0, 0, "");
		*/
#ifdef WITH_BULLET
		uiDefButBitI(block, TOG, CLOTH_COLLSETTINGS_FLAG_ENABLED, B_BAKE_CACHE_CHANGE, "Enable collisions",	10,60,150,20, &clmd->coll_parms->flags, 0, 0, 0, 0, "Enable collisions with this object");
		if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_ENABLED)
		{
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Min Distance:",	   160,60,150,20, &clmd->coll_parms->epsilon, 0.001f, 1.0, 0.01f, 0, "Minimum distance between collision objects before collision response takes in, can be changed for each frame");
			uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Collision Quality:",	   10,40,150,20, &clmd->coll_parms->loop_count, 1.0, 20.0, 1.0, 0, "How many collision iterations should be done. (higher = better = slower)");
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Friction:",	   160,40,150,20, &clmd->coll_parms->friction, 0.0, 80.0, 1.0, 0, "Friction force if a collision happened (0=movement not changed, 100=no movement left)");
			
			uiDefButBitI(block, TOG, CLOTH_COLLSETTINGS_FLAG_SELF, B_BAKE_CACHE_CHANGE, "Enable selfcollisions",	10,20,150,20, &clmd->coll_parms->flags, 0, 0, 0, 0, "Enable selfcollisions with this object");
			if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF)	
			{
				uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "Min Distance:",	   160,20,150,20, &clmd->coll_parms->selfepsilon, 0.5f, 1.0, 0.01f, 0, "0.5 means no distance at all, 1.0 is maximum distance");
				// self_loop_count
				uiDefButS(block, NUM, B_BAKE_CACHE_CHANGE, "Selfcoll Quality:",	   10,0,150,20, &clmd->coll_parms->self_loop_count, 1.0, 10.0, 1.0, 0, "How many selfcollision iterations should be done. (higher = better = slower), can be changed for each frame");
			}
			else
				uiDefBut(block, LABEL, 0, "",160,20,150,20, NULL, 0.0, 0, 0, 0, "");	
		}
		else
			uiDefBut(block, LABEL, 0, "",160,60,150,20, NULL, 0.0, 0, 0, 0, "");	
#else
		uiDefBut(block, LABEL, 0, "No collisions available (compile with bullet).",10,60,300,20, NULL, 0.0, 0, 0, 0, "");
#endif
	}
	
	uiBlockEndAlign(block);
	
}

static void object_panel_cloth_III(Object *ob)
{
	uiBlock *block;
	ClothModifierData *clmd = NULL;
	PointCache *cache;
	int libdata;

	block= uiNewBlock(&curarea->uiblocks, "object_cloth_III", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Cloth ", "Physics");
	if(uiNewPanel(curarea, block, "Cloth Advanced", "Physics", 651, 0, 318, 204)==0) return;

	libdata= object_is_libdata(ob);
	uiSetButLock(libdata, ERROR_LIBDATA_MESSAGE);
	
	clmd = (ClothModifierData *)modifiers_findByType(ob, eModifierType_Cloth);
	
	if(clmd)
	{
		int defCount;
		char *clvg1, *clvg2;
		char clmvg [] = "Vertex Groups%t|None%x0|";
		char clmvg2 [] = "Vertex Groups%t|None%x0|";

		cache= clmd->point_cache;
		
		if(!libdata) {
			uiClearButLock();
			if(cache->flag & PTCACHE_BAKE_EDIT_ACTIVE)
				uiSetButLock(1, "Please leave editmode.");
			else if(cache->flag & PTCACHE_BAKED)
				uiSetButLock(1, "Simulation frames are baked");
		}
		
		uiDefButBitI(block, TOG, CLOTH_SIMSETTINGS_FLAG_SCALING, B_BAKE_CACHE_CHANGE, "Enable stiffness scaling",10,130,300,20, &clmd->sim_parms->flags, 0, 0, 0, 0, "If enabled, stiffness can be scaled along a weight painted vertex group.");
		
		if ((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SCALING)&& (BLI_countlist (&ob->defbase) > 0))
		{	
			uiDefBut(block, LABEL, 0, "StructStiff VGroup:",10,110,150,20, NULL, 0.0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "BendStiff VGroup:",160,110,150,20, NULL, 0.0, 0, 0, 0, "");
			
			defCount = sizeof (clmvg);
			clvg1 = get_vertexgroup_menustr (ob);
			clvg2 = MEM_callocN (strlen (clvg1) + 1 + defCount, "clothVgST");
			if (! clvg2) {
				printf ("draw_modifier: error allocating memory for cloth vertex group menu string.\n");
				return;
			}
			defCount = BLI_countlist (&ob->defbase);
			if (defCount == 0) 
			{
				clmd->sim_parms->vgroup_struct = 0;
			}
			else
			{
				if(clmd->sim_parms->vgroup_struct > defCount)
					clmd->sim_parms->vgroup_struct = 0;
			}
						
			sprintf (clvg2, "%s%s", clmvg, clvg1);
			
			uiDefButS(block, MENU, B_BAKE_CACHE_CHANGE, clvg2,	10,90,150,20, &clmd->sim_parms->vgroup_struct, 0, defCount, 0, 0, "Browses available vertex groups");
			MEM_freeN (clvg1);
			MEM_freeN (clvg2);
			
			defCount = sizeof (clmvg);
			clvg1 = get_vertexgroup_menustr (ob);
			clvg2 = MEM_callocN (strlen (clvg1) + 1 + defCount, "clothVgBD");
			if (! clvg2) {
				printf ("draw_modifier: error allocating memory for cloth vertex group menu string.\n");
				return;
			}
			defCount = BLI_countlist (&ob->defbase);
			if (defCount == 0) 
			{
				clmd->sim_parms->vgroup_bend = 0;
			}
			else
			{
				if(clmd->sim_parms->vgroup_bend > defCount)
					clmd->sim_parms->vgroup_bend = 0;
			}
						
			sprintf (clvg2, "%s%s", clmvg2, clvg1);
			
			uiDefButS(block, MENU, B_BAKE_CACHE_CHANGE, clvg2, 160,90,150,20, &clmd->sim_parms->vgroup_bend, 0, defCount, 0, 0, "Browses available vertex groups");
			MEM_freeN (clvg1);
			MEM_freeN (clvg2);
			
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "StructStiff Max:",10,70,150,20, &clmd->sim_parms->max_struct, clmd->sim_parms->structural, 10000.0, 0.01f, 0, "Maximum structural stiffness value");
			
			uiDefButF(block, NUM, B_BAKE_CACHE_CHANGE, "BendStiff Max:",160,70,150,20, &clmd->sim_parms->max_bend, clmd->sim_parms->bending, 10000.0, 0.01f, 0, "Maximum bending stiffness value");

		}
		else if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SCALING)
		{
			uiDefBut(block, LABEL, 0, " ",  10,110,300,20, NULL, 0.0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "No vertex group for stiffness scaling available.",  10,90,300,20, NULL, 0.0, 0, 0, 0, "");
		}
	
		
		
	}
	
	uiBlockEndAlign(block);
	
}

void object_panels()
{
	Object *ob;

	/* check context here */
	ob= OBACT;
	if(ob) {
		object_panel_object(ob);
		object_panel_anim(ob);
		object_panel_draw(ob);
		object_panel_constraint("Object");

		uiClearButLock();
	}
}

void physics_panels()
{
	Object *ob;
	
	/* check context here */
	ob= OBACT;
	if(ob) {
		object_panel_fields(ob);
		if(ob->type==OB_MESH)
			object_panel_collision(ob);
		object_softbodies(ob);
		object_softbodies_collision(ob);
		object_softbodies_solver(ob);
		object_panel_cloth(ob);
		object_panel_cloth_II(ob);
		object_panel_cloth_III(ob);
		object_panel_fluidsim(ob);
	}
}
void particle_panels()
{
	Object *ob;
	ParticleSystem *psys;

	ob=OBACT;

	if(ob && ob->type==OB_MESH) {
		object_panel_particle_system(ob);

		psys=psys_get_current(ob);

		if(psys ){
			object_panel_particle_bake(ob);
			object_panel_particle_physics(ob);
			object_panel_particle_visual(ob);
			object_panel_particle_simplification(ob);
			object_panel_particle_extra(ob);
			object_panel_particle_children(ob);
		}
	}
}
