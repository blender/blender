/**
 * header_buttonswin.c oct-2003
 *
 * Functions to draw the "Buttons Window" window header
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "DNA_ID.h"
#include "DNA_armature_types.h"
#include "DNA_brush_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
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
#include "BKE_blender.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BSE_drawipo.h"
#include "BSE_node.h"
#include "BSE_headerbuttons.h"

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"

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

	for(a=0; a<MAX_MTEX; a++) {
		if(matcopybuf.mtex[a]) {
			MEM_freeN(matcopybuf.mtex[a]);
			matcopybuf.mtex[a]= NULL;
		}
	}
 
	if(matcopybuf.ramp_col) MEM_freeN(matcopybuf.ramp_col);
	if(matcopybuf.ramp_spec) MEM_freeN(matcopybuf.ramp_spec);
	
	matcopybuf.ramp_col= NULL;
	matcopybuf.ramp_spec= NULL;
	
	if(matcopybuf.nodetree) {
		ntreeFreeTree(matcopybuf.nodetree);
		MEM_freeN(matcopybuf.nodetree);
		matcopybuf.nodetree= NULL;
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
		BIF_preview_changed(ID_TE);
		G.buts->oldkeypress = 0;
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		break;
	case B_CONTEXT_SWITCH:
		G.buts->oldkeypress = 0;
		scrarea_queue_headredraw(curarea);
		scrarea_queue_winredraw(curarea);
		break;
	case B_MATCOPY:
		if(G.buts->lockpoin) {
			ma= G.buts->lockpoin;
			if(matcopied) free_matcopybuf();

			memcpy(&matcopybuf, ma, sizeof(Material));
			if(matcopybuf.ramp_col) matcopybuf.ramp_col= MEM_dupallocN(matcopybuf.ramp_col);
			if(matcopybuf.ramp_spec) matcopybuf.ramp_spec= MEM_dupallocN(matcopybuf.ramp_spec);

			for(a=0; a<MAX_MTEX; a++) {
				mtex= matcopybuf.mtex[a];
				if(mtex) {
					matcopybuf.mtex[a]= MEM_dupallocN(mtex);
				}
			}
			matcopybuf.nodetree= ntreeCopyTree(ma->nodetree, 0);
			matcopybuf.preview= NULL;
			matcopied= 1;
		}
		break;
	case B_MATPASTE:
		if(matcopied && G.buts->lockpoin) {
			ma= G.buts->lockpoin;
			
			/* free current mat */
			if(ma->ramp_col) MEM_freeN(ma->ramp_col);
			if(ma->ramp_spec) MEM_freeN(ma->ramp_spec);
			for(a=0; a<MAX_MTEX; a++) {
				mtex= ma->mtex[a];
				if(mtex && mtex->tex) mtex->tex->id.us--;
				if(mtex) MEM_freeN(mtex);
			}
			
			if(ma->nodetree) {
				ntreeFreeTree(ma->nodetree);
				MEM_freeN(ma->nodetree);
			}
			
			id= (ma->id);
			memcpy(ma, &matcopybuf, sizeof(Material));
			(ma->id)= id;
			
			if(matcopybuf.ramp_col) ma->ramp_col= MEM_dupallocN(matcopybuf.ramp_col);
			if(matcopybuf.ramp_spec) ma->ramp_spec= MEM_dupallocN(matcopybuf.ramp_spec);
			
			for(a=0; a<MAX_MTEX; a++) {
				mtex= ma->mtex[a];
				if(mtex) {
					ma->mtex[a]= MEM_dupallocN(mtex);
					if(mtex->tex) id_us_plus((ID *)mtex->tex);
				}
			}
			
			ma->nodetree= ntreeCopyTree(matcopybuf.nodetree, 0);

			BIF_preview_changed(ID_MA);
			BIF_undo_push("Paste material settings");
			scrarea_queue_winredraw(curarea);
		}
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
					bNode *node= NULL;
					
					ma= give_current_material(ob, ob->actcol);
					if(ma && ma->use_nodes)
						node= editnode_get_active_idnode(ma->nodetree, ID_TE);
						
					if(node) {
						*idfrom= NULL;
						*id= node->id;
					}
					else {
						ma= editnode_get_active_material(ma);
						*idfrom= (ID *)ma;
						if(ma) {
							mtex= ma->mtex[ ma->texact ];
							if(mtex) *id= (ID *)mtex->tex;
						}
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
			else if(G.buts->texfrom==3) {
				if(G.f & G_SCULPTMODE) {
					if(G.scene->sculptdata.texact != -1) {
						mtex= G.scene->sculptdata.mtex[G.scene->sculptdata.texact];
						if(mtex) *id= (ID*)mtex->tex;
					}
				} else {
					Brush *brush= G.scene->toolsettings->imapaint.brush;
					if (brush) {
						mtex= brush->mtex[brush->texact];
						if(mtex) *id= (ID*)mtex->tex;
					}
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

static void do_buts_view_shadingmenu(void *arg, int event)
{
	G.buts->mainb = CONTEXT_SHADING;
	
	allqueue(REDRAWBUTSALL, 0);
}


static uiBlock *buts_view_shadingmenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "buts_view_shadingmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_buts_view_shadingmenu, NULL);
	
	if((G.buts->mainb == CONTEXT_SHADING) && (G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_LAMP)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Lamp|F5",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_LAMP, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Lamp|F5",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_LAMP, 0.0, 0, 10, "");
	}
	
	if((G.buts->mainb == CONTEXT_SHADING) && (G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_MAT)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Material|F5",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_MAT, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Material|F5",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_MAT, 0.0, 0, 10, "");
	}
	
	if((G.buts->mainb == CONTEXT_SHADING) && (G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_TEX)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Texture|F6",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_TEX, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Texture|F6",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_TEX, 0.0, 0, 10, "");
	}
	
	if((G.buts->mainb == CONTEXT_SHADING) && (G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_RAD)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Radiosity",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_RAD, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Radiosity",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_RAD, 0.0, 0, 10, "");
	}
	
	if((G.buts->mainb == CONTEXT_SHADING) && (G.buts->tab[CONTEXT_SHADING]==TAB_SHADING_WORLD)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "World|F8",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_WORLD, 1.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "World|F8",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SHADING]), (float)TAB_SHADING_WORLD, 1.0, 0, 10, "");
	}
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
		
	return block;
}

