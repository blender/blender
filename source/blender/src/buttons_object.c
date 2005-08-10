/**
 * $Id: 
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

#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_softbody.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_toolbox.h"
#include "BDR_editcurve.h"
#include "BDR_drawobject.h"

//#include "BIF_editsca.h"

#include "BIF_butspace.h"

#include "mydevice.h"
#include "blendef.h"

/* -----includes for this file specific----- */


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
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"

#include "BIF_editconstraint.h"
#include "BSE_editipo.h"
#include "BDR_editobject.h"

#include "butspace.h" // own module

static float prspeed=0.0;
float prlen=0.0;


/* ********************* CONSTRAINT ***************************** */

#if 0
static void add_influence_key_to_constraint_func (void *arg1v, void *unused)
{
	bConstraint *con = arg1v;
	add_influence_key_to_constraint(con);
}
#endif

static void activate_constraint_ipo_func (void *arg1v, void *unused)
{

	bConstraint *con = arg1v;
	bConstraintChannel *chan;
	ListBase *conbase;

	get_constraint_client(NULL, NULL, NULL);

	conbase = get_constraint_client_channels(1);

	if (!conbase)
		return;

	/* See if this list already has an appropriate channel */
	chan = find_constraint_channel(conbase, con->name);

	if (!chan){
		/* Add a new constraint channel */
		chan = add_new_constraint_channel(con->name);
		BLI_addtail(conbase, chan);
	}

	/* Ensure there is an ipo to display */
	if (!chan->ipo){
		chan->ipo = add_ipo(con->name, IPO_CO);
	}

	/* Make this the active channel */
	OBACT->activecon = chan;

	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWNLA, 0);
}

static void del_constraint_func (void *arg1v, void *arg2v)
{
	bConstraint *con= arg1v;
	Object *ob;

	ListBase *lb= arg2v;
	
	ob=OBACT;
	
	if (ob->activecon && !strcmp(ob->activecon->name, con->name))
		ob->activecon = NULL;

	free_constraint_data (con);

	BLI_freelinkN(lb, con);

	BIF_undo_push("Delete constraint");
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWIPO, 0); 

}

static void verify_constraint_name_func (void *data, void *data2_unused)
{
	ListBase *conlist;
	bConstraint *con;
	char ownerstr[64];
	short type;
	
	con = (bConstraint*) data;
	if (!con)
		return;
	
	conlist = get_constraint_client(ownerstr, &type, NULL);
	unique_constraint_name (con, conlist);
}

static void constraint_changed_func (void *data, void *data2_unused)
{
	bConstraint *con = (bConstraint*) data;

	if (con->type == con->otype)
		return;

	free_constraint_data (con);
	con->data = new_constraint_data(con->type);

}

static void move_constraint_func (void *datav, void *data2_unused)
{
	bConstraint *constraint_to_move= datav;
	int val;
	ListBase *conlist;
	char ownerstr[64];
	short	type;
	bConstraint *curCon, *con, *neighbour;
	
	val= pupmenu("Move up%x1|Move down %x2");
	
	con = constraint_to_move;

	if(val>0) {
		conlist = get_constraint_client(ownerstr, &type, NULL);
		for (curCon = conlist->first; curCon; curCon = curCon->next){
			if (curCon == con){
				/* Move up */
				if (val == 1 && con->prev){
					neighbour = con->prev;
					BLI_remlink(conlist, neighbour);
					BLI_insertlink(conlist, con, neighbour);
				}
				/* Move down */
				else if (val == 2 && con->next){
					neighbour = con->next;
					BLI_remlink (conlist, con);
					BLI_insertlink(conlist, neighbour, con);
				}
				break;
			}
		}
	}
}

static void get_constraint_typestring (char *str, bConstraint *con)
{
	switch (con->type){
	case CONSTRAINT_TYPE_CHILDOF:
		strcpy (str, "Child Of");
		return;
	case CONSTRAINT_TYPE_NULL:
		strcpy (str, "Null");
		return;
	case CONSTRAINT_TYPE_TRACKTO:
		strcpy (str, "Track To");
		return;
	case CONSTRAINT_TYPE_KINEMATIC:
		strcpy (str, "IK Solver");
		return;
	case CONSTRAINT_TYPE_ROTLIKE:
		strcpy (str, "Copy Rotation");
		return;
	case CONSTRAINT_TYPE_LOCLIKE:
		strcpy (str, "Copy Location");
		return;
	case CONSTRAINT_TYPE_ACTION:
		strcpy (str, "Action");
		return;
	case CONSTRAINT_TYPE_LOCKTRACK:
		strcpy (str, "Locked Track");
		return;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		strcpy (str, "Follow Path");
		return;
	case CONSTRAINT_TYPE_STRETCHTO:
		strcpy (str, "Stretch To");
		return;
	default:
		strcpy (str, "Unknown");
		return;
	}
}

static int get_constraint_col(bConstraint *con)
{
	switch (con->type) {
	case CONSTRAINT_TYPE_NULL:
		return TH_BUT_NEUTRAL;
	case CONSTRAINT_TYPE_KINEMATIC:
		return TH_BUT_SETTING2;
	case CONSTRAINT_TYPE_TRACKTO:
		return TH_BUT_SETTING;
	case CONSTRAINT_TYPE_ROTLIKE:
		return TH_BUT_SETTING1;
	case CONSTRAINT_TYPE_LOCLIKE:
		return TH_BUT_POPUP;
	case CONSTRAINT_TYPE_ACTION:
		return TH_BUT_ACTION;
	case CONSTRAINT_TYPE_LOCKTRACK:
		return TH_BUT_SETTING;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		return TH_BUT_SETTING2;
	case CONSTRAINT_TYPE_STRETCHTO:
		return TH_BUT_SETTING;
	default:
		return TH_REDALERT;
	}
}

