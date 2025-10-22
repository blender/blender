/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_addon.h"
#include "BKE_appdir.hh"

#include "BLO_userdef_default.h"

#include "BLF_api.hh"

#include "ED_screen.hh"

#include "UI_interface_icons.hh"

#include "GPU_framebuffer.hh"
#include "interface_intern.hh"

/* be sure to keep 'bThemeState' in sync */
static bThemeState g_theme_state = {
    nullptr,
    SPACE_VIEW3D,
    RGN_TYPE_WINDOW,
};

/* -------------------------------------------------------------------- */
/** \name Init/Exit
 * \{ */

void ui_resources_init()
{
  UI_icons_init();
}

void ui_resources_free()
{
  UI_icons_free();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Themes
 * \{ */

const uchar *UI_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid)
{
  ThemeSpace *ts = nullptr;
  static uchar error[4] = {240, 0, 240, 255};
  static uchar back[4] = {0, 0, 0, 255};
  static uchar none[4] = {0, 0, 0, 0};
  static uchar white[4] = {255, 255, 255, 255};
  static uchar black[4] = {0, 0, 0, 255};
  static uchar setting = 0;
  const uchar *cp = error;

  /* ensure we're not getting a color after running BKE_blender_userdef_free */
  BLI_assert(BLI_findindex(&U.themes, g_theme_state.theme) != -1);
  BLI_assert(colorid != TH_UNDEFINED);

  if (btheme) {

    /* first check for ui buttons theme */
    if (colorid < TH_THEMEUI) {

      switch (colorid) {
        case TH_NONE:
          cp = none;
          break;
        case TH_BLACK:
          cp = black;
          break;
        case TH_WHITE:
          cp = white;
          break;
        case TH_REDALERT:
        case TH_ERROR:
          cp = btheme->tui.wcol_state.error;
          break;
        case TH_WARNING:
          cp = btheme->tui.wcol_state.warning;
          break;
        case TH_INFO:
          cp = btheme->tui.wcol_state.info;
          break;
        case TH_SUCCESS:
          cp = btheme->tui.wcol_state.success;
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
          if (ELEM(g_theme_state.regionid, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW)) {
            cp = ts->back;
          }
          else if (g_theme_state.regionid == RGN_TYPE_CHANNELS) {
            cp = btheme->regions.channels.back;
          }
          else if (ELEM(g_theme_state.regionid, RGN_TYPE_HEADER, RGN_TYPE_FOOTER)) {
            cp = ts->header;
          }
          else if (g_theme_state.regionid == RGN_TYPE_NAV_BAR) {
            cp = btheme->regions.sidebars.tab_back;
          }
          else if (g_theme_state.regionid == RGN_TYPE_ASSET_SHELF) {
            cp = btheme->regions.asset_shelf.back;
          }
          else if (g_theme_state.regionid == RGN_TYPE_ASSET_SHELF_HEADER) {
            cp = btheme->regions.asset_shelf.header_back;
          }
          else {
            cp = btheme->regions.sidebars.back;
          }

          copy_v4_v4_uchar(back, cp);
          if (!ED_region_is_overlap(spacetype, g_theme_state.regionid)) {
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
          if (ELEM(g_theme_state.regionid, RGN_TYPE_UI, RGN_TYPE_TOOLS) ||
              ELEM(g_theme_state.spacetype, SPACE_PROPERTIES, SPACE_USERPREF))
          {
            cp = btheme->tui.panel_text;
          }
          else if (g_theme_state.regionid == RGN_TYPE_CHANNELS) {
            cp = btheme->regions.channels.text;
          }
          else if (ELEM(g_theme_state.regionid,
                        RGN_TYPE_HEADER,
                        RGN_TYPE_FOOTER,
                        RGN_TYPE_ASSET_SHELF_HEADER))
          {
            cp = ts->header_text;
          }
          else {
            cp = ts->text;
          }
          break;
        case TH_TEXT_HI:
          if (g_theme_state.regionid == RGN_TYPE_CHANNELS) {
            cp = btheme->regions.channels.text_selected;
          }
          else if (ELEM(g_theme_state.regionid,
                        RGN_TYPE_HEADER,
                        RGN_TYPE_FOOTER,
                        RGN_TYPE_ASSET_SHELF_HEADER))
          {
            cp = ts->header_text_hi;
          }
          else {
            cp = ts->text_hi;
          }
          break;
        case TH_TITLE:
          if (ELEM(g_theme_state.regionid, RGN_TYPE_UI, RGN_TYPE_TOOLS, RGN_TYPE_CHANNELS) ||
              ELEM(g_theme_state.spacetype, SPACE_PROPERTIES, SPACE_USERPREF))
          {
            cp = btheme->tui.panel_title;
          }
          else if (ELEM(g_theme_state.regionid,
                        RGN_TYPE_HEADER,
                        RGN_TYPE_FOOTER,
                        RGN_TYPE_ASSET_SHELF_HEADER))
          {
            cp = ts->header_title;
          }
          else {
            cp = ts->title;
          }
          break;

        case TH_HEADER:
          cp = ts->header;
          break;

        case TH_HEADER_TEXT:
          cp = ts->header_text;
          break;
        case TH_HEADER_TEXT_HI:
          cp = ts->header_text_hi;
          break;

        case TH_PANEL_HEADER:
          cp = btheme->tui.panel_header;
          break;
        case TH_PANEL_BACK:
          cp = btheme->tui.panel_back;
          break;
        case TH_PANEL_SUB_BACK:
          cp = btheme->tui.panel_sub_back;
          break;
        case TH_PANEL_OUTLINE:
          cp = btheme->tui.panel_outline;
          break;
        case TH_PANEL_ACTIVE:
          cp = btheme->tui.panel_active;
          break;

        case TH_TAB_TEXT:
          cp = btheme->tui.wcol_tab.text;
          break;
        case TH_TAB_TEXT_HI:
          cp = btheme->tui.wcol_tab.text_sel;
          break;
        case TH_TAB_ACTIVE:
          cp = btheme->tui.wcol_tab.inner_sel;
          break;
        case TH_TAB_INACTIVE:
          cp = btheme->tui.wcol_tab.inner;
          break;
        case TH_TAB_OUTLINE:
          cp = btheme->tui.wcol_tab.outline;
          break;
        case TH_TAB_OUTLINE_ACTIVE:
          cp = btheme->tui.wcol_tab.outline_sel;
          break;
        case TH_TAB_BACK:
          cp = btheme->regions.sidebars.tab_back;
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
          cp = btheme->regions.scrubbing.back;
          break;
        case TH_TIME_SCRUB_TEXT:
          cp = btheme->regions.scrubbing.text;
          break;
        case TH_TIME_MARKER_LINE:
          cp = btheme->regions.scrubbing.time_marker;
          break;
        case TH_TIME_MARKER_LINE_SELECTED:
          cp = btheme->regions.scrubbing.time_marker_selected;
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
          cp = btheme->common.anim.channel_group;
          break;
        case TH_GROUP_ACTIVE:
          cp = btheme->common.anim.channel_group_active;
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
        case TH_EDGE_WIDTH:
          cp = &ts->edge_width;
          break;
        case TH_EDGE_SELECT:
          cp = ts->edge_select;
          break;
        case TH_EDGE_MODE_SELECT:
          cp = ts->edge_mode_select;
          break;
        case TH_EDITMESH_ACTIVE:
          cp = ts->editmesh_active;
          break;
        case TH_FACE:
          cp = ts->face;
          break;
        case TH_FACE_SELECT:
          cp = ts->face_select;
          break;
        case TH_FACE_MODE_SELECT:
          cp = ts->face_mode_select;
          break;
        case TH_FACE_RETOPOLOGY:
          cp = ts->face_retopology;
          break;
        case TH_FACE_BACK:
          cp = ts->face_back;
          break;
        case TH_FACE_FRONT:
          cp = ts->face_front;
          break;
        case TH_FACEDOT_SIZE:
          cp = &ts->facedot_size;
          break;

        case TH_BEVEL:
          cp = btheme->space_view3d.bevel;
          break;
        case TH_CREASE:
          cp = btheme->space_view3d.crease;
          break;
        case TH_SEAM:
          cp = btheme->space_view3d.seam;
          break;
        case TH_SHARP:
          cp = btheme->space_view3d.sharp;
          break;
        case TH_FREESTYLE:
          cp = btheme->space_view3d.freestyle;
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
        case TH_LONGKEY:
          cp = btheme->common.anim.long_key;
          break;
        case TH_LONGKEY_SELECT:
          cp = btheme->common.anim.long_key_selected;
          break;
        case TH_STRIP:
          cp = ts->strip;
          break;
        case TH_STRIP_SELECT:
          cp = ts->strip_select;
          break;
        case TH_CHANNEL:
          cp = btheme->common.anim.channel;
          break;
        case TH_CHANNEL_SELECT:
          cp = btheme->common.anim.channel_selected;
          break;
        case TH_KEYTYPE_KEYFRAME:
          cp = btheme->common.anim.keyframe;
          break;
        case TH_KEYTYPE_KEYFRAME_SELECT:
          cp = btheme->common.anim.keyframe_selected;
          break;
        case TH_KEYTYPE_EXTREME:
          cp = btheme->common.anim.keyframe_extreme;
          break;
        case TH_KEYTYPE_EXTREME_SELECT:
          cp = btheme->common.anim.keyframe_extreme_selected;
          break;
        case TH_KEYTYPE_BREAKDOWN:
          cp = btheme->common.anim.keyframe_breakdown;
          break;
        case TH_KEYTYPE_BREAKDOWN_SELECT:
          cp = btheme->common.anim.keyframe_breakdown_selected;
          break;
        case TH_KEYTYPE_JITTER:
          cp = btheme->common.anim.keyframe_jitter;
          break;
        case TH_KEYTYPE_JITTER_SELECT:
          cp = btheme->common.anim.keyframe_jitter_selected;
          break;
        case TH_KEYTYPE_MOVEHOLD:
          cp = btheme->common.anim.keyframe_moving_hold;
          break;
        case TH_KEYTYPE_MOVEHOLD_SELECT:
          cp = btheme->common.anim.keyframe_moving_hold_selected;
          break;
        case TH_KEYTYPE_GENERATED:
          cp = btheme->common.anim.keyframe_generated;
          break;
        case TH_KEYTYPE_GENERATED_SELECT:
          cp = btheme->common.anim.keyframe_generated_selected;
          break;
        case TH_KEYBORDER:
          cp = ts->keyborder;
          break;
        case TH_KEYBORDER_SELECT:
          cp = ts->keyborder_select;
          break;
        case TH_CFRAME:
          cp = btheme->common.anim.playhead;
          break;
        case TH_FRAME_BEFORE:
          cp = ts->before_current_frame;
          break;
        case TH_FRAME_AFTER:
          cp = ts->after_current_frame;
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

        case TH_HANDLE_FREE:
          cp = btheme->common.curves.handle_free;
          break;
        case TH_HANDLE_SEL_FREE:
          cp = btheme->common.curves.handle_sel_free;
          break;
        case TH_HANDLE_AUTO:
          cp = btheme->common.curves.handle_auto;
          break;
        case TH_HANDLE_SEL_AUTO:
          cp = btheme->common.curves.handle_sel_auto;
          break;
        case TH_HANDLE_VECT:
          cp = btheme->common.curves.handle_vect;
          break;
        case TH_HANDLE_SEL_VECT:
          cp = btheme->common.curves.handle_sel_vect;
          break;
        case TH_HANDLE_ALIGN:
          cp = btheme->common.curves.handle_align;
          break;
        case TH_HANDLE_SEL_ALIGN:
          cp = btheme->common.curves.handle_sel_align;
          break;
        case TH_HANDLE_AUTOCLAMP:
          cp = btheme->common.curves.handle_auto_clamped;
          break;
        case TH_HANDLE_SEL_AUTOCLAMP:
          cp = btheme->common.curves.handle_sel_auto_clamped;
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
        case TH_NODE_OUTLINE:
          cp = ts->node_outline;
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
        case TH_NODE_SCRIPT:
          cp = ts->nodeclass_script;
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
        case TH_NODE_CONVERTER:
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
        case TH_NODE_ZONE_SIMULATION:
          cp = ts->node_zone_simulation;
          break;
        case TH_NODE_ZONE_REPEAT:
          cp = ts->node_zone_repeat;
          break;
        case TH_NODE_ZONE_FOREACH_GEOMETRY_ELEMENT:
          cp = ts->node_zone_foreach_geometry_element;
          break;
        case TH_NODE_ZONE_CLOSURE:
          cp = ts->node_zone_closure;
          break;
        case TH_SIMULATED_FRAMES:
          cp = ts->simulated_frames;
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
        case TH_SEQ_TRANSITION:
          cp = ts->transition;
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
        case TH_SEQ_TEXT_CURSOR:
          cp = ts->text_strip_cursor;
          break;
        case TH_SEQ_SELECTED_TEXT:
          cp = ts->selected_text;
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
          cp = btheme->common.curves.handle_vertex;
          break;
        case TH_HANDLE_VERTEX_SELECT:
          cp = btheme->common.curves.handle_vertex_select;
          break;
        case TH_HANDLE_VERTEX_SIZE:
          cp = &btheme->common.curves.handle_vertex_size;
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
          cp = btheme->common.anim.channels;
          break;
        case TH_DOPESHEET_CHANNELSUBOB:
          cp = btheme->common.anim.channels_sub;
          break;
        case TH_DOPESHEET_IPOLINE:
          cp = ts->anim_interpolation_linear;
          break;
        case TH_DOPESHEET_IPOCONST:
          cp = ts->anim_interpolation_constant;
          break;
        case TH_DOPESHEET_IPOOTHER:
          cp = ts->anim_interpolation_other;
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
        case TH_CAMERA_PASSEPARTOUT:
          cp = ts->camera_passepartout;
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
          cp = btheme->common.anim.preview_range;
          break;
        case TH_ANIM_SCENE_STRIP_RANGE:
          cp = btheme->common.anim.scene_strip_range;
          break;

        case TH_NLA_TWEAK:
          cp = ts->nla_tweaking;
          break;
        case TH_NLA_TWEAK_DUPLI:
          cp = ts->nla_tweakdupli;
          break;

        case TH_NLA_TRACK:
          cp = btheme->common.anim.channel;
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

        case TH_EDITOR_BORDER:
          cp = btheme->tui.editor_border;
          break;
        case TH_EDITOR_OUTLINE:
          cp = btheme->tui.editor_outline;
          break;
        case TH_EDITOR_OUTLINE_ACTIVE:
          cp = btheme->tui.editor_outline_active;
          break;
        case TH_WIDGET_TEXT_CURSOR:
          cp = btheme->tui.widget_text_cursor;
          break;
        case TH_WIDGET_TEXT_SELECTION:
          cp = btheme->tui.wcol_text.item;
          break;
        case TH_WIDGET_TEXT_HIGHLIGHT:
          cp = btheme->tui.wcol_text.text_sel;
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
        case TH_AXIS_W:
          cp = btheme->tui.waxis;
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
        case TH_ICON_AUTOKEY:
          cp = btheme->tui.icon_autokey;
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
        case TH_INFO_ERROR_TEXT:
          cp = ts->info_error_text;
          break;
        case TH_INFO_WARNING_TEXT:
          cp = ts->info_warning_text;
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

void UI_theme_init_default()
{
  /* We search for the theme with the default name. */
  bTheme *btheme = static_cast<bTheme *>(
      BLI_findstring(&U.themes, U_theme_default.name, offsetof(bTheme, name)));
  if (btheme == nullptr) {
    btheme = MEM_callocN<bTheme>(__func__);
    STRNCPY_UTF8(btheme->name, U_theme_default.name);
    BLI_addhead(&U.themes, btheme);
  }

  /* Must be first, see `U.themes` doc-string. */
  BLI_listbase_rotate_first(&U.themes, btheme);

  UI_SetTheme(0, 0); /* make sure the global used in this file is set */

  const int active_theme_area = btheme->active_theme_area;
  MEMCPY_STRUCT_AFTER(btheme, &U_theme_default, name);
  btheme->active_theme_area = active_theme_area;
}

void UI_style_init_default()
{
  BLI_freelistN(&U.uistyles);
  /* gets automatically re-allocated */
  uiStyleInit();
}

void UI_SetTheme(int spacetype, int regionid)
{
  if (spacetype) {
    /* later on, a local theme can be found too */
    g_theme_state.theme = static_cast<bTheme *>(U.themes.first);
    g_theme_state.spacetype = spacetype;
    g_theme_state.regionid = regionid;
  }
  else if (regionid) {
    /* popups */
    g_theme_state.theme = static_cast<bTheme *>(U.themes.first);
    g_theme_state.spacetype = SPACE_PROPERTIES;
    g_theme_state.regionid = regionid;
  }
  else {
    /* for safety, when theme was deleted */
    g_theme_state.theme = static_cast<bTheme *>(U.themes.first);
    g_theme_state.spacetype = SPACE_VIEW3D;
    g_theme_state.regionid = RGN_TYPE_WINDOW;
  }
}

bTheme *UI_GetTheme()
{
  return static_cast<bTheme *>(U.themes.first);
}

void UI_Theme_Store(bThemeState *theme_state)
{
  *theme_state = g_theme_state;
}
void UI_Theme_Restore(const bThemeState *theme_state)
{
  g_theme_state = *theme_state;
}

void UI_GetThemeColorShadeAlpha4ubv(int colorid, int coloffset, int alphaoffset, uchar col[4])
{
  int r, g, b, a;
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  r = coloffset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = coloffset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = coloffset + int(cp[2]);
  CLAMP(b, 0, 255);
  a = alphaoffset + int(cp[3]);
  CLAMP(a, 0, 255);

  col[0] = r;
  col[1] = g;
  col[2] = b;
  col[3] = a;
}

void UI_GetThemeColorBlend3ubv(int colorid1, int colorid2, float fac, uchar col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);
  col[0] = floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
  col[1] = floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
  col[2] = floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
}

void UI_GetThemeColorBlend3f(int colorid1, int colorid2, float fac, float r_col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);
  r_col[0] = ((1.0f - fac) * cp1[0] + fac * cp2[0]) / 255.0f;
  r_col[1] = ((1.0f - fac) * cp1[1] + fac * cp2[1]) / 255.0f;
  r_col[2] = ((1.0f - fac) * cp1[2] + fac * cp2[2]) / 255.0f;
}

void UI_GetThemeColorBlend4f(int colorid1, int colorid2, float fac, float r_col[4])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid2);

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

float UI_GetThemeValuef(int colorid)
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  return float(cp[0]);
}

int UI_GetThemeValue(int colorid)
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  return int(cp[0]);
}

float UI_GetThemeValueTypef(int colorid, int spacetype)
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, spacetype, colorid);
  return float(cp[0]);
}

