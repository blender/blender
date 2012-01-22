/*
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/editors/interface/resources.c
 *  \ingroup edinterface
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_curve_types.h"
#include "DNA_userdef_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_texture.h"


#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "interface_intern.h"

/* global for themes */
typedef void (*VectorDrawFunc)(int x, int y, int w, int h, float alpha);

static bTheme *theme_active=NULL;
static int theme_spacetype= SPACE_VIEW3D;
static int theme_regionid= RGN_TYPE_WINDOW;

void ui_resources_init(void)
{
	UI_icons_init(BIFICONID_LAST);
}

void ui_resources_free(void)
{
	UI_icons_free();
}


/* ******************************************************** */
/*    THEMES */
/* ******************************************************** */

const unsigned char *UI_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid)
{
	ThemeSpace *ts= NULL;
	static char error[4]={240, 0, 240, 255};
	static char alert[4]={240, 60, 60, 255};
	static char headerdesel[4]={0,0,0,255};
	
	const char *cp= error;
	
	if(btheme) {
	
		// first check for ui buttons theme
		if(colorid < TH_THEMEUI) {
		
			switch(colorid) {

			case TH_REDALERT:
				cp= alert; break;
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
			case SPACE_TEXT:
				ts= &btheme->text;
				break;
			case SPACE_OUTLINER:
				ts= &btheme->toops;
				break;
			case SPACE_INFO:
				ts= &btheme->tinfo;
				break;
			case SPACE_USERPREF:
				ts= &btheme->tuserpref;
				break;
			case SPACE_CONSOLE:
				ts= &btheme->tconsole;
				break;
			case SPACE_TIME:
				ts= &btheme->ttime;
				break;
			case SPACE_NODE:
				ts= &btheme->tnode;
				break;
			case SPACE_LOGIC:
				ts= &btheme->tlogic;
				break;
			case SPACE_CLIP:
				ts= &btheme->tclip;
				break;
			default:
				ts= &btheme->tv3d;
				break;
			}
			
			switch(colorid) {
			case TH_BACK:
				if(theme_regionid==RGN_TYPE_WINDOW)
					cp= ts->back;
				else if(theme_regionid==RGN_TYPE_CHANNELS)
					cp= ts->list;
				else if(theme_regionid==RGN_TYPE_HEADER)
					cp= ts->header;
				else
					cp= ts->button; 
				break;
			case TH_TEXT:
				if(theme_regionid==RGN_TYPE_WINDOW)
					cp= ts->text; 
				else if(theme_regionid==RGN_TYPE_CHANNELS)
					cp= ts->list_text;
				else if(theme_regionid==RGN_TYPE_HEADER)
					cp= ts->header_text;
				else
					cp= ts->button_text; 
				break;
			case TH_TEXT_HI:
				if(theme_regionid==RGN_TYPE_WINDOW)
					cp= ts->text_hi;
				else if(theme_regionid==RGN_TYPE_CHANNELS)
					cp= ts->list_text_hi;
				else if(theme_regionid==RGN_TYPE_HEADER)
					cp= ts->header_text_hi;
				else
					cp= ts->button_text_hi; 
				break;
			case TH_TITLE:
				if(theme_regionid==RGN_TYPE_WINDOW)
					cp= ts->title;
				else if(theme_regionid==RGN_TYPE_CHANNELS)
					cp= ts->list_title;
				else if(theme_regionid==RGN_TYPE_HEADER)
					cp= ts->header_title;
				else
					cp= ts->button_title; 
				break;
				
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
			case TH_HEADER_TEXT:
				cp= ts->header_text; break;
			case TH_HEADER_TEXT_HI:
				cp= ts->header_text_hi; break;
				
			case TH_PANEL:
				cp= ts->panel; break;
			case TH_PANEL_TEXT:
				cp= ts->panel_text; break;
			case TH_PANEL_TEXT_HI:
				cp= ts->panel_text_hi; break;
				
			case TH_BUTBACK:
				cp= ts->button; break;
			case TH_BUTBACK_TEXT:
				cp= ts->button_text; break;
			case TH_BUTBACK_TEXT_HI:
				cp= ts->button_text_hi; break;
				
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
			case TH_SPEAKER:
				cp= ts->speaker; break;
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
			case TH_OUTLINE_WIDTH:
				cp= &ts->outline_width; break;
			case TH_EDGE:
				cp= ts->edge; break;
			case TH_EDGE_SELECT:
				cp= ts->edge_select; break;
			case TH_EDGE_SEAM:
				cp= ts->edge_seam; break;
			case TH_EDGE_SHARP:
				cp= ts->edge_sharp; break;
			case TH_EDGE_CREASE:
				cp= ts->edge_crease; break;
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
			case TH_DRAWEXTRA_EDGELEN:
				cp= ts->extra_edge_len; break;
			case TH_DRAWEXTRA_FACEAREA:
				cp= ts->extra_face_area; break;
			case TH_DRAWEXTRA_FACEANG:
				cp= ts->extra_face_angle; break;
			case TH_NORMAL:
				cp= ts->normal; break;
			case TH_VNORMAL:
				cp= ts->vertex_normal; break;
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
			case TH_NURB_ULINE:
				cp= ts->nurb_uline; break;
			case TH_NURB_VLINE:
				cp= ts->nurb_vline; break;
			case TH_NURB_SEL_ULINE:
				cp= ts->nurb_sel_uline; break;
			case TH_NURB_SEL_VLINE:
				cp= ts->nurb_sel_vline; break;
			case TH_ACTIVE_SPLINE:
				cp= ts->act_spline; break;
			case TH_LASTSEL_POINT:
				cp= ts->lastsel_point; break;
			case TH_HANDLE_FREE:
				cp= ts->handle_free; break;
			case TH_HANDLE_AUTO:
				cp= ts->handle_auto; break;
			case TH_HANDLE_AUTOCLAMP:
				cp= ts->handle_auto_clamped; break;
			case TH_HANDLE_VECT:
				cp= ts->handle_vect; break;
			case TH_HANDLE_ALIGN:
				cp= ts->handle_align; break;
			case TH_HANDLE_SEL_FREE:
				cp= ts->handle_sel_free; break;
			case TH_HANDLE_SEL_AUTO:
				cp= ts->handle_sel_auto; break;
			case TH_HANDLE_SEL_AUTOCLAMP:
				cp= ts->handle_sel_auto_clamped; break;
			case TH_HANDLE_SEL_VECT:
				cp= ts->handle_sel_vect; break;
			case TH_HANDLE_SEL_ALIGN:
				cp= ts->handle_sel_align; break;
		
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
			case TH_NODE_CURVING:
				cp= &ts->noodle_curving; break;

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
				
			case TH_CONSOLE_OUTPUT:
				cp= ts->console_output; break;
			case TH_CONSOLE_INPUT:
				cp= ts-> console_input; break;
			case TH_CONSOLE_INFO:
				cp= ts->console_info; break;
			case TH_CONSOLE_ERROR:
				cp= ts->console_error; break;
			case TH_CONSOLE_CURSOR:
				cp= ts->console_cursor; break;

			case TH_HANDLE_VERTEX:
				cp= ts->handle_vertex;
				break;
			case TH_HANDLE_VERTEX_SELECT:
				cp= ts->handle_vertex_select;
				break;
			case TH_HANDLE_VERTEX_SIZE:
				cp= &ts->handle_vertex_size;
				break;
				
			case TH_DOPESHEET_CHANNELOB:
				cp= ts->ds_channel;
				break;
			case TH_DOPESHEET_CHANNELSUBOB:
				cp= ts->ds_subchannel;
				break;	
					
			case TH_PREVIEW_BACK:
				cp= ts->preview_back;
				break;	

			case TH_STITCH_PREVIEW_FACE:
				cp = ts->preview_stitch_face;
				break;

			case TH_STITCH_PREVIEW_EDGE:
				cp = ts->preview_stitch_edge;
				break;

			case TH_STITCH_PREVIEW_VERT:
				cp = ts->preview_stitch_vert;
				break;

			case TH_STITCH_PREVIEW_STITCHABLE:
				cp = ts->preview_stitch_stitchable;
				break;

			case TH_STITCH_PREVIEW_UNSTITCHABLE:
				cp = ts->preview_stitch_unstitchable;
				break;
			case TH_STITCH_PREVIEW_ACTIVE:
				cp = ts->preview_stitch_active;
				break;
			case TH_MARKER_OUTLINE:
				cp= ts->marker_outline; break;
			case TH_MARKER:
				cp= ts->marker; break;
			case TH_ACT_MARKER:
				cp= ts->act_marker; break;
			case TH_SEL_MARKER:
				cp= ts->sel_marker; break;
			case TH_BUNDLE_SOLID:
				cp= ts->bundle_solid; break;
			case TH_DIS_MARKER:
				cp= ts->dis_marker; break;
			case TH_PATH_BEFORE:
				cp= ts->path_before; break;
			case TH_PATH_AFTER:
				cp= ts->path_after; break;
			case TH_CAMERA_PATH:
				cp= ts->camera_path; break;
			case TH_LOCK_MARKER:
				cp= ts->lock_marker; break;
			
			case TH_MATCH:
				cp= ts->match;
				break;
				
			case TH_SELECT_HIGHLIGHT:
				cp= ts->selected_highlight;
				break;
			}
		}
	}
	
	return (unsigned char *)cp;
}


