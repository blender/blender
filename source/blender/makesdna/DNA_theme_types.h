/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/**
 * Scaling factor for all UI elements, based on the "Resolution Scale" user preference and the
 * DPI/OS Scale of each monitor. This is a read-only, run-time value calculated by
 * `WM_window_dpi_set_userdef` at various times, including between the drawing of each window and
 * so can vary between monitors.
 */
#define UI_SCALE_FAC ((void)0, U.scale_factor)

/* Inverse of UI_SCALE_FAC ( 1 / UI_SCALE_FAC). */
#define UI_INV_SCALE_FAC ((void)0, U.inv_scale_factor)

/* 16 to copy ICON_DEFAULT_HEIGHT */
#define UI_ICON_SIZE ((float)16 * U.scale_factor)

/* Themes; defines in `BIF_resource.h`. */

/* ************************ style definitions ******************** */

/**
 * Default offered by Blender.
 * #uiFont.uifont_id
 */
typedef enum eUIFont_ID {
  UIFONT_DEFAULT = 0,
  // UIFONT_BITMAP = 1, /* UNUSED */

  /* free slots */
  UIFONT_CUSTOM1 = 2,
  // UIFONT_CUSTOM2 = 3, /* UNUSED */
} eUIFont_ID;

/**
 * Default fonts to load/initialize.
 * First font is the default (index 0), others optional.
 */
#
#
typedef struct uiFont {
  struct uiFont *next, *prev;
  char filepath[/*FILE_MAX*/ 1024];
  /** From BLF library. */
  short blf_id;
  /** Own id (eUIFont_ID). */
  short uifont_id;
} uiFont;

/** This state defines appearance of text. */
typedef struct uiFontStyle {
  /** Saved in file, 0 is default. */
  short uifont_id;
  char _pad1[2];
  /** Actual size depends on 'global' DPI. */
  float points;
  /** Style hint. */
  short italic, bold;
  /** Value is amount of pixels blur. */
  short shadow;
  /** Shadow offset in pixels. */
  short shadx, shady;
  char _pad0[2];
  /** Total alpha. */
  float shadowalpha;
  /** 1 value, typically white or black anyway. */
  float shadowcolor;
  /** Weight class 100-900, 400 is normal. */
  int character_weight;
} uiFontStyle;

/* this is fed to the layout engine and widget code */

typedef struct uiStyle {
  struct uiStyle *next, *prev;

  char name[/*MAX_NAME*/ 64];

  uiFontStyle paneltitle;
  uiFontStyle grouplabel;
  uiFontStyle widget;
  uiFontStyle tooltip;

  float panelzoom;

  /** In characters. */
  short minlabelchars;
  /** In characters. */
  short minwidgetchars;

  short columnspace;
  short templatespace;
  short boxspace;
  short buttonspacex;
  short buttonspacey;
  short panelspace;
  short panelouter;

  char _pad0[2];
} uiStyle;

typedef struct ThemeRegionsAssetShelf {
  unsigned char back[4];
  unsigned char header_back[4];
} ThemeRegionsAssetShelf;

typedef struct ThemeRegionsChannels {
  unsigned char back[4];
  unsigned char text[4];
  unsigned char text_selected[4];
  char _pad0[4];
} ThemeRegionsChannels;

typedef struct ThemeRegionsScrubbing {
  unsigned char back[4];
  unsigned char text[4];
  unsigned char time_marker[4], time_marker_selected[4];
} ThemeRegionsScrubbing;

typedef struct ThemeRegionsSidebars {
  unsigned char back[4];
  unsigned char tab_back[4];
} ThemeRegionsSidebars;

typedef struct ThemeRegions {
  ThemeRegionsAssetShelf asset_shelf;
  ThemeRegionsChannels channels;
  ThemeRegionsScrubbing scrubbing;
  ThemeRegionsSidebars sidebars;
} ThemeRegions;

