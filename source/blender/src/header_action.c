/**
 * header_action.c oct-2003
 *
 * Functions to draw the "Action Editor" window header
 * and handle user events sent to it.
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA	02111-1307, USA.
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

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "BMF_Api.h"
#include "BIF_language.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "DNA_ID.h"
#include "DNA_action_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_key_types.h"
#include "DNA_curve_types.h"

#include "BIF_interface.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editaction.h"

#include "BKE_action.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BSE_drawipo.h"
#include "BSE_editaction.h"
#include "BSE_headerbuttons.h"

#include "nla.h"

#include "blendef.h"
#include "mydevice.h"

#define ACTMENU_VIEW_CENTERVIEW 0
#define ACTMENU_VIEW_AUTOUPDATE 1
#define ACTMENU_VIEW_PLAY3D     2
#define ACTMENU_VIEW_PLAYALL    3
#define ACTMENU_VIEW_ALL        4
#define ACTMENU_VIEW_MAXIMIZE   5

#define ACTMENU_SEL_BORDER      0
#define ACTMENU_SEL_ALL_KEYS    1
#define ACTMENU_SEL_ALL_CHAN    2

#define ACTMENU_KEY_DUPLICATE     0
#define ACTMENU_KEY_DELETE        1
#define ACTMENU_KEY_BAKE          2

#define ACTMENU_KEY_HANDLE_AUTO   0
#define ACTMENU_KEY_HANDLE_ALIGN  1
#define ACTMENU_KEY_HANDLE_FREE   2
#define ACTMENU_KEY_HANDLE_VECTOR 3

#define ACTMENU_KEY_INTERP_CONST  0
#define ACTMENU_KEY_INTERP_LINEAR 1
#define ACTMENU_KEY_INTERP_BEZIER 2

void do_action_buttons(unsigned short event)
{

	switch(event){
		case B_ACTBAKE:
			bake_action_with_client (G.saction->action, OBACT, 0.01);
			break;
		case B_ACTCONT:
			set_exprap_action(IPO_HORIZ);
			break;
//	case B_ACTEXTRAP:
//		set_exprap_ipo(IPO_DIR);
//		break;
		case B_ACTCYCLIC:
			set_exprap_action(IPO_CYCL);
			break;
//	case B_ACTCYCLICX:
//		set_exprap_ipo(IPO_CYCLX);
//		break;
		case B_ACTHOME:
			//	Find X extents
			//v2d= &(G.saction->v2d);

			G.v2d->cur.xmin = 0;
			G.v2d->cur.ymin=-SCROLLB;

			if (!G.saction->action){	// here the mesh rvk?
				G.v2d->cur.xmax=100;
			}
			else {
				float extra;
				G.v2d->cur.xmin= calc_action_start(G.saction->action);
				G.v2d->cur.xmax= calc_action_end(G.saction->action);
				extra= 0.05*(G.v2d->cur.xmax - G.v2d->cur.xmin);
				G.v2d->cur.xmin-= extra;
				G.v2d->cur.xmax+= extra;
			}

			G.v2d->tot= G.v2d->cur;
			test_view2d(G.v2d, curarea->winx, curarea->winy);


			addqueue (curarea->win, REDRAW, 1);

			break;
		case B_ACTCOPY:
			copy_posebuf();
			allqueue(REDRAWVIEW3D, 1);
			break;
		case B_ACTPASTE:
			paste_posebuf(0);
			clear_object_constraint_status(OBACT);
			make_displists_by_armature(OBACT);
			allqueue(REDRAWVIEW3D, 1);
			break;
		case B_ACTPASTEFLIP:
			paste_posebuf(1);
			clear_object_constraint_status(OBACT);
			make_displists_by_armature(OBACT);
			allqueue(REDRAWVIEW3D, 1);
			break;

		case B_ACTPIN:	/* __PINFAKE */
/*		if (G.saction->flag & SACTION_PIN){
		if (G.saction->action)
		G.saction->action->id.us ++;
		
		}
		else {
			if (G.saction->action)
				G.saction->action->id.us --;
				}
*/		/* end PINFAKE */
			allqueue(REDRAWACTION, 1);
			break;

	}
}

