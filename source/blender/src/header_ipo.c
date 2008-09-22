/**
 * header_ipo.c oct-2003
 *
 * Functions to draw the "Ipo Curve Editor" window header
 * and handle user events sent to it.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_action_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ID.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_world_types.h"

#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"

#include "BSE_drawipo.h"
#include "BSE_editipo_types.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"

#include "BIF_editaction.h"
#include "BIF_editconstraint.h"
#include "BIF_interface.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "nla.h"

#include "blendef.h"
#include "mydevice.h"

extern int totipo_edit, totipo_sel;

/* headerbutton call, assuming full context is set */
/* it aligns with editipo.c, verify_ipo */
void spaceipo_assign_ipo(SpaceIpo *si, Ipo *ipo)
{
	if(si->from==NULL || si->from->lib) return;
	
	if(ipo) ipo->id.us++;

	/* first check action ipos */
	if(si->actname && si->actname[0]) {
		Object *ob= (Object *)si->from;
		bActionChannel *achan;
		
		if(ob->action) {
			achan= verify_action_channel(ob->action, si->actname);
		
			if(achan) {
				/* constraint exception */
				if(si->blocktype==ID_CO) {
					bConstraintChannel *conchan= get_constraint_channel(&achan->constraintChannels, si->constname);
					if(conchan) {
						if(conchan->ipo)
							conchan->ipo->id.us--;
						conchan->ipo= ipo;
					}
				}
				else {
					if(achan->ipo)
						achan->ipo->id.us--;
					achan->ipo= ipo;
				}
			}
		}
	}
	else {
		switch(GS(si->from->name)) {
			case ID_OB:
			{
				Object *ob= (Object *)si->from;
				/* constraint exception */
				if(si->blocktype==ID_CO) {
					/* check the local constraint ipo */
					if(si->bonename && si->bonename[0] && ob->pose) {
						bPoseChannel *pchan= get_pose_channel(ob->pose, si->bonename);
						bConstraint *con;

						for(con= pchan->constraints.first; con; con= con->next)
							if(strcmp(con->name, si->constname)==0)
								break;
						if(con) {
							if(con->ipo)
								con->ipo->id.us--;
							con->ipo= ipo;
						}
					}
					else {
						bConstraintChannel *conchan= get_constraint_channel(&ob->constraintChannels, si->constname);
						if(conchan) {
							if(conchan->ipo)
								conchan->ipo->id.us--;
							conchan->ipo= ipo;
						}
					}
				}
				else if(si->blocktype==ID_FLUIDSIM) { // NT
					FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
					if( fluidmd && fluidmd->fss && 
						(fluidmd->fss->ipo) ) {
						// decrement users counter
						fluidmd->fss->ipo->id.us--; 
					}
					fluidmd->fss->ipo = ipo;
				} 
				else if(si->blocktype==ID_PA) {
					ParticleSystem *psys=psys_get_current(ob);
					if(psys){
						if(psys->part->ipo){
							psys->part->ipo->id.us--;
						}
						psys->part->ipo = ipo;
					}
				}
				else if(si->blocktype==ID_OB) {
					if(ob->ipo)
						ob->ipo->id.us--;
					ob->ipo= ipo;
				}
			}
				break;
			case ID_MA:
			{
				Material *ma= (Material *)si->from;
				
				if(ma->ipo)
					ma->ipo->id.us--;
				ma->ipo= ipo;
			}
				break;
			case ID_TE:
			{
				Tex *tex= (Tex *)si->from;
				
				if(tex->ipo)
					tex->ipo->id.us--;
				tex->ipo= ipo;
			}
				break;
			case ID_SEQ:
			{
				Sequence *seq= (Sequence *)si->from;	/* note, sequence is mimicing Id */
				
				if(seq->ipo)
					seq->ipo->id.us--;
				seq->ipo= ipo;
			}
				break;
			case ID_CU:
			{
				Curve *cu= (Curve *)si->from;
				
				if(cu->ipo)
					cu->ipo->id.us--;
				cu->ipo= ipo;
			}
				break;
			case ID_KE:
			{
				Key *key= (Key *)si->from;
				
				if(key->ipo)
					key->ipo->id.us--;
				key->ipo= ipo;
			}
				break;
			case ID_WO:
			{
				World *wo= (World *)si->from;
				
				if(wo->ipo)
					wo->ipo->id.us--;
				wo->ipo= ipo;
			}
				break;
			case ID_LA:
			{
				Lamp *la= (Lamp *)si->from;
				
				if(la->ipo)
					la->ipo->id.us--;
				la->ipo= ipo;
			}
				break;
			case ID_CA:
			{
				Camera *ca= (Camera *)si->from;
				
				if(ca->ipo)
					ca->ipo->id.us--;
				ca->ipo= ipo;
			}
				break;
			case ID_SO:
			{
				bSound *snd= (bSound *)si->from;
				
				if(snd->ipo)
					snd->ipo->id.us--;
				snd->ipo= ipo;
			}
		}
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWBUTSALL, 0);
	
}