static void draw_constraint (uiBlock *block, ListBase *list, bConstraint *con, short *xco, short *yco, short type)
{
	uiBut *but;
	char typestr[64];
	short height, width = 265;
	int curCol;

	get_constraint_typestring (typestr, con);

	curCol = get_constraint_col(con);

	/* Draw constraint header */
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	uiDefIconButBitS(block, ICONTOG, CONSTRAINT_EXPAND, B_CONSTRAINT_REDRAW, ICON_DISCLOSURE_TRI_RIGHT, *xco-10, *yco, 20, 20, &con->flag, 0.0, 0.0, 0.0, 0.0, "Collapse/Expand Constraint");

	if (con->flag & CONSTRAINT_EXPAND) {
		
		if (con->flag & CONSTRAINT_DISABLE) {
			BIF_ThemeColor(TH_REDALERT);
			uiBlockSetCol(block, TH_REDALERT);
		}
		else
			BIF_ThemeColor(curCol);

		/*if (type==TARGET_BONE)
			but = uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Bone Constraint%t|Track To%x2|IK Solver%x3|Copy Rotation%x8|Copy Location%x9|Action%x12|Null%x0", *xco+20, *yco, 100, 20, &con->type, 0.0, 0.0, 0.0, 0.0, "Constraint type"); 
		else
			but = uiDefButC(block, MENU, B_CONSTRAINT_TEST, "Object Constraint%t|Track To%x2|Copy Rotation%x8|Copy Location%x9|Null%x0", *xco+20, *yco, 100, 20, &con->type, 0.0, 0.0, 0.0, 0.0, "Constraint type"); 
		*/
		uiBlockSetEmboss(block, UI_EMBOSS);
		
		/* rounded header */
		BIF_ThemeColorShade(curCol, -20);
		uiSetRoundBox(3);
		uiRoundBox((float)*xco+4, (float)*yco-18, (float)*xco+width+30, (float)*yco+6, 5.0);

		but = uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, *xco+10, *yco-1, 100, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

		uiButSetFunc(but, constraint_changed_func, con, NULL);
		con->otype = con->type;
		
		but = uiDefBut(block, TEX, B_CONSTRAINT_REDRAW, "", *xco+120, *yco-1, 135, 19, con->name, 0.0, 29.0, 0.0, 0.0, "Constraint name"); 
		uiButSetFunc(but, verify_constraint_name_func, con, NULL);
	}	
	else{
		uiBlockSetEmboss(block, UI_EMBOSSN);

		if (con->flag & CONSTRAINT_DISABLE) {
			uiBlockSetCol(block, TH_REDALERT);
			BIF_ThemeColor(TH_REDALERT);
		}
		else
			BIF_ThemeColor(curCol);
			
		/* coloured rectangle to hold constraint controls */
		if (con->type!=CONSTRAINT_TYPE_NULL) glRects(*xco+3, *yco-36, *xco+width+30, *yco-15);
	
		/* rounded header */
		BIF_ThemeColorShade(curCol, -20);
		uiSetRoundBox(3);
		uiRoundBox((float)*xco+4, (float)*yco-15, (float)*xco+width+30, (float)*yco+6, 5.0);
		
		but = uiDefBut(block, LABEL, B_CONSTRAINT_TEST, typestr, *xco+10, *yco-1, 100, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		uiButSetFunc(but, move_constraint_func, con, NULL);
		
		but = uiDefBut(block, LABEL, B_CONSTRAINT_TEST, con->name, *xco+120, *yco-1, 135, 19, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
		uiButSetFunc(but, move_constraint_func, con, NULL);
	}

	uiBlockSetCol(block, TH_AUTO);	
	
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	but = uiDefIconBut(block, BUT, B_CONSTRAINT_DEL, ICON_X, *xco+262, *yco, 19, 19, list, 0.0, 0.0, 0.0, 0.0, "Delete constraint");
	uiButSetFunc(but, del_constraint_func, con, list);

	uiBlockSetEmboss(block, UI_EMBOSS);


	/* Draw constraint data*/
	if (!(con->flag & CONSTRAINT_EXPAND)) {
		(*yco)-=21;
	}
	else {
		switch (con->type){
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data = con->data;
				bArmature *arm;

				height = 86;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);

				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm){
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				}
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);

				/* Draw action button */
				uiBlockBeginAlign(block);
				uiDefButS(block, TOG, B_CONSTRAINT_TEST, "Local",						*xco+((width/2)-117), *yco-46, 78, 18, &data->local, 0, 0, 0, 0, "Use true local rotation difference");
				uiDefIDPoinBut(block, test_actionpoin_but, B_CONSTRAINT_TEST, "AC:",	*xco+((width/2)-117), *yco-64, 78, 18, &data->act, "Action containing the keyed motion for this bone"); 
				uiDefButS(block, MENU, B_CONSTRAINT_TEST, "Key on%t|X Rot%x0|Y Rot%x1|Z Rot%x2", *xco+((width/2)-117), *yco-84, 78, 18, &data->type, 0, 24, 0, 0, "Specify which transformation channel from the target is used to key the action");

				uiBlockBeginAlign(block);
				uiDefButS(block, NUM, B_CONSTRAINT_TEST, "Start:", *xco+((width/2)-36), *yco-64, 78, 18, &data->start, 1, MAXFRAME, 0.0, 0.0, "Starting frame of the keyed motion"); 
				uiDefButS(block, NUM, B_CONSTRAINT_TEST, "End:", *xco+((width/2)-36), *yco-84, 78, 18, &data->end, 1, MAXFRAME, 0.0, 0.0, "Ending frame of the keyed motion"); 
				
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Min:", *xco+((width/2)+45), *yco-64, 78, 18, &data->min, -180, 180, 0, 0, "Minimum value for target channel range");
				uiDefButF(block, NUM, B_CONSTRAINT_TEST, "Max:", *xco+((width/2)+45), *yco-84, 78, 18, &data->max, -180, 180, 0, 0, "Maximum value for target channel range");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data = con->data;
				bArmature *arm;
				height = 66;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);

				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm){
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				}
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);
				
				/* Draw XYZ toggles */
				uiBlockBeginAlign(block);
				but=uiDefButBitI(block, TOG, LOCLIKE_X, B_CONSTRAINT_TEST, "X", *xco+((width/2)-48), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy X component");
				but=uiDefButBitI(block, TOG, LOCLIKE_Y, B_CONSTRAINT_TEST, "Y", *xco+((width/2)-16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Y component");
				but=uiDefButBitI(block, TOG, LOCLIKE_Z, B_CONSTRAINT_TEST, "Z", *xco+((width/2)+16), *yco-64, 32, 18, &data->flag, 0, 24, 0, 0, "Copy Z component");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data = con->data;
				bArmature *arm;
				height = 46;

				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm){
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				}
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data = con->data;
				bArmature *arm;
				
				height = 66;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm)
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);
	
				uiDefButBitS(block, TOG, CONSTRAINT_IK_TIP, B_CONSTRAINT_REDRAW, "Use Tip", *xco+((width/2)-117), *yco-42, 80, 18, &data->flag, 0, 0, 0, 0, "Include Bone's tip als last element in Chain");
				
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_CONSTRAINT_REDRAW, "Tolerance:", *xco+((width/2)-117), *yco-64, 120, 18, &data->tolerance, 0.0001f, 1.0, 0.0, 0.0, "Maximum distance to target after solving"); 
				uiDefButS(block, NUM, B_CONSTRAINT_REDRAW, "Iterations:", *xco+((width/2)+3), *yco-64, 120, 18, &data->iterations, 1, 10000, 0.0, 0.0, "Maximum number of solving iterations"); 
				uiBlockEndAlign(block);
				
			}
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data = con->data;
				bArmature *arm;

				height = 66;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm){
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				}
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);

				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "To:", *xco+12, *yco-64, 25, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	*xco+39, *yco-64,17,18, &data->reserved1, 12.0, 0.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Y",	*xco+56, *yco-64,17,18, &data->reserved1, 12.0, 1.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	*xco+73, *yco-64,17,18, &data->reserved1, 12.0, 2.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-X",	*xco+90, *yco-64,24,18, &data->reserved1, 12.0, 3.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-Y",	*xco+114, *yco-64,24,18, &data->reserved1, 12.0, 4.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-Z",	*xco+138, *yco-64,24,18, &data->reserved1, 12.0, 5.0, 0, 0, "The axis that points to the target object");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Up:", *xco+174, *yco-64, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
				
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	*xco+204, *yco-64,17,18, &data->reserved2, 13.0, 0.0, 0, 0, "The axis that points upward");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Y",	*xco+221, *yco-64,17,18, &data->reserved2, 13.0, 1.0, 0, 0, "The axis that points upward");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	*xco+238, *yco-64,17,18, &data->reserved2, 13.0, 2.0, 0, 0, "The axis that points upward");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_LOCKTRACK:
			{
				bLockTrackConstraint *data = con->data;
				bArmature *arm;
				height = 66;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);

				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm){
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				}
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "To:", *xco+12, *yco-64, 25, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	*xco+39, *yco-64,17,18, &data->trackflag, 12.0, 0.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Y",	*xco+56, *yco-64,17,18, &data->trackflag, 12.0, 1.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	*xco+73, *yco-64,17,18, &data->trackflag, 12.0, 2.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-X",	*xco+90, *yco-64,24,18, &data->trackflag, 12.0, 3.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-Y",	*xco+114, *yco-64,24,18, &data->trackflag, 12.0, 4.0, 0, 0, "The axis that points to the target object");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-Z",	*xco+138, *yco-64,24,18, &data->trackflag, 12.0, 5.0, 0, 0, "The axis that points to the target object");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Lock:", *xco+166, *yco-64, 38, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
				
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	*xco+204, *yco-64,17,18, &data->lockflag, 13.0, 0.0, 0, 0, "The axis that is locked");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Y",	*xco+221, *yco-64,17,18, &data->lockflag, 13.0, 1.0, 0, 0, "The axis that is locked");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	*xco+238, *yco-64,17,18, &data->lockflag, 13.0, 2.0, 0, 0, "The axis that is locked");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_FOLLOWPATH:
			{
				bFollowPathConstraint *data = con->data;

				height = 66;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 

				/* Draw target parameters */
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 
				
				/* Draw Curve Follow toggle */
				but=uiDefButBitI(block, TOG, 1, B_CONSTRAINT_TEST, "CurveFollow", *xco+39, *yco-44, 100, 18, &data->followflag, 0, 24, 0, 0, "Object will follow the heading and banking of the curve");

				/* Draw Offset number button */
				uiDefButF(block, NUM, B_CONSTRAINT_REDRAW, "Offset:", *xco+155, *yco-44, 100, 18, &data->offset, -9000, 9000, 100.0, 0.0, "Offset from the position corresponding to the time frame"); 

				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Fw:", *xco+12, *yco-64, 27, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	*xco+39, *yco-64,17,18, &data->trackflag, 12.0, 0.0, 0, 0, "The axis that points forward along the path");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Y",	*xco+56, *yco-64,17,18, &data->trackflag, 12.0, 1.0, 0, 0, "The axis that points forward along the path");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	*xco+73, *yco-64,17,18, &data->trackflag, 12.0, 2.0, 0, 0, "The axis that points forward along the path");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-X",	*xco+90, *yco-64,24,18, &data->trackflag, 12.0, 3.0, 0, 0, "The axis that points forward along the path");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-Y",	*xco+114, *yco-64,24,18, &data->trackflag, 12.0, 4.0, 0, 0, "The axis that points forward along the path");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"-Z",	*xco+138, *yco-64,24,18, &data->trackflag, 12.0, 5.0, 0, 0, "The axis that points forward along the path");
				uiBlockEndAlign(block);
				
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Up:", *xco+174, *yco-64, 30, 18, NULL, 0.0, 0.0, 0.0, 0.0, "");
				
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	*xco+204, *yco-64,17,18, &data->upflag, 13.0, 0.0, 0, 0, "The axis that points upward");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Y",	*xco+221, *yco-64,17,18, &data->upflag, 13.0, 1.0, 0, 0, "The axis that points upward");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	*xco+238, *yco-64,17,18, &data->upflag, 13.0, 2.0, 0, 0, "The axis that points upward");
				uiBlockEndAlign(block);
			}
			break;
		case CONSTRAINT_TYPE_STRETCHTO:
			{
				bStretchToConstraint *data = con->data;
				bArmature *arm;
				height = 105;
				BIF_ThemeColor(curCol);

				glRects(*xco+3, *yco-height-39, *xco+width+30, *yco-18);
				
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Target:", *xco+65, *yco-24, 50, 18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 


				/* Draw target parameters */
				uiBlockBeginAlign(block);
				uiDefIDPoinBut(block, test_obpoin_but, B_CONSTRAINT_CHANGETARGET, "OB:", *xco+120, *yco-24, 135, 18, &data->tar, "Target Object"); 

				arm = get_armature(data->tar);
				if (arm){
					but=uiDefBut(block, TEX, B_CONSTRAINT_CHANGETARGET, "BO:", *xco+120, *yco-42,135,18, &data->subtarget, 0, 24, 0, 0, "Subtarget Bone");
				}
				else
					strcpy (data->subtarget, "");
				uiBlockEndAlign(block);

				
				uiBlockBeginAlign(block);
				uiDefButF(block,BUTM,B_CONSTRAINT_REDRAW,"R",*xco, *yco-60,20,18,&(data->orglength),0.0,0,0,0,"Recalculate RLenght");
				uiDefButF(block,NUM,B_CONSTRAINT_REDRAW,"Rest Length:",*xco+18, *yco-60,237,18,&(data->orglength),0.0,100,0.5,0.5,"Lenght at Rest Position");
				uiBlockEndAlign(block);

				uiDefButF(block,NUM,B_CONSTRAINT_REDRAW,"Volume Variation:",*xco+18, *yco-82,237,18,&(data->bulge),0.0,100,0.5,0.5,"Factor between volume variation and stretching");

				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST, "Vol:",*xco+14, *yco-104,30,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"XZ",	 *xco+44, *yco-104,30,18, &data->volmode, 12.0, 0.0, 0, 0, "Keep Volume: Scaling X & Z");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	 *xco+74, *yco-104,20,18, &data->volmode, 12.0, 1.0, 0, 0, "Keep Volume: Scaling X");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	 *xco+94, *yco-104,20,18, &data->volmode, 12.0, 2.0, 0, 0, "Keep Volume: Scaling Z");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"NONE", *xco+114, *yco-104,50,18, &data->volmode, 12.0, 3.0, 0, 0, "Ignore Volume");
				uiBlockEndAlign(block);

				
				uiBlockBeginAlign(block);
				uiDefBut(block, LABEL, B_CONSTRAINT_TEST,"Plane:",*xco+175, *yco-104,40,18, NULL, 0.0, 0.0, 0.0, 0.0, ""); 
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"X",	  *xco+215, *yco-104,20,18, &data->plane, 12.0, 0.0, 0, 0, "Keep X axis");
				uiDefButI(block, ROW,B_CONSTRAINT_REDRAW,"Z",	  *xco+235, *yco-104,20,18, &data->plane, 12.0, 2.0, 0, 0, "Keep Z axis");
				uiBlockEndAlign(block);
				}
			break;
		case CONSTRAINT_TYPE_NULL:
			{
				height = 17;
				
				BIF_ThemeColor(curCol);
				glRects(*xco+3, *yco-height-17, *xco+width+30, *yco-18);
				
			}
			break;
		default:
			height = 0;
			break;
		}

		(*yco)-=(24+height);
	}

	if (con->type!=CONSTRAINT_TYPE_NULL) {
		uiDefButF(block, NUMSLI, B_CONSTRAINT_REDRAW, "Influence ", *xco+17, *yco, 197, 19, &(con->enforce), 0.0, 1.0, 0.0, 0.0, "Amount of influence this constraint will have on the final solution");
		but = uiDefBut(block, BUT, B_CONSTRAINT_REDRAW, "Edit", *xco+215, *yco, 41, 19, 0, 0.0, 1.0, 0.0, 0.0, "Show this constraint's ipo in the object's Ipo window");
		/* If this is on an object, add the constraint to the object */
		uiButSetFunc (but, activate_constraint_ipo_func, con, NULL);
		/* If this is on a bone, add the constraint to the action (if any) */
		//but = uiDefBut(block, BUT, B_CONSTRAINT_REDRAW, "Key", *xco+227, *yco, 41, 20, 0, 0.0, 1.0, 0.0, 0.0, "Add an influence keyframe to the constraint");
		/* Add a keyframe to the influence IPO */
		//uiButSetFunc (but, add_influence_key_to_constraint_func, con, NULL);
		(*yco)-=24;
	} else {
		(*yco)-=3;
	}
	
}

