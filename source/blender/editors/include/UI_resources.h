/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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

#ifndef UI_RESOURCES_H
#define UI_RESOURCES_H

/* elubie: TODO: move the typedef for icons to UI_interface_icons.h */
/* and add/replace include of UI_resources.h by UI_interface_icons.h */
#define DEF_ICON(name) name,

typedef enum {
#define BIFICONID_FIRST		(ICON_BLENDER)
	/* ui */
#include "UI_icons.h"
	BIFICONID_LAST
#define BIFNICONIDS			(BIFICONID_LAST-BIFICONID_FIRST + 1)
} BIFIconID;
#undef DEF_ICON


typedef enum {
#define BIFCOLORSHADE_FIRST     (COLORSHADE_DARK)
		COLORSHADE_DARK,
		COLORSHADE_GREY,
		COLORSHADE_MEDIUM,
		COLORSHADE_HILITE,
		COLORSHADE_LIGHT,
		COLORSHADE_WHITE
#define BIFCOLORSHADE_LAST      (COLORSHADE_WHITE)
#define BIFNCOLORSHADES         (BIFCOLORSHADE_LAST-BIFCOLORSHADE_FIRST + 1)
} BIFColorShade;

typedef enum {
#define BIFCOLORID_FIRST	(BUTGREY)
	BUTGREY = 0,
	BUTGREEN,
	BUTBLUE,
	BUTSALMON,
	MIDGREY,
	BUTPURPLE,
	BUTYELLOW,
	REDALERT,
	BUTRUST,
	BUTWHITE,
	BUTDBLUE,
	BUTPINK,
	BUTDPINK,
	BUTMACTIVE,

	BUTIPO,
	BUTAUDIO,
	BUTCAMERA,
	BUTRANDOM,
	BUTEDITOBJECT,
	BUTPROPERTY,
	BUTSCENE,
	BUTMOTION,
	BUTMESSAGE,
	BUTACTION,
	BUTCD,
	BUTGAME,
	BUTVISIBILITY,
	BUTYUCK,
	BUTSEASICK,
	BUTCHOKE,
	BUTIMPERIAL,

	BUTTEXTCOLOR,
	BUTTEXTPRESSED,
	BUTSBACKGROUND,
	
	VIEWPORTBACKCOLOR,
	VIEWPORTGRIDCOLOR,
	VIEWPORTACTIVECOLOR,
	VIEWPORTSELECTEDCOLOR,
	VIEWPORTUNSELCOLOR,
	
	EDITVERTSEL, 
	EDITVERTUNSEL, 
	EDITEDGESEL, 
	EDITEDGEUNSEL
	
#define BIFCOLORID_LAST		(EDITEDGEUNSEL)
#define BIFNCOLORIDS		(BIFCOLORID_LAST-BIFCOLORID_FIRST + 1)

} BIFColorID;

enum {
	TH_REDALERT,

	TH_THEMEUI,
// common colors among spaces
	
	TH_BACK,
	TH_TEXT,
	TH_TEXT_HI,
	TH_TITLE,
	
	TH_HEADER,
	TH_HEADERDESEL,
	TH_HEADER_TEXT,
	TH_HEADER_TEXT_HI,
	
	/* float panels */
	TH_PANEL,
	TH_PANEL_TEXT,
	TH_PANEL_TEXT_HI,
	
	TH_BUTBACK,
	TH_BUTBACK_TEXT,
	TH_BUTBACK_TEXT_HI,
	
	TH_SHADE1,
	TH_SHADE2,
	TH_HILITE,

	TH_GRID,
	TH_WIRE,
	TH_SELECT,
	TH_ACTIVE,
	TH_GROUP,
	TH_GROUP_ACTIVE,
	TH_TRANSFORM,
	TH_VERTEX,
	TH_VERTEX_SELECT,
	TH_VERTEX_SIZE,
	TH_EDGE,
	TH_EDGE_SELECT,
	TH_EDGE_SEAM,
	TH_EDGE_FACESEL,
	TH_FACE,
	TH_FACE_SELECT,
	TH_NORMAL,
	TH_VNORMAL,
	TH_FACE_DOT,
	TH_FACEDOT_SIZE,
	TH_CFRAME,
	TH_NURB_ULINE,
	TH_NURB_VLINE,
	TH_NURB_SEL_ULINE,
	TH_NURB_SEL_VLINE,

