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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_userdef_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
 */

#ifndef __DNA_USERDEF_TYPES_H__
#define __DNA_USERDEF_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_texture_types.h" /* ColorBand */

#ifdef __cplusplus
extern "C" {
#endif

/* themes; defines in BIF_resource.h */
struct ColorBand;

/* ************************ style definitions ******************** */

#define MAX_STYLE_NAME	64

#define GPU_VIEWPORT_QUALITY_FXAA 0.10f
#define GPU_VIEWPORT_QUALITY_TAA8 0.25f
#define GPU_VIEWPORT_QUALITY_TAA16 0.6f
#define GPU_VIEWPORT_QUALITY_TAA32 0.8f

/* default offered by Blender.
 * uiFont.uifont_id */
typedef enum eUIFont_ID {
	UIFONT_DEFAULT	= 0,
/*	UIFONT_BITMAP	= 1 */ /* UNUSED */

	/* free slots */
	UIFONT_CUSTOM1	= 2,
	UIFONT_CUSTOM2	= 3
} eUIFont_ID;

/* default fonts to load/initalize */
/* first font is the default (index 0), others optional */
typedef struct uiFont {
	struct uiFont *next, *prev;
	char filename[1024];/* 1024 = FILE_MAX */
	short blf_id;		/* from blfont lib */
	short uifont_id;	/* own id (eUIFont_ID) */
	short r_to_l;		/* fonts that read from left to right */
	short hinting;
} uiFont;

/* this state defines appearance of text */
typedef struct uiFontStyle {
	short uifont_id;		/* saved in file, 0 is default */
	short points;			/* actual size depends on 'global' dpi */
	short kerning;			/* unfitted or default kerning value. */
	char word_wrap;			/* enable word-wrap when drawing */
	char pad[5];
	short italic, bold;		/* style hint */
	short shadow;			/* value is amount of pixels blur */
	short shadx, shady;		/* shadow offset in pixels */
	short align;			/* text align hint */
	float shadowalpha;		/* total alpha */
	float shadowcolor;		/* 1 value, typically white or black anyway */
} uiFontStyle;

/* uiFontStyle.align */
typedef enum eFontStyle_Align {
	UI_STYLE_TEXT_LEFT		= 0,
	UI_STYLE_TEXT_CENTER	= 1,
	UI_STYLE_TEXT_RIGHT		= 2
} eFontStyle_Align;


/* this is fed to the layout engine and widget code */

typedef struct uiStyle {
	struct uiStyle *next, *prev;

	char name[64];			/* MAX_STYLE_NAME */

	uiFontStyle paneltitle;
	uiFontStyle grouplabel;
	uiFontStyle widgetlabel;
	uiFontStyle widget;

	float panelzoom;

	short minlabelchars;	/* in characters */
	short minwidgetchars;	/* in characters */

	short columnspace;
	short templatespace;
	short boxspace;
	short buttonspacex;
	short buttonspacey;
	short panelspace;
	short panelouter;

	short pad;
} uiStyle;

typedef struct uiWidgetColors {
	char outline[4];
	char inner[4];
	char inner_sel[4];
	char item[4];
	char text[4];
	char text_sel[4];
	short shaded;
	short shadetop, shadedown;
	short alpha_check;
	float roundness;
	float pad;
} uiWidgetColors;

typedef struct uiWidgetStateColors {
	char inner_anim[4];
	char inner_anim_sel[4];
	char inner_key[4];
	char inner_key_sel[4];
	char inner_driven[4];
	char inner_driven_sel[4];
	char inner_overridden[4];
	char inner_overridden_sel[4];
	float blend, pad;
} uiWidgetStateColors;

typedef struct uiPanelColors {
	char header[4];
	char back[4];
	char sub_back[4];
	char pad2[4];
} uiPanelColors;

typedef struct uiGradientColors {
	char gradient[4];
	char high_gradient[4];
	int show_grad;
	int pad2;
} uiGradientColors;

typedef struct ThemeUI {
	/* Interface Elements (buttons, menus, icons) */
	uiWidgetColors wcol_regular, wcol_tool, wcol_toolbar_item, wcol_text;
	uiWidgetColors wcol_radio, wcol_option, wcol_toggle;
	uiWidgetColors wcol_num, wcol_numslider, wcol_tab;
	uiWidgetColors wcol_menu, wcol_pulldown, wcol_menu_back, wcol_menu_item, wcol_tooltip;
	uiWidgetColors wcol_box, wcol_scroll, wcol_progress, wcol_list_item, wcol_pie_menu;

	uiWidgetStateColors wcol_state;

	uiPanelColors panel; /* depricated, but we keep it for do_versions (2.66.1) */

	char widget_emboss[4];

	/* fac: 0 - 1 for blend factor, width in pixels */
	float menu_shadow_fac;
	short menu_shadow_width;

	char editor_outline[4];
	short pad[1];

	char iconfile[256];	// FILE_MAXFILE length
	float icon_alpha;
	float icon_saturation;
	char _pad[4];

	/* Axis Colors */
	char xaxis[4], yaxis[4], zaxis[4];

	/* Gizmo Colors. */
	char gizmo_hi[4];
	char gizmo_primary[4];
	char gizmo_secondary[4];
	char gizmo_a[4];
	char gizmo_b[4];
	char pad2[4];
} ThemeUI;

/* try to put them all in one, if needed a special struct can be created as well
 * for example later on, when we introduce wire colors for ob types or so...
 */
typedef struct ThemeSpace {
	/* main window colors */
	char back[4];
	char title[4]; 	/* panel title */
	char text[4];
	char text_hi[4];

	/* header colors */
	char header[4];			/* region background */
	char header_title[4];	/* unused */
	char header_text[4];
	char header_text_hi[4];

	/* region tabs */
	char tab_active[4];
	char tab_inactive[4];
	char tab_back[4];
	char tab_outline[4];

	/* button/tool regions */
	char button[4];			/* region background */
	char button_title[4];	/* panel title */
	char button_text[4];
	char button_text_hi[4];

	/* listview regions */
	char list[4];			/* region background */
	char list_title[4]; 	/* panel title */
	char list_text[4];
	char list_text_hi[4];

	/* float panel */
/*	char panel[4];			unused */
/*	char panel_title[4];	unused */
/*	char panel_text[4];		unused */
/*	char panel_text_hi[4];	unused */

	/* note, cannot use name 'panel' because of DNA mapping old files */
	uiPanelColors panelcolors;

	uiGradientColors gradients;

	char shade1[4];
	char shade2[4];

	char hilite[4];
	char grid[4];

	char view_overlay[4];

	char wire[4], wire_edit[4], select[4];
	char lamp[4], speaker[4], empty[4], camera[4];
	char active[4], group[4], group_active[4], transform[4];
	char vertex[4], vertex_select[4], vertex_bevel[4], vertex_unreferenced[4];
	char edge[4], edge_select[4];
	char edge_seam[4], edge_sharp[4], edge_facesel[4], edge_crease[4], edge_bevel[4];
	char face[4], face_select[4];	/* solid faces */
	char face_dot[4];				/*  selected color */
	char extra_edge_len[4], extra_edge_angle[4], extra_face_angle[4], extra_face_area[4];
	char normal[4];
	char vertex_normal[4];
	char loop_normal[4];
	char bone_solid[4], bone_pose[4], bone_pose_active[4];
	char strip[4], strip_select[4];
	char cframe[4];
	char time_keyframe[4], time_gp_keyframe[4];
	char freestyle_edge_mark[4], freestyle_face_mark[4];

	char nurb_uline[4], nurb_vline[4];
	char act_spline[4], nurb_sel_uline[4], nurb_sel_vline[4], lastsel_point[4];

	char handle_free[4], handle_auto[4], handle_vect[4], handle_align[4], handle_auto_clamped[4];
	char handle_sel_free[4], handle_sel_auto[4], handle_sel_vect[4], handle_sel_align[4], handle_sel_auto_clamped[4];

	char ds_channel[4], ds_subchannel[4]; /* dopesheet */
	char keytype_keyframe[4], keytype_extreme[4], keytype_breakdown[4], keytype_jitter[4]; /* keytypes */
	char keytype_keyframe_select[4], keytype_extreme_select[4], keytype_breakdown_select[4], keytype_jitter_select[4]; /* keytypes */
	char keyborder[4], keyborder_select[4];

	char console_output[4], console_input[4], console_info[4], console_error[4];
	char console_cursor[4], console_select[4];

	char vertex_size, outline_width, facedot_size;
	char noodle_curving;

	/* syntax for textwindow and nodes */
	char syntaxl[4], syntaxs[4]; // in nodespace used for backdrop matte
	char syntaxb[4], syntaxn[4]; // in nodespace used for color input
	char syntaxv[4], syntaxc[4]; // in nodespace used for converter group
	char syntaxd[4], syntaxr[4]; // in nodespace used for distort

	char nodeclass_output[4], nodeclass_filter[4];
	char nodeclass_vector[4], nodeclass_texture[4];
	char nodeclass_shader[4], nodeclass_script[4];
	char nodeclass_pattern[4], nodeclass_layout[4];

	char movie[4], movieclip[4], mask[4], image[4], scene[4], audio[4];		/* for sequence editor */
	char effect[4], transition[4], meta[4], text_strip[4];

	float keyframe_scale_fac; /* for dopesheet - scale factor for size of keyframes (i.e. height of channels) */

	char editmesh_active[4];

	char handle_vertex[4];
	char handle_vertex_select[4];

	char handle_vertex_size;

	char clipping_border_3d[4];

	char marker_outline[4], marker[4], act_marker[4], sel_marker[4], dis_marker[4], lock_marker[4];
	char bundle_solid[4];
	char path_before[4], path_after[4];
	char camera_path[4];
	char hpad[2];

	char gp_vertex_size;
	char gp_vertex[4], gp_vertex_select[4];

	char preview_back[4];
	char preview_stitch_face[4];
	char preview_stitch_edge[4];
	char preview_stitch_vert[4];
	char preview_stitch_stitchable[4];
	char preview_stitch_unstitchable[4];
	char preview_stitch_active[4];

	char uv_shadow[4]; /* two uses, for uvs with modifier applied on mesh and uvs during painting */
	char uv_others[4]; /* uvs of other objects */

	char match[4];				/* outliner - filter match */
	char selected_highlight[4];	/* outliner - selected item */

	char skin_root[4]; /* Skin modifier root color */

	/* NLA */
	char anim_active[4];	 /* Active Action + Summary Channel */
	char anim_non_active[4]; /* Active Action = NULL */

	char nla_tweaking[4];   /* NLA 'Tweaking' action/strip */
	char nla_tweakdupli[4]; /* NLA - warning color for duplicate instances of tweaking strip */

	char nla_transition[4], nla_transition_sel[4]; /* NLA "Transition" strips */
	char nla_meta[4], nla_meta_sel[4];             /* NLA "Meta" strips */
	char nla_sound[4], nla_sound_sel[4];           /* NLA "Sound" strips */

	/* info */
	char info_selected[4], info_selected_text[4];
	char info_error[4], info_error_text[4];
	char info_warning[4], info_warning_text[4];
	char info_info[4], info_info_text[4];
	char info_debug[4], info_debug_text[4];

	char paint_curve_pivot[4];
	char paint_curve_handle[4];

	char metadatabg[4];
	char metadatatext[4];
} ThemeSpace;


/* set of colors for use as a custom color set for Objects/Bones wire drawing */
typedef struct ThemeWireColor {
	char 	solid[4];
	char	select[4];
	char 	active[4];

	short 	flag;  /* eWireColor_Flags */
	short 	pad;
} ThemeWireColor;

/* ThemeWireColor.flag */
typedef enum eWireColor_Flags {
	TH_WIRECOLOR_CONSTCOLS	= (1 << 0),
	TH_WIRECOLOR_TEXTCOLS	= (1 << 1),
} eWireColor_Flags;

/* A theme */
typedef struct bTheme {
	struct bTheme *next, *prev;
	char name[32];

	ThemeUI tui;

	/* Individual Spacetypes */
	/* note: ensure UI_THEMESPACE_END is updated when adding */
	ThemeSpace tbuts;
	ThemeSpace tv3d;
	ThemeSpace tfile;
	ThemeSpace tipo;
	ThemeSpace tinfo;
	ThemeSpace tact;
	ThemeSpace tnla;
	ThemeSpace tseq;
	ThemeSpace tima;
	ThemeSpace text;
	ThemeSpace toops;
	ThemeSpace ttime;
	ThemeSpace tnode;
	ThemeSpace tuserpref;
	ThemeSpace tconsole;
	ThemeSpace tclip;
	ThemeSpace ttopbar;
	ThemeSpace tstatusbar;

	/* 20 sets of bone colors for this theme */
	ThemeWireColor tarm[20];
	/*ThemeWireColor tobj[20];*/

	int active_theme_area, pad;
} bTheme;

#define UI_THEMESPACE_START(btheme)  (CHECK_TYPE_INLINE(btheme, bTheme *),  &((btheme)->tbuts))
#define UI_THEMESPACE_END(btheme)    (CHECK_TYPE_INLINE(btheme, bTheme *), (&((btheme)->tclip) + 1))

typedef struct bAddon {
	struct bAddon *next, *prev;
	char module[64];
	IDProperty *prop;  /* User-Defined Properties on this  Addon (for storing preferences) */
} bAddon;

typedef struct bPathCompare {
	struct bPathCompare *next, *prev;
	char path[768];  /* FILE_MAXDIR */
	char flag, pad[7];
} bPathCompare;

typedef struct bUserMenu {
	struct bUserMenu *next, *prev;
	char space_type;
	char _pad0[7];
	char context[64];
	/* bUserMenuItem */
	ListBase items;
} bUserMenu;

/* May be part of bUserMenu or other list. */
typedef struct bUserMenuItem {
	struct bUserMenuItem *next, *prev;
	char ui_name[64];
	char type;
	char _pad0[7];
} bUserMenuItem;

typedef struct bUserMenuItem_Op {
	bUserMenuItem item;
	char op_idname[64];
	struct IDProperty *prop;
	char opcontext;
	char _pad0[7];
} bUserMenuItem_Op;

typedef struct bUserMenuItem_Menu {
	bUserMenuItem item;
	char mt_idname[64];
} bUserMenuItem_Menu;

typedef struct bUserMenuItem_Prop {
	bUserMenuItem item;
	char context_data_path[256];
	char prop_id[64];
	int  prop_index;
	char _pad0[4];
} bUserMenuItem_Prop;

enum {
	USER_MENU_TYPE_SEP = 1,
	USER_MENU_TYPE_OPERATOR = 2,
	USER_MENU_TYPE_MENU = 3,
	USER_MENU_TYPE_PROP = 4,
};

typedef struct SolidLight {
	int flag, pad;
	float col[4], spec[4], vec[4];
} SolidLight;

typedef struct WalkNavigation {
	float mouse_speed;  /* speed factor for look around */
	float walk_speed;
	float walk_speed_factor;
	float view_height;
	float jump_height;
	float teleport_time;  /* duration to use for teleporting */
	short flag;
	short pad[3];
} WalkNavigation;

typedef struct UserDef {
	/* UserDef has separate do-version handling, and can be read from other files */
	int versionfile, subversionfile;

	int flag;  /* eUserPref_Flag */
	int dupflag;  /* eDupli_ID_Flags */
	int savetime;
	char tempdir[768];	/* FILE_MAXDIR length */
	char fontdir[768];
	char renderdir[1024]; /* FILE_MAX length */
	/* EXR cache path */
	char render_cachedir[768];  /* 768 = FILE_MAXDIR */
	char textudir[768];
	char pythondir[768];
	char sounddir[768];
	char i18ndir[768];
	char image_editor[1024];    /* 1024 = FILE_MAX */
	char anim_player[1024];	    /* 1024 = FILE_MAX */
	int anim_player_preset;

	short v2d_min_gridsize;		/* minimum spacing between gridlines in View2D grids */
	short timecode_style;		/* eTimecodeStyles, style of timecode display */

	short versions;
	short dbl_click_time;

	short pad;
	short wheellinescroll;
	int uiflag;   /* eUserpref_UI_Flag */
	int uiflag2;  /* eUserpref_UI_Flag2 */
	/* Experimental flag for app-templates to make changes to behavior
	 * which are outside the scope of typical preferences. */
	short app_flag;
	short language;
	short userpref, viewzoom;

	int mixbufsize;
	int audiodevice;
	int audiorate;
	int audioformat;
	int audiochannels;

	float ui_scale;     /* setting for UI scale */
	int ui_line_width;  /* setting for UI line width */
	int dpi;            /* runtime, full DPI divided by pixelsize */
	float dpi_fac;      /* runtime, multiplier to scale UI elements based on DPI */
	float pixelsize;	/* runtime, line width and point size based on DPI */
	int virtual_pixel;	/* deprecated, for forward compatibility */

	int scrollback;     /* console scrollback limit */
	char node_margin;   /* node insert offset (aka auto-offset) margin, but might be useful for later stuff as well */
	char pad2[5];
	short transopts;    /* eUserpref_Translation_Flags */
	short menuthreshold1, menuthreshold2;

	/* startup template */
	char app_template[64];

	struct ListBase themes;
	struct ListBase uifonts;
	struct ListBase uistyles;
	struct ListBase keymaps  DNA_DEPRECATED; /* deprecated in favor of user_keymaps */
	struct ListBase user_keymaps;
	struct ListBase addons;
	struct ListBase autoexec_paths;
	struct ListBase user_menus; /* bUserMenu */

	char keyconfigstr[64];

	short undosteps;
	short pad1;
	int undomemory;
	float gpu_viewport_quality;
	short gp_manhattendist, gp_euclideandist, gp_eraser;
	short gp_settings;  /* eGP_UserdefSettings */
	short tb_leftmouse, tb_rightmouse;
	struct SolidLight light[3];
	short gizmo_flag, gizmo_size;
	short pad6[3];
	short textimeout, texcollectrate;
	short dragthreshold;
	int memcachelimit;
	int prefetchframes;
	float pad_rot_angle; /* control the rotation step of the view when PAD2, PAD4, PAD6&PAD8 is use */
	short _pad0;
	short obcenter_dia;
	short rvisize;			/* rotating view icon size */
	short rvibright;		/* rotating view icon brightness */
	short recent_files;		/* maximum number of recently used files to remember  */
	short smooth_viewtx;	/* miliseconds to spend spinning the view */
	short glreslimit;
	short curssize;
	short color_picker_type;  /* eColorPicker_Types */
	char  ipo_new;			/* interpolation mode for newly added F-Curves */
	char  keyhandles_new;	/* handle types for newly added keyframes */
	char  gpu_select_method;
	char  gpu_select_pick_deph;
	char  pad0;
	char  view_frame_type;  /* eZoomFrame_Mode */

	int view_frame_keyframes; /* number of keyframes to zoom around current frame */
	float view_frame_seconds; /* seconds to zoom around current frame */

	char _pad1[4];

	short widget_unit;		/* private, defaults to 20 for 72 DPI setting */
	short anisotropic_filter;
	short use_16bit_textures, use_gpu_mipmap;

	float ndof_sensitivity;	/* overall sensitivity of 3D mouse */
	float ndof_orbit_sensitivity;
	float ndof_deadzone; /* deadzone of 3D mouse */
	int ndof_flag;			/* eNdof_Flag, flags for 3D mouse */

	short ogl_multisamples;	/* eMultiSample_Type, amount of samples for OpenGL FSA, if zero no FSA */

	/* eImageDrawMethod, Method to be used to draw the images (AUTO, GLSL, Textures or DrawPixels) */
	short image_draw_method;

	float glalphaclip;

	short autokey_mode;		/* eAutokey_Mode, autokeying mode */
	short autokey_flag;		/* flags for autokeying */

	short text_render, pad9;		/* options for text rendering */

	struct ColorBand coba_weight;	/* from texture.h */

	float sculpt_paint_overlay_col[3];
	float gpencil_new_layer_col[4]; /* default color for newly created Grease Pencil layers */

	short tweak_threshold;
	char navigation_mode, pad10;

	char author[80];	/* author name for file formats supporting it */

	char font_path_ui[1024];
	char font_path_ui_mono[1024];

	int compute_device_type;
	int compute_device_id;

	float fcu_inactive_alpha;	/* opacity of inactive F-Curves in F-Curve Editor */

	short pie_interaction_type;     /* if keeping a pie menu spawn button pressed after this time, it turns into
	                             * a drag/release pie menu */
	short pie_initial_timeout;  /* direction in the pie menu will always be calculated from the initial position
	                             * within this time limit */
	short pie_animation_timeout;
	short pie_menu_confirm;
	short pie_menu_radius;        /* pie menu radius */
	short pie_menu_threshold;     /* pie menu distance from center before a direction is set */

	struct WalkNavigation walk_navigation;

	short opensubdiv_compute_type;
	char pad5[6];
} UserDef;

extern UserDef U; /* from blenkernel blender.c */

/* ***************** USERDEF ****************** */

/* UserDef.userpref (UI active_section) */
typedef enum eUserPref_Section {
	USER_SECTION_INTERFACE	= 0,
	USER_SECTION_EDIT		= 1,
	USER_SECTION_FILE		= 2,
	USER_SECTION_SYSTEM		= 3,
	USER_SECTION_THEME		= 4,
	USER_SECTION_INPUT		= 5,
	USER_SECTION_ADDONS 	= 6,
	USER_SECTION_LIGHT 	= 7,
} eUserPref_Section;

/* UserDef.flag */
typedef enum eUserPref_Flag {
	USER_AUTOSAVE			= (1 << 0),
	USER_FLAG_NUMINPUT_ADVANCED = (1 << 1),
	USER_FLAG_DEPRECATED_2	= (1 << 2),  /* cleared */
	USER_FLAG_DEPRECATED_3	= (1 << 3),  /* cleared */
/*	USER_SCENEGLOBAL         = (1 << 4), deprecated */
	USER_TRACKBALL			= (1 << 5),
	USER_FLAG_DEPRECATED_6	= (1 << 6),  /* cleared */
	USER_FLAG_DEPRECATED_7	= (1 << 7),  /* cleared */
	USER_MAT_ON_OB			= (1 << 8),
	USER_FLAG_DEPRECATED_9	= (1 << 9),   /* cleared */
	USER_DEVELOPER_UI		= (1 << 10),
	USER_TOOLTIPS			= (1 << 11),
	USER_TWOBUTTONMOUSE		= (1 << 12),
	USER_NONUMPAD			= (1 << 13),
	USER_LMOUSESELECT		= (1 << 14),
	USER_FILECOMPRESS		= (1 << 15),
	USER_SAVE_PREVIEWS		= (1 << 16),
	USER_CUSTOM_RANGE		= (1 << 17),
	USER_ADD_EDITMODE		= (1 << 18),
	USER_ADD_VIEWALIGNED	= (1 << 19),
	USER_RELPATHS			= (1 << 20),
	USER_RELEASECONFIRM		= (1 << 21),
	USER_SCRIPT_AUTOEXEC_DISABLE	= (1 << 22),
	USER_FILENOUI			= (1 << 23),
	USER_NONEGFRAMES		= (1 << 24),
	USER_TXT_TABSTOSPACES_DISABLE	= (1 << 25),
	USER_TOOLTIPS_PYTHON    = (1 << 26),
} eUserPref_Flag;

/* bPathCompare.flag */
typedef enum ePathCompare_Flag {
	USER_PATHCMP_GLOB		= (1 << 0),
} ePathCompare_Flag;

/* helper macro for checking frame clamping */
#define FRAMENUMBER_MIN_CLAMP(cfra)  {                                        \
	if ((U.flag & USER_NONEGFRAMES) && (cfra < 0))                            \
		cfra = 0;                                                             \
	} (void)0