static void do_ipo_editmenu_transformmenu(void *arg, int event)
{
	switch(event)
	{
	case 0: /* grab/move */
		transform_ipo('g');
		break;
	case 1: /* rotate */
		transform_ipo('r');
		break;
	case 2: /* scale */
		transform_ipo('s');
		break;
	}
}

static uiBlock *ipo_editmenu_transformmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_transformmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_transformmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move|G", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Rotate|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Scale|S", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_ipo_editmenu_snapmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* Horizontal */
	case 2: /* To Next */
	case 3: /* To Frame */
	case 4: /* To Current Frame */
		ipo_snap(event);
	    break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *ipo_editmenu_snapmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_snapmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_snapmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Horizontal|Shift S, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Next|Shift S, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Frame|Shift S, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "To Current Frame|Shift S, 4",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_ipo_editmenu_mirrormenu(void *arg, int event)
{
	switch(event) {
		case 1: /* mirror over current frame */
		case 2: /* mirror over frame 0 */
		case 3: /* mirror over horizontal axis */
			ipo_mirror(event);
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *ipo_editmenu_mirrormenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_mirrormenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_mirrormenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Over Current Frame|Shift M, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Over Vertical Axis|Shift M, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Over Horizontal Axis|Shift M, 3",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");	
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
} 

static void do_ipo_editmenu_joinmenu(void *arg, int event)
{
	switch(event) {
	case 1: /* All Selected */
	case 2: /* Selected Doubles */
		join_ipo(event);
	    break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *ipo_editmenu_joinmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_joinmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_joinmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "All Selected|Ctrl J, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selected Doubles|Ctrl J, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_ipo_editmenu_keymenu(void *arg, int event)
{
	Key *key;
	KeyBlock *kb;
	Object *ob= OBACT;

	if(G.sipo->blocktype==ID_KE && totipo_edit==0 && totipo_sel==0) {
		key= ob_get_key((Object *)G.sipo->from);
		if(key==NULL) return;

		kb= BLI_findlink(&key->block, ob->shapenr-1);
		kb->type= 0;
		switch(event){
			case 0:
				kb->type= KEY_LINEAR;
				break;
			case 1:
				kb->type= KEY_CARDINAL;
				break;
			case 2:
				kb->type= KEY_BSPLINE;
				break;
		}
	}
}

static uiBlock *ipo_editmenu_keymenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_keymenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_keymenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linear|T, 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cardinal|T, 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "BSpline|T, 3", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;

}

static void do_ipo_editmenu_handlemenu(void *arg, int event)
{
	switch(event){
	case 0:
		sethandles_ipo(HD_AUTO);
		break;
	case 1:
	case 2:
		sethandles_ipo(HD_ALIGN);
		break;
	case 3:
		sethandles_ipo(HD_VECT);
		break;
	case 4:
		sethandles_ipo(HD_AUTO_ANIM);
		break;
	}
}

static uiBlock *ipo_editmenu_handlemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_handlemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_handlemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto|Shift H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto Clamped|Alt H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Aligned|H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Free|H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Vector|V", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_ipo_editmenu_intpolmenu(void *arg, int event)
{
	EditIpo *ei;
	int a;

	get_status_editipo();

	ei = G.sipo->editipo;

	switch(event)
	{
	case 0:
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				ei->icu->ipo= IPO_CONST;
			}
		}
		break;
	case 1:
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				ei->icu->ipo= IPO_LIN;
			}
		}
		break;
	case 2:
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN3(ei, flag & IPO_VISIBLE, flag & IPO_SELECT, icu) {
				ei->icu->ipo= IPO_BEZ;
			}
		}
		break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *ipo_editmenu_intpolmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_intpolmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_intpolmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Constant|T, 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linear|T, 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bezier|T, 3", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_ipo_editmenu_extendmenu(void *arg, int event)
{
	switch(event)
	{
	case 0:
		do_ipo_buttons(B_IPOCONT);
		break;
	case 1:
		do_ipo_buttons(B_IPOEXTRAP);
		break;
	case 2:
		do_ipo_buttons(B_IPOCYCLIC);
		break;
	case 3:
		do_ipo_buttons(B_IPOCYCLICX);
		break;
	}
}

