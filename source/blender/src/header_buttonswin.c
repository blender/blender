/**
 * header_buttonswin.c oct-2003
 *
 * Functions to draw the "Buttons Window" window header
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
#include "DNA_armature_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#include "BIF_editconstraint.h"
#include "BIF_interface.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_butspace.h"

#include "BKE_armature.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"

#include "MEM_guardedalloc.h"

#include "blendef.h"
#include "mydevice.h"
#include "butspace.h"

Material matcopybuf;

void clear_matcopybuf(void)
{
	memset(&matcopybuf, 0, sizeof(Material));
}

void free_matcopybuf(void)
{
	extern MTex mtexcopybuf;	/* buttons.c */
	int a;

	for(a=0; a<8; a++) {
		if(matcopybuf.mtex[a]) {
			MEM_freeN(matcopybuf.mtex[a]);
			matcopybuf.mtex[a]= 0;
		}
	}
 
	default_mtex(&mtexcopybuf);
}

void do_buts_buttons(short event)
{
	static short matcopied=0;
	MTex *mtex;
	Material *ma;
	ID id;
	int a;
	float dx, dy;
	if(curarea->win==0) return;

	switch(event) {
	case B_BUTSHOME:
		uiSetPanel_view2d(curarea);
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
		
		/* we always put in left/top */
		dy= G.v2d->tot.ymax - G.v2d->cur.ymax;
		G.v2d->cur.ymin += dy;
		G.v2d->cur.ymax += dy;
		dx= G.v2d->tot.xmin - G.v2d->cur.xmin;
		G.v2d->cur.xmin += dx;
		G.v2d->cur.xmax += dx;
		
		scrarea_queue_winredraw(curarea);
		break;
	case B_BUTSPREVIEW:
		BIF_preview_changed(G.buts);
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		break;
	case B_MATCOPY:
		if(G.buts->lockpoin) {

			if(matcopied) free_matcopybuf();

			memcpy(&matcopybuf, G.buts->lockpoin, sizeof(Material));
			for(a=0; a<8; a++) {
				mtex= matcopybuf.mtex[a];
				if(mtex) {
					matcopybuf.mtex[a]= MEM_dupallocN(mtex);
				}
			}
			matcopied= 1;
		}
		break;
	case B_MATPASTE:
		if(matcopied && G.buts->lockpoin) {
			ma= G.buts->lockpoin;
			/* free current mat */
			for(a=0; a<8; a++) {
				mtex= ma->mtex[a];
				if(mtex && mtex->tex) mtex->tex->id.us--;
				if(mtex) MEM_freeN(mtex);
			}
			
			id= (ma->id);
			memcpy(G.buts->lockpoin, &matcopybuf, sizeof(Material));
			(ma->id)= id;
			
			for(a=0; a<8; a++) {
				mtex= ma->mtex[a];
				if(mtex) {
					ma->mtex[a]= MEM_dupallocN(mtex);
					if(mtex->tex) id_us_plus((ID *)mtex->tex);
				}
			}
			BIF_preview_changed(G.buts);
			scrarea_queue_winredraw(curarea);
		}
		break;
	case B_MESHTYPE:
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	}
}

void buttons_active_id(ID **id, ID **idfrom)
{
	Object *ob= OBACT;
	Material *ma;

	*id= NULL;
	*idfrom= (ID *)ob;
	
	if(G.buts->mainb==CONTEXT_SCENE) {
		int tab= G.buts->tab[CONTEXT_SCENE];
		
		if(tab==TAB_SCENE_RENDER) *id= (ID *)G.scene;
		else if(tab==TAB_SCENE_SOUND) {
			ID *search;
			
			// validate lockpoin, maybe its not a sound
			if (G.buts->lockpoin) {
				search = G.main->sound.first;
				while (search) {
					if (search == G.buts->lockpoin) {
						break;
					}
					search = search->next;
				}
				if (search == NULL) {
					*id = G.main->sound.first;
				} else {
					*id = search;
				}
			}
			else {
				*id = G.main->sound.first;
			}
		
		}
	}
	else if(G.buts->mainb==CONTEXT_SHADING) {
		int tab= G.buts->tab[CONTEXT_SHADING];
		
		if(tab==TAB_SHADING_LAMP) {
			if(ob && ob->type==OB_LAMP) {
				*id= ob->data;
			}
		}
		else if(tab==TAB_SHADING_MAT) {
			if(ob && (ob->type<OB_LAMP) && ob->type) {
				*id= (ID *)give_current_material(ob, ob->actcol);
				*idfrom= material_from(ob, ob->actcol);
			}
		}
		else if(tab==TAB_SHADING_WORLD) {
			*id= (ID *)G.scene->world;
			*idfrom= (ID *)G.scene;
		}
		else if(tab==TAB_SHADING_TEX) {
			MTex *mtex;
			
			if(G.buts->mainbo==G.buts->mainb && G.buts->tabo!=tab) {
				if(G.buts->tabo==TAB_SHADING_LAMP) G.buts->texfrom= 2;
				else if(G.buts->tabo==TAB_SHADING_WORLD) G.buts->texfrom= 1;
				else if(G.buts->tabo==TAB_SHADING_MAT) G.buts->texfrom= 0;
			}
	
			if(G.buts->texfrom==0) {
				if(ob && ob->type<OB_LAMP && ob->type) {
					ma= give_current_material(ob, ob->actcol);
					*idfrom= (ID *)ma;
					if(ma) {
						mtex= ma->mtex[ ma->texact ];
						if(mtex) *id= (ID *)mtex->tex;
					}
				}
			}
			else if(G.buts->texfrom==1) {
				World *wrld= G.scene->world;
				*idfrom= (ID *)wrld;
				if(wrld) {
					mtex= wrld->mtex[ wrld->texact];
					if(mtex) *id= (ID *)mtex->tex;
				}
			}
			else if(G.buts->texfrom==2) {
				Lamp *la;
				if(ob && ob->type==OB_LAMP) {
					la= ob->data;
					*idfrom= (ID *)la;
					mtex= la->mtex[ la->texact];
					if(mtex) *id= (ID *)mtex->tex;
				}
			}
		}
	}
	else if(G.buts->mainb==CONTEXT_OBJECT || G.buts->mainb==CONTEXT_LOGIC) {
		if(ob) {
			*idfrom= (ID *)G.scene;
			*id= (ID *)ob;
		}
	}
	else if(G.buts->mainb==CONTEXT_EDITING) {
		if(ob && ob->data) {
			*id= ob->data;
		}
	}
}

