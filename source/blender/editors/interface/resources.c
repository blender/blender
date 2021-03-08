/*
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
 */

/** \file
 * \ingroup edinterface
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_addon.h"
#include "BKE_appdir.h"
#include "BKE_main.h"
#include "BKE_mesh_runtime.h"

#include "BLO_readfile.h" /* for UserDef version patching. */

#include "BLF_api.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "GPU_framebuffer.h"
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
  UI_icons_init();
}

void ui_resources_free(void)
{
  UI_icons_free();
}

/* ******************************************************** */
/*    THEMES */
/* ******************************************************** */

const uchar *UI_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid)
{
  ThemeSpace *ts = NULL;
  static uchar error[4] = {240, 0, 240, 255};
  static uchar alert[4] = {240, 60, 60, 255};
  static uchar headerdesel[4] = {0, 0, 0, 255};
  static uchar back[4] = {0, 0, 0, 255};
  static uchar setting = 0;
  const uchar *cp = error;

  /* ensure we're not getting a color after running BKE_blender_userdef_free */
  BLI_assert(BLI_findindex(&U.themes, theme_active) != -1);
  BLI_assert(colorid != TH_UNDEFINED);

  if (btheme) {

    /* first check for ui buttons theme */
    if (colorid < TH_THEMEUI) {

      switch (colorid) {

        case TH_REDALERT:
          cp = alert;
          break;
      }
    }
    else {

      switch (spacetype) {
        case SPACE_PROPERTIES:
          ts = &btheme->space_properties;
          break;
        case SPACE_VIEW3D:
          ts = &btheme->space_view3d;
          break;
        case SPACE_GRAPH:
          ts = &btheme->space_graph;
          break;
        case SPACE_FILE:
          ts = &btheme->space_file;
          break;
        case SPACE_NLA:
          ts = &btheme->space_nla;
          break;
        case SPACE_ACTION:
          ts = &btheme->space_action;
          break;
        case SPACE_SEQ:
          ts = &btheme->space_sequencer;
          break;
        case SPACE_IMAGE:
          ts = &btheme->space_image;
          break;
        case SPACE_TEXT:
          ts = &btheme->space_text;
          break;
        case SPACE_OUTLINER:
          ts = &btheme->space_outliner;
          break;
        case SPACE_INFO:
          ts = &btheme->space_info;
          break;
        case SPACE_USERPREF:
          ts = &btheme->space_preferences;
          break;
        case SPACE_CONSOLE:
          ts = &btheme->space_console;
          break;
        case SPACE_NODE:
          ts = &btheme->space_node;
          break;
        case SPACE_CLIP:
          ts = &btheme->space_clip;
          break;
        case SPACE_TOPBAR:
          ts = &btheme->space_topbar;
          break;
        case SPACE_STATUSBAR:
          ts = &btheme->space_statusbar;
          break;
        case SPACE_SPREADSHEET:
          ts = &btheme->space_spreadsheet;
          break;
        default:
          ts = &btheme->space_view3d;
          break;
      }

      switch (colorid) {
        case TH_BACK:
          if (ELEM(theme_regionid, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW)) {
            cp = ts->back;
          }
          else if (theme_regionid == RGN_TYPE_CHANNELS) {
            cp = ts->list;
          }
          else if (ELEM(theme_regionid, RGN_TYPE_HEADER, RGN_TYPE_FOOTER)) {
            cp = ts->header;
          }
          else if (theme_regionid == RGN_TYPE_NAV_BAR) {
            cp = ts->navigation_bar;
          }
          else if (theme_regionid == RGN_TYPE_EXECUTE) {
            cp = ts->execution_buts;
          }
          else {
            cp = ts->button;
          }

          copy_v4_v4_uchar(back, cp);
          if (!ED_region_is_overlap(spacetype, theme_regionid)) {
            back[3] = 255;
          }
          cp = back;
          break;
        case TH_BACK_GRAD:
          cp = ts->back_grad;
          break;

        case TH_BACKGROUND_TYPE:
          cp = &setting;
          setting = ts->background_type;
          break;
        case TH_TEXT:
          if (theme_regionid == RGN_TYPE_WINDOW) {
            cp = ts->text;
          }
          else if (theme_regionid == RGN_TYPE_CHANNELS) {
            cp = ts->list_text;
          }
          else if (ELEM(theme_regionid, RGN_TYPE_HEADER, RGN_TYPE_FOOTER)) {
            cp = ts->header_text;
          }
          else {
            cp = ts->button_text;
          }
          break;
        case TH_TEXT_HI:
          if (theme_regionid == RGN_TYPE_WINDOW) {
            cp = ts->text_hi;
          }
          else if (theme_regionid == RGN_TYPE_CHANNELS) {
            cp = ts->list_text_hi;
          }
          else if (ELEM(theme_regionid, RGN_TYPE_HEADER, RGN_TYPE_FOOTER)) {
            cp = ts->header_text_hi;
          }
          else {
            cp = ts->button_text_hi;
          }
          break;
        case TH_TITLE:
          if (theme_regionid == RGN_TYPE_WINDOW) {
            cp = ts->title;
          }
          else if (theme_regionid == RGN_TYPE_CHANNELS) {
            cp = ts->list_title;
          }
          else if (ELEM(theme_regionid, RGN_TYPE_HEADER, RGN_TYPE_FOOTER)) {
            cp = ts->header_title;
          }
          else {
            cp = ts->button_title;
          }
          break;

        case TH_HEADER:
          cp = ts->header;
          break;
        case TH_HEADERDESEL:
          /* We calculate a dynamic builtin header deselect color, also for pull-downs. */
          cp = ts->header;
          headerdesel[0] = cp[0] > 10 ? cp[0] - 10 : 0;
          headerdesel[1] = cp[1] > 10 ? cp[1] - 10 : 0;
          headerdesel[2] = cp[2] > 10 ? cp[2] - 10 : 0;
          headerdesel[3] = cp[3];
          cp = headerdesel;
          break;
        case TH_HEADER_TEXT:
          cp = ts->header_text;
          break;
        case TH_HEADER_TEXT_HI:
          cp = ts->header_text_hi;
          break;

        case TH_PANEL_HEADER:
          cp = ts->panelcolors.header;
          break;
        case TH_PANEL_BACK:
          cp = ts->panelcolors.back;
          break;
        case TH_PANEL_SUB_BACK:
          cp = ts->panelcolors.sub_back;
          break;

        case TH_BUTBACK:
          cp = ts->button;
          break;
        case TH_BUTBACK_TEXT:
          cp = ts->button_text;
          break;
        case TH_BUTBACK_TEXT_HI:
          cp = ts->button_text_hi;
          break;

        case TH_TAB_ACTIVE:
          cp = ts->tab_active;
          break;
        case TH_TAB_INACTIVE:
          cp = ts->tab_inactive;
          break;
        case TH_TAB_BACK:
          cp = ts->tab_back;
          break;
        case TH_TAB_OUTLINE:
          cp = ts->tab_outline;
          break;

        case TH_SHADE1:
          cp = ts->shade1;
          break;
        case TH_SHADE2:
          cp = ts->shade2;
          break;
        case TH_HILITE:
          cp = ts->hilite;
          break;

        case TH_GRID:
          cp = ts->grid;
          break;
        case TH_TIME_SCRUB_BACKGROUND:
          cp = ts->time_scrub_background;
          break;
        case TH_TIME_MARKER_LINE:
          cp = ts->time_marker_line;
          break;
        case TH_TIME_MARKER_LINE_SELECTED:
          cp = ts->time_marker_line_selected;
          break;
        case TH_VIEW_OVERLAY:
          cp = ts->view_overlay;
          break;
        case TH_WIRE:
          cp = ts->wire;
          break;
        case TH_WIRE_INNER:
          cp = ts->syntaxr;
          break;
        case TH_WIRE_EDIT:
          cp = ts->wire_edit;
          break;
        case TH_LIGHT:
          cp = ts->lamp;
          break;
        case TH_SPEAKER:
          cp = ts->speaker;
          break;
        case TH_CAMERA:
          cp = ts->camera;
          break;
        case TH_EMPTY:
          cp = ts->empty;
          break;
        case TH_SELECT:
          cp = ts->select;
          break;
        case TH_ACTIVE:
          cp = ts->active;
          break;
        case TH_GROUP:
          cp = ts->group;
          break;
        case TH_GROUP_ACTIVE:
          cp = ts->group_active;
          break;
        case TH_TRANSFORM:
          cp = ts->transform;
          break;
        case TH_VERTEX:
          cp = ts->vertex;
          break;
        case TH_VERTEX_SELECT:
          cp = ts->vertex_select;
          break;
        case TH_VERTEX_ACTIVE:
          cp = ts->vertex_active;
          break;
        case TH_VERTEX_BEVEL:
          cp = ts->vertex_bevel;
          break;
        case TH_VERTEX_UNREFERENCED:
          cp = ts->vertex_unreferenced;
          break;
        case TH_VERTEX_SIZE:
          cp = &ts->vertex_size;
          break;
        case TH_OUTLINE_WIDTH:
          cp = &ts->outline_width;
          break;
        case TH_OBCENTER_DIA:
          cp = &ts->obcenter_dia;
          break;
        case TH_EDGE:
          cp = ts->edge;
          break;
        case TH_EDGE_SELECT:
          cp = ts->edge_select;
          break;
        case TH_EDGE_SEAM:
          cp = ts->edge_seam;
          break;
        case TH_EDGE_SHARP:
          cp = ts->edge_sharp;
          break;
        case TH_EDGE_CREASE:
          cp = ts->edge_crease;
          break;
        case TH_EDGE_BEVEL:
          cp = ts->edge_bevel;
          break;
        case TH_EDITMESH_ACTIVE:
          cp = ts->editmesh_active;
          break;
        case TH_EDGE_FACESEL:
          cp = ts->edge_facesel;
          break;
        case TH_FACE:
          cp = ts->face;
          break;
        case TH_FACE_SELECT:
          cp = ts->face_select;
          break;
        case TH_FACE_BACK:
          cp = ts->face_back;
          break;
        case TH_FACE_FRONT:
          cp = ts->face_front;
          break;
        case TH_FACE_DOT:
          cp = ts->face_dot;
          break;
        case TH_FACEDOT_SIZE:
          cp = &ts->facedot_size;
          break;
        case TH_DRAWEXTRA_EDGELEN:
          cp = ts->extra_edge_len;
          break;
        case TH_DRAWEXTRA_EDGEANG:
          cp = ts->extra_edge_angle;
          break;
        case TH_DRAWEXTRA_FACEAREA:
          cp = ts->extra_face_area;
          break;
        case TH_DRAWEXTRA_FACEANG:
          cp = ts->extra_face_angle;
          break;
        case TH_NORMAL:
          cp = ts->normal;
          break;
        case TH_VNORMAL:
          cp = ts->vertex_normal;
          break;
        case TH_LNORMAL:
          cp = ts->loop_normal;
          break;
        case TH_BONE_SOLID:
          cp = ts->bone_solid;
          break;
        case TH_BONE_POSE:
          cp = ts->bone_pose;
          break;
        case TH_BONE_POSE_ACTIVE:
          cp = ts->bone_pose_active;
          break;
        case TH_BONE_LOCKED_WEIGHT:
          cp = ts->bone_locked_weight;
          break;
        case TH_STRIP:
          cp = ts->strip;
          break;
        case TH_STRIP_SELECT:
          cp = ts->strip_select;
          break;
        case TH_KEYTYPE_KEYFRAME:
          cp = ts->keytype_keyframe;
          break;
        case TH_KEYTYPE_KEYFRAME_SELECT:
          cp = ts->keytype_keyframe_select;
          break;
        case TH_KEYTYPE_EXTREME:
          cp = ts->keytype_extreme;
          break;
        case TH_KEYTYPE_EXTREME_SELECT:
          cp = ts->keytype_extreme_select;
          break;
        case TH_KEYTYPE_BREAKDOWN:
          cp = ts->keytype_breakdown;
          break;
        case TH_KEYTYPE_BREAKDOWN_SELECT:
          cp = ts->keytype_breakdown_select;
          break;
        case TH_KEYTYPE_JITTER:
          cp = ts->keytype_jitter;
          break;
        case TH_KEYTYPE_JITTER_SELECT:
          cp = ts->keytype_jitter_select;
          break;
        case TH_KEYTYPE_MOVEHOLD:
          cp = ts->keytype_movehold;
          break;
        case TH_KEYTYPE_MOVEHOLD_SELECT:
          cp = ts->keytype_movehold_select;
          break;
        case TH_KEYBORDER:
          cp = ts->keyborder;
          break;
        case TH_KEYBORDER_SELECT:
          cp = ts->keyborder_select;
          break;
        case TH_CFRAME:
          cp = ts->cframe;
          break;
        case TH_TIME_KEYFRAME:
          cp = ts->time_keyframe;
          break;
        case TH_TIME_GP_KEYFRAME:
          cp = ts->time_gp_keyframe;
          break;
        case TH_NURB_ULINE:
          cp = ts->nurb_uline;
          break;
        case TH_NURB_VLINE:
          cp = ts->nurb_vline;
          break;
        case TH_NURB_SEL_ULINE:
          cp = ts->nurb_sel_uline;
          break;
        case TH_NURB_SEL_VLINE:
          cp = ts->nurb_sel_vline;
          break;
        case TH_ACTIVE_SPLINE:
          cp = ts->act_spline;
          break;
        case TH_ACTIVE_VERT:
          cp = ts->lastsel_point;
          break;
        case TH_HANDLE_FREE:
          cp = ts->handle_free;
          break;
        case TH_HANDLE_AUTO:
          cp = ts->handle_auto;
          break;
        case TH_HANDLE_AUTOCLAMP:
          cp = ts->handle_auto_clamped;
          break;
        case TH_HANDLE_VECT:
          cp = ts->handle_vect;
          break;
        case TH_HANDLE_ALIGN:
          cp = ts->handle_align;
          break;
        case TH_HANDLE_SEL_FREE:
          cp = ts->handle_sel_free;
          break;
        case TH_HANDLE_SEL_AUTO:
          cp = ts->handle_sel_auto;
          break;
        case TH_HANDLE_SEL_AUTOCLAMP:
          cp = ts->handle_sel_auto_clamped;
          break;
        case TH_HANDLE_SEL_VECT:
          cp = ts->handle_sel_vect;
          break;
        case TH_HANDLE_SEL_ALIGN:
          cp = ts->handle_sel_align;
          break;
        case TH_FREESTYLE_EDGE_MARK:
          cp = ts->freestyle_edge_mark;
          break;
        case TH_FREESTYLE_FACE_MARK:
          cp = ts->freestyle_face_mark;
          break;

        case TH_SYNTAX_B:
          cp = ts->syntaxb;
          break;
        case TH_SYNTAX_V:
          cp = ts->syntaxv;
          break;
        case TH_SYNTAX_C:
          cp = ts->syntaxc;
          break;
        case TH_SYNTAX_L:
          cp = ts->syntaxl;
          break;
        case TH_SYNTAX_D:
          cp = ts->syntaxd;
          break;
        case TH_SYNTAX_R:
          cp = ts->syntaxr;
          break;
        case TH_SYNTAX_N:
          cp = ts->syntaxn;
          break;
        case TH_SYNTAX_S:
          cp = ts->syntaxs;
          break;
        case TH_LINENUMBERS:
          cp = ts->line_numbers;
          break;

        case TH_NODE:
          cp = ts->syntaxl;
          break;
        case TH_NODE_INPUT:
          cp = ts->syntaxn;
          break;
        case TH_NODE_OUTPUT:
          cp = ts->nodeclass_output;
          break;
        case TH_NODE_COLOR:
          cp = ts->syntaxb;
          break;
        case TH_NODE_FILTER:
          cp = ts->nodeclass_filter;
          break;
        case TH_NODE_VECTOR:
          cp = ts->nodeclass_vector;
          break;
        case TH_NODE_TEXTURE:
          cp = ts->nodeclass_texture;
          break;
        case TH_NODE_PATTERN:
          cp = ts->nodeclass_pattern;
          break;
        case TH_NODE_SCRIPT:
          cp = ts->nodeclass_script;
          break;
        case TH_NODE_LAYOUT:
          cp = ts->nodeclass_layout;
          break;
        case TH_NODE_GEOMETRY:
          cp = ts->nodeclass_geometry;
          break;
        case TH_NODE_ATTRIBUTE:
          cp = ts->nodeclass_attribute;
          break;
        case TH_NODE_SHADER:
          cp = ts->nodeclass_shader;
          break;
        case TH_NODE_CONVERTOR:
          cp = ts->syntaxv;
          break;
        case TH_NODE_GROUP:
          cp = ts->syntaxc;
          break;
        case TH_NODE_INTERFACE:
          cp = ts->console_output;
          break;
        case TH_NODE_FRAME:
          cp = ts->movie;
          break;
        case TH_NODE_MATTE:
          cp = ts->syntaxs;
          break;
        case TH_NODE_DISTORT:
          cp = ts->syntaxd;
          break;
        case TH_NODE_CURVING:
          cp = &ts->noodle_curving;
          break;
        case TH_NODE_GRID_LEVELS:
          cp = &ts->grid_levels;
          break;

        case TH_SEQ_MOVIE:
          cp = ts->movie;
          break;
        case TH_SEQ_MOVIECLIP:
          cp = ts->movieclip;
          break;
        case TH_SEQ_MASK:
          cp = ts->mask;
          break;
        case TH_SEQ_IMAGE:
          cp = ts->image;
          break;
        case TH_SEQ_SCENE:
          cp = ts->scene;
          break;
        case TH_SEQ_AUDIO:
          cp = ts->audio;
          break;
        case TH_SEQ_EFFECT:
          cp = ts->effect;
          break;
        case TH_SEQ_META:
          cp = ts->meta;
          break;
        case TH_SEQ_TEXT:
          cp = ts->text_strip;
          break;
        case TH_SEQ_PREVIEW:
          cp = ts->preview_back;
          break;
        case TH_SEQ_COLOR:
          cp = ts->color_strip;
          break;
        case TH_SEQ_ACTIVE:
          cp = ts->active_strip;
          break;
        case TH_SEQ_SELECTED:
          cp = ts->selected_strip;
          break;

        case TH_CONSOLE_OUTPUT:
          cp = ts->console_output;
          break;
        case TH_CONSOLE_INPUT:
          cp = ts->console_input;
          break;
        case TH_CONSOLE_INFO:
          cp = ts->console_info;
          break;
        case TH_CONSOLE_ERROR:
          cp = ts->console_error;
          break;
        case TH_CONSOLE_CURSOR:
          cp = ts->console_cursor;
          break;
        case TH_CONSOLE_SELECT:
          cp = ts->console_select;
          break;

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
        case TH_DOPESHEET_IPOLINE:
          cp = ts->ds_ipoline;
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

        case TH_UV_SHADOW:
          cp = ts->uv_shadow;
          break;

        case TH_MARKER_OUTLINE:
          cp = ts->marker_outline;
          break;
        case TH_MARKER:
          cp = ts->marker;
          break;
        case TH_ACT_MARKER:
          cp = ts->act_marker;
          break;
        case TH_SEL_MARKER:
          cp = ts->sel_marker;
          break;
        case TH_BUNDLE_SOLID:
          cp = ts->bundle_solid;
          break;
        case TH_DIS_MARKER:
          cp = ts->dis_marker;
          break;
        case TH_PATH_BEFORE:
          cp = ts->path_before;
          break;
        case TH_PATH_AFTER:
          cp = ts->path_after;
          break;
        case TH_PATH_KEYFRAME_BEFORE:
          cp = ts->path_keyframe_before;
          break;
        case TH_PATH_KEYFRAME_AFTER:
          cp = ts->path_keyframe_after;
          break;
        case TH_CAMERA_PATH:
          cp = ts->camera_path;
          break;
        case TH_LOCK_MARKER:
          cp = ts->lock_marker;
          break;

        case TH_MATCH:
          cp = ts->match;
          break;

        case TH_SELECT_HIGHLIGHT:
          cp = ts->selected_highlight;
          break;

        case TH_SELECT_ACTIVE:
          cp = ts->active;
          break;

        case TH_SELECTED_OBJECT:
          cp = ts->selected_object;
          break;

        case TH_ACTIVE_OBJECT:
          cp = ts->active_object;
          break;

        case TH_EDITED_OBJECT:
          cp = ts->edited_object;
          break;

        case TH_ROW_ALTERNATE:
          cp = ts->row_alternate;
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
        case TH_ANIM_PREVIEW_RANGE:
          cp = ts->anim_preview_range;
          break;

        case TH_NLA_TWEAK:
          cp = ts->nla_tweaking;
          break;
        case TH_NLA_TWEAK_DUPLI:
          cp = ts->nla_tweakdupli;
          break;

        case TH_NLA_TRACK:
          cp = ts->nla_track;
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
          cp = btheme->tui.widget_emboss;
          break;

        case TH_EDITOR_OUTLINE:
          cp = btheme->tui.editor_outline;
          break;
        case TH_WIDGET_TEXT_CURSOR:
          cp = btheme->tui.widget_text_cursor;
          break;

        case TH_TRANSPARENT_CHECKER_PRIMARY:
          cp = btheme->tui.transparent_checker_primary;
          break;
        case TH_TRANSPARENT_CHECKER_SECONDARY:
          cp = btheme->tui.transparent_checker_secondary;
          break;
        case TH_TRANSPARENT_CHECKER_SIZE:
          cp = &btheme->tui.transparent_checker_size;
          break;

        case TH_AXIS_X:
          cp = btheme->tui.xaxis;
          break;
        case TH_AXIS_Y:
          cp = btheme->tui.yaxis;
          break;
        case TH_AXIS_Z:
          cp = btheme->tui.zaxis;
          break;

        case TH_GIZMO_HI:
          cp = btheme->tui.gizmo_hi;
          break;
        case TH_GIZMO_PRIMARY:
          cp = btheme->tui.gizmo_primary;
          break;
        case TH_GIZMO_SECONDARY:
          cp = btheme->tui.gizmo_secondary;
          break;
        case TH_GIZMO_VIEW_ALIGN:
          cp = btheme->tui.gizmo_view_align;
          break;
        case TH_GIZMO_A:
          cp = btheme->tui.gizmo_a;
          break;
        case TH_GIZMO_B:
          cp = btheme->tui.gizmo_b;
          break;

        case TH_ICON_SCENE:
          cp = btheme->tui.icon_scene;
          break;
        case TH_ICON_COLLECTION:
          cp = btheme->tui.icon_collection;
          break;
        case TH_ICON_OBJECT:
          cp = btheme->tui.icon_object;
          break;
        case TH_ICON_OBJECT_DATA:
          cp = btheme->tui.icon_object_data;
          break;
        case TH_ICON_MODIFIER:
          cp = btheme->tui.icon_modifier;
          break;
        case TH_ICON_SHADING:
          cp = btheme->tui.icon_shading;
          break;
        case TH_ICON_FOLDER:
          cp = btheme->tui.icon_folder;
          break;
        case TH_ICON_FUND: {
          /* Development fund icon color is not part of theme. */
          static const uchar red[4] = {204, 48, 72, 255};
          cp = red;
          break;
        }

        case TH_SCROLL_TEXT:
          cp = btheme->tui.wcol_scroll.text;
          break;

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
        case TH_INFO_PROPERTY:
          cp = ts->info_property;
          break;
        case TH_INFO_PROPERTY_TEXT:
          cp = ts->info_property_text;
          break;
        case TH_INFO_OPERATOR:
          cp = ts->info_operator;
          break;
        case TH_INFO_OPERATOR_TEXT:
          cp = ts->info_operator_text;
          break;
        case TH_V3D_CLIPPING_BORDER:
          cp = ts->clipping_border_3d;
          break;
      }
    }
  }

  return (const uchar *)cp;
}