static uiBlock *ipo_editmenu_extendmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_extendmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_extendmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Constant|E, 1", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrapolation|E, 2", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cyclic|E, 3", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cyclic Extrapolation|E, 4", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}


static void do_ipo_editmenu(void *arg, int event)
{
	switch(event)
	{
	case 0:
		del_ipo(1);
		break;
	case 1:
		add_duplicate_editipo();
		break;
	case 2:
		ipo_record();
		break;
	case 3:
		mainqenter(IKEY, 1);
		break;
	case 4 :
		add_blockhandler(curarea, IPO_HANDLER_PROPERTIES, UI_PNL_UNSTOW);
		break;
	case 5:
		//join_ipo();
		break;
	case 6:
		/*IPO Editmode*/
		set_editflag_editipo();
		break;
	case 7:
		sethandles_ipo(HD_AUTO_ANIM);
		break;
	case 8: /* clean ipo */
		clean_ipo();
		break;
	case 9: /* smooth ipo */
		smooth_ipo();
		break;
	}
}

static uiBlock *ipo_editmenu(void *arg_unused)
{
	uiBlock *block;
	EditIpo *ei;
	short yco= 0, menuwidth=120;
	int a,isedit = 0;

	get_status_editipo();

	ei = G.sipo->editipo;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
	uiDefIconTextBlockBut(block, ipo_editmenu_transformmenu, NULL, ICON_RIGHTARROW_THIN, "Transform", 0, yco-=20, 120, 19, "");	
	
	uiDefIconTextBlockBut(block, ipo_editmenu_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");	

	uiDefIconTextBlockBut(block, ipo_editmenu_mirrormenu, NULL, ICON_RIGHTARROW_THIN, "Mirror", 0, yco-=20, 120, 19, "");	
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");	

	/*Look to see if any ipos are being edited, so there can be a check next to the menu option*/
	for(a=0; a<G.sipo->totipo; a++, ei++) {		
		if(ei->icu) {
			if(ei->flag & IPO_VISIBLE) {
				if(totipo_edit && (ei->flag & IPO_EDIT)) {
					isedit = 1;
					break;
				}
			}
		}
	}
	if(isedit)
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT,   "Edit Selected|TAB", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");		
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Edit Selected|TAB", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 6, "");
	
	ei = get_active_editipo();
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(ei && ei->icu && ei->icu->driver)
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert 1:1 Curve...|I", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe...|I", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Record Mouse Movement|Ctrl R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clean IPO Curves|O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Smooth IPO Curves|Shift O", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 9, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBlockBut(block, ipo_editmenu_joinmenu, NULL, ICON_RIGHTARROW_THIN, "Join", 0, yco-=20, 120, 19, "");	

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto Clamped Handles|Alt H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 7, "");
	
	if (!G.sipo->showkey){
		uiDefIconTextBlockBut(block, ipo_editmenu_extendmenu, NULL, ICON_RIGHTARROW_THIN, "Extend Mode", 0, yco-=20, 120, 19, "");	
		uiDefIconTextBlockBut(block, ipo_editmenu_intpolmenu, NULL, ICON_RIGHTARROW_THIN, "Interpolation Mode   ", 0, yco-=20, 120, 20, "");
		if(ei != NULL && (ei->flag & IPO_EDIT))
			uiDefIconTextBlockBut(block, ipo_editmenu_handlemenu, NULL, ICON_RIGHTARROW_THIN, "Handle Type", 0, yco-=20, 120, 19, "");
		if(G.sipo->blocktype==ID_KE && totipo_edit==0 && totipo_sel==0)
			uiDefIconTextBlockBut(block, ipo_editmenu_keymenu, NULL, ICON_RIGHTARROW_THIN, "Key Type", 0, yco-=20, 120, 19, "");
	}
	

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

static void do_ipo_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);
	
	switch(event)
	{
	case 1:
		do_ipo_buttons(B_IPOHOME);
		break;
	case 2:
		ipo_toggle_showkey();
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case 3:
		move_to_frame();
		break;
	case 4:
		mainqenter(PADPLUSKEY,1);
		break;
	case 5:
		mainqenter(PADMINUS,1);
		break;
	case 6: /* Play Animation */
		play_anim(0);
		break;
	case 7: /* Play Animation in All */
		play_anim(1);
		break;	
	case 8:
		add_blockhandler(curarea, IPO_HANDLER_PROPERTIES, UI_PNL_UNSTOW);
		break;	
	case 9:
		G.v2d->flag ^= V2D_VIEWLOCK;
		if(G.v2d->flag & V2D_VIEWLOCK)
			view2d_do_locks(curarea, 0);
		break;	
	case 10: /* center view to current frame */
		center_currframe();
		scrarea_queue_winredraw(curarea);
		break;
	case 11:
		do_ipo_buttons(B_IPOVIEWCENTER);
		break;
	case 12:	
		G.sipo->flag ^= SIPO_LOCK_VIEW;
		break;
	case 13: /* Set Preview Range */
		anim_previewrange_set();
		break;
	case 14: /* Clear Preview Range */
		anim_previewrange_clear();
		break;
	case 15: /* AutoMerge Keyframes */
		G.sipo->flag ^= SIPO_NOTRANSKEYCULL;
		break;
	}
}