#if 0
static void validate_bonebutton(void *bonev, void *data2_unused){
	Bone *bone= bonev;
	bArmature *arm;

	arm = get_armature(G.obpose);
	unique_bone_name(bone, arm);
}


static int bonename_exists(Bone *orig, char *name, ListBase *list)
{
	Bone *curbone;
	
	for (curbone=list->first; curbone; curbone=curbone->next){
		/* Check this bone */
		if (orig!=curbone){
			if (!strcmp(curbone->name, name))
				return 1;
		}
		
		/* Check Children */
		if (bonename_exists(orig, name, &curbone->childbase))
			return 1;
	}
	
	return 0;

}

static void unique_bone_name (Bone *bone, bArmature *arm)
{
	char		tempname[64];
	char		oldname[64];
	int			number;
	char		*dot;

	if (!arm)
		return;

	strcpy(oldname, bone->name);

	/* See if we even need to do this */
	if (!bonename_exists(bone, bone->name, &arm->bonebase))
		return;

	/* Strip off the suffix */
	dot=strchr(bone->name, '.');
	if (dot)
		*dot=0;
	
	for (number = 1; number <=999; number++){
		sprintf (tempname, "%s.%03d", bone->name, number);
		
		if (!bonename_exists(bone, tempname, &arm->bonebase)){
			strcpy (bone->name, tempname);
			return;
		}
	}
}

static uiBlock *sbuts_context_menu(void *arg_unused)
{
	uiBlock *block;
	short yco = 0;

	block= uiNewBlock(&curarea->uiblocks, "context_options", UI_EMBOSSP, UI_HELV, curarea->headwin);

	/* should be branches from tree */
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_SCENE_DEHLT, "Scene|F10", 0, yco-=22, 100, 20, &G.buts->mainb, 0.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_EDIT, "Editing|F9", 0, yco-=22, 100, 20, &G.buts->mainb, 4.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_OBJECT, "Object|F6", 0, yco-=22, 100, 20, &G.buts->mainb, 1.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_MATERIAL_DEHLT, "Shading|F5", 0, yco-=22, 100, 20, &G.buts->mainb, 3.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_GAME, "Logic|F4", 0, yco-=22, 100, 20, &G.buts->mainb, 6.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_SCRIPT, "Script", 0, yco-=22, 100, 20, &G.buts->mainb, 5.0, 0.0, 0, 0, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	return block;
}
#endif

static void do_buts_viewmenu(void *arg, int event)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	
	switch(event) {
		case 0: /* panel alignment */
		case 1:
		case 2:
			sbuts->align= event;
			if(event) {
				uiAlignPanelStep(curarea, 1.0);
				do_buts_buttons(B_BUTSHOME);
			}
			break;
		case 3: /* View All */
			do_buts_buttons(B_BUTSHOME);
			break;
		case 4: /* Maximize Window */
			/* using event B_FULL */
			break;
	}
	allqueue(REDRAWVIEW3D, 0);
}

