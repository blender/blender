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

#include "BKE_addon.h"
#include "BKE_appdir.h"
#include "BKE_colorband.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh_runtime.h"

#include "BIF_gl.h"

#include "BLF_api.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "interface_intern.h"
#include "GPU_framebuffer.h"


extern const bTheme U_theme_default;

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
	UI_icons_init();
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
	static char back[4] = {0, 0, 0, 255};
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
				case SPACE_NODE:
					ts = &btheme->tnode;
					break;
				case SPACE_CLIP:
					ts = &btheme->tclip;
					break;
				case SPACE_TOPBAR:
					ts = &btheme->ttopbar;
					break;
				case SPACE_STATUSBAR:
					ts = &btheme->tstatusbar;
					break;
				default:
					ts = &btheme->tv3d;
					break;
			}

			switch (colorid) {
				case TH_BACK:
					if (ELEM(theme_regionid, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW))
						cp = ts->back;
					else if (theme_regionid == RGN_TYPE_CHANNELS)
						cp = ts->list;
					else if (theme_regionid == RGN_TYPE_HEADER)
						cp = ts->header;
					else
						cp = ts->button;

					copy_v4_v4_char(back, cp);
					if (!ED_region_is_overlap(spacetype, theme_regionid)) {
						back[3] = 255;
					}
					cp = back;
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
					headerdesel[3] = cp[3];
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
				case TH_PANEL_SUB_BACK:
					cp = ts->panelcolors.sub_back; break;

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

				case TH_EDITOR_OUTLINE:
					cp = btheme->tui.editor_outline;
					break;
				case TH_AXIS_X:
					cp = btheme->tui.xaxis; break;
				case TH_AXIS_Y:
					cp = btheme->tui.yaxis; break;
				case TH_AXIS_Z:
					cp = btheme->tui.zaxis; break;

				case TH_GIZMO_HI:
					cp = btheme->tui.gizmo_hi; break;
				case TH_GIZMO_PRIMARY:
					cp = btheme->tui.gizmo_primary; break;
				case TH_GIZMO_SECONDARY:
					cp = btheme->tui.gizmo_secondary; break;
				case TH_GIZMO_A:
					cp = btheme->tui.gizmo_a; break;
				case TH_GIZMO_B:
					cp = btheme->tui.gizmo_b; break;

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

/**
 * initialize default theme
 * \note: when you add new colors, created & saved themes need initialized
 * use function below, init_userdef_do_versions()
 */
void ui_theme_init_default(void)
{

	/* we search for the theme with name Default */
	bTheme *btheme = BLI_findstring(&U.themes, "Default", offsetof(bTheme, name));
	if (btheme == NULL) {
		btheme = MEM_callocN(sizeof(bTheme), __func__);
		BLI_addtail(&U.themes, btheme);
	}

	UI_SetTheme(0, 0);  /* make sure the global used in this file is set */

	const int active_theme_area = btheme->active_theme_area;
	memcpy(btheme, &U_theme_default, sizeof(*btheme));
	btheme->active_theme_area = active_theme_area;
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
	unsigned char col[4];
	UI_GetThemeColorShade4ubv(colorid, offset, col);
	glColor4ubv(col);
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

void UI_GetThemeColorShadeAlpha4ubv(int colorid, int coloffset, int alphaoffset, unsigned char col[4])
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

	col[0] = r;
	col[1] = g;
	col[2] = b;
	col[3] = a;
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

void UI_GetThemeColorBlend3f(int colorid1, int colorid2, float fac, float r_col[3])
{
	const unsigned char *cp1, *cp2;

	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);
	r_col[0] = ((1.0f - fac) * cp1[0] + fac * cp2[0]) / 255.0f;
	r_col[1] = ((1.0f - fac) * cp1[1] + fac * cp2[1]) / 255.0f;
	r_col[2] = ((1.0f - fac) * cp1[2] + fac * cp2[2]) / 255.0f;
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

void UI_FontThemeColor(int fontid, int colorid)
{
	unsigned char color[4];
	UI_GetThemeColor4ubv(colorid, color);
	BLF_color4ubv(fontid, color);
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

void UI_GetThemeColorBlendShade3ubv(int colorid1, int colorid2, float fac, int offset, unsigned char col[3])
{
	const unsigned char *cp1, *cp2;

	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);

	float blend[3];
	blend[0] = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	blend[1] = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	blend[2] = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);

	unit_float_to_uchar_clamp_v3(col, blend);
}

void UI_GetThemeColorShade4ubv(int colorid, int offset, unsigned char col[4])
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
	col[3] = cp[3];
}