static uiBlock *ipo_viewmenu(void *arg_unused)
{
	uiBlock *block;
	EditIpo *ei;
	short yco= 0, menuwidth=120;

	ei = get_active_editipo();

	block= uiNewBlock(&curarea->uiblocks, "ipo_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_viewmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Channel Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 8, "");
	
	if (G.sipo->showkey)
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Show Keys|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Show Keys|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	uiDefIconTextBut(block, BUTM, 1, (G.sipo->flag & SIPO_NOTRANSKEYCULL)?ICON_CHECKBOX_DEHLT:ICON_CHECKBOX_HLT, 
					 "AutoMerge Keyframes|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 15, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Animation|Alt A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 6, "");
	//uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Play Animation in 3D View|Alt Shift A", 0, yco-=20,
	//				 menuwidth, 19, NULL, 0.0, 0.0, 1, 7, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Set Preview Range|Ctrl P", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 13, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Clear Preview Range|Alt P", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 14, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");


	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Center on Current Frame|Shift C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 10, "");
	uiDefIconTextBut(block, BUTM, 1, (G.v2d->flag & V2D_VIEWLOCK)?ICON_CHECKBOX_HLT:ICON_CHECKBOX_DEHLT, 
					 "Lock Time to Other Windows|", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 9, "");
	
	if (G.sipo->flag & SIPO_LOCK_VIEW)
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Lock View Area", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Lock View Area", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 12, "");

	if (ei != NULL && (ei->flag & IPO_EDIT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move Current Frame to Selected|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	}

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View Selected|NumPad .",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 11, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	if(!curarea->full) uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0,20, "");
	else uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 20, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

static void do_ipo_selectmenu(void *arg, int event)
{
	switch(event)
	{
	case 0:
		borderselect_ipo();
		break;
	case 1:
		swap_selectall_editipo();
		break;
	case 2:
		borderselect_markers();
		allqueue(REDRAWMARKER, 0);
		break;
	case 3:
		deselect_markers(1, 0);
		allqueue(REDRAWMARKER, 0);
		break;
	}
}

static uiBlock *ipo_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_selectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select Markers|Ctrl B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All Markers|Ctrl A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

static void do_ipo_markermenu(void *arg, int event)
{	
	switch(event)
	{
		case 1:
			add_marker(CFRA);
			break;
		case 2:
			duplicate_marker();
			break;
		case 3:
			remove_marker();
			break;
		case 4:
			rename_marker();
			break;
		case 5:
			transform_markers('g', 0);
			break;
	}
	
	allqueue(REDRAWMARKER, 0);
}

static uiBlock *ipo_markermenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_markermenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_markermenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Add Marker|M", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate Marker|Ctrl Shift D", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete Marker|X", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
					 
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "(Re)Name Marker|Ctrl M", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 4, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Grab/Move Marker|Ctrl G", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 5, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	uiTextBoundsBlock(block, 50);

	return block;
}

static char *ipo_modeselect_pup(void)
{
	Object *ob= OBACT;
	static char formatstring[] = "|%s %%x%d %%i%d";
	static char string[1024];
	char *str = string;
 	
	str += sprintf(str, "Ipo type: %%t");

	if(ob)
		str += sprintf(str,formatstring, "Object",ID_OB, ICON_OBJECT);

	if(ob && give_current_material(ob, ob->actcol)) // check for material
		str += sprintf(str,formatstring, "Material",ID_MA, ICON_MATERIAL);

	if(G.scene->world)
		str += sprintf(str,formatstring, "World",ID_WO, ICON_WORLD);

	if(ob && ob->type==OB_CURVE)
		str += sprintf(str,formatstring, "Path",ID_CU, ICON_CURVE);

	if(ob && ob->type==OB_CAMERA)
		str += sprintf(str,formatstring, "Camera",ID_CA, ICON_CAMERA);
	
	if(ob && ob->type==OB_LAMP)
		str += sprintf(str,formatstring, "Lamp",ID_LA, ICON_LAMP);

	if((ob && give_current_texture(ob, ob->actcol))||(give_current_world_texture()))
		str += sprintf(str,formatstring, "Texture",ID_TE, ICON_TEXTURE);

	if(ob){
		FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(ob, eModifierType_Fluidsim);
		
		if ELEM4(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE)
			str += sprintf(str,formatstring, "Shape",ID_KE, ICON_EDIT);
		if (ob->type==OB_ARMATURE)
			str += sprintf(str,formatstring, "Pose",ID_PO, ICON_POSE_HLT);
#ifdef __CON_IPO
		str += sprintf(str,formatstring, "Constraint",ID_CO, ICON_CONSTRAINT);
#endif
		if(fluidmd) {
			str += sprintf(str,formatstring,"Fluidsim",ID_FLUIDSIM, ICON_WORLD);
		}

		if(ob->particlesystem.first) {
			str += sprintf(str,formatstring,"Particles",ID_PA, ICON_PARTICLES);
		}
	}

	str += sprintf(str,formatstring, "Sequence",ID_SEQ, ICON_SEQUENCE);

	return (string);
}

void do_ipo_buttons(short event)
{
	EditIpo *ei;
	View2D *v2d;
	rcti rect;
	Object *ob= OBACT;
	float xmin, ymin, dx, dy;
	int a, val, first;
	short mval[2];

	if(curarea->win==0) return;

	switch(event) {
	case B_IPOVIEWCENTER:
	case B_IPOHOME:

		/* boundbox */

		v2d= &(G.sipo->v2d);
		first= 1;

		ei= G.sipo->editipo;
		if(ei==0) return;
		
		/* map ipo-points for drawing if scaled ipo */
		if (OBACT && OBACT->action && G.sipo->pin==0 && G.sipo->actname) {
			actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 0, 0);
		}
		
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			
				boundbox_ipocurve(ei->icu, (event==B_IPOVIEWCENTER));
				
				if(first) {
					v2d->tot= ei->icu->totrct;
					first= 0;
				}
				else BLI_union_rctf(&(v2d->tot), &(ei->icu->totrct));
			}
		}
		
		/* undo mapping of ipo-points for drawing if scaled ipo */
		if (OBACT && OBACT->action && G.sipo->pin==0 && G.sipo->actname) {
			actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 1, 0);
		}

		/* speciale home */
		if(G.qual & LR_SHIFTKEY) {
			v2d->tot.xmin= SFRA;
			v2d->tot.xmax= EFRA;
		}

		/* zoom out a bit */
		dx= 0.10*(v2d->tot.xmax-v2d->tot.xmin);
		dy= 0.10*(v2d->tot.ymax-v2d->tot.ymin);
		
		if(dx<v2d->min[0]) dx= v2d->min[0];
		if(dy<v2d->min[1]) dy= v2d->min[1];
		
		v2d->cur.xmin= v2d->tot.xmin- dx;
		v2d->cur.xmax= v2d->tot.xmax+ dx;
		v2d->cur.ymin= v2d->tot.ymin- dy;
		v2d->cur.ymax= v2d->tot.ymax+ dy;

		test_view2d(G.v2d, curarea->winx, curarea->winy);
		view2d_do_locks(curarea, V2D_LOCK_COPY);
		if(G.sipo->ipo) G.sipo->ipo->cur = G.v2d->cur;
		
		scrarea_queue_winredraw(curarea);
		break;
	case B_IPOBORDER:
		val= get_border(&rect, 3);
		if(val) {
			mval[0]= rect.xmin;
			mval[1]= rect.ymin;
			areamouseco_to_ipoco(G.v2d, mval, &xmin, &ymin);
			mval[0]= rect.xmax;
			mval[1]= rect.ymax;
			areamouseco_to_ipoco(G.v2d, mval, &(G.v2d->cur.xmax), &(G.v2d->cur.ymax));
			G.v2d->cur.xmin= xmin;
			G.v2d->cur.ymin= ymin;
			
			test_view2d(G.v2d, curarea->winx, curarea->winy);
			view2d_do_locks(curarea, V2D_LOCK_COPY);
			scrarea_queue_winredraw(curarea);
		}
		break;

	case B_IPOPIN:
		allqueue (REDRAWIPO, 0);
		break;

	case B_IPOCOPY:
		copy_editipo();
		break;
	case B_IPOPASTE:
		paste_editipo();
		break;
	case B_IPOCONT:
		set_exprap_ipo(IPO_HORIZ);
		break;
	case B_IPOEXTRAP:
		set_exprap_ipo(IPO_DIR);
		break;
	case B_IPOCYCLIC:
		set_exprap_ipo(IPO_CYCL);
		break;
	case B_IPOCYCLICX:
		set_exprap_ipo(IPO_CYCLX);
		break;
	case B_IPOMAIN:
		/* pass 1 to enforce a refresh when there's no Ipo */
		test_editipo(1);
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);
		if(ob) ob->ipowin= G.sipo->blocktype;
		break;
	case B_IPOSHOWKEY:
		/* reverse value because of winqread */
		G.sipo->showkey= 1-G.sipo->showkey;
		ipo_toggle_showkey();
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_VIEW2DZOOM:
		view2dzoom(event);
		scrarea_queue_headredraw(curarea);
		break;
	case B_IPO_ACTION_OB:
		if(ob && G.sipo->from && G.sipo->pin==0) {
			if(ob->ipoflag & OB_ACTION_OB) {	/* check if channel exists, and flip ipo link */
				bActionChannel *achan;
				
				if(has_ipo_code(ob->ipo, OB_LAY))
					notice("Note: Layer Ipo doesn't work in Actions");
				
				if(ob->action==NULL) 
					ob->action= add_empty_action("ObAction");
				achan= verify_action_channel(ob->action, "Object");
				if(achan->ipo==NULL && ob->ipo) {
					achan->ipo= ob->ipo;
					ob->ipo= NULL;
				}
				
				/* object constraints */
				if(ob->constraintChannels.first) {
					free_constraint_channels(&achan->constraintChannels);
					achan->constraintChannels= ob->constraintChannels;
					ob->constraintChannels.first= ob->constraintChannels.last= NULL;
				}
			}
			else if(ob->action) {
				bActionChannel *achan= get_action_channel(ob->action, "Object");
				if(achan) {
					
					if(achan->ipo && ob->ipo==NULL) {
						ob->ipo= achan->ipo;
						achan->ipo= NULL;
					}
					
					/* object constraints */
					if(achan->constraintChannels.first) {
						free_constraint_channels(&ob->constraintChannels);
						ob->constraintChannels= achan->constraintChannels;
						achan->constraintChannels.first= achan->constraintChannels.last= NULL;
					}
				}
			}
			allqueue(REDRAWVIEW3D, 0);
			allqueue(REDRAWIPO, 0);
			allqueue(REDRAWACTION, 0);
			allqueue(REDRAWOOPS, 0);
			allqueue(REDRAWNLA, 0);
		}
		break;
		
	case B_IPO_ACTION_KEY:
		if(ob && G.sipo->from && G.sipo->pin==0) {
			Key *key= ob_get_key(ob);
			if(key) {
				if(ob->ipoflag & OB_ACTION_KEY) {	/* check if channel exists, and flip ipo link */
					bActionChannel *achan;
					
					if(ob->action==NULL) 
						ob->action= add_empty_action("ShapeAction");
					achan= verify_action_channel(ob->action, "Shape");
					if(achan->ipo==NULL && key->ipo) {
						achan->ipo= key->ipo;
						key->ipo= NULL;
					}
				}
				else if(ob->action) {
					bActionChannel *achan= get_action_channel(ob->action, "Shape");
					if(achan) {
						if(achan->ipo && key->ipo==NULL) {
							key->ipo= achan->ipo;
							achan->ipo= NULL;
						}
					}
				}
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWIPO, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWOOPS, 0);
				allqueue(REDRAWNLA, 0);
			}
		}
		break;
	case B_IPOVIEWALL:
		/* set visible active */
		for(a=0, ei=G.sipo->editipo; a<G.sipo->totipo; a++, ei++) {
			if (ei->icu)	ei->flag |= IPO_VISIBLE;
			else			ei->flag &= ~IPO_VISIBLE;
		}
		break;
	case B_IPOREDRAW:
		DAG_object_flush_update(G.scene, ob, OB_RECALC);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWIPO, 0);
		break;
	}
}