/**
 * Initialize default theme.
 *
 * \note When you add new colors, created & saved themes need initialized
 * use function below, #init_userdef_do_versions.
 */
void UI_theme_init_default(void)
{
  /* we search for the theme with name Default */
  bTheme *btheme = BLI_findstring(&U.themes, "Default", offsetof(bTheme, name));
  if (btheme == NULL) {
    btheme = MEM_callocN(sizeof(bTheme), __func__);
    BLI_addtail(&U.themes, btheme);
  }

  UI_SetTheme(0, 0); /* make sure the global used in this file is set */

  const int active_theme_area = btheme->active_theme_area;
  memcpy(btheme, &U_theme_default, sizeof(*btheme));
  btheme->active_theme_area = active_theme_area;
}

void UI_style_init_default(void)
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
    theme_spacetype = SPACE_PROPERTIES;
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

void UI_GetThemeColorShadeAlpha4ubv(int colorid, int coloffset, int alphaoffset, uchar col[4])
{
  int r, g, b, a;
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  r = coloffset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = coloffset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = coloffset + (int)cp[2];
  CLAMP(b, 0, 255);
  a = alphaoffset + (int)cp[3];
  CLAMP(a, 0, 255);

  col[0] = r;
  col[1] = g;
  col[2] = b;
  col[3] = a;
}

