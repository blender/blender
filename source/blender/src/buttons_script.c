/**
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
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_radio_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_text_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vfont_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_space_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_sound.h"
#include "BKE_texture.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "BSE_filesel.h"

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

#include "BIF_butspace.h"

#include "mydevice.h"
#include "blendef.h"
#include "butspace.h"

/* ************************ function prototypes ********************** */
void draw_scriptlink(uiBlock *, ScriptLink *, int , int , int ) ;

/* *************************** SCRIPT ******************************** */

static void extend_scriptlink(ScriptLink *slink)
{
	void *stmp, *ftmp;

	if (!slink) return;
		
	stmp= slink->scripts;		
	slink->scripts= MEM_mallocN(sizeof(ID*)*(slink->totscript+1), "scriptlistL");
	
	ftmp= slink->flag;		
	slink->flag= MEM_mallocN(sizeof(short*)*(slink->totscript+1), "scriptlistF");
	
	if (slink->totscript) {
		memcpy(slink->scripts, stmp, sizeof(ID*)*(slink->totscript));
		MEM_freeN(stmp);

		memcpy(slink->flag, ftmp, sizeof(short)*(slink->totscript));
		MEM_freeN(ftmp);
	}

	slink->scripts[slink->totscript]= NULL;
	slink->flag[slink->totscript]= SCRIPT_FRAMECHANGED;

	slink->totscript++;
	slink->actscript = slink->totscript;
}

static void delete_scriptlink(ScriptLink *slink)
{
	int i;
	
	if (!slink) return;
	
	if (slink->totscript>0) {
		for (i=slink->actscript-1; i<slink->totscript-1; i++) {
			slink->flag[i]= slink->flag[i+1];
			slink->scripts[i]= slink->scripts[i+1];
		}
		
		slink->totscript--;
	}
		
	CLAMP(slink->actscript, 1, slink->totscript);
		
	if (slink->totscript==0) {
		if (slink->scripts) MEM_freeN(slink->scripts);
		if (slink->flag) MEM_freeN(slink->flag);

		slink->scripts= NULL;
		slink->flag= NULL;
		slink->totscript= slink->actscript= 0;			
	}
}

static char *scriptlinks_menu_string(void)
{
	char *menu = NULL;
	DynStr *ds = BLI_dynstr_new();
	Text *text = G.main->text.first;
	int txtcounter = 0;

	if (text) {
		BLI_dynstr_append(ds, "Select Script Link%t");
		while (text) {
			BLI_dynstr_append(ds, "|");
			BLI_dynstr_append(ds, text->id.name+2);
			txtcounter += 1;
			text = text->id.next;
		}
		if (txtcounter) menu = BLI_dynstr_get_cstring(ds);
	}
	BLI_dynstr_free(ds);
	return menu;
}

static void scriptlinks_pupmenu(ScriptLink *slink)
{
	short menuitem;
	char *menustr = scriptlinks_menu_string();

	if (!slink) return;

	if (menustr) {
		menuitem = pupmenu_col(menustr, 20);
		MEM_freeN(menustr);
		if (menuitem > 0) {
			Text *text = G.main->text.first;
			while (--menuitem) text = text->id.next;
			if (text) slink->scripts[slink->totscript - 1]= (ID *)text;
		}
	}
}

void do_scriptbuts(unsigned short event)
{
	Object *ob=NULL;
	ScriptLink *slink=NULL;
	Material *ma;
	
	switch (event) {
	case B_SSCRIPT_ADD:
		slink = &G.scene->scriptlink;
		extend_scriptlink(slink);
		BIF_undo_push("Add scriptlink");
		scriptlinks_pupmenu(slink);
		break;
	case B_SSCRIPT_DEL:
		BIF_undo_push("Delete scriptlink");
		delete_scriptlink(&G.scene->scriptlink);
		break;
		
	case B_SCRIPT_ADD:
	case B_SCRIPT_DEL:
		ob= OBACT;

		if (ob && G.buts->scriptblock==ID_OB) {
				slink= &ob->scriptlink;

		} else if (ob && G.buts->scriptblock==ID_MA) {
			ma= give_current_material(ob, ob->actcol);
			if (ma)	slink= &ma->scriptlink;

		} else if (ob && G.buts->scriptblock==ID_CA) {
			if (ob->type==OB_CAMERA)
				slink= &((Camera *)ob->data)->scriptlink;

		} else if (ob && G.buts->scriptblock==ID_LA) {
			if (ob->type==OB_LAMP)
				slink= &((Lamp *)ob->data)->scriptlink;

		} else if (G.buts->scriptblock==ID_WO) {
			if (G.scene->world) 
				slink= &(G.scene->world->scriptlink);
		}
		
		if (event==B_SCRIPT_ADD) {
			extend_scriptlink(slink);
			BIF_undo_push("Add scriptlink");
			scriptlinks_pupmenu(slink);
		}
		else {
			delete_scriptlink(slink);
			BIF_undo_push("Delete scriptlink");
		}
		break;
	default:
		break;
	}

	allqueue(REDRAWBUTSSCRIPT, 0);
	allqueue(REDRAWOOPS, 0);
}

