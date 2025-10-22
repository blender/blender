/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#include "BLI_assert.h"

struct bTheme;

/* Define icon enum. */
#define DEF_ICON(name) ICON_##name,
#define DEF_ICON_VECTOR(name) ICON_##name,
#define DEF_ICON_COLOR(name) ICON_##name,
#define DEF_ICON_BLANK(name) ICON_BLANK_##name,

/**
 * Builtin icons with a compile-time icon-id. Dynamically created icons such as preview image
 * icons get a dynamic icon-id <= #BIFICONID_LAST_STATIC, #BIFIconID can hold both.
 */
enum BIFIconID_Static {
/* ui */
#include "UI_icons.hh"
  BIFICONID_LAST_STATIC,
};

/** Type that fits all compile time and dynamic icon-ids. */
using BIFIconID = int;
BLI_STATIC_ASSERT(sizeof(BIFIconID_Static) <= sizeof(BIFIconID),
                  "Expected all builtin icon IDs to fit into `BIFIconID`");

#define BIFICONID_FIRST (ICON_NONE)

/* use to denote intentionally unset theme color */
#define TH_UNDEFINED -1

enum ThemeColorID {
  TH_NONE,
  TH_BLACK,
  TH_WHITE,
  TH_REDALERT,
  TH_ERROR,
  TH_WARNING,
  TH_INFO,
  TH_SUCCESS,

  TH_THEMEUI,
  /* Common colors among spaces. */

  TH_BACK,
  /** Use when 'TH_SHOW_BACK_GRAD' is set (the lower, darker color). */
  TH_BACK_GRAD,
  TH_TEXT,
  TH_TEXT_HI,
  TH_TITLE,

  /* Tabs. */
  TH_TAB_TEXT,
  TH_TAB_TEXT_HI,
  TH_TAB_ACTIVE,
  TH_TAB_INACTIVE,
  TH_TAB_BACK,
  TH_TAB_OUTLINE,
  TH_TAB_OUTLINE_ACTIVE,

  TH_HEADER,
  TH_HEADER_TEXT,
  TH_HEADER_TEXT_HI,

  /* panels */
  TH_PANEL_HEADER,
  TH_PANEL_BACK,
  TH_PANEL_SUB_BACK,
  TH_PANEL_OUTLINE,
  TH_PANEL_ACTIVE,

  TH_SHADE1,
  TH_SHADE2,
  TH_HILITE,

  TH_GRID,
  TH_WIRE,
  TH_WIRE_INNER,
  TH_WIRE_EDIT,
  TH_SELECT,
  TH_ACTIVE,
  TH_GROUP,
  TH_GROUP_ACTIVE,
  TH_TRANSFORM,
  TH_VERTEX,
  TH_VERTEX_SELECT,
  TH_VERTEX_ACTIVE,
  TH_VERTEX_UNREFERENCED,
  TH_VERTEX_SIZE,
  TH_EDGE_WIDTH,
  TH_OUTLINE_WIDTH,
  TH_OBCENTER_DIA,
  TH_EDGE,
  TH_EDGE_SELECT, /* Stands for edge selection, not edge select mode. */
  TH_EDGE_MODE_SELECT,
  TH_FACE,
  TH_FACE_SELECT, /* Stands for face selection, not face select mode. */
  TH_FACE_MODE_SELECT,
  TH_FACE_RETOPOLOGY,
  TH_FACE_BACK,
  TH_FACE_FRONT,
  TH_NORMAL,
  TH_VNORMAL,
  TH_LNORMAL,
  TH_FACEDOT_SIZE,
  TH_CFRAME,
  TH_FRAME_BEFORE,
  TH_FRAME_AFTER,
  TH_TIME_SCRUB_BACKGROUND,
  TH_TIME_SCRUB_TEXT,
  TH_TIME_MARKER_LINE,
  TH_TIME_MARKER_LINE_SELECTED,
  TH_TIME_GP_KEYFRAME,
  TH_NURB_ULINE,
  TH_NURB_VLINE,
  TH_NURB_SEL_ULINE,
  TH_NURB_SEL_VLINE,

  /* this eight colors should be in one block */
  TH_HANDLE_FREE,
  TH_HANDLE_AUTO,
  TH_HANDLE_VECT,
  TH_HANDLE_ALIGN,
  TH_HANDLE_AUTOCLAMP,
  TH_HANDLE_SEL_FREE,
  TH_HANDLE_SEL_AUTO,
  TH_HANDLE_SEL_VECT,
  TH_HANDLE_SEL_ALIGN,
  TH_HANDLE_SEL_AUTOCLAMP,