int UI_GetThemeValueType(int colorid, int spacetype)
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, spacetype, colorid);
  return int(cp[0]);
}

void UI_GetThemeColor3fv(int colorid, float col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  col[0] = float(cp[0]) / 255.0f;
  col[1] = float(cp[1]) / 255.0f;
  col[2] = float(cp[2]) / 255.0f;
}

void UI_GetThemeColor4fv(int colorid, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  col[0] = float(cp[0]) / 255.0f;
  col[1] = float(cp[1]) / 255.0f;
  col[2] = float(cp[2]) / 255.0f;
  col[3] = float(cp[3]) / 255.0f;
}

void UI_GetThemeColorType4fv(int colorid, int spacetype, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, spacetype, colorid);
  col[0] = float(cp[0]) / 255.0f;
  col[1] = float(cp[1]) / 255.0f;
  col[2] = float(cp[2]) / 255.0f;
  col[3] = float(cp[3]) / 255.0f;
}

void UI_GetThemeColorShade3fv(int colorid, int offset, float col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  int r, g, b;

  r = offset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = offset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = offset + int(cp[2]);
  CLAMP(b, 0, 255);

  col[0] = float(r) / 255.0f;
  col[1] = float(g) / 255.0f;
  col[2] = float(b) / 255.0f;
}

