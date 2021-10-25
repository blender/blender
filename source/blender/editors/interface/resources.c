/*
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
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_appdir.h"
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

/* be sure to keep 'bThemeState' in sync */
static struct bThemeState g_theme_state = {
    NULL,
    SPACE_VIEW3D,
    RGN_TYPE_WINDOW,
};

#define theme_active g_theme_state.theme
#define theme_spacetype g_theme_state.spacetype
#define theme_regionid g_theme_state.regionid

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
	ThemeSpace *ts = NULL;
	static char error[4] = {240, 0, 240, 255};
	static char alert[4] = {240, 60, 60, 255};
	static char headerdesel[4] = {0, 0, 0, 255};
	static char setting = 0;
	const char *cp = error;
	
	/* ensure we're not getting a color after running BKE_blender_userdef_free */
	BLI_assert(BLI_findindex(&U.themes, theme_active) != -1);
	BLI_assert(colorid != TH_UNDEFINED);

	if (btheme) {
	
		/* first check for ui buttons theme */
		if (colorid < TH_THEMEUI) {
		
			switch (colorid) {

				case TH_REDALERT:
					cp = alert; break;
			}
		}
		else {
		
			switch (spacetype) {
				case SPACE_BUTS:
					ts = &btheme->tbuts;
					break;
				case SPACE_VIEW3D:
					ts = &btheme->tv3d;
					break;
				case SPACE_IPO:
					ts = &btheme->tipo;
					break;
				case SPACE_FILE:
					ts = &btheme->tfile;
					break;
				case SPACE_NLA:
					ts = &btheme->tnla;
					break;
				case SPACE_ACTION:
					ts = &btheme->tact;
					break;
				case SPACE_SEQ:
					ts = &btheme->tseq;
					break;
				case SPACE_IMAGE:
					ts = &btheme->tima;
					break;
				case SPACE_TEXT:
					ts = &btheme->text;
					break;
				case SPACE_OUTLINER:
					ts = &btheme->toops;
					break;
				case SPACE_INFO:
					ts = &btheme->tinfo;
					break;
				case SPACE_USERPREF:
					ts = &btheme->tuserpref;
					break;
				case SPACE_CONSOLE:
					ts = &btheme->tconsole;
					break;
				case SPACE_TIME:
					ts = &btheme->ttime;
					break;
				case SPACE_NODE:
					ts = &btheme->tnode;
					break;
				case SPACE_LOGIC:
					ts = &btheme->tlogic;
					break;
				case SPACE_CLIP:
					ts = &btheme->tclip;
					break;
				default:
					ts = &btheme->tv3d;
					break;
			}

			switch (colorid) {
				case TH_BACK:
					if (theme_regionid == RGN_TYPE_WINDOW)
						cp = ts->back;
					else if (theme_regionid == RGN_TYPE_CHANNELS)
						cp = ts->list;
					else if (theme_regionid == RGN_TYPE_HEADER)
						cp = ts->header;
					else
						cp = ts->button;
					break;
				case TH_LOW_GRAD:
					cp = ts->gradients.gradient;
					break;
				case TH_HIGH_GRAD:
					cp = ts->gradients.high_gradient;
					break;
				case TH_SHOW_BACK_GRAD:
					cp = &setting;
					setting = ts->gradients.show_grad;
					break;
				case TH_TEXT:
					if (theme_regionid == RGN_TYPE_WINDOW)
						cp = ts->text;
					else if (theme_regionid == RGN_TYPE_CHANNELS)
						cp = ts->list_text;
					else if (theme_regionid == RGN_TYPE_HEADER)
						cp = ts->header_text;
					else
						cp = ts->button_text;
					break;
				case TH_TEXT_HI:
					if (theme_regionid == RGN_TYPE_WINDOW)
						cp = ts->text_hi;
					else if (theme_regionid == RGN_TYPE_CHANNELS)
						cp = ts->list_text_hi;
					else if (theme_regionid == RGN_TYPE_HEADER)
						cp = ts->header_text_hi;
					else
						cp = ts->button_text_hi;
					break;
				case TH_TITLE:
					if (theme_regionid == RGN_TYPE_WINDOW)
						cp = ts->title;
					else if (theme_regionid == RGN_TYPE_CHANNELS)
						cp = ts->list_title;
					else if (theme_regionid == RGN_TYPE_HEADER)
						cp = ts->header_title;
					else
						cp = ts->button_title;
					break;

				case TH_HEADER:
					cp = ts->header; break;
				case TH_HEADERDESEL:
					/* we calculate a dynamic builtin header deselect color, also for pulldowns... */
					cp = ts->header;
					headerdesel[0] = cp[0] > 10 ? cp[0] - 10 : 0;
					headerdesel[1] = cp[1] > 10 ? cp[1] - 10 : 0;
					headerdesel[2] = cp[2] > 10 ? cp[2] - 10 : 0;
					cp = headerdesel;
					break;
				case TH_HEADER_TEXT:
					cp = ts->header_text; break;
				case TH_HEADER_TEXT_HI:
					cp = ts->header_text_hi; break;

				case TH_PANEL_HEADER:
					cp = ts->panelcolors.header; break;
				case TH_PANEL_BACK:
					cp = ts->panelcolors.back; break;
				case TH_PANEL_SHOW_HEADER:
					cp = &setting;
					setting = ts->panelcolors.show_header;
					break;
				case TH_PANEL_SHOW_BACK:
					cp = &setting;
					setting = ts->panelcolors.show_back;
					break;
					
				case TH_BUTBACK:
					cp = ts->button; break;
				case TH_BUTBACK_TEXT:
					cp = ts->button_text; break;
				case TH_BUTBACK_TEXT_HI:
					cp = ts->button_text_hi; break;

				case TH_TAB_ACTIVE:
					cp = ts->tab_active; break;
				case TH_TAB_INACTIVE:
					cp = ts->tab_inactive; break;
				case TH_TAB_BACK:
					cp = ts->tab_back; break;
				case TH_TAB_OUTLINE:
					cp = ts->tab_outline; break;

				case TH_SHADE1:
					cp = ts->shade1; break;
				case TH_SHADE2:
					cp = ts->shade2; break;
				case TH_HILITE:
					cp = ts->hilite; break;
				
				case TH_GRID:
					cp = ts->grid; break;
				case TH_VIEW_OVERLAY:
					cp = ts->view_overlay; break;
				case TH_WIRE:
					cp = ts->wire; break;
				case TH_WIRE_INNER:
					cp = ts->syntaxr; break;
				case TH_WIRE_EDIT:
					cp = ts->wire_edit; break;
				case TH_LAMP:
					cp = ts->lamp; break;
				case TH_SPEAKER:
					cp = ts->speaker; break;
				case TH_CAMERA:
					cp = ts->camera; break;
				case TH_EMPTY:
					cp = ts->empty; break;
				case TH_SELECT:
					cp = ts->select; break;
				case TH_ACTIVE:
					cp = ts->active; break;
				case TH_GROUP:
					cp = ts->group; break;
				case TH_GROUP_ACTIVE:
					cp = ts->group_active; break;
				case TH_TRANSFORM:
					cp = ts->transform; break;
				case TH_VERTEX:
					cp = ts->vertex; break;
				case TH_VERTEX_SELECT:
					cp = ts->vertex_select; break;
				case TH_VERTEX_BEVEL:
					cp = ts->vertex_bevel; break;
				case TH_VERTEX_UNREFERENCED:
					cp = ts->vertex_unreferenced; break;
				case TH_VERTEX_SIZE:
					cp = &ts->vertex_size; break;
				case TH_OUTLINE_WIDTH:
					cp = &ts->outline_width; break;
				case TH_EDGE:
					cp = ts->edge; break;
				case TH_EDGE_SELECT:
					cp = ts->edge_select; break;
				case TH_EDGE_SEAM:
					cp = ts->edge_seam; break;
				case TH_EDGE_SHARP:
					cp = ts->edge_sharp; break;
				case TH_EDGE_CREASE:
					cp = ts->edge_crease; break;
				case TH_EDGE_BEVEL:
					cp = ts->edge_bevel; break;
				case TH_EDITMESH_ACTIVE:
					cp = ts->editmesh_active; break;
				case TH_EDGE_FACESEL:
					cp = ts->edge_facesel; break;
				case TH_FACE:
					cp = ts->face; break;
				case TH_FACE_SELECT:
					cp = ts->face_select; break;
				case TH_FACE_DOT:
					cp = ts->face_dot; break;
				case TH_FACEDOT_SIZE:
					cp = &ts->facedot_size; break;
				case TH_DRAWEXTRA_EDGELEN:
					cp = ts->extra_edge_len; break;
				case TH_DRAWEXTRA_EDGEANG:
					cp = ts->extra_edge_angle; break;
				case TH_DRAWEXTRA_FACEAREA:
					cp = ts->extra_face_area; break;
				case TH_DRAWEXTRA_FACEANG:
					cp = ts->extra_face_angle; break;
				case TH_NORMAL:
					cp = ts->normal; break;
				case TH_VNORMAL:
					cp = ts->vertex_normal; break;
				case TH_LNORMAL:
					cp = ts->loop_normal; break;
				case TH_BONE_SOLID:
					cp = ts->bone_solid; break;
				case TH_BONE_POSE:
					cp = ts->bone_pose; break;
				case TH_BONE_POSE_ACTIVE:
					cp = ts->bone_pose_active; break;
				case TH_STRIP:
					cp = ts->strip; break;
				case TH_STRIP_SELECT:
					cp = ts->strip_select; break;
				case TH_KEYTYPE_KEYFRAME:
					cp = ts->keytype_keyframe; break;
				case TH_KEYTYPE_KEYFRAME_SELECT:
					cp = ts->keytype_keyframe_select; break;
				case TH_KEYTYPE_EXTREME:
					cp = ts->keytype_extreme; break;
				case TH_KEYTYPE_EXTREME_SELECT:
					cp = ts->keytype_extreme_select; break;
				case TH_KEYTYPE_BREAKDOWN:
					cp = ts->keytype_breakdown; break;
				case TH_KEYTYPE_BREAKDOWN_SELECT:
					cp = ts->keytype_breakdown_select; break;
				case TH_KEYTYPE_JITTER:
					cp = ts->keytype_jitter; break;
				case TH_KEYTYPE_JITTER_SELECT:
					cp = ts->keytype_jitter_select; break;
				case TH_KEYBORDER:
					cp = ts->keyborder; break;
				case TH_KEYBORDER_SELECT:
					cp = ts->keyborder_select; break;
				case TH_CFRAME:
					cp = ts->cframe; break;
				case TH_TIME_KEYFRAME:
					cp = ts->time_keyframe; break;
				case TH_TIME_GP_KEYFRAME:
					cp = ts->time_gp_keyframe; break;
				case TH_NURB_ULINE:
					cp = ts->nurb_uline; break;
				case TH_NURB_VLINE:
					cp = ts->nurb_vline; break;
				case TH_NURB_SEL_ULINE:
					cp = ts->nurb_sel_uline; break;
				case TH_NURB_SEL_VLINE:
					cp = ts->nurb_sel_vline; break;
				case TH_ACTIVE_SPLINE:
					cp = ts->act_spline; break;
				case TH_ACTIVE_VERT:
					cp = ts->lastsel_point; break;
				case TH_HANDLE_FREE:
					cp = ts->handle_free; break;
				case TH_HANDLE_AUTO:
					cp = ts->handle_auto; break;
				case TH_HANDLE_AUTOCLAMP:
					cp = ts->handle_auto_clamped; break;
				case TH_HANDLE_VECT:
					cp = ts->handle_vect; break;
				case TH_HANDLE_ALIGN:
					cp = ts->handle_align; break;
				case TH_HANDLE_SEL_FREE:
					cp = ts->handle_sel_free; break;
				case TH_HANDLE_SEL_AUTO:
					cp = ts->handle_sel_auto; break;
				case TH_HANDLE_SEL_AUTOCLAMP:
					cp = ts->handle_sel_auto_clamped; break;
				case TH_HANDLE_SEL_VECT:
					cp = ts->handle_sel_vect; break;
				case TH_HANDLE_SEL_ALIGN:
					cp = ts->handle_sel_align; break;
				case TH_FREESTYLE_EDGE_MARK:
					cp = ts->freestyle_edge_mark; break;
				case TH_FREESTYLE_FACE_MARK:
					cp = ts->freestyle_face_mark; break;

				case TH_SYNTAX_B:
					cp = ts->syntaxb; break;
				case TH_SYNTAX_V:
					cp = ts->syntaxv; break;
				case TH_SYNTAX_C:
					cp = ts->syntaxc; break;
				case TH_SYNTAX_L:
					cp = ts->syntaxl; break;
				case TH_SYNTAX_D:
					cp = ts->syntaxd; break;
				case TH_SYNTAX_R:
					cp = ts->syntaxr; break;
				case TH_SYNTAX_N:
					cp = ts->syntaxn; break;
				case TH_SYNTAX_S:
					cp = ts->syntaxs; break;

				case TH_NODE:
					cp = ts->syntaxl; break;
				case TH_NODE_INPUT:
					cp = ts->syntaxn; break;
				case TH_NODE_OUTPUT:
					cp = ts->nodeclass_output; break;
				case TH_NODE_COLOR:
					cp = ts->syntaxb; break;
				case TH_NODE_FILTER:
					cp = ts->nodeclass_filter; break;
				case TH_NODE_VECTOR:
					cp = ts->nodeclass_vector; break;
				case TH_NODE_TEXTURE:
					cp = ts->nodeclass_texture; break;
				case TH_NODE_PATTERN:
					cp = ts->nodeclass_pattern; break;
				case TH_NODE_SCRIPT:
					cp = ts->nodeclass_script; break;
				case TH_NODE_LAYOUT:
					cp = ts->nodeclass_layout; break;
				case TH_NODE_SHADER:
					cp = ts->nodeclass_shader; break;
				case TH_NODE_CONVERTOR:
					cp = ts->syntaxv; break;
				case TH_NODE_GROUP:
					cp = ts->syntaxc; break;
				case TH_NODE_INTERFACE:
					cp = ts->console_output; break;
				case TH_NODE_FRAME:
					cp = ts->movie; break;
				case TH_NODE_MATTE:
					cp = ts->syntaxs; break;
				case TH_NODE_DISTORT:
					cp = ts->syntaxd; break;
				case TH_NODE_CURVING:
					cp = &ts->noodle_curving; break;

				case TH_SEQ_MOVIE:
					cp = ts->movie; break;
				case TH_SEQ_MOVIECLIP:
					cp = ts->movieclip; break;
				case TH_SEQ_MASK:
					cp = ts->mask; break;
				case TH_SEQ_IMAGE:
					cp = ts->image; break;
				case TH_SEQ_SCENE:
					cp = ts->scene; break;
				case TH_SEQ_AUDIO:
					cp = ts->audio; break;
				case TH_SEQ_EFFECT:
					cp = ts->effect; break;
				case TH_SEQ_TRANSITION:
					cp = ts->transition; break;
				case TH_SEQ_META:
					cp = ts->meta; break;
				case TH_SEQ_TEXT:
					cp = ts->text_strip; break;
				case TH_SEQ_PREVIEW:
					cp = ts->preview_back; break;

				case TH_CONSOLE_OUTPUT:
					cp = ts->console_output; break;
				case TH_CONSOLE_INPUT:
					cp = ts->console_input; break;
				case TH_CONSOLE_INFO:
					cp = ts->console_info; break;
				case TH_CONSOLE_ERROR:
					cp = ts->console_error; break;
				case TH_CONSOLE_CURSOR:
					cp = ts->console_cursor; break;
				case TH_CONSOLE_SELECT:
					cp = ts->console_select; break;

				case TH_HANDLE_VERTEX:
					cp = ts->handle_vertex;
					break;
				case TH_HANDLE_VERTEX_SELECT:
					cp = ts->handle_vertex_select;
					break;
				case TH_HANDLE_VERTEX_SIZE:
					cp = &ts->handle_vertex_size;
					break;
					
				case TH_GP_VERTEX:
					cp = ts->gp_vertex;
					break;
				case TH_GP_VERTEX_SELECT:
					cp = ts->gp_vertex_select;
					break;
				case TH_GP_VERTEX_SIZE:
					cp = &ts->gp_vertex_size;
					break;
					
				case TH_DOPESHEET_CHANNELOB:
					cp = ts->ds_channel;
					break;
				case TH_DOPESHEET_CHANNELSUBOB:
					cp = ts->ds_subchannel;
					break;

				case TH_PREVIEW_BACK:
					cp = ts->preview_back;
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

				case TH_PAINT_CURVE_HANDLE:
					cp = ts->paint_curve_handle;
					break;
				case TH_PAINT_CURVE_PIVOT:
					cp = ts->paint_curve_pivot;
					break;

				case TH_METADATA_BG:
					cp = ts->metadatabg;
					break;
				case TH_METADATA_TEXT:
					cp = ts->metadatatext;
					break;

				case TH_UV_OTHERS:
					cp = ts->uv_others;
					break;
				case TH_UV_SHADOW:
					cp = ts->uv_shadow;
					break;

				case TH_MARKER_OUTLINE:
					cp = ts->marker_outline; break;
				case TH_MARKER:
					cp = ts->marker; break;
				case TH_ACT_MARKER:
					cp = ts->act_marker; break;
				case TH_SEL_MARKER:
					cp = ts->sel_marker; break;
				case TH_BUNDLE_SOLID:
					cp = ts->bundle_solid; break;
				case TH_DIS_MARKER:
					cp = ts->dis_marker; break;
				case TH_PATH_BEFORE:
					cp = ts->path_before; break;
				case TH_PATH_AFTER:
					cp = ts->path_after; break;
				case TH_CAMERA_PATH:
					cp = ts->camera_path; break;
				case TH_LOCK_MARKER:
					cp = ts->lock_marker; break;

				case TH_MATCH:
					cp = ts->match;
					break;

				case TH_SELECT_HIGHLIGHT:
					cp = ts->selected_highlight;
					break;

				case TH_SKIN_ROOT:
					cp = ts->skin_root;
					break;
					
				case TH_ANIM_ACTIVE:
					cp = ts->anim_active;
					break;
				case TH_ANIM_INACTIVE:
					cp = ts->anim_non_active;
					break;
				
				case TH_NLA_TWEAK:
					cp = ts->nla_tweaking;
					break;
				case TH_NLA_TWEAK_DUPLI:
					cp = ts->nla_tweakdupli;
					break;
				
				case TH_NLA_TRANSITION:
					cp = ts->nla_transition;
					break;
				case TH_NLA_TRANSITION_SEL:
					cp = ts->nla_transition_sel;
					break;
				case TH_NLA_META:
					cp = ts->nla_meta;
					break;
				case TH_NLA_META_SEL:
					cp = ts->nla_meta_sel;
					break;
				case TH_NLA_SOUND:
					cp = ts->nla_sound;
					break;
				case TH_NLA_SOUND_SEL:
					cp = ts->nla_sound_sel;
					break;
					
				case TH_WIDGET_EMBOSS:
					cp = btheme->tui.widget_emboss; break;

				case TH_AXIS_X:
					cp = btheme->tui.xaxis; break;
				case TH_AXIS_Y:
					cp = btheme->tui.yaxis; break;
				case TH_AXIS_Z:
					cp = btheme->tui.zaxis; break;

				case TH_INFO_SELECTED:
					cp = ts->info_selected;
					break;
				case TH_INFO_SELECTED_TEXT:
					cp = ts->info_selected_text;
					break;
				case TH_INFO_ERROR:
					cp = ts->info_error;
					break;
				case TH_INFO_ERROR_TEXT:
					cp = ts->info_error_text;
					break;
				case TH_INFO_WARNING:
					cp = ts->info_warning;
					break;
				case TH_INFO_WARNING_TEXT:
					cp = ts->info_warning_text;
					break;
				case TH_INFO_INFO:
					cp = ts->info_info;
					break;
				case TH_INFO_INFO_TEXT:
					cp = ts->info_info_text;
					break;
				case TH_INFO_DEBUG:
					cp = ts->info_debug;
					break;
				case TH_INFO_DEBUG_TEXT:
					cp = ts->info_debug_text;
					break;
				case TH_V3D_CLIPPING_BORDER:
					cp = ts->clipping_border_3d;
					break;
			}
		}
	}
	
	return (const unsigned char *)cp;
}