  TH_SYNTAX_B,
  TH_SYNTAX_V,
  TH_SYNTAX_R,
  TH_SYNTAX_C,
  TH_SYNTAX_L,
  TH_SYNTAX_D,
  TH_SYNTAX_N,
  TH_SYNTAX_S,
  TH_LINENUMBERS,

  TH_BONE_SOLID,
  TH_BONE_POSE,
  TH_BONE_POSE_ACTIVE,
  TH_BONE_LOCKED_WEIGHT,

  TH_STRIP,
  TH_STRIP_SELECT,

  TH_CHANNEL,
  TH_CHANNEL_SELECT,

  TH_LONGKEY,
  TH_LONGKEY_SELECT,

  TH_KEYTYPE_KEYFRAME, /* KEYTYPES */
  TH_KEYTYPE_KEYFRAME_SELECT,
  TH_KEYTYPE_EXTREME,
  TH_KEYTYPE_EXTREME_SELECT,
  TH_KEYTYPE_BREAKDOWN,
  TH_KEYTYPE_BREAKDOWN_SELECT,
  TH_KEYTYPE_JITTER,
  TH_KEYTYPE_JITTER_SELECT,
  TH_KEYTYPE_MOVEHOLD,
  TH_KEYTYPE_MOVEHOLD_SELECT,
  TH_KEYTYPE_GENERATED,
  TH_KEYTYPE_GENERATED_SELECT,

  TH_KEYBORDER,
  TH_KEYBORDER_SELECT,

  TH_LIGHT,
  TH_SPEAKER,
  TH_CAMERA,
  TH_EMPTY,

  TH_NODE,
  TH_NODE_OUTLINE,
  TH_NODE_INPUT,
  TH_NODE_OUTPUT,
  TH_NODE_COLOR,
  TH_NODE_FILTER,
  TH_NODE_VECTOR,
  TH_NODE_TEXTURE,
  TH_NODE_SCRIPT,
  TH_NODE_SHADER,
  TH_NODE_INTERFACE,
  TH_NODE_CONVERTER,
  TH_NODE_GROUP,
  TH_NODE_FRAME,
  TH_NODE_MATTE,
  TH_NODE_DISTORT,
  TH_NODE_GEOMETRY,
  TH_NODE_ATTRIBUTE,

  TH_NODE_ZONE_SIMULATION,
  TH_NODE_ZONE_REPEAT,
  TH_NODE_ZONE_FOREACH_GEOMETRY_ELEMENT,
  TH_NODE_ZONE_CLOSURE,
  TH_SIMULATED_FRAMES,

  TH_CONSOLE_OUTPUT,
  TH_CONSOLE_INPUT,
  TH_CONSOLE_INFO,
  TH_CONSOLE_ERROR,
  TH_CONSOLE_CURSOR,
  TH_CONSOLE_SELECT,

  TH_SEQ_MOVIE,
  TH_SEQ_MOVIECLIP,
  TH_SEQ_MASK,
  TH_SEQ_IMAGE,
  TH_SEQ_SCENE,
  TH_SEQ_AUDIO,
  TH_SEQ_EFFECT,
  TH_SEQ_TRANSITION,
  TH_SEQ_META,
  TH_SEQ_TEXT,
  TH_SEQ_PREVIEW,
  TH_SEQ_COLOR,
  TH_SEQ_ACTIVE,
  TH_SEQ_SELECTED,
  TH_SEQ_TEXT_CURSOR,
  TH_SEQ_SELECTED_TEXT,

  TH_EDITMESH_ACTIVE,

  TH_HANDLE_VERTEX,
  TH_HANDLE_VERTEX_SELECT,
  TH_HANDLE_VERTEX_SIZE,

  TH_GP_VERTEX,
  TH_GP_VERTEX_SELECT,
  TH_GP_VERTEX_SIZE,

  TH_DOPESHEET_CHANNELOB,
  TH_DOPESHEET_CHANNELSUBOB,
  TH_DOPESHEET_IPOLINE,
  TH_DOPESHEET_IPOCONST,
  TH_DOPESHEET_IPOOTHER,

  TH_PREVIEW_BACK,

  TH_DRAWEXTRA_EDGELEN,
  TH_DRAWEXTRA_EDGEANG,
  TH_DRAWEXTRA_FACEAREA,
  TH_DRAWEXTRA_FACEANG,

  TH_NODE_CURVING,
  TH_NODE_GRID_LEVELS,

  TH_MARKER_OUTLINE,
  TH_MARKER,
  TH_ACT_MARKER,
  TH_SEL_MARKER,
  TH_BUNDLE_SOLID,
  TH_DIS_MARKER,
  TH_PATH_BEFORE,
  TH_PATH_AFTER,
  TH_PATH_KEYFRAME_BEFORE,
  TH_PATH_KEYFRAME_AFTER,
  TH_CAMERA_PATH,
  TH_CAMERA_PASSEPARTOUT,
  TH_LOCK_MARKER,