typedef struct ThemeCommonAnim {
  unsigned char playhead[4];
  unsigned char preview_range[4];

  unsigned char channels[4], channels_sub[4];
  unsigned char channel_group[4], channel_group_active[4];
  unsigned char channel[4], channel_selected[4];

  /** Key-types. */
  unsigned char keyframe[4], keyframe_extreme[4], keyframe_breakdown[4], keyframe_jitter[4],
      keyframe_moving_hold[4], keyframe_generated[4];
  unsigned char keyframe_selected[4], keyframe_extreme_selected[4], keyframe_breakdown_selected[4],
      keyframe_jitter_selected[4], keyframe_moving_hold_selected[4],
      keyframe_generated_selected[4];
  unsigned char long_key[4], long_key_selected[4];

  unsigned char scene_strip_range[4];
  char _pad0[4];
} ThemeCommonAnim;

typedef struct ThemeCommonCurves {
  /** Curve handles. */
  unsigned char handle_free[4], handle_auto[4], handle_vect[4], handle_align[4],
      handle_auto_clamped[4];
  unsigned char handle_sel_free[4], handle_sel_auto[4], handle_sel_vect[4], handle_sel_align[4],
      handle_sel_auto_clamped[4];

  /** Curve points. */
  unsigned char handle_vertex[4];
  unsigned char handle_vertex_select[4];
  unsigned char handle_vertex_size;

  char _pad0[3];
} ThemeCommonCurves;

typedef struct ThemeCommon {
  ThemeCommonAnim anim;
  ThemeCommonCurves curves;
  char _pad0[4];
} ThemeCommon;

typedef struct uiWidgetColors {
  unsigned char outline[4];
  unsigned char outline_sel[4];
  unsigned char inner[4];
  unsigned char inner_sel[4];
  unsigned char item[4];
  unsigned char text[4];
  unsigned char text_sel[4];
  unsigned char shaded;
  char _pad0[3];
  short shadetop, shadedown;
  float roundness;
} uiWidgetColors;

typedef struct uiWidgetStateColors {
  unsigned char error[4];
  unsigned char warning[4];
  unsigned char info[4];
  unsigned char success[4];
  unsigned char inner_anim[4];
  unsigned char inner_anim_sel[4];
  unsigned char inner_key[4];
  unsigned char inner_key_sel[4];
  unsigned char inner_driven[4];
  unsigned char inner_driven_sel[4];
  unsigned char inner_overridden[4];
  unsigned char inner_overridden_sel[4];
  unsigned char inner_changed[4];
  unsigned char inner_changed_sel[4];
  float blend;
  char _pad0[4];
} uiWidgetStateColors;