/* use this call to init new bone color sets in Theme */
static void ui_theme_init_boneColorSets(bTheme *btheme)
{
	int i;
	
	/* define default color sets - currently we only define 15 of these, though that should be ample */
	/* set 1 */
	rgba_char_args_set(btheme->tarm[0].solid, 0x9a, 0x00, 0x00, 255);
	rgba_char_args_set(btheme->tarm[0].select, 0xbd, 0x11, 0x11, 255);
	rgba_char_args_set(btheme->tarm[0].active, 0xf7, 0x0a, 0x0a, 255);
	/* set 2 */
	rgba_char_args_set(btheme->tarm[1].solid, 0xf7, 0x40, 0x18, 255);
	rgba_char_args_set(btheme->tarm[1].select, 0xf6, 0x69, 0x13, 255);
	rgba_char_args_set(btheme->tarm[1].active, 0xfa, 0x99, 0x00, 255);
	/* set 3 */
	rgba_char_args_set(btheme->tarm[2].solid, 0x1e, 0x91, 0x09, 255);
	rgba_char_args_set(btheme->tarm[2].select, 0x59, 0xb7, 0x0b, 255);
	rgba_char_args_set(btheme->tarm[2].active, 0x83, 0xef, 0x1d, 255);
	/* set 4 */
	rgba_char_args_set(btheme->tarm[3].solid, 0x0a, 0x36, 0x94, 255);
	rgba_char_args_set(btheme->tarm[3].select, 0x36, 0x67, 0xdf, 255);
	rgba_char_args_set(btheme->tarm[3].active, 0x5e, 0xc1, 0xef, 255);
	/* set 5 */
	rgba_char_args_set(btheme->tarm[4].solid, 0xa9, 0x29, 0x4e, 255);
	rgba_char_args_set(btheme->tarm[4].select, 0xc1, 0x41, 0x6a, 255);
	rgba_char_args_set(btheme->tarm[4].active, 0xf0, 0x5d, 0x91, 255);
	/* set 6 */
	rgba_char_args_set(btheme->tarm[5].solid, 0x43, 0x0c, 0x78, 255);
	rgba_char_args_set(btheme->tarm[5].select, 0x54, 0x3a, 0xa3, 255);
	rgba_char_args_set(btheme->tarm[5].active, 0x87, 0x64, 0xd5, 255);
	/* set 7 */
	rgba_char_args_set(btheme->tarm[6].solid, 0x24, 0x78, 0x5a, 255);
	rgba_char_args_set(btheme->tarm[6].select, 0x3c, 0x95, 0x79, 255);
	rgba_char_args_set(btheme->tarm[6].active, 0x6f, 0xb6, 0xab, 255);
	/* set 8 */
	rgba_char_args_set(btheme->tarm[7].solid, 0x4b, 0x70, 0x7c, 255);
	rgba_char_args_set(btheme->tarm[7].select, 0x6a, 0x86, 0x91, 255);
	rgba_char_args_set(btheme->tarm[7].active, 0x9b, 0xc2, 0xcd, 255);
	/* set 9 */
	rgba_char_args_set(btheme->tarm[8].solid, 0xf4, 0xc9, 0x0c, 255);
	rgba_char_args_set(btheme->tarm[8].select, 0xee, 0xc2, 0x36, 255);
	rgba_char_args_set(btheme->tarm[8].active, 0xf3, 0xff, 0x00, 255);
	/* set 10 */
	rgba_char_args_set(btheme->tarm[9].solid, 0x1e, 0x20, 0x24, 255);
	rgba_char_args_set(btheme->tarm[9].select, 0x48, 0x4c, 0x56, 255);
	rgba_char_args_set(btheme->tarm[9].active, 0xff, 0xff, 0xff, 255);
	/* set 11 */
	rgba_char_args_set(btheme->tarm[10].solid, 0x6f, 0x2f, 0x6a, 255);
	rgba_char_args_set(btheme->tarm[10].select, 0x98, 0x45, 0xbe, 255);
	rgba_char_args_set(btheme->tarm[10].active, 0xd3, 0x30, 0xd6, 255);
	/* set 12 */
	rgba_char_args_set(btheme->tarm[11].solid, 0x6c, 0x8e, 0x22, 255);
	rgba_char_args_set(btheme->tarm[11].select, 0x7f, 0xb0, 0x22, 255);
	rgba_char_args_set(btheme->tarm[11].active, 0xbb, 0xef, 0x5b, 255);
	/* set 13 */
	rgba_char_args_set(btheme->tarm[12].solid, 0x8d, 0x8d, 0x8d, 255);
	rgba_char_args_set(btheme->tarm[12].select, 0xb0, 0xb0, 0xb0, 255);
	rgba_char_args_set(btheme->tarm[12].active, 0xde, 0xde, 0xde, 255);
	/* set 14 */
	rgba_char_args_set(btheme->tarm[13].solid, 0x83, 0x43, 0x26, 255);
	rgba_char_args_set(btheme->tarm[13].select, 0x8b, 0x58, 0x11, 255);
	rgba_char_args_set(btheme->tarm[13].active, 0xbd, 0x6a, 0x11, 255);
	/* set 15 */
	rgba_char_args_set(btheme->tarm[14].solid, 0x08, 0x31, 0x0e, 255);
	rgba_char_args_set(btheme->tarm[14].select, 0x1c, 0x43, 0x0b, 255);
	rgba_char_args_set(btheme->tarm[14].active, 0x34, 0x62, 0x2b, 255);
	
	/* reset flags too */
	for (i = 0; i < 20; i++)
		btheme->tarm[i].flag = 0;
}

/* use this call to init new variables in themespace, if they're same for all */
static void ui_theme_init_new_do(ThemeSpace *ts)
{
	rgba_char_args_set(ts->header_text,    0, 0, 0, 255);
	rgba_char_args_set(ts->header_title,   0, 0, 0, 255);
	rgba_char_args_set(ts->header_text_hi, 255, 255, 255, 255);

#if 0
	rgba_char_args_set(ts->panel_text,     0, 0, 0, 255);
	rgba_char_args_set(ts->panel_title,        0, 0, 0, 255);
	rgba_char_args_set(ts->panel_text_hi,  255, 255, 255, 255);
#endif

	ts->panelcolors.show_back = false;
	ts->panelcolors.show_header = false;
	rgba_char_args_set(ts->panelcolors.back,   114, 114, 114, 128);
	rgba_char_args_set(ts->panelcolors.header, 0, 0, 0, 25);

	rgba_char_args_set(ts->button,         145, 145, 145, 245);
	rgba_char_args_set(ts->button_title,   0, 0, 0, 255);
	rgba_char_args_set(ts->button_text,    0, 0, 0, 255);
	rgba_char_args_set(ts->button_text_hi, 255, 255, 255, 255);

	rgba_char_args_set(ts->list,           165, 165, 165, 255);
	rgba_char_args_set(ts->list_title,     0, 0, 0, 255);
	rgba_char_args_set(ts->list_text,      0, 0, 0, 255);
	rgba_char_args_set(ts->list_text_hi,   255, 255, 255, 255);

	rgba_char_args_set(ts->tab_active,     114, 114, 114, 255);
	rgba_char_args_set(ts->tab_inactive,   83, 83, 83, 255);
	rgba_char_args_set(ts->tab_back,       64, 64, 64, 255);
	rgba_char_args_set(ts->tab_outline,    60, 60, 60, 255);
}

