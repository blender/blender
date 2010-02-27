/**
 * blenkernel/DNA_userdef_types.h (mar-2001 nzc)
 *
 *	$Id$
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

#ifndef DNA_USERDEF_TYPES_H
#define DNA_USERDEF_TYPES_H

#include "DNA_listBase.h"
#include "DNA_texture_types.h"

/* themes; defines in BIF_resource.h */
struct ColorBand;

/* ************************ style definitions ******************** */

#define MAX_STYLE_NAME	64
#define MAX_FONT_NAME	256

/* default uifont_id offered by Blender */
#define UIFONT_DEFAULT	0
#define UIFONT_BITMAP	1
/* free slots */
#define UIFONT_CUSTOM1	2
#define UIFONT_CUSTOM2	3

/* default fonts to load/initalize */
/* first font is the default (index 0), others optional */
typedef struct uiFont {
	struct uiFont *next, *prev;
	char filename[256];
	short blf_id;		/* from blfont lib */
	short uifont_id;	/* own id */
	short r_to_l;		/* fonts that read from left to right */
	short pad;
	
} uiFont;

/* this state defines appearance of text */
typedef struct uiFontStyle {
	short uifont_id;		/* saved in file, 0 is default */
	short points;			/* actual size depends on 'global' dpi */
	short kerning;			/* unfitted or default kerning value. */
	char pad[6];
	short italic, bold;		/* style hint */
	short shadow;			/* value is amount of pixels blur */
	short shadx, shady;		/* shadow offset in pixels */
	short align;			/* text align hint */
	float shadowalpha;		/* total alpha */
	float shadowcolor;		/* 1 value, typically white or black anyway */
	
} uiFontStyle;

/* uiFontStyle->align */
#define UI_STYLE_TEXT_LEFT		0
#define UI_STYLE_TEXT_CENTER	1
#define UI_STYLE_TEXT_RIGHT		2


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

	short pad[1];
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
	short pad;
} uiWidgetColors;

typedef struct uiWidgetStateColors {
	char inner_anim[4];
	char inner_anim_sel[4];
	char inner_key[4];
	char inner_key_sel[4];
	char inner_driven[4];
	char inner_driven_sel[4];
	float blend, pad;
} uiWidgetStateColors;

typedef struct ThemeUI {
	
	/* Interface Elements (buttons, menus, icons) */
	uiWidgetColors wcol_regular, wcol_tool, wcol_text;
	uiWidgetColors wcol_radio, wcol_option, wcol_toggle;
	uiWidgetColors wcol_num, wcol_numslider;
	uiWidgetColors wcol_menu, wcol_pulldown, wcol_menu_back, wcol_menu_item;
	uiWidgetColors wcol_box, wcol_scroll, wcol_list_item;

	uiWidgetStateColors wcol_state;
	
	char iconfile[80];	// FILE_MAXFILE length
	
} ThemeUI;

/* try to put them all in one, if needed a special struct can be created as well
 * for example later on, when we introduce wire colors for ob types or so...
 */
typedef struct ThemeSpace {
	/* main window colors */
	char back[4];
	char title[4];
	char text[4];	
	char text_hi[4];
	
	/* header colors */
	char header[4];
	char header_title[4];
	char header_text[4];	
	char header_text_hi[4];

	/* button/tool regions */
	char button[4];
	char button_title[4];
	char button_text[4];	
	char button_text_hi[4];
	
	/* listview regions */
	char list[4];
	char list_title[4];
	char list_text[4];	
	char list_text_hi[4];
	
	/* float panel */
	char panel[4];
	char panel_title[4];	
	char panel_text[4];	
	char panel_text_hi[4];
	
	char shade1[4];
	char shade2[4];
	
	char hilite[4];
	char grid[4]; 
	
	char wire[4], select[4];
	char lamp[4];
	char active[4], group[4], group_active[4], transform[4];
	char vertex[4], vertex_select[4];
	char edge[4], edge_select[4];
	char edge_seam[4], edge_sharp[4], edge_facesel[4];
	char face[4], face_select[4];	// solid faces
	char face_dot[4];				// selected color
	char normal[4];
	char vertex_normal[4];
	char bone_solid[4], bone_pose[4];
	char strip[4], strip_select[4];
	char cframe[4];
	char ds_channel[4], ds_subchannel[4]; // dopesheet
	
	char console_output[4], console_input[4], console_info[4], console_error[4];
	char console_cursor[4];
	
	char vertex_size, facedot_size;
	char bpad[2];

	char syntaxl[4], syntaxn[4], syntaxb[4]; // syntax for textwindow and nodes
	char syntaxv[4], syntaxc[4];
	
	char movie[4], image[4], scene[4], audio[4];		// for sequence editor
	char effect[4], plugin[4], transition[4], meta[4];
	char editmesh_active[4]; 

	char handle_vertex[4];
	char handle_vertex_select[4];
	
	char handle_vertex_size;
	char hpad[3];
	
	char preview_back[4];
	
} ThemeSpace;