typedef struct ThemeUI {
  /* Interface Elements (buttons, menus, icons) */
  uiWidgetColors wcol_regular, wcol_tool, wcol_toolbar_item, wcol_text;
  uiWidgetColors wcol_radio, wcol_option, wcol_toggle;
  uiWidgetColors wcol_num, wcol_numslider, wcol_tab, wcol_curve;
  uiWidgetColors wcol_menu, wcol_pulldown, wcol_menu_back, wcol_menu_item, wcol_tooltip;
  uiWidgetColors wcol_box, wcol_scroll, wcol_progress, wcol_list_item, wcol_pie_menu;

  uiWidgetStateColors wcol_state;

  unsigned char widget_emboss[4];

  /* fac: 0 - 1 for blend factor, width in pixels */
  float menu_shadow_fac;
  short menu_shadow_width;

  unsigned char editor_border[4];
  unsigned char editor_outline[4];
  unsigned char editor_outline_active[4];

  /* Transparent Grid */
  unsigned char transparent_checker_primary[4], transparent_checker_secondary[4];
  unsigned char transparent_checker_size;
  char _pad1[5];

  float icon_alpha;
  float icon_saturation;
  unsigned char widget_text_cursor[4];

  /* Axis Colors */
  unsigned char xaxis[4], yaxis[4], zaxis[4], waxis[4];

  /* Gizmo Colors. */
  unsigned char gizmo_hi[4];
  unsigned char gizmo_primary[4];
  unsigned char gizmo_secondary[4];
  unsigned char gizmo_view_align[4];
  unsigned char gizmo_a[4];
  unsigned char gizmo_b[4];

  /* Icon Colors. */
  /** Scene items. */
  unsigned char icon_scene[4];
  /** Collection items. */
  unsigned char icon_collection[4];
  /** Object items. */
  unsigned char icon_object[4];
  /** Object data items. */
  unsigned char icon_object_data[4];
  /** Modifier and constraint items. */
  unsigned char icon_modifier[4];
  /** Shading related items. */
  unsigned char icon_shading[4];
  /** File folders. */
  unsigned char icon_folder[4];
  /** Auto Keying indicator. */
  unsigned char icon_autokey[4];
  char _pad3[4];
  /** Intensity of the border icons. >0 will render an border around themed
   * icons. */
  float icon_border_intensity;
  /* Panels. */
  float panel_roundness;
  unsigned char panel_header[4];
  unsigned char panel_back[4];
  unsigned char panel_sub_back[4];
  unsigned char panel_outline[4];
  unsigned char panel_title[4];
  unsigned char panel_text[4];
  unsigned char panel_active[4];

} ThemeUI;

/* try to put them all in one, if needed a special struct can be created as well
 * for example later on, when we introduce wire colors for ob types or so...
 */