static void ui_theme_init_new(bTheme *btheme)
{
	ThemeSpace *ts;

	for (ts = UI_THEMESPACE_START(btheme); ts != UI_THEMESPACE_END(btheme); ts++) {
		ui_theme_init_new_do(ts);
	}
}

static void ui_theme_space_init_handles_color(ThemeSpace *theme_space)
{
	rgba_char_args_set(theme_space->handle_free, 0, 0, 0, 255);
	rgba_char_args_set(theme_space->handle_auto, 0x90, 0x90, 0x00, 255);
	rgba_char_args_set(theme_space->handle_vect, 0x40, 0x90, 0x30, 255);
	rgba_char_args_set(theme_space->handle_align, 0x80, 0x30, 0x60, 255);
	rgba_char_args_set(theme_space->handle_sel_free, 0, 0, 0, 255);
	rgba_char_args_set(theme_space->handle_sel_auto, 0xf0, 0xff, 0x40, 255);
	rgba_char_args_set(theme_space->handle_sel_vect, 0x40, 0xc0, 0x30, 255);
	rgba_char_args_set(theme_space->handle_sel_align, 0xf0, 0x90, 0xa0, 255);
	rgba_char_args_set(theme_space->handle_vertex, 0x00, 0x00, 0x00, 0xff);
	rgba_char_args_set(theme_space->handle_vertex_select, 0xff, 0xff, 0, 0xff);
	rgba_char_args_set(theme_space->act_spline, 0xdb, 0x25, 0x12, 255);
}

/**
 * initialize default theme
 * \note: when you add new colors, created & saved themes need initialized
 * use function below, init_userdef_do_versions()
 */