void UI_GetThemeColorShade3ubv(int colorid, int offset, uchar col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  int r, g, b;

  r = offset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = offset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = offset + int(cp[2]);
  CLAMP(b, 0, 255);

  col[0] = r;
  col[1] = g;
  col[2] = b;
}

void UI_GetThemeColorBlendShade3ubv(
    int colorid1, int colorid2, float fac, int offset, uchar col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid2);

  CLAMP(fac, 0.0f, 1.0f);

  float blend[3];
  blend[0] = (offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0])) / 255.0f;
  blend[1] = (offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1])) / 255.0f;
  blend[2] = (offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2])) / 255.0f;

  unit_float_to_uchar_clamp_v3(col, blend);
}

void UI_GetThemeColorShade4ubv(int colorid, int offset, uchar col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  int r, g, b;

  r = offset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = offset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = offset + int(cp[2]);
  CLAMP(b, 0, 255);

  col[0] = r;
  col[1] = g;
  col[2] = b;
  col[3] = cp[3];
}

void UI_GetThemeColorShadeAlpha4fv(int colorid, int coloffset, int alphaoffset, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  int r, g, b, a;

  r = coloffset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = coloffset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = coloffset + int(cp[2]);
  CLAMP(b, 0, 255);
  a = alphaoffset + int(cp[3]);
  CLAMP(a, 0, 255);

  col[0] = float(r) / 255.0f;
  col[1] = float(g) / 255.0f;
  col[2] = float(b) / 255.0f;
  col[3] = float(a) / 255.0f;
}