/* set of colors for use as a custom color set for Objects/Bones wire drawing */
typedef struct ThemeWireColor {
	char 	solid[4];
	char	select[4];
	char 	active[4];
	
	short 	flag;
	short 	pad;
} ThemeWireColor; 

/* flags for ThemeWireColor */
#define TH_WIRECOLOR_CONSTCOLS	(1<<0)
#define TH_WIRECOLOR_TEXTCOLS	(1<<1)

/* A theme */
typedef struct bTheme {
	struct bTheme *next, *prev;
	char name[32];
	
	ThemeUI tui;
	
	/* Individual Spacetypes */
	ThemeSpace tbuts;	
	ThemeSpace tv3d;
	ThemeSpace tfile;
	ThemeSpace tipo;
	ThemeSpace tinfo;	
	ThemeSpace tsnd;
	ThemeSpace tact;
	ThemeSpace tnla;
	ThemeSpace tseq;
	ThemeSpace tima;
	ThemeSpace timasel;
	ThemeSpace text;
	ThemeSpace toops;
	ThemeSpace ttime;
	ThemeSpace tnode;
	ThemeSpace tlogic;
	ThemeSpace tuserpref;	
	ThemeSpace tconsole;
	
	/* 20 sets of bone colors for this theme */
	ThemeWireColor tarm[20];
	/*ThemeWireColor tobj[20];*/
	
	int active_theme_area, pad;
	
} bTheme;

/* for the moment only the name. may want to store options with this later */
typedef struct bAddon {
	struct bAddon *next, *prev;
	char module[64];
} bAddon;

typedef struct SolidLight {
	int flag, pad;
	float col[4], spec[4], vec[4];
} SolidLight;

typedef struct UserDef {
	int flag, dupflag;
	int savetime;
	char tempdir[160];	// FILE_MAXDIR length
	char fontdir[160];
	char renderdir[160];
	char textudir[160];
	char plugtexdir[160];
	char plugseqdir[160];
	char pythondir[160];
	char sounddir[160];
	char anim_player[240];	// FILE_MAX length
	int anim_player_preset;
	
	short v2d_min_gridsize;		/* minimum spacing between gridlines in View2D grids */
	short timecode_style;		/* style of timecode display */
	
	short versions;
	short dbl_click_time;
	
	int gameflags;
	int wheellinescroll;
	int uiflag, language;
	short userpref, viewzoom;
	
	int mixbufsize;
	int audiodevice;
	int audiorate;
	int audioformat;
	int audiochannels;

	int scrollback; /* console scrollback limit */
	int dpi;		/* range 48-128? */
	short encoding;
	short transopts;
	short menuthreshold1, menuthreshold2;
	
	struct ListBase themes;
	struct ListBase uifonts;
	struct ListBase uistyles;
	struct ListBase keymaps;
	struct ListBase addons;
	char keyconfigstr[64];
	
	short undosteps;
	short undomemory;
	short gp_manhattendist, gp_euclideandist, gp_eraser;
	short gp_settings;
	short tb_leftmouse, tb_rightmouse;
	struct SolidLight light[3];
	short tw_hotspot, tw_flag, tw_handlesize, tw_size;
	short textimeout,texcollectrate;
	short wmdrawmethod, wmpad;
	int memcachelimit;
	int prefetchframes;
	short frameserverport;
	short pad_rot_angle;	/*control the rotation step of the view when PAD2,PAD4,PAD6&PAD8 is use*/
	short obcenter_dia;
	short rvisize;			/* rotating view icon size */
	short rvibright;		/* rotating view icon brightness */
	short recent_files;		/* maximum number of recently used files to remember  */
	short smooth_viewtx;	/* miliseconds to spend spinning the view */
	short glreslimit;
	short ndof_pan, ndof_rotate;
	short curssize, ipo_new;
	short color_picker_type;
	short pad2;

	short scrcastfps;		/* frame rate for screencast to be played back */
	short scrcastwait;		/* milliseconds between screencast snapshots */

	char versemaster[160];
	char verseuser[160];
	float glalphaclip;
	
	short autokey_mode;		/* autokeying mode */
	short autokey_flag;		/* flags for autokeying */

	struct ColorBand coba_weight;	/* from texture.h */
} UserDef;