void ui_theme_init_default(void)
{
	bTheme *btheme;
	
	/* we search for the theme with name Default */
	btheme = BLI_findstring(&U.themes, "Default", offsetof(bTheme, name));
	
	if (btheme == NULL) {
		btheme = MEM_callocN(sizeof(bTheme), "theme");
		BLI_addtail(&U.themes, btheme);
		strcpy(btheme->name, "Default");
	}
	
	UI_SetTheme(0, 0);  /* make sure the global used in this file is set */

	/* UI buttons */
	ui_widget_color_init(&btheme->tui);

	btheme->tui.iconfile[0] = 0;
	rgba_char_args_set(btheme->tui.wcol_tooltip.text, 255, 255, 255, 255);
	rgba_char_args_set_fl(btheme->tui.widget_emboss, 1.0f, 1.0f, 1.0f, 0.02f);

	rgba_char_args_set(btheme->tui.xaxis, 220,   0,   0, 255);
	rgba_char_args_set(btheme->tui.yaxis,   0, 220,   0, 255);
	rgba_char_args_set(btheme->tui.zaxis,   0,   0, 220, 255);

	btheme->tui.menu_shadow_fac = 0.5f;
	btheme->tui.menu_shadow_width = 12;
	
	/* Bone Color Sets */
	ui_theme_init_boneColorSets(btheme);
	
	/* common (new) variables */
	ui_theme_init_new(btheme);
	
	/* space view3d */
	rgba_char_args_set_fl(btheme->tv3d.back,       0.225, 0.225, 0.225, 1.0);
	rgba_char_args_set(btheme->tv3d.text,       0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.text_hi, 255, 255, 255, 255);
	
	rgba_char_args_set_fl(btheme->tv3d.header,  0.45, 0.45, 0.45, 1.0);
	rgba_char_args_set_fl(btheme->tv3d.button,  0.45, 0.45, 0.45, 0.5);
//	rgba_char_args_set(btheme->tv3d.panel,      165, 165, 165, 127);
	
	rgba_char_args_set(btheme->tv3d.shade1,  160, 160, 160, 100);
	rgba_char_args_set(btheme->tv3d.shade2,  0x7f, 0x70, 0x70, 100);

	rgba_char_args_set_fl(btheme->tv3d.grid,     0.251, 0.251, 0.251, 1.0);
	rgba_char_args_set(btheme->tv3d.view_overlay, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.wire,       0x0, 0x0, 0x0, 255);
	rgba_char_args_set(btheme->tv3d.wire_edit,  0x0, 0x0, 0x0, 255);
	rgba_char_args_set(btheme->tv3d.lamp,       0, 0, 0, 40);
	rgba_char_args_set(btheme->tv3d.speaker,    0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.camera,    0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.empty,    0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.select, 241, 88, 0, 255);
	rgba_char_args_set(btheme->tv3d.active, 255, 170, 64, 255);
	rgba_char_args_set(btheme->tv3d.group,      8, 48, 8, 255);
	rgba_char_args_set(btheme->tv3d.group_active, 85, 187, 85, 255);
	rgba_char_args_set(btheme->tv3d.transform, 0xff, 0xff, 0xff, 255);
	rgba_char_args_set(btheme->tv3d.vertex, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.vertex_select, 255, 133, 0, 255);
	rgba_char_args_set(btheme->tv3d.vertex_bevel, 0, 165, 255, 255);
	rgba_char_args_set(btheme->tv3d.vertex_unreferenced, 0, 0, 0, 255);
	btheme->tv3d.vertex_size = 3;
	btheme->tv3d.outline_width = 1;
	rgba_char_args_set(btheme->tv3d.edge,       0x0, 0x0, 0x0, 255);
	rgba_char_args_set(btheme->tv3d.edge_select, 255, 160, 0, 255);
	rgba_char_args_set(btheme->tv3d.edge_seam, 219, 37, 18, 255);
	rgba_char_args_set(btheme->tv3d.edge_bevel, 0, 165, 255, 255);
	rgba_char_args_set(btheme->tv3d.edge_facesel, 75, 75, 75, 255);
	rgba_char_args_set(btheme->tv3d.face,       0, 0, 0, 18);
	rgba_char_args_set(btheme->tv3d.face_select, 255, 133, 0, 60);
	rgba_char_args_set(btheme->tv3d.normal, 0x22, 0xDD, 0xDD, 255);
	rgba_char_args_set(btheme->tv3d.vertex_normal, 0x23, 0x61, 0xDD, 255);
	rgba_char_args_set(btheme->tv3d.loop_normal, 0xDD, 0x23, 0xDD, 255);
	rgba_char_args_set(btheme->tv3d.face_dot, 255, 133, 0, 255);
	rgba_char_args_set(btheme->tv3d.editmesh_active, 255, 255, 255, 128);
	rgba_char_args_set_fl(btheme->tv3d.edge_crease, 0.8, 0, 0.6, 1.0);
	rgba_char_args_set(btheme->tv3d.edge_sharp, 0, 255, 255, 255);
	rgba_char_args_set(btheme->tv3d.header_text, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.header_text_hi, 255, 255, 255, 255);
	rgba_char_args_set(btheme->tv3d.button_text, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.button_text_hi, 255, 255, 255, 255);
	rgba_char_args_set(btheme->tv3d.button_title, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.title, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.freestyle_edge_mark, 0x7f, 0xff, 0x7f, 255);
	rgba_char_args_set(btheme->tv3d.freestyle_face_mark, 0x7f, 0xff, 0x7f, 51);
	rgba_char_args_set_fl(btheme->tv3d.paint_curve_handle, 0.5f, 1.0f, 0.5f, 0.5f);
	rgba_char_args_set_fl(btheme->tv3d.paint_curve_pivot, 1.0f, 0.5f, 0.5f, 0.5f);
	rgba_char_args_set(btheme->tv3d.gp_vertex, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.gp_vertex_select, 255, 133, 0, 255);
	btheme->tv3d.gp_vertex_size = 3;

	btheme->tv3d.facedot_size = 4;

	rgba_char_args_set(btheme->tv3d.extra_edge_len, 32, 0, 0, 255);
	rgba_char_args_set(btheme->tv3d.extra_edge_angle, 32, 32, 0, 255);
	rgba_char_args_set(btheme->tv3d.extra_face_area, 0, 32, 0, 255);
	rgba_char_args_set(btheme->tv3d.extra_face_angle, 0, 0, 128, 255);

	rgba_char_args_set(btheme->tv3d.cframe, 0x60, 0xc0,  0x40, 255);

	rgba_char_args_set(btheme->tv3d.nurb_uline, 0x90, 0x90, 0x00, 255);
	rgba_char_args_set(btheme->tv3d.nurb_vline, 0x80, 0x30, 0x60, 255);
	rgba_char_args_set(btheme->tv3d.nurb_sel_uline, 0xf0, 0xff, 0x40, 255);
	rgba_char_args_set(btheme->tv3d.nurb_sel_vline, 0xf0, 0x90, 0xa0, 255);

	ui_theme_space_init_handles_color(&btheme->tv3d);

	rgba_char_args_set(btheme->tv3d.act_spline, 0xdb, 0x25, 0x12, 255);
	rgba_char_args_set(btheme->tv3d.lastsel_point,  0xff, 0xff, 0xff, 255);

	rgba_char_args_set(btheme->tv3d.bone_solid, 200, 200, 200, 255);
	/* alpha 80 is not meant editable, used for wire+action draw */
	rgba_char_args_set(btheme->tv3d.bone_pose, 80, 200, 255, 80);
	rgba_char_args_set(btheme->tv3d.bone_pose_active, 140, 255, 255, 80);

	rgba_char_args_set(btheme->tv3d.bundle_solid, 200, 200, 200, 255);
	rgba_char_args_set(btheme->tv3d.camera_path, 0x00, 0x00, 0x00, 255);

	rgba_char_args_set(btheme->tv3d.skin_root, 180, 77, 77, 255);
	rgba_char_args_set(btheme->tv3d.gradients.gradient, 0, 0, 0, 0);
	rgba_char_args_set(btheme->tv3d.gradients.high_gradient, 58, 58, 58, 255);
	btheme->tv3d.gradients.show_grad = false;

	rgba_char_args_set(btheme->tv3d.clipping_border_3d, 50, 50, 50, 255);

	rgba_char_args_set(btheme->tv3d.time_keyframe, 0xDD, 0xD7, 0x00, 0xFF);
	rgba_char_args_set(btheme->tv3d.time_gp_keyframe, 0xB5, 0xE6, 0x1D, 0xFF);

	/* space buttons */
	/* to have something initialized */
	btheme->tbuts = btheme->tv3d;

	rgba_char_args_set_fl(btheme->tbuts.back,   0.45, 0.45, 0.45, 1.0);
//	rgba_char_args_set(btheme->tbuts.panel, 0x82, 0x82, 0x82, 255);

	/* graph editor */
	btheme->tipo = btheme->tv3d;
	rgba_char_args_set_fl(btheme->tipo.back,    0.42, 0.42, 0.42, 1.0);
	rgba_char_args_set_fl(btheme->tipo.list,    0.4, 0.4, 0.4, 1.0);
	rgba_char_args_set(btheme->tipo.grid,   94, 94, 94, 255);
//	rgba_char_args_set(btheme->tipo.panel,  255, 255, 255, 150);
	rgba_char_args_set(btheme->tipo.shade1,     150, 150, 150, 100);    /* scrollbars */
	rgba_char_args_set(btheme->tipo.shade2,     0x70, 0x70, 0x70, 100);
	rgba_char_args_set(btheme->tipo.vertex,     0, 0, 0, 255);
	rgba_char_args_set(btheme->tipo.vertex_select, 255, 133, 0, 255);
	rgba_char_args_set(btheme->tipo.hilite, 0x60, 0xc0, 0x40, 255);
	btheme->tipo.vertex_size = 6;

	rgba_char_args_set(btheme->tipo.handle_vertex,      0, 0, 0, 255);
	rgba_char_args_set(btheme->tipo.handle_vertex_select, 255, 133, 0, 255);
	rgba_char_args_set(btheme->tipo.handle_auto_clamped, 0x99, 0x40, 0x30, 255);
	rgba_char_args_set(btheme->tipo.handle_sel_auto_clamped, 0xf0, 0xaf, 0x90, 255);
	btheme->tipo.handle_vertex_size = 5;
	
	rgba_char_args_set(btheme->tipo.ds_channel,      82, 96, 110, 255);
	rgba_char_args_set(btheme->tipo.ds_subchannel,  124, 137, 150, 255);
	rgba_char_args_set(btheme->tipo.group,           79, 101, 73, 255);
	rgba_char_args_set(btheme->tipo.group_active,   135, 177, 125, 255);

	/* dopesheet */
	btheme->tact = btheme->tipo;
	rgba_char_args_set(btheme->tact.strip,          12, 10, 10, 128);
	rgba_char_args_set(btheme->tact.strip_select,   255, 140, 0, 255);
	
	rgba_char_args_set(btheme->tact.anim_active,    204, 112, 26, 102);
	
	rgba_char_args_set(btheme->tact.keytype_keyframe,           232, 232, 232, 255);
	rgba_char_args_set(btheme->tact.keytype_keyframe_select,    255, 190,  50, 255);
	rgba_char_args_set(btheme->tact.keytype_extreme,            232, 179, 204, 255);
	rgba_char_args_set(btheme->tact.keytype_extreme_select,     242, 128, 128, 255);
	rgba_char_args_set(btheme->tact.keytype_breakdown,          179, 219, 232, 255);
	rgba_char_args_set(btheme->tact.keytype_breakdown_select,    84, 191, 237, 255);
	rgba_char_args_set(btheme->tact.keytype_jitter,             148, 229, 117, 255);
	rgba_char_args_set(btheme->tact.keytype_jitter_select,       97, 192,  66, 255);
	
	rgba_char_args_set(btheme->tact.keyborder,               0,   0,   0, 255);
	rgba_char_args_set(btheme->tact.keyborder_select,        0,   0,   0, 255);
	
	btheme->tact.keyframe_scale_fac = 1.0f;
	
	/* space nla */
	btheme->tnla = btheme->tact;
	
	rgba_char_args_set(btheme->tnla.anim_active,     204, 112, 26, 102); /* same as for dopesheet; duplicate here for easier reference */
	rgba_char_args_set(btheme->tnla.anim_non_active, 153, 135, 97, 77);
	
	rgba_char_args_set(btheme->tnla.nla_tweaking,   77, 243, 26, 77);
	rgba_char_args_set(btheme->tnla.nla_tweakdupli, 217, 0, 0, 255);
	
	rgba_char_args_set(btheme->tnla.nla_transition,     28, 38, 48, 255);
	rgba_char_args_set(btheme->tnla.nla_transition_sel, 46, 117, 219, 255);
	rgba_char_args_set(btheme->tnla.nla_meta,           51, 38, 66, 255);
	rgba_char_args_set(btheme->tnla.nla_meta_sel,       105, 33, 150, 255);
	rgba_char_args_set(btheme->tnla.nla_sound,          43, 61, 61, 255);
	rgba_char_args_set(btheme->tnla.nla_sound_sel,      31, 122, 122, 255);
	
	rgba_char_args_set(btheme->tnla.keyborder,               0,   0,   0, 255);
	rgba_char_args_set(btheme->tnla.keyborder_select,        0,   0,   0, 255);
	
	/* space file */
	/* to have something initialized */
	btheme->tfile = btheme->tv3d;
	rgba_char_args_set_fl(btheme->tfile.back, 0.3, 0.3, 0.3, 1);
//	rgba_char_args_set_fl(btheme->tfile.panel, 0.3, 0.3, 0.3, 1);
	rgba_char_args_set_fl(btheme->tfile.list, 0.4, 0.4, 0.4, 1);
	rgba_char_args_set(btheme->tfile.text,  250, 250, 250, 255);
	rgba_char_args_set(btheme->tfile.text_hi, 15, 15, 15, 255);
//	rgba_char_args_set(btheme->tfile.panel, 145, 145, 145, 255);  /* bookmark/ui regions */
	rgba_char_args_set(btheme->tfile.hilite, 255, 140, 25, 255);  /* selected files */

	rgba_char_args_set(btheme->tfile.image, 250, 250, 250, 255);
	rgba_char_args_set(btheme->tfile.movie, 250, 250, 250, 255);
	rgba_char_args_set(btheme->tfile.scene, 250, 250, 250, 255);

	
	/* space seq */
	btheme->tseq = btheme->tv3d;
	rgba_char_args_set(btheme->tseq.back,   116, 116, 116, 255);
	rgba_char_args_set(btheme->tseq.movie,  81, 105, 135, 255);
	rgba_char_args_set(btheme->tseq.movieclip,  32, 32, 143, 255);
	rgba_char_args_set(btheme->tseq.mask,   152, 78, 62, 255);
	rgba_char_args_set(btheme->tseq.image,  109, 88, 129, 255);
	rgba_char_args_set(btheme->tseq.scene,  78, 152, 62, 255);
	rgba_char_args_set(btheme->tseq.audio,  46, 143, 143, 255);
	rgba_char_args_set(btheme->tseq.effect,     169, 84, 124, 255);
	rgba_char_args_set(btheme->tseq.transition, 162, 95, 111, 255);
	rgba_char_args_set(btheme->tseq.meta,   109, 145, 131, 255);
	rgba_char_args_set(btheme->tseq.text_strip,   162, 151, 0, 255);
	rgba_char_args_set(btheme->tseq.preview_back,   0, 0, 0, 255);
	rgba_char_args_set(btheme->tseq.grid,   64, 64, 64, 255);

	/* space image */
	btheme->tima = btheme->tv3d;
	rgba_char_args_set(btheme->tima.back,   53, 53, 53, 255);
	rgba_char_args_set(btheme->tima.vertex, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tima.vertex_select, 255, 133, 0, 255);
	rgba_char_args_set(btheme->tima.wire_edit, 192, 192, 192, 255);
	rgba_char_args_set(btheme->tima.edge_select, 255, 133, 0, 255);
	btheme->tima.vertex_size = 3;
	btheme->tima.facedot_size = 3;
	rgba_char_args_set(btheme->tima.face,   255, 255, 255, 10);
	rgba_char_args_set(btheme->tima.face_select, 255, 133, 0, 60);
	rgba_char_args_set(btheme->tima.editmesh_active, 255, 255, 255, 128);
	rgba_char_args_set_fl(btheme->tima.preview_back,        0.0, 0.0, 0.0, 0.3);
	rgba_char_args_set_fl(btheme->tima.preview_stitch_face, 0.5, 0.5, 0.0, 0.2);
	rgba_char_args_set_fl(btheme->tima.preview_stitch_edge, 1.0, 0.0, 1.0, 0.2);
	rgba_char_args_set_fl(btheme->tima.preview_stitch_vert, 0.0, 0.0, 1.0, 0.2);
	rgba_char_args_set_fl(btheme->tima.preview_stitch_stitchable, 0.0, 1.0, 0.0, 1.0);
	rgba_char_args_set_fl(btheme->tima.preview_stitch_unstitchable, 1.0, 0.0, 0.0, 1.0);
	rgba_char_args_set_fl(btheme->tima.preview_stitch_active, 0.886, 0.824, 0.765, 0.140);

	rgba_char_args_test_set(btheme->tima.uv_others, 96, 96, 96, 255);
	rgba_char_args_test_set(btheme->tima.uv_shadow, 112, 112, 112, 255);

	ui_theme_space_init_handles_color(&btheme->tima);
	btheme->tima.handle_vertex_size = 5;

	/* space text */
	btheme->text = btheme->tv3d;
	rgba_char_args_set(btheme->text.back,   153, 153, 153, 255);
	rgba_char_args_set(btheme->text.shade1,     143, 143, 143, 255);
	rgba_char_args_set(btheme->text.shade2,     0xc6, 0x77, 0x77, 255);
	rgba_char_args_set(btheme->text.hilite,     255, 0, 0, 255);
	
	/* syntax highlighting */
	rgba_char_args_set(btheme->text.syntaxn,    0, 0, 200, 255);    /* Numbers  Blue*/
	rgba_char_args_set(btheme->text.syntaxl,    100, 0, 0, 255);    /* Strings  Red */
	rgba_char_args_set(btheme->text.syntaxc,    0, 100, 50, 255);   /* Comments  Greenish */
	rgba_char_args_set(btheme->text.syntaxv,    95, 95, 0, 255);    /* Special  Yellow*/
	rgba_char_args_set(btheme->text.syntaxd,    50, 0, 140, 255);   /* Decorator/Preprocessor Dir.  Blue-purple */
	rgba_char_args_set(btheme->text.syntaxr,    140, 60, 0, 255);   /* Reserved  Orange*/
	rgba_char_args_set(btheme->text.syntaxb,    128, 0, 80, 255);   /* Builtin  Red-purple */
	rgba_char_args_set(btheme->text.syntaxs,    76, 76, 76, 255);   /* Gray (mix between fg/bg) */
	
	/* space oops */
	btheme->toops = btheme->tv3d;
	rgba_char_args_set_fl(btheme->toops.back,   0.45, 0.45, 0.45, 1.0);
	
	rgba_char_args_set_fl(btheme->toops.match,  0.2, 0.5, 0.2, 0.3);    /* highlighting search match - soft green*/
	rgba_char_args_set_fl(btheme->toops.selected_highlight, 0.51, 0.53, 0.55, 0.3);

	/* space info */
	btheme->tinfo = btheme->tv3d;
	rgba_char_args_set_fl(btheme->tinfo.back,   0.45, 0.45, 0.45, 1.0);
	rgba_char_args_set(btheme->tinfo.info_selected, 96, 128, 255, 255);
	rgba_char_args_set(btheme->tinfo.info_selected_text, 255, 255, 255, 255);
	rgba_char_args_set(btheme->tinfo.info_error, 220, 0, 0, 255);
	rgba_char_args_set(btheme->tinfo.info_error_text, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tinfo.info_warning, 220, 128, 96, 255);
	rgba_char_args_set(btheme->tinfo.info_warning_text, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tinfo.info_info, 0, 170, 0, 255);
	rgba_char_args_set(btheme->tinfo.info_info_text, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tinfo.info_debug, 196, 196, 196, 255);
	rgba_char_args_set(btheme->tinfo.info_debug_text, 0, 0, 0, 255);

	/* space user preferences */
	btheme->tuserpref = btheme->tv3d;
	rgba_char_args_set_fl(btheme->tuserpref.back, 0.45, 0.45, 0.45, 1.0);
	
	/* space console */
	btheme->tconsole = btheme->tv3d;
	rgba_char_args_set(btheme->tconsole.back, 0, 0, 0, 255);
	rgba_char_args_set(btheme->tconsole.console_output, 96, 128, 255, 255);
	rgba_char_args_set(btheme->tconsole.console_input, 255, 255, 255, 255);
	rgba_char_args_set(btheme->tconsole.console_info, 0, 170, 0, 255);
	rgba_char_args_set(btheme->tconsole.console_error, 220, 96, 96, 255);
	rgba_char_args_set(btheme->tconsole.console_cursor, 220, 96, 96, 255);
	rgba_char_args_set(btheme->tconsole.console_select, 255, 255, 255, 48);
	
	/* space time */
	btheme->ttime = btheme->tv3d;
	rgba_char_args_set_fl(btheme->ttime.back,   0.45, 0.45, 0.45, 1.0);
	rgba_char_args_set_fl(btheme->ttime.grid,   0.36, 0.36, 0.36, 1.0);
	rgba_char_args_set(btheme->ttime.shade1,  173, 173, 173, 255);      /* sliders */
	
	rgba_char_args_set(btheme->ttime.time_keyframe, 0xDD, 0xD7, 0x00, 0xFF);
	rgba_char_args_set(btheme->ttime.time_gp_keyframe, 0xB5, 0xE6, 0x1D, 0xFF);
	
	/* space node, re-uses syntax and console color storage */
	btheme->tnode = btheme->tv3d;
	rgba_char_args_set(btheme->tnode.syntaxr, 115, 115, 115, 255);  /* wire inner color */
	rgba_char_args_set(btheme->tnode.edge_select, 255, 255, 255, 255);  /* wire selected */
	rgba_char_args_set(btheme->tnode.syntaxl, 155, 155, 155, 160);  /* TH_NODE, backdrop */
	rgba_char_args_set(btheme->tnode.syntaxn, 100, 100, 100, 255);  /* in */
	rgba_char_args_set(btheme->tnode.nodeclass_output, 100, 100, 100, 255);  /* output */
	rgba_char_args_set(btheme->tnode.syntaxb, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.syntaxv, 104, 106, 117, 255);  /* generator */
	rgba_char_args_set(btheme->tnode.syntaxc, 105, 117, 110, 255);  /* group */
	rgba_char_args_set(btheme->tnode.nodeclass_texture, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.nodeclass_shader, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.nodeclass_filter, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.nodeclass_script, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.nodeclass_pattern, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.nodeclass_vector, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.nodeclass_layout, 108, 105, 111, 255);  /* operator */
	rgba_char_args_set(btheme->tnode.movie, 155, 155, 155, 160);  /* frame */
	rgba_char_args_set(btheme->tnode.syntaxs, 151, 116, 116, 255);  /* matte nodes */
	rgba_char_args_set(btheme->tnode.syntaxd, 116, 151, 151, 255);  /* distort nodes */
	rgba_char_args_set(btheme->tnode.console_output, 223, 202, 53, 255);  /* interface nodes */
	btheme->tnode.noodle_curving = 5;

	/* space logic */
	btheme->tlogic = btheme->tv3d;
	rgba_char_args_set(btheme->tlogic.back, 100, 100, 100, 255);
	
	/* space clip */
	btheme->tclip = btheme->tv3d;

	rgba_char_args_set(btheme->tclip.marker_outline, 0x00, 0x00, 0x00, 255);
	rgba_char_args_set(btheme->tclip.marker, 0x7f, 0x7f, 0x00, 255);
	rgba_char_args_set(btheme->tclip.act_marker, 0xff, 0xff, 0xff, 255);
	rgba_char_args_set(btheme->tclip.sel_marker, 0xff, 0xff, 0x00, 255);
	rgba_char_args_set(btheme->tclip.dis_marker, 0x7f, 0x00, 0x00, 255);
	rgba_char_args_set(btheme->tclip.lock_marker, 0x7f, 0x7f, 0x7f, 255);
	rgba_char_args_set(btheme->tclip.path_before, 0xff, 0x00, 0x00, 255);
	rgba_char_args_set(btheme->tclip.path_after, 0x00, 0x00, 0xff, 255);
	rgba_char_args_set(btheme->tclip.grid, 0x5e, 0x5e, 0x5e, 255);
	rgba_char_args_set(btheme->tclip.cframe, 0x60, 0xc0, 0x40, 255);
	rgba_char_args_set(btheme->tclip.list, 0x66, 0x66, 0x66, 0xff);
	rgba_char_args_set(btheme->tclip.strip, 0x0c, 0x0a, 0x0a, 0x80);
	rgba_char_args_set(btheme->tclip.strip_select, 0xff, 0x8c, 0x00, 0xff);
	btheme->tclip.handle_vertex_size = 5;
	ui_theme_space_init_handles_color(&btheme->tclip);
}

void ui_style_init_default(void)
{
	BLI_freelistN(&U.uistyles);
	/* gets automatically re-allocated */
	uiStyleInit();
}