/* UserDef.viewzoom */
typedef enum eViewZoom_Style {
	USER_ZOOM_CONT			= 0,
	USER_ZOOM_SCALE			= 1,
	USER_ZOOM_DOLLY			= 2
} eViewZoom_Style;

/* UserDef.navigation_mode */
typedef enum eViewNavigation_Method {
	VIEW_NAVIGATION_WALK = 0,
	VIEW_NAVIGATION_FLY  = 1,
} eViewNavigation_Method;

/* UserDef.flag */
typedef enum eWalkNavigation_Flag {
	USER_WALK_GRAVITY			= (1 << 0),
	USER_WALK_MOUSE_REVERSE		= (1 << 1),
} eWalkNavigation_Flag;

/* UserDef.uiflag */
typedef enum eUserpref_UI_Flag {
	/* flags 0 and 1 were old flags (for autokeying) that aren't used anymore */
	USER_WHEELZOOMDIR		= (1 << 2),
	USER_FILTERFILEEXTS		= (1 << 3),
	USER_DRAWVIEWINFO		= (1 << 4),
	USER_PLAINMENUS			= (1 << 5),
	USER_LOCK_CURSOR_ADJUST	= (1 << 6),
	/* Avoid accidentally adjusting the layout
	 * (exact behavior may change based on whats considered reasonable to lock down). */
	USER_UIFLAG_DEPRECATED_7 = (1 << 7),
	USER_ALLWINCODECS		= (1 << 8),
	USER_MENUOPENAUTO		= (1 << 9),
	USER_DEPTH_CURSOR		= (1 << 10),
	USER_AUTOPERSP     		= (1 << 11),
	/* USER_LOCKAROUND     	= (1 << 12), */  /* DEPRECATED */
	USER_GLOBALUNDO     	= (1 << 13),
	USER_ORBIT_SELECTION	= (1 << 14),
	USER_DEPTH_NAVIGATE     = (1 << 15),
	USER_HIDE_DOT			= (1 << 16),
	USER_SHOW_GIZMO_AXIS	= (1 << 17),
	USER_SHOW_VIEWPORTNAME	= (1 << 18),
	USER_CAM_LOCK_NO_PARENT	= (1 << 19),
	USER_ZOOM_TO_MOUSEPOS	= (1 << 20),
	USER_SHOW_FPS			= (1 << 21),
	USER_MMB_PASTE			= (1 << 22),
	USER_MENUFIXEDORDER		= (1 << 23),
	USER_CONTINUOUS_MOUSE	= (1 << 24),
	USER_ZOOM_INVERT		= (1 << 25),
	USER_ZOOM_HORIZ			= (1 << 26), /* for CONTINUE and DOLLY zoom */
	USER_SPLASH_DISABLE		= (1 << 27),
	USER_HIDE_RECENT		= (1 << 28),
	USER_SHOW_THUMBNAILS	= (1 << 29),
	USER_QUIT_PROMPT		= (1 << 30),
	USER_HIDE_SYSTEM_BOOKMARKS = (1u << 31)
} eUserpref_UI_Flag;