void UI_GetThemeColorBlend3ubv(int colorid1, int colorid2, float fac, uchar col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);
  col[0] = floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
  col[1] = floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
  col[2] = floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
}

void UI_GetThemeColorBlend3f(int colorid1, int colorid2, float fac, float r_col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);
  r_col[0] = ((1.0f - fac) * cp1[0] + fac * cp2[0]) / 255.0f;
  r_col[1] = ((1.0f - fac) * cp1[1] + fac * cp2[1]) / 255.0f;
  r_col[2] = ((1.0f - fac) * cp1[2] + fac * cp2[2]) / 255.0f;
}

void UI_GetThemeColorBlend4f(int colorid1, int colorid2, float fac, float r_col[4])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);
  r_col[0] = ((1.0f - fac) * cp1[0] + fac * cp2[0]) / 255.0f;
  r_col[1] = ((1.0f - fac) * cp1[1] + fac * cp2[1]) / 255.0f;
  r_col[2] = ((1.0f - fac) * cp1[2] + fac * cp2[2]) / 255.0f;
  r_col[3] = ((1.0f - fac) * cp1[3] + fac * cp2[3]) / 255.0f;
}

void UI_FontThemeColor(int fontid, int colorid)
{
  uchar color[4];
  UI_GetThemeColor4ubv(colorid, color);
  BLF_color4ubv(fontid, color);
}