static void do_buts_view_scenemenu(void *arg, int event)
{
	G.buts->mainb = CONTEXT_SCENE;
	
	allqueue(REDRAWBUTSALL, 0);
}


static uiBlock *buts_view_scenemenu(void *arg_unused)
{
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "buts_view_scenemenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_buts_view_scenemenu, NULL);
	
	if((G.buts->mainb == CONTEXT_SCENE) && (G.buts->tab[CONTEXT_SCENE]==TAB_SCENE_RENDER)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Render|F10",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_RENDER, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Render|F10",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_RENDER, 0.0, 0, 10, "");
	}

	if((G.buts->mainb == CONTEXT_SCENE) && (G.buts->tab[CONTEXT_SCENE]==TAB_SCENE_SEQUENCER)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Sequencer",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_SEQUENCER, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Sequencer",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_SEQUENCER, 0.0, 0, 10, "");
	}
	
	if((G.buts->mainb == CONTEXT_SCENE) && (G.buts->tab[CONTEXT_SCENE]==TAB_SCENE_ANIM)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Animation",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_ANIM, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Animation",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_ANIM, 0.0, 0, 10, "");
	}
	
	if((G.buts->mainb == CONTEXT_SCENE) && (G.buts->tab[CONTEXT_SCENE]==TAB_SCENE_SOUND)) {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Sound",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_SOUND, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButC(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Sound",
		0, yco-=20, menuwidth, 19, &(G.buts->tab[CONTEXT_SCENE]), (float)TAB_SCENE_SOUND, 0.0, 0, 10, "");
	}
		
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
		
	return block;
}