typedef struct ThemeSpace {
  /* main window colors */
  unsigned char back[4];
  unsigned char back_grad[4];

  char background_type;
  char _pad0[3];

  /** Panel title. */
  unsigned char title[4];
  unsigned char text[4];
  unsigned char text_hi[4];

  /* header colors */
  /** Region background. */
  unsigned char header[4];
  /** Unused. */
  unsigned char header_title[4];
  unsigned char header_text[4];
  unsigned char header_text_hi[4];

  /* button/tool regions */
  unsigned char shade1[4];
  unsigned char shade2[4];

  unsigned char hilite[4];
  unsigned char grid[4];

  unsigned char view_overlay[4];

  unsigned char wire[4], wire_edit[4], select[4];
  unsigned char lamp[4], speaker[4], empty[4], camera[4];
  unsigned char active[4], transform[4];
  unsigned char vertex[4], vertex_select[4], vertex_active[4], vertex_unreferenced[4];
  unsigned char edge[4], edge_select[4], edge_mode_select[4];
  /** Solid faces. */
  unsigned char face[4], face_select[4], face_mode_select[4], face_retopology[4];
  unsigned char face_back[4], face_front[4];
  /** Selected color. */
  unsigned char extra_edge_len[4], extra_edge_angle[4], extra_face_angle[4], extra_face_area[4];
  unsigned char normal[4];
  unsigned char vertex_normal[4];
  unsigned char loop_normal[4];
  unsigned char bone_solid[4], bone_pose[4], bone_pose_active[4], bone_locked_weight[4];
  unsigned char strip[4], strip_select[4];
  unsigned char before_current_frame[4], after_current_frame[4];
  unsigned char time_gp_keyframe[4];

  /** Geometry attributes. */
  unsigned char bevel[4], seam[4], sharp[4], crease[4], freestyle[4];

  unsigned char nurb_uline[4], nurb_vline[4];
  unsigned char nurb_sel_uline[4], nurb_sel_vline[4];

  /** Dope-sheet. */
  unsigned char anim_interpolation_linear[4], anim_interpolation_constant[4],
      anim_interpolation_other[4];
  /** Keyframe border. */
  unsigned char keyborder[4], keyborder_select[4];
  char _pad4[3];

  unsigned char console_output[4], console_input[4], console_info[4], console_error[4];
  unsigned char console_cursor[4], console_select[4];

  unsigned char vertex_size, edge_width, outline_width, obcenter_dia, facedot_size;
  unsigned char noodle_curving;
  unsigned char grid_levels;
  char _pad2[2];
  float dash_alpha;

  /* Syntax for text-window and nodes. */
  unsigned char syntaxl[4], syntaxs[4]; /* In node-space used for backdrop matte. */
  unsigned char syntaxb[4], syntaxn[4]; /* In node-space used for color input. */
  unsigned char syntaxv[4], syntaxc[4]; /* In node-space used for converter group. */
  unsigned char syntaxd[4], syntaxr[4]; /* In node-space used for distort. */

  unsigned char line_numbers[4];

  unsigned char node_outline[4];

  unsigned char nodeclass_output[4], nodeclass_filter[4];
  unsigned char nodeclass_vector[4], nodeclass_texture[4];
  unsigned char nodeclass_shader[4], nodeclass_script[4];
  unsigned char nodeclass_geometry[4], nodeclass_attribute[4];

  unsigned char node_zone_simulation[4];
  unsigned char node_zone_repeat[4];
  unsigned char node_zone_foreach_geometry_element[4];
  unsigned char node_zone_closure[4];
  unsigned char simulated_frames[4];

  /** For sequence editor. */
  unsigned char movie[4], movieclip[4], mask[4], image[4], scene[4], audio[4];
  unsigned char effect[4], transition[4], meta[4], text_strip[4], color_strip[4];
  unsigned char active_strip[4], selected_strip[4], text_strip_cursor[4], selected_text[4];

  /** For dope-sheet - scale factor for size of keyframes (i.e. height of channels). */
  float keyframe_scale_fac;

  unsigned char editmesh_active[4];
  char _pad3[1];

  unsigned char clipping_border_3d[4];

  unsigned char marker_outline[4], marker[4], act_marker[4], sel_marker[4], dis_marker[4],
      lock_marker[4];
  unsigned char bundle_solid[4];
  unsigned char path_before[4], path_after[4];
  unsigned char path_keyframe_before[4], path_keyframe_after[4];
  unsigned char camera_path[4];
  unsigned char camera_passepartout[4];
  unsigned char _pad1[2];

  unsigned char gp_vertex_size;
  unsigned char gp_vertex[4], gp_vertex_select[4];

  unsigned char preview_back[4];
  unsigned char preview_stitch_face[4];
  unsigned char preview_stitch_edge[4];
  unsigned char preview_stitch_vert[4];
  unsigned char preview_stitch_stitchable[4];
  unsigned char preview_stitch_unstitchable[4];
  unsigned char preview_stitch_active[4];

  /** Two uses, for uvs with modifier applied on mesh and uvs during painting. */
  unsigned char uv_shadow[4];

  /** Search filter match, used for property search and in the outliner. */
  unsigned char match[4];
  /** Outliner - selected item. */
  unsigned char selected_highlight[4];
  /** Outliner - selected object. */
  unsigned char selected_object[4];
  /** Outliner - active object. */
  unsigned char active_object[4];
  /** Outliner - edited object. */
  unsigned char edited_object[4];
  /** Outliner - row color difference. */
  unsigned char row_alternate[4];

  /** Skin modifier root color. */
  unsigned char skin_root[4];

  /* NLA */
  /** Active Action + Summary Channel. */
  unsigned char anim_active[4];
  /** Active Action = NULL. */
  unsigned char anim_non_active[4];

  /** NLA 'Tweaking' action/strip. */
  unsigned char nla_tweaking[4];
  /** NLA - warning color for duplicate instances of tweaking strip. */
  unsigned char nla_tweakdupli[4];

  /** NLA "Transition" strips. */
  unsigned char nla_transition[4], nla_transition_sel[4];
  /** NLA "Meta" strips. */
  unsigned char nla_meta[4], nla_meta_sel[4];
  /** NLA "Sound" strips. */
  unsigned char nla_sound[4], nla_sound_sel[4];

  /* info */
  unsigned char info_selected[4], info_selected_text[4];
  unsigned char info_error_text[4];
  unsigned char info_warning_text[4];
  unsigned char info_info_text[4];
  unsigned char info_debug[4], info_debug_text[4];
  unsigned char info_property[4], info_property_text[4];
  unsigned char info_operator[4], info_operator_text[4];

  unsigned char metadatabg[4];
  unsigned char metadatatext[4];
} ThemeSpace;