static uiBlock *add_constraintmenu(void *arg_unused)
{
	uiBlock *block;
	
	ListBase *conlist;
	char ownerstr[64];
	short type;
	short yco= 0;
	
	conlist = get_constraint_client(ownerstr, &type, NULL);
	
	block= uiNewBlock(&curarea->uiblocks, "add_constraintmenu", UI_EMBOSSP, UI_HELV, curarea->win);

	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_LOCLIKE,"Copy Location",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_ROTLIKE,"Copy Rotation",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_TRACKTO,"Track To",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_LOCKTRACK,"Locked Track",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_FOLLOWPATH,"Follow Path",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_STRETCHTO,"Stretch To",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");

	if (type==TARGET_BONE) {
	
		uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefBut(block, BUTM, B_CONSTRAINT_ADD_KINEMATIC,"IK Solver",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
		uiDefBut(block, BUTM, B_CONSTRAINT_ADD_ACTION,"Action",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
		
	}
	uiDefBut(block, SEPR, 0, "",					0, yco-=6, 120, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefBut(block, BUTM, B_CONSTRAINT_ADD_NULL,"Null",		0, yco-=20, 160, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiTextBoundsBlock(block, 50);
	uiBlockSetDirection(block, UI_DOWN);
		
	return block;
}

void do_constraintbuts(unsigned short event)
{
	Object *ob= OBACT;
	
	object_test_constraints(ob);
	
	switch(event) {
	case B_CONSTRAINT_CHANGENAME:
	case B_CONSTRAINT_TEST:
	case B_CONSTRAINT_REDRAW:
	case B_CONSTRAINT_CHANGETYPE:
		break;  // no handling
		
	case B_CONSTRAINT_DEL:
	case B_CONSTRAINT_CHANGETARGET:
		if(ob->pose) ob->pose->flag |= POSE_RECALC;	// checks & sorts pose channels
		DAG_scene_sort(G.scene);
		break;
		
	case B_CONSTRAINT_ADD_NULL:
		{
			bConstraint *con;
			
			con = add_new_constraint(CONSTRAINT_TYPE_NULL);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_KINEMATIC:
		{
			bConstraint *con;
			
			con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_TRACKTO:
		{
			bConstraint *con;

			con = add_new_constraint(CONSTRAINT_TYPE_TRACKTO);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_ROTLIKE:
		{
			bConstraint *con;

			con = add_new_constraint(CONSTRAINT_TYPE_ROTLIKE);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_LOCLIKE:
		{
			bConstraint *con;

			con = add_new_constraint(CONSTRAINT_TYPE_LOCLIKE);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_ACTION:
		{
			bConstraint *con;

			con = add_new_constraint(CONSTRAINT_TYPE_ACTION);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_LOCKTRACK:
		{
			bConstraint *con;

			con = add_new_constraint(CONSTRAINT_TYPE_LOCKTRACK);
			add_constraint_to_client(con);

			BIF_undo_push("Add constraint");
		}
		break;
	case B_CONSTRAINT_ADD_FOLLOWPATH:
		{
			bConstraint *con;

			con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
			add_constraint_to_client(con);

		}
		break;
	case B_CONSTRAINT_ADD_STRETCHTO:
	{
		bConstraint *con;
		con = add_new_constraint(CONSTRAINT_TYPE_STRETCHTO);
		add_constraint_to_client(con);
			
		BIF_undo_push("Add constraint");
	}
		break;

	default:
		break;
	}

	if(ob->pose) update_pose_constraint_flags(ob->pose);
	
	if(ob->type==OB_ARMATURE) DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	else DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
}

static void object_panel_constraint(void)
{
	uiBlock *block;
	ListBase *conlist;
	bConstraint *curcon;
	short xco, yco, type;
	char ownerstr[64];
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_constraint", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Constraints", "Object", 640, 0, 318, 204)==0) return;

	/* this is a variable height panel, newpanel doesnt force new size on existing panels */
	/* so first we make it default height */
	uiNewPanelHeight(block, 204);

	conlist = get_constraint_client(ownerstr, &type, NULL);
	
	if (conlist) {
		 
		uiDefBlockBut(block, add_constraintmenu, NULL, "Add Constraint", 0, 190, 130, 20, "Add a new constraint");
		
		/* print active object or bone */
		{
			short type;
			void *data=NULL;
			char str[64];
			
			str[0]= 0;
			get_constraint_client(NULL, &type, &data);
			if (data && type==TARGET_BONE){
				sprintf(str, "To Bone: %s", ((Bone*)data)->name);
			}
			else if(OBACT) {
				Object *ob= OBACT;
				sprintf(str, "To Object: %s", ob->id.name+2);
			}
			uiDefBut(block, LABEL, 1, str,	150, 190, 150, 20, NULL, 0.0, 0.0, 0, 0, "Displays Active Object or Bone name");
		}
		
		/* Go through the list of constraints and draw them */
		xco = 10;
		yco = 160;
		// local panel coords
		uiPanelPush(block);
		
		for (curcon = conlist->first; curcon; curcon=curcon->next) {
			/* Draw default constraint header */			
			draw_constraint(block, conlist, curcon, &xco, &yco, type);	
		}
		
		uiPanelPop(block);
		
		if(yco < 0) uiNewPanelHeight(block, 204-yco);
		
	}
}

static void object_panel_draw(Object *ob)
{
	uiBlock *block;
	int xco, a, dx, dy;
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_draw", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Draw", "Object", 320, 0, 318, 204)==0) return;

	/* LAYERS */
	xco= 120;
	dx= 35;
	dy= 30;

	uiDefBut(block, LABEL, 0, "Layers",				10,170,100,20, NULL, 0, 0, 0, 0, "");
	
	uiBlockBeginAlign(block);
	for(a=0; a<5; a++)
		uiDefButBitI(block, TOG, 1<<a, B_OBLAY+a, "",	(short)(xco+a*(dx/2)), 180, (short)(dx/2), (short)(dy/2), &(BASACT->lay), 0, 0, 0, 0, "");
	for(a=0; a<5; a++)
		uiDefButBitI(block, TOG, 1<<(a+10), B_OBLAY+a+10, "",	(short)(xco+a*(dx/2)), 165, (short)(dx/2), (short)(dy/2), &(BASACT->lay), 0, 0, 0, 0, "");
		
	xco+= 7;
	uiBlockBeginAlign(block);
	for(a=5; a<10; a++)
		uiDefButBitI(block, TOG, 1<<a, B_OBLAY+a, "",	(short)(xco+a*(dx/2)), 180, (short)(dx/2), (short)(dy/2), &(BASACT->lay), 0, 0, 0, 0, "");
	for(a=5; a<10; a++)
		uiDefButBitI(block, TOG, 1<<(a+10), B_OBLAY+a+10, "",	(short)(xco+a*(dx/2)), 165, (short)(dx/2), (short)(dy/2), &(BASACT->lay), 0, 0, 0, 0, "");

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
	uiDefButBitC(block, TOG, OB_AXIS, REDRAWVIEW3D, "Axis",			210, 80, 90, 20, &ob->dtx, 0, 0, 0, 0, "Displays the active object's centre and axis");
	
	uiDefButBitC(block, TOG, OB_TEXSPACE, REDRAWVIEW3D, "TexSpace",	120, 60, 90, 20, &ob->dtx, 0, 0, 0, 0, "Displays the active object's texture space");
	uiDefButBitC(block, TOG, OB_DRAWWIRE, REDRAWVIEW3D, "Wire",		210, 60, 90, 20, &ob->dtx, 0, 0, 0, 0, "Adds the active object's wireframe over solid drawing");
	
	uiDefButBitC(block, TOG, OB_DRAWTRANSP, REDRAWVIEW3D, "Transp",	120, 40, 90, 20, &ob->dtx, 0, 0, 0, 0, "Enables transparent materials for the active object (Mesh only)");
	uiDefButBitC(block, TOG, OB_DRAWXRAY, REDRAWVIEW3D, "X-ray",	210, 40, 90, 20, &ob->dtx, 0, 0, 0, 0, "Makes the active object draw in front of others");

}

static void softbody_bake(Object *ob)
{
	SoftBody *sb= ob->soft;
	ScrArea *sa;
	float frameleno= G.scene->r.framelen;
	int cfrao= CFRA;
	unsigned short event=0;
	short val;
	
	G.scene->r.framelen= 1.0;		// baking has to be in uncorrected time
	CFRA= sb->sfra;
	update_for_newframe_muted();	// put everything on this frame
	sbObjectToSoftbody(ob, NULL);	// put softbody in restposition
	ob->softflag |= OB_SB_BAKEDO;
	
	curarea->win_swap= 0;		// clean swapbuffers
	
	for(; CFRA <= sb->efra; CFRA++) {
		set_timecursor(CFRA);
		
		update_for_newframe_muted();
		
		for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
			if(sa->spacetype == SPACE_VIEW3D) {
				scrarea_do_windraw(sa);
			}
		}
		screen_swapbuffers();
		
		while(qtest()) {
			
			event= extern_qread(&val);
			if(event==ESCKEY) break;
		}
		if(event==ESCKEY) break;
	}
	
	if(event==ESCKEY) sbObjectToSoftbody(ob, NULL);	// clears all
	
	/* restore */
	waitcursor(0);
	ob->softflag &= ~OB_SB_BAKEDO;
	CFRA= cfrao;
	G.scene->r.framelen= frameleno;
	update_for_newframe_muted();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
}


void do_object_panels(unsigned short event)
{
	Object *ob;
	Effect *eff;
	
	ob= OBACT;

	switch(event) {
	case B_TRACKBUTS:
		ob= OBACT;
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_RECALCPATH:
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_PRINTSPEED:
		ob= OBACT;
		if(ob) {
			float vec[3];
			CFRA++;
			do_ob_ipo(ob);
			where_is_object(ob);
			VECCOPY(vec, ob->obmat[3]);
			CFRA--;
			do_ob_ipo(ob);
			where_is_object(ob);
			VecSubf(vec, vec, ob->obmat[3]);
			prspeed= Normalise(vec);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_PRINTLEN:
		ob= OBACT;
		if(ob && ob->type==OB_CURVE) {
			Curve *cu=ob->data;
			
			if(cu->path) prlen= cu->path->totdist; else prlen= -1.0;
			scrarea_queue_winredraw(curarea);
		} 
		break;
	case B_RELKEY:
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWIPO, 0);
		break;
	case B_CURVECHECK:
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		break;
	
	case B_SOFTBODY_CHANGE:
		ob= OBACT;
		if(ob) {
			ob->softflag |= OB_SB_REDO;
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SOFTBODY_DEL_VG:
		ob= OBACT;
		if(ob && ob->soft) {
			ob->soft->vertgroup= 0;
			ob->softflag |= OB_SB_REDO;
			allqueue(REDRAWBUTSOBJECT, 0);
			allqueue(REDRAWVIEW3D, 0);
		}
		break;
	case B_SOFTBODY_BAKE:
		ob= OBACT;
		if(ob && ob->soft) softbody_bake(ob);
		break;
	case B_SOFTBODY_BAKE_FREE:
		ob= OBACT;
		if(ob && ob->soft) sbObjectToSoftbody(ob, NULL);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
		
	default:
		if(event>=B_SELEFFECT && event<B_SELEFFECT+MAX_EFFECT) {
			ob= OBACT;
			if(ob) {
				int a=B_SELEFFECT;
				
				eff= ob->effect.first;
				while(eff) {
					if(event==a) eff->flag |= SELECT;
					else eff->flag &= ~SELECT;
					
					a++;
					eff= eff->next;
				}
				allqueue(REDRAWBUTSOBJECT, 0);
			}
		}
	}

}

static void object_panel_anim(Object *ob)
{
	uiBlock *block;
	char str[32];
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_anim", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Anim settings", "Object", 0, 0, 318, 204)==0) return;
	
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW,B_TRACKBUTS,"TrackX",	24,190,59,19, &ob->trackflag, 12.0, 0.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,B_TRACKBUTS,"Y",		85,190,19,19, &ob->trackflag, 12.0, 1.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,B_TRACKBUTS,"Z",		104,190,19,19, &ob->trackflag, 12.0, 2.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,B_TRACKBUTS,"-X",		124,190,24,19, &ob->trackflag, 12.0, 3.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,B_TRACKBUTS,"-Y",		150,190,24,19, &ob->trackflag, 12.0, 4.0, 0, 0, "Specify the axis that points to another object");
	uiDefButC(block, ROW,B_TRACKBUTS,"-Z",		178,190,24,19, &ob->trackflag, 12.0, 5.0, 0, 0, "Specify the axis that points to another object");
	uiBlockBeginAlign(block);
	uiDefButC(block, ROW,REDRAWVIEW3D,"UpX",	226,190,45,19, &ob->upflag, 13.0, 0.0, 0, 0, "Specify the axis that points up");
	uiDefButC(block, ROW,REDRAWVIEW3D,"Y",		274,190,20,19, &ob->upflag, 13.0, 1.0, 0, 0, "Specify the axis that points up");
	uiDefButC(block, ROW,REDRAWVIEW3D,"Z",		298,190,19,19, &ob->upflag, 13.0, 2.0, 0, 0, "Specify the axis that points up");
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, OB_DRAWKEY, REDRAWVIEW3D, "Draw Key",		24,160,71,19, &ob->ipoflag, 0, 0, 0, 0, "Draw object as key position");
	uiDefButBitC(block, TOG, OB_DRAWKEYSEL, REDRAWVIEW3D, "Draw Key Sel",	97,160,81,19, &ob->ipoflag, 0, 0, 0, 0, "Limit the drawing of object keys");
	uiDefButBitC(block, TOG, OB_POWERTRACK, REDRAWVIEW3D, "Powertrack",		180,160,78,19, &ob->transflag, 0, 0, 0, 0, "Switch objects rotation off");
	uiDefButBitS(block, TOG, PARSLOW, 0, "SlowPar",					260,160,56,19, &ob->partype, 0, 0, 0, 0, "Create a delay in the parent relationship");
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, OB_DUPLIFRAMES, REDRAWVIEW3D, "DupliFrames",	24,128,89,19, &ob->transflag, 0, 0, 0, 0, "Make copy of object for every frame");
	uiDefButBitC(block, TOG, OB_DUPLIVERTS, REDRAWVIEW3D, "DupliVerts",		114,128,82,19, &ob->transflag, 0, 0, 0, 0, "Duplicate child objects on all vertices");
	uiDefButBitC(block, TOG, OB_DUPLIROT, REDRAWVIEW3D, "Rot",		200,128,31,19, &ob->transflag, 0, 0, 0, 0, "Rotate dupli according to facenormal");
	uiDefButBitC(block, TOG, OB_DUPLINOSPEED, REDRAWVIEW3D, "No Speed",	234,128,82,19, &ob->transflag, 0, 0, 0, 0, "Set dupliframes to still, regardless of frame");
	uiBlockBeginAlign(block);
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupSta:",		24,105,141,19, &ob->dupsta, 1.0, (MAXFRAMEF - 1.0f), 0, 0, "Specify startframe for Dupliframes");
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupOn:",		170,105,146,19, &ob->dupon, 1.0, 1500.0, 0, 0, "");
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupEnd",		24,82,140,19, &ob->dupend, 1.0, MAXFRAMEF, 0, 0, "Specify endframe for Dupliframes");
	uiDefButS(block, NUM, REDRAWVIEW3D, "DupOff",		171,82,145,19, &ob->dupoff, 0.0, 1500.0, 0, 0, "");
	uiBlockBeginAlign(block);
	uiDefButBitC(block, TOG, OB_OFFS_OB, REDRAWALL, "Offs Ob",			24,51,56,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on its own objectipo");
	uiDefButBitC(block, TOG, OB_OFFS_PARENT, REDRAWALL, "Offs Par",			82,51,56,20 , &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the parent");
	uiDefButBitC(block, TOG, OB_OFFS_PARTICLE, REDRAWALL, "Offs Particle",		140,51,103,20, &ob->ipoflag, 0, 0, 0, 0, "Let the timeoffset work on the particle effect");
	
	uiBlockBeginAlign(block);
	uiDefButF(block, NUM, REDRAWALL, "TimeOffset:",			24,17,115,30, &ob->sf, -9000.0, 9000.0, 100, 0, "Specify an offset in frames");
	uiDefBut(block, BUT, B_AUTOTIMEOFS, "Automatic Time",	139,17,104,31, 0, 0, 0, 0, 0, "Generate automatic timeoffset values for all selected frames");
	uiDefBut(block, BUT, B_PRINTSPEED,	"PrSpeed",			248,17,67,31, 0, 0, 0, 0, 0, "Print objectspeed");
	uiBlockEndAlign(block);
	
	sprintf(str, "%.4f", prspeed);
	uiDefBut(block, LABEL, 0, str,							247,40,63,31, NULL, 1.0, 0, 0, 0, "");
	
}

void do_effects_panels(unsigned short event)
{
	Object *ob;
	Base *base;
	Effect *eff, *effn;
	int type;
	
	ob= OBACT;

	switch(event) {

    case B_AUTOTIMEOFS:
		auto_timeoffs();
		break;
	case B_FRAMEMAP:
		G.scene->r.framelen= G.scene->r.framapto;
		G.scene->r.framelen/= G.scene->r.images;
		allqueue(REDRAWALL, 0);
		break;
	case B_NEWEFFECT:
		if(ob) {
			if (BLI_countlist(&ob->effect)==MAX_EFFECT)
				error("Unable to add: effect limit reached");
			else
				copy_act_effect(ob);
		}
		BIF_undo_push("New effect");
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_DELEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			effn= eff->next;
			if(eff->flag & SELECT) {
				BLI_remlink(&ob->effect, eff);
				free_effect(eff);
				break;
			}
			eff= effn;
		}
		BIF_undo_push("Delete effect");
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_NEXTEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->next) {
					eff->flag &= ~SELECT;
					eff->next->flag |= SELECT;
				}
				break;
			}
			eff= eff->next;
		}
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_PREVEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->prev) {
					eff->flag &= ~SELECT;
					eff->prev->flag |= SELECT;
				}
				break;
			}
			eff= eff->next;
		}
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_CHANGEEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->type!=eff->buttype) {
					BLI_remlink(&ob->effect, eff);
					type= eff->buttype;
					free_effect(eff);
					eff= add_effect(type);
					BLI_addtail(&ob->effect, eff);
				}
				break;
			}
			eff= eff->next;
		}
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_CALCEFFECT:
		if(ob==0 || ob->type!=OB_MESH) break;
		eff= ob->effect.first;
		while(eff) {
			if(eff->flag & SELECT) {
				if(eff->type==EFF_PARTICLE) build_particle_system(ob);
				else if(eff->type==EFF_WAVE) freedisplist(&ob->disp);
			}
			eff= eff->next;
		}
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		break;
	case B_RECALCAL:
		base= FIRSTBASE;
		while(base) {
			if(base->lay & G.vd->lay) {
				ob= base->object;
				eff= ob->effect.first;
				while(eff) {
					if(eff->flag & SELECT) {
						if(eff->type==EFF_PARTICLE) build_particle_system(ob);
					}
					eff= eff->next;
				}
			}
			base= base->next;
		}
		allqueue(REDRAWVIEW3D, 0);
		break;

	default:
		if(event>=B_SELEFFECT && event<B_SELEFFECT+MAX_EFFECT) {
			ob= OBACT;
			if(ob) {
				int a=B_SELEFFECT;
				
				eff= ob->effect.first;
				while(eff) {
					if(event==a) eff->flag |= SELECT;
					else eff->flag &= ~SELECT;
					
					a++;
					eff= eff->next;
				}
				allqueue(REDRAWBUTSOBJECT, 0);
			}
		}
	}

}

