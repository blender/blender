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
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BIF_interface.h"
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
	ID *id, *from;
	uiBlock *block;
	short xco;
	char naam[20];

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_IPO;

	xco = 8;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), xco,0,XIC+10,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	xco+= XIC+22;
	
	test_editipo();	/* test if current editipo is OK, make_editipo sets v2d->cur */

	/* FULL WINDOW en HOME */

	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");
	uiDefIconBut(block, BUT, B_IPOHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
	uiDefIconButS(block, ICONTOG, B_IPOSHOWKEY, ICON_KEY_DEHLT,	xco+=XIC,0,XIC,YIC, &G.sipo->showkey, 0, 0, 0, 0, "Toggles between Curve and Key display (KKEY)");

	/* mainmenu, only when data is there and no pin */
	uiSetButLock(G.sipo->pin, "Can't change because of pinned data");
	
	ob= OBACT;
	xco+= XIC/2;
	uiDefIconButS(block, ROW, B_IPOMAIN, ICON_OBJECT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_OB, 0, 0, "Displays Object Ipos");
	
	if(ob && give_current_material(ob, ob->actcol)) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_MATERIAL,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_MA, 0, 0, "Displays Material Ipos");
		if(G.sipo->blocktype==ID_MA) {
			uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active Material texture. Click to change.");
			xco-= 4;
		}
	}
	if(G.scene->world) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_WORLD,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_WO, 0, 0, "Display World Ipos");
		if(G.sipo->blocktype==ID_WO) {
			uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active World texture. Click to change.");
			xco-= 4;
		}
	}
	
	if(ob && ob->type==OB_CURVE)
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_ANIM,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_CU, 0, 0, "Display Curve Ipos");
	
	if(ob && ob->type==OB_CAMERA)
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_CAMERA,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_CA, 0, 0, "Display Camera Ipos");
	
	if(ob && ob->type==OB_LAMP) {
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_LAMP,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_LA, 0, 0, "Display Lamp Ipos");
		if(G.sipo->blocktype==ID_LA) {
			uiDefButS(block, NUM, B_IPOMAIN, "",	xco+=XIC,0,XIC-4,YIC, &G.sipo->channel, 0.0, 7.0, 0, 0, "Displays Channel Number of the active Lamp texture. Click to change.");
			xco-= 4;
		}
	}
	
	if(ob) {
		if ELEM4(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE)
			uiDefIconButS(block, ROW, B_IPOMAIN, ICON_EDIT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_KE, 0, 0, "Displays VertexKeys Ipos");
		if (ob->action)
			uiDefIconButS(block, ROW, B_IPOMAIN, ICON_ACTION,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_AC, 0, 0, "Displays Action Ipos");
#ifdef __CON_IPO
		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_CONSTRAINT,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)IPO_CO, 0, 0, "Displays Constraint Ipos");
#endif
	}
	uiDefIconButS(block, ROW, B_IPOMAIN, ICON_SEQUENCE,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_SEQ, 0, 0, "Displays Sequence Ipos");

//	if(G.buts && G.buts->mainb == BUTS_SOUND && G.buts->lockpoin) 
//		uiDefIconButS(block, ROW, B_IPOMAIN, ICON_SOUND,	xco+=XIC,0,XIC,YIC, &G.sipo->blocktype, 1.0, (float)ID_SO, 0, 0, "Displays Sound Ipos");

	
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
	
	/* EXTRAP */
	uiDefIconBut(block, BUT, B_IPOCONT, ICON_CONSTANT,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Sets the extend mode to constant");
	uiDefIconBut(block, BUT, B_IPOEXTRAP, ICON_LINEAR,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Sets the extend mode to extrapolation");
	uiDefIconBut(block, BUT, B_IPOCYCLIC, ICON_CYCLIC,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0,	"Sets the extend mode to cyclic");
	uiDefIconBut(block, BUT, B_IPOCYCLICX, ICON_CYCLICLINEAR,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0,	"Sets the extend mode to cyclic extrapolation");
	xco+= XIC/2;

	uiClearButLock();
	/* ZOOM en BORDER */
	uiDefIconButI(block, TOG, B_VIEW2DZOOM, ICON_VIEWZOOM,	xco+=XIC,0,XIC,YIC, &viewmovetemp, 0, 0, 0, 0, "Zooms view (CTRL+MiddleMouse)");
	uiDefIconBut(block, BUT, B_IPOBORDER, ICON_BORDERMOVE,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms view to area");
	
	/* draw LOCK */
	uiDefIconButS(block, ICONTOG, 1, ICON_UNLOCKED,	xco+=XIC,0,XIC,YIC, &(G.sipo->lock), 0, 0, 0, 0, "Toggles forced redraw of other windows to reflect changes in real time");

	/* always do as last */
	curarea->headbutlen= xco+2*XIC;

	uiDrawBlock(block);
}