void ipo_buttons(void)
{
	Object *ob;
	EditIpo *ei;
	uiBlock *block;
	short xco,xmax;
	char naam[20];
	int icon=0, allow_pin= B_IPOPIN;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_IPO;

	xco = 8;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	xco+= XIC+14;
	
	test_editipo(0);	/* test if current editipo is OK, make_editipo sets v2d->cur */

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Enables display of pulldown menus");
	} else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hides pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	/* pull down menus */
	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		ei = get_active_editipo();
	
		xmax= GetButStringLength("View");
		uiDefPulldownBut(block,ipo_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
		xco+=xmax;
	
		xmax= GetButStringLength("Select");
		uiDefPulldownBut(block,ipo_selectmenu, NULL, "Select", xco, -2, xmax-3, 24, "");
		xco+=xmax;
		
		xmax= GetButStringLength("Marker");
		uiDefPulldownBut(block,ipo_markermenu, NULL, "Marker", xco, -2, xmax-3, 24, "");
		xco+=xmax;
		
		if (G.sipo->showkey) {
			xmax= GetButStringLength("Key");
			uiDefPulldownBut(block,ipo_editmenu, NULL, "Key", xco, -2, xmax-3, 24, "");
		}
		else if(ei != NULL && (ei->flag & IPO_EDIT)) {
			xmax= GetButStringLength("Point");
			uiDefPulldownBut(block,ipo_editmenu, NULL, "Point", xco, -2, xmax-3, 24, "");
		}
		else {
			xmax= GetButStringLength("Curve");
			uiDefPulldownBut(block,ipo_editmenu, NULL, "Curve", xco, -2, xmax-3, 24, "");
		}
		xco+=xmax;
	}

	/* end of pull down menus */
	uiBlockSetEmboss(block, UI_EMBOSS);

	ob= OBACT;
	
	/* action switch option, only when active object is there and no pin */
	uiSetButLock(G.sipo->pin, "Can't change because of pinned data");
	
	/* define whether ipos are on Object or on action */
	if(ob) {
		static short fake1= 1;
		
		uiBlockBeginAlign(block);
		
		if(G.sipo->blocktype==ID_OB) {
			uiDefIconButBitS(block, TOG, OB_ACTION_OB, B_IPO_ACTION_OB, ICON_ACTION,	xco,0,XIC,YIC, &(ob->ipoflag), 0, 0, 0, 0, "Sets Ipo to be included in an Action or not");
			xco+= XIC;
		}
		else if(G.sipo->blocktype==ID_KE) {
			uiDefIconButBitS(block, TOG, OB_ACTION_KEY, B_IPO_ACTION_KEY, ICON_ACTION,	xco,0,XIC,YIC, &(ob->ipoflag), 0, 0, 0, 0, "Sets Ipo to be included in an Action or not");
			xco+= XIC;
		}
		else if(G.sipo->blocktype==ID_CO) {
			
			if(ob->pose==NULL)
				uiDefIconButBitS(block, TOG, OB_ACTION_OB, B_IPO_ACTION_OB, ICON_ACTION,	xco,0,XIC,YIC, &(ob->ipoflag), 0, 0, 0, 0, "Sets Ipo to be included in an Action or not");
			else {
				bConstraint *con= get_active_constraint(ob);
				if(con)
					uiDefIconButBitS(block, TOGN, CONSTRAINT_OWN_IPO, B_IPOREDRAW, ICON_ACTION,	xco,0,XIC,YIC, &con->flag, 0, 0, 0, 0, 
									 (con->flag & CONSTRAINT_OWN_IPO)?"Ipo is connected to Constraint itself":"Ipo is connected to Pose Action"
									 );
			}
			xco+= XIC;
		}
		else if(G.sipo->blocktype==ID_PO) {	/* only to indicate we have action ipos */
			uiSetButLock(1, "Pose Action Ipo cannot be switched");
			uiDefIconButS(block, TOG, 1, ICON_ACTION,	xco,0,XIC,YIC, &fake1, 0, 0, 0, 0, "Ipo is connected to Pose Action");
			xco+= XIC;
		}
		uiClearButLock();
	}
	
	/* ipo muting */
	if (G.sipo->ipo) {
		uiDefIconButS(block, ICONTOG, 1, ICON_MUTE_IPO_OFF, xco,0,XIC,YIC, &(G.sipo->ipo->muteipo), 0, 0, 0, 0, "Mute IPO-block");
		xco += XIC;
	}
	
	/* mainmenu, only when data is there and no pin */
	uiSetButLock(G.sipo->pin, "Can't change because of pinned data");

	if (G.sipo->blocktype == ID_OB)
		icon = ICON_OBJECT;
	else if (G.sipo->blocktype == ID_MA)
		icon = ICON_MATERIAL;
	else if (G.sipo->blocktype == ID_WO)
		icon = ICON_WORLD;
	else if (G.sipo->blocktype == ID_CU)
		icon = ICON_ANIM;
	else if (G.sipo->blocktype == ID_CA)
		icon = ICON_CAMERA;
	else if (G.sipo->blocktype == ID_LA)
		icon = ICON_LAMP;
	else if (G.sipo->blocktype == ID_KE)
		icon = ICON_EDIT;
	else if (G.sipo->blocktype == ID_PO)
		icon = ICON_POSE_HLT;
	else if (G.sipo->blocktype == ID_CO)
		icon = ICON_CONSTRAINT;
	else if (G.sipo->blocktype == ID_SEQ)
		icon = ICON_SEQUENCE;
	else if(G.sipo->blocktype == ID_TE)
		icon = ICON_TEXTURE;
	else if(G.sipo->blocktype == ID_FLUIDSIM)
		icon = ICON_WORLD;
	else if(G.sipo->blocktype == ID_PA)
		icon = ICON_PARTICLES;

	uiDefIconTextButS(block, MENU, B_IPOMAIN, icon, ipo_modeselect_pup(), xco,0,100,20, &(G.sipo->blocktype), 0, 0, 0, 0, "Show IPO type");

	xco += 85;

	if(G.sipo->blocktype==ID_MA) {
		uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, MAX_MTEX-1.0, 0, 0, "Channel Number of the active Material texture.");
		xco-= 4;
	}
	if(G.sipo->blocktype==ID_WO) {
		uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, MAX_MTEX-1.0, 0, 0, "Channel Number of the active World texture.");
		xco-= 4;
	}
	
	if(G.sipo->blocktype==ID_LA) {
		uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, MAX_MTEX-1.0, 0, 0, "Channel Number of the active Lamp texture. ");
		xco-= 4;
	}
	
	uiBlockEndAlign(block);
	
	uiClearButLock();

	/* if(G.sipo->blocktype==ID_SEQ)
	   allow_pin= 0; */
	xco= std_libbuttons(block, (short)(xco+1.5*XIC), 0, allow_pin, &G.sipo->pin, B_IPOBROWSE, ID_IP, 
						G.sipo->blocktype, (ID*)G.sipo->ipo, G.sipo->from, &(G.sipo->menunr), B_IPOALONE, B_IPOLOCAL, B_IPODELETE, 0, B_KEEPDATA);

	/* COPY PASTE */
	xco-= XIC/2;
	uiBlockBeginAlign(block);
	if(curarea->headertype==HEADERTOP) {
		uiDefIconBut(block, BUT, B_IPOCOPY, ICON_COPYUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected curves to the buffer");
		uiSetButLock(G.sipo->ipo && G.sipo->ipo->id.lib, ERROR_LIBDATA_MESSAGE);
		uiDefIconBut(block, BUT, B_IPOPASTE, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the curves from the buffer");
	}
	else {
		uiDefIconBut(block, BUT, B_IPOCOPY, ICON_COPYDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected curves to the buffer");
		uiSetButLock(G.sipo->ipo && G.sipo->ipo->id.lib, ERROR_LIBDATA_MESSAGE);
		uiDefIconBut(block, BUT, B_IPOPASTE, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the curves from the buffer");
	}
	uiBlockEndAlign(block);
	xco+=XIC/2;
	
	uiClearButLock();

	/* ZOOMBORDER */
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area (Shift B)");
	
	xco+=XIC/2;
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.sipo->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");
	
	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