void UI_GetThemeColorShadeAlpha4fv(int colorid, int coloffset, int alphaoffset, float col[4])
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
	CLAMP(b, 0, 255);

	col[0] = ((float)r) / 255.0f;
	col[1] = ((float)g) / 255.0f;
	col[2] = ((float)b) / 255.0f;
	col[3] = ((float)a) / 255.0f;
}

void UI_GetThemeColorBlendShade3fv(int colorid1, int colorid2, float fac, int offset, float col[3])
{
	int r, g, b;
	const unsigned char *cp1, *cp2;

	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);

	r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	CLAMP(r, 0, 255);
	g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	CLAMP(g, 0, 255);
	b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
	CLAMP(b, 0, 255);

	col[0] = ((float)r) / 255.0f;
	col[1] = ((float)g) / 255.0f;
	col[2] = ((float)b) / 255.0f;
}

void UI_GetThemeColorBlendShade4fv(int colorid1, int colorid2, float fac, int offset, float col[4])
{
	int r, g, b, a;
	const unsigned char *cp1, *cp2;

	cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	CLAMP(fac, 0.0f, 1.0f);

	r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
	CLAMP(r, 0, 255);
	g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
	CLAMP(g, 0, 255);
	b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
	CLAMP(b, 0, 255);
	a = offset + floorf((1.0f - fac) * cp1[3] + fac * cp2[3]);
	CLAMP(a, 0, 255);

	col[0] = ((float)r) / 255.0f;
	col[1] = ((float)g) / 255.0f;
	col[2] = ((float)b) / 255.0f;
	col[3] = ((float)a) / 255.0f;
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
	GPU_clear_color(col[0], col[1], col[2], 0.0f);
}