	/* this eight colors should be in one block */
	TH_HANDLE_FREE,
	TH_HANDLE_AUTO,
	TH_HANDLE_VECT,
	TH_HANDLE_ALIGN,
	TH_HANDLE_SEL_FREE,
	TH_HANDLE_SEL_AUTO,
	TH_HANDLE_SEL_VECT,
	TH_HANDLE_SEL_ALIGN,

	TH_ACTIVE_SPLINE,

	TH_SYNTAX_B,
	TH_SYNTAX_V,
	TH_SYNTAX_C,
	TH_SYNTAX_L,
	TH_SYNTAX_N,
	
	TH_BONE_SOLID,
	TH_BONE_POSE,
	
	TH_STRIP,
	TH_STRIP_SELECT,
	
	TH_LAMP,
	
	TH_NODE,
	TH_NODE_IN_OUT,
	TH_NODE_OPERATOR,
	TH_NODE_CONVERTOR,
	TH_NODE_GROUP,
	
	TH_CONSOLE_OUTPUT,
	TH_CONSOLE_INPUT,
	TH_CONSOLE_INFO,
	TH_CONSOLE_ERROR,
	TH_CONSOLE_CURSOR,
	
	TH_SEQ_MOVIE,
	TH_SEQ_IMAGE,
	TH_SEQ_SCENE,
	TH_SEQ_AUDIO,
	TH_SEQ_EFFECT,
	TH_SEQ_PLUGIN,
	TH_SEQ_TRANSITION,
	TH_SEQ_META,
	
	TH_EDGE_SHARP,
	TH_EDITMESH_ACTIVE,
	
	TH_HANDLE_VERTEX,
	TH_HANDLE_VERTEX_SELECT,
	TH_HANDLE_VERTEX_SIZE,
	
	TH_DOPESHEET_CHANNELOB,
	TH_DOPESHEET_CHANNELSUBOB,
	
	TH_PREVIEW_BACK,
	
	TH_EDGE_CREASE,
};
/* XXX WARNING: previous is saved in file, so do not change order! */

/* specific defines per space should have higher define values */

struct bTheme;
struct PointerRNA;

// THE CODERS API FOR THEMES:

// sets the color
void 	UI_ThemeColor(int colorid);

// sets the color plus alpha
void 	UI_ThemeColor4(int colorid);

// sets color plus offset for shade
void 	UI_ThemeColorShade(int colorid, int offset);

// sets color plus offset for alpha
void	UI_ThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset);

// sets color, which is blend between two theme colors
void 	UI_ThemeColorBlend(int colorid1, int colorid2, float fac);
// same, with shade offset
void    UI_ThemeColorBlendShade(int colorid1, int colorid2, float fac, int offset);
void	UI_ThemeColorBlendShadeAlpha(int colorid1, int colorid2, float fac, int offset, int alphaoffset);

// returns one value, not scaled
float 	UI_GetThemeValuef(int colorid);
int 	UI_GetThemeValue(int colorid);

// get three color values, scaled to 0.0-1.0 range
void 	UI_GetThemeColor3fv(int colorid, float *col);
// get the color, range 0.0-1.0, complete with shading offset
void 	UI_GetThemeColorShade3fv(int colorid, int offset, float *col);

// get the 3 or 4 byte values
void 	UI_GetThemeColor3ubv(int colorid, char *col);
void 	UI_GetThemeColor4ubv(int colorid, char *col);

// get a theme color from specified space type
void	UI_GetThemeColorType4ubv(int colorid, int spacetype, char *col);

// blends and shades between two color pointers
void	UI_ColorPtrBlendShade3ubv(char *cp1, char *cp2, float fac, int offset);

// get a 3 byte color, blended and shaded between two other char color pointers
void	UI_GetColorPtrBlendShade3ubv(char *cp1, char *cp2, char *col, float fac, int offset);

// internal (blender) usage only, for init and set active
void 	UI_SetTheme(int spacetype, int regionid);

/* only for buttons in theme editor! */
char 	*UI_ThemeGetColorPtr(struct bTheme *btheme, int spacetype, int colorid);
char 	*UI_ThemeColorsPup(int spacetype);

void UI_make_axis_color(char *src_col, char *dst_col, char axis);

#endif /*  UI_ICONS_H */