/* Panel for particle interaction settings */
static void object_panel_deflectors(Object *ob)
{
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "object_panel_deflectors", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Constraints", "Object");
	if(uiNewPanel(curarea, block, "Particle Interaction", "Object", 640, 0, 318, 204)==0) return;

	/* should become button, option? */
	if(ob->pd==NULL) {
		ob->pd= MEM_callocN(sizeof(PartDeflect), "PartDeflect");
		/* and if needed, init here */
		ob->pd->pdef_sbdamp = 0.1;
		ob->pd->pdef_sbift  = 0.2;
		ob->pd->pdef_sboft  = 0.02;
	}
	
	if(ob->pd) {
		PartDeflect *pd= ob->pd;
		
		uiDefBut(block, LABEL, 0, "Fields",		10,180,140,20, NULL, 0.0, 0, 0, 0, "");
		
		uiBlockBeginAlign(block);
		uiDefButS(block, ROW, REDRAWVIEW3D, "None",			10,160,50,20, &pd->forcefield, 1.0, 0, 0, 0, "No force");
		uiDefButS(block, ROW, REDRAWVIEW3D, "Force field",	60,160,90,20, &pd->forcefield, 1.0, PFIELD_FORCE, 0, 0, "Object center attracts or repels particles");
		uiDefButS(block, ROW, REDRAWVIEW3D, "Wind",			10,140,50,20, &pd->forcefield, 1.0, PFIELD_WIND, 0, 0, "Constant force applied in direction of Object Z axis");
		uiDefButS(block, ROW, REDRAWVIEW3D, "Vortex field",	60,140,90,20, &pd->forcefield, 1.0, PFIELD_VORTEX, 0, 0, "Particles swirl around Z-axis of the object");

		uiBlockBeginAlign(block);
		uiDefButF(block, NUM, REDRAWVIEW3D, "Strength: ",	10,110,140,20, &pd->f_strength, -1000, 1000, 1000, 0, "Strength of force field");
		uiDefButF(block, NUM, REDRAWVIEW3D, "Fall-off: ",	10,90,140,20, &pd->f_power, 0, 10, 100, 0, "Falloff power (real gravitational fallof = 2)");
		
		uiBlockBeginAlign(block);
		uiDefButBitS(block, TOG, PFIELD_USEMAX, REDRAWVIEW3D, "Use MaxDist",	10,60,140,20, &pd->flag, 0.0, 0, 0, 0, "Use a maximum distance for the field to work");
		uiDefButF(block, NUM, REDRAWVIEW3D, "MaxDist: ",	10,40,140,20, &pd->maxdist, 0, 1000.0, 100, 0, "Maximum distance for the field to work");
		uiBlockEndAlign(block);

		if(ob->softflag & OB_SB_ENABLE) {
			uiDefBut(block, LABEL, 0, "Object is Softbody,",		160,160,150,20, NULL, 0.0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "no Deflection possible",		160,140,150,20, NULL, 0.0, 0, 0, 0, "");
			pd->deflect= 0;
		}
		else {
			
			uiDefBut(block, LABEL, 0, "Deflection",	160,180,140,20, NULL, 0.0, 0, 0, 0, "");
			
			/* only meshes collide now */
			if(ob->type==OB_MESH) {
				uiDefButBitS(block, TOG, 1, B_REDR, "Deflection",160,160,150,20, &pd->deflect, 0, 0, 0, 0, "Deflects particles based on collision");
				uiDefBut(block, LABEL, 0, "Particles",			160,140,150,20, NULL, 0.0, 0, 0, 0, "");
				
				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_DIFF, "Damping: ",		160,120,150,20, &pd->pdef_damp, 0.0, 1.0, 10, 0, "Amount of damping during particle collision");
				uiDefButF(block, NUM, B_DIFF, "Rnd Damping: ",	160,100,150,20, &pd->pdef_rdamp, 0.0, 1.0, 10, 0, "Random variation of damping");
				uiDefButF(block, NUM, B_DIFF, "Permeability: ",	160,80,150,20, &pd->pdef_perm, 0.0, 1.0, 10, 0, "Chance that the particle will pass through the mesh");
				uiBlockEndAlign(block);
				
				uiDefBut(block, LABEL, 0, "Soft Body",			160,60,150,20, NULL, 0.0, 0, 0, 0, "");

				uiBlockBeginAlign(block);
				uiDefButF(block, NUM, B_DIFF, "Damping:",	160,40,150,20, &pd->pdef_sbdamp, 0.0, 1.0, 10, 0, "Amount of damping during softbody collision");
				uiDefButF(block, NUM, B_DIFF, "Inner:",	160,20,150,20, &pd->pdef_sbift, 0.001, 1.0, 10, 0, "Inner face thickness");
				uiDefButF(block, NUM, B_DIFF, "Outer:",	160, 0,150,20, &pd->pdef_sboft, 0.001, 1.0, 10, 0, "Outer face thickness");
				uiBlockBeginAlign(block);
 			    uiDefButBitS(block, TOG, PDEFLE_DEFORM , 0,"UMS or CRASH",	0,0,150,20, &pd->flag, 0, 0, 0, 0, "Let collision object move with armatures/lattices WARNING logical circles will CRASH");
			}		
		}	
	}
}