static uiBlock *buts_viewmenu(void *arg_unused)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "buts_viewmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_buts_viewmenu, NULL);
	
	if (sbuts->align == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Horizontal Align", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Horizontal Align", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	if (sbuts->align == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Vertical Align", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Vertical Align", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	if (sbuts->align == 0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Free Align", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Free Align", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "View All|Home", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 3, "");
		
	if (!curarea->full) 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Maximize Window|Ctrl UpArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	else 
		uiDefIconTextBut(block, BUTM, B_FULL, ICON_BLANK1, "Tile Window|Ctrl DownArrow", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 0, 4, "");
	
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

void buts_buttons(void)
{
	uiBlock *block;
	ID *id, *idfrom;
	short xco, xmax, t_base= 0;
	char naam[20];
	int colorid;

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSS, UI_HELV, curarea->headwin);

	if(area_is_active_area(curarea)) uiBlockSetCol(block, TH_HEADER);
	else uiBlockSetCol(block, TH_HEADERDESEL);

	curarea->butspacetype= SPACE_BUTS;
	
	
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
		uiDefBlockBut(block, buts_viewmenu, NULL, 
					  "View", xco, -2, xmax-3, 24, "");
		xco+= xmax;

	}

	uiBlockSetEmboss(block, UI_EMBOSSX);

	
	/* FULL WINDOW */
//	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
//	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");

	/* HOME */
//	uiDefIconBut(block, BUT, B_BUTSHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
//	xco+=XIC;
	
	/* mainb menu */
	/* (this could be done later with a dynamic tree and branches, also for python) */
	//{
	//	char mainbname[8][12]= {" Scene", " Object", " Types", " Shading", " Editing", " Script", " Logic"};
	//	char mainbicon[8]= {ICON_SCENE_DEHLT, ICON_OBJECT, ICON_BBOX, ICON_MATERIAL_DEHLT, ICON_EDIT, ICON_SCRIPT, ICON_GAME};
	//	uiBut *but= uiDefIconTextBlockBut(block, sbuts_context_menu, NULL, mainbicon[G.buts->mainb], mainbname[G.buts->mainb], xco, 0, 90, YIC, "Set main context for button panels");
	//	uiButClearFlag(but, UI_ICON_RIGHT); // this type has both flags set, and draws icon right.. uhh
	//	xco+= 90-XIC+10;
	//}
	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_REDR,	ICON_GAME,			xco, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_LOGIC, 0, 0, "Logic (F4) ");
	uiDefIconButS(block, ROW, B_REDR,	ICON_SCRIPT,		xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_SCRIPT, 0, 0, "Script ");
	uiDefIconButS(block, ROW, B_REDR,	ICON_MATERIAL_DEHLT,xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_SHADING, 0, 0, "Shading (F5) ");
	uiDefIconButS(block, ROW, B_REDR,	ICON_OBJECT,		xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_OBJECT, 0, 0, "Object (F7) ");
	uiDefIconButS(block, ROW, B_REDR,	ICON_EDIT,			xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_EDITING, 0, 0, "Editing (F9) ");
	uiDefIconButS(block, ROW, B_REDR,	ICON_SCENE_DEHLT,	xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_SCENE, 0, 0, "Scene (F10) ");
	
	xco+= XIC;
	
	// if(curarea->headertype==HEADERTOP)  t_base= -3; else t_base= 4;
	
	/* select the context to be drawn, per contex/tab the actual context is tested */
	uiBlockSetEmboss(block, UI_EMBOSS);	// normal
	switch(G.buts->mainb) {
	case CONTEXT_SCENE:
		uiBlockBeginAlign(block);
		uiDefIconButC(block, ROW, B_REDR,		ICON_SCENE,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_RENDER, 0, 0, "Render buttons ");
		uiDefIconButC(block, ROW, B_REDR,		ICON_ANIM,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_ANIM, 0, 0, "Anim/playback buttons");
		uiDefIconButC(block, ROW, B_REDR,		ICON_SOUND,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_SOUND, 0, 0, "Sound block buttons");
		
		break;
	case CONTEXT_OBJECT:
		
		break;
	case CONTEXT_SHADING:
		uiBlockBeginAlign(block);
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_LAMP,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_LAMP, 0, 0, "Lamp buttons");
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_MATERIAL,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_MAT, 0, 0, "Material buttons");
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_TEXTURE,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_TEX, 0, 0, "Texture buttons");
		uiDefIconButC(block, ROW, B_REDR,			ICON_RADIO,xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_RAD, 0, 0, "Radiosity buttons");
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_WORLD,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_WORLD, 0, 0, "World buttons");
		
		break;
	case CONTEXT_EDITING:
		
		break;
	case CONTEXT_SCRIPT:
		
		break;
	case CONTEXT_LOGIC:
		
		break;
	}
	
	uiBlockEndAlign(block);
	
	xco+=XIC;
	uiDefButS(block, NUM, B_NEWFRAME, "",	(short)(xco+20),0,60,YIC, &(G.scene->r.cfra), 1.0, 18000.0, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;

	buttons_active_id(&id, &idfrom);
	G.buts->lockpoin= id;

	/* always do as last */
	uiDrawBlock(block);
	curarea->headbutlen= xco;
}