  TH_STITCH_PREVIEW_FACE,
  TH_STITCH_PREVIEW_EDGE,
  TH_STITCH_PREVIEW_VERT,
  TH_STITCH_PREVIEW_STITCHABLE,
  TH_STITCH_PREVIEW_UNSTITCHABLE,
  TH_STITCH_PREVIEW_ACTIVE,

  TH_UV_SHADOW,

  TH_MATCH,            /* highlight color for search matches */
  TH_SELECT_HIGHLIGHT, /* highlight color for selected outliner item */
  TH_SELECT_ACTIVE,    /* highlight color for active outliner item */
  TH_SELECTED_OBJECT,  /* selected object color for outliner */
  TH_ACTIVE_OBJECT,    /* active object color for outliner */
  TH_EDITED_OBJECT,    /* edited object color for outliner */
  TH_ROW_ALTERNATE,    /* overlay on every other row */

  TH_SKIN_ROOT,

  TH_ANIM_ACTIVE,            /* active action */
  TH_ANIM_INACTIVE,          /* no active action */
  TH_ANIM_PREVIEW_RANGE,     /* preview range overlay */
  TH_ANIM_SCENE_STRIP_RANGE, /* scene strip range overlay */

  TH_ICON_SCENE,
  TH_ICON_COLLECTION,
  TH_ICON_OBJECT,
  TH_ICON_OBJECT_DATA,
  TH_ICON_MODIFIER,
  TH_ICON_SHADING,
  TH_ICON_FOLDER,
  TH_ICON_AUTOKEY,
  TH_ICON_FUND,

  TH_SCROLL_TEXT,

  TH_NLA_TWEAK,       /* 'tweaking' track in NLA */
  TH_NLA_TWEAK_DUPLI, /* error/warning flag for other strips referencing dupli strip */

  TH_NLA_TRACK,
  TH_NLA_TRANSITION,
  TH_NLA_TRANSITION_SEL,
  TH_NLA_META,
  TH_NLA_META_SEL,
  TH_NLA_SOUND,
  TH_NLA_SOUND_SEL,

  TH_WIDGET_EMBOSS,
  TH_WIDGET_TEXT_CURSOR,
  TH_WIDGET_TEXT_SELECTION,
  TH_WIDGET_TEXT_HIGHLIGHT,
  TH_EDITOR_OUTLINE,
  TH_EDITOR_OUTLINE_ACTIVE,
  TH_EDITOR_BORDER,

  TH_TRANSPARENT_CHECKER_PRIMARY,
  TH_TRANSPARENT_CHECKER_SECONDARY,
  TH_TRANSPARENT_CHECKER_SIZE,

  TH_AXIS_X, /* X/Y/Z Axis */
  TH_AXIS_Y,
  TH_AXIS_Z,

  TH_AXIS_W, /* W (quaternion and axis-angle rotations) */

  TH_GIZMO_HI,
  TH_GIZMO_PRIMARY,
  TH_GIZMO_SECONDARY,
  TH_GIZMO_VIEW_ALIGN,
  TH_GIZMO_A,
  TH_GIZMO_B,

  TH_BACKGROUND_TYPE,

  TH_INFO_SELECTED,
  TH_INFO_SELECTED_TEXT,
  TH_INFO_ERROR_TEXT,
  TH_INFO_WARNING_TEXT,
  TH_INFO_INFO_TEXT,
  TH_INFO_DEBUG,
  TH_INFO_DEBUG_TEXT,
  TH_INFO_PROPERTY,
  TH_INFO_PROPERTY_TEXT,
  TH_INFO_OPERATOR,
  TH_INFO_OPERATOR_TEXT,
  TH_VIEW_OVERLAY,

  TH_V3D_CLIPPING_BORDER,

  TH_METADATA_BG,
  TH_METADATA_TEXT,

  TH_BEVEL,
  TH_CREASE,
  TH_SEAM,
  TH_SHARP,
  TH_FREESTYLE,
};

/* Specific defines per space should have higher define values. */

struct bThemeState {
  bTheme *theme;
  int spacetype, regionid;
};

/* THE CODERS API FOR THEMES: */

/**
 * Get individual values, not scaled.
 */
float UI_GetThemeValuef(int colorid);
/**
 * Get individual values, not scaled.
 */
int UI_GetThemeValue(int colorid);

/* Versions of #UI_GetThemeValue & #UI_GetThemeValuef, which take a space-type */

float UI_GetThemeValueTypef(int colorid, int spacetype);
int UI_GetThemeValueType(int colorid, int spacetype);

/**
 * Get three color values, scaled to 0.0-1.0 range.
 */