/* UserDef.uiflag2 */
typedef enum eUserpref_UI_Flag2 {
	USER_KEEP_SESSION			= (1 << 0),
	USER_REGION_OVERLAP			= (1 << 1),
	USER_TRACKPAD_NATURAL		= (1 << 2),
} eUserpref_UI_Flag2;

/* UserDef.app_flag */
typedef enum eUserpref_APP_Flag {
	USER_APP_LOCK_UI_LAYOUT = (1 << 0),
	USER_APP_VIEW3D_HIDE_CURSOR = (1 << 1),
} eUserpref_APP_Flag;

/* Auto-Keying mode.
 * UserDef.autokey_mode */
typedef enum eAutokey_Mode {
	/* AUTOKEY_ON is a bitflag */
	AUTOKEY_ON             = 1,

	/* AUTOKEY_ON + 2**n...  (i.e. AUTOKEY_MODE_NORMAL = AUTOKEY_ON + 2) to preserve setting, even when autokey turned off  */
	AUTOKEY_MODE_NORMAL    = 3,
	AUTOKEY_MODE_EDITKEYS  = 5
} eAutokey_Mode;

/* Zoom to frame mode.
 * UserDef.view_frame_type */
typedef enum eZoomFrame_Mode {
	ZOOM_FRAME_MODE_KEEP_RANGE = 0,
	ZOOM_FRAME_MODE_SECONDS = 1,
	ZOOM_FRAME_MODE_KEYFRAMES = 2
} eZoomFrame_Mode;