#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;
#define SETCOLF(col, r, g, b, a)  col[0]=r*255; col[1]=g*255; col[2]= b*255; col[3]= a*255;
#define SETCOLTEST(col, r, g, b, a)  if(col[3]==0) {col[0]=r; col[1]=g; col[2]= b; col[3]= a;}

/* use this call to init new bone color sets in Theme */
static void ui_theme_init_boneColorSets(bTheme *btheme)
{
	int i;
	
	/* define default color sets - currently we only define 15 of these, though that should be ample */
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
	
	/* reset flags too */
	for (i = 0; i < 20; i++)
		btheme->tarm[i].flag = 0;
}

/* use this call to init new variables in themespace, if they're same for all */
static void ui_theme_init_new_do(ThemeSpace *ts)
{
	SETCOLTEST(ts->header_text,		0, 0, 0, 255);
	SETCOLTEST(ts->header_title,	0, 0, 0, 255);
	SETCOLTEST(ts->header_text_hi,	255, 255, 255, 255);
	
	SETCOLTEST(ts->panel_text,		0, 0, 0, 255);
	SETCOLTEST(ts->panel_title,		0, 0, 0, 255);
	SETCOLTEST(ts->panel_text_hi,	255, 255, 255, 255);
	
	SETCOLTEST(ts->button,			145, 145, 145, 245);
	SETCOLTEST(ts->button_title,	0, 0, 0, 255);
	SETCOLTEST(ts->button_text,		0, 0, 0, 255);
	SETCOLTEST(ts->button_text_hi,	255, 255, 255, 255);
	
	SETCOLTEST(ts->list,			165, 165, 165, 255);
	SETCOLTEST(ts->list_title,		0, 0, 0, 255);
	SETCOLTEST(ts->list_text,		0, 0, 0, 255);
	SETCOLTEST(ts->list_text_hi,	255, 255, 255, 255);
}

static void ui_theme_init_new(bTheme *btheme)
{
	ui_theme_init_new_do(&btheme->tbuts);
	ui_theme_init_new_do(&btheme->tv3d);
	ui_theme_init_new_do(&btheme->tfile);
	ui_theme_init_new_do(&btheme->tipo);
	ui_theme_init_new_do(&btheme->tinfo);
	ui_theme_init_new_do(&btheme->tact);
	ui_theme_init_new_do(&btheme->tnla);
	ui_theme_init_new_do(&btheme->tseq);
	ui_theme_init_new_do(&btheme->tima);
	ui_theme_init_new_do(&btheme->text);
	ui_theme_init_new_do(&btheme->toops);
	ui_theme_init_new_do(&btheme->ttime);
	ui_theme_init_new_do(&btheme->tnode);
	ui_theme_init_new_do(&btheme->tlogic);
	ui_theme_init_new_do(&btheme->tuserpref);
	ui_theme_init_new_do(&btheme->tconsole);
	ui_theme_init_new_do(&btheme->tclip);
	
}