void UI_SetTheme(int spacetype, int regionid)
{
	if (spacetype) {
		/* later on, a local theme can be found too */
		theme_active = U.themes.first;
		theme_spacetype = spacetype;
		theme_regionid = regionid;
	}
	else if (regionid) {
		/* popups */
		theme_active = U.themes.first;
		theme_spacetype = SPACE_BUTS;
		theme_regionid = regionid;
	}
	else {
		/* for safety, when theme was deleted */
		theme_active = U.themes.first;
		theme_spacetype = SPACE_VIEW3D;
		theme_regionid = RGN_TYPE_WINDOW;
	}
}

bTheme *UI_GetTheme(void)
{
	return U.themes.first;
}

/**
 * for the rare case we need to temp swap in a different theme (offscreen render)
 */
void UI_Theme_Store(struct bThemeState *theme_state)
{
	*theme_state = g_theme_state;
}
void UI_Theme_Restore(struct bThemeState *theme_state)
{
	g_theme_state = *theme_state;
}

/* for space windows only */
void UI_ThemeColor(int colorid)
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor3ubv(cp);

}

/* plus alpha */
void UI_ThemeColor4(int colorid)
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor4ubv(cp);

}

/* set the color with offset for shades */
void UI_ThemeColorShade(int colorid, int offset)
{
	int r, g, b;
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r = offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g = offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b = offset + (int) cp[2];
	CLAMP(b, 0, 255);
	glColor4ub(r, g, b, cp[3]);
}
void UI_ThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
	int r, g, b, a;
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r = coloffset + (int) cp[0];
	CLAMP(r, 0, 255);
	g = coloffset + (int) cp[1];
	CLAMP(g, 0, 255);
	b = coloffset + (int) cp[2];
	CLAMP(b, 0, 255);
	a = alphaoffset + (int) cp[3];
	CLAMP(a, 0, 255);
	glColor4ub(r, g, b, a);
}

void UI_GetThemeColorBlend3ubv(int colorid1, int colorid2, float fac, unsigned char col[3])
{
	const unsigned char *cp1, *cp2;

	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	col[0] = floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	col[1] = floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	col[2] = floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
}

/* blend between to theme colors, and set it */
void UI_ThemeColorBlend(int colorid1, int colorid2, float fac)
{
	unsigned char col[3];
	UI_GetThemeColorBlend3ubv(colorid1, colorid2, fac, col);
	glColor3ubv(col);
}

/* blend between to theme colors, shade it, and set it */
void UI_ThemeColorBlendShade(int colorid1, int colorid2, float fac, int offset)
{
	int r, g, b;
	const unsigned char *cp1, *cp2;
	
	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
	
	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);
	
	glColor3ub(r, g, b);
}

/* blend between to theme colors, shade it, and set it */
void UI_ThemeColorBlendShadeAlpha(int colorid1, int colorid2, float fac, int offset, int alphaoffset)
{
	int r, g, b, a;
	const unsigned char *cp1, *cp2;
	
	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
	a = alphaoffset + floorf((1.0f - fac) * cp1[3] + fac * cp2[3]);
	
	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);
	CLAMP(a, 0, 255);

	glColor4ub(r, g, b, a);
}


/* get individual values, not scaled */
float UI_GetThemeValuef(int colorid)
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((float)cp[0]);
}

/* get individual values, not scaled */
int UI_GetThemeValue(int colorid)
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((int) cp[0]);
}

/* versions of the function above, which take a space-type */
float UI_GetThemeValueTypef(int colorid, int spacetype)
{
	const unsigned char *cp;

	cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
	return ((float)cp[0]);
}

int UI_GetThemeValueType(int colorid, int spacetype)
{
	const unsigned char *cp;

	cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
	return ((int)cp[0]);
}


/* get the color, range 0.0-1.0 */
void UI_GetThemeColor3fv(int colorid, float col[3])
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0] = ((float)cp[0]) / 255.0f;
	col[1] = ((float)cp[1]) / 255.0f;
	col[2] = ((float)cp[2]) / 255.0f;
}

void UI_GetThemeColor4fv(int colorid, float col[4])
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0] = ((float)cp[0]) / 255.0f;
	col[1] = ((float)cp[1]) / 255.0f;
	col[2] = ((float)cp[2]) / 255.0f;
	col[3] = ((float)cp[3]) / 255.0f;
}

/* get the color, range 0.0-1.0, complete with shading offset */
void UI_GetThemeColorShade3fv(int colorid, int offset, float col[3])
{
	int r, g, b;
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	
	r = offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g = offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b = offset + (int) cp[2];
	CLAMP(b, 0, 255);
	
	col[0] = ((float)r) / 255.0f;
	col[1] = ((float)g) / 255.0f;
	col[2] = ((float)b) / 255.0f;
}

void UI_GetThemeColorShade3ubv(int colorid, int offset, unsigned char col[3])
{
	int r, g, b;
	const unsigned char *cp;

	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);

	r = offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g = offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b = offset + (int) cp[2];
	CLAMP(b, 0, 255);

	col[0] = r;
	col[1] = g;
	col[2] = b;
}

/* get the color, in char pointer */
void UI_GetThemeColor3ubv(int colorid, unsigned char col[3])
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0] = cp[0];
	col[1] = cp[1];
	col[2] = cp[2];
}

/* get the color, range 0.0-1.0, complete with shading offset */
void UI_GetThemeColorShade4fv(int colorid, int offset, float col[4])
{
	int r, g, b, a;
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	
	r = offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g = offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b = offset + (int) cp[2];
	CLAMP(b, 0, 255);
	
	a = (int) cp[3]; /* no shading offset... */
	CLAMP(a, 0, 255);
	
	col[0] = ((float)r) / 255.0f;
	col[1] = ((float)g) / 255.0f;
	col[2] = ((float)b) / 255.0f;
	col[3] = ((float)a) / 255.0f;
}

/* get the color, in char pointer */
void UI_GetThemeColor4ubv(int colorid, unsigned char col[4])
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0] = cp[0];
	col[1] = cp[1];
	col[2] = cp[2];
	col[3] = cp[3];
}

void UI_GetThemeColorType4ubv(int colorid, int spacetype, char col[4])
{
	const unsigned char *cp;
	
	cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
	col[0] = cp[0];
	col[1] = cp[1];
	col[2] = cp[2];
	col[3] = cp[3];
}

/* blends and shades between two char color pointers */
void UI_ColorPtrBlendShade3ubv(const unsigned char cp1[3], const unsigned char cp2[3], float fac, int offset)
{
	int r, g, b;
	CLAMP(fac, 0.0f, 1.0f);
	r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);

	r = r < 0 ? 0 : (r > 255 ? 255 : r);
	g = g < 0 ? 0 : (g > 255 ? 255 : g);
	b = b < 0 ? 0 : (b > 255 ? 255 : b);
	
	glColor3ub(r, g, b);
}

void UI_GetColorPtrShade3ubv(const unsigned char cp[3], unsigned char col[3], int offset)
{
	int r, g, b;

	r = offset + (int)cp[0];
	g = offset + (int)cp[1];
	b = offset + (int)cp[2];

	CLAMP(r, 0, 255);
	CLAMP(g, 0, 255);
	CLAMP(b, 0, 255);

	col[0] = r;
	col[1] = g;
	col[2] = b;
}

/* get a 3 byte color, blended and shaded between two other char color pointers */
void UI_GetColorPtrBlendShade3ubv(
        const unsigned char cp1[3], const unsigned char cp2[3], unsigned char col[3],
        float fac, int offset)
{
	int r, g, b;

	CLAMP(fac, 0.0f, 1.0f);
	r = offset + floor((1.0f - fac) * cp1[0] + fac * cp2[0]);
	g = offset + floor((1.0f - fac) * cp1[1] + fac * cp2[1]);
	b = offset + floor((1.0f - fac) * cp1[2] + fac * cp2[2]);

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
	glClearColor(col[0], col[1], col[2], 0.0f);
}

void UI_ThemeClearColorAlpha(int colorid, float alpha)
{
	float col[3];
	UI_GetThemeColor3fv(colorid, col);
	glClearColor(col[0], col[1], col[2], alpha);
}


int UI_ThemeMenuShadowWidth(void)
{
	bTheme *btheme = UI_GetTheme();
	return (int)(btheme->tui.menu_shadow_width * UI_DPI_FAC);
}

void UI_make_axis_color(const unsigned char src_col[3], unsigned char dst_col[3], const char axis)
{
	unsigned char col[3];
	
	switch (axis) {
		case 'X':
			UI_GetThemeColor3ubv(TH_AXIS_X, col);
			UI_GetColorPtrBlendShade3ubv(src_col, col, dst_col, 0.5f, -10);
			break;
		case 'Y':
			UI_GetThemeColor3ubv(TH_AXIS_Y, col);
			UI_GetColorPtrBlendShade3ubv(src_col, col, dst_col, 0.5f, -10);
			break;
		case 'Z':
			UI_GetThemeColor3ubv(TH_AXIS_Z, col);
			UI_GetColorPtrBlendShade3ubv(src_col, col, dst_col, 0.5f, -10);
			break;
		default:
			BLI_assert(0);
			break;
	}
}

/* ************************************************************* */