extern UserDef U; /* from blenkernel blender.c */

/* ***************** USERDEF ****************** */

/* userpref/section */
#define USER_SECTION_INTERFACE	0
#define USER_SECTION_EDIT		1
#define USER_SECTION_FILE		2
#define USER_SECTION_SYSTEM		3
#define USER_SECTION_THEME		4
#define USER_SECTION_INPUT		5
#define USER_SECTION_ADDONS 	6

/* flag */
#define USER_AUTOSAVE			(1 << 0)
#define USER_AUTOGRABGRID		(1 << 1)
#define USER_AUTOROTGRID		(1 << 2)
#define USER_AUTOSIZEGRID		(1 << 3)
#define USER_SCENEGLOBAL		(1 << 4)
#define USER_TRACKBALL			(1 << 5)
#define USER_DUPLILINK			(1 << 6)
#define USER_FSCOLLUM			(1 << 7)
#define USER_MAT_ON_OB			(1 << 8)
/*#define USER_NO_CAPSLOCK		(1 << 9)*/ /* not used anywhere */
#define USER_VIEWMOVE			(1 << 10)
#define USER_TOOLTIPS			(1 << 11)
#define USER_TWOBUTTONMOUSE		(1 << 12)
#define USER_NONUMPAD			(1 << 13)
#define USER_LMOUSESELECT		(1 << 14)
#define USER_FILECOMPRESS		(1 << 15)
#define USER_SAVE_PREVIEWS		(1 << 16)
#define USER_CUSTOM_RANGE		(1 << 17)
#define USER_ADD_EDITMODE		(1 << 18)
#define USER_ADD_VIEWALIGNED	(1 << 19)
#define USER_RELPATHS			(1 << 20)
#define USER_DRAGIMMEDIATE		(1 << 21)
#define USER_SCRIPT_AUTOEXEC_DISABLE	(1 << 22)
#define USER_FILENOUI			(1 << 23)
#define USER_NONEGFRAMES		(1 << 24)

/* helper macro for checking frame clamping */
#define FRAMENUMBER_MIN_CLAMP(cfra) \
	{ \
		if ((U.flag & USER_NONEGFRAMES) && (cfra < 0)) \
			cfra = 0; \
	}

/* viewzom */
#define USER_ZOOM_CONT			0
#define USER_ZOOM_SCALE			1
#define USER_ZOOM_DOLLY			2

/* uiflag */
// old flag for #define	USER_KEYINSERTACT		(1 << 0)
// old flag for #define	USER_KEYINSERTOBJ		(1 << 1)
#define USER_WHEELZOOMDIR		(1 << 2)
#define USER_FILTERFILEEXTS		(1 << 3)
#define USER_DRAWVIEWINFO		(1 << 4)
#define USER_PLAINMENUS			(1 << 5)		// old EVTTOCONSOLE print ghost events, here for tuhopuu compat. --phase
								// old flag for hide pulldown was here 
#define USER_FLIPFULLSCREEN		(1 << 7)
#define USER_ALLWINCODECS		(1 << 8)
#define USER_MENUOPENAUTO		(1 << 9)
#define USER_PANELPINNED		(1 << 10)
#define USER_AUTOPERSP     		(1 << 11)
#define USER_LOCKAROUND     	(1 << 12)
#define USER_GLOBALUNDO     	(1 << 13)
#define USER_ORBIT_SELECTION	(1 << 14)
// old flag for #define USER_KEYINSERTAVAI		(1 << 15)
#define USER_ORBIT_ZBUF			(1 << 15)
#define USER_HIDE_DOT			(1 << 16)
#define USER_SHOW_ROTVIEWICON	(1 << 17)
#define USER_SHOW_VIEWPORTNAME	(1 << 18)
// old flag for #define USER_KEYINSERTNEED		(1 << 19)
#define USER_ZOOM_TO_MOUSEPOS	(1 << 20)
#define USER_SHOW_FPS			(1 << 21)
#define USER_MMB_PASTE			(1 << 22)
#define USER_MENUFIXEDORDER		(1 << 23)
#define USER_CONTINUOUS_MOUSE	(1 << 24)
#define USER_ZOOM_INVERT		(1 << 25)
#define USER_ZOOM_DOLLY_HORIZ	(1 << 26)