static void do_action_viewmenu(void *arg, int event)
{
	extern int play_anim(int mode);

	switch(event) {
		case ACTMENU_VIEW_CENTERVIEW: /* Center View to Current Frame */
			center_currframe();
			break;
		case ACTMENU_VIEW_AUTOUPDATE: /* Update Automatically */
			if (BTST(G.saction->lock, 0)) 
				G.saction->lock = BCLR(G.saction->lock, 0);
			else 
				G.saction->lock = BSET(G.saction->lock, 0);
			break;
		case ACTMENU_VIEW_PLAY3D: /* Play Back Animation */
			play_anim(0);
			break;
		case ACTMENU_VIEW_PLAYALL: /* Play Back Animation in All */
			play_anim(1);
			break;	
		case ACTMENU_VIEW_ALL: /* View All */
			do_action_buttons(B_SIMAGEHOME);
			break;
		case ACTMENU_VIEW_MAXIMIZE: /* Maximize Window */
			/* using event B_FULL */
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *action_viewmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "action_viewmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_viewmenu, NULL);
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Center View to Current Frame|C", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_CENTERVIEW, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	if(BTST(G.saction->lock, 0)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, 
						 "Update Automatically|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_AUTOUPDATE, "");
	} 
	else {
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, 
						 "Update Automatically|", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 1, 
						 ACTMENU_VIEW_AUTOUPDATE, "");
	}

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Play Back Animation|Alt A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PLAY3D, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Play Back Animation in 3D View|Alt Shift A", 0, yco-=20,
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_PLAYALL, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "View All|Home", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 1, 
					 ACTMENU_VIEW_ALL, "");
		
	if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, 
						 "Maximize Window|Ctrl UpArrow", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_VIEW_MAXIMIZE, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, 
						 "Tile Window|Ctrl DownArrow", 0, yco-=20, 
						 menuwidth, 19, NULL, 0.0, 0.0, 0, 
						 ACTMENU_VIEW_MAXIMIZE, "");
	
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

static void do_action_selectmenu(void *arg, int event)
{	
	SpaceAction *saction;
	bAction	*act;
	Key *key;
	
	saction = curarea->spacedata.first;
	if (!saction)
		return;

	act = saction->action;
	key = get_action_mesh_key();
	
	switch(event)
	{
		case ACTMENU_SEL_BORDER: /* Border Select */
			if (key) {
				borderselect_mesh(key);
			}
			else {
				borderselect_action();
			}
			break;

		case ACTMENU_SEL_ALL_KEYS: /* Select/Deselect All Keys */
			if (key) {
				deselect_meshchannel_keys(key, 1);
				allqueue (REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
				allqueue (REDRAWIPO, 0);
			}
			else {
				deselect_actionchannel_keys (act, 1);
				allqueue (REDRAWACTION, 0);
				allqueue(REDRAWNLA, 0);
				allqueue (REDRAWIPO, 0);
			}
			break;

		case ACTMENU_SEL_ALL_CHAN: /* Select/Deselect All Channels */
			deselect_actionchannels (act, 1);
			allqueue (REDRAWVIEW3D, 0);
			allqueue (REDRAWACTION, 0);
			allqueue(REDRAWNLA, 0);
			allqueue (REDRAWIPO, 0);
			break;
	}
}

static uiBlock *action_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_selectmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Border Select|B", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_BORDER, "");
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Select/Deselect All Keys|A", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_ALL_KEYS, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Select/Deselect All Channels", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_SEL_ALL_CHAN, "");

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

static void do_action_keymenu_handlemenu(void *arg, int event)
{
	Key *key;
	
	key = get_action_mesh_key();

	switch (event){
		case ACTMENU_KEY_HANDLE_AUTO:
			if (key) {
				sethandles_meshchannel_keys(HD_AUTO, key);
			} else {
				sethandles_actionchannel_keys(HD_AUTO);
			}
			break;

		case ACTMENU_KEY_HANDLE_ALIGN:
		case ACTMENU_KEY_HANDLE_FREE:
			/* OK, this is kinda dumb, need to fix the
			 * toggle crap in sethandles_ipo_keys() 
			 */
			if (key) {
				sethandles_meshchannel_keys(HD_ALIGN, key);
			} else {
				sethandles_actionchannel_keys(HD_ALIGN);
			}
			break;

		case ACTMENU_KEY_HANDLE_VECTOR:
			if (key) {
				sethandles_meshchannel_keys(HD_VECT, key);
			} else {
				sethandles_actionchannel_keys(HD_VECT);
			}
			break;
	}
}

static uiBlock *action_keymenu_handlemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_handlemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_handlemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Auto|Shift H", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_AUTO, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Aligned|H", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_ALIGN, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Free|H", 0, yco-=20, menuwidth, 
					 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_FREE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Vector|V", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_HANDLE_VECTOR, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu_intpolmenu(void *arg, int event)
{
	Key *key;

	key = get_action_mesh_key();

	switch(event)
	{
		case ACTMENU_KEY_INTERP_CONST:
			if (key) {
				/* to do */
			}
			else {
				set_ipotype_actionchannels(SET_IPO_CONSTANT);
			}
			break;
		case ACTMENU_KEY_INTERP_LINEAR:
			if (key) {
				/* to do */
			}
			else {
				set_ipotype_actionchannels(SET_IPO_LINEAR);
			}
			break;
		case ACTMENU_KEY_INTERP_BEZIER:
			if (key) {
				/* to do */
			}
			else {
				set_ipotype_actionchannels(SET_IPO_BEZIER);
			}
			break;
	}

	scrarea_queue_winredraw(curarea);
}