/* Viewport Background Gradient Types. */

typedef enum eBackgroundGradientTypes {
  TH_BACKGROUND_SINGLE_COLOR = 0,
  TH_BACKGROUND_GRADIENT_LINEAR = 1,
  TH_BACKGROUND_GRADIENT_RADIAL = 2,
} eBackgroundGradientTypes;

/** Set of colors for use as a custom color set for Objects/Bones wire drawing. */
typedef struct ThemeWireColor {
  unsigned char solid[4];
  unsigned char select[4];
  unsigned char active[4];

  /** #eWireColor_Flags. */
  short flag;
  char _pad0[2];
} ThemeWireColor;

/** #ThemeWireColor.flag */
typedef enum eWireColor_Flags {
  TH_WIRECOLOR_CONSTCOLS = (1 << 0),
  /* TH_WIRECOLOR_TEXTCOLS = (1 << 1), */ /* UNUSED */
} eWireColor_Flags;

typedef struct ThemeCollectionColor {
  unsigned char color[4];
} ThemeCollectionColor;

typedef struct ThemeStripColor {
  unsigned char color[4];
} ThemeStripColor;

/**
 * A theme.
 *
 * \note Currently only the first theme is used at once.
 * Different theme presets are stored as external files now.
 */
typedef struct bTheme {
  struct bTheme *next, *prev;
  char name[/*MAX_NAME*/ 64];

  /* NOTE: Values after `name` are copied when resetting the default theme. */

  /**
   * The file-path for the preset that was loaded into this theme.
   *
   * This is needed so it's possible to know if updating or removing a theme preset
   * should apply changes to the current theme.
   */
  char filepath[/*FILE_MAX*/ 1024];

  ThemeUI tui;

  ThemeRegions regions;
  ThemeCommon common;

  /**
   * Individual Space-types:
   * \note Ensure #UI_THEMESPACE_END is updated when adding.
   */
  ThemeSpace space_properties;
  ThemeSpace space_view3d;
  ThemeSpace space_file;
  ThemeSpace space_graph;
  ThemeSpace space_info;
  ThemeSpace space_action;
  ThemeSpace space_nla;
  ThemeSpace space_sequencer;
  ThemeSpace space_image;
  ThemeSpace space_text;
  ThemeSpace space_outliner;
  ThemeSpace space_node;
  ThemeSpace space_preferences;
  ThemeSpace space_console;
  ThemeSpace space_clip;
  ThemeSpace space_topbar;
  ThemeSpace space_statusbar;
  ThemeSpace space_spreadsheet;

  /* 20 sets of bone colors for this theme */
  ThemeWireColor tarm[20];
  // ThemeWireColor tobj[20];

  /* See COLLECTION_COLOR_TOT for the number of collection colors. */
  ThemeCollectionColor collection_color[8];

  /* See STRIP_COLOR_TOT for the total number of strip colors. */
  ThemeStripColor strip_color[9];

  int active_theme_area;
} bTheme;

#define UI_THEMESPACE_START(btheme) \
  (CHECK_TYPE_INLINE(btheme, bTheme *), &((btheme)->space_properties))
#define UI_THEMESPACE_END(btheme) \
  (CHECK_TYPE_INLINE(btheme, bTheme *), (&((btheme)->space_spreadsheet) + 1))
