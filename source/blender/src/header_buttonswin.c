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
#include "interface.h"
#include "mydevice.h"
#include "butspace.h"

Material matcopybuf;

static void unique_bone_name(Bone *bone, bArmature *arm);
static int bonename_exists(Bone *orig, char *name, ListBase *list);

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
	
	if(curarea->win==0) return;

	switch(event) {
	case B_BUTSHOME:
		uiSetPanel_view2d(curarea);
		G.v2d->cur= G.v2d->tot;
		test_view2d(G.v2d, curarea->winx, curarea->winy);
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
		*id= (ID *)G.scene;
		
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
	else if (G.buts->mainb == BUTS_SOUND) {
#if 0
		ID * search;

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
		} else {
			*id = G.main->sound.first;
		}
		//	printf("id:		 %d\n\n", *id);
#endif
	}
}

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
	uiBlockSetCol(block, MENUCOL);

	/* should be branches from tree */
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_SCENE_DEHLT, "Scene|F10", 0, yco-=22, 100, 20, &G.buts->mainb, 0.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_EDIT, "Editing|F9", 0, yco-=22, 100, 20, &G.buts->mainb, 4.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_OBJECT, "Object|F6", 0, yco-=22, 100, 20, &G.buts->mainb, 1.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_SCRIPT, "Script", 0, yco-=22, 100, 20, &G.buts->mainb, 5.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_GAME, "Logic", 0, yco-=22, 100, 20, &G.buts->mainb, 6.0, 0.0, 0, 0, "");
	uiDefIconTextButS(block, BUTM, B_REDR, ICON_MATERIAL_DEHLT, "Shading|F5", 0, yco-=22, 100, 20, &G.buts->mainb, 3.0, 0.0, 0, 0, "");
	
	if(curarea->headertype==HEADERTOP) {
		uiBlockSetDirection(block, UI_DOWN);
	}
	else {
		uiBlockSetDirection(block, UI_TOP);
		uiBlockFlipOrder(block);
	}

	return block;
}