void UI_GetThemeColor3fv(int colorid, float col[3]);
void UI_GetThemeColorBlend3ubv(int colorid1, int colorid2, float fac, unsigned char col[3]);
void UI_GetThemeColorBlend3f(int colorid1, int colorid2, float fac, float r_col[3]);
void UI_GetThemeColorBlend4f(int colorid1, int colorid2, float fac, float r_col[4]);
/**
 * Get the color, range 0.0-1.0, complete with shading offset.
 */
void UI_GetThemeColorShade3fv(int colorid, int offset, float col[3]);
void UI_GetThemeColorShade3ubv(int colorid, int offset, unsigned char col[3]);
void UI_GetThemeColorShade4ubv(int colorid, int offset, unsigned char col[4]);

/**
 * Get three color values, range 0-255,
 * complete with shading offset for the RGB components and blending.
 */
void UI_GetThemeColorBlendShade3ubv(
    int colorid1, int colorid2, float fac, int offset, unsigned char col[3]);

/**
 * Get four color values, scaled to 0.0-1.0 range.
 */
void UI_GetThemeColor4fv(int colorid, float col[4]);

/**
 * Get four color values from specified space type, scaled to 0.0-1.0 range.
 */
void UI_GetThemeColorType4fv(int colorid, int spacetype, float col[4]);

/**
 * Get four color values, range 0.0-1.0, complete with shading offset for the RGB components.
 */
void UI_GetThemeColorShade4fv(int colorid, int offset, float col[4]);
void UI_GetThemeColorShadeAlpha4fv(int colorid, int coloffset, int alphaoffset, float col[4]);

/**
 * Get four color values ranged between 0 and 255; includes the alpha channel.
 */
void UI_GetThemeColorShadeAlpha4ubv(int colorid,
                                    int coloffset,
                                    int alphaoffset,
                                    unsigned char col[4]);

/**
 * Get four color values, range 0.0-1.0,
 * complete with shading offset for the RGB components and blending.
 */
void UI_GetThemeColorBlendShade3fv(
    int colorid1, int colorid2, float fac, int offset, float col[3]);
void UI_GetThemeColorBlendShade4fv(
    int colorid1, int colorid2, float fac, int offset, float col[4]);

/**
 * Get the 3 or 4 byte values.
 */
void UI_GetThemeColor3ubv(int colorid, unsigned char col[3]);
/**
 * Get the color, in char pointer.
 */
void UI_GetThemeColor4ubv(int colorid, unsigned char col[4]);

/**
 * Get a theme color from specified space type.
 */
void UI_GetThemeColorType3fv(int colorid, int spacetype, float col[3]);
void UI_GetThemeColorType3ubv(int colorid, int spacetype, unsigned char col[3]);
void UI_GetThemeColorType4ubv(int colorid, int spacetype, unsigned char col[4]);

/**
 * Get theme color for coloring monochrome icons.
 */
bool UI_GetIconThemeColor4ubv(int colorid, unsigned char col[4]);

/**
 * Get four color values, range 0.0-1.0, blended between two other float color pointers,
 * complete with offset for the alpha component.
 */
void UI_GetColorPtrBlendAlpha4fv(
    const float cp1[4], const float cp2[4], float fac, float alphaoffset, float r_col[4]);

/**
 * Shade a 3 byte color (same as UI_GetColorPtrBlendShade3ubv with 0.0 factor).
 */
void UI_GetColorPtrShade3ubv(const unsigned char cp[3], int offset, unsigned char r_col[3]);

/**
 * Get a 3 byte color, blended and shaded between two other char color pointers.
 */
void UI_GetColorPtrBlendShade3ubv(const unsigned char cp1[3],
                                  const unsigned char cp2[3],
                                  float fac,
                                  int offset,
                                  unsigned char r_col[3]);

/**
 * Sets the font color
 * (for anything fancy use UI_GetThemeColor[Fancy] then BLF_color).
 */
void UI_FontThemeColor(int fontid, int colorid);

/**
 * Clear the frame-buffer using the input colorid.
 */
void UI_ThemeClearColor(int colorid);

/**
 * Internal (blender) usage only, for init and set active.
 */
void UI_SetTheme(int spacetype, int regionid);

/**
 * Get current theme.
 */
bTheme *UI_GetTheme();

/**
 * For the rare case we need to temp swap in a different theme (off-screen render).
 */
void UI_Theme_Store(bThemeState *theme_state);
void UI_Theme_Restore(const bThemeState *theme_state);

/**
 * Return shadow width outside menus and popups.
 */
int UI_ThemeMenuShadowWidth();

/**
 * Only for buttons in theme editor!
 */
const unsigned char *UI_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid);

void UI_make_axis_color(const unsigned char col[3], char axis, unsigned char r_col[3]);