/* initialize default theme
   Note: when you add new colors, created & saved themes need initialized
   use function below, init_userdef_do_versions() 
*/
void ui_theme_init_default(void)
{
	bTheme *btheme;
	
	/* we search for the theme with name Default */
	for(btheme= U.themes.first; btheme; btheme= btheme->next) {
		if(strcmp("Default", btheme->name)==0) break;
	}
	
	if(btheme==NULL) {
		btheme= MEM_callocN(sizeof(bTheme), "theme");
		BLI_addtail(&U.themes, btheme);
		strcpy(btheme->name, "Default");
	}
	
	UI_SetTheme(0, 0);	// make sure the global used in this file is set

	/* UI buttons */
	ui_widget_color_init(&btheme->tui);
	btheme->tui.iconfile[0]= 0;
	
	/* Bone Color Sets */
	ui_theme_init_boneColorSets(btheme);
	
	/* common (new) variables */
	ui_theme_init_new(btheme);
	
	/* space view3d */
	SETCOLF(btheme->tv3d.back,       0.225, 0.225, 0.225, 1.0);
	SETCOL(btheme->tv3d.text,       0, 0, 0, 255);
	SETCOL(btheme->tv3d.text_hi, 255, 255, 255, 255);
	
	SETCOLF(btheme->tv3d.header,	0.45, 0.45, 0.45, 1.0);
	SETCOLF(btheme->tv3d.button,	0.45, 0.45, 0.45, 1.0);
	SETCOL(btheme->tv3d.panel,      165, 165, 165, 127);
	
	SETCOL(btheme->tv3d.shade1,  160, 160, 160, 100);
	SETCOL(btheme->tv3d.shade2,  0x7f, 0x70, 0x70, 100);

	SETCOLF(btheme->tv3d.grid,     0.251, 0.251, 0.251, 1.0);
	SETCOL(btheme->tv3d.wire,       0x0, 0x0, 0x0, 255);
	SETCOL(btheme->tv3d.lamp,       0, 0, 0, 40);
	SETCOL(btheme->tv3d.speaker,    0, 0, 0, 255);
	SETCOL(btheme->tv3d.select, 241, 88, 0, 255);
	SETCOL(btheme->tv3d.active, 255, 170, 64, 255);
	SETCOL(btheme->tv3d.group,      8, 48, 8, 255);
	SETCOL(btheme->tv3d.group_active, 85, 187, 85, 255);
	SETCOL(btheme->tv3d.transform, 0xff, 0xff, 0xff, 255);
	SETCOL(btheme->tv3d.vertex, 0, 0, 0, 255);
	SETCOL(btheme->tv3d.vertex_select, 255, 133, 0, 255);
	btheme->tv3d.vertex_size= 3;
	btheme->tv3d.outline_width= 1;
	SETCOL(btheme->tv3d.edge,       0x0, 0x0, 0x0, 255);
	SETCOL(btheme->tv3d.edge_select, 255, 160, 0, 255);
	SETCOL(btheme->tv3d.edge_seam, 219, 37, 18, 255);
	SETCOL(btheme->tv3d.edge_facesel, 75, 75, 75, 255);
	SETCOL(btheme->tv3d.face,       0, 0, 0, 18);
	SETCOL(btheme->tv3d.face_select, 255, 133, 0, 60);
	SETCOL(btheme->tv3d.normal, 0x22, 0xDD, 0xDD, 255);
	SETCOL(btheme->tv3d.vertex_normal, 0x23, 0x61, 0xDD, 255);
	SETCOL(btheme->tv3d.face_dot, 255, 133, 0, 255);
	SETCOL(btheme->tv3d.editmesh_active, 255, 255, 255, 128);
	SETCOLF(btheme->tv3d.edge_crease, 0.8, 0, 0.6, 1.0);
	SETCOL(btheme->tv3d.edge_sharp, 0, 255, 255, 255);
	SETCOL(btheme->tv3d.header_text, 0, 0, 0, 255);
	SETCOL(btheme->tv3d.header_text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tv3d.button_text, 0, 0, 0, 255);
	SETCOL(btheme->tv3d.button_text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tv3d.button_title, 0, 0, 0, 255);
	SETCOL(btheme->tv3d.title, 0, 0, 0, 255);

	btheme->tv3d.facedot_size= 4;

	SETCOL(btheme->tv3d.extra_edge_len, 32, 0, 0, 255);
	SETCOL(btheme->tv3d.extra_face_area, 0, 32, 0, 255);
	SETCOL(btheme->tv3d.extra_face_angle, 0, 0, 128, 255);

	SETCOL(btheme->tv3d.cframe, 0x60, 0xc0,	 0x40, 255);

	SETCOL(btheme->tv3d.nurb_uline, 0x90, 0x90, 0x00, 255);
	SETCOL(btheme->tv3d.nurb_vline, 0x80, 0x30, 0x60, 255);
	SETCOL(btheme->tv3d.nurb_sel_uline, 0xf0, 0xff, 0x40, 255);
	SETCOL(btheme->tv3d.nurb_sel_vline, 0xf0, 0x90, 0xa0, 255);

	SETCOL(btheme->tv3d.handle_free, 0, 0, 0, 255);
	SETCOL(btheme->tv3d.handle_auto, 0x90, 0x90, 0x00, 255);
	SETCOL(btheme->tv3d.handle_vect, 0x40, 0x90, 0x30, 255);
	SETCOL(btheme->tv3d.handle_align, 0x80, 0x30, 0x60, 255);
	SETCOL(btheme->tv3d.handle_sel_free, 0, 0, 0, 255);
	SETCOL(btheme->tv3d.handle_sel_auto, 0xf0, 0xff, 0x40, 255);
	SETCOL(btheme->tv3d.handle_sel_vect, 0x40, 0xc0, 0x30, 255);
	SETCOL(btheme->tv3d.handle_sel_align, 0xf0, 0x90, 0xa0, 255);

	SETCOL(btheme->tv3d.act_spline, 0xdb, 0x25, 0x12, 255);
	SETCOL(btheme->tv3d.lastsel_point,  0xff, 0xff, 0xff, 255);

	SETCOL(btheme->tv3d.bone_solid, 200, 200, 200, 255);
	SETCOL(btheme->tv3d.bone_pose, 80, 200, 255, 80);               // alpha 80 is not meant editable, used for wire+action draw

	SETCOL(btheme->tv3d.bundle_solid, 200, 200, 200, 255);
	SETCOL(btheme->tv3d.camera_path, 0x00, 0x00, 0x00, 255);
	
	/* space buttons */
	/* to have something initialized */
	btheme->tbuts= btheme->tv3d;

	SETCOLF(btheme->tbuts.back, 	0.45, 0.45, 0.45, 1.0);
	SETCOL(btheme->tbuts.panel, 0x82, 0x82, 0x82, 255);

	/* graph editor */
	btheme->tipo= btheme->tv3d;
	SETCOLF(btheme->tipo.back, 	0.42, 0.42, 0.42, 1.0);
	SETCOLF(btheme->tipo.list, 	0.4, 0.4, 0.4, 1.0);
	SETCOL(btheme->tipo.grid, 	94, 94, 94, 255);
	SETCOL(btheme->tipo.panel,  255, 255, 255, 150);
	SETCOL(btheme->tipo.shade1,		150, 150, 150, 100);	/* scrollbars */
	SETCOL(btheme->tipo.shade2,		0x70, 0x70, 0x70, 100);
	SETCOL(btheme->tipo.vertex,		0, 0, 0, 255);
	SETCOL(btheme->tipo.vertex_select, 255, 133, 0, 255);
	SETCOL(btheme->tipo.hilite, 0x60, 0xc0, 0x40, 255); 
	btheme->tipo.vertex_size= 3;

	SETCOL(btheme->tipo.handle_vertex, 		0, 0, 0, 255);
	SETCOL(btheme->tipo.handle_vertex_select, 255, 133, 0, 255);
	SETCOL(btheme->tipo.handle_auto_clamped, 0x99, 0x40, 0x30, 255);
	SETCOL(btheme->tipo.handle_sel_auto_clamped, 0xf0, 0xaf, 0x90, 255);
	btheme->tipo.handle_vertex_size= 4;
	
	SETCOL(btheme->tipo.ds_channel, 	82, 96, 110, 255);
	SETCOL(btheme->tipo.ds_subchannel,	124, 137, 150, 255);
	SETCOL(btheme->tipo.group, 79, 101, 73, 255);
	SETCOL(btheme->tipo.group_active, 135, 177, 125, 255);

	/* dopesheet */
	btheme->tact= btheme->tipo;
	SETCOL(btheme->tact.strip, 			12, 10, 10, 128); 
	SETCOL(btheme->tact.strip_select, 	255, 140, 0, 255); 
	
	/* space nla */
	btheme->tnla= btheme->tact;
	
	/* space file */
	/* to have something initialized */
	btheme->tfile= btheme->tv3d;
	SETCOLF(btheme->tfile.back, 0.3, 0.3, 0.3, 1);
	SETCOLF(btheme->tfile.panel, 0.3, 0.3, 0.3, 1);
	SETCOLF(btheme->tfile.list, 0.4, 0.4, 0.4, 1);
	SETCOL(btheme->tfile.text, 	250, 250, 250, 255);
	SETCOL(btheme->tfile.text_hi, 15, 15, 15, 255);
	SETCOL(btheme->tfile.panel, 145, 145, 145, 255);	// bookmark/ui regions
	SETCOL(btheme->tfile.active, 130, 130, 130, 255); // selected files
	SETCOL(btheme->tfile.hilite, 255, 140, 25, 255); // selected files
	
	SETCOL(btheme->tfile.grid,	250, 250, 250, 255);
	SETCOL(btheme->tfile.image,	250, 250, 250, 255);
	SETCOL(btheme->tfile.movie,	250, 250, 250, 255);
	SETCOL(btheme->tfile.scene,	250, 250, 250, 255);

	
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
	SETCOL(btheme->tima.vertex, 0, 0, 0, 255);
	SETCOL(btheme->tima.vertex_select, 255, 133, 0, 255);
	btheme->tima.vertex_size= 3;
	btheme->tima.facedot_size= 3;
	SETCOL(btheme->tima.face,   255, 255, 255, 10);
	SETCOL(btheme->tima.face_select, 255, 133, 0, 60);
	SETCOL(btheme->tima.editmesh_active, 255, 255, 255, 128);
	SETCOLF(btheme->tima.preview_back, 	0.45, 0.45, 0.45, 1.0);
	SETCOLF(btheme->tima.preview_stitch_face, 0.5, 0.5, 0.0, 0.2);
	SETCOLF(btheme->tima.preview_stitch_edge, 1.0, 0.0, 1.0, 0.2);
	SETCOLF(btheme->tima.preview_stitch_vert, 0.0, 0.0, 1.0, 0.2);
	SETCOLF(btheme->tima.preview_stitch_stitchable, 0.0, 1.0, 0.0, 1.0);
	SETCOLF(btheme->tima.preview_stitch_unstitchable, 1.0, 0.0, 0.0, 1.0);

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
	SETCOLF(btheme->toops.back, 	0.45, 0.45, 0.45, 1.0);
	
	SETCOLF(btheme->toops.match, 	0.2, 0.5, 0.2, 0.3);	/* highlighting search match - soft green*/
	SETCOLF(btheme->toops.selected_highlight, 0.51, 0.53, 0.55, 0.3);

	/* space info */
	btheme->tinfo= btheme->tv3d;
	SETCOLF(btheme->tinfo.back, 	0.45, 0.45, 0.45, 1.0);

	/* space user preferences */
	btheme->tuserpref= btheme->tv3d;
	SETCOLF(btheme->tuserpref.back, 0.45, 0.45, 0.45, 1.0);
	
	/* space console */
	btheme->tconsole= btheme->tv3d;
	SETCOL(btheme->tconsole.back, 0, 0, 0, 255);
	SETCOL(btheme->tconsole.console_output, 96, 128, 255, 255);
	SETCOL(btheme->tconsole.console_input, 255, 255, 255, 255);
	SETCOL(btheme->tconsole.console_info, 0, 170, 0, 255);
	SETCOL(btheme->tconsole.console_error, 220, 96, 96, 255);
	SETCOL(btheme->tconsole.console_cursor, 220, 96, 96, 255);
	
	/* space time */
	btheme->ttime= btheme->tv3d;
	SETCOLF(btheme->ttime.back, 	0.45, 0.45, 0.45, 1.0);
	SETCOLF(btheme->ttime.grid, 	0.36, 0.36, 0.36, 1.0);
	SETCOL(btheme->ttime.shade1,  173, 173, 173, 255);		// sliders
	
	/* space node, re-uses syntax color storage */
	btheme->tnode= btheme->tv3d;
	SETCOL(btheme->tnode.edge_select, 255, 255, 255, 255);
	SETCOL(btheme->tnode.syntaxl, 155, 155, 155, 160);	/* TH_NODE, backdrop */
	SETCOL(btheme->tnode.syntaxn, 100, 100, 100, 255);	/* in/output */
	SETCOL(btheme->tnode.syntaxb, 108, 105, 111, 255);	/* operator */
	SETCOL(btheme->tnode.syntaxv, 104, 106, 117, 255);	/* generator */
	SETCOL(btheme->tnode.syntaxc, 105, 117, 110, 255);	/* group */
	btheme->tnode.noodle_curving = 5;

	/* space logic */
	btheme->tlogic= btheme->tv3d;
	SETCOL(btheme->tlogic.back, 100, 100, 100, 255);
	
	/* space clip */
	btheme->tclip= btheme->tv3d;

	SETCOL(btheme->tclip.marker_outline, 0x00, 0x00, 0x00, 255);
	SETCOL(btheme->tclip.marker, 0x7f, 0x7f, 0x00, 255);
	SETCOL(btheme->tclip.act_marker, 0xff, 0xff, 0xff, 255);
	SETCOL(btheme->tclip.sel_marker, 0xff, 0xff, 0x00, 255);
	SETCOL(btheme->tclip.dis_marker, 0x7f, 0x00, 0x00, 255);
	SETCOL(btheme->tclip.lock_marker, 0x7f, 0x7f, 0x7f, 255);
	SETCOL(btheme->tclip.path_before, 0xff, 0x00, 0x00, 255);
	SETCOL(btheme->tclip.path_after, 0x00, 0x00, 0xff, 255);
	SETCOL(btheme->tclip.grid, 0x5e, 0x5e, 0x5e, 255);
	SETCOL(btheme->tclip.cframe, 0x60, 0xc0, 0x40, 255);
	SETCOL(btheme->tclip.handle_vertex, 0x00, 0x00, 0x00, 0xff);
	SETCOL(btheme->tclip.handle_vertex_select, 0xff, 0xff, 0, 0xff);
	btheme->tclip.handle_vertex_size= 4;
}