/* get individual values, not scaled */
float UI_GetThemeValuef(int colorid)
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  return ((float)cp[0]);
}

/* get individual values, not scaled */
int UI_GetThemeValue(int colorid)
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  return ((int)cp[0]);
}

/* versions of the function above, which take a space-type */
float UI_GetThemeValueTypef(int colorid, int spacetype)
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
  return ((float)cp[0]);
}

int UI_GetThemeValueType(int colorid, int spacetype)
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
  return ((int)cp[0]);
}

/* get the color, range 0.0-1.0 */
void UI_GetThemeColor3fv(int colorid, float col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  col[0] = ((float)cp[0]) / 255.0f;
  col[1] = ((float)cp[1]) / 255.0f;
  col[2] = ((float)cp[2]) / 255.0f;
}

void UI_GetThemeColor4fv(int colorid, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  col[0] = ((float)cp[0]) / 255.0f;
  col[1] = ((float)cp[1]) / 255.0f;
  col[2] = ((float)cp[2]) / 255.0f;
  col[3] = ((float)cp[3]) / 255.0f;
}

void UI_GetThemeColorType4fv(int colorid, int spacetype, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
  col[0] = ((float)cp[0]) / 255.0f;
  col[1] = ((float)cp[1]) / 255.0f;
  col[2] = ((float)cp[2]) / 255.0f;
  col[3] = ((float)cp[3]) / 255.0f;
}