/* Panel for softbodies */

static void object_softbodies(Object *ob)
{
	uiBlock *block;
	
	block= uiNewBlock(&curarea->uiblocks, "object_softbodies", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Constraints", "Object");
	if(uiNewPanel(curarea, block, "Softbody", "Object", 640, 0, 318, 204)==0) return;

	/* do not allow to combine with force fields */
	if(ob->pd) {
		PartDeflect *pd= ob->pd;
		
		if(pd->deflect) {
			uiDefBut(block, LABEL, 0, "Object has Deflection,",		10,160,300,20, NULL, 0.0, 0, 0, 0, "");
			uiDefBut(block, LABEL, 0, "no Softbody possible",		10,140,300,20, NULL, 0.0, 0, 0, 0, "");
			ob->softflag &= ~OB_SB_ENABLE;
			
			return;
		}
	}
	
	uiDefButBitS(block, TOG, OB_SB_ENABLE, REDRAWBUTSOBJECT, "Enable Soft Body",	10,200,150,20, &ob->softflag, 0, 0, 0, 0, "Sets object to become soft body");
	uiDefBut(block, LABEL, 0, "",	160, 200,150,20, NULL, 0.0, 0.0, 0, 0, "");	// alignment reason
	
	if(ob->softflag & OB_SB_ENABLE) {
		SoftBody *sb= ob->soft;
		int defCount;
		char *menustr;
		
		if(sb==NULL) {
			sb= ob->soft= sbNew();
			ob->softflag |= OB_SB_GOAL|OB_SB_EDGES;
			// default add edges for softbody
			if(ob->type==OB_MESH) {
				Mesh *me= ob->data;
				if(me->medge==NULL) make_edges(me);
			}
		}
		
		uiDefButBitS(block, TOG, OB_SB_BAKESET, REDRAWBUTSOBJECT, "Bake settings",	180,200,130,20, &ob->softflag, 0, 0, 0, 0, "To convert simulation into baked (cached) result");
		
		if(sb->keys) uiSetButLock(1, "SoftBody is baked, free it first");
		
		if(ob->softflag & OB_SB_BAKESET) {
			uiBlockBeginAlign(block);
			uiDefButS(block, NUM, B_DIFF, "Start:",			10, 170,100,20, &sb->sfra, 1.0, 10000.0, 10, 0, "Start frame for baking");
			uiDefButS(block, NUM, B_DIFF, "End:",			110, 170,100,20, &sb->efra, 1.0, 10000.0, 10, 0, "End frame for baking");
			uiDefButS(block, NUM, B_DIFF, "Interval:",		210, 170,100,20, &sb->interval, 1.0, 10.0, 10, 0, "Interval in frames between baked keys");
			
			uiClearButLock();
			
			uiBlockBeginAlign(block);
			if(sb->keys) {
				char str[128];
				uiDefIconTextBut(block, BUT, B_SOFTBODY_BAKE_FREE, ICON_X, "FREE BAKE", 10, 120,300,20, NULL, 0.0, 0.0, 0, 0, "Free baked result");
				sprintf(str, "Stored %d vertices %d keys %.3f MB", sb->totpoint, sb->totkey, ((float)16*sb->totpoint*sb->totkey)/(1024.0*1024.0));
				uiDefBut(block, LABEL, 0, str, 10, 100,300,20, NULL, 0.0, 0.0, 00, 0, "");
			}
			else				
				uiDefBut(block, BUT, B_SOFTBODY_BAKE, "BAKE",	10, 120,300,20, NULL, 0.0, 0.0, 10, 0, "Start baking. Press ESC to exit without baking");
		}
		else {
			/* GENERAL STUFF */
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_DIFF, "Friction:",		10, 170,150,20, &sb->mediafrict, 0.0, 10.0, 10, 0, "General media friction for point movements");
			uiDefButF(block, NUM, B_DIFF, "Mass:",			160, 170,150,20, &sb->nodemass , 0.001, 50.0, 10, 0, "Point Mass, the heavier the slower");
			uiDefButF(block, NUM, B_DIFF, "Grav:",			10,150,150,20, &sb->grav , 0.0, 10.0, 10, 0, "Apply gravitation to point movement");
			uiDefButF(block, NUM, B_DIFF, "Speed:",			160,150,150,20, &sb->physics_speed , 0.01, 100.0, 10, 0, "Tweak timing for physics to control frequency and speed");
			uiDefButF(block, NUM, B_DIFF, "Error Limit:",	10,130,150,20, &sb->rklimit , 0.01, 1.0, 10, 0, "The Runge-Kutta ODE solver error limit, low value gives more precision");
			uiDefButBitS(block, TOG, OB_SB_POSTDEF, B_DIFF, "Apply Deform First",	160,130,150,20, &ob->softflag, 0, 0, 0, 0, "Softbody is calculated AFTER Deformation");
			uiBlockEndAlign(block);
			
			/* GOAL STUFF */
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, OB_SB_GOAL, B_SOFTBODY_CHANGE, "Use Goal",	10,100,130,20, &ob->softflag, 0, 0, 0, 0, "Define forces for vertices to stick to animated position");
			
			menustr= get_vertexgroup_menustr(ob);
			defCount=BLI_countlist(&ob->defbase);
			if(defCount==0) sb->vertgroup= 0;
			uiDefButS(block, MENU, B_SOFTBODY_CHANGE, menustr,		140,100,20,20, &sb->vertgroup, 0, defCount, 0, 0, "Browses available vertex groups");
			
			if(sb->vertgroup) {
				bDeformGroup *defGroup = BLI_findlink(&ob->defbase, sb->vertgroup-1);
				if(defGroup)
					uiDefBut(block, BUT, B_DIFF, defGroup->name,	160,100,130,20, NULL, 0.0, 0.0, 0, 0, "Name of current vertex group");
				else
					uiDefBut(block, BUT, B_DIFF, "(no group)",	160,100,130,20, NULL, 0.0, 0.0, 0, 0, "Vertex Group doesn't exist anymore");
				uiDefIconBut(block, BUT, B_SOFTBODY_DEL_VG, ICON_X, 290,100,20,20, 0, 0, 0, 0, 0, "Disable use of vertex group");
			}
			else {
				uiDefButF(block, NUM, B_SOFTBODY_CHANGE, "Goal:",	160,100,150,20, &sb->defgoal, 0.0, 1.0, 10, 0, "Default Goal (vertex target position) value, when no Vertex Group used");
			}
			MEM_freeN (menustr);

			uiDefButF(block, NUM, B_DIFF, "G Stiff:",	10,80,150,20, &sb->goalspring, 0.0, 0.999, 10, 0, "Goal (vertex target position) spring stiffness");
			uiDefButF(block, NUM, B_DIFF, "G Damp:",	160,80,150,20, &sb->goalfrict  , 0.0, 10.0, 10, 0, "Goal (vertex target position) friction");
			uiDefButF(block, NUM, B_SOFTBODY_CHANGE, "G Min:",		10,60,150,20, &sb->mingoal, 0.0, 1.0, 10, 0, "Goal minimum, vertex group weights are scaled to match this range");
			uiDefButF(block, NUM, B_SOFTBODY_CHANGE, "G Max:",		160,60,150,20, &sb->maxgoal, 0.0, 1.0, 10, 0, "Goal maximum, vertex group weights are scaled to match this range");
			uiBlockEndAlign(block);
			
			/* EDGE SPRING STUFF */
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, OB_SB_EDGES, B_SOFTBODY_CHANGE, "Use Edges",		10,30,150,20, &ob->softflag, 0, 0, 0, 0, "Use Edges as springs");
			uiDefButBitS(block, TOG, OB_SB_QUADS, B_SOFTBODY_CHANGE, "Stiff Quads",		160,30,150,20, &ob->softflag, 0, 0, 0, 0, "Adds diagonal springs on 4-gons");
			uiDefButF(block, NUM, B_DIFF, "E Stiff:",	10,10,150,20, &sb->inspring, 0.0,  0.999, 10, 0, "Edge spring stiffness");
			uiDefButF(block, NUM, B_DIFF, "E Damp:",	160,10,150,20, &sb->infrict, 0.0,  10.0, 10, 0, "Edge spring friction");
			uiBlockEndAlign(block);
		}
	}
	uiBlockEndAlign(block);

}