void UI_GetThemeColorBlendShade3fv(int colorid1, int colorid2, float fac, int offset, float col[3])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid2);
  int r, g, b;

  CLAMP(fac, 0.0f, 1.0f);

  r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
  CLAMP(r, 0, 255);
  g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
  CLAMP(g, 0, 255);
  b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
  CLAMP(b, 0, 255);

  col[0] = float(r) / 255.0f;
  col[1] = float(g) / 255.0f;
  col[2] = float(b) / 255.0f;
}

void UI_GetThemeColorBlendShade4fv(int colorid1, int colorid2, float fac, int offset, float col[4])
{
  const uchar *cp1 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid1);
  const uchar *cp2 = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid2);
  int r, g, b, a;

  CLAMP(fac, 0.0f, 1.0f);

  r = offset + floorf((1.0f - fac) * cp1[0] + fac * cp2[0]);
  CLAMP(r, 0, 255);
  g = offset + floorf((1.0f - fac) * cp1[1] + fac * cp2[1]);
  CLAMP(g, 0, 255);
  b = offset + floorf((1.0f - fac) * cp1[2] + fac * cp2[2]);
  CLAMP(b, 0, 255);

  a = floorf((1.0f - fac) * cp1[3] + fac * cp2[3]); /* No shading offset. */
  CLAMP(a, 0, 255);

  col[0] = float(r) / 255.0f;
  col[1] = float(g) / 255.0f;
  col[2] = float(b) / 255.0f;
  col[3] = float(a) / 255.0f;
}

