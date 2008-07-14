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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_resources.h"
#include "BIF_interface_icons.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "blendef.h"	// CLAMP
#include "datatoc.h"

/* global for themes */
typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

static bTheme *theme_active=NULL;
static int theme_spacetype= SPACE_VIEW3D;


void BIF_resources_init(void)
{
	BIF_icons_init(BIFICONID_LAST+1);
}

void BIF_resources_free(void)
{
	BIF_icons_free();
}


/* ******************************************************** */
/*    THEMES */
/* ******************************************************** */

char *BIF_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid)
{
	ThemeSpace *ts= NULL;
	static char error[4]={240, 0, 240, 255};
	static char alert[4]={240, 60, 60, 255};
	static char headerdesel[4]={0,0,0,255};
	static char custom[4]={0,0,0,255};
	
	char *cp= error;
	
	if(btheme) {
	
		// first check for ui buttons theme
		if(colorid < TH_THEMEUI) {
		
			switch(colorid) {
			case TH_BUT_OUTLINE:
				cp= btheme->tui.outline; break;
			case TH_BUT_NEUTRAL:
				cp= btheme->tui.neutral; break;
			case TH_BUT_ACTION:
				cp= btheme->tui.action; break;
			case TH_BUT_SETTING:
				cp= btheme->tui.setting; break;
			case TH_BUT_SETTING1:
				cp= btheme->tui.setting1; break;
			case TH_BUT_SETTING2:
				cp= btheme->tui.setting2; break;
			case TH_BUT_NUM:
				cp= btheme->tui.num; break;
			case TH_BUT_TEXTFIELD:
				cp= btheme->tui.textfield; break;
			case TH_BUT_TEXTFIELD_HI:
				cp= btheme->tui.textfield_hi; break;
			case TH_BUT_POPUP:
				cp= btheme->tui.popup; break;
			case TH_BUT_TEXT:
				cp= btheme->tui.text; break;
			case TH_BUT_TEXT_HI:
				cp= btheme->tui.text_hi; break;
			case TH_MENU_BACK:
				cp= btheme->tui.menu_back; break;
			case TH_MENU_ITEM:
				cp= btheme->tui.menu_item; break;
			case TH_MENU_HILITE:
				cp= btheme->tui.menu_hilite; break;
			case TH_MENU_TEXT:
				cp= btheme->tui.menu_text; break;
			case TH_MENU_TEXT_HI:
				cp= btheme->tui.menu_text_hi; break;
			
			case TH_BUT_DRAWTYPE:
				cp= &btheme->tui.but_drawtype; break;

			case TH_ICONFILE:
				cp= btheme->tui.iconfile; break;
				
			case TH_REDALERT:
				cp= alert; break;
			case TH_CUSTOM:
				cp= custom; break;
			}
		}
		else {
		
			switch(spacetype) {
			case SPACE_BUTS:
				ts= &btheme->tbuts;
				break;
			case SPACE_VIEW3D:
				ts= &btheme->tv3d;
				break;
			case SPACE_IPO:
				ts= &btheme->tipo;
				break;
			case SPACE_FILE:
				ts= &btheme->tfile;
				break;
			case SPACE_NLA:
				ts= &btheme->tnla;
				break;
			case SPACE_ACTION:
				ts= &btheme->tact;
				break;
			case SPACE_SEQ:
				ts= &btheme->tseq;
				break;
			case SPACE_IMAGE:
				ts= &btheme->tima;
				break;
			case SPACE_IMASEL:
				ts= &btheme->timasel;
				break;
			case SPACE_TEXT:
				ts= &btheme->text;
				break;
			case SPACE_OOPS:
				ts= &btheme->toops;
				break;
			case SPACE_SOUND:
				ts= &btheme->tsnd;
				break;
			case SPACE_INFO:
				ts= &btheme->tinfo;
				break;
			case SPACE_TIME:
				ts= &btheme->ttime;
				break;
			case SPACE_NODE:
				ts= &btheme->tnode;
				break;
			default:
				ts= &btheme->tv3d;
				break;
			}
			
			switch(colorid) {
			case TH_BACK:
				cp= ts->back; break;
			case TH_TEXT:
				cp= ts->text; break;
			case TH_TEXT_HI:
				cp= ts->text_hi; break;
			case TH_HEADER:
				cp= ts->header; break;
			case TH_HEADERDESEL:
				/* we calculate a dynamic builtin header deselect color, also for pulldowns... */
				cp= ts->header; 
				headerdesel[0]= cp[0]>10?cp[0]-10:0;
				headerdesel[1]= cp[1]>10?cp[1]-10:0;
				headerdesel[2]= cp[2]>10?cp[2]-10:0;
				cp= headerdesel;
				break;
			case TH_PANEL:
				cp= ts->panel; break;
			case TH_SHADE1:
				cp= ts->shade1; break;
			case TH_SHADE2:
				cp= ts->shade2; break;
			case TH_HILITE:
				cp= ts->hilite; break;
				
			case TH_GRID:
				cp= ts->grid; break;
			case TH_WIRE:
				cp= ts->wire; break;
			case TH_LAMP:
				cp= ts->lamp; break;
			case TH_SELECT:
				cp= ts->select; break;
			case TH_ACTIVE:
				cp= ts->active; break;
			case TH_GROUP:
				cp= ts->group; break;
			case TH_GROUP_ACTIVE:
				cp= ts->group_active; break;
			case TH_TRANSFORM:
				cp= ts->transform; break;
			case TH_VERTEX:
				cp= ts->vertex; break;
			case TH_VERTEX_SELECT:
				cp= ts->vertex_select; break;
			case TH_VERTEX_SIZE:
				cp= &ts->vertex_size; break;
			case TH_EDGE:
				cp= ts->edge; break;
			case TH_EDGE_SELECT:
				cp= ts->edge_select; break;
			case TH_EDGE_SEAM:
				cp= ts->edge_seam; break;
			case TH_EDGE_SHARP:
				cp= ts->edge_sharp; break;
			case TH_EDITMESH_ACTIVE:
				cp= ts->editmesh_active; break;
			case TH_EDGE_FACESEL:
				cp= ts->edge_facesel; break;
			case TH_FACE:
				cp= ts->face; break;
			case TH_FACE_SELECT:
				cp= ts->face_select; break;
			case TH_FACE_DOT:
				cp= ts->face_dot; break;
			case TH_FACEDOT_SIZE:
				cp= &ts->facedot_size; break;
			case TH_NORMAL:
				cp= ts->normal; break;
			case TH_BONE_SOLID:
				cp= ts->bone_solid; break;
			case TH_BONE_POSE:
				cp= ts->bone_pose; break;
			case TH_STRIP:
				cp= ts->strip; break;
			case TH_STRIP_SELECT:
				cp= ts->strip_select; break;
			case TH_CFRAME:
				cp= ts->cframe; break;
				
			case TH_SYNTAX_B:
				cp= ts->syntaxb; break;
			case TH_SYNTAX_V:
				cp= ts->syntaxv; break;
			case TH_SYNTAX_C:
				cp= ts->syntaxc; break;
			case TH_SYNTAX_L:
				cp= ts->syntaxl; break;
			case TH_SYNTAX_N:
				cp= ts->syntaxn; break;

			case TH_NODE:
				cp= ts->syntaxl; break;
			case TH_NODE_IN_OUT:
				cp= ts->syntaxn; break;
			case TH_NODE_OPERATOR:
				cp= ts->syntaxb; break;
			case TH_NODE_CONVERTOR:
				cp= ts->syntaxv; break;
			case TH_NODE_GROUP:
				cp= ts->syntaxc; break;
				
			case TH_SEQ_MOVIE:
				cp= ts->movie; break;
			case TH_SEQ_IMAGE:
				cp= ts->image; break;
			case TH_SEQ_SCENE:
				cp= ts->scene; break;
			case TH_SEQ_AUDIO:
				cp= ts->audio; break;
			case TH_SEQ_EFFECT:
				cp= ts->effect; break;
			case TH_SEQ_PLUGIN:
				cp= ts->plugin; break;
			case TH_SEQ_TRANSITION:
				cp= ts->transition; break;
			case TH_SEQ_META:
				cp= ts->meta; break;
				
			}

		}
	}
	
	return cp;
}