/* Auto-Keying flag
 * U.autokey_flag (not strictly used when autokeying only - is also used when keyframing these days)
 * note: AUTOKEY_FLAG_* is used with a macro, search for lines like IS_AUTOKEY_FLAG(INSERTAVAIL)
 */
typedef enum eAutokey_Flag {
	AUTOKEY_FLAG_INSERTAVAIL	= (1 << 0),
	AUTOKEY_FLAG_INSERTNEEDED	= (1 << 1),
	AUTOKEY_FLAG_AUTOMATKEY		= (1 << 2),
	AUTOKEY_FLAG_XYZ2RGB		= (1 << 3),

	/* toolsettings->autokey_flag */
	AUTOKEY_FLAG_ONLYKEYINGSET	= (1 << 6),
	AUTOKEY_FLAG_NOWARNING		= (1 << 7),
	ANIMRECORD_FLAG_WITHNLA		= (1 << 10),
} eAutokey_Flag;

/* UserDef.transopts */
typedef enum eUserpref_Translation_Flags {
	USER_TR_TOOLTIPS		= (1 << 0),
	USER_TR_IFACE			= (1 << 1),
	USER_TR_DEPRECATED_2	= (1 << 2),  /* cleared */
	USER_TR_DEPRECATED_3	= (1 << 3),  /* cleared */
	USER_TR_DEPRECATED_4	= (1 << 4),  /* cleared */
	USER_DOTRANSLATE		= (1 << 5),
	USER_TR_DEPRECATED_6	= (1 << 6),  /* cleared */
	USER_TR_DEPRECATED_7	= (1 << 7),  /* cleared */
	USER_TR_NEWDATANAME		= (1 << 8),
} eUserpref_Translation_Flags;