static void do_buts_view_alignmenu(void *arg, int event)
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
	}
	
	allqueue(REDRAWBUTSALL, 0);
}


static uiBlock *buts_view_alignmenu(void *arg_unused)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	uiBlock *block;
	short yco = 20, menuwidth = 120;

	block= uiNewBlock(&curarea->uiblocks, "buts_view_alignmenu", UI_EMBOSSP, UI_HELV, G.curscreen->mainwin);
	uiBlockSetButmFunc(block, do_buts_view_alignmenu, NULL);
	
	if (sbuts->align == 1) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Horizontal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Horizontal", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");

	if (sbuts->align == 2) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Vertical", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Vertical", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
	if (sbuts->align == 0) uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_HLT, "Free", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	else uiDefIconTextBut(block, BUTM, 1, ICON_CHECKBOX_DEHLT, "Free", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 0, "");
	
	uiBlockSetDirection(block, UI_RIGHT);
	uiTextBoundsBlock(block, 60);
		
	return block;
}

static void do_buts_viewmenu(void *arg, int event)
{	
	SpaceButs *sbuts= curarea->spacedata.first;
	
	switch(event) {
		case 1: /* zoom in */
			view2d_zoom(&sbuts->v2d, 0.06f, curarea->winx, curarea->winy);
			break;
		case 2: /* zoom out */
			view2d_zoom(&sbuts->v2d, -0.075f, curarea->winx, curarea->winy);
			break;
		case 3: /* View All */
			do_buts_buttons(B_BUTSHOME);
			break;
		case 4: /* Maximize Window */
			/* using event B_FULL */
			break;
		case 10: /* empty for the context events */
			break;	
	}
	allqueue(REDRAWBUTSALL, 0);
}