void UI_GetThemeColor3ubv(int colorid, uchar col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
}

void UI_GetThemeColorShade4fv(int colorid, int offset, float col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  int r, g, b, a;

  r = offset + int(cp[0]);
  CLAMP(r, 0, 255);
  g = offset + int(cp[1]);
  CLAMP(g, 0, 255);
  b = offset + int(cp[2]);
  CLAMP(b, 0, 255);

  a = int(cp[3]); /* no shading offset... */
  CLAMP(a, 0, 255);

  col[0] = float(r) / 255.0f;
  col[1] = float(g) / 255.0f;
  col[2] = float(b) / 255.0f;
  col[3] = float(a) / 255.0f;
}

void UI_GetThemeColor4ubv(int colorid, uchar col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
  col[3] = cp[3];
}

void UI_GetThemeColorType3fv(int colorid, int spacetype, float col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, spacetype, colorid);
  col[0] = float(cp[0]) / 255.0f;
  col[1] = float(cp[1]) / 255.0f;
  col[2] = float(cp[2]) / 255.0f;
}

void UI_GetThemeColorType3ubv(int colorid, int spacetype, uchar col[3])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
}

void UI_GetThemeColorType4ubv(int colorid, int spacetype, uchar col[4])
{
  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, spacetype, colorid);
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
  else if (!((g_theme_state.spacetype == SPACE_OUTLINER &&
              g_theme_state.regionid == RGN_TYPE_WINDOW) ||
             (g_theme_state.spacetype == SPACE_PROPERTIES &&
              g_theme_state.regionid == RGN_TYPE_NAV_BAR) ||
             (g_theme_state.spacetype == SPACE_FILE && g_theme_state.regionid == RGN_TYPE_WINDOW)))
  {
    /* Only colored icons in specific places, overall UI is intended
     * to stay monochrome and out of the way except a few places where it
     * is important to communicate different data types. */
    return false;
  }

  const uchar *cp = UI_ThemeGetColorPtr(g_theme_state.theme, g_theme_state.spacetype, colorid);
  col[0] = cp[0];
  col[1] = cp[1];
  col[2] = cp[2];
  col[3] = cp[3];

  return true;
}