#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;

/* initialize
   Note: when you add new colors, created & saved themes need initialized
   in usiblender.c, search for "versionfile"
*/
void BIF_InitTheme(void)
{
	bTheme *btheme= U.themes.first;
	
	/* we search for the theme with name Default */
	for(btheme= U.themes.first; btheme; btheme= btheme->next) {
		if(strcmp("Default", btheme->name)==0) break;
	}
	
	if(btheme==NULL) {
		btheme= MEM_callocN(sizeof(bTheme), "theme");
		BLI_addtail(&U.themes, btheme);
		strcpy(btheme->name, "Default");
	}
	
	BIF_SetTheme(NULL);	// make sure the global used in this file is set

	/* UI buttons (todo) */
	SETCOL(btheme->tui.outline, 	0xA0,0xA0,0xA0, 255);
	SETCOL(btheme->tui.neutral, 	0xA0,0xA0,0xA0, 255);
	SETCOL(btheme->tui.action, 		0xAD,0xA0,0x93, 255);
	SETCOL(btheme->tui.setting, 	0x8A,0x9E,0xA1, 255);
	SETCOL(btheme->tui.setting1, 	0xA1,0xA1,0xAE, 255);
	SETCOL(btheme->tui.setting2, 	0xA1,0x99,0xA7, 255);
	SETCOL(btheme->tui.num,		 	0x90,0x90,0x90, 255);
	SETCOL(btheme->tui.textfield,	0x90,0x90,0x90, 255);
	SETCOL(btheme->tui.textfield_hi,0xc6,0x77,0x77, 255);
	SETCOL(btheme->tui.popup,		0xA0,0xA0,0xA0, 255);
	
	SETCOL(btheme->tui.text,		0,0,0, 255);
	SETCOL(btheme->tui.text_hi, 	255, 255, 255, 255);
	
	SETCOL(btheme->tui.menu_back, 	0xD2,0xD2,0xD2, 255);
	SETCOL(btheme->tui.menu_item, 	0xDA,0xDA,0xDA, 255);
	SETCOL(btheme->tui.menu_hilite, 0x7F,0x7F,0x7F, 255);
	SETCOL(btheme->tui.menu_text, 	0, 0, 0, 255);
	SETCOL(btheme->tui.menu_text_hi, 255, 255, 255, 255);
	btheme->tui.but_drawtype= TH_SHADED;
	
	BLI_strncpy(btheme->tui.iconfile, "", sizeof(btheme->tui.iconfile));
	
	/* bone color sets */
		/* set 1 */
	SETCOL(btheme->tarm[0].solid, 0x9a, 0x00, 0x00, 255);
	SETCOL(btheme->tarm[0].select, 0xbd, 0x11, 0x11, 255);
	SETCOL(btheme->tarm[0].active, 0xf7, 0x0a, 0x0a, 255);
		/* set 2 */
	SETCOL(btheme->tarm[1].solid, 0xf7, 0x40, 0x18, 255);
	SETCOL(btheme->tarm[1].select, 0xf6, 0x69, 0x13, 255);
	SETCOL(btheme->tarm[1].active, 0xfa, 0x99, 0x00, 255);
	    /* set 3 */
	SETCOL(btheme->tarm[2].solid, 0x1e, 0x91, 0x09, 255);
	SETCOL(btheme->tarm[2].select, 0x59, 0xb7, 0x0b, 255);
	SETCOL(btheme->tarm[2].active, 0x83, 0xef, 0x1d, 255);
		/* set 4 */
	SETCOL(btheme->tarm[3].solid, 0x0a, 0x36, 0x94, 255);
	SETCOL(btheme->tarm[3].select, 0x36, 0x67, 0xdf, 255);
	SETCOL(btheme->tarm[3].active, 0x5e, 0xc1, 0xef, 255);
		/* set 5 */
	SETCOL(btheme->tarm[4].solid, 0xa9, 0x29, 0x4e, 255);
	SETCOL(btheme->tarm[4].select, 0xc1, 0x41, 0x6a, 255);
	SETCOL(btheme->tarm[4].active, 0xf0, 0x5d, 0x91, 255);
		/* set 6 */
	SETCOL(btheme->tarm[5].solid, 0x43, 0x0c, 0x78, 255);
	SETCOL(btheme->tarm[5].select, 0x54, 0x3a, 0xa3, 255);
	SETCOL(btheme->tarm[5].active, 0x87, 0x64, 0xd5, 255);
		/* set 7 */
	SETCOL(btheme->tarm[6].solid, 0x24, 0x78, 0x5a, 255);
	SETCOL(btheme->tarm[6].select, 0x3c, 0x95, 0x79, 255);
	SETCOL(btheme->tarm[6].active, 0x6f, 0xb6, 0xab, 255);
		/* set 8 */
	SETCOL(btheme->tarm[7].solid, 0x4b, 0x70, 0x7c, 255);
	SETCOL(btheme->tarm[7].select, 0x6a, 0x86, 0x91, 255);
	SETCOL(btheme->tarm[7].active, 0x9b, 0xc2, 0xcd, 255);
		/* set 9 */
	SETCOL(btheme->tarm[8].solid, 0xf4, 0xc9, 0x0c, 255);
	SETCOL(btheme->tarm[8].select, 0xee, 0xc2, 0x36, 255);
	SETCOL(btheme->tarm[8].active, 0xf3, 0xff, 0x00, 255);
		/* set 10 */
	SETCOL(btheme->tarm[9].solid, 0x1e, 0x20, 0x24, 255);
	SETCOL(btheme->tarm[9].select, 0x48, 0x4c, 0x56, 255);
	SETCOL(btheme->tarm[9].active, 0xff, 0xff, 0xff, 255);
		/* set 11 */
	SETCOL(btheme->tarm[10].solid, 0x6f, 0x2f, 0x6a, 255);
	SETCOL(btheme->tarm[10].select, 0x98, 0x45, 0xbe, 255);
	SETCOL(btheme->tarm[10].active, 0xd3, 0x30, 0xd6, 255);
		/* set 12 */
	SETCOL(btheme->tarm[11].solid, 0x6c, 0x8e, 0x22, 255);
	SETCOL(btheme->tarm[11].select, 0x7f, 0xb0, 0x22, 255);
	SETCOL(btheme->tarm[11].active, 0xbb, 0xef, 0x5b, 255);
		/* set 13 */
	SETCOL(btheme->tarm[12].solid, 0x8d, 0x8d, 0x8d, 255);
	SETCOL(btheme->tarm[12].select, 0xb0, 0xb0, 0xb0, 255);
	SETCOL(btheme->tarm[12].active, 0xde, 0xde, 0xde, 255);
		/* set 14 */
	SETCOL(btheme->tarm[13].solid, 0x83, 0x43, 0x26, 255);
	SETCOL(btheme->tarm[13].select, 0x8b, 0x58, 0x11, 255);
	SETCOL(btheme->tarm[13].active, 0xbd, 0x6a, 0x11, 255);
		/* set 15 */
	SETCOL(btheme->tarm[14].solid, 0x08, 0x31, 0x0e, 255);
	SETCOL(btheme->tarm[14].select, 0x1c, 0x43, 0x0b, 255);
	SETCOL(btheme->tarm[14].active, 0x34, 0x62, 0x2b, 255);
	
	/* space view3d */
	SETCOL(btheme->tv3d.back, 	115, 115, 115, 255);
	SETCOL(btheme->tv3d.text, 	0, 0, 0, 255);
	SETCOL(btheme->tv3d.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tv3d.header, 195, 195, 195, 255);
	SETCOL(btheme->tv3d.panel, 	165, 165, 165, 127);
	
	SETCOL(btheme->tv3d.shade1,  160, 160, 160, 100);
	SETCOL(btheme->tv3d.shade2,  0x7f, 0x70, 0x70, 100);

	SETCOL(btheme->tv3d.grid, 	92, 92, 92, 255);
	SETCOL(btheme->tv3d.wire, 	0x0, 0x0, 0x0, 255);
	SETCOL(btheme->tv3d.lamp, 	0, 0, 0, 40);
	SETCOL(btheme->tv3d.select, 0xff, 0x88, 0xff, 255);
	SETCOL(btheme->tv3d.active, 0xff, 0xbb, 0xff, 255);
	SETCOL(btheme->tv3d.group, 0x10, 0x40, 0x10, 255);
	SETCOL(btheme->tv3d.group_active, 0x55, 0xbb, 0x55, 255);
	SETCOL(btheme->tv3d.transform, 0xff, 0xff, 0xff, 255);
	SETCOL(btheme->tv3d.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tv3d.vertex_select, 0xff, 0xff, 0x70, 255);
	btheme->tv3d.vertex_size= 2;
	SETCOL(btheme->tv3d.edge, 	0x0, 0x0, 0x0, 255);
	SETCOL(btheme->tv3d.edge_select, 0xb0, 0xb0, 0x30, 255);
	SETCOL(btheme->tv3d.edge_seam, 230, 150, 50, 255);
	SETCOL(btheme->tv3d.edge_sharp, 255, 32, 32, 255);
	SETCOL(btheme->tv3d.edge_facesel, 75, 75, 75, 255);
	SETCOL(btheme->tv3d.editmesh_active, 255, 255, 255, 128);
	
	SETCOL(btheme->tv3d.face, 	0, 50, 150, 30);
	SETCOL(btheme->tv3d.face_select, 200, 100, 200, 60);
	SETCOL(btheme->tv3d.normal, 0x22, 0xDD, 0xDD, 255);
	SETCOL(btheme->tv3d.face_dot, 255, 138, 48, 255);
	btheme->tv3d.facedot_size= 4;
	SETCOL(btheme->tv3d.cframe, 0x60, 0xc0, 0x40, 255);
	
	SETCOL(btheme->tv3d.bone_solid, 200, 200, 200, 255);
	SETCOL(btheme->tv3d.bone_pose, 80, 200, 255, 80);		// alpha 80 is not meant editable, used for wire+action draw
	
	
	/* space buttons */
	/* to have something initialized */
	btheme->tbuts= btheme->tv3d;

	SETCOL(btheme->tbuts.back, 	180, 180, 180, 255);
	SETCOL(btheme->tbuts.header, 195, 195, 195, 255);
	SETCOL(btheme->tbuts.panel,  255, 255, 255, 40);

	/* space ipo */
	/* to have something initialized */
	btheme->tipo= btheme->tv3d;

	SETCOL(btheme->tipo.grid, 	94, 94, 94, 255);
	SETCOL(btheme->tipo.back, 	120, 120, 120, 255);
	SETCOL(btheme->tipo.header, 195, 195, 195, 255);
	SETCOL(btheme->tipo.panel,  255, 255, 255, 150);
	SETCOL(btheme->tipo.shade1,  172, 172, 172, 100);
	SETCOL(btheme->tipo.shade2,  0x70, 0x70, 0x70, 100);
	SETCOL(btheme->tipo.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tipo.vertex_select, 0xff, 0xff, 0x70, 255);
	SETCOL(btheme->tipo.hilite, 0x60, 0xc0, 0x40, 255); 
	btheme->tipo.vertex_size= 3;

	/* space file */
	/* to have something initialized */
	btheme->tfile= btheme->tv3d;
	SETCOL(btheme->tfile.back, 	128, 128, 128, 255);
	SETCOL(btheme->tfile.text, 	0, 0, 0, 255);
	SETCOL(btheme->tfile.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tfile.header, 182, 182, 182, 255);
	SETCOL(btheme->tfile.hilite, 0xA0, 0xA0, 0xD0, 255); // selected files

	
	/* space action */
	btheme->tact= btheme->tv3d;
	SETCOL(btheme->tact.back, 	116, 116, 116, 255);
	SETCOL(btheme->tact.text, 	0, 0, 0, 255);
	SETCOL(btheme->tact.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tact.header, 182, 182, 182, 255);
	SETCOL(btheme->tact.grid,  94, 94, 94, 255);
	SETCOL(btheme->tact.face,  166, 166, 166, 255);	// RVK
	SETCOL(btheme->tact.shade1,  172, 172, 172, 255);		// sliders
	SETCOL(btheme->tact.shade2,  84, 44, 31, 100);	// bar
	SETCOL(btheme->tact.hilite,  17, 27, 60, 100);	// bar
	SETCOL(btheme->tact.strip_select, 	0xff, 0xff, 0xaa, 255);
	SETCOL(btheme->tact.strip, 0xe4, 0x9c, 0xc6, 255);
	SETCOL(btheme->tact.group, 0x39, 0x7d, 0x1b, 255);
	SETCOL(btheme->tact.group_active, 0x7d, 0xe9, 0x60, 255);

	/* space nla */
	btheme->tnla= btheme->tv3d;
	SETCOL(btheme->tnla.back, 	116, 116, 116, 255);
	SETCOL(btheme->tnla.text, 	0, 0, 0, 255);
	SETCOL(btheme->tnla.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tnla.header, 182, 182, 182, 255);
	SETCOL(btheme->tnla.grid,  94, 94, 94, 255);	
	SETCOL(btheme->tnla.shade1,  172, 172, 172, 255);		// sliders
	SETCOL(btheme->tnla.shade2,  84, 44, 31, 100);	// bar
	SETCOL(btheme->tnla.hilite,  17, 27, 60, 100);	// bar
	SETCOL(btheme->tnla.strip_select, 	0xff, 0xff, 0xaa, 255);
	SETCOL(btheme->tnla.strip, 0xe4, 0x9c, 0xc6, 255);
	
	/* space seq */
	btheme->tseq= btheme->tv3d;
	SETCOL(btheme->tseq.back, 	116, 116, 116, 255);
	SETCOL(btheme->tseq.movie, 	81, 105, 135, 255);
	SETCOL(btheme->tseq.image, 	109, 88, 129, 255);
	SETCOL(btheme->tseq.scene, 	78, 152, 62, 255);
	SETCOL(btheme->tseq.audio, 	46, 143, 143, 255);
	SETCOL(btheme->tseq.effect, 	169, 84, 124, 255);
	SETCOL(btheme->tseq.plugin, 	126, 126, 80, 255);
	SETCOL(btheme->tseq.transition, 162, 95, 111, 255);
	SETCOL(btheme->tseq.meta, 	109, 145, 131, 255);
	

	/* space image */
	btheme->tima= btheme->tv3d;
	SETCOL(btheme->tima.back, 	53, 53, 53, 255);
	SETCOL(btheme->tima.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tima.vertex_select, 0xff, 0xff, 0x70, 255);
	btheme->tima.vertex_size= 2;
	SETCOL(btheme->tima.face,   0, 50, 150, 40);
	SETCOL(btheme->tima.face_select, 200, 100, 200, 80);

	/* space imageselect */
	btheme->timasel= btheme->tv3d;
	SETCOL(btheme->timasel.active, 	195, 195, 195, 255); /* active tile */
	SETCOL(btheme->timasel.grid,  94, 94, 94, 255); /* active file text */
	SETCOL(btheme->timasel.back, 	110, 110, 110, 255);
	SETCOL(btheme->timasel.header,	195, 195, 195, 255);	
	SETCOL(btheme->timasel.shade1,  94, 94, 94, 255);	/* bar */
	SETCOL(btheme->timasel.shade2,  172, 172, 172, 255); /* sliders */
	SETCOL(btheme->timasel.hilite,  17, 27, 60, 100);	/* selected tile */
	SETCOL(btheme->timasel.text, 	0, 0, 0, 255);
	SETCOL(btheme->timasel.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->timasel.panel, 	132, 132, 132, 255);

	/* space text */
	btheme->text= btheme->tv3d;
	SETCOL(btheme->text.back, 	153, 153, 153, 255);
	SETCOL(btheme->text.shade1, 	143, 143, 143, 255);
	SETCOL(btheme->text.shade2, 	0xc6, 0x77, 0x77, 255);
	SETCOL(btheme->text.hilite, 	255, 0, 0, 255);
	
	/* syntax highlighting */
	SETCOL(btheme->text.syntaxn,	0, 0, 200, 255);	/* Numbers  Blue*/
	SETCOL(btheme->text.syntaxl,	100, 0, 0, 255);	/* Strings  red */
	SETCOL(btheme->text.syntaxc,	0, 100, 50, 255);	/* Comments greenish */
	SETCOL(btheme->text.syntaxv,	95, 95, 0, 255);	/* Special */
	SETCOL(btheme->text.syntaxb,	128, 0, 80, 255);	/* Builtin, red-purple */
	
	/* space oops */
	btheme->toops= btheme->tv3d;
	SETCOL(btheme->toops.back, 	153, 153, 153, 255);

	/* space info */
	btheme->tinfo= btheme->tv3d;
	SETCOL(btheme->tinfo.back, 	153, 153, 153, 255);

	/* space sound */
	btheme->tsnd= btheme->tv3d;
	SETCOL(btheme->tsnd.back, 	153, 153, 153, 255);
	SETCOL(btheme->tsnd.shade1,  173, 173, 173, 255);		// sliders
	SETCOL(btheme->tsnd.grid, 140, 140, 140, 255);
	
	/* space time */
	btheme->ttime= btheme->tsnd;	// same as sound space
	
	/* space node, re-uses syntax color storage */
	btheme->tnode= btheme->tv3d;
	SETCOL(btheme->tnode.edge_select, 255, 255, 255, 255);
	SETCOL(btheme->tnode.syntaxl, 150, 150, 150, 255);	/* TH_NODE, backdrop */
	SETCOL(btheme->tnode.syntaxn, 129, 131, 144, 255);	/* in/output */
	SETCOL(btheme->tnode.syntaxb, 127,127,127, 255);	/* operator */
	SETCOL(btheme->tnode.syntaxv, 142, 138, 145, 255);	/* generator */
	SETCOL(btheme->tnode.syntaxc, 120, 145, 120, 255);	/* group */

}