void buts_buttons(void)
{
	ID *id, *idfrom;
	Object *ob;
	uiBlock *block;
	uiBut *but;
	short xco, t_base= -2;
	int alone, local, browse;
	char naam[20];

	sprintf(naam, "header %d", curarea->headwin);
	block= uiNewBlock(&curarea->uiblocks, naam, UI_EMBOSSX, UI_HELV, curarea->headwin);
	uiBlockSetCol(block, BUTGREY);

	curarea->butspacetype= SPACE_BUTS;

	uiDefIconTextButC(block, ICONTEXTROW,B_NEWSPACE, ICON_VIEW3D, windowtype_pup(), 6,0,XIC,YIC, &(curarea->butspacetype), 1.0, SPACEICONMAX, 0, 0, "Displays Current Window Type. Click for menu of available types.");

	/* FULL WINDOW */
	xco= 25;
	if(curarea->full) uiDefIconBut(block, BUT,B_FULL, ICON_SPLITSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Returns to multiple views window (CTRL+Up arrow)");
	else uiDefIconBut(block, BUT,B_FULL, ICON_FULLSCREEN,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Makes current window full screen (CTRL+Down arrow)");

	/* HOME */
	uiDefIconBut(block, BUT, B_BUTSHOME, ICON_HOME,	xco+=XIC,0,XIC,YIC, 0, 0, 0, 0, 0, "Zooms window to home view showing all items (HOMEKEY)");
	xco+=XIC+10;
	
	/* mainb menu */
	/* (this could be done later with a dynamic tree and branches, also for python) */
	uiBlockSetCol(block, MIDGREY);
	uiBlockSetEmboss(block, UI_EMBOSSMB);	// menu but

	{
		char mainbname[8][12]= {" Scene", " Object", " Types", " Shading", " Editing", " Script", " Logic"};
		char mainbicon[8]= {ICON_SCENE_DEHLT, ICON_OBJECT, ICON_BBOX, ICON_MATERIAL_DEHLT, ICON_EDIT, ICON_SCRIPT, ICON_GAME};
		uiBut *but= uiDefIconTextBlockBut(block, sbuts_context_menu, NULL, mainbicon[G.buts->mainb], mainbname[G.buts->mainb], xco, 0, 90, YIC, "Set main context for button panels");
		uiButClearFlag(but, UI_ICON_RIGHT); // this type has both flags set, and draws icon right.. uhh
		xco+= 90-XIC+10;
	}
	
	/* select the context to be drawn, per contex/tab the actual context is tested */
	uiBlockSetEmboss(block, UI_EMBOSSX);	// normal
	switch(G.buts->mainb) {
	case CONTEXT_SCENE:
		uiDefIconButC(block, ROW, B_REDR,		ICON_SCENE,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_RENDER, 0, 0, "Render buttons ");
		uiDefIconButC(block, ROW, B_REDR,		ICON_ANIM,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_ANIM, 0, 0, "Anim/playback buttons");
		uiDefIconButC(block, ROW, B_REDR,		ICON_SOUND,	xco+=XIC, t_base, XIC, YIC, &(G.buts->tab[CONTEXT_SCENE]), 1.0, (float)TAB_SCENE_SOUND, 0, 0, "Sound block buttons");
		
		break;
	case CONTEXT_OBJECT:
		
		break;
	case CONTEXT_SHADING:
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


	xco+=XIC+XIC;
	ob= OBACT;

	buttons_active_id(&id, &idfrom);
	G.buts->lockpoin= id;
	
	if(G.buts->mainb==CONTEXT_SHADING) {
#if 0
		int tab= G.buts->tab[CONTEXT_SHADING];
		
		if(tab==TAB_SHADING_MAT) {
		}
		else if(tab==TAB_SHADING_TEX) {
			if(G.buts->texfrom==0) {
				if(idfrom) {
					xco= std_libbuttons(block, xco, 0, 0, NULL, B_TEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
				}
			}
			else if(G.buts->texfrom==1) {
				if(idfrom) {
					xco= std_libbuttons(block, xco, 0, 0, NULL, B_WTEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
				}
			}
			else if(G.buts->texfrom==2) {
				if(idfrom) {
					xco= std_libbuttons(block, xco, 0, 0, NULL, B_LTEXBROWSE, id, idfrom, &(G.buts->texnr), B_TEXALONE, B_TEXLOCAL, B_TEXDELETE, B_AUTOTEXNAME, B_KEEPDATA);
				}
			}
		}
		else if(tab==TAB_SHADING_LAMP) {
			if(id) {
				xco= std_libbuttons(block, xco, 0, 0, NULL, B_LAMPBROWSE, id, (ID *)ob, &(G.buts->menunr), B_LAMPALONE, B_LAMPLOCAL, 0, 0, 0);	
			}
		}
		else if(tab==TAB_SHADING_WORLD) {
			xco= std_libbuttons(block, xco, 0, 0, NULL, B_WORLDBROWSE, id, idfrom, &(G.buts->menunr), B_WORLDALONE, B_WORLDLOCAL, B_WORLDDELETE, 0, B_KEEPDATA);
		}
#endif
	}
	else if(G.buts->mainb==CONTEXT_EDITING) {

		if(id) {
			
			alone= 0;
			local= 0;
			browse= B_EDITBROWSE;
			xco+= XIC;
			
			if(ob->type==OB_MESH) {
				browse= B_MESHBROWSE;
				alone= B_MESHALONE;
				local= B_MESHLOCAL;
				uiSetButLock(G.obedit!=0, "Unable to perform function in EditMode");
			}
			else if(ob->type==OB_MBALL) {
				alone= B_MBALLALONE;
				local= B_MBALLLOCAL;
			}
			else if ELEM3(ob->type, OB_CURVE, OB_FONT, OB_SURF) {
				alone= B_CURVEALONE;
				local= B_CURVELOCAL;
			}
			else if(ob->type==OB_CAMERA) {
				alone= B_CAMERAALONE;
				local= B_CAMERALOCAL;
			}
			else if(ob->type==OB_LAMP) {
				alone= B_LAMPALONE;
				local= B_LAMPLOCAL;
			}
			else if (ob->type==OB_ARMATURE){
				alone = B_ARMALONE;
				local = B_ARMLOCAL;
			}
			else if(ob->type==OB_LATTICE) {
				alone= B_LATTALONE;
				local= B_LATTLOCAL;
			}
			
			xco= std_libbuttons(block, xco, 0, 0, NULL, browse, id, idfrom, &(G.buts->menunr), alone, local, 0, 0, B_KEEPDATA);
				
			xco+= XIC;
		}
		if(ob) {
			but= uiDefBut(block, TEX, B_IDNAME, "OB:",	xco, 0, 135, YIC, ob->id.name+2, 0.0, 19.0, 0, 0, "Displays Active Object name. Click to change.");
			uiButSetFunc(but, test_idbutton_cb, ob->id.name, NULL);
			xco+= 135;
		}
	}

#if 0
	else if (G.buts->mainb==BUTS_SOUND) {
		xco= std_libbuttons(block, xco, 0, 0, NULL, B_SOUNDBROWSE2, id, idfrom, &(G.buts->texnr), 1, 0, 0, 0, 0);
	}

	else if(G.buts->mainb==BUTS_CONSTRAINT){
		if(id) {

	short type;
	void *data=NULL;
	char str[256];


			xco= std_libbuttons(block, xco, 0, 0, NULL, 0, id, idfrom, &(G.buts->menunr), B_OBALONE, B_OBLOCAL, 0, 0, 0);

			get_constraint_client(NULL, &type, &data);
			if (data && type==TARGET_BONE){
				sprintf(str, "BO:%s", ((Bone*)data)->name);
				but= uiDefBut(block, LABEL, 1, str,	xco, 0, 0, 135, YIC, ((Bone*)data)->name, 0.0, 19.0, 0, 0, "Displays Active Bone name. Click to change.");
				xco+= 135;
			}
		}
	}
	else if(G.buts->mainb==BUTS_SCRIPT) {
		if(ob)
			uiDefIconButS(block, ROW, B_REDR, ICON_OBJECT, xco,0,XIC,YIC, &G.buts->scriptblock,  2.0, (float)ID_OB, 0, 0, "Displays Object script links");

		if(ob && give_current_material(ob, ob->actcol))
			uiDefIconButS(block, ROW, B_REDR, ICON_MATERIAL,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_MA, 0, 0, "Displays Material script links ");

		if(G.scene->world) 
			uiDefIconButS(block, ROW, B_REDR, ICON_WORLD,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_WO, 0, 0, "Displays World script links");
	
		if(ob && ob->type==OB_CAMERA)
			uiDefIconButS(block, ROW, B_REDR, ICON_CAMERA,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_CA, 0, 0, "Displays Camera script links");

		if(ob && ob->type==OB_LAMP)
			uiDefIconButS(block, ROW, B_REDR, ICON_LAMP,	xco+=XIC,0,XIC,YIC, &G.buts->scriptblock, 2.0, (float)ID_LA, 0, 0, "Displays Lamp script links");

		xco+= 20;
	}
	
	uiDefButS(block, NUM, B_NEWFRAME, "",	(short)(xco+20),0,60,YIC, &(G.scene->r.cfra), 1.0, 18000.0, 0, 0, "Displays Current Frame of animation. Click to change.");
	xco+= 80;
#endif

	/* always do as last */
	uiDrawBlock(block);
	curarea->headbutlen= xco;
}