void draw_scriptlink(uiBlock *block, ScriptLink *script, int sx, int sy, int idcode) 
{
	char str[256];

	if (script->totscript) {
		strcpy(str, "FrameChanged%x 1|");
		strcat(str, "Redraw%x 4|");
		strcat(str, "Render%x 16|");
		if (idcode==ID_SCE) {
			strcat(str, "OnLoad%x 2|");
			strcat(str, "OnSave%x 8");
		} else {
			strcat(str, "ObjectUpdate%x 64|");
			strcat(str, "ObDataUpdate%x 128");
 		}
		uiBlockBeginAlign(block);
		uiDefButS(block, MENU, 1, str, (short)sx, (short)sy, 140, 19, &script->flag[script->actscript-1], 0, 0, 0, 0, "Script links for this event");

		uiDefIDPoinBut(block, test_scriptpoin_but, ID_SCRIPT, 1, "", (short)(sx+140),(short)sy, 140, 19, &script->scripts[script->actscript-1], "Name of Script to link");
		uiBlockEndAlign(block);
	}
	
	sprintf(str,"%d Scr:", script->totscript);
	
	uiDefButS(block, NUM, REDRAWBUTSSCRIPT, str, (short)(sx+140), (short)sy-20,60,19, &script->actscript, 1, script->totscript, 0, 0, "Total / Active Script link (LeftMouse + Drag to change)");

	if (idcode==ID_SCE) {
		
		if (script->totscript<32767) 
			uiDefBut(block, BUT, B_SSCRIPT_ADD, "New", (short)(sx+240), (short)sy-20, 40, 19, 0, 0, 0, 0, 0, "Add a new Script link");
		if (script->totscript) 
			uiDefBut(block, BUT, B_SSCRIPT_DEL, "Del", (short)(sx+200), (short)sy-20, 40, 19, 0, 0, 0, 0, 0, "Delete the current Script link");
		uiDefBut(block, LABEL, 0, "Scene Script link",	sx,sy-20,140,20, 0, 0, 0, 0, 0, "");
	}
	else {
		if (script->totscript<32767) 
			uiDefBut(block, BUT, B_SCRIPT_ADD, "New", (short)(sx+240), (short)sy-20, 40, 19, 0, 0, 0, 0, 0, "Add a new Script link");
		if (script->totscript) 
			uiDefBut(block, BUT, B_SCRIPT_DEL, "Del", (short)(sx+200), (short)sy-20, 40, 19, 0, 0, 0, 0, 0, "Delete the current Script link");
		uiDefBut(block, LABEL, 0, "Selected Script link",	sx,sy-20,140,20, 0, 0, 0, 0, 0, "");
		
	}
	
}

/* ************************************* */

static void  script_panel_scriptlink(void)
{
	uiBlock *block;
	Object *ob=NULL;
	ScriptLink *script=NULL;
	Material *ma;
	int xco = 10;

	block= uiNewBlock(&curarea->uiblocks, "script_panel_scriptlink", UI_EMBOSS, UI_HELV, curarea->win);
	if(uiNewPanel(curarea, block, "Scriptlinks", "Script", 0, 0, 318, 204)==0) return;

	uiDefButBitI(block, TOG, G_DOSCRIPTLINKS, REDRAWBUTSSCRIPT,
			"Enable Script Links", xco, 200, 150, 20, &G.f, 0, 0, 0, 0,
			"Enable execution of all assigned Script links and Space Handelers");
	/* for proper alignment: */
	uiDefBut(block, LABEL, 0, "",	160, 200,150,20, NULL, 0.0, 0.0, 0, 0, "");
	
	if (G.f & G_DOSCRIPTLINKS) {
		//uiBlockBeginAlign(block);
		ob= OBACT;
		if(ob) 
			uiDefIconButS(block, ROW, B_REDR, ICON_OBJECT, xco,175,25,20, &G.buts->scriptblock,  2.0, (float)ID_OB, 0, 0, "Displays Object script links");

		if(ob && give_current_material(ob, ob->actcol))
			uiDefIconButS(block, ROW, B_REDR, ICON_MATERIAL,	xco+=25,175,25,20, &G.buts->scriptblock, 2.0, (float)ID_MA, 0, 0, "Displays Material script links ");

		if(G.scene->world) 
			uiDefIconButS(block, ROW, B_REDR, ICON_WORLD,	xco+=25,175,25,20, &G.buts->scriptblock, 2.0, (float)ID_WO, 0, 0, "Displays World script links");

		if(ob && ob->type==OB_CAMERA)
			uiDefIconButS(block, ROW, B_REDR, ICON_CAMERA,	xco+=25,175,25,20, &G.buts->scriptblock, 2.0, (float)ID_CA, 0, 0, "Displays Camera script links");

		if(ob && ob->type==OB_LAMP)
			uiDefIconButS(block, ROW, B_REDR, ICON_LAMP,	xco+=25,175,25,20, &G.buts->scriptblock, 2.0, (float)ID_LA, 0, 0, "Displays Lamp script links");
		//uiBlockEndAlign(block);

		if (ob && G.buts->scriptblock==ID_OB) {
			script= &ob->scriptlink;
		
		} else if (ob && G.buts->scriptblock==ID_MA) {
			ma= give_current_material(ob, ob->actcol);
			if (ma)	script= &ma->scriptlink;
		
		} else if (ob && G.buts->scriptblock==ID_CA) {
			if (ob->type==OB_CAMERA)
				script= &((Camera *)ob->data)->scriptlink;
			
		} else if (ob && G.buts->scriptblock==ID_LA) {
			if (ob->type==OB_LAMP)
				script= &((Lamp *)ob->data)->scriptlink;

		} else if (G.buts->scriptblock==ID_WO) {
			if (G.scene->world)
				script= &(G.scene->world->scriptlink);
		}

		if (script) draw_scriptlink(block, script, 10, 140, 0);

		draw_scriptlink(block, &G.scene->scriptlink, 10, 80, ID_SCE);
	}
}




void script_panels()
{
	script_panel_scriptlink();
	
}