/* get the color, range 0.0-1.0, complete with shading offset */
void UI_GetThemeColorShade3fv(int colorid, int offset, float col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  int r, g, b;

  r = offset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = offset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = offset + (int)cp[2];
  CLAMP(b, 0, 255);

  col[0] = ((float)r) / 255.0f;
  col[1] = ((float)g) / 255.0f;
  col[2] = ((float)b) / 255.0f;
}

void UI_GetThemeColorShade3ubv(int colorid, int offset, uchar col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  int r, g, b;

  r = offset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = offset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = offset + (int)cp[2];
  CLAMP(b, 0, 255);

  col[0] = r;
  col[1] = g;
  col[2] = b;
}

void UI_GetThemeColorBlendShade3ubv(
    int colorid1, int colorid2, float fac, int offset, uchar col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);

  float blend[3];
  blend[0] = (offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0])) / 255.0f;
  blend[1] = (offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1])) / 255.0f;
  blend[2] = (offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2])) / 255.0f;

  unit_float_to_uchar_clamp_v3(col, blend);
}

void UI_GetThemeColorShade4ubv(int colorid, int offset, uchar col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  int r, g, b;

  r = offset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = offset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = offset + (int)cp[2];
  CLAMP(b, 0, 255);

  col[0] = r;
  col[1] = g;
  col[2] = b;
  col[3] = cp[3];
}

