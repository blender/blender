/**
 * header_ipo.c oct-2003
 *
 * Functions to draw the "Ipo Curve Editor" window header
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
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_interface.h"
#include "BIF_mainqueue.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"
#include "BLI_blenlib.h"
#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_headerbuttons.h"

#include "ipo.h"
#include "nla.h"

#include "blendef.h"
#include "mydevice.h"

static int viewmovetemp = 0;
extern int totipo_edit, totipo_sel;


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
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "All Selected|J, 1",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Selected Doubles|J, 2",			0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
	return block;
}

static void do_ipo_editmenu_keymenu(void *arg, int event)
{
	Key *key;
	KeyBlock *kb;

	if(G.sipo->blocktype==ID_KE && totipo_edit==0 && totipo_sel==0) {
		key= (Key *)G.sipo->from;
		if(key==0) return;
		kb= key->block.first;

		while(kb) {
			if(kb->flag & SELECT) {
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
			kb= kb->next;
		}
	}
}

static uiBlock *ipo_editmenu_keymenu(void *arg_unused)
{
	uiBlock *block;
	EditIpo *ei;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_keymenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_keymenu, NULL);

	ei = get_editipo();

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linear", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cardinal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "BSpline", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

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
	}
}

static uiBlock *ipo_editmenu_handlemenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu_handlemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu_handlemenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Auto|Shift H", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
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

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Constant", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Linear", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Bezier", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

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

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Constant", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Extrapolation", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cyclic", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Cyclic Extrapolation", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");

	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);

	return block;
}


static void do_ipo_editmenu(void *arg, int event)
{
	switch(event)
	{
	case 0:
		del_ipo();
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
	}
}

static uiBlock *ipo_editmenu(void *arg_unused)
{
	uiBlock *block;
	EditIpo *ei;
	short yco= 0, menuwidth=120;

	get_status_editipo();

	ei = G.sipo->editipo;

	block= uiNewBlock(&curarea->uiblocks, "ipo_editmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_editmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_MENU_PANEL, "Transform Properties|N", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	uiDefIconTextBlockBut(block, ipo_editmenu_snapmenu, NULL, ICON_RIGHTARROW_THIN, "Snap", 0, yco-=20, 120, 19, "");	
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Insert Keyframe...|I", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Duplicate|Shift D", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Record Mouse Movement|R", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Delete|X", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBlockBut(block, ipo_editmenu_joinmenu, NULL, ICON_RIGHTARROW_THIN, "Join", 0, yco-=20, 120, 19, "");	

	if (!G.sipo->showkey){
		uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
		
		uiDefIconTextBlockBut(block, ipo_editmenu_extendmenu, NULL, ICON_RIGHTARROW_THIN, "Extend Mode", 0, yco-=20, 120, 19, "");	
		uiDefIconTextBlockBut(block, ipo_editmenu_intpolmenu, NULL, ICON_RIGHTARROW_THIN, "Interpolation Mode", 0, yco-=20, 120, 20, "");
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
	}
}

static uiBlock *ipo_viewmenu(void *arg_unused)
{
	uiBlock *block;
	EditIpo *ei;
	short yco= 0, menuwidth=120;

	ei = get_editipo();

	block= uiNewBlock(&curarea->uiblocks, "ipo_viewmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_viewmenu, NULL);

	if (G.sipo->showkey)
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Show Keys|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");
	else
		uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Show Keys|K", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 2, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 5, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

	if (ei != NULL && (ei->flag & IPO_EDIT)) {
		uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Move Current Frame to Selected|C", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 3, "");
	}

	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");

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
	}
}

static uiBlock *ipo_selectmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;

	block= uiNewBlock(&curarea->uiblocks, "ipo_selectmenu", UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_ipo_selectmenu, NULL);

	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Border Select|B", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 0, "");
	uiDefBut(block, SEPR, 0, "",        0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Select/Deselect All|A", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 1, "");

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
	static char string[1024];
	char tmpstr[1024];
	char formatstring[1024];

	strcpy(string, "Ipo type: %t");
	
	strcpy(formatstring, "|%s %%x%d %%i%d");

	if(OBACT) {
		sprintf(tmpstr,formatstring,"Object",ID_OB, ICON_OBJECT);
		strcat(string,tmpstr);
	}

	if(OBACT && give_current_material(OBACT, OBACT->actcol)) { // check for material
		sprintf(tmpstr,formatstring,"Material",ID_MA, ICON_MATERIAL);
		strcat(string,tmpstr);
	}

	if(G.scene->world) {
		sprintf(tmpstr,formatstring,"World",ID_WO, ICON_WORLD);
		strcat(string,tmpstr);
	}

	if(OBACT && OBACT->type==OB_CURVE) {
		sprintf(tmpstr,formatstring,"Path",ID_CU, ICON_CURVE);
		strcat(string,tmpstr);
	}

	if(OBACT && OBACT->type==OB_CAMERA) {
		sprintf(tmpstr,formatstring,"Camera",ID_CA, ICON_CAMERA);
		strcat(string,tmpstr);
	}
	
	if(OBACT && OBACT->type==OB_LAMP) {
		sprintf(tmpstr,formatstring,"Lamp",ID_LA, ICON_LAMP);
		strcat(string,tmpstr);
	}

	if(OBACT){
		if ELEM4(OBACT->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE) {
			sprintf(tmpstr,formatstring,"Vertex",ID_KE, ICON_EDIT);
			strcat(string,tmpstr);
		}
		if (OBACT->action){
			sprintf(tmpstr,formatstring,"Action",ID_AC, ICON_ACTION);
			strcat(string,tmpstr);
		}
#ifdef __CON_IPO
		sprintf(tmpstr,formatstring,"Constraint",IPO_CO, ICON_CONSTRAINT);
		strcat(string,tmpstr);
#endif
	}

	sprintf(tmpstr,formatstring,"Sequence",ID_SEQ, ICON_SEQUENCE);
	strcat(string,tmpstr);


	return (string);
}

void do_ipo_buttons(short event)
{
	EditIpo *ei;
	View2D *v2d;
	rcti rect;
	float xmin, ymin, dx, dy;
	int a, val, first;
	short mval[2];

	if(curarea->win==0) return;

	switch(event) {
	case B_IPOHOME:

		/* boundbox */

		v2d= &(G.sipo->v2d);
		first= 1;

		ei= G.sipo->editipo;
		if(ei==0) return;
		for(a=0; a<G.sipo->totipo; a++, ei++) {
			if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			
				boundbox_ipocurve(ei->icu);
				
				if(first) {
					v2d->tot= ei->icu->totrct;
					first= 0;
				}
				else BLI_union_rctf(&(v2d->tot), &(ei->icu->totrct));
			}
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
		scrarea_queue_winredraw(curarea);
		break;
	case B_IPOBORDER:
		val= get_border(&rect, 2);
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
		make_editipo();
		scrarea_queue_winredraw(curarea);
		scrarea_queue_headredraw(curarea);

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
		viewmovetemp= 0;
		view2dzoom(event);
		scrarea_queue_headredraw(curarea);
		break;
			
	} 
}