void UI_GetColorPtrBlendAlpha4fv(
    const float cp1[4], const float cp2[4], float fac, const float alphaoffset, float r_col[4])
{
  float r, g, b, a;

  CLAMP(fac, 0.0f, 1.0f);
  r = (1.0f - fac) * cp1[0] + fac * cp2[0];
  g = (1.0f - fac) * cp1[1] + fac * cp2[1];
  b = (1.0f - fac) * cp1[2] + fac * cp2[2];
  a = (1.0f - fac) * cp1[3] + fac * cp2[3] + alphaoffset;

  CLAMP(r, 0.0f, 1.0f);
  CLAMP(g, 0.0f, 1.0f);
  CLAMP(b, 0.0f, 1.0f);
  CLAMP(a, 0.0f, 1.0f);

  r_col[0] = r;
  r_col[1] = g;
  r_col[2] = b;
  r_col[3] = a;
}

void UI_GetColorPtrShade3ubv(const uchar cp[3], int offset, uchar r_col[3])
{
  int r, g, b;

  r = offset + int(cp[0]);
  g = offset + int(cp[1]);
  b = offset + int(cp[2]);

  CLAMP(r, 0, 255);
  CLAMP(g, 0, 255);
  CLAMP(b, 0, 255);

  r_col[0] = r;
  r_col[1] = g;
  r_col[2] = b;
}