void UI_SetTheme(int spacetype, int regionid)
{
	if(spacetype==0) {	// called for safety, when delete themes
		theme_active= U.themes.first;
		theme_spacetype= SPACE_VIEW3D;
		theme_regionid= RGN_TYPE_WINDOW;
	}
	else {
		// later on, a local theme can be found too
		theme_active= U.themes.first;
		theme_spacetype= spacetype;
		theme_regionid= regionid;
	}
}

bTheme *UI_GetTheme()
{
	return U.themes.first;
}

// for space windows only
void UI_ThemeColor(int colorid)
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor3ubv(cp);

}

// plus alpha
void UI_ThemeColor4(int colorid)
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor4ubv(cp);

}

// set the color with offset for shades
void UI_ThemeColorShade(int colorid, int offset)
{
	int r, g, b;
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r= offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g= offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b= offset + (int) cp[2];
	CLAMP(b, 0, 255);
	//glColor3ub(r, g, b);
	glColor4ub(r, g, b, cp[3]);
}
void UI_ThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
	int r, g, b, a;
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
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
void UI_ThemeColorBlend(int colorid1, int colorid2, float fac)
{
	int r, g, b;
	const unsigned char *cp1, *cp2;
	
	cp1= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	r= floorf((1.0f-fac)*cp1[0] + fac*cp2[0]);
	g= floorf((1.0f-fac)*cp1[1] + fac*cp2[1]);
	b= floorf((1.0f-fac)*cp1[2] + fac*cp2[2]);
	
	glColor3ub(r, g, b);
}