void UI_GetThemeColorShadeAlpha4fv(int colorid, int coloffset, int alphaoffset, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  int r, g, b, a;

  r = coloffset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = coloffset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = coloffset + (int)cp[2];
  CLAMP(b, 0, 255);
  a = alphaoffset + (int)cp[3];
  CLAMP(a, 0, 255);

  col[0] = ((float)r) / 255.0f;
  col[1] = ((float)g) / 255.0f;
  col[2] = ((float)b) / 255.0f;
  col[3] = ((float)a) / 255.0f;
}

void UI_GetThemeColorBlendShade3fv(int colorid1, int colorid2, float fac, int offset, float col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);
  int r, g, b;

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
  const uchar *cp1 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);
  int r, g, b, a;

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
void UI_GetThemeColor3ubv(int colorid, uchar col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
}

/* get the color, range 0.0-1.0, complete with shading offset */
void UI_GetThemeColorShade4fv(int colorid, int offset, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  int r, g, b, a;

  r = offset + (int)cp[0];
  CLAMP(r, 0, 255);
  g = offset + (int)cp[1];
  CLAMP(g, 0, 255);
  b = offset + (int)cp[2];
  CLAMP(b, 0, 255);

  a = (int)cp[3]; /* no shading offset... */
  CLAMP(a, 0, 255);

  col[0] = ((float)r) / 255.0f;
  col[1] = ((float)g) / 255.0f;
  col[2] = ((float)b) / 255.0f;
  col[3] = ((float)a) / 255.0f;
}