char *BIF_ThemeColorsPup(int spacetype)
{
	char *cp= MEM_callocN(32*64, "theme pup");
	char *str = cp;
	
	if(spacetype==0) {
		str += sprintf(str, "Outline %%x%d|", TH_BUT_OUTLINE);
		str += sprintf(str, "Neutral %%x%d|", TH_BUT_NEUTRAL);
		str += sprintf(str, "Action %%x%d|", TH_BUT_ACTION);
		str += sprintf(str, "Setting %%x%d|", TH_BUT_SETTING);
		str += sprintf(str, "Special Setting 1%%x%d|", TH_BUT_SETTING1);
		str += sprintf(str, "Special Setting 2 %%x%d|", TH_BUT_SETTING2);
		str += sprintf(str, "Number Input %%x%d|", TH_BUT_NUM);
		str += sprintf(str, "Text Input %%x%d|", TH_BUT_TEXTFIELD);
		str += sprintf(str, "Text Input Highlight %%x%d|", TH_BUT_TEXTFIELD_HI);
		str += sprintf(str, "Popup %%x%d|", TH_BUT_POPUP);
		str += sprintf(str, "Text %%x%d|", TH_BUT_TEXT);
		str += sprintf(str, "Text Highlight %%x%d|", TH_BUT_TEXT_HI);
		str += sprintf(str, "%%l|");
		str += sprintf(str, "Menu Background %%x%d|", TH_MENU_BACK);
		str += sprintf(str, "Menu Item %%x%d|", TH_MENU_ITEM);
		str += sprintf(str, "Menu Item Highlight %%x%d|", TH_MENU_HILITE);
		str += sprintf(str, "Menu Text %%x%d|", TH_MENU_TEXT);
		str += sprintf(str, "Menu Text Highlight %%x%d|", TH_MENU_TEXT_HI);
		str += sprintf(str, "%%l|");
		str += sprintf(str, "Drawtype %%x%d|", TH_BUT_DRAWTYPE);
		str += sprintf(str, "%%l|");
		str += sprintf(str, "Icon File %%x%d|", TH_ICONFILE);
	}
	else {
		// first defaults for each space
		str += sprintf(str, "Background %%x%d|", TH_BACK);
		str += sprintf(str, "Text %%x%d|", TH_TEXT);
		str += sprintf(str, "Text Highlight %%x%d|", TH_TEXT_HI);
		str += sprintf(str, "Header %%x%d|", TH_HEADER);
		
		switch(spacetype) {
		case SPACE_VIEW3D:
			str += sprintf(str, "Panel %%x%d|", TH_PANEL);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "Wire %%x%d|", TH_WIRE);
			str += sprintf(str, "Lamp %%x%d|", TH_LAMP);
			str += sprintf(str, "Object Selected %%x%d|", TH_SELECT);
			str += sprintf(str, "Object Active %%x%d|", TH_ACTIVE);
			str += sprintf(str, "Object Grouped %%x%d|", TH_GROUP);
			str += sprintf(str, "Object Grouped Active %%x%d|", TH_GROUP_ACTIVE);
			str += sprintf(str, "Transform %%x%d|", TH_TRANSFORM);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Vertex %%x%d|", TH_VERTEX);
			str += sprintf(str, "Vertex Selected %%x%d|", TH_VERTEX_SELECT);
			str += sprintf(str, "Vertex Size %%x%d|", TH_VERTEX_SIZE);
			str += sprintf(str, "Edge Selected %%x%d|", TH_EDGE_SELECT);
			str += sprintf(str, "Edge Seam %%x%d|", TH_EDGE_SEAM);
			str += sprintf(str, "Edge Sharp %%x%d|", TH_EDGE_SHARP);
			str += sprintf(str, "Edge UV Face Select %%x%d|", TH_EDGE_FACESEL);
			str += sprintf(str, "Face (transp) %%x%d|", TH_FACE);
			str += sprintf(str, "Face Selected (transp) %%x%d|", TH_FACE_SELECT);
			str += sprintf(str, "Face Dot Selected %%x%d|", TH_FACE_DOT);
			str += sprintf(str, "Face Dot Size %%x%d|", TH_FACEDOT_SIZE);
			str += sprintf(str, "Active Vert/Edge/Face %%x%d|", TH_EDITMESH_ACTIVE);
			str += sprintf(str, "Normal %%x%d|", TH_NORMAL);
			str += sprintf(str, "Bone Solid %%x%d|", TH_BONE_SOLID);
			str += sprintf(str, "Bone Pose %%x%d|", TH_BONE_POSE);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_IPO:
			str += sprintf(str, "Panel %%x%d|", TH_PANEL);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "Window Sliders %%x%d|", TH_SHADE1);
			str += sprintf(str, "Ipo Channels %%x%d|", TH_SHADE2);
			str += sprintf(str, "Vertex %%x%d|", TH_VERTEX);
			str += sprintf(str, "Vertex Selected %%x%d|", TH_VERTEX_SELECT);
			str += sprintf(str, "Vertex Size %%x%d|", TH_VERTEX_SIZE);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_FILE:
			str += sprintf(str, "Selected file %%x%d", TH_HILITE);
			break;
		case SPACE_NLA:
			//str += sprintf(str, "Panel %%x%d|", TH_PANEL);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "View Sliders %%x%d|", TH_SHADE1);
			str += sprintf(str, "Bars %%x%d|", TH_SHADE2);
			str += sprintf(str, "Bars selected %%x%d|", TH_HILITE);
			str += sprintf(str, "Strips %%x%d|", TH_STRIP);
			str += sprintf(str, "Strips selected %%x%d|", TH_STRIP_SELECT);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_ACTION:
			//str += sprintf(str, "Panel %%x%d|", TH_PANEL);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "RVK Sliders %%x%d|", TH_FACE);
			str += sprintf(str, "View Sliders %%x%d|", TH_SHADE1);
			str += sprintf(str, "Channels %%x%d|", TH_SHADE2);
			str += sprintf(str, "Channels Selected %%x%d|", TH_HILITE);
			str += sprintf(str, "Channel Group %%x%d|", TH_GROUP);
			str += sprintf(str, "Active Channel Group %%x%d|", TH_GROUP_ACTIVE);
			str += sprintf(str, "Long Key %%x%d|", TH_STRIP);
			str += sprintf(str, "Long Key selected %%x%d|", TH_STRIP_SELECT);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_IMAGE:
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Vertex %%x%d|", TH_VERTEX);
			str += sprintf(str, "Vertex Selected %%x%d|", TH_VERTEX_SELECT);
			str += sprintf(str, "Vertex Size %%x%d|", TH_VERTEX_SIZE);
			str += sprintf(str, "Face %%x%d|", TH_FACE);
			str += sprintf(str, "Face Selected %%x%d", TH_FACE_SELECT);
			str += sprintf(str, "Face Dot Selected %%x%d|", TH_FACE_DOT);
			str += sprintf(str, "Face Dot Size %%x%d|", TH_FACEDOT_SIZE);
			str += sprintf(str, "Active Vert/Edge/Face %%x%d|", TH_EDITMESH_ACTIVE);
			break;
		case SPACE_SEQ:
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "Window Sliders %%x%d|", TH_SHADE1);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Movie Strip %%x%d|", TH_SEQ_MOVIE);
			str += sprintf(str, "Image Strip %%x%d|", TH_SEQ_IMAGE);
			str += sprintf(str, "Scene Strip %%x%d|", TH_SEQ_SCENE);
			str += sprintf(str, "Audio Strip %%x%d|", TH_SEQ_AUDIO);
			str += sprintf(str, "Effect Strip %%x%d|", TH_SEQ_EFFECT);
			str += sprintf(str, "Plugin Strip %%x%d|", TH_SEQ_PLUGIN);
			str += sprintf(str, "Transition Strip %%x%d|", TH_SEQ_TRANSITION);
			str += sprintf(str, "Meta Strip %%x%d|", TH_SEQ_META);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_SOUND:
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "Window Slider %%x%d|", TH_SHADE1);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_BUTS:
			str += sprintf(str, "Panel %%x%d|", TH_PANEL);
			break;
		case SPACE_IMASEL:
			str += sprintf(str, "Tiles %%x%d|", TH_PANEL);
			str += sprintf(str, "Scrollbar %%x%d|", TH_SHADE1);
			str += sprintf(str, "Scroll Handle %%x%d|", TH_SHADE2);
			str += sprintf(str, "Selected File %%x%d|", TH_HILITE);
			str += sprintf(str, "Active File %%x%d|", TH_ACTIVE);
			str += sprintf(str, "Active File Text%%x%d|", TH_GRID);			
			break;
		case SPACE_TEXT:
			str += sprintf(str, "Scroll Bar %%x%d|", TH_SHADE1);
			str += sprintf(str, "Selected Text %%x%d|", TH_SHADE2);
			str += sprintf(str, "Cursor %%x%d|", TH_HILITE);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Syntax Builtin %%x%d|", TH_SYNTAX_B);
			str += sprintf(str, "Syntax Special %%x%d|", TH_SYNTAX_V);
			str += sprintf(str, "Syntax Comment %%x%d|", TH_SYNTAX_C);
			str += sprintf(str, "Syntax Strings %%x%d|", TH_SYNTAX_L);
			str += sprintf(str, "Syntax Numbers %%x%d|", TH_SYNTAX_N);
			break;
		case SPACE_TIME:
			str += sprintf(str, "Grid %%x%d|", TH_GRID);
			str += sprintf(str, "Current Frame %%x%d", TH_CFRAME);
			break;
		case SPACE_NODE:
			str += sprintf(str, "Wires %%x%d|", TH_WIRE);
			str += sprintf(str, "Wires Select %%x%d|", TH_EDGE_SELECT);
			str += sprintf(str, "%%l|");
			str += sprintf(str, "Node Backdrop %%x%d|", TH_NODE);
			str += sprintf(str, "In/Out Node %%x%d|", TH_NODE_IN_OUT);
			str += sprintf(str, "Convertor Node %%x%d|", TH_NODE_CONVERTOR);
			str += sprintf(str, "Operator Node %%x%d|", TH_NODE_OPERATOR);
			str += sprintf(str, "Group Node %%x%d|", TH_NODE_GROUP);
			break;
		}
	}
	return cp;
}