/* patching UserDef struct and Themes */
void init_userdef_do_versions(void)
{
	Main *bmain = G.main;
	
#define USER_VERSION_ATLEAST(ver, subver) MAIN_VERSION_ATLEAST(bmain, ver, subver)

	/* the UserDef struct is not corrected with do_versions() .... ugh! */
	if (U.wheellinescroll == 0) U.wheellinescroll = 3;
	if (U.menuthreshold1 == 0) {
		U.menuthreshold1 = 5;
		U.menuthreshold2 = 2;
	}
	if (U.tb_leftmouse == 0) {
		U.tb_leftmouse = 5;
		U.tb_rightmouse = 5;
	}
	if (U.mixbufsize == 0) U.mixbufsize = 2048;
	if (STREQ(U.tempdir, "/")) {
		BKE_tempdir_system_init(U.tempdir);
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
	if (U.tw_hotspot == 0) {
		U.tw_hotspot = 14;
		U.tw_size = 25;          /* percentage of window size */
		U.tw_handlesize = 16;    /* percentage of widget radius */
	}
	if (U.pad_rot_angle == 0.0f)
		U.pad_rot_angle = 15.0f;
	
	/* graph editor - unselected F-Curve visibility */
	if (U.fcu_inactive_alpha == 0) {
		U.fcu_inactive_alpha = 0.25f;
	}
	
	/* signal for derivedmesh to use colorband */
	/* run in case this was on and is now off in the user prefs [#28096] */
	vDM_ColorBand_store((U.flag & USER_CUSTOM_RANGE) ? (&U.coba_weight) : NULL, UI_GetTheme()->tv3d.vertex_unreferenced);

	if (!USER_VERSION_ATLEAST(192, 0)) {
		strcpy(U.sounddir, "/");
	}
	
	/* patch to set Dupli Armature */
	if (!USER_VERSION_ATLEAST(220, 0)) {
		U.dupflag |= USER_DUP_ARM;
	}
	
	/* added seam, normal color, undo */
	if (!USER_VERSION_ATLEAST(235, 0)) {
		bTheme *btheme;
		
		U.uiflag |= USER_GLOBALUNDO;
		if (U.undosteps == 0) U.undosteps = 32;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* check for (alpha == 0) is safe, then color was never set */
			if (btheme->tv3d.edge_seam[3] == 0) {
				rgba_char_args_set(btheme->tv3d.edge_seam, 230, 150, 50, 255);
			}
			if (btheme->tv3d.normal[3] == 0) {
				rgba_char_args_set(btheme->tv3d.normal, 0x22, 0xDD, 0xDD, 255);
			}
			if (btheme->tv3d.vertex_normal[3] == 0) {
				rgba_char_args_set(btheme->tv3d.vertex_normal, 0x23, 0x61, 0xDD, 255);
			}
			if (btheme->tv3d.face_dot[3] == 0) {
				rgba_char_args_set(btheme->tv3d.face_dot, 255, 138, 48, 255);
				btheme->tv3d.facedot_size = 4;
			}
		}
	}
	if (!USER_VERSION_ATLEAST(236, 0)) {
		/* illegal combo... */
		if (U.flag & USER_LMOUSESELECT) 
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (!USER_VERSION_ATLEAST(237, 0)) {
		bTheme *btheme;
		/* new space type */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* check for (alpha == 0) is safe, then color was never set */
			if (btheme->ttime.back[3] == 0) {
				/* copied from ui_theme_init_default */
				btheme->ttime = btheme->tv3d;
				rgba_char_args_set_fl(btheme->ttime.back,   0.45, 0.45, 0.45, 1.0);
				rgba_char_args_set_fl(btheme->ttime.grid,   0.36, 0.36, 0.36, 1.0);
				rgba_char_args_set(btheme->ttime.shade1,  173, 173, 173, 255);  /* sliders */
			}
			if (btheme->text.syntaxn[3] == 0) {
				rgba_char_args_set(btheme->text.syntaxn,    0, 0, 200, 255);    /* Numbers  Blue*/
				rgba_char_args_set(btheme->text.syntaxl,    100, 0, 0, 255);    /* Strings  red */
				rgba_char_args_set(btheme->text.syntaxc,    0, 100, 50, 255);   /* Comments greenish */
				rgba_char_args_set(btheme->text.syntaxv,    95, 95, 0, 255);    /* Special */
				rgba_char_args_set(btheme->text.syntaxb,    128, 0, 80, 255);   /* Builtin, red-purple */
			}
		}
	}
	if (!USER_VERSION_ATLEAST(238, 0)) {
		bTheme *btheme;
		/* bone colors */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if (btheme->tv3d.bone_solid[3] == 0) {
				rgba_char_args_set(btheme->tv3d.bone_solid, 200, 200, 200, 255);
				rgba_char_args_set(btheme->tv3d.bone_pose, 80, 200, 255, 80);
			}
		}
	}
	if (!USER_VERSION_ATLEAST(239, 0)) {
		bTheme *btheme;
		/* bone colors */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* check for alpha==0 is safe, then color was never set */
			if (btheme->tnla.strip[3] == 0) {
				rgba_char_args_set(btheme->tnla.strip_select,   0xff, 0xff, 0xaa, 255);
				rgba_char_args_set(btheme->tnla.strip, 0xe4, 0x9c, 0xc6, 255);
			}
		}
	}
	if (!USER_VERSION_ATLEAST(240, 0)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* Lamp theme, check for alpha==0 is safe, then color was never set */
			if (btheme->tv3d.lamp[3] == 0) {
				rgba_char_args_set(btheme->tv3d.lamp,   0, 0, 0, 40);
/* TEMPORAL, remove me! (ton) */				
				U.uiflag |= USER_PLAINMENUS;
			}
			
		}
		if (U.obcenter_dia == 0) U.obcenter_dia = 6;
	}
	if (!USER_VERSION_ATLEAST(242, 0)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* Node editor theme, check for alpha==0 is safe, then color was never set */
			if (btheme->tnode.syntaxn[3] == 0) {
				/* re-uses syntax color storage */
				btheme->tnode = btheme->tv3d;
				rgba_char_args_set(btheme->tnode.edge_select, 255, 255, 255, 255);
				rgba_char_args_set(btheme->tnode.syntaxl, 150, 150, 150, 255);  /* TH_NODE, backdrop */
				rgba_char_args_set(btheme->tnode.syntaxn, 129, 131, 144, 255);  /* in/output */
				rgba_char_args_set(btheme->tnode.syntaxb, 127, 127, 127, 255);  /* operator */
				rgba_char_args_set(btheme->tnode.syntaxv, 142, 138, 145, 255);  /* generator */
				rgba_char_args_set(btheme->tnode.syntaxc, 120, 145, 120, 255);  /* group */
			}
			/* Group theme colors */
			if (btheme->tv3d.group[3] == 0) {
				rgba_char_args_set(btheme->tv3d.group, 0x0C, 0x30, 0x0C, 255);
				rgba_char_args_set(btheme->tv3d.group_active, 0x66, 0xFF, 0x66, 255);
			}
			/* Sequence editor theme*/
			if (btheme->tseq.movie[3] == 0) {
				rgba_char_args_set(btheme->tseq.movie,  81, 105, 135, 255);
				rgba_char_args_set(btheme->tseq.image,  109, 88, 129, 255);
				rgba_char_args_set(btheme->tseq.scene,  78, 152, 62, 255);
				rgba_char_args_set(btheme->tseq.audio,  46, 143, 143, 255);
				rgba_char_args_set(btheme->tseq.effect,     169, 84, 124, 255);
				rgba_char_args_set(btheme->tseq.transition, 162, 95, 111, 255);
				rgba_char_args_set(btheme->tseq.meta,   109, 145, 131, 255);
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
	if (!USER_VERSION_ATLEAST(243, 0)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* long keyframe color */
			/* check for alpha==0 is safe, then color was never set */
			if (btheme->tact.strip[3] == 0) {
				rgba_char_args_set(btheme->tv3d.edge_sharp, 255, 32, 32, 255);
				rgba_char_args_set(btheme->tact.strip_select,   0xff, 0xff, 0xaa, 204);
				rgba_char_args_set(btheme->tact.strip, 0xe4, 0x9c, 0xc6, 204);
			}
			
			/* IPO-Editor - Vertex Size*/
			if (btheme->tipo.vertex_size == 0) {
				btheme->tipo.vertex_size = 3;
			}
		}
	}
	if (!USER_VERSION_ATLEAST(244, 0)) {
		/* set default number of recently-used files (if not set) */
		if (U.recent_files == 0) U.recent_files = 10;
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tv3d.editmesh_active, 255, 255, 255, 128);
		}
		if (U.coba_weight.tot == 0)
			init_colorband(&U.coba_weight, true);
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* these should all use the same color */
			rgba_char_args_set(btheme->tv3d.cframe, 0x60, 0xc0, 0x40, 255);
			rgba_char_args_set(btheme->tipo.cframe, 0x60, 0xc0, 0x40, 255);
			rgba_char_args_set(btheme->tact.cframe, 0x60, 0xc0, 0x40, 255);
			rgba_char_args_set(btheme->tnla.cframe, 0x60, 0xc0, 0x40, 255);
			rgba_char_args_set(btheme->tseq.cframe, 0x60, 0xc0, 0x40, 255);
			//rgba_char_args_set(btheme->tsnd.cframe, 0x60, 0xc0, 0x40, 255); Not needed anymore
			rgba_char_args_set(btheme->ttime.cframe, 0x60, 0xc0, 0x40, 255);
		}
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* action channel groups (recolor anyway) */
			rgba_char_args_set(btheme->tact.group, 0x39, 0x7d, 0x1b, 255);
			rgba_char_args_set(btheme->tact.group_active, 0x7d, 0xe9, 0x60, 255);
			
			/* bone custom-color sets */
			if (btheme->tarm[0].solid[3] == 0)
				ui_theme_init_boneColorSets(btheme);
		}
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		U.flag |= USER_ADD_VIEWALIGNED | USER_ADD_EDITMODE;
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		bTheme *btheme;
		
		/* adjust themes */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			const char *col;
			
			/* IPO Editor: Handles/Vertices */
			col = btheme->tipo.vertex;
			rgba_char_args_set(btheme->tipo.handle_vertex, col[0], col[1], col[2], 255);
			col = btheme->tipo.vertex_select;
			rgba_char_args_set(btheme->tipo.handle_vertex_select, col[0], col[1], col[2], 255);
			btheme->tipo.handle_vertex_size = btheme->tipo.vertex_size;
			
			/* Sequence/Image Editor: colors for GPencil text */
			col = btheme->tv3d.bone_pose;
			rgba_char_args_set(btheme->tseq.bone_pose, col[0], col[1], col[2], 255);
			rgba_char_args_set(btheme->tima.bone_pose, col[0], col[1], col[2], 255);
			col = btheme->tv3d.vertex_select;
			rgba_char_args_set(btheme->tseq.vertex_select, col[0], col[1], col[2], 255);
		}
	}
	if (!USER_VERSION_ATLEAST(250, 0)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* this was not properly initialized in 2.45 */
			if (btheme->tima.face_dot[3] == 0) {
				rgba_char_args_set(btheme->tima.editmesh_active, 255, 255, 255, 128);
				rgba_char_args_set(btheme->tima.face_dot, 255, 133, 0, 255);
				btheme->tima.facedot_size = 2;
			}
			
			/* DopeSheet - (Object) Channel color */
			rgba_char_args_set(btheme->tact.ds_channel,     82, 96, 110, 255);
			rgba_char_args_set(btheme->tact.ds_subchannel,  124, 137, 150, 255);
			/* DopeSheet - Group Channel color (saner version) */
			rgba_char_args_set(btheme->tact.group, 79, 101, 73, 255);
			rgba_char_args_set(btheme->tact.group_active, 135, 177, 125, 255);
			
			/* Graph Editor - (Object) Channel color */
			rgba_char_args_set(btheme->tipo.ds_channel,     82, 96, 110, 255);
			rgba_char_args_set(btheme->tipo.ds_subchannel,  124, 137, 150, 255);
			/* Graph Editor - Group Channel color */
			rgba_char_args_set(btheme->tipo.group, 79, 101, 73, 255);
			rgba_char_args_set(btheme->tipo.group_active, 135, 177, 125, 255);
			
			/* Nla Editor - (Object) Channel color */
			rgba_char_args_set(btheme->tnla.ds_channel,     82, 96, 110, 255);
			rgba_char_args_set(btheme->tnla.ds_subchannel,  124, 137, 150, 255);
			/* NLA Editor - New Strip colors */
			rgba_char_args_set(btheme->tnla.strip,          12, 10, 10, 128);
			rgba_char_args_set(btheme->tnla.strip_select,   255, 140, 0, 255);
		}
		
		/* adjust grease-pencil distances */
		U.gp_manhattendist = 1;
		U.gp_euclideandist = 2;
		
		/* adjust default interpolation for new IPO-curves */
		U.ipo_new = BEZT_IPO_BEZ;
	}
	
	if (!USER_VERSION_ATLEAST(250, 1)) {
		bTheme *btheme;

		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			
			/* common (new) variables, it checks for alpha==0 */
			ui_theme_init_new(btheme);

			if (btheme->tui.wcol_num.outline[3] == 0)
				ui_widget_color_init(&btheme->tui);
			
			/* Logic editor theme, check for alpha==0 is safe, then color was never set */
			if (btheme->tlogic.syntaxn[3] == 0) {
				/* re-uses syntax color storage */
				btheme->tlogic = btheme->tv3d;
				rgba_char_args_set(btheme->tlogic.back, 100, 100, 100, 255);
			}

			rgba_char_args_set_fl(btheme->tinfo.back, 0.45, 0.45, 0.45, 1.0);
			rgba_char_args_set_fl(btheme->tuserpref.back, 0.45, 0.45, 0.45, 1.0);
		}
	}

	if (!USER_VERSION_ATLEAST(250, 3)) {
		/* new audio system */
		if (U.audiochannels == 0)
			U.audiochannels = 2;
		if (U.audiodevice == 0) {
#ifdef WITH_OPENAL
			U.audiodevice = 2;
#endif
#ifdef WITH_SDL
			U.audiodevice = 1;
#endif
		}
		if (U.audioformat == 0)
			U.audioformat = 0x24;
		if (U.audiorate == 0)
			U.audiorate = 48000;
	}
	
	if (!USER_VERSION_ATLEAST(250, 8)) {
		wmKeyMap *km;
		
		for (km = U.user_keymaps.first; km; km = km->next) {
			if (STREQ(km->idname, "Armature_Sketch"))
				strcpy(km->idname, "Armature Sketch");
			else if (STREQ(km->idname, "View3D"))
				strcpy(km->idname, "3D View");
			else if (STREQ(km->idname, "View3D Generic"))
				strcpy(km->idname, "3D View Generic");
			else if (STREQ(km->idname, "EditMesh"))
				strcpy(km->idname, "Mesh");
			else if (STREQ(km->idname, "TimeLine"))
				strcpy(km->idname, "Timeline");
			else if (STREQ(km->idname, "UVEdit"))
				strcpy(km->idname, "UV Editor");
			else if (STREQ(km->idname, "Animation_Channels"))
				strcpy(km->idname, "Animation Channels");
			else if (STREQ(km->idname, "GraphEdit Keys"))
				strcpy(km->idname, "Graph Editor");
			else if (STREQ(km->idname, "GraphEdit Generic"))
				strcpy(km->idname, "Graph Editor Generic");
			else if (STREQ(km->idname, "Action_Keys"))
				strcpy(km->idname, "Dopesheet");
			else if (STREQ(km->idname, "NLA Data"))
				strcpy(km->idname, "NLA Editor");
			else if (STREQ(km->idname, "Node Generic"))
				strcpy(km->idname, "Node Editor");
			else if (STREQ(km->idname, "Logic Generic"))
				strcpy(km->idname, "Logic Editor");
			else if (STREQ(km->idname, "File"))
				strcpy(km->idname, "File Browser");
			else if (STREQ(km->idname, "FileMain"))
				strcpy(km->idname, "File Browser Main");
			else if (STREQ(km->idname, "FileButtons"))
				strcpy(km->idname, "File Browser Buttons");
			else if (STREQ(km->idname, "Buttons Generic"))
				strcpy(km->idname, "Property Editor");
		}
	}
	if (!USER_VERSION_ATLEAST(250, 16)) {
		if (U.wmdrawmethod == USER_DRAW_TRIPLE)
			U.wmdrawmethod = USER_DRAW_AUTOMATIC;
	}
	
	if (!USER_VERSION_ATLEAST(252, 3)) {
		if (U.flag & USER_LMOUSESELECT) 
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (!USER_VERSION_ATLEAST(252, 4)) {
		bTheme *btheme;
		
		/* default new handle type is auto handles */
		U.keyhandles_new = HD_AUTO;
		
		/* init new curve colors */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			ui_theme_space_init_handles_color(&btheme->tv3d);
			ui_theme_space_init_handles_color(&btheme->tipo);

			/* edge crease */
			rgba_char_args_set_fl(btheme->tv3d.edge_crease, 0.8, 0, 0.6, 1.0);
		}
	}
	if (!USER_VERSION_ATLEAST(253, 0)) {
		bTheme *btheme;

		/* init new curve colors */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tv3d.lastsel_point[3] == 0)
				rgba_char_args_set(btheme->tv3d.lastsel_point, 0xff, 0xff, 0xff, 255);
		}
	}
	if (!USER_VERSION_ATLEAST(252, 5)) {
		bTheme *btheme;
		
		/* interface_widgets.c */
		struct uiWidgetColors wcol_progress = {
			{0, 0, 0, 255},
			{190, 190, 190, 255},
			{100, 100, 100, 180},
			{128, 128, 128, 255},
			
			{0, 0, 0, 255},
			{255, 255, 255, 255},
			
			0,
			5, -5
		};
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* init progress bar theme */
			btheme->tui.wcol_progress = wcol_progress;
		}
	}

	if (!USER_VERSION_ATLEAST(255, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tv3d.extra_edge_len, 32, 0, 0, 255);
			rgba_char_args_set(btheme->tv3d.extra_face_angle, 0, 32, 0, 255);
			rgba_char_args_set(btheme->tv3d.extra_face_area, 0, 0, 128, 255);
		}
	}
	
	if (!USER_VERSION_ATLEAST(256, 4)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if ((btheme->tv3d.outline_width) == 0) btheme->tv3d.outline_width = 1;
		}
	}

	if (!USER_VERSION_ATLEAST(257, 0)) {
		/* clear "AUTOKEY_FLAG_ONLYKEYINGSET" flag from userprefs,
		 * so that it doesn't linger around from old configs like a ghost */
		U.autokey_flag &= ~AUTOKEY_FLAG_ONLYKEYINGSET;
	}

	if (!USER_VERSION_ATLEAST(258, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			btheme->tnode.noodle_curving = 5;
		}
	}

	if (!USER_VERSION_ATLEAST(259, 1)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			btheme->tv3d.speaker[3] = 255;
		}
	}

	if (!USER_VERSION_ATLEAST(260, 3)) {
		bTheme *btheme;
		
		/* if new keyframes handle default is stuff "auto", make it "auto-clamped" instead 
		 * was changed in 260 as part of GSoC11, but version patch was wrong
		 */
		if (U.keyhandles_new == HD_AUTO) 
			U.keyhandles_new = HD_AUTO_ANIM;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tv3d.bundle_solid[3] == 0)
				rgba_char_args_set(btheme->tv3d.bundle_solid, 200, 200, 200, 255);
			
			if (btheme->tv3d.camera_path[3] == 0)
				rgba_char_args_set(btheme->tv3d.camera_path, 0x00, 0x00, 0x00, 255);
				
			if ((btheme->tclip.back[3]) == 0) {
				btheme->tclip = btheme->tv3d;
				
				rgba_char_args_set(btheme->tclip.marker_outline, 0x00, 0x00, 0x00, 255);
				rgba_char_args_set(btheme->tclip.marker, 0x7f, 0x7f, 0x00, 255);
				rgba_char_args_set(btheme->tclip.act_marker, 0xff, 0xff, 0xff, 255);
				rgba_char_args_set(btheme->tclip.sel_marker, 0xff, 0xff, 0x00, 255);
				rgba_char_args_set(btheme->tclip.dis_marker, 0x7f, 0x00, 0x00, 255);
				rgba_char_args_set(btheme->tclip.lock_marker, 0x7f, 0x7f, 0x7f, 255);
				rgba_char_args_set(btheme->tclip.path_before, 0xff, 0x00, 0x00, 255);
				rgba_char_args_set(btheme->tclip.path_after, 0x00, 0x00, 0xff, 255);
				rgba_char_args_set(btheme->tclip.grid, 0x5e, 0x5e, 0x5e, 255);
				rgba_char_args_set(btheme->tclip.cframe, 0x60, 0xc0, 0x40, 255);
				rgba_char_args_set(btheme->tclip.handle_vertex, 0x00, 0x00, 0x00, 0xff);
				rgba_char_args_set(btheme->tclip.handle_vertex_select, 0xff, 0xff, 0, 0xff);
				btheme->tclip.handle_vertex_size = 5;
			}
			
			/* auto-clamped handles -> based on auto */
			if (btheme->tipo.handle_auto_clamped[3] == 0)
				rgba_char_args_set(btheme->tipo.handle_auto_clamped, 0x99, 0x40, 0x30, 255);
			if (btheme->tipo.handle_sel_auto_clamped[3] == 0)
				rgba_char_args_set(btheme->tipo.handle_sel_auto_clamped, 0xf0, 0xaf, 0x90, 255);
		}
		
		/* enable (Cycles) addon by default */
		if (!BLI_findstring(&U.addons, "cycles", offsetof(bAddon, module))) {
			bAddon *baddon = MEM_callocN(sizeof(bAddon), "bAddon");
			BLI_strncpy(baddon->module, "cycles", sizeof(baddon->module));
			BLI_addtail(&U.addons, baddon);
		}
	}
	
	if (!USER_VERSION_ATLEAST(260, 5)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tui.panel.header, 0, 0, 0, 25);
			btheme->tui.icon_alpha = 1.0;
		}
	}
	
	if (!USER_VERSION_ATLEAST(261, 4)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set_fl(btheme->tima.preview_stitch_face, 0.071, 0.259, 0.694, 0.150);
			rgba_char_args_set_fl(btheme->tima.preview_stitch_edge, 1.0, 0.522, 0.0, 0.7);
			rgba_char_args_set_fl(btheme->tima.preview_stitch_vert, 1.0, 0.522, 0.0, 0.5);
			rgba_char_args_set_fl(btheme->tima.preview_stitch_stitchable, 0.0, 1.0, 0.0, 1.0);
			rgba_char_args_set_fl(btheme->tima.preview_stitch_unstitchable, 1.0, 0.0, 0.0, 1.0);
			rgba_char_args_set_fl(btheme->tima.preview_stitch_active, 0.886, 0.824, 0.765, 0.140);
			
			rgba_char_args_set_fl(btheme->toops.match, 0.2, 0.5, 0.2, 0.3);
			rgba_char_args_set_fl(btheme->toops.selected_highlight, 0.51, 0.53, 0.55, 0.3);
		}
		
		U.use_16bit_textures = true;
	}

	if (!USER_VERSION_ATLEAST(262, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tui.wcol_menu_item.item[3] == 255)
				rgba_char_args_set(btheme->tui.wcol_menu_item.item, 172, 172, 172, 128);
		}
	}

	if (!USER_VERSION_ATLEAST(262, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tui.wcol_tooltip.inner[3] == 0) {
				btheme->tui.wcol_tooltip = btheme->tui.wcol_menu_back;
			}
			if (btheme->tui.wcol_tooltip.text[0] == 160) { /* hrmf */
				rgba_char_args_set(btheme->tui.wcol_tooltip.text, 255, 255, 255, 255);
			}
		}
	}

	if (!USER_VERSION_ATLEAST(262, 4)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tseq.movieclip[3] == 0) {
				rgba_char_args_set(btheme->tseq.movieclip,  32, 32, 143, 255);
			}
		}
	}

	if (!USER_VERSION_ATLEAST(263, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tclip.strip[0] == 0) {
				rgba_char_args_set(btheme->tclip.list, 0x66, 0x66, 0x66, 0xff);
				rgba_char_args_set(btheme->tclip.strip, 0x0c, 0x0a, 0x0a, 0x80);
				rgba_char_args_set(btheme->tclip.strip_select, 0xff, 0x8c, 0x00, 0xff);
			}
		}
	}

	if (!USER_VERSION_ATLEAST(263, 6)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next)
			rgba_char_args_set(btheme->tv3d.skin_root, 180, 77, 77, 255);
	}
	
	if (!USER_VERSION_ATLEAST(263, 7)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* DopeSheet Summary */
			rgba_char_args_set(btheme->tact.anim_active,    204, 112, 26, 102); 
			
			/* NLA Colors */
			rgba_char_args_set(btheme->tnla.anim_active,     204, 112, 26, 102); /* same as dopesheet above */
			rgba_char_args_set(btheme->tnla.anim_non_active, 153, 135, 97, 77);
			
			rgba_char_args_set(btheme->tnla.nla_tweaking,   77, 243, 26, 77);
			rgba_char_args_set(btheme->tnla.nla_tweakdupli, 217, 0, 0, 255);
			
			rgba_char_args_set(btheme->tnla.nla_transition,     28, 38, 48, 255);
			rgba_char_args_set(btheme->tnla.nla_transition_sel, 46, 117, 219, 255);
			rgba_char_args_set(btheme->tnla.nla_meta,           51, 38, 66, 255);
			rgba_char_args_set(btheme->tnla.nla_meta_sel,       105, 33, 150, 255);
			rgba_char_args_set(btheme->tnla.nla_sound,          43, 61, 61, 255);
			rgba_char_args_set(btheme->tnla.nla_sound_sel,      31, 122, 122, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(263, 11)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tseq.mask[3] == 0) {
				rgba_char_args_set(btheme->tseq.mask,  152, 78, 62, 255);
			}
		}
	}

	if (!USER_VERSION_ATLEAST(263, 15)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tv3d.bone_pose_active, 140, 255, 255, 80);
		}
	}

	if (!USER_VERSION_ATLEAST(263, 16)) {
		bTheme *btheme;

		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tact.anim_active[3] == 0)
				rgba_char_args_set(btheme->tact.anim_active, 204, 112, 26, 102);

			if (btheme->tnla.anim_active[3] == 0)
				rgba_char_args_set(btheme->tnla.anim_active, 204, 112, 26, 102);
		}
	}

	if (!USER_VERSION_ATLEAST(263, 22)) {
		bTheme *btheme;

		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tipo.lastsel_point[3] == 0)
				rgba_char_args_set(btheme->tipo.lastsel_point, 0xff, 0xff, 0xff, 255);

			if (btheme->tv3d.skin_root[3] == 0)
				rgba_char_args_set(btheme->tv3d.skin_root, 180, 77, 77, 255);
		}
	}
	
	if (!USER_VERSION_ATLEAST(264, 9)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tui.xaxis, 220,   0,   0, 255);
			rgba_char_args_set(btheme->tui.yaxis,   0, 220,   0, 255);
			rgba_char_args_set(btheme->tui.zaxis,   0,   0, 220, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(267, 0)) {
		/* Freestyle color settings */
		bTheme *btheme;

		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* check for alpha == 0 is safe, then color was never set */
			if (btheme->tv3d.freestyle_edge_mark[3] == 0) {
				rgba_char_args_set(btheme->tv3d.freestyle_edge_mark, 0x7f, 0xff, 0x7f, 255);
				rgba_char_args_set(btheme->tv3d.freestyle_face_mark, 0x7f, 0xff, 0x7f, 51);
			}

			if (btheme->tv3d.wire_edit[3] == 0) {
				rgba_char_args_set(btheme->tv3d.wire_edit,  0x0, 0x0, 0x0, 255);
			}
		}

		/* GL Texture Garbage Collection */
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
		if (U.scrcastfps == 0) {
			U.scrcastfps = 10;
			U.scrcastwait = 50;
		}
		if (U.v2d_min_gridsize == 0) {
			U.v2d_min_gridsize = 35;
		}
		if (U.dragthreshold == 0)
			U.dragthreshold = 5;
		if (U.widget_unit == 0)
			U.widget_unit = 20;
		if (U.anisotropic_filter <= 0)
			U.anisotropic_filter = 1;

		if (U.ndof_sensitivity == 0.0f) {
			U.ndof_sensitivity = 1.0f;
			U.ndof_flag = (NDOF_LOCK_HORIZON | NDOF_SHOULD_PAN | NDOF_SHOULD_ZOOM | NDOF_SHOULD_ROTATE);
		}
		
		if (U.ndof_orbit_sensitivity == 0.0f) {
			U.ndof_orbit_sensitivity = U.ndof_sensitivity;

			if (!(U.flag & USER_TRACKBALL))
				U.ndof_flag |= NDOF_TURNTABLE;
		}
		if (U.tweak_threshold == 0)
			U.tweak_threshold = 10;
	}

	if (!USER_VERSION_ATLEAST(265, 1)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* note: the toggle operator for transparent backdrops limits to these spacetypes */
			if (btheme->tnode.button[3] == 255) {
				btheme->tv3d.button[3] = 128;
				btheme->tnode.button[3] = 128;
				btheme->tima.button[3] = 128;
				btheme->tseq.button[3] = 128;
				btheme->tclip.button[3] = 128;
			}
		}
	}
	
	/* panel header/backdrop supported locally per editor now */
	if (!USER_VERSION_ATLEAST(265, 2)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			ThemeSpace *ts;
			
			/* new color, panel backdrop. Not used anywhere yet, until you enable it */
			copy_v3_v3_char(btheme->tui.panel.back, btheme->tbuts.button);
			btheme->tui.panel.back[3] = 128;
			
			for (ts = UI_THEMESPACE_START(btheme); ts != UI_THEMESPACE_END(btheme); ts++) {
				ts->panelcolors = btheme->tui.panel;
			}
		}
	}

	/* NOTE!! from now on use U.versionfile and U.subversionfile */