static uiBlock *action_keymenu_intpolmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu_intpolmenu", 
					  UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_action_keymenu_intpolmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Constant", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_INTERP_CONST, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Linear", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_INTERP_LINEAR, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Bezier", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_INTERP_BEZIER, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}

static void do_action_keymenu(void *arg, int event)
{	
	SpaceAction *saction;
	bAction	*act;
	Key *key;
	
	saction= curarea->spacedata.first;
	if (!saction)
		return;


	act = saction->action;
	key = get_action_mesh_key();

	switch(event)
	{
		case ACTMENU_KEY_DUPLICATE:
			if (key) {
				duplicate_meshchannel_keys(key);
			}
			else {
				duplicate_actionchannel_keys();
				remake_action_ipos(act);
			}
 			break;

		case ACTMENU_KEY_DELETE:
			if (key) {
				delete_meshchannel_keys(key);
			}
			else {
				delete_actionchannel_keys ();
			}
			break;
		case ACTMENU_KEY_BAKE:
			bake_action_with_client (G.saction->action, OBACT, 0.01);
			break;

			break;
	}
}

static uiBlock *action_keymenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "action_keymenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_action_keymenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Duplicate|Shift D", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_DUPLICATE, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Delete|X", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_DELETE, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, 
					 "Bake Action to Ipo Keys", 0, yco-=20, 
					 menuwidth, 19, NULL, 0.0, 0.0, 0, 
					 ACTMENU_KEY_BAKE, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, 
			 menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBlockBut(block, action_keymenu_intpolmenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Interpolation Mode", 0, yco-=20, 120, 20, "");
	uiDefIconTextBlockBut(block, action_keymenu_handlemenu, 
						  NULL, ICON_RIGHTARROW_THIN, 
						  "Handle Type", 0, yco-=20, 120, 20, "");
	
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

void action_buttons(void)
{
	uiBlock *block;
	short xco, xmax;
	char naam[256];
	Object *ob;
	ID *from;

	if (!G.saction)
		return;

	// copy from drawactionspace....
	if (!G.saction->pin) {
		if (OBACT)
			G.saction->action = OBACT->action;
		else
			G.saction->action=NULL;
	}

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, 
					  UI_EMBOSS, UI_HELV, curarea->headwin);

	if (area_is_active_area(curarea)) 
		uiBlockSetCol(block, TH_HEADER);
	else 
		uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_ACTION;
	
	xco = 8;
	
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, 
					  windowtype_pup(), xco, 0, XIC+10, YIC, 
					  &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, 
					  "Displays Current Window Type. "
					  "Click for menu of available types.");

	xco += XIC + 14;

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if (curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  xco,2,XIC,YIC-2,
					  &(curarea->flag), 0, 0, 0, 0, 
					  "Show pulldown menus");
	}
	else {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  xco,2,XIC,YIC-2,
					  &(curarea->flag), 0, 0, 0, 0, 
					  "Hide pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		/* pull down menus */
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		xmax= GetButStringLength("View");
		uiDefBlockBut(block, action_viewmenu, NULL, 
					  "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;
		
		xmax= GetButStringLength("Select");
		uiDefBlockBut(block, action_selectmenu, NULL, 
					  "Select", xco, -2, xmax-3, 24, "");
		xco+= xmax;
	
		xmax= GetButStringLength("Key");
		uiDefBlockBut(block, action_keymenu, NULL, 
					  "Key", xco, -2, xmax-3, 24, "");
		xco+= xmax;
	}

	uiBlockSetEmboss(block, UI_EMBOSSX);
	
	if (!get_action_mesh_key()) {
		/* NAME ETC */
		ob=OBACT;
		from = (ID*) ob;

		xco= std_libbuttons(block, xco, 0, B_ACTPIN, &G.saction->pin, 
							B_ACTIONBROWSE, (ID*)G.saction->action, 
							from, &(G.saction->actnr), B_ACTALONE, 
							B_ACTLOCAL, B_ACTIONDELETE, 0, 0);	

		/* Draw action baker */
		xco+= 8;
		
		uiDefBut(block, BUT, B_ACTBAKE, 
				 "Bake", xco, 0, 64, YIC, 0, 0, 0, 0, 0, 
				 "Create an action with the constraint effects "
				 "converted into Ipo keys");
		xco+=64;

	}
	uiClearButLock();

	/* draw LOCK */
	xco+= 8;
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco, 0, XIC, YIC, 
				  &(G.saction->lock), 0, 0, 0, 0, 
				  "Updates other affected window spaces automatically "
				  "to reflect changes in real time");


	/* always as last  */
	curarea->headbutlen = xco + 2*XIC;

	uiDrawBlock(block);
}