/* Auto-Keying mode */
	/* AUTOKEY_ON is a bitflag */
#define 	AUTOKEY_ON				1
	/* AUTOKEY_ON + 2**n...  (i.e. AUTOKEY_MODE_NORMAL = AUTOKEY_ON + 2) to preserve setting, even when autokey turned off  */
#define		AUTOKEY_MODE_NORMAL		3
#define		AUTOKEY_MODE_EDITKEYS	5

/* Auto-Keying flag
 * U.autokey_flag (not strictly used when autokeying only - is also used when keyframing these days)
 * note: AUTOKEY_FLAG_* is used with a macro, search for lines like IS_AUTOKEY_FLAG(INSERTAVAIL)
 */
#define		AUTOKEY_FLAG_INSERTAVAIL	(1<<0)
#define		AUTOKEY_FLAG_INSERTNEEDED	(1<<1)
#define		AUTOKEY_FLAG_AUTOMATKEY		(1<<2)
#define		AUTOKEY_FLAG_XYZ2RGB		(1<<3)
	/* U.autokey_flag (strictly autokeying only) */
#define 	AUTOKEY_FLAG_ONLYKEYINGSET	(1<<6)
	/* toolsettings->autokey_flag */
#define 	ANIMRECORD_FLAG_WITHNLA		(1<<10)

/* transopts */
#define	USER_TR_TOOLTIPS		(1 << 0)
#define	USER_TR_BUTTONS			(1 << 1)
#define USER_TR_MENUS			(1 << 2)
#define USER_TR_FILESELECT		(1 << 3)
#define USER_TR_TEXTEDIT		(1 << 4)
#define USER_DOTRANSLATE		(1 << 5)
#define USER_USETEXTUREFONT		(1 << 6)
#define CONVERT_TO_UTF8			(1 << 7)

/* dupflag */
#define USER_DUP_MESH			(1 << 0)
#define USER_DUP_CURVE			(1 << 1)
#define USER_DUP_SURF			(1 << 2)
#define USER_DUP_FONT			(1 << 3)
#define USER_DUP_MBALL			(1 << 4)
#define USER_DUP_LAMP			(1 << 5)
#define USER_DUP_IPO			(1 << 6)
#define USER_DUP_MAT			(1 << 7)
#define USER_DUP_TEX			(1 << 8)
#define	USER_DUP_ARM			(1 << 9)
#define	USER_DUP_ACT			(1 << 10)
#define	USER_DUP_PSYS			(1 << 11)

/* gameflags */
#define USER_DEPRECATED_FLAG	1
// #define USER_DISABLE_SOUND		2 deprecated, don't use without checking for
// backwards compatibilty in do_versions!
#define USER_DISABLE_MIPMAP		4
#define USER_DISABLE_VBO		8
#define USER_DISABLE_AA			16

/* wm draw method */
#define USER_DRAW_TRIPLE		0
#define USER_DRAW_OVERLAP		1
#define USER_DRAW_FULL			2
#define USER_DRAW_AUTOMATIC		3
#define USER_DRAW_OVERLAP_FLIP	4

/* tw_flag (transform widget) */

/* gp_settings (Grease Pencil Settings) */
#define GP_PAINT_DOSMOOTH		(1<<0)
#define GP_PAINT_DOSIMPLIFY		(1<<1)

/* color picker types */
#define USER_CP_CIRCLE		0
#define USER_CP_SQUARE_SV	1
#define USER_CP_SQUARE_HS	2
#define USER_CP_SQUARE_HV	3

/* timecode display styles */
	/* as little info as is necessary to show relevant info
	 * with '+' to denote the frames 
	 * i.e. HH:MM:SS+FF, MM:SS+FF, SS+FF, or MM:SS
	 */
#define USER_TIMECODE_MINIMAL		0
	/* reduced SMPTE - (HH:)MM:SS:FF */
#define USER_TIMECODE_SMPTE_MSF		1
	/* full SMPTE - HH:MM:SS:FF */
#define USER_TIMECODE_SMPTE_FULL	2
	/* milliseconds for sub-frames - HH:MM:SS.sss */
#define USER_TIMECODE_MILLISECONDS	3
	/* seconds only */
#define USER_TIMECODE_SECONDS_ONLY	4

/* theme drawtypes */
#define TH_MINIMAL  	0
#define TH_ROUNDSHADED	1
#define TH_ROUNDED  	2
#define TH_OLDSKOOL 	3
#define TH_SHADED   	4

#endif