/* get the color, in char pointer */
void UI_GetThemeColor4ubv(int colorid, uchar col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
  col[3] = cp[3];
}

void UI_GetThemeColorType3fv(int colorid, int spacetype, float col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
  col[0] = ((float)cp[0]) / 255.0f;
  col[1] = ((float)cp[1]) / 255.0f;
  col[2] = ((float)cp[2]) / 255.0f;
}

void UI_GetThemeColorType3ubv(int colorid, int spacetype, uchar col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
}

void UI_GetThemeColorType4ubv(int colorid, int spacetype, uchar col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(theme_active, spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
  col[3] = cp[3];
}

bool UI_GetIconThemeColor4ubv(int colorid, uchar col[4])
{
  if (colorid == 0) {
    return false;
  }
  if (colorid == TH_ICON_FUND) {
    /* Always color development fund icon. */
  }
  else if (!((theme_spacetype == SPACE_OUTLINER && theme_regionid == RGN_TYPE_WINDOW) ||
             (theme_spacetype == SPACE_PROPERTIES && theme_regionid == RGN_TYPE_NAV_BAR) ||
             (theme_spacetype == SPACE_FILE && theme_regionid == RGN_TYPE_WINDOW))) {
    /* Only colored icons in specific places, overall UI is intended
     * to stay monochrome and out of the way except a few places where it
     * is important to communicate different data types. */
    return false;
  }

  const uchar *cp = UI_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
  col[3] = cp[3];

  return true;
}

void UI_GetColorPtrShade3ubv(const uchar cp[3], uchar col[3], int offset)
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
    const uchar cp1[3], const uchar cp2[3], uchar col[3], float fac, int offset)
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
  GPU_clear_color(col[0], col[1], col[2], 1.0f);
}

int UI_ThemeMenuShadowWidth(void)
{
  bTheme *btheme = UI_GetTheme();
  return (int)(btheme->tui.menu_shadow_width * UI_DPI_FAC);
}

void UI_make_axis_color(const uchar src_col[3], uchar dst_col[3], const char axis)
{
  uchar col[3];

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