void UI_ThemeClearColorAlpha(int colorid, float alpha)
{
	float col[3];
	UI_GetThemeColor3fv(colorid, col);
	GPU_clear_color(col[0], col[1], col[2], alpha);
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
void init_userdef_do_versions(Main *bmain)
{
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
	if (U.gizmo_size == 0) {
		U.gizmo_size = 75;
		U.gizmo_flag |= USER_GIZMO_DRAW;
	}
	if (U.pad_rot_angle == 0.0f)
		U.pad_rot_angle = 15.0f;

	/* graph editor - unselected F-Curve visibility */
	if (U.fcu_inactive_alpha == 0) {
		U.fcu_inactive_alpha = 0.25f;
	}

	/* signal for evaluated mesh to use colorband */
	/* run in case this was on and is now off in the user prefs [#28096] */
	BKE_mesh_runtime_color_band_store((U.flag & USER_CUSTOM_RANGE) ? (&U.coba_weight) : NULL, UI_GetTheme()->tv3d.vertex_unreferenced);

	if (!USER_VERSION_ATLEAST(192, 0)) {
		strcpy(U.sounddir, "/");
	}

	/* patch to set Dupli Armature */
	if (!USER_VERSION_ATLEAST(220, 0)) {
		U.dupflag |= USER_DUP_ARM;
	}

	/* added seam, normal color, undo */
	if (!USER_VERSION_ATLEAST(235, 0)) {
		U.uiflag |= USER_GLOBALUNDO;
		if (U.undosteps == 0) U.undosteps = 32;
	}
	if (!USER_VERSION_ATLEAST(236, 0)) {
		/* illegal combo... */
		if (U.flag & USER_LMOUSESELECT)
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (!USER_VERSION_ATLEAST(240, 0)) {
		U.uiflag |= USER_PLAINMENUS;
		if (U.obcenter_dia == 0) U.obcenter_dia = 6;
	}
	if (!USER_VERSION_ATLEAST(242, 0)) {
		/* set defaults for 3D View rotating axis indicator */
		/* since size can't be set to 0, this indicates it's not saved in startup.blend */
		if (U.rvisize == 0) {
			U.rvisize = 15;
			U.rvibright = 8;
			U.uiflag |= USER_SHOW_GIZMO_AXIS;
		}

	}
	if (!USER_VERSION_ATLEAST(244, 0)) {
		/* set default number of recently-used files (if not set) */
		if (U.recent_files == 0) U.recent_files = 10;
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		if (U.coba_weight.tot == 0)
			BKE_colorband_init(&U.coba_weight, true);
	}
	if (!USER_VERSION_ATLEAST(245, 3)) {
		U.flag |= USER_ADD_VIEWALIGNED | USER_ADD_EDITMODE;
	}
	if (!USER_VERSION_ATLEAST(250, 0)) {
		/* adjust grease-pencil distances */
		U.gp_manhattendist = 1;
		U.gp_euclideandist = 2;

		/* adjust default interpolation for new IPO-curves */
		U.ipo_new = BEZT_IPO_BEZ;
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

	if (!USER_VERSION_ATLEAST(252, 3)) {
		if (U.flag & USER_LMOUSESELECT)
			U.flag &= ~USER_TWOBUTTONMOUSE;
	}
	if (!USER_VERSION_ATLEAST(252, 4)) {
		/* default new handle type is auto handles */
		U.keyhandles_new = HD_AUTO;
	}

	if (!USER_VERSION_ATLEAST(257, 0)) {
		/* clear "AUTOKEY_FLAG_ONLYKEYINGSET" flag from userprefs,
		 * so that it doesn't linger around from old configs like a ghost */
		U.autokey_flag &= ~AUTOKEY_FLAG_ONLYKEYINGSET;
	}

	if (!USER_VERSION_ATLEAST(260, 3)) {
		/* if new keyframes handle default is stuff "auto", make it "auto-clamped" instead
		 * was changed in 260 as part of GSoC11, but version patch was wrong
		 */
		if (U.keyhandles_new == HD_AUTO)
			U.keyhandles_new = HD_AUTO_ANIM;

		/* enable (Cycles) addon by default */
		BKE_addon_ensure(&U.addons, "cycles");
	}

	if (!USER_VERSION_ATLEAST(261, 4)) {
		U.use_16bit_textures = true;
	}

	if (!USER_VERSION_ATLEAST(267, 0)) {

		/* GL Texture Garbage Collection */
		if (U.textimeout == 0) {
			U.texcollectrate = 60;
			U.textimeout = 120;
		}
		if (U.memcachelimit <= 0) {
			U.memcachelimit = 32;
		}
		if (U.dbl_click_time == 0) {
			U.dbl_click_time = 350;
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

	/* NOTE!! from now on use U.versionfile and U.subversionfile */
#undef USER_VERSION_ATLEAST
#define USER_VERSION_ATLEAST(ver, subver) MAIN_VERSION_ATLEAST((&(U)), ver, subver)

	if (!USER_VERSION_ATLEAST(271, 5)) {
		U.pie_menu_radius = 100;
		U.pie_menu_threshold = 12;
		U.pie_animation_timeout = 6;
	}

	if (!USER_VERSION_ATLEAST(275, 2)) {
		U.ndof_deadzone = 0.1;
	}

	if (!USER_VERSION_ATLEAST(275, 4)) {
		U.node_margin = 80;
	}

	if (!USER_VERSION_ATLEAST(278, 6)) {
		/* Clear preference flags for re-use. */
		U.flag &= ~(
		    USER_FLAG_NUMINPUT_ADVANCED | USER_FLAG_DEPRECATED_2 | USER_FLAG_DEPRECATED_3 |
		    USER_FLAG_DEPRECATED_6 | USER_FLAG_DEPRECATED_7 |
		    USER_FLAG_DEPRECATED_9 | USER_DEVELOPER_UI);
		U.uiflag &= ~(
		    USER_UIFLAG_DEPRECATED_7);
		U.transopts &= ~(
		    USER_TR_DEPRECATED_2 | USER_TR_DEPRECATED_3 | USER_TR_DEPRECATED_4 |
		    USER_TR_DEPRECATED_6 | USER_TR_DEPRECATED_7);

		U.uiflag |= USER_LOCK_CURSOR_ADJUST;
	}


	if (!USER_VERSION_ATLEAST(280, 20)) {
		U.gpu_viewport_quality = 0.6f;

		/* Reset theme, old themes will not be compatible with minor version updates from now on. */
		for (bTheme *btheme = U.themes.first; btheme; btheme = btheme->next) {
			memcpy(btheme, &U_theme_default, sizeof(*btheme));
		}
		
		/* Annotations - new layer color
		 * Replace anything that used to be set if it looks like was left
		 * on the old default (i.e. black), which most users used
		 */
		if ((U.gpencil_new_layer_col[3] < 0.1f) || (U.gpencil_new_layer_col[0] < 0.1f)) {
			/* - New color matches the annotation pencil icon
			 * - Non-full alpha looks better!
			 */
			ARRAY_SET_ITEMS(U.gpencil_new_layer_col, 0.38f, 0.61f, 0.78f, 0.9f);
		}
	}

	/**
	 * Include next version bump.
	 */
	{
		/* (keep this block even if it becomes empty). */
	}

	if (U.pixelsize == 0.0f)
		U.pixelsize = 1.0f;

	if (U.image_draw_method == 0)
		U.image_draw_method = IMAGE_DRAW_METHOD_2DTEXTURE;

	// we default to the first audio device
	U.audiodevice = 0;

	/* Not versioning, just avoid errors. */
#ifndef WITH_CYCLES
	BKE_addon_remove_safe(&U.addons, "cycles");
#endif

	/* funny name, but it is GE stuff, moves userdef stuff to engine */
// XXX	space_set_commmandline_options();
	/* this timer uses U */
// XXX	reset_autosave();

}