#undef USER_VERSION_ATLEAST
#define USER_VERSION_ATLEAST(ver, subver) MAIN_VERSION_ATLEAST((&(U)), ver, subver)

	if (!USER_VERSION_ATLEAST(266, 0)) {
		bTheme *btheme;
		
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* rna definition limits fac to 0.01 */
			if (btheme->tui.menu_shadow_fac == 0.0f) {
				btheme->tui.menu_shadow_fac = 0.5f;
				btheme->tui.menu_shadow_width = 12;
			}
		}
	}

	if (!USER_VERSION_ATLEAST(265, 4)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->text.syntaxd,    50, 0, 140, 255);   /* Decorator/Preprocessor Dir.  Blue-purple */
			rgba_char_args_set(btheme->text.syntaxr,    140, 60, 0, 255);   /* Reserved  Orange */
			rgba_char_args_set(btheme->text.syntaxs,    76, 76, 76, 255);   /* Gray (mix between fg/bg) */
		}
	}

	if (!USER_VERSION_ATLEAST(265, 6)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			copy_v4_v4_char(btheme->tv3d.gradients.high_gradient, btheme->tv3d.back);
		}
	}

	if (!USER_VERSION_ATLEAST(265, 9)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_test_set(btheme->tnode.syntaxs, 151, 116, 116, 255);  /* matte nodes */
			rgba_char_args_test_set(btheme->tnode.syntaxd, 116, 151, 151, 255);  /* distort nodes */
		}
	}

	if (!USER_VERSION_ATLEAST(265, 11)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_test_set(btheme->tconsole.console_select, 255, 255, 255, 48);
		}
	}

	if (!USER_VERSION_ATLEAST(266, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_test_set(btheme->tnode.console_output, 223, 202, 53, 255);  /* interface nodes */
		}
	}

	if (!USER_VERSION_ATLEAST(268, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_test_set(btheme->tima.uv_others, 96, 96, 96, 255);
			rgba_char_args_test_set(btheme->tima.uv_shadow, 112, 112, 112, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(269, 5)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tima.wire_edit, 192, 192, 192, 255);
			rgba_char_args_set(btheme->tima.edge_select, 255, 133, 0, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(269, 6)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			char r, g, b;
			r = btheme->tnode.syntaxn[0];
			g = btheme->tnode.syntaxn[1];
			b = btheme->tnode.syntaxn[2];
			rgba_char_args_test_set(btheme->tnode.nodeclass_output, r, g, b, 255);
			r = btheme->tnode.syntaxb[0];
			g = btheme->tnode.syntaxb[1];
			b = btheme->tnode.syntaxb[2];
			rgba_char_args_test_set(btheme->tnode.nodeclass_filter, r, g, b, 255);
			rgba_char_args_test_set(btheme->tnode.nodeclass_vector, r, g, b, 255);
			rgba_char_args_test_set(btheme->tnode.nodeclass_texture, r, g, b, 255);
			rgba_char_args_test_set(btheme->tnode.nodeclass_shader, r, g, b, 255);
			rgba_char_args_test_set(btheme->tnode.nodeclass_script, r, g, b, 255);
			rgba_char_args_test_set(btheme->tnode.nodeclass_pattern, r, g, b, 255);
			rgba_char_args_test_set(btheme->tnode.nodeclass_layout, r, g, b, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(269, 8)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_test_set(btheme->tinfo.info_selected, 96, 128, 255, 255);
			rgba_char_args_test_set(btheme->tinfo.info_selected_text, 255, 255, 255, 255);
			rgba_char_args_test_set(btheme->tinfo.info_error, 220, 0, 0, 255);
			rgba_char_args_test_set(btheme->tinfo.info_error_text, 0, 0, 0, 255);
			rgba_char_args_test_set(btheme->tinfo.info_warning, 220, 128, 96, 255);
			rgba_char_args_test_set(btheme->tinfo.info_warning_text, 0, 0, 0, 255);
			rgba_char_args_test_set(btheme->tinfo.info_info, 0, 170, 0, 255);
			rgba_char_args_test_set(btheme->tinfo.info_info_text, 0, 0, 0, 255);
			rgba_char_args_test_set(btheme->tinfo.info_debug, 196, 196, 196, 255);
			rgba_char_args_test_set(btheme->tinfo.info_debug_text, 0, 0, 0, 255);
		}
	}
	
	if (!USER_VERSION_ATLEAST(269, 9)) {
		bTheme *btheme;
		
		U.tw_size = U.tw_size * 5.0f;
		
		/* Action Editor (and NLA Editor) - Keyframe Colors */
		/* Graph Editor - larger vertex size defaults */
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* Action Editor ................. */
			/* key types */
			rgba_char_args_set(btheme->tact.keytype_keyframe,           232, 232, 232, 255);
			rgba_char_args_set(btheme->tact.keytype_keyframe_select,    255, 190,  50, 255);
			rgba_char_args_set(btheme->tact.keytype_extreme,            232, 179, 204, 255);
			rgba_char_args_set(btheme->tact.keytype_extreme_select,     242, 128, 128, 255);
			rgba_char_args_set(btheme->tact.keytype_breakdown,          179, 219, 232, 255);
			rgba_char_args_set(btheme->tact.keytype_breakdown_select,    84, 191, 237, 255);
			rgba_char_args_set(btheme->tact.keytype_jitter,             148, 229, 117, 255);
			rgba_char_args_set(btheme->tact.keytype_jitter_select,       97, 192,  66, 255);
			
			/* key border */
			rgba_char_args_set(btheme->tact.keyborder,               0,   0,   0, 255);
			rgba_char_args_set(btheme->tact.keyborder_select,        0,   0,   0, 255);
			
			/* NLA ............................ */
			/* key border */
			rgba_char_args_set(btheme->tnla.keyborder,               0,   0,   0, 255);
			rgba_char_args_set(btheme->tnla.keyborder_select,        0,   0,   0, 255);
			
			/* Graph Editor ................... */
			btheme->tipo.vertex_size = 6;
			btheme->tipo.handle_vertex_size = 5;
		}
		
		/* grease pencil - new layer color */
		if (U.gpencil_new_layer_col[3] < 0.1f) {
			/* defaults to black, but must at least be visible! */
			U.gpencil_new_layer_col[3] = 0.9f;
		}
	}

	if (!USER_VERSION_ATLEAST(269, 10)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			ThemeSpace *ts;

			for (ts = UI_THEMESPACE_START(btheme); ts != UI_THEMESPACE_END(btheme); ts++) {
				rgba_char_args_set(ts->tab_active, 114, 114, 114, 255);
				rgba_char_args_set(ts->tab_inactive, 83, 83, 83, 255);
				rgba_char_args_set(ts->tab_back, 64, 64, 64, 255);
				rgba_char_args_set(ts->tab_outline, 60, 60, 60, 255);
			}
		}
	}

	if (!USER_VERSION_ATLEAST(271, 0)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tui.wcol_tooltip.text, 255, 255, 255, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(272, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set_fl(btheme->tv3d.paint_curve_handle, 0.5f, 1.0f, 0.5f, 0.5f);
			rgba_char_args_set_fl(btheme->tv3d.paint_curve_pivot, 1.0f, 0.5f, 0.5f, 0.5f);
			rgba_char_args_set_fl(btheme->tima.paint_curve_handle, 0.5f, 1.0f, 0.5f, 0.5f);
			rgba_char_args_set_fl(btheme->tima.paint_curve_pivot, 1.0f, 0.5f, 0.5f, 0.5f);
			rgba_char_args_set(btheme->tnode.syntaxr, 115, 115, 115, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(271, 5)) {
		bTheme *btheme;

		struct uiWidgetColors wcol_pie_menu = {
			{10, 10, 10, 200},
			{25, 25, 25, 230},
			{140, 140, 140, 255},
			{45, 45, 45, 230},

			{160, 160, 160, 255},
			{255, 255, 255, 255},

			1,
			10, -10
		};

		U.pie_menu_radius = 100;
		U.pie_menu_threshold = 12;
		U.pie_animation_timeout = 6;

		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			btheme->tui.wcol_pie_menu = wcol_pie_menu;

			ui_theme_space_init_handles_color(&btheme->tclip);
			ui_theme_space_init_handles_color(&btheme->tima);
			btheme->tima.handle_vertex_size = 5;
			btheme->tclip.handle_vertex_size = 5;
		}
	}

	if (!USER_VERSION_ATLEAST(271, 6)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* check for (alpha == 0) is safe, then color was never set */
			if (btheme->tv3d.loop_normal[3] == 0) {
				rgba_char_args_set(btheme->tv3d.loop_normal, 0xDD, 0x23, 0xDD, 255);
			}
		}
	}

	if (!USER_VERSION_ATLEAST(272, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set_fl(btheme->tui.widget_emboss, 1.0f, 1.0f, 1.0f, 0.02f);
		}
	}
	
	if (!USER_VERSION_ATLEAST(273, 1)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* Grease Pencil vertex settings */
			rgba_char_args_set(btheme->tv3d.gp_vertex, 0, 0, 0, 255);
			rgba_char_args_set(btheme->tv3d.gp_vertex_select, 255, 133, 0, 255);
			btheme->tv3d.gp_vertex_size = 3;
			
			rgba_char_args_set(btheme->tseq.gp_vertex, 0, 0, 0, 255);
			rgba_char_args_set(btheme->tseq.gp_vertex_select, 255, 133, 0, 255);
			btheme->tseq.gp_vertex_size = 3;
			
			rgba_char_args_set(btheme->tima.gp_vertex, 0, 0, 0, 255);
			rgba_char_args_set(btheme->tima.gp_vertex_select, 255, 133, 0, 255);
			btheme->tima.gp_vertex_size = 3;
			
			rgba_char_args_set(btheme->tnode.gp_vertex, 0, 0, 0, 255);
			rgba_char_args_set(btheme->tnode.gp_vertex_select, 255, 133, 0, 255);
			btheme->tnode.gp_vertex_size = 3;
			
			/* Timeline Keyframe Indicators */
			rgba_char_args_set(btheme->ttime.time_keyframe, 0xDD, 0xD7, 0x00, 0xFF);
			rgba_char_args_set(btheme->ttime.time_gp_keyframe, 0xB5, 0xE6, 0x1D, 0xFF);
		}
	}

	if (!USER_VERSION_ATLEAST(273, 5)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			unsigned char *cp = (unsigned char *)btheme->tv3d.clipping_border_3d;
			int c;
			copy_v4_v4_char((char *)cp, btheme->tv3d.back);
			c = cp[0] - 8;
			CLAMP(c, 0, 255);
			cp[0] = c;
			c = cp[1] - 8;
			CLAMP(c, 0, 255);
			cp[1] = c;
			c = cp[2] - 8;
			CLAMP(c, 0, 255);
			cp[2] = c;
			cp[3] = 255;
		}
	}

	if (!USER_VERSION_ATLEAST(274, 5)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			copy_v4_v4_char(btheme->tima.metadatatext, btheme->tima.text_hi);
			copy_v4_v4_char(btheme->tseq.metadatatext, btheme->tseq.text_hi);
		}
	}

	if (!USER_VERSION_ATLEAST(275, 1)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			copy_v4_v4_char(btheme->tclip.metadatatext, btheme->tseq.text_hi);
		}
	}

	if (!USER_VERSION_ATLEAST(275, 2)) {
		U.ndof_deadzone = 0.1;
	}

	if (!USER_VERSION_ATLEAST(275, 4)) {
		U.node_margin = 80;
	}

	if (!USER_VERSION_ATLEAST(276, 1)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set_fl(btheme->tima.preview_back, 0.0f, 0.0f, 0.0f, 0.3f);
		}
	}

	if (!USER_VERSION_ATLEAST(276, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tclip.gp_vertex, 0, 0, 0, 255);
			rgba_char_args_set(btheme->tclip.gp_vertex_select, 255, 133, 0, 255);
			btheme->tclip.gp_vertex_size = 3;
		}
	}

	if (!USER_VERSION_ATLEAST(276, 3)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tseq.text_strip, 162, 151, 0, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(276, 8)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tui.wcol_progress.item, 128, 128, 128, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(276, 10)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* 3dView Keyframe Indicators */
			rgba_char_args_set(btheme->tv3d.time_keyframe, 0xDD, 0xD7, 0x00, 0xFF);
			rgba_char_args_set(btheme->tv3d.time_gp_keyframe, 0xB5, 0xE6, 0x1D, 0xFF);
		}
	}

	if (!USER_VERSION_ATLEAST(277, 0)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (memcmp(btheme->tui.wcol_list_item.item, btheme->tui.wcol_list_item.text_sel, sizeof(char) * 3) == 0) {
				copy_v4_v4_char(btheme->tui.wcol_list_item.item, btheme->tui.wcol_text.item);
				copy_v4_v4_char(btheme->tui.wcol_list_item.text_sel, btheme->tui.wcol_text.text_sel);
			}
		}
	}
	
	if (!USER_VERSION_ATLEAST(277, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			if (btheme->tact.keyframe_scale_fac < 0.1f)
				btheme->tact.keyframe_scale_fac = 1.0f;
		}
	}

	if (!USER_VERSION_ATLEAST(278, 2)) {
		bTheme *btheme;
		for (btheme = U.themes.first; btheme; btheme = btheme->next) {
			rgba_char_args_set(btheme->tv3d.vertex_bevel, 0, 165, 255, 255);
			rgba_char_args_set(btheme->tv3d.edge_bevel, 0, 165, 255, 255);
		}
	}

	if (!USER_VERSION_ATLEAST(278, 3)) {
		for (bTheme *btheme = U.themes.first; btheme; btheme = btheme->next) {
			/* Keyframe Indicators (were using wrong alpha) */
			btheme->tv3d.time_keyframe[3] = btheme->tv3d.time_gp_keyframe[3] = 255;
			btheme->ttime.time_keyframe[3] = btheme->ttime.time_gp_keyframe[3] = 255;
		}
	}

	if (!USER_VERSION_ATLEAST(278, 6)) {
		/* Clear preference flags for re-use. */
		U.flag &= ~(
		    USER_FLAG_DEPRECATED_1 | USER_FLAG_DEPRECATED_2 | USER_FLAG_DEPRECATED_3 |
		    USER_FLAG_DEPRECATED_6 | USER_FLAG_DEPRECATED_7 |
		    USER_FLAG_DEPRECATED_9 | USER_FLAG_DEPRECATED_10);
		U.uiflag &= ~(
		    USER_UIFLAG_DEPRECATED_7);
		U.transopts &= ~(
		    USER_TR_DEPRECATED_2 | USER_TR_DEPRECATED_3 | USER_TR_DEPRECATED_4 |
		    USER_TR_DEPRECATED_6 | USER_TR_DEPRECATED_7);
		U.gameflags &= ~(
		    USER_GL_RENDER_DEPRECATED_0 | USER_GL_RENDER_DEPRECATED_1 |
		    USER_GL_RENDER_DEPRECATED_3 | USER_GL_RENDER_DEPRECATED_4);

		U.uiflag |= USER_LOCK_CURSOR_ADJUST;
	}

	/**
	 * Include next version bump.
	 *
	 * (keep this block even if it becomes empty).
	 */
	{
		
	}

	if (U.pixelsize == 0.0f)
		U.pixelsize = 1.0f;
	
	if (U.image_draw_method == 0)
		U.image_draw_method = IMAGE_DRAW_METHOD_2DTEXTURE;
	
	// keep the following until the new audaspace is default to be built with
#ifdef WITH_SYSTEM_AUDASPACE
	// we default to the first audio device
	U.audiodevice = 0;
#endif

	/* funny name, but it is GE stuff, moves userdef stuff to engine */
// XXX	space_set_commmandline_options();
	/* this timer uses U */
// XXX	reset_autosave();

}