static void object_panel_effects(Object *ob)
{
	Effect *eff;
	uiBlock *block;
	int a;
	short x, y;
	
	block= uiNewBlock(&curarea->uiblocks, "object_panel_effects", UI_EMBOSS, UI_HELV, curarea->win);
	uiNewPanelTabbed("Constraints", "Object");
	if(uiNewPanel(curarea, block, "Effects", "Object", 640, 0, 418, 204)==0) return;

	/* EFFECTS */
	
	if (ob->type == OB_MESH) {
		uiBlockBeginAlign(block);
		uiDefBut(block, BUT, B_NEWEFFECT, "NEW Effect", 550,187,124,27, 0, 0, 0, 0, 0, "Create a new effect");
		uiDefBut(block, BUT, B_DELEFFECT, "Delete", 676,187,62,27, 0, 0, 0, 0, 0, "Delete the effect");
		uiBlockEndAlign(block);
	}

	/* select effs */
	eff= ob->effect.first;
	a= 0;
	while(eff) {
		
		x= 15 * a + 550;
		y= 172; // - 12*( abs(a/10) ) ;
		uiDefButBitS(block, TOG, SELECT, B_SELEFFECT+a, "", x, y, 15, 12, &eff->flag, 0, 0, 0, 0, "");
		
		a++;
		if(a==MAX_EFFECT) break;
		eff= eff->next;
	}
	
	eff= ob->effect.first;
	while(eff) {
		if(eff->flag & SELECT) break;
		eff= eff->next;
	}
	
	if(eff) {
		uiDefButS(block, MENU, B_CHANGEEFFECT, "Particles %x1", 895,187,107,27, &eff->buttype, 0, 0, 0, 0, "Set effect type");
		
		if(eff->type==EFF_PARTICLE) {
			PartEff *paf;
			
			paf= (PartEff *)eff;
			
			uiDefBut(block, BUT, B_RECALCAL, "RecalcAll", 741,187,67,27, 0, 0, 0, 0, 0, "Update the particle system");
			uiBlockBeginAlign(block);
			uiDefButBitS(block, TOG, PAF_STATIC, B_CALCEFFECT, "Static",	825,187,67,27, &paf->flag, 0, 0, 0, 0, "Make static particles (deform only works with SubSurf)");
			if(paf->flag & PAF_STATIC)
				uiDefButBitS(block, TOG, PAF_ANIMATED, B_DIFF, "Animated",825,167,67,20, &paf->flag, 0, 0, 0, 0, "Static particles are recalculated each rendered frame");
			
			uiBlockBeginAlign(block);
			uiDefButI(block, NUM, B_CALCEFFECT, "Tot:",			550,146,91,20, &paf->totpart, 1.0, 100000.0, 0, 0, "Set the total number of particles");
			if(paf->flag & PAF_STATIC) {
				uiDefButS(block, NUM, REDRAWVIEW3D, "Step:",	644,146,84+97,20, &paf->staticstep, 1.0, 100.0, 10, 0, "For static duplicators, the Step value skips particles");
			}
			else {
				uiDefButF(block, NUM, B_CALCEFFECT, "Sta:",		644,146,84,20, &paf->sta, -250.0, 9000.0, 100, 0, "Specify the startframe");
				uiDefButF(block, NUM, B_CALCEFFECT, "End:",		731,146,97,20, &paf->end, 1.0, 9000.0, 100, 0, "Specify the endframe");
			}
			uiDefButF(block, NUM, B_CALCEFFECT, "Life:",		831,146,88,20, &paf->lifetime, 1.0, 9000.0, 100, 0, "Specify the life span of the particles");
			uiDefButI(block, NUM, B_CALCEFFECT, "Keys:",		922,146,80,20, &paf->totkey, 1.0, 100.0, 0, 0, "Specify the number of key positions");
			
			uiDefButS(block, NUM, B_REDR,		"CurMul:",		550,124,91,20, &paf->curmult, 0.0, 3.0, 0, 0, "Multiply the particles");
			uiDefButS(block, NUM, B_CALCEFFECT, "Mat:",			644,124,84,20, paf->mat+paf->curmult, 1.0, 8.0, 0, 0, "Specify the material used for the particles");
			uiDefButF(block, NUM, B_CALCEFFECT, "Mult:",		730,124,98,20, paf->mult+paf->curmult, 0.0, 1.0, 10, 0, "Probability \"dying\" particle spawns a new one.");
			uiDefButF(block, NUM, B_CALCEFFECT, "Life:",		831,124,89,20, paf->life+paf->curmult, 1.0, 600.0, 100, 0, "Specify the lifespan of the next generation particles");
			uiDefButS(block, NUM, B_CALCEFFECT, "Child:",		922,124,80,20, paf->child+paf->curmult, 1.0, 600.0, 100, 0, "Specify the number of children of a particle that multiply itself");
			
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_CALCEFFECT, "Randlife:",	550,96,96,20, &paf->randlife, 0.0, 2.0, 10, 0, "Give the particlelife a random variation");
			uiDefButI(block, NUM, B_CALCEFFECT, "Seed:",		652,96,80,20, &paf->seed, 0.0, 255.0, 0, 0, "Set an offset in the random table");

			uiDefButBitS(block, TOG, PAF_FACE, B_CALCEFFECT, "Face",		735,96,46,20, &paf->flag, 0, 0, 0, 0, "Emit particles also from faces");
			uiDefButBitS(block, TOG, PAF_BSPLINE, B_CALCEFFECT, "Bspline",	782,96,54,20, &paf->flag, 0, 0, 0, 0, "Use B spline formula for particle interpolation");
			uiDefButS(block, TOG, REDRAWVIEW3D, "Vect",				837,96,45,20, &paf->stype, 0, 0, 0, 0, "Give the particles a rotation direction");
			uiDefButF(block, NUM, B_DIFF,			"VectSize",		885,96,116,20, &paf->vectsize, 0.0, 1.0, 10, 0, "Set the speed for Vect");	

			uiBlockBeginAlign(block);
			uiBlockSetCol(block, TH_BUT_SETTING2);
			uiDefButF(block, NUM, B_CALCEFFECT, "Norm:",		550,67,96,20, &paf->normfac, -2.0, 2.0, 10, 0, "Let the mesh give the particle a starting speed");
			uiDefButF(block, NUM, B_CALCEFFECT, "Ob:",		649,67,86,20, &paf->obfac, -1.0, 1.0, 10, 0, "Let the object give the particle a starting speed");
			uiDefButF(block, NUM, B_CALCEFFECT, "Rand:",		738,67,86,20, &paf->randfac, 0.0, 2.0, 10, 0, "Give the startingspeed a random variation");
			uiDefButF(block, NUM, B_CALCEFFECT, "Tex:",		826,67,85,20, &paf->texfac, 0.0, 2.0, 10, 0, "Let the texture give the particle a starting speed");
			uiDefButF(block, NUM, B_CALCEFFECT, "Damp:",		913,67,89,20, &paf->damp, 0.0, 1.0, 10, 0, "Specify the damping factor");
			uiBlockSetCol(block, TH_AUTO);
			
			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_CALCEFFECT, "X:",		550,31,72,20, paf->force, -1.0, 1.0, 1, 0, "Specify the X axis of a continues force");
			uiDefButF(block, NUM, B_CALCEFFECT, "Y:",		624,31,78,20, paf->force+1,-1.0, 1.0, 1, 0, "Specify the Y axis of a continues force");
			uiDefBut(block, LABEL, 0, "Force:",				550,9,72,20, NULL, 1.0, 0, 0, 0, "");
			uiDefButF(block, NUM, B_CALCEFFECT, "Z:",		623,9,79,20, paf->force+2, -1.0, 1.0, 1, 0, "Specify the Z axis of a continues force");

			uiBlockBeginAlign(block);
			uiDefButF(block, NUM, B_CALCEFFECT, "X:",		722,31,74,20, paf->defvec, -1.0, 1.0, 1, 0, "Specify the X axis of a force, determined by the texture");
			uiDefButF(block, NUM, B_CALCEFFECT, "Y:",		798,31,74,20, paf->defvec+1,-1.0, 1.0, 1, 0, "Specify the Y axis of a force, determined by the texture");
			uiDefBut(block, LABEL, 0, "Texture:",			722,9,74,20, NULL, 1.0, 0, 0, 0, "");
			uiDefButF(block, NUM, B_CALCEFFECT, "Z:",		797,9,75,20, paf->defvec+2, -1.0, 1.0, 1, 0, "Specify the Z axis of a force, determined by the texture");
			uiBlockEndAlign(block);

			uiDefButS(block, ROW, B_CALCEFFECT, "Int",		875,9,32,43, &paf->texmap, 14.0, 0.0, 0, 0, "Use texture intensity as a factor for texture force");

			uiBlockBeginAlign(block);
			uiDefButS(block, ROW, B_CALCEFFECT, "RGB",		911,31,45,20, &paf->texmap, 14.0, 1.0, 0, 0, "Use RGB values as a factor for particle speed vector");
			uiDefButS(block, ROW, B_CALCEFFECT, "Grad",		958,31,44,20, &paf->texmap, 14.0, 2.0, 0, 0, "Use texture gradient as a factor for particle speed vector");
			uiDefButF(block, NUM, B_CALCEFFECT, "Nabla:",		911,9,91,20, &paf->nabla, 0.0001f, 1.0, 1, 0, "Specify the dimension of the area for gradient calculation");

		}
	}
}

void object_panels()
{
	Object *ob;

	/* check context here */
	ob= OBACT;
	if(ob) {
		if(ob->id.lib) uiSetButLock(1, "Can't edit library data");

		object_panel_anim(ob);
		object_panel_draw(ob);
		object_panel_constraint();
		if(ob->type==OB_MESH) {
			object_panel_effects(ob);
		}
		object_panel_deflectors(ob);
		object_softbodies(ob);

		uiClearButLock();
	}
}