char *BIF_ThemeColorSetsPup (short inc_custom)
{
	DynStr *pupds= BLI_dynstr_new();
	char *str;
	char buf[48];
	int i;
	
	/* add title first (and the "default" entry) */
	BLI_dynstr_append(pupds, "Bone Color Set%t|Default Colors%x0|");
	
	/* loop through set indices, adding them */
	for (i=1; i<21; i++) {
		sprintf(buf, "%d - Theme Color Set%%x%d|", i, i);
		BLI_dynstr_append(pupds, buf);
	}
	
	/* add the 'custom' entry */
	if (inc_custom)
		BLI_dynstr_append(pupds, "Custom Set %x-1");
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

void BIF_SetTheme(ScrArea *sa)
{
	if(sa==NULL) {	// called for safety, when delete themes
		theme_active= U.themes.first;
		theme_spacetype= SPACE_VIEW3D;
	}
	else {
		// later on, a local theme can be found too
		theme_active= U.themes.first;
		theme_spacetype= sa->spacetype;
	
	}
}

// for space windows only
void BIF_ThemeColor(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor3ub(cp[0], cp[1], cp[2]);

}

// plus alpha
void BIF_ThemeColor4(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor4ub(cp[0], cp[1], cp[2], cp[3]);

}

// set the color with offset for shades
void BIF_ThemeColorShade(int colorid, int offset)
{
	int r, g, b;
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r= offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g= offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b= offset + (int) cp[2];
	CLAMP(b, 0, 255);
	//glColor3ub(r, g, b);
	glColor4ub(r, g, b, cp[3]);
}
void BIF_ThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
	int r, g, b, a;
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r= coloffset + (int) cp[0];
	CLAMP(r, 0, 255);
	g= coloffset + (int) cp[1];
	CLAMP(g, 0, 255);
	b= coloffset + (int) cp[2];
	CLAMP(b, 0, 255);
	a= alphaoffset + (int) cp[3];
	CLAMP(a, 0, 255);
	glColor4ub(r, g, b, a);
}