void ipo_buttons(void)
{
	Object *ob;
	EditIpo *ei;
	ID *id, *from;
	uiBlock *block;
	short xco,xmax;
	char naam[20];
	int icon=0;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_IPO;

	xco = 8;
	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");
	xco+= XIC+14;
	
	test_editipo();	/* test if current editipo is OK, make_editipo sets v2d->cur */

	uiBlockSetEmboss(block, UI_EMBOSSN);
	if(curarea->flag & HEADER_NO_PULLDOWN) {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_RIGHT,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Enables display of pulldown menus");
	} else {
		uiDefIconButS(block, TOG|BIT|0, B_FLIPINFOMENU, ICON_DISCLOSURE_TRI_DOWN,
				xco,2,XIC,YIC-2,
				&(curarea->flag), 0, 0, 0, 0, "Hides pulldown menus");
	}
	uiBlockSetEmboss(block, UI_EMBOSS);
	xco+=XIC;

	/* pull down menus */
	if((curarea->flag & HEADER_NO_PULLDOWN)==0) {
		uiBlockSetEmboss(block, UI_EMBOSSP);
	
		ei = get_editipo();
	
		xmax= GetButStringLength("View");
		uiDefBlockBut(block,ipo_viewmenu, NULL, "View", xco, -2, xmax-3, 24, "");
		xco+=xmax;
	
		xmax= GetButStringLength("Select");
		uiDefBlockBut(block,ipo_selectmenu, NULL, "Select", xco, -2, xmax-3, 24, "");
		xco+=xmax;
	
		if (G.sipo->showkey) {
			xmax= GetButStringLength("Key");
			uiDefBlockBut(block,ipo_editmenu, NULL, "Key", xco, -2, xmax-3, 24, "");
		}
		else if(ei != NULL && (ei->flag & IPO_EDIT)) {
			xmax= GetButStringLength("Point");
			uiDefBlockBut(block,ipo_editmenu, NULL, "Point", xco, -2, xmax-3, 24, "");
		}
		else {
			xmax= GetButStringLength("Curve");
			uiDefBlockBut(block,ipo_editmenu, NULL, "Curve", xco, -2, xmax-3, 24, "");
		}
			
		xco+=xmax;
	}

	/* end of pull down menus */
	uiBlockSetEmboss(block, UI_EMBOSSX);

	/* mainmenu, only when data is there and no pin */
	uiSetButLock(G.sipo->pin, "Can't change because of pinned data");
	
	ob= OBACT;

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
	else if (G.sipo->blocktype == ID_AC)
		icon = ICON_ACTION;
	else if (G.sipo->blocktype == IPO_CO)
		icon = ICON_CONSTRAINT;
	else if (G.sipo->blocktype == ID_SEQ)
		icon = ICON_SEQUENCE;

	uiDefIconTextButS(block, MENU, B_IPOMAIN, icon, ipo_modeselect_pup(), xco,0,100,20, &(G.sipo->blocktype), 0, 0, 0, 0, "Display IPO type");

	xco += 85;

	if(G.sipo->blocktype==ID_MA) {
		uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active Material texture. Click to change.");
		xco-= 4;
	}
	if(G.sipo->blocktype==ID_WO) {
		uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active World texture. Click to change.");
		xco-= 4;
	}
	
	if(G.sipo->blocktype==ID_LA) {
		uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active Lamp texture. Click to change.");
		xco-= 4;
	}
	
	uiClearButLock();

	/* NAME ETC */
	id= (ID *)get_ipo_to_edit(&from);

	xco= std_libbuttons(block, (short)(xco+1.5*XIC), 0, B_IPOPIN, &G.sipo->pin, B_IPOBROWSE, (ID*)G.sipo->ipo, from, &(G.sipo->menunr), B_IPOALONE, B_IPOLOCAL, B_IPODELETE, 0, B_KEEPDATA);

	uiSetButLock(id && id->lib, "Can't edit library data");

	/* COPY PASTE */
	xco-= XIC/2;
	if(curarea->headertype==HEADERTOP) {
		uiDefIconBut(block, BUT, B_IPOCOPY, ICON_COPYUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected curves to the buffer");
		uiSetButLock(id && id->lib, "Can't edit library data");
		uiDefIconBut(block, BUT, B_IPOPASTE, ICON_PASTEUP,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the curves from the buffer");
	}
	else {
		uiDefIconBut(block, BUT, B_IPOCOPY, ICON_COPYDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Copies the selected curves to the buffer");
		uiSetButLock(id && id->lib, "Can't edit library data");
		uiDefIconBut(block, BUT, B_IPOPASTE, ICON_PASTEDOWN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Pastes the curves from the buffer");
	}
	xco+=XIC/2;
	
	uiClearButLock();
	/* ZOOMBORDER */
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.sipo->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");

	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