/* UserDef.dupflag */
typedef enum eDupli_ID_Flags {
	USER_DUP_MESH			= (1 << 0),
	USER_DUP_CURVE			= (1 << 1),
	USER_DUP_SURF			= (1 << 2),
	USER_DUP_FONT			= (1 << 3),
	USER_DUP_MBALL			= (1 << 4),
	USER_DUP_LAMP			= (1 << 5),
	USER_DUP_IPO			= (1 << 6),
	USER_DUP_MAT			= (1 << 7),
	USER_DUP_TEX			= (1 << 8),
	USER_DUP_ARM			= (1 << 9),
	USER_DUP_ACT			= (1 << 10),
	USER_DUP_PSYS			= (1 << 11)
} eDupli_ID_Flags;

/* selection method for opengl gpu_select_method */
typedef enum eOpenGL_SelectOptions {
	USER_SELECT_AUTO = 0,
	USER_SELECT_USE_OCCLUSION_QUERY = 1,
	USER_SELECT_USE_SELECT_RENDERMODE = 2
} eOpenGL_SelectOptions;

/* max anti alias draw method UserDef.gpu_viewport_antialias */
typedef enum eOpenGL_AntiAliasMethod {
	USER_AA_NONE  = 0,
	USER_AA_FXAA  = 1,
	USER_AA_TAA8  = 2,
} eOpenGL_AntiAliasMethod;