// blend between to theme colors, and set it
void BIF_ThemeColorBlend(int colorid1, int colorid2, float fac)
{
	int r, g, b;
	char *cp1, *cp2;
	
	cp1= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	if(fac<0.0) fac=0.0; else if(fac>1.0) fac= 1.0;
	r= floor((1.0-fac)*cp1[0] + fac*cp2[0]);
	g= floor((1.0-fac)*cp1[1] + fac*cp2[1]);
	b= floor((1.0-fac)*cp1[2] + fac*cp2[2]);
	
	glColor3ub(r, g, b);
}

// blend between to theme colors, shade it, and set it
void BIF_ThemeColorBlendShade(int colorid1, int colorid2, float fac, int offset)
{
	int r, g, b;
	char *cp1, *cp2;
	
	cp1= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	if(fac<0.0) fac=0.0; else if(fac>1.0) fac= 1.0;
	r= offset+floor((1.0-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floor((1.0-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floor((1.0-fac)*cp1[2] + fac*cp2[2]);
	
	r= r<0?0:(r>255?255:r);
	g= g<0?0:(g>255?255:g);
	b= b<0?0:(b>255?255:b);
	
	glColor3ub(r, g, b);
}

// get individual values, not scaled
float BIF_GetThemeValuef(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((float)cp[0]);

}

// get individual values, not scaled
int BIF_GetThemeValue(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((int) cp[0]);

}


// get the color, range 0.0-1.0
void BIF_GetThemeColor3fv(int colorid, float *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= ((float)cp[0])/255.0;
	col[1]= ((float)cp[1])/255.0;
	col[2]= ((float)cp[2])/255.0;
}

// get the color, in char pointer
void BIF_GetThemeColor3ubv(int colorid, char *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
}

// get the color, in char pointer
void BIF_GetThemeColor4ubv(int colorid, char *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
	col[3]= cp[3];
}

void BIF_GetThemeColorType4ubv(int colorid, int spacetype, char *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
	col[3]= cp[3];
}

// blends and shades between two char color pointers
void BIF_ColorPtrBlendShade3ubv(char *cp1, char *cp2, float fac, int offset)
{
	int r, g, b;
	
	if(fac<0.0) fac=0.0; else if(fac>1.0) fac= 1.0;
	r= offset+floor((1.0-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floor((1.0-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floor((1.0-fac)*cp1[2] + fac*cp2[2]);
	
	r= r<0?0:(r>255?255:r);
	g= g<0?0:(g>255?255:g);
	b= b<0?0:(b>255?255:b);
	
	glColor3ub(r, g, b);
}

// get a 3 byte color, blended and shaded between two other char color pointers
void BIF_GetColorPtrBlendShade3ubv(char *cp1, char *cp2, char *col, float fac, int offset)
{
	int r, g, b;
	
	if(fac<0.0) fac=0.0; else if(fac>1.0) fac= 1.0;
	r= offset+floor((1.0-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floor((1.0-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floor((1.0-fac)*cp1[2] + fac*cp2[2]);
	
	r= r<0?0:(r>255?255:r);
	g= g<0?0:(g>255?255:g);
	b= b<0?0:(b>255?255:b);
	
	col[0] = r;
	col[1] = g;
	col[2] = b;
}