void UI_GetColorPtrBlendShade3ubv(
    const uchar cp1[3], const uchar cp2[3], float fac, int offset, uchar r_col[3])
{
  int r, g, b;

  CLAMP(fac, 0.0f, 1.0f);
  r = offset + floor((1.0f - fac) * cp1[0] + fac * cp2[0]);
  g = offset + floor((1.0f - fac) * cp1[1] + fac * cp2[1]);
  b = offset + floor((1.0f - fac) * cp1[2] + fac * cp2[2]);

  CLAMP(r, 0, 255);
  CLAMP(g, 0, 255);
  CLAMP(b, 0, 255);

  r_col[0] = r;
  r_col[1] = g;
  r_col[2] = b;
}

void UI_ThemeClearColor(int colorid)
{
  float col[3];

  UI_GetThemeColor3fv(colorid, col);
  GPU_clear_color(col[0], col[1], col[2], 1.0f);
}

int UI_ThemeMenuShadowWidth()
{
  const bTheme *btheme = UI_GetTheme();
  return int(btheme->tui.menu_shadow_width * UI_SCALE_FAC);
}

void UI_make_axis_color(const uchar col[3], const char axis, uchar r_col[3])
{
  uchar col_axis[3];

  switch (axis) {
    case 'X':
      UI_GetThemeColor3ubv(TH_AXIS_X, col_axis);
      UI_GetColorPtrBlendShade3ubv(col, col_axis, 0.5f, -10, r_col);
      break;
    case 'Y':
      UI_GetThemeColor3ubv(TH_AXIS_Y, col_axis);
      UI_GetColorPtrBlendShade3ubv(col, col_axis, 0.5f, -10, r_col);
      break;
    case 'Z':
      UI_GetThemeColor3ubv(TH_AXIS_Z, col_axis);
      UI_GetColorPtrBlendShade3ubv(col, col_axis, 0.5f, -10, r_col);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

/** \} */