/* text draw options
 * UserDef.text_render */
typedef enum eText_Draw_Options {
	USER_TEXT_DISABLE_AA	= (1 << 0),
} eText_Draw_Options;

/* tw_flag (transform widget) */

/* Grease Pencil Settings.
 * UserDef.gp_settings */
typedef enum eGP_UserdefSettings {
	GP_PAINT_DOSMOOTH		= (1 << 0),
	GP_PAINT_DOSIMPLIFY		= (1 << 1),
} eGP_UserdefSettings;

enum {
	USER_GIZMO_DRAW            = (1 << 0),
};

/* Color Picker Types.
 * UserDef.color_picker_type */
typedef enum eColorPicker_Types {
	USER_CP_CIRCLE_HSV	= 0,
	USER_CP_SQUARE_SV	= 1,
	USER_CP_SQUARE_HS	= 2,
	USER_CP_SQUARE_HV	= 3,
	USER_CP_CIRCLE_HSL	= 4,
} eColorPicker_Types;

/* timecode display styles
 * UserDef.timecode_style */
typedef enum eTimecodeStyles {
	/* as little info as is necessary to show relevant info
	 * with '+' to denote the frames
	 * i.e. HH:MM:SS+FF, MM:SS+FF, SS+FF, or MM:SS
	 */
	USER_TIMECODE_MINIMAL       = 0,

	/* reduced SMPTE - (HH:)MM:SS:FF */
	USER_TIMECODE_SMPTE_MSF     = 1,

	/* full SMPTE - HH:MM:SS:FF */
	USER_TIMECODE_SMPTE_FULL    = 2,

	/* milliseconds for sub-frames - HH:MM:SS.sss */
	USER_TIMECODE_MILLISECONDS  = 3,

	/* seconds only */
	USER_TIMECODE_SECONDS_ONLY  = 4,

	/* Private (not exposed as generic choices) options. */
	/* milliseconds for sub-frames , SubRip format- HH:MM:SS,sss */
	USER_TIMECODE_SUBRIP        = 100,
} eTimecodeStyles;