static uiBlock *buts_viewmenu(void *arg_unused)
{
	uiBlock *block;
	short yco= 0, menuwidth=120;
	
	block= uiNewBlock(&curarea->uiblocks, "buts_viewmenu", 
					  UI_EMBOSSP, UI_HELV, curarea->headwin);
	uiBlockSetButmFunc(block, do_buts_viewmenu, NULL);
	
	if(G.buts->mainb==CONTEXT_LOGIC) {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Logic|F4",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), CONTEXT_LOGIC, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Logic|F4",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), CONTEXT_LOGIC, 0.0, 0, 10, "");
	}
	
	if(G.buts->mainb==CONTEXT_SCRIPT) {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Script",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), (float)CONTEXT_SCRIPT, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Script",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), (float)CONTEXT_SCRIPT, 0.0, 0, 10, "");
	}
	
	uiDefIconTextBlockBut(block, buts_view_shadingmenu, NULL, ICON_RIGHTARROW_THIN, "Shading", 0, yco-=20, 120, 19, "");

	if(G.buts->mainb==CONTEXT_OBJECT) {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Object|F7",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), (float)CONTEXT_OBJECT, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Object|F7",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), (float)CONTEXT_OBJECT, 0.0, 0, 10, "");
	}
	
	if(G.buts->mainb==CONTEXT_EDITING) {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_HLT, "Editing|F9",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), (float)CONTEXT_EDITING, 0.0, 0, 10, "");
	} else {
		uiDefIconTextButS(block, BUTM, B_REDR, ICON_CHECKBOX_DEHLT, "Editing|F9",
		0, yco-=20, menuwidth, 19, &(G.buts->mainb), (float)CONTEXT_EDITING, 0.0, 0, 10, "");
	}
	
	uiDefIconTextBlockBut(block, buts_view_scenemenu, NULL, ICON_RIGHTARROW_THIN, "Scene", 0, yco-=20, 120, 19, "");
	
	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBlockBut(block, buts_view_alignmenu, NULL, ICON_RIGHTARROW_THIN, "Align", 0, yco-=20, 120, 19, "");

	uiDefBut(block, SEPR, 0, "", 0, yco-=6, menuwidth, 6, NULL, 0.0, 0.0, 0, 0, "");
	
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom In|NumPad +", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 1, "");
	uiDefIconTextBut(block, BUTM, 1, ICON_BLANK1, "Zoom Out|NumPad -", 0, yco-=20, menuwidth, 19, NULL, 0.0, 0.0, 1, 2, "");
	
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
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  xco,2,XIC,YIC-2,
					  &(curarea->flag), 0, 0, 0, 0, 
					  "Show pulldown menus");
	}
	else {
		uiDefIconButBitS(block, TOG, HEADER_NO_PULLDOWN, B_FLIPINFOMENU, 
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
	
		xmax= GetButStringLength("Panels");
		uiDefPulldownBut(block, buts_viewmenu, NULL, 
					  "Panels", xco, -2, xmax-3, 24, "");
		xco+= xmax;

	}

	uiBlockSetEmboss(block, UI_EMBOSS);

	
	/* FULL WINDOW */
//	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
//	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");

	/* HOME */
//	uiDefIconBut(block, BUT, B_BUTSHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
//	xco+=XIC;
	
	uiBlockBeginAlign(block);
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_GAME,			xco, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_LOGIC, 0, 0, "Logic (F4) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCRIPT,		xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_SCRIPT, 0, 0, "Script ");
	uiDefIconButS(block, ROW, B_BUTSPREVIEW,	ICON_MATERIAL_DEHLT,xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_SHADING, 0, 0, "Shading (F5) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_OBJECT,		xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_OBJECT, 0, 0, "Object (F7) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_EDIT,			xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_EDITING, 0, 0, "Editing (F9) ");
	uiDefIconButS(block, ROW, B_CONTEXT_SWITCH,	ICON_SCENE_DEHLT,	xco+=XIC, 0, XIC, YIC, &(G.buts->mainb), 0.0, (float)CONTEXT_SCENE, 0, 0, "Scene (F10) ");
	
	xco+= XIC;
	
	/* select the context to be drawn, per contex/tab the actual context is tested */
	uiBlockSetEmboss(block, UI_EMBOSS);	// normal
	switch(G.buts->mainb) {
	case CONTEXT_SCENE:
		uiBlockBeginAlign(block);
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_SCENE,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_RENDER, 0, 0, "Render buttons ");
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_SEQUENCE,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_SEQUENCER, 0, 0, "Sequencer buttons ");
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_ANIM,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_ANIM, 0, 0, "Anim/playback buttons");
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_SOUND,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_SOUND, 0, 0, "Sound block buttons");
		
		break;
	case CONTEXT_OBJECT:
		uiBlockBeginAlign(block);
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_OBJECT,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_OBJECT, 0, 0, "Object buttons ");
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_PHYSICS,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_PHYSICS, 0, 0, "Physics buttons");
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,		ICON_PARTICLES,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_OBJECT]), 1.0, (float)TAB_OBJECT_PARTICLE, 0, 0, "Particle buttons");

		break;
	case CONTEXT_SHADING:
		uiBlockBeginAlign(block);
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_LAMP,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_LAMP, 0, 0, "Lamp buttons");
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_MATERIAL,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_MAT, 0, 0, "Material buttons");
		uiDefIconButC(block, ROW, B_BUTSPREVIEW,	ICON_TEXTURE,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_TEX, 0, 0, "Texture buttons(F6)");
		uiDefIconButC(block, ROW, B_CONTEXT_SWITCH,			ICON_RADIO,xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SHADING]), 1.0, (float)TAB_SHADING_RAD, 0, 0, "Radiosity buttons");
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
	uiDefButI(block, NUM, B_NEWFRAME, "",	(xco+20),0,60,YIC, &(G.scene->r.cfra), 1.0, MAXFRAMEF, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;

	buttons_active_id(&id, &idfrom);
	G.buts->lockpoin= id;

	/* always do as last */
	uiDrawBlock(block);
	curarea->headbutlen= xco;
}