// blend between to theme colors, shade it, and set it
void UI_ThemeColorBlendShade(int colorid1, int colorid2, float fac, int offset)
{
	int r, g, b;
	const unsigned char *cp1, *cp2;
	
	cp1= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	r= offset+floorf((1.0f-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floorf((1.0f-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floorf((1.0f-fac)*cp1[2] + fac*cp2[2]);
	
	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);
	
	glColor3ub(r, g, b);
}

// blend between to theme colors, shade it, and set it
void UI_ThemeColorBlendShadeAlpha(int colorid1, int colorid2, float fac, int offset, int alphaoffset)
{
	int r, g, b, a;
	const unsigned char *cp1, *cp2;
	
	cp1= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	r= offset+floorf((1.0f-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floorf((1.0f-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floorf((1.0f-fac)*cp1[2] + fac*cp2[2]);
	a= alphaoffset + floorf((1.0f-fac)*cp1[3] + fac*cp2[3]);
	
	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);
	CLAMP(a, 0, 255);

	glColor4ub(r, g, b, a);
}


// get individual values, not scaled
float UI_GetThemeValuef(int colorid)
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((float)cp[0]);

}

// get individual values, not scaled
int UI_GetThemeValue(int colorid)
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((int) cp[0]);

}


// get the color, range 0.0-1.0
void UI_GetThemeColor3fv(int colorid, float *col)
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= ((float)cp[0])/255.0f;
	col[1]= ((float)cp[1])/255.0f;
	col[2]= ((float)cp[2])/255.0f;
}

// get the color, range 0.0-1.0, complete with shading offset
void UI_GetThemeColorShade3fv(int colorid, int offset, float *col)
{
	int r, g, b;
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	
	r= offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g= offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b= offset + (int) cp[2];
	CLAMP(b, 0, 255);
	
	col[0]= ((float)r)/255.0f;
	col[1]= ((float)g)/255.0f;
	col[2]= ((float)b)/255.0f;
}

// get the color, in char pointer
void UI_GetThemeColor3ubv(int colorid, unsigned char col[3])
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
}

// get the color, in char pointer
void UI_GetThemeColor4ubv(int colorid, unsigned char col[4])
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
	col[3]= cp[3];
}

void UI_GetThemeColorType4ubv(int colorid, int spacetype, char col[4])
{
	const unsigned char *cp;
	
	cp= UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
	col[3]= cp[3];
}

// blends and shades between two char color pointers
void UI_ColorPtrBlendShade3ubv(const unsigned char cp1[3], const unsigned char cp2[3], float fac, int offset)
{
	int r, g, b;
	CLAMP(fac, 0.0f, 1.0f);
	r= offset+floorf((1.0f-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floorf((1.0f-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floorf((1.0f-fac)*cp1[2] + fac*cp2[2]);
	
	r= r<0?0:(r>255?255:r);
	g= g<0?0:(g>255?255:g);
	b= b<0?0:(b>255?255:b);
	
	glColor3ub(r, g, b);
}

void UI_GetColorPtrShade3ubv(const unsigned char cp[3], unsigned char col[3], int offset)
{
	int r, g, b;

	r= offset+(int)cp[0];
	g= offset+(int)cp[1];
	b= offset+(int)cp[2];

	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);

	col[0] = r;
	col[1] = g;
	col[2] = b;
}

// get a 3 byte color, blended and shaded between two other char color pointers
void UI_GetColorPtrBlendShade3ubv(const unsigned char cp1[3], const unsigned char cp2[3], unsigned char col[3], float fac, int offset)
{
	int r, g, b;

	CLAMP(fac, 0.0f, 1.0f);
	r= offset+floor((1.0f-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floor((1.0f-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floor((1.0f-fac)*cp1[2] + fac*cp2[2]);

	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);

	col[0] = r;
	col[1] = g;
	col[2] = b;
}

void UI_ThemeClearColor(int colorid)
{
	float col[3];
	
	UI_GetThemeColor3fv(colorid, col);
	glClearColor(col[0], col[1], col[2], 0.0);
}

void UI_make_axis_color(const unsigned char src_col[3], unsigned char dst_col[3], const char axis)
{
	switch(axis)
	{
		case 'X':
			dst_col[0]= src_col[0]>219?255:src_col[0]+36;
			dst_col[1]= src_col[1]<26?0:src_col[1]-26;
			dst_col[2]= src_col[2]<26?0:src_col[2]-26;
			break;
		case 'Y':
			dst_col[0]= src_col[0]<46?0:src_col[0]-36;
			dst_col[1]= src_col[1]>189?255:src_col[1]+66;
			dst_col[2]= src_col[2]<46?0:src_col[2]-36;
			break;
		case 'Z':
			dst_col[0]= src_col[0]<26?0:src_col[0]-26; 
			dst_col[1]= src_col[1]<26?0:src_col[1]-26; 
			dst_col[2]= src_col[2]>209?255:src_col[2]+46;
			break;
		default:
			BLI_assert(!"invalid axis arg");
	}
}

/* ************************************************************* */

/* patching UserDef struct and Themes */
void init_userdef_do_versions(void)
{
	Main *bmain= G.main;
//	countall();
	
	/* the UserDef struct is not corrected with do_versions() .... ugh! */
	if(U.wheellinescroll == 0) U.wheellinescroll = 3;
	if(U.menuthreshold1==0) {
		U.menuthreshold1= 5;
		U.menuthreshold2= 2;
	}
	if(U.tb_leftmouse==0) {
		U.tb_leftmouse= 5;
		U.tb_rightmouse= 5;
	}
	if(U.mixbufsize==0) U.mixbufsize= 2048;
	if (strcmp(U.tempdir, "/") == 0) {
		BLI_system_temporary_dir(U.tempdir);
	}
	if (U.autokey_mode == 0) {
		/* 'add/replace' but not on */
		U.autokey_mode = 2;
	}
	if (U.savetime <= 0) {
		U.savetime = 1;
// XXX		error(STRINGIFY(BLENDER_STARTUP_FILE)" is buggy, please consider removing it.\n");
	}
	/* transform widget settings */
	if(U.tw_hotspot==0) {
		U.tw_hotspot= 14;
		U.tw_size= 20;			// percentage of window size
		U.tw_handlesize= 16;	// percentage of widget radius
	}
	if(U.pad_rot_angle==0)
		U.pad_rot_angle= 15;

	/* signal for derivedmesh to use colorband */
	/* run incase this was on and is now off in the user prefs [#28096] */
	vDM_ColorBand_store((U.flag & USER_CUSTOM_RANGE) ? (&U.coba_weight):NULL);

	if (bmain->versionfile <= 191) {
		BLI_strncpy(U.plugtexdir, U.textudir, sizeof(U.plugtexdir));
		strcpy(U.sounddir, "/");
	}
	
	/* patch to set Dupli Armature */
	if (bmain->versionfile < 220) {
		U.dupflag |= USER_DUP_ARM;
	}
	
	/* added seam, normal color, undo */
	if (bmain->versionfile <= 234) {
		bTheme *btheme;
		
		U.uiflag |= USER_GLOBALUNDO;
		if (U.undosteps==0) U.undosteps=32;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tv3d.edge_seam[3]==0) {
				SETCOL(btheme->tv3d.edge_seam, 230, 150, 50, 255);
			}
			if(btheme->tv3d.normal[3]==0) {
				SETCOL(btheme->tv3d.normal, 0x22, 0xDD, 0xDD, 255);
			}
			if(btheme->tv3d.vertex_normal[3]==0) {
				SETCOL(btheme->tv3d.vertex_normal, 0x23, 0x61, 0xDD, 255);
			}
			if(btheme->tv3d.face_dot[3]==0) {
				SETCOL(btheme->tv3d.face_dot, 255, 138, 48, 255);
				btheme->tv3d.facedot_size= 4;
			}
		}
	}
	if (bmain->versionfile <= 235) {
		/* illegal combo... */
		if (U.flag & USER_LMOUSESELECT) 
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (bmain->versionfile <= 236) {
		bTheme *btheme;
		/* new space type */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->ttime.back[3]==0) {
				// copied from ui_theme_init_default
				btheme->ttime= btheme->tv3d;
				SETCOLF(btheme->ttime.back, 	0.45, 0.45, 0.45, 1.0);
				SETCOLF(btheme->ttime.grid, 	0.36, 0.36, 0.36, 1.0);
				SETCOL(btheme->ttime.shade1,  173, 173, 173, 255);		// sliders
			}
			if(btheme->text.syntaxn[3]==0) {
				SETCOL(btheme->text.syntaxn,	0, 0, 200, 255);	/* Numbers  Blue*/
				SETCOL(btheme->text.syntaxl,	100, 0, 0, 255);	/* Strings  red */
				SETCOL(btheme->text.syntaxc,	0, 100, 50, 255);	/* Comments greenish */
				SETCOL(btheme->text.syntaxv,	95, 95, 0, 255);	/* Special */
				SETCOL(btheme->text.syntaxb,	128, 0, 80, 255);	/* Builtin, red-purple */
			}
		}
	}
	if (bmain->versionfile <= 237) {
		bTheme *btheme;
		/* bone colors */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tv3d.bone_solid[3]==0) {
				SETCOL(btheme->tv3d.bone_solid, 200, 200, 200, 255);
				SETCOL(btheme->tv3d.bone_pose, 80, 200, 255, 80);
			}
		}
	}
	if (bmain->versionfile <= 238) {
		bTheme *btheme;
		/* bone colors */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tnla.strip[3]==0) {
				SETCOL(btheme->tnla.strip_select, 	0xff, 0xff, 0xaa, 255);
				SETCOL(btheme->tnla.strip, 0xe4, 0x9c, 0xc6, 255);
			}
		}
	}
	if (bmain->versionfile <= 239) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* Lamp theme, check for alpha==0 is safe, then color was never set */
			if(btheme->tv3d.lamp[3]==0) {
				SETCOL(btheme->tv3d.lamp, 	0, 0, 0, 40);
/* TEMPORAL, remove me! (ton) */				
				U.uiflag |= USER_PLAINMENUS;
			}
			
		}
		if(U.obcenter_dia==0) U.obcenter_dia= 6;
	}
	if (bmain->versionfile <= 241) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* Node editor theme, check for alpha==0 is safe, then color was never set */
			if(btheme->tnode.syntaxn[3]==0) {
				/* re-uses syntax color storage */
				btheme->tnode= btheme->tv3d;
				SETCOL(btheme->tnode.edge_select, 255, 255, 255, 255);
				SETCOL(btheme->tnode.syntaxl, 150, 150, 150, 255);	/* TH_NODE, backdrop */
				SETCOL(btheme->tnode.syntaxn, 129, 131, 144, 255);	/* in/output */
				SETCOL(btheme->tnode.syntaxb, 127,127,127, 255);	/* operator */
				SETCOL(btheme->tnode.syntaxv, 142, 138, 145, 255);	/* generator */
				SETCOL(btheme->tnode.syntaxc, 120, 145, 120, 255);	/* group */
			}
			/* Group theme colors */
			if(btheme->tv3d.group[3]==0) {
				SETCOL(btheme->tv3d.group, 0x0C, 0x30, 0x0C, 255);
				SETCOL(btheme->tv3d.group_active, 0x66, 0xFF, 0x66, 255);
			}
			/* Sequence editor theme*/
			if(btheme->tseq.movie[3]==0) {
				SETCOL(btheme->tseq.movie, 	81, 105, 135, 255);
				SETCOL(btheme->tseq.image, 	109, 88, 129, 255);
				SETCOL(btheme->tseq.scene, 	78, 152, 62, 255);
				SETCOL(btheme->tseq.audio, 	46, 143, 143, 255);
				SETCOL(btheme->tseq.effect, 	169, 84, 124, 255);
				SETCOL(btheme->tseq.plugin, 	126, 126, 80, 255);
				SETCOL(btheme->tseq.transition, 162, 95, 111, 255);
				SETCOL(btheme->tseq.meta, 	109, 145, 131, 255);
			}
		}
		
		/* set defaults for 3D View rotating axis indicator */ 
		/* since size can't be set to 0, this indicates it's not saved in startup.blend */
		if (U.rvisize == 0) {
			U.rvisize = 15;
			U.rvibright = 8;
			U.uiflag |= USER_SHOW_ROTVIEWICON;
		}
		
	}
	if (bmain->versionfile <= 242) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* long keyframe color */
			/* check for alpha==0 is safe, then color was never set */
			if(btheme->tact.strip[3]==0) {
				SETCOL(btheme->tv3d.edge_sharp, 255, 32, 32, 255);
				SETCOL(btheme->tact.strip_select, 	0xff, 0xff, 0xaa, 204);
				SETCOL(btheme->tact.strip, 0xe4, 0x9c, 0xc6, 204);
			}
			
			/* IPO-Editor - Vertex Size*/
			if(btheme->tipo.vertex_size == 0) {
				btheme->tipo.vertex_size= 3;
			}
		}
	}
	if (bmain->versionfile <= 243) {
		/* set default number of recently-used files (if not set) */
		if (U.recent_files == 0) U.recent_files = 10;
	}
	if (bmain->versionfile < 245 || (bmain->versionfile == 245 && bmain->subversionfile < 3)) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			SETCOL(btheme->tv3d.editmesh_active, 255, 255, 255, 128);
		}
		if(U.coba_weight.tot==0)
			init_colorband(&U.coba_weight, 1);
	}
	if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 11)) {
		bTheme *btheme;
		for (btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* these should all use the same color */
			SETCOL(btheme->tv3d.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tipo.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tact.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tnla.cframe, 0x60, 0xc0, 0x40, 255);
			SETCOL(btheme->tseq.cframe, 0x60, 0xc0, 0x40, 255);
			//SETCOL(btheme->tsnd.cframe, 0x60, 0xc0, 0x40, 255); Not needed anymore
			SETCOL(btheme->ttime.cframe, 0x60, 0xc0, 0x40, 255);
		}
	}
	if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 13)) {
		bTheme *btheme;
		for (btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* action channel groups (recolor anyway) */
			SETCOL(btheme->tact.group, 0x39, 0x7d, 0x1b, 255);
			SETCOL(btheme->tact.group_active, 0x7d, 0xe9, 0x60, 255);
			
			/* bone custom-color sets */
			if (btheme->tarm[0].solid[3] == 0)
				ui_theme_init_boneColorSets(btheme);
		}
	}
	if ((bmain->versionfile < 245) || (bmain->versionfile == 245 && bmain->subversionfile < 16)) {
		U.flag |= USER_ADD_VIEWALIGNED|USER_ADD_EDITMODE;
	}
	if ((bmain->versionfile < 247) || (bmain->versionfile == 247 && bmain->subversionfile <= 2)) {
		bTheme *btheme;
		
		/* adjust themes */
		for (btheme= U.themes.first; btheme; btheme= btheme->next) {
			char *col;
			
			/* IPO Editor: Handles/Vertices */
			col = btheme->tipo.vertex;
			SETCOL(btheme->tipo.handle_vertex, col[0], col[1], col[2], 255);
			col = btheme->tipo.vertex_select;
			SETCOL(btheme->tipo.handle_vertex_select, col[0], col[1], col[2], 255);
			btheme->tipo.handle_vertex_size= btheme->tipo.vertex_size;
			
			/* Sequence/Image Editor: colors for GPencil text */
			col = btheme->tv3d.bone_pose;
			SETCOL(btheme->tseq.bone_pose, col[0], col[1], col[2], 255);
			SETCOL(btheme->tima.bone_pose, col[0], col[1], col[2], 255);
			col = btheme->tv3d.vertex_select;
			SETCOL(btheme->tseq.vertex_select, col[0], col[1], col[2], 255);
		}
	}
	if (bmain->versionfile < 250) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* this was not properly initialized in 2.45 */
			if(btheme->tima.face_dot[3]==0) {
				SETCOL(btheme->tima.editmesh_active, 255, 255, 255, 128);
				SETCOL(btheme->tima.face_dot, 255, 133, 0, 255);
				btheme->tima.facedot_size= 2;
			}
			
			/* DopeSheet - (Object) Channel color */
			SETCOL(btheme->tact.ds_channel, 	82, 96, 110, 255);
			SETCOL(btheme->tact.ds_subchannel,	124, 137, 150, 255);
			/* DopeSheet - Group Channel color (saner version) */
			SETCOL(btheme->tact.group, 79, 101, 73, 255);
			SETCOL(btheme->tact.group_active, 135, 177, 125, 255);
			
			/* Graph Editor - (Object) Channel color */
			SETCOL(btheme->tipo.ds_channel, 	82, 96, 110, 255);
			SETCOL(btheme->tipo.ds_subchannel,	124, 137, 150, 255);
			/* Graph Editor - Group Channel color */
			SETCOL(btheme->tipo.group, 79, 101, 73, 255);
			SETCOL(btheme->tipo.group_active, 135, 177, 125, 255);
			
			/* Nla Editor - (Object) Channel color */
			SETCOL(btheme->tnla.ds_channel, 	82, 96, 110, 255);
			SETCOL(btheme->tnla.ds_subchannel,	124, 137, 150, 255);
			/* NLA Editor - New Strip colors */
			SETCOL(btheme->tnla.strip, 			12, 10, 10, 128); 
			SETCOL(btheme->tnla.strip_select, 	255, 140, 0, 255);
		}
		
		/* adjust grease-pencil distances */
		U.gp_manhattendist= 1;
		U.gp_euclideandist= 2;
		
		/* adjust default interpolation for new IPO-curves */
		U.ipo_new= BEZT_IPO_BEZ;
	}
	
	if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 1)) {
		bTheme *btheme;

		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			
			/* common (new) variables, it checks for alpha==0 */
			ui_theme_init_new(btheme);

			if(btheme->tui.wcol_num.outline[3]==0)
				ui_widget_color_init(&btheme->tui);
			
			/* Logic editor theme, check for alpha==0 is safe, then color was never set */
			if(btheme->tlogic.syntaxn[3]==0) {
				/* re-uses syntax color storage */
				btheme->tlogic= btheme->tv3d;
				SETCOL(btheme->tlogic.back, 100, 100, 100, 255);
			}

			SETCOLF(btheme->tinfo.back, 0.45, 0.45, 0.45, 1.0);
			SETCOLF(btheme->tuserpref.back, 0.45, 0.45, 0.45, 1.0);
		}
	}

	if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 3)) {
		/* new audio system */
		if(U.audiochannels == 0)
			U.audiochannels = 2;
		if(U.audiodevice == 0) {
#ifdef WITH_OPENAL
			U.audiodevice = 2;
#endif
#ifdef WITH_SDL
			U.audiodevice = 1;
#endif
		}
		if(U.audioformat == 0)
			U.audioformat = 0x24;
		if(U.audiorate == 0)
			U.audiorate = 44100;
	}

	if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 5))
		U.gameflags |= USER_DISABLE_VBO;
	
	if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 8)) {
		wmKeyMap *km;
		
		for(km=U.user_keymaps.first; km; km=km->next) {
			if (strcmp(km->idname, "Armature_Sketch")==0)
				strcpy(km->idname, "Armature Sketch");
			else if (strcmp(km->idname, "View3D")==0)
				strcpy(km->idname, "3D View");
			else if (strcmp(km->idname, "View3D Generic")==0)
				strcpy(km->idname, "3D View Generic");
			else if (strcmp(km->idname, "EditMesh")==0)
				strcpy(km->idname, "Mesh");
			else if (strcmp(km->idname, "TimeLine")==0)
				strcpy(km->idname, "Timeline");
			else if (strcmp(km->idname, "UVEdit")==0)
				strcpy(km->idname, "UV Editor");
			else if (strcmp(km->idname, "Animation_Channels")==0)
				strcpy(km->idname, "Animation Channels");
			else if (strcmp(km->idname, "GraphEdit Keys")==0)
				strcpy(km->idname, "Graph Editor");
			else if (strcmp(km->idname, "GraphEdit Generic")==0)
				strcpy(km->idname, "Graph Editor Generic");
			else if (strcmp(km->idname, "Action_Keys")==0)
				strcpy(km->idname, "Dopesheet");
			else if (strcmp(km->idname, "NLA Data")==0)
				strcpy(km->idname, "NLA Editor");
			else if (strcmp(km->idname, "Node Generic")==0)
				strcpy(km->idname, "Node Editor");
			else if (strcmp(km->idname, "Logic Generic")==0)
				strcpy(km->idname, "Logic Editor");
			else if (strcmp(km->idname, "File")==0)
				strcpy(km->idname, "File Browser");
			else if (strcmp(km->idname, "FileMain")==0)
				strcpy(km->idname, "File Browser Main");
			else if (strcmp(km->idname, "FileButtons")==0)
				strcpy(km->idname, "File Browser Buttons");
			else if (strcmp(km->idname, "Buttons Generic")==0)
				strcpy(km->idname, "Property Editor");
		}
	}
	if (bmain->versionfile < 250 || (bmain->versionfile == 250 && bmain->subversionfile < 16)) {
		if(U.wmdrawmethod == USER_DRAW_TRIPLE)
			U.wmdrawmethod = USER_DRAW_AUTOMATIC;
	}
	
	if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 3)) {
		if (U.flag & USER_LMOUSESELECT) 
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 4)) {
		bTheme *btheme;
		
		/* default new handle type is auto handles */
		U.keyhandles_new = HD_AUTO;
		
		/* init new curve colors */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* init colors used for handles in 3D-View  */
			SETCOL(btheme->tv3d.handle_free, 0, 0, 0, 255);
			SETCOL(btheme->tv3d.handle_auto, 0x90, 0x90, 0x00, 255);
			SETCOL(btheme->tv3d.handle_vect, 0x40, 0x90, 0x30, 255);
			SETCOL(btheme->tv3d.handle_align, 0x80, 0x30, 0x60, 255);
			SETCOL(btheme->tv3d.handle_sel_free, 0, 0, 0, 255);
			SETCOL(btheme->tv3d.handle_sel_auto, 0xf0, 0xff, 0x40, 255);
			SETCOL(btheme->tv3d.handle_sel_vect, 0x40, 0xc0, 0x30, 255);
			SETCOL(btheme->tv3d.handle_sel_align, 0xf0, 0x90, 0xa0, 255);
			SETCOL(btheme->tv3d.act_spline, 0xdb, 0x25, 0x12, 255);
			
			/* same colors again for Graph Editor... */
			SETCOL(btheme->tipo.handle_free, 0, 0, 0, 255);
			SETCOL(btheme->tipo.handle_auto, 0x90, 0x90, 0x00, 255);
			SETCOL(btheme->tipo.handle_vect, 0x40, 0x90, 0x30, 255);
			SETCOL(btheme->tipo.handle_align, 0x80, 0x30, 0x60, 255);
			SETCOL(btheme->tipo.handle_sel_free, 0, 0, 0, 255);
			SETCOL(btheme->tipo.handle_sel_auto, 0xf0, 0xff, 0x40, 255);
			SETCOL(btheme->tipo.handle_sel_vect, 0x40, 0xc0, 0x30, 255);
			SETCOL(btheme->tipo.handle_sel_align, 0xf0, 0x90, 0xa0, 255);
			
			/* edge crease */
			SETCOLF(btheme->tv3d.edge_crease, 0.8, 0, 0.6, 1.0);
		}
	}
	if (bmain->versionfile <= 252) {
		bTheme *btheme;

		/* init new curve colors */
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			if (btheme->tv3d.lastsel_point[3] == 0)
				SETCOL(btheme->tv3d.lastsel_point, 0xff, 0xff, 0xff, 255);
		}
	}
	if (bmain->versionfile < 252 || (bmain->versionfile == 252 && bmain->subversionfile < 5)) {
		bTheme *btheme;
		
		/* interface_widgets.c */
		struct uiWidgetColors wcol_progress= {
			{0, 0, 0, 255},
			{190, 190, 190, 255},
			{100, 100, 100, 180},
			{68, 68, 68, 255},
			
			{0, 0, 0, 255},
			{255, 255, 255, 255},
			
			0,
			5, -5
		};
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			/* init progress bar theme */
			btheme->tui.wcol_progress= wcol_progress;
		}
	}

	if (bmain->versionfile < 255 || (bmain->versionfile == 255 && bmain->subversionfile < 2)) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			SETCOL(btheme->tv3d.extra_edge_len, 32, 0, 0, 255);
			SETCOL(btheme->tv3d.extra_face_angle, 0, 32, 0, 255);
			SETCOL(btheme->tv3d.extra_face_area, 0, 0, 128, 255);
		}
	}
	
	if (bmain->versionfile < 256 || (bmain->versionfile == 256 && bmain->subversionfile < 4)) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			if((btheme->tv3d.outline_width) == 0) btheme->tv3d.outline_width= 1;
		}
	}

	if (bmain->versionfile < 257) {
		/* clear "AUTOKEY_FLAG_ONLYKEYINGSET" flag from userprefs, so that it doesn't linger around from old configs like a ghost */
		U.autokey_flag &= ~AUTOKEY_FLAG_ONLYKEYINGSET;
	}

	if (bmain->versionfile < 258 || (bmain->versionfile == 258 && bmain->subversionfile < 2)) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			btheme->tnode.noodle_curving = 5;
		}
	}

	if (bmain->versionfile < 259 || (bmain->versionfile == 259 && bmain->subversionfile < 1)) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			btheme->tv3d.speaker[3] = 255;
		}
	}

	if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 3)) {
		bTheme *btheme;
		
		/* if new keyframes handle default is stuff "auto", make it "auto-clamped" instead 
		 * was changed in 260 as part of GSoC11, but version patch was wrong
		 */
		if (U.keyhandles_new == HD_AUTO) 
			U.keyhandles_new = HD_AUTO_ANIM;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {		
			if(btheme->tv3d.bundle_solid[3] == 0)
				SETCOL(btheme->tv3d.bundle_solid, 200, 200, 200, 255);
			
			if(btheme->tv3d.camera_path[3] == 0)
				SETCOL(btheme->tv3d.camera_path, 0x00, 0x00, 0x00, 255);
				
			if((btheme->tclip.back[3]) == 0) {
				btheme->tclip= btheme->tv3d;
				
				SETCOL(btheme->tclip.marker_outline, 0x00, 0x00, 0x00, 255);
				SETCOL(btheme->tclip.marker, 0x7f, 0x7f, 0x00, 255);
				SETCOL(btheme->tclip.act_marker, 0xff, 0xff, 0xff, 255);
				SETCOL(btheme->tclip.sel_marker, 0xff, 0xff, 0x00, 255);
				SETCOL(btheme->tclip.dis_marker, 0x7f, 0x00, 0x00, 255);
				SETCOL(btheme->tclip.lock_marker, 0x7f, 0x7f, 0x7f, 255);
				SETCOL(btheme->tclip.path_before, 0xff, 0x00, 0x00, 255);
				SETCOL(btheme->tclip.path_after, 0x00, 0x00, 0xff, 255);
				SETCOL(btheme->tclip.grid, 0x5e, 0x5e, 0x5e, 255);
				SETCOL(btheme->tclip.cframe, 0x60, 0xc0, 0x40, 255);
				SETCOL(btheme->tclip.handle_vertex, 0x00, 0x00, 0x00, 0xff);
				SETCOL(btheme->tclip.handle_vertex_select, 0xff, 0xff, 0, 0xff);
				btheme->tclip.handle_vertex_size= 4;
			}
			
			/* auto-clamped handles -> based on auto */
			if(btheme->tipo.handle_auto_clamped[3] == 0)
				SETCOL(btheme->tipo.handle_auto_clamped, 0x99, 0x40, 0x30, 255);
			if(btheme->tipo.handle_sel_auto_clamped[3] == 0)
				SETCOL(btheme->tipo.handle_sel_auto_clamped, 0xf0, 0xaf, 0x90, 255);
		}
		
		/* enable (Cycles) addon by default */
		if(!BLI_findstring(&U.addons, "cycles", offsetof(bAddon, module))) {
			bAddon *baddon= MEM_callocN(sizeof(bAddon), "bAddon");
			BLI_strncpy(baddon->module, "cycles", sizeof(baddon->module));
			BLI_addtail(&U.addons, baddon);
		}
	}
	
	if (bmain->versionfile < 260 || (bmain->versionfile == 260 && bmain->subversionfile < 5)) {
		bTheme *btheme;
		
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			SETCOL(btheme->tui.panel.header, 0, 0, 0, 25);
			btheme->tui.icon_alpha= 1.0;
		}
	}
	
	if (bmain->versionfile < 261 || (bmain->versionfile == 261 && bmain->subversionfile < 4)) {
		bTheme *btheme;
		for(btheme= U.themes.first; btheme; btheme= btheme->next) {
			SETCOLF(btheme->tima.preview_stitch_face, 0.071, 0.259, 0.694, 0.150);
			SETCOLF(btheme->tima.preview_stitch_edge, 1.0, 0.522, 0.0, 0.7);
			SETCOLF(btheme->tima.preview_stitch_vert, 1.0, 0.522, 0.0, 0.5);
			SETCOLF(btheme->tima.preview_stitch_stitchable, 0.0, 1.0, 0.0, 1.0);
			SETCOLF(btheme->tima.preview_stitch_unstitchable, 1.0, 0.0, 0.0, 1.0);
			SETCOLF(btheme->tima.preview_stitch_active, 0.886, 0.824, 0.765, 0.140);
			
			SETCOLF(btheme->toops.match, 0.2, 0.5, 0.2, 0.3);
			SETCOLF(btheme->toops.selected_highlight, 0.51, 0.53, 0.55, 0.3);
		}
		
		U.use_16bit_textures = 0;
	}

	/* GL Texture Garbage Collection (variable abused above!) */
	if (U.textimeout == 0) {
		U.texcollectrate = 60;
		U.textimeout = 120;
	}
	if (U.memcachelimit <= 0) {
		U.memcachelimit = 32;
	}
	if (U.frameserverport == 0) {
		U.frameserverport = 8080;
	}
	if (U.dbl_click_time == 0) {
		U.dbl_click_time = 350;
	}
	if (U.anim_player_preset == 0) {
		U.anim_player_preset = 1 ;
	}
	if (U.scrcastfps == 0) {
		U.scrcastfps = 10;
		U.scrcastwait = 50;
	}
	if (U.v2d_min_gridsize == 0) {
		U.v2d_min_gridsize= 35;
	}
	if (U.dragthreshold == 0 )
		U.dragthreshold= 5;
	if (U.widget_unit==0)
		U.widget_unit= (U.dpi * 20 + 36)/72;
	if (U.anisotropic_filter <= 0)
		U.anisotropic_filter = 1;

	if (U.ndof_sensitivity == 0.0f) {
		U.ndof_sensitivity = 1.0f;
		U.ndof_flag = NDOF_LOCK_HORIZON |
			NDOF_SHOULD_PAN | NDOF_SHOULD_ZOOM | NDOF_SHOULD_ROTATE;
	}
	if (U.tweak_threshold == 0 )
		U.tweak_threshold= 10;

	/* funny name, but it is GE stuff, moves userdef stuff to engine */
// XXX	space_set_commmandline_options();
	/* this timer uses U */
// XXX	reset_autosave();

}