/* theme drawtypes */
/* XXX: These are probably only for the old UI engine? */
typedef enum eTheme_DrawTypes {
	TH_MINIMAL  	= 0,
	TH_ROUNDSHADED	= 1,
	TH_ROUNDED  	= 2,
	TH_OLDSKOOL 	= 3,
	TH_SHADED   	= 4
} eTheme_DrawTypes;

/* UserDef.ndof_flag (3D mouse options) */
typedef enum eNdof_Flag {
	NDOF_SHOW_GUIDE     = (1 << 0),
	NDOF_FLY_HELICOPTER = (1 << 1),
	NDOF_LOCK_HORIZON   = (1 << 2),

	/* the following might not need to be saved between sessions,
	 * but they do need to live somewhere accessible... */
	NDOF_SHOULD_PAN     = (1 << 3),
	NDOF_SHOULD_ZOOM    = (1 << 4),
	NDOF_SHOULD_ROTATE  = (1 << 5),

	/* orbit navigation modes */

	/* exposed as Orbit|Explore in the UI */
	NDOF_MODE_ORBIT      = (1 << 6),

	/* actually... users probably don't care about what the mode
	 * is called, just that it feels right */
	/* zoom is up/down if this flag is set (otherwise forward/backward) */
	NDOF_PAN_YZ_SWAP_AXIS   = (1 << 7),
	NDOF_ZOOM_INVERT        = (1 << 8),
	NDOF_ROTX_INVERT_AXIS   = (1 << 9),
	NDOF_ROTY_INVERT_AXIS   = (1 << 10),
	NDOF_ROTZ_INVERT_AXIS   = (1 << 11),
	NDOF_PANX_INVERT_AXIS   = (1 << 12),
	NDOF_PANY_INVERT_AXIS   = (1 << 13),
	NDOF_PANZ_INVERT_AXIS   = (1 << 14),
	NDOF_TURNTABLE          = (1 << 15),
} eNdof_Flag;

#define NDOF_PIXELS_PER_SECOND 600.0f

/* UserDef.ogl_multisamples */
typedef enum eMultiSample_Type {
	USER_MULTISAMPLE_NONE	= 0,
	USER_MULTISAMPLE_2	= 2,
	USER_MULTISAMPLE_4	= 4,
	USER_MULTISAMPLE_8	= 8,
	USER_MULTISAMPLE_16	= 16,
} eMultiSample_Type;

/* UserDef.image_draw_method */
typedef enum eImageDrawMethod {
	/* IMAGE_DRAW_METHOD_AUTO = 0, */ /* Currently unused */
	IMAGE_DRAW_METHOD_GLSL = 1,
	IMAGE_DRAW_METHOD_2DTEXTURE = 2,
	IMAGE_DRAW_METHOD_DRAWPIXELS = 3,
} eImageDrawMethod;

/* UserDef.virtual_pixel */
typedef enum eUserpref_VirtualPixel {
	VIRTUAL_PIXEL_NATIVE = 0,
	VIRTUAL_PIXEL_DOUBLE = 1,
} eUserpref_VirtualPixel;

typedef enum eOpensubdiv_Computee_Type {
	USER_OPENSUBDIV_COMPUTE_NONE = 0,
	USER_OPENSUBDIV_COMPUTE_CPU = 1,
	USER_OPENSUBDIV_COMPUTE_OPENMP = 2,
	USER_OPENSUBDIV_COMPUTE_OPENCL = 3,
	USER_OPENSUBDIV_COMPUTE_CUDA = 4,
	USER_OPENSUBDIV_COMPUTE_GLSL_TRANSFORM_FEEDBACK = 5,
	USER_OPENSUBDIV_COMPUTE_GLSL_COMPUTE = 6,
} eOpensubdiv_Computee_Type;

#ifdef __cplusplus
}
#endif

#endif
