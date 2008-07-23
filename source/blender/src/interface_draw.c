
/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

/* 
     a full doc with API notes can be found in bf-blender/blender/doc/interface_API.txt

 */
 

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_color_types.h"
#include "DNA_key_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"
#include "DNA_vfont_types.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_utildefines.h"

#include "datatoc.h"            /* std font */

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_keyval.h"
#include "BIF_mainqueue.h"

#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_butspace.h"
#include "BIF_language.h"

#include "BSE_view.h"

#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

#include "mydevice.h"
#include "interface.h"
#include "blendef.h"

// globals
extern float UIwinmat[4][4];


/* ************** safe rasterpos for pixmap alignment with pixels ************* */

void ui_rasterpos_safe(float x, float y, float aspect)
{
	float vals[4], remainder;
	int doit=0;
	
	glRasterPos2f(x, y);
	glGetFloatv(GL_CURRENT_RASTER_POSITION, vals);

	remainder= vals[0] - floor(vals[0]);
	if(remainder > 0.4 && remainder < 0.6) {
		if(remainder < 0.5) x -= 0.1*aspect;
		else x += 0.1*aspect;
		doit= 1;
	}
	remainder= vals[1] - floor(vals[1]);
	if(remainder > 0.4 && remainder < 0.6) {
		if(remainder < 0.5) y -= 0.1*aspect;
		else y += 0.1*aspect;
		doit= 1;
	}
	
	if(doit) glRasterPos2f(x, y);

	BIF_RasterPos(x, y);
	BIF_SetScale(aspect);
}

/* ************** generic embossed rect, for window sliders etc ************* */

void uiEmboss(float x1, float y1, float x2, float y2, int sel)
{
	
	/* below */
	if(sel) glColor3ub(200,200,200);
	else glColor3ub(50,50,50);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(sel) glColor3ub(50,50,50);
	else glColor3ub(200,200,200);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
}

/* ************** GENERIC ICON DRAW, NO THEME HERE ************* */

/* icons have been standardized... and this call draws in untransformed coordinates */
#define ICON_HEIGHT		16.0f

static void ui_draw_icon(uiBut *but, BIFIconID icon, int blend)
{
	float xs=0, ys=0, aspect, height;

	/* this icon doesn't need draw... */
	if(icon==ICON_BLANK1) return;
	
	/* we need aspect from block, for menus... these buttons are scaled in uiPositionBlock() */
	aspect= but->block->aspect;
	if(aspect != but->aspect) {
		/* prevent scaling up icon in pupmenu */
		if (aspect < 1.0f) {			
			height= ICON_HEIGHT;
			aspect = 1.0f;
			
		}
		else 
			height= ICON_HEIGHT/aspect;
	}
	else
		height= ICON_HEIGHT;
	
	if(but->flag & UI_ICON_LEFT) {
		if (but->type==BUT_TOGDUAL) {
			if (but->drawstr[0]) {
				xs= but->x1-1.0;
			} else {
				xs= (but->x1+but->x2- height)/2.0;
			}
		}
		else if (but->type==BUTM ) {
			xs= but->x1+1.0;
		}
		else if ((but->type==ICONROW) || (but->type==ICONTEXTROW)) {
			xs= but->x1+3.0;
		}
		else {
			xs= but->x1+4.0;
		}
		ys= (but->y1+but->y2- height)/2.0;
	}
	if(but->flag & UI_ICON_RIGHT) {
		xs= but->x2-17.0;
		ys= (but->y1+but->y2- height)/2.0;
	}
	if (!((but->flag & UI_ICON_RIGHT) || (but->flag & UI_ICON_LEFT))) {
		xs= (but->x1+but->x2- height)/2.0;
		ys= (but->y1+but->y2- height)/2.0;
	}

	glEnable(GL_BLEND);

	/* calculate blend color */
	if ELEM3(but->type, TOG, ROW, TOGN) {
		if(but->flag & UI_SELECT);
		else if(but->flag & UI_ACTIVE);
		else blend= -60;
	}
	BIF_icon_draw_aspect_blended(xs, ys, icon, aspect, blend);
	
	glDisable(GL_BLEND);

}


/* ************** DEFAULT THEME, SHADED BUTTONS ************* */


#define M_WHITE		BIF_ThemeColorShade(colorid, 80)

#define M_ACT_LIGHT	BIF_ThemeColorShade(colorid, 55)
#define M_LIGHT		BIF_ThemeColorShade(colorid, 45)
#define M_HILITE	BIF_ThemeColorShade(colorid, 25)
#define M_LMEDIUM	BIF_ThemeColorShade(colorid, 10)
#define M_MEDIUM	BIF_ThemeColor(colorid)
#define M_LGREY		BIF_ThemeColorShade(colorid, -20)
#define M_GREY		BIF_ThemeColorShade(colorid, -45)
#define M_DARK		BIF_ThemeColorShade(colorid, -80)

#define M_NUMTEXT				BIF_ThemeColorShade(colorid, 25)
#define M_NUMTEXT_ACT_LIGHT		BIF_ThemeColorShade(colorid, 35)

#define MM_WHITE	BIF_ThemeColorShade(TH_BUT_NEUTRAL, 120)

/* Used for the subtle sunken effect around buttons.
 * One option is to hardcode to white, with alpha, however it causes a 
 * weird 'building up' efect, so it's commented out for now.
 */
 
/*
#define MM_WHITE_OP	glColor4ub(255, 255, 255, 60)
#define MM_WHITE_TR	glColor4ub(255, 255, 255, 0)
 */

#define MM_WHITE_OP	BIF_ThemeColorShadeAlpha(TH_BACK, 55, -100)
#define MM_WHITE_TR	BIF_ThemeColorShadeAlpha(TH_BACK, 55, -255)

#define MM_LIGHT	BIF_ThemeColorShade(TH_BUT_OUTLINE, 45)
#define MM_MEDIUM	BIF_ThemeColor(TH_BUT_OUTLINE)
#define MM_GREY		BIF_ThemeColorShade(TH_BUT_OUTLINE, -45)
#define MM_DARK		BIF_ThemeColorShade(TH_BUT_OUTLINE, -80)

/* base shaded button */
static void shaded_button(float x1, float y1, float x2, float y2, float asp, int colorid, int flag, int mid)
{
	/* 'mid' arg determines whether the button is in the middle of
	 * an alignment group or not. 0 = not middle, 1 = is in the middle.
	 * Done to allow cleaner drawing
	 */
	 
	/* *** SHADED BUTTON BASE *** */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) M_MEDIUM;
		else M_LGREY;
	} else {
		if(flag & UI_ACTIVE) M_LIGHT;
		else M_HILITE;
	}

	glVertex2f(x1,y1);
	glVertex2f(x2,y1);

	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) M_LGREY;
		else M_GREY;
	} else {
		if(flag & UI_ACTIVE) M_ACT_LIGHT;
		else M_LIGHT;
	}

	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x1,(y2-(y2-y1)/3));
	glEnd();
	

	glShadeModel(GL_FLAT);
	glBegin(GL_QUADS);
	
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) M_LGREY;
		else M_GREY;
	} else {
		if(flag & UI_ACTIVE) M_ACT_LIGHT;
		else M_LIGHT;
	}
	
	glVertex2f(x1,(y2-(y2-y1)/3));
	glVertex2f(x2,(y2-(y2-y1)/3));
	glVertex2f(x2,y2);
	glVertex2f(x1,y2);

	glEnd();
	/* *** END SHADED BUTTON BASE *** */
	
	/* *** INNER OUTLINE *** */
	/* left */
	if(!(flag & UI_SELECT)) {
		glShadeModel(GL_SMOOTH);
		glBegin(GL_LINES);
		M_MEDIUM;
		glVertex2f(x1+1,y1+2);
		M_WHITE;
		glVertex2f(x1+1,y2);
		glEnd();
	}
	
	/* right */
		if(!(flag & UI_SELECT)) {
		glShadeModel(GL_SMOOTH);
		glBegin(GL_LINES);
		M_MEDIUM;
		glVertex2f(x2-1,y1+2);
		M_WHITE;
		glVertex2f(x2-1,y2);
		glEnd();
	}
	
	glShadeModel(GL_FLAT);
	
	/* top */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) M_LGREY;
		else M_GREY;
	} else {
		if(flag & UI_ACTIVE) M_WHITE;
		else M_WHITE;
	}

	fdrawline(x1, (y2-1), x2, (y2-1));
	
	/* bottom */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) M_MEDIUM;
		else M_LGREY;
	} else {
		if(flag & UI_ACTIVE) M_LMEDIUM;
		else M_MEDIUM;
	}
	fdrawline(x1, (y1+1), x2, (y1+1));
	/* *** END INNER OUTLINE *** */
	
	/* *** OUTER OUTLINE *** */
	if (mid) {
		// we draw full outline, its not AA, and it works better button mouse-over hilite
		MM_DARK;
		
		// left right
		fdrawline(x1, y1, x1, y2);
		fdrawline(x2, y1, x2, y2);
	
		// top down
		fdrawline(x1, y2, x2, y2);
		fdrawline(x1, y1, x2, y1); 
	} else {
		MM_DARK;
		gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, 1.5);
	}
	/* END OUTER OUTLINE */
}

/* base flat button */
static void flat_button(float x1, float y1, float x2, float y2, float asp, int colorid, int flag, int mid)
{
	/* 'mid' arg determines whether the button is in the middle of
	 * an alignment group or not. 0 = not middle, 1 = is in the middle.
	 * Done to allow cleaner drawing
	 */
	 
	/* *** FLAT TEXT/NUM FIELD *** */
	glShadeModel(GL_FLAT);
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) M_LGREY;
		else M_GREY;
	}
	else {
		if(flag & UI_ACTIVE) M_NUMTEXT_ACT_LIGHT;
		else M_NUMTEXT;
	}

	glRectf(x1, y1, x2, y2);
	/* *** END FLAT TEXT/NUM FIELD *** */
	
	/* *** OUTER OUTLINE *** */
	if (mid) {
		// we draw full outline, its not AA, and it works better button mouse-over hilite
		MM_DARK;
		
		// left right
		fdrawline(x1, y1, x1, y2);
		fdrawline(x2, y1, x2, y2);
	
		// top down
		fdrawline(x1, y2, x2, y2);
		fdrawline(x1, y1, x2, y1); 
	} else {
		MM_DARK;
		gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, 1.5);
	}
	/* END OUTER OUTLINE */
}

/* small side double arrow for iconrow */
static void ui_default_iconrow_arrows(float x1, float y1, float x2, float y2)
{
	glEnable( GL_POLYGON_SMOOTH );
	glEnable( GL_BLEND );
	
	glShadeModel(GL_FLAT);
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-2,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-6,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2)+4);
	glEnd();
		
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-2,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-6,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2) -4);
	glEnd();
	
	glDisable( GL_BLEND );
	glDisable( GL_POLYGON_SMOOTH );
}

/* side double arrow for menu */
static void ui_default_menu_arrows(float x1, float y1, float x2, float y2)
{
	glEnable( GL_POLYGON_SMOOTH );
	glEnable( GL_BLEND );
	
	glShadeModel(GL_FLAT);
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-12,(short)(y2-(y2-y1)/2)+1);
	glVertex2f((short)x2-8,(short)(y2-(y2-y1)/2)+4);
	glEnd();
		
	glBegin(GL_TRIANGLES);
	glVertex2f((short)x2-4,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-12,(short)(y2-(y2-y1)/2) -1);
	glVertex2f((short)x2-8,(short)(y2-(y2-y1)/2) -4);
	glEnd();
	
	glDisable( GL_BLEND );
	glDisable( GL_POLYGON_SMOOTH );
}

/* left/right arrows for number fields */
static void ui_default_num_arrows(float x1, float y1, float x2, float y2)
{
	if( x2-x1 > 25) {	// 25 is a bit arbitrary, but small buttons cant have arrows

		glEnable( GL_POLYGON_SMOOTH );
		glEnable( GL_BLEND );
		
		glShadeModel(GL_FLAT);
		glBegin(GL_TRIANGLES);
		
		glVertex2f((short)x1+5,(short)(y2-(y2-y1)/2));
		glVertex2f((short)x1+10,(short)(y2-(y2-y1)/2)+4);
		glVertex2f((short)x1+10,(short)(y2-(y2-y1)/2)-4);
		glEnd();

		/* right */
		glShadeModel(GL_FLAT);
		glBegin(GL_TRIANGLES);

		glVertex2f((short)x2-5,(short)(y2-(y2-y1)/2));
		glVertex2f((short)x2-10,(short)(y2-(y2-y1)/2)-4);
		glVertex2f((short)x2-10,(short)(y2-(y2-y1)/2)+4);
		glEnd();
		
		glDisable( GL_BLEND );
		glDisable( GL_POLYGON_SMOOTH );
	}
}

/* changing black/white for TOG3 buts */
static void ui_tog3_invert(float x1, float y1, float x2, float y2, int seltype)
{
	short alpha = 30;
	
	if (seltype == 0) {
		glEnable(GL_BLEND);
		
		glColor4ub(0, 0, 0, alpha);
		glRectf(x2-6, y1, x2, (y1+(y2-y1)/2));
		
		glColor4ub(255, 255, 255, alpha);
		glRectf(x2-6, (y1+(y2-y1)/2), x2, y2);
		
		glDisable(GL_BLEND);
	} else {
		glEnable(GL_BLEND);
		
		glColor4ub(255, 255, 255, alpha);
		glRectf(x2-6, y1, x2, (y1+(y2-y1)/2));
		
		glColor4ub(0, 0, 0, alpha);
		glRectf(x2-6, (y1+(y2-y1)/2), x2, y2);
		
		glDisable(GL_BLEND);
	}
}

/* button/popup menu/iconrow drawing code */
static void ui_default_button(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
	int align= (flag & UI_BUT_ALIGN);

	if(align) {
	
		/* *** BOTTOM OUTER SUNKEN EFFECT *** */
		if (!((align == UI_BUT_ALIGN_DOWN) ||
			(align == (UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT)) ||
			(align == (UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT)))) {
			glEnable(GL_BLEND);
			MM_WHITE_OP;
			fdrawline(x1, y1-1, x2, y1-1);	
			glDisable(GL_BLEND);
		}
		/* *** END BOTTOM OUTER SUNKEN EFFECT *** */
		
		switch(align) {
		case UI_BUT_ALIGN_TOP:
			uiSetRoundBox(12);
			
			/* last arg in shaded_button() determines whether the button is in the middle of
			 * an alignment group or not. 0 = not middle, 1 = is in the middle.
			 * Done to allow cleaner drawing
			 */
			 
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_DOWN:
			uiSetRoundBox(3);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_LEFT:
			
			/* RIGHT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x2+1,y1);
			MM_WHITE_TR;
			glVertex2f(x2+1,y2);
			glEnd();
			glDisable(GL_BLEND);
			
			uiSetRoundBox(6);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_RIGHT:
		
			/* LEFT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x1-1,y1);
			MM_WHITE_TR;
			glVertex2f(x1-1,y2);
			glEnd();
			glDisable(GL_BLEND);
		
			uiSetRoundBox(9);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
			
		case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT:
			uiSetRoundBox(1);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT:
			uiSetRoundBox(2);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_RIGHT:
		
			/* LEFT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x1-1,y1);
			MM_WHITE_TR;
			glVertex2f(x1-1,y2);
			glEnd();
			glDisable(GL_BLEND);
		
			uiSetRoundBox(8);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_LEFT:
		
			/* RIGHT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x2+1,y1);
			MM_WHITE_TR;
			glVertex2f(x2+1,y2);
			glEnd();
			glDisable(GL_BLEND);
			
			uiSetRoundBox(4);
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
			
		default:
			shaded_button(x1, y1, x2, y2, asp, colorid, flag, 1);
			break;
		}
	} 
	else {	
		glEnable(GL_BLEND);
		glShadeModel(GL_SMOOTH);
		
		/* BOTTOM OUTER SUNKEN EFFECT */
		MM_WHITE_OP;
		fdrawline(x1, y1-1, x2, y1-1);	
		
		/* LEFT OUTER SUNKEN EFFECT */
		glBegin(GL_LINES);
		MM_WHITE_OP;
		glVertex2f(x1-1,y1);
		MM_WHITE_TR;
		glVertex2f(x1-1,y2);
		glEnd();
		
		/* RIGHT OUTER SUNKEN EFFECT */
		glBegin(GL_LINES);
		MM_WHITE_OP;
		glVertex2f(x2+1,y1);
		MM_WHITE_TR;
		glVertex2f(x2+1,y2);
		glEnd();
		
		glDisable(GL_BLEND);
	
		uiSetRoundBox(15);
		shaded_button(x1, y1, x2, y2, asp, colorid, flag, 0);
	}
	
	/* *** EXTRA DRAWING FOR SPECIFIC CONTROL TYPES *** */
	switch(type) {
	case ICONROW:
	case ICONTEXTROW:
		/* DARKENED AREA */
		glEnable(GL_BLEND);
		
		glColor4ub(0, 0, 0, 30);
		glRectf(x2-9, y1, x2, y2);
	
		glDisable(GL_BLEND);
		/* END DARKENED AREA */
	
		/* ICONROW DOUBLE-ARROW  */
		M_DARK;
		ui_default_iconrow_arrows(x1, y1, x2, y2);
		/* END ICONROW DOUBLE-ARROW */
		break;
	case MENU:
		/* DARKENED AREA */
		glEnable(GL_BLEND);
		
		glColor4ub(0, 0, 0, 30);
		glRectf(x2-18, y1, x2, y2);
	
		glDisable(GL_BLEND);
		/* END DARKENED AREA */
	
		/* MENU DOUBLE-ARROW  */
		M_DARK;
		ui_default_menu_arrows(x1, y1, x2, y2);
		/* MENU DOUBLE-ARROW */
		break;
	}	
}


/* number/text field drawing code */
static void ui_default_flat(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
	int align= (flag & UI_BUT_ALIGN);

	if(align) {
	
		/* *** BOTTOM OUTER SUNKEN EFFECT *** */
		if (!((align == UI_BUT_ALIGN_DOWN) ||
			(align == (UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT)) ||
			(align == (UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT)))) {
			glEnable(GL_BLEND);
			MM_WHITE_OP;
			fdrawline(x1, y1-1, x2, y1-1);	
			glDisable(GL_BLEND);
		}
		/* *** END BOTTOM OUTER SUNKEN EFFECT *** */
		
		switch(align) {
		case UI_BUT_ALIGN_TOP:
			uiSetRoundBox(12);
			
			/* last arg in shaded_button() determines whether the button is in the middle of
			 * an alignment group or not. 0 = not middle, 1 = is in the middle.
			 * Done to allow cleaner drawing
			 */
			 
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_DOWN:
			uiSetRoundBox(3);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_LEFT:
			
			/* RIGHT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x2+1,y1);
			MM_WHITE_TR;
			glVertex2f(x2+1,y2);
			glEnd();
			glDisable(GL_BLEND);
			
			uiSetRoundBox(6);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_RIGHT:
		
			/* LEFT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x1-1,y1);
			MM_WHITE_TR;
			glVertex2f(x1-1,y2);
			glEnd();
			glDisable(GL_BLEND);
		
			uiSetRoundBox(9);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
			
		case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT:
			uiSetRoundBox(1);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT:
			uiSetRoundBox(2);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_RIGHT:
		
			/* LEFT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x1-1,y1);
			MM_WHITE_TR;
			glVertex2f(x1-1,y2);
			glEnd();
			glDisable(GL_BLEND);
		
			uiSetRoundBox(8);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
		case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_LEFT:
		
			/* RIGHT OUTER SUNKEN EFFECT */
			glEnable(GL_BLEND);
			glShadeModel(GL_SMOOTH);
			glBegin(GL_LINES);
			MM_WHITE_OP;
			glVertex2f(x2+1,y1);
			MM_WHITE_TR;
			glVertex2f(x2+1,y2);
			glEnd();
			glDisable(GL_BLEND);
			
			uiSetRoundBox(4);
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
			break;
			
		default:
			flat_button(x1, y1, x2, y2, asp, colorid, flag, 1);
			break;
		}
	} 
	else {
	
		glEnable(GL_BLEND);
		glShadeModel(GL_SMOOTH);
		
		/* BOTTOM OUTER SUNKEN EFFECT */
		MM_WHITE_OP;
		fdrawline(x1, y1-1, x2, y1-1);	
		
		/* LEFT OUTER SUNKEN EFFECT */
		glBegin(GL_LINES);
		MM_WHITE_OP;
		glVertex2f(x1-1,y1);
		MM_WHITE_TR;
		glVertex2f(x1-1,y2);
		glEnd();
		
		/* RIGHT OUTER SUNKEN EFFECT */
		glBegin(GL_LINES);
		MM_WHITE_OP;
		glVertex2f(x2+1,y1);
		MM_WHITE_TR;
		glVertex2f(x2+1,y2);
		glEnd();
		
		glDisable(GL_BLEND);

		uiSetRoundBox(15);
		flat_button(x1, y1, x2, y2, asp, colorid, flag, 0);
	}
	
	/* *** EXTRA DRAWING FOR SPECIFIC CONTROL TYPES *** */
	switch(type) {
	case NUM:
	case NUMABS:
		/* SIDE ARROWS */
		/* left */
		if(flag & UI_SELECT) {
			if(flag & UI_ACTIVE) M_DARK;
			else M_DARK;
		} else {
			if(flag & UI_ACTIVE) M_GREY;
			else M_LGREY;
		}
		
		ui_default_num_arrows(x1, y1, x2, y2);
		/* END SIDE ARROWS */
	}
}

static void ui_default_slider(int colorid, float fac, float aspect, float x1, float y1, float x2, float y2, int flag)
{
	float ymid, yc;

	/* the slider background line */
	ymid= (y1+y2)/2.0;
	//yc= 2.5*aspect;	// height of center line
	yc = 2.3; // height of center line
	
	if(flag & UI_SELECT) 
			BIF_ThemeColorShade(TH_BUT_NUM, -5);
	else {
		if(flag & UI_ACTIVE) 
			BIF_ThemeColorShade(TH_BUT_NUM, +35); 
		else
			BIF_ThemeColorShade(TH_BUT_NUM, +25); 
	}

	glRectf(x1, ymid-yc, x2, ymid+yc);
	
	/* top inner bevel */
	if(flag & UI_SELECT) BIF_ThemeColorShade(TH_BUT_NUM, -40); 
	else BIF_ThemeColorShade(TH_BUT_NUM, -5); 
	fdrawline(x1+1, ymid+yc, x2, ymid+yc);
	
	/* bottom inner bevel */
	if(flag & UI_SELECT) BIF_ThemeColorShade(TH_BUT_NUM, +15); 
	else BIF_ThemeColorShade(TH_BUT_NUM, +45); 
	fdrawline(x1+1, ymid-yc, x2, ymid-yc);
	
	
	/* the movable slider */
	if(flag & UI_SELECT) BIF_ThemeColorShade(TH_BUT_NUM, +80); 
	else BIF_ThemeColorShade(TH_BUT_NUM, -45); 

	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);

	BIF_ThemeColorShade(TH_BUT_NUM, -45); 

	glVertex2f(x1,     y1+2.5);
	glVertex2f(x1+fac, y1+2.5);

	BIF_ThemeColor(TH_BUT_NUM); 

	glVertex2f(x1+fac, y2-2.5);
	glVertex2f(x1,     y2-2.5);

	glEnd();
	

	/* slider handle center */
	glShadeModel(GL_SMOOTH);
	glBegin(GL_QUADS);

	BIF_ThemeColor(TH_BUT_NUM); 
	glVertex2f(x1+fac-3, y1+2);
	glVertex2f(x1+fac, y1+4);
	BIF_ThemeColorShade(TH_BUT_NUM, +80); 
	glVertex2f(x1+fac, y2-2);
	glVertex2f(x1+fac-3, y2-2);

	glEnd();
	
	/* slider handle left bevel */
	BIF_ThemeColorShade(TH_BUT_NUM, +70); 
	fdrawline(x1+fac-3, y2-2, x1+fac-3, y1+2);
	
	/* slider handle right bevel */
	BIF_ThemeColorShade(TH_BUT_NUM, -35); 
	fdrawline(x1+fac, y2-2, x1+fac, y1+2);

	glShadeModel(GL_FLAT);
}

/* default theme callback */
static void ui_draw_default(int type, int colorid, float aspect, float x1, float y1, float x2, float y2, int flag)
{

	switch(type) {
	case TEX:
	case IDPOIN:
	case NUM:
	case NUMABS:
		ui_default_flat(type, colorid, aspect, x1, y1, x2, y2, flag);
		break;
	case ICONROW: 
	case ICONTEXTROW: 
	case MENU: 
	default: 
		ui_default_button(type, colorid, aspect, x1, y1, x2, y2, flag);
	}

}


/* *************** OLDSKOOL THEME ***************** */

static void ui_draw_outlineX(float x1, float y1, float x2, float y2, float asp1)
{
	float vec[2];
	
	glBegin(GL_LINE_LOOP);
	vec[0]= x1+asp1; vec[1]= y1-asp1;
	glVertex2fv(vec);
	vec[0]= x2-asp1; 
	glVertex2fv(vec);
	vec[0]= x2+asp1; vec[1]= y1+asp1;
	glVertex2fv(vec);
	vec[1]= y2-asp1;
	glVertex2fv(vec);
	vec[0]= x2-asp1; vec[1]= y2+asp1;
	glVertex2fv(vec);
	vec[0]= x1+asp1;
	glVertex2fv(vec);
	vec[0]= x1-asp1; vec[1]= y2-asp1;
	glVertex2fv(vec);
	vec[1]= y1+asp1;
	glVertex2fv(vec);
	glEnd();                
        
}


static void ui_draw_oldskool(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
 	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, -40);
		else BIF_ThemeColorShade(colorid, -30);
	}
	else {
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, +30);
		else BIF_ThemeColorShade(colorid, +20);
	}
	
	glRectf(x1+1, y1+1, x2-1, y2-1);

	x1+= asp;
	x2-= asp;
	y1+= asp;
	y2-= asp;

	/* below */
	if(flag & UI_SELECT) BIF_ThemeColorShade(colorid, 0);
	else BIF_ThemeColorShade(colorid, -30);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(flag & UI_SELECT) BIF_ThemeColorShade(colorid, -30);
	else BIF_ThemeColorShade(colorid, 0);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
	/* outline */
	glColor3ub(0,0,0);
	ui_draw_outlineX(x1, y1, x2, y2, asp);
	
	
	/* special type decorations */
	switch(type) {
	case NUM:
	case NUMABS:
		if(flag & UI_SELECT) BIF_ThemeColorShade(colorid, -60);
		else BIF_ThemeColorShade(colorid, -30);
		ui_default_num_arrows(x1, y1, x2, y2);
		break;

	case ICONROW: 
	case ICONTEXTROW: 
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, 0);
		else BIF_ThemeColorShade(colorid, -10);
		glRectf(x2-9, y1+asp, x2-asp, y2-asp);

		BIF_ThemeColorShade(colorid, -50);
		ui_default_iconrow_arrows(x1, y1, x2, y2);
		break;
		
	case MENU: 
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, 0);
		else BIF_ThemeColorShade(colorid, -10);
		glRectf(x2-17, y1+asp, x2-asp, y2-asp);

		BIF_ThemeColorShade(colorid, -50);
		ui_default_menu_arrows(x1, y1, x2, y2);
		break;
	}
	
}

/* *************** BASIC ROUNDED THEME ***************** */

static void round_button(float x1, float y1, float x2, float y2, float asp, 
						 int colorid, int round, int menudeco, int curshade)
{
	float rad;
	char col[4];
	
	rad= (y2-y1)/2.0;
	if(rad>7.0) rad= 7.0;
	
	uiSetRoundBox(round);
	gl_round_box(GL_POLYGON, x1, y1, x2, y2, rad);

	if(menudeco) {
		uiSetRoundBox(round & ~9);
		BIF_ThemeColorShade(colorid, curshade-20);
		gl_round_box(GL_POLYGON, x2-menudeco, y1, x2, y2, rad);
	}
	
	/* fake AA */
	uiSetRoundBox(round);
	glEnable( GL_BLEND );

	BIF_GetThemeColor3ubv(colorid, col);
		
	if(col[0]<100) col[0]= 0; else col[0]-= 100;
	if(col[1]<100) col[1]= 0; else col[1]-= 100;
	if(col[2]<100) col[2]= 0; else col[2]-= 100;
	col[3]= 80;
	glColor4ubv((GLubyte *)col);
	gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, rad - asp);
	gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, rad + asp);
	col[3]= 180;
	glColor4ubv((GLubyte *)col);
	gl_round_box(GL_LINE_LOOP, x1, y1, x2, y2, rad);

	glDisable( GL_BLEND );
}

/* button in midst of alignment row */
static void round_button_mid(float x1, float y1, float x2, float y2, float asp, 
							 int colorid, int align, int menudeco, int curshade)
{
	glRectf(x1, y1, x2, y2);
	
	if(menudeco) {
		BIF_ThemeColorShade(colorid, curshade-20);
		glRectf(x2-menudeco, y1, x2, y2);
	}
	
	BIF_ThemeColorBlendShade(colorid, TH_BACK, 0.5, -70);
	// we draw full outline, its not AA, and it works better button mouse-over hilite
	
	// left right
	fdrawline(x1, y1, x1, y2);
	fdrawline(x2, y1, x2, y2);

	// top down
	fdrawline(x1, y2, x2, y2);
	fdrawline(x1, y1, x2, y1);   
}

static void ui_draw_round(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
	int align= (flag & UI_BUT_ALIGN);
	int curshade= 0, menudeco= 0;
	
	if(type==ICONROW || type==ICONTEXTROW) menudeco= 9;
	else if((type==MENU || type==BLOCK) && x2-x1>24) menudeco= 16;
	
	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) curshade= -40;
		else curshade= -30;
	}
	else {
		if(flag & UI_ACTIVE) curshade= 30;
		else curshade= +20;
	}
	
	BIF_ThemeColorShade(colorid, curshade);

	if(align) {
		switch(align) {
		case UI_BUT_ALIGN_TOP:
			round_button(x1, y1, x2, y2, asp, colorid, 12, menudeco, curshade);
			break;
		case UI_BUT_ALIGN_DOWN:
			round_button(x1, y1, x2, y2, asp, colorid, 3, menudeco, curshade);
			break;
		case UI_BUT_ALIGN_LEFT:
			round_button(x1, y1, x2, y2, asp, colorid, 6, menudeco, curshade);
			break;
		case UI_BUT_ALIGN_RIGHT:
			round_button(x1, y1, x2, y2, asp, colorid, 9, menudeco, curshade);
			break;
			
		case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT:
			round_button(x1, y1, x2, y2, asp, colorid, 1, menudeco, curshade);
			break;
		case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT:
			round_button(x1, y1, x2, y2, asp, colorid, 2, menudeco, curshade);
			break;
		case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_RIGHT:
			round_button(x1, y1, x2, y2, asp, colorid, 8, menudeco, curshade);
			break;
		case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_LEFT:
			round_button(x1, y1, x2, y2, asp, colorid, 4, menudeco, curshade);
			break;
			
		default:
			round_button_mid(x1, y1, x2, y2, asp, colorid, align, menudeco, curshade);
			break;
		}
	} 
	else {
		round_button(x1, y1, x2, y2, asp, colorid, 15, menudeco, curshade);
	}

	/* special type decorations */
	switch(type) {
	case NUM:
	case NUMABS:
		BIF_ThemeColorShade(colorid, curshade-60);
		ui_default_num_arrows(x1, y1, x2, y2);
		break;

	case ICONROW: 
	case ICONTEXTROW: 
		BIF_ThemeColorShade(colorid, curshade-60);
		ui_default_iconrow_arrows(x1, y1, x2, y2);
		break;
		
	case MENU: 
	case BLOCK: 
		BIF_ThemeColorShade(colorid, curshade-60);
		ui_default_menu_arrows(x1, y1, x2, y2);
		break;
	}
}

/* *************** MINIMAL THEME ***************** */

// theme can define an embosfunc and sliderfunc, text+icon drawing is standard, no theme.



/* super minimal button as used in logic menu */
static void ui_draw_minimal(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
	/* too much space between buttons */
	
	if (type==TEX || type==IDPOIN) {
		x1+= asp;
		x2-= (asp*2);
		//y1+= asp;
		y2-= asp;
	} else {
		/* Less space between buttons looks nicer */
		y2-= asp;
		x2-= asp;
	}
	
	/* paper */
	if(flag & UI_SELECT) {
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, -40);
		else BIF_ThemeColorShade(colorid, -30);
	}
	else {
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, +20);
		else BIF_ThemeColorShade(colorid, +10);
	}
	
	glRectf(x1, y1, x2, y2);
	
	if (type==TEX || type==IDPOIN) {
		BIF_ThemeColorShade(colorid, -60);

		/* top */
		fdrawline(x1, y2, x2, y2);
		/* left */
		fdrawline(x1, y1, x1, y2);
		
		
		/* text underline, some  */ 
		BIF_ThemeColorShade(colorid, +50);
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(1, 0x8888);
		fdrawline(x1+(asp*2), y1+(asp*3), x2-(asp*2), y1+(asp*3));
		glDisable(GL_LINE_STIPPLE);
		
		
		BIF_ThemeColorShade(colorid, +60);
		/* below */
		fdrawline(x1, y1, x2, y1);
		/* right */
		fdrawline(x2, y1, x2, y2);
		
	} else {
		if(flag & UI_SELECT) {
			BIF_ThemeColorShade(colorid, -60);

			/* top */
			fdrawline(x1, y2, x2, y2);
			/* left */
			fdrawline(x1, y1, x1, y2);
			BIF_ThemeColorShade(colorid, +40);

			/* below */
			fdrawline(x1, y1, x2, y1);
			/* right */
			fdrawline(x2, y1, x2, y2);
		}
		else {
			BIF_ThemeColorShade(colorid, +40);

			/* top */
			fdrawline(x1, y2, x2, y2);
			/* left */
			fdrawline(x1, y1, x1, y2);
			
			BIF_ThemeColorShade(colorid, -60);
			/* below */
			fdrawline(x1, y1, x2, y1);
			/* right */
			fdrawline(x2, y1, x2, y2);
		}
	}
	
	/* special type decorations */
	switch(type) {
	case NUM:
	case NUMABS:
		if(flag & UI_SELECT) BIF_ThemeColorShade(colorid, -60);
		else BIF_ThemeColorShade(colorid, -30);
		ui_default_num_arrows(x1, y1, x2, y2);
		break;

	case ICONROW: 
	case ICONTEXTROW: 
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, 0);
		else BIF_ThemeColorShade(colorid, -10);
		glRectf(x2-9, y1+asp, x2-asp, y2-asp);

		BIF_ThemeColorShade(colorid, -50);
		ui_default_iconrow_arrows(x1, y1, x2, y2);
		break;
		
	case MENU: 
	case BLOCK: 
		if(flag & UI_ACTIVE) BIF_ThemeColorShade(colorid, 0);
		else BIF_ThemeColorShade(colorid, -10);
		glRectf(x2-17, y1+asp, x2-asp, y2-asp);

		BIF_ThemeColorShade(colorid, -50);
		ui_default_menu_arrows(x1, y1, x2, y2);
		break;
	}
	
	
}


/* fac is the slider handle position between x1 and x2 */
static void ui_draw_slider(int colorid, float fac, float aspect, float x1, float y1, float x2, float y2, int flag)
{
	float ymid, yc;

	/* the slider background line */
	ymid= (y1+y2)/2.0;
	yc= 1.7*aspect;	

	if(flag & UI_ACTIVE) 
		BIF_ThemeColorShade(colorid, -50); 
	else 
		BIF_ThemeColorShade(colorid, -40); 

	/* left part */
	glRectf(x1, ymid-2.0*yc, x1+fac, ymid+2.0*yc);
	/* right part */
	glRectf(x1+fac, ymid-yc, x2, ymid+yc);

	/* the movable slider */
	
	BIF_ThemeColorShade(colorid, +70); 
	glRectf(x1+fac-aspect, ymid-2.0*yc, x1+fac+aspect, ymid+2.0*yc);

}

/* ************** STANDARD MENU DRAWING FUNCTION ************* */


static void ui_shadowbox(float minx, float miny, float maxx, float maxy, float shadsize, unsigned char alpha)
{
	glEnable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
	
	/* right quad */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glVertex2f(maxx, maxy-shadsize);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx+shadsize, maxy-shadsize-shadsize);
	glVertex2f(maxx+shadsize, miny);
	glEnd();
	
	/* corner shape */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx+shadsize, miny);
	glVertex2f(maxx+0.7*shadsize, miny-0.7*shadsize);
	glVertex2f(maxx, miny-shadsize);
	glEnd();
	
	/* bottom quad */		
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(minx+shadsize, miny);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx, miny-shadsize);
	glVertex2f(minx+shadsize+shadsize, miny-shadsize);
	glEnd();
	
	glDisable(GL_BLEND);
	glShadeModel(GL_FLAT);
}

void uiDrawBoxShadow(unsigned char alpha, float minx, float miny, float maxx, float maxy)
{
	/* accumulated outline boxes to make shade not linear, is more pleasant */
	ui_shadowbox(minx, miny, maxx, maxy, 6.0, (30*alpha)>>8);
	ui_shadowbox(minx, miny, maxx, maxy, 4.0, (70*alpha)>>8);
	ui_shadowbox(minx, miny, maxx, maxy, 2.0, (100*alpha)>>8);
	
}

// background for pulldowns, pullups, and other drawing temporal menus....
// has to be made themable still (now only color)

void uiDrawMenuBox(float minx, float miny, float maxx, float maxy, short flag)
{
	char col[4];
	BIF_GetThemeColor4ubv(TH_MENU_BACK, col);
	
	if( (flag & UI_BLOCK_NOSHADOW)==0) {
		/* accumulated outline boxes to make shade not linear, is more pleasant */
		ui_shadowbox(minx, miny, maxx, maxy, 6.0, (30*col[3])>>8);
		ui_shadowbox(minx, miny, maxx, maxy, 4.0, (70*col[3])>>8);
		ui_shadowbox(minx, miny, maxx, maxy, 2.0, (100*col[3])>>8);
		
		glEnable(GL_BLEND);
		glColor4ubv((GLubyte *)col);
		glRectf(minx-1, miny, minx, maxy);	// 1 pixel on left, to distinguish sublevel menus
	}
	glEnable(GL_BLEND);
	glColor4ubv((GLubyte *)col);
	glRectf(minx, miny, maxx, maxy);
	glDisable(GL_BLEND);
}



/* pulldown menu item */
static void ui_draw_pulldown_item(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
	char col[4];
	
	BIF_GetThemeColor4ubv(TH_MENU_BACK, col);
	if(col[3]!=255) {
		glEnable(GL_BLEND);
	}
	
	if((flag & UI_ACTIVE) && type!=LABEL) {
		BIF_ThemeColor4(TH_MENU_HILITE);
		glRectf(x1, y1, x2, y2);
	

	} else {
		BIF_ThemeColor4(colorid);	// is set at TH_MENU_ITEM when pulldown opened.
		glRectf(x1, y1, x2, y2);
	}

	glDisable(GL_BLEND);
}

/* pulldown menu calling button */
static void ui_draw_pulldown_round(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
	
	if(flag & UI_ACTIVE) {
		BIF_ThemeColor(TH_MENU_HILITE);

		uiSetRoundBox(15);
		gl_round_box(GL_POLYGON, x1, y1+3, x2, y2-3, 7.0);

		glEnable( GL_LINE_SMOOTH );
		glEnable( GL_BLEND );
		gl_round_box(GL_LINE_LOOP, x1, y1+3, x2, y2-3, 7.0);
		glDisable( GL_LINE_SMOOTH );
		glDisable( GL_BLEND );
		
	} else {
		BIF_ThemeColor(colorid);	// is set at TH_MENU_ITEM when pulldown opened.
		glRectf(x1-1, y1+2, x2+1, y2-2);
	}
	
}


/* ************** TEXT AND ICON DRAWING FUNCTIONS ************* */



/* draws text and icons for buttons */
static void ui_draw_text_icon(uiBut *but)
{
	float x;
	int len;
	char *cpoin;
	short t, pos, ch;
	short selsta_tmp, selend_tmp, selsta_draw, selwidth_draw;
	
	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd), 0);
	}
	else {

		/* text button selection and cursor */
		if(but->pos != -1) {
		
			if ((but->selend - but->selsta) > 0) {
				/* text button selection */
				selsta_tmp = but->selsta + strlen(but->str);
				selend_tmp = but->selend + strlen(but->str);
					
				if(but->drawstr[0]!=0) {
					ch= but->drawstr[selsta_tmp];
					but->drawstr[selsta_tmp]= 0;
					
					selsta_draw = but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, (U.transopts & USER_TR_BUTTONS)) + 3;
					
					but->drawstr[selsta_tmp]= ch;
					
					
					ch= but->drawstr[selend_tmp];
					but->drawstr[selend_tmp]= 0;
					
					selwidth_draw = but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, (U.transopts & USER_TR_BUTTONS)) + 3;
					
					but->drawstr[selend_tmp]= ch;
					
					BIF_ThemeColor(TH_BUT_TEXTFIELD_HI);
					glRects(but->x1+selsta_draw+1, but->y1+2, but->x1+selwidth_draw+1, but->y2-2);
				}
			} else {
				/* text cursor */
				pos= but->pos+strlen(but->str);
				if(pos >= but->ofs) {
					if(but->drawstr[0]!=0) {
						ch= but->drawstr[pos];
						but->drawstr[pos]= 0;
			
						t= but->aspect*BIF_GetStringWidth(but->font, but->drawstr+but->ofs, (U.transopts & USER_TR_BUTTONS)) + 3;
						
						but->drawstr[pos]= ch;
					}
					else t= 3;
					
					glColor3ub(255,0,0);
					glRects(but->x1+t, but->y1+2, but->x1+t+2, but->y2-2);
				}
			}
		}
		
		if(but->type==BUT_TOGDUAL) {
			int dualset= 0;
			if(but->pointype==SHO)
				dualset= BTST( *(((short *)but->poin)+1), but->bitnr);
			else if(but->pointype==INT)
				dualset= BTST( *(((int *)but->poin)+1), but->bitnr);
			
			ui_draw_icon(but, ICON_DOT, dualset?0:-100);
		}
		
		if(but->drawstr[0]!=0) {
			int transopts;
			int tog3= 0;
			
			// cut string in 2 parts
			cpoin= strchr(but->drawstr, '|');
			if(cpoin) *cpoin= 0;

			/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
			and offset the text label to accomodate it */
			
			if ( (but->flag & UI_HAS_ICON) && (but->flag & UI_ICON_LEFT) ) {
				ui_draw_icon(but, but->icon, 0);

				if(but->flag & UI_TEXT_LEFT) x= but->x1 + but->aspect*BIF_icon_get_width(but->icon)+5.0;
				else x= (but->x1+but->x2-but->strwidth+1)/2.0;
			}
			else {
				if(but->flag & UI_TEXT_LEFT) x= but->x1+4.0;
				else x= (but->x1+but->x2-but->strwidth+1)/2.0;
			}
			
			/* tog3 button exception; draws with glColor! */
			if(but->type==TOG3 && (but->flag & UI_SELECT)) {
				
				if( but->pointype==CHA ) {
					if( BTST( *(but->poin+2), but->bitnr )) tog3= 1;
				}
				else if( but->pointype ==SHO ) {
					short *sp= (short *)but->poin;
					if( BTST( sp[1], but->bitnr )) tog3= 1;
				}
				
				ui_tog3_invert(but->x1,but->y1,but->x2,but->y2, tog3);
				if (tog3) glColor3ub(255, 255, 0);
			}
			
			/* text color, with pulldown item exception */
			if(tog3);	// color already set
			else if(but->dt==UI_EMBOSSP) {
				if((but->flag & (UI_SELECT|UI_ACTIVE)) && but->type!=LABEL) {	// LABEL = title in pulldowns
					BIF_ThemeColor(TH_MENU_TEXT_HI);
				} else {
					BIF_ThemeColor(TH_MENU_TEXT);
				}
			}
			else {
				if(but->flag & UI_SELECT) {		
					BIF_ThemeColor(TH_BUT_TEXT_HI);
				} else {
					BIF_ThemeColor(TH_BUT_TEXT);
				}
			}

			/* LABEL button exception */
			if(but->type==LABEL && but->min!=0.0) BIF_ThemeColor(TH_BUT_TEXT_HI);
		
			ui_rasterpos_safe(x, (but->y1+but->y2- 9.0)/2.0, but->aspect);
			if(but->type==IDPOIN) transopts= 0;	// no translation, of course!
			else transopts= (U.transopts & USER_TR_BUTTONS);
			
		#ifdef INTERNATIONAL
			if (but->type == FTPREVIEW)
				FTF_DrawNewFontString (but->drawstr+but->ofs, FTF_INPUT_UTF8);
			else
				BIF_DrawString(but->font, but->drawstr+but->ofs, transopts);
		#else
			BIF_DrawString(but->font, but->drawstr+but->ofs, transopts);
		#endif

			/* part text right aligned */
			if(cpoin) {
				len= BIF_GetStringWidth(but->font, cpoin+1, (U.transopts & USER_TR_BUTTONS));
				ui_rasterpos_safe( but->x2 - len*but->aspect-3, (but->y1+but->y2- 9.0)/2.0, but->aspect);
				BIF_DrawString(but->font, cpoin+1, (U.transopts & USER_TR_BUTTONS));
				*cpoin= '|';
			}
		}
		/* if there's no text label, then check to see if there's an icon only and draw it */
		else if( but->flag & UI_HAS_ICON ) {
			ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd), 0);
		}
	}
}

static void ui_draw_but_COL(uiBut *but)
{
	float *fp;
	char colr, colg, colb;
	
	if( but->pointype==FLO ) {
		fp= (float *)but->poin;
		colr= floor(255.0*fp[0]+0.5);
		colg= floor(255.0*fp[1]+0.5);
		colb= floor(255.0*fp[2]+0.5);
	}
	else {
		char *cp= (char *)but->poin;
		colr= cp[0];
		colg= cp[1];
		colb= cp[2];
	}
	
	/* exception... hrms, but can't simply use the emboss callback for this now. */
	/* this button type needs review, and nice integration with rest of API here */
	if(but->embossfunc == ui_draw_round) {
		char *cp= BIF_ThemeGetColorPtr(U.themes.first, 0, TH_CUSTOM);
		cp[0]= colr; cp[1]= colg; cp[2]= colb;
		but->flag &= ~UI_SELECT;
		but->embossfunc(but->type, TH_CUSTOM, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);
	}
	else {
		
		glColor3ub(colr,  colg,  colb);
		glRectf((but->x1), (but->y1), (but->x2), (but->y2));
		glColor3ub(0,  0,  0);
		fdrawbox((but->x1), (but->y1), (but->x2), (but->y2));
	}
}

/* draws in resolution of 20x4 colors */
static void ui_draw_but_HSVCUBE(uiBut *but)
{
	int a;
	float h,s,v;
	float dx, dy, sx1, sx2, sy, x, y;
	float col0[4][3];	// left half, rect bottom to top
	float col1[4][3];	// right half, rect bottom to top
	
	h= but->hsv[0];
	s= but->hsv[1];
	v= but->hsv[2];
	
	/* draw series of gouraud rects */
	glShadeModel(GL_SMOOTH);
	
	if(but->a1==0) {	// H and V vary
		hsv_to_rgb(0.0, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
		hsv_to_rgb(0.0, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
		hsv_to_rgb(0.0, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
		hsv_to_rgb(0.0, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
		x= h; y= v;
	}
	else if(but->a1==1) {	// H and S vary
		hsv_to_rgb(0.0, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
		hsv_to_rgb(0.0, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
		hsv_to_rgb(0.0, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
		hsv_to_rgb(0.0, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
		x= h; y= s;
	}
	else if(but->a1==2) {	// S and V vary
		hsv_to_rgb(h, 0.0, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
		hsv_to_rgb(h, 0.333, 0.0, &col1[1][0], &col1[1][1], &col1[1][2]);
		hsv_to_rgb(h, 0.666, 0.0, &col1[2][0], &col1[2][1], &col1[2][2]);
		hsv_to_rgb(h, 1.0, 0.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
		x= v; y= s;
	}
	else {		// only hue slider
		hsv_to_rgb(0.0, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
		VECCOPY(col1[1], col1[0]);
		VECCOPY(col1[2], col1[0]);
		VECCOPY(col1[3], col1[0]);
		x= h; y= 0.5;
	}
	
	for(dx=0.0; dx<1.0; dx+= 0.05) {
		// previous color
		VECCOPY(col0[0], col1[0]);
		VECCOPY(col0[1], col1[1]);
		VECCOPY(col0[2], col1[2]);
		VECCOPY(col0[3], col1[3]);

		// new color
		if(but->a1==0) {	// H and V vary
			hsv_to_rgb(dx, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(dx, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(dx, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(dx, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
		}
		else if(but->a1==1) {	// H and S vary
			hsv_to_rgb(dx, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(dx, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(dx, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(dx, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
		}
		else if(but->a1==2) {	// S and V vary
			hsv_to_rgb(h, 0.0, dx,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(h, 0.333, dx, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(h, 0.666, dx, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(h, 1.0, dx,   &col1[3][0], &col1[3][1], &col1[3][2]);
		}
		else {	// only H
			hsv_to_rgb(dx, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			VECCOPY(col1[1], col1[0]);
			VECCOPY(col1[2], col1[0]);
			VECCOPY(col1[3], col1[0]);
		}
		
		// rect
		sx1= but->x1 + dx*(but->x2-but->x1);
		sx2= but->x1 + (dx+0.05)*(but->x2-but->x1);
		sy= but->y1;
		dy= (but->y2-but->y1)/3.0;
		
		glBegin(GL_QUADS);
		for(a=0; a<3; a++, sy+=dy) {
			glColor3fv(col0[a]);
			glVertex2f(sx1, sy);

			glColor3fv(col1[a]);
			glVertex2f(sx2, sy);
			
			glColor3fv(col1[a+1]);
			glVertex2f(sx2, sy+dy);
			
			glColor3fv(col0[a+1]);
			glVertex2f(sx1, sy+dy);
		}
		glEnd();
	}

	glShadeModel(GL_FLAT);

	/* cursor */
	x= but->x1 + x*(but->x2-but->x1);
	y= but->y1 + y*(but->y2-but->y1);
	CLAMP(x, but->x1+3.0, but->x2-3.0);
	CLAMP(y, but->y1+3.0, but->y2-3.0);
	
	fdrawXORcirc(x, y, 3.1);

	/* outline */
	glColor3ub(0,  0,  0);
	fdrawbox((but->x1), (but->y1), (but->x2), (but->y2));
}

#ifdef INTERNATIONAL
static void ui_draw_but_CHARTAB(uiBut *but)
{
	/* Some local variables */
	float sx, sy, ex, ey;
	float width, height;
	float butw, buth;
	int x, y, cs;
	wchar_t wstr[2];
	unsigned char ustr[16];
	PackedFile *pf;
	int result = 0;
	int charmax = G.charmax;
	
	/* <builtin> font in use. There are TTF <builtin> and non-TTF <builtin> fonts */
	if(!strcmp(G.selfont->name, "<builtin>"))
	{
		if(G.ui_international == TRUE)
		{
			charmax = 0xff;
		}
		else
		{
			charmax = 0xff;
		}
	}

	/* Category list exited without selecting the area */
	if(G.charmax == 0)
		charmax = G.charmax = 0xffff;

	/* Calculate the size of the button */
	width = abs(but->x2 - but->x1);
	height = abs(but->y2 - but->y1);
	
	butw = floor(width / 12);
	buth = floor(height / 6);
	
	/* Initialize variables */
	sx = but->x1;
	ex = but->x1 + butw;
	sy = but->y1 + height - buth;
	ey = but->y1 + height;

	cs = G.charstart;

	/* Set the font, in case it is not <builtin> font */
	if(G.selfont && strcmp(G.selfont->name, "<builtin>"))
	{
		char tmpStr[256];

		// Is the font file packed, if so then use the packed file
		if(G.selfont->packedfile)
		{
			pf = G.selfont->packedfile;		
			FTF_SetFont(pf->data, pf->size, 14.0);
		}
		else
		{
			int err;

			strcpy(tmpStr, G.selfont->name);
			BLI_convertstringcode(tmpStr, G.sce);
			err = FTF_SetFont((unsigned char *)tmpStr, 0, 14.0);
		}
	}
	else
	{
		if(G.ui_international == TRUE)
		{
			FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, 14.0);
		}
	}

	/* Start drawing the button itself */
	glShadeModel(GL_SMOOTH);

	glColor3ub(200,  200,  200);
	glRectf((but->x1), (but->y1), (but->x2), (but->y2));

	glColor3ub(0,  0,  0);
	for(y = 0; y < 6; y++)
	{
		// Do not draw more than the category allows
		if(cs > charmax) break;

		for(x = 0; x < 12; x++)
		{
			// Do not draw more than the category allows
			if(cs > charmax) break;

			// Draw one grid cell
			glBegin(GL_LINE_LOOP);
				glVertex2f(sx, sy);
				glVertex2f(ex, sy);
				glVertex2f(ex, ey);
				glVertex2f(sx, ey);				
			glEnd();	

			// Draw character inside the cell
			memset(wstr, 0, sizeof(wchar_t)*2);
			memset(ustr, 0, 16);

			// Set the font to be either unicode or <builtin>				
			wstr[0] = cs;
			if(strcmp(G.selfont->name, "<builtin>"))
			{
				wcs2utf8s((char *)ustr, (wchar_t *)wstr);
			}
			else
			{
				if(G.ui_international == TRUE)
				{
					wcs2utf8s((char *)ustr, (wchar_t *)wstr);
				}
				else
				{
					ustr[0] = cs;
					ustr[1] = 0;
				}
			}

			if((G.selfont && strcmp(G.selfont->name, "<builtin>")) || (G.selfont && !strcmp(G.selfont->name, "<builtin>") && G.ui_international == TRUE))
			{
				float wid;
				float llx, lly, llz, urx, ury, urz;
				float dx, dy;
				float px, py;
	
				// Calculate the position
				wid = FTF_GetStringWidth((char *) ustr, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				FTF_GetBoundingBox((char *) ustr, &llx,&lly,&llz,&urx,&ury,&urz, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				dx = urx-llx;
				dy = ury-lly;

				// This isn't fully functional since the but->aspect isn't working like I suspected
				px = sx + ((butw/but->aspect)-dx)/2;
				py = sy + ((buth/but->aspect)-dy)/2;

				// Set the position and draw the character
				ui_rasterpos_safe(px, py, but->aspect);
				FTF_DrawString((char *) ustr, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			}
			else
			{
				ui_rasterpos_safe(sx + butw/2, sy + buth/2, but->aspect);
				BIF_DrawString(but->font, (char *) ustr, 0);
			}
	
			// Calculate the next position and character
			sx += butw; ex +=butw;
			cs++;
		}
		/* Add the y position and reset x position */
		sy -= buth; 
		ey -= buth;
		sx = but->x1;
		ex = but->x1 + butw;
	}	
	glShadeModel(GL_FLAT);

	/* Return Font Settings to original */
	if(U.fontsize && U.fontname[0])
	{
		result = FTF_SetFont((unsigned char *)U.fontname, 0, U.fontsize);
	}
	else if (U.fontsize)
	{
		result = FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, U.fontsize);
	}

	if (result == 0)
	{
		result = FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, 11);
	}
	
	/* resets the font size */
	if(G.ui_international == TRUE)
	{
		uiSetCurFont(but->block, UI_HELV);
	}
}

#endif // INTERNATIONAL

static void ui_draw_but_COLORBAND(uiBut *but)
{
	ColorBand *coba= (ColorBand *)but->poin;
	CBData *cbd;
	float x1, y1, sizex, sizey;
	float dx, v3[2], v1[2], v2[2], v1a[2], v2a[2];
	int a;
		
	if(coba==NULL) return;
	
	x1= but->x1;
	y1= but->y1;
	sizex= but->x2-x1;
	sizey= but->y2-y1;
	
	/* first background, to show tranparency */
	dx= sizex/12.0;
	v1[0]= x1;
	for(a=0; a<12; a++) {
		if(a & 1) glColor3f(0.3, 0.3, 0.3); else glColor3f(0.8, 0.8, 0.8);
		glRectf(v1[0], y1, v1[0]+dx, y1+0.5*sizey);
		if(a & 1) glColor3f(0.8, 0.8, 0.8); else glColor3f(0.3, 0.3, 0.3);
		glRectf(v1[0], y1+0.5*sizey, v1[0]+dx, y1+sizey);
		v1[0]+= dx;
	}
	
	glShadeModel(GL_SMOOTH);
	glEnable(GL_BLEND);
	
	cbd= coba->data;
	
	v1[0]= v2[0]= x1;
	v1[1]= y1;
	v2[1]= y1+sizey;
	
	glBegin(GL_QUAD_STRIP);
	
	glColor4fv( &cbd->r );
	glVertex2fv(v1); glVertex2fv(v2);
	
	for(a=0; a<coba->tot; a++, cbd++) {
		
		v1[0]=v2[0]= x1+ cbd->pos*sizex;
		
		glColor4fv( &cbd->r );
		glVertex2fv(v1); glVertex2fv(v2);
	}
	
	v1[0]=v2[0]= x1+ sizex;
	glVertex2fv(v1); glVertex2fv(v2);
	
	glEnd();
	glShadeModel(GL_FLAT);
	glDisable(GL_BLEND);
	
	/* outline */
	v1[0]= x1; v1[1]= y1;
	
	cpack(0x0);
	glBegin(GL_LINE_LOOP);
	glVertex2fv(v1);
	v1[0]+= sizex;
	glVertex2fv(v1);
	v1[1]+= sizey;
	glVertex2fv(v1);
	v1[0]-= sizex;
	glVertex2fv(v1);
	glEnd();
	
	
	/* help lines */
	v1[0]= v2[0]=v3[0]= x1;
	v1[1]= y1;
	v1a[1]= y1+0.25*sizey;
	v2[1]= y1+0.5*sizey;
	v2a[1]= y1+0.75*sizey;
	v3[1]= y1+sizey;
	
	
	cbd= coba->data;
	glBegin(GL_LINES);
	for(a=0; a<coba->tot; a++, cbd++) {
		v1[0]=v2[0]=v3[0]=v1a[0]=v2a[0]= x1+ cbd->pos*sizex;
		
		if(a==coba->cur) {
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v3);
			glEnd();
			
			setlinestyle(2);
			glBegin(GL_LINES);
			glColor3ub(255, 255, 255);
			glVertex2fv(v1);
			glVertex2fv(v3);
			glEnd();
			setlinestyle(0);
			glBegin(GL_LINES);
			
			/* glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v1a);
			glColor3ub(255, 255, 255);
			glVertex2fv(v1a);
			glVertex2fv(v2);
			glColor3ub(0, 0, 0);
			glVertex2fv(v2);
			glVertex2fv(v2a);
			glColor3ub(255, 255, 255);
			glVertex2fv(v2a);
			glVertex2fv(v3);
			*/
		}
		else {
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v2);
			
			glColor3ub(255, 255, 255);
			glVertex2fv(v2);
			glVertex2fv(v3);
		}	
	}
	glEnd();
}

static void ui_draw_but_NORMAL(uiBut *but)
{
	static GLuint displist=0;
	int a, old[8];
	GLfloat diff[4], diffn[4]={1.0f, 1.0f, 1.0f, 1.0f};
	float vec0[4]={0.0f, 0.0f, 0.0f, 0.0f};
	float dir[4], size;
	
	/* store stuff */
	glGetMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
		
	/* backdrop */
	BIF_ThemeColor(TH_BUT_NEUTRAL);
	uiSetRoundBox(15);
	gl_round_box(GL_POLYGON, but->x1, but->y1, but->x2, but->y2, 5.0f);
	
	/* sphere color */
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffn);
	glCullFace(GL_BACK); glEnable(GL_CULL_FACE);
	
	/* disable blender light */
	for(a=0; a<8; a++) {
		old[a]= glIsEnabled(GL_LIGHT0+a);
		glDisable(GL_LIGHT0+a);
	}
	
	/* own light */
	glEnable(GL_LIGHT7);
	glEnable(GL_LIGHTING);
	
	VECCOPY(dir, (float *)but->poin);
	dir[3]= 0.0f;	/* glLight needs 4 args, 0.0 is sun */
	glLightfv(GL_LIGHT7, GL_POSITION, dir); 
	glLightfv(GL_LIGHT7, GL_DIFFUSE, diffn); 
	glLightfv(GL_LIGHT7, GL_SPECULAR, vec0); 
	glLightf(GL_LIGHT7, GL_CONSTANT_ATTENUATION, 1.0f);
	glLightf(GL_LIGHT7, GL_LINEAR_ATTENUATION, 0.0f);
	
	/* transform to button */
	glPushMatrix();
	glTranslatef(but->x1 + 0.5f*(but->x2-but->x1), but->y1+ 0.5f*(but->y2-but->y1), 0.0f);
	size= (but->x2-but->x1)/200.f;
	glScalef(size, size, size);
			 
	if(displist==0) {
		GLUquadricObj	*qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
		
		qobj= gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL); 
		glShadeModel(GL_SMOOTH);
		gluSphere( qobj, 100.0, 32, 24);
		glShadeModel(GL_FLAT);
		gluDeleteQuadric(qobj);  
		
		glEndList();
	}
	else glCallList(displist);
	
	/* restore */
	glPopMatrix();
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diff); 
	
	glDisable(GL_LIGHT7);
	
	/* enable blender light */
	for(a=0; a<8; a++) {
		if(old[a])
			glEnable(GL_LIGHT0+a);
	}
}

static void ui_draw_but_curve_grid(uiBut *but, float zoomx, float zoomy, float offsx, float offsy, float step)
{
	float dx, dy, fx, fy;
	
	glBegin(GL_LINES);
	dx= step*zoomx;
	fx= but->x1 + zoomx*(-offsx);
	if(fx > but->x1) fx -= dx*( floor(fx-but->x1));
	while(fx < but->x2) {
		glVertex2f(fx, but->y1); 
		glVertex2f(fx, but->y2);
		fx+= dx;
	}
	
	dy= step*zoomy;
	fy= but->y1 + zoomy*(-offsy);
	if(fy > but->y1) fy -= dy*( floor(fy-but->y1));
	while(fy < but->y2) {
		glVertex2f(but->x1, fy); 
		glVertex2f(but->x2, fy);
		fy+= dy;
	}
	glEnd();
	
}

static void ui_draw_but_CURVE(uiBut *but)
{
	CurveMapping *cumap= (CurveMapping *)but->poin;
	CurveMap *cuma= cumap->cm+cumap->cur;
	CurveMapPoint *cmp;
	float fx, fy, dx, dy, fac[2], zoomx, zoomy, offsx, offsy;
	GLint scissor[4];
	int a;
	
	/* need scissor test, curve can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	fx= but->x1; fy= but->y1;
	ui_graphics_to_window(but->win, &fx, &fy);
	dx= but->x2; dy= but->y2;
	ui_graphics_to_window(but->win, &dx, &dy);
	glScissor((int)floor(fx), (int)floor(fy), (int)ceil(dx-fx), (int)ceil(dy-fy));
	
	/* calculate offset and zoom */
	zoomx= (but->x2-but->x1-2.0*but->aspect)/(cumap->curr.xmax - cumap->curr.xmin);
	zoomy= (but->y2-but->y1-2.0*but->aspect)/(cumap->curr.ymax - cumap->curr.ymin);
	offsx= cumap->curr.xmin-but->aspect/zoomx;
	offsy= cumap->curr.ymin-but->aspect/zoomy;
	
	/* backdrop */
	if(cumap->flag & CUMA_DO_CLIP) {
		BIF_ThemeColorShade(TH_BUT_NEUTRAL, -20);
		glRectf(but->x1, but->y1, but->x2, but->y2);
		BIF_ThemeColor(TH_BUT_NEUTRAL);
		glRectf(but->x1 + zoomx*(cumap->clipr.xmin-offsx),
				but->y1 + zoomy*(cumap->clipr.ymin-offsy),
				but->x1 + zoomx*(cumap->clipr.xmax-offsx),
				but->y1 + zoomy*(cumap->clipr.ymax-offsy));
	}
	else {
		BIF_ThemeColor(TH_BUT_NEUTRAL);
		glRectf(but->x1, but->y1, but->x2, but->y2);
	}
	
	/* grid, every .25 step */
	BIF_ThemeColorShade(TH_BUT_NEUTRAL, -16);
	ui_draw_but_curve_grid(but, zoomx, zoomy, offsx, offsy, 0.25f);
	/* grid, every 1.0 step */
	BIF_ThemeColorShade(TH_BUT_NEUTRAL, -24);
	ui_draw_but_curve_grid(but, zoomx, zoomy, offsx, offsy, 1.0f);
	/* axes */
	BIF_ThemeColorShade(TH_BUT_NEUTRAL, -50);
	glBegin(GL_LINES);
	glVertex2f(but->x1, but->y1 + zoomy*(-offsy));
	glVertex2f(but->x2, but->y1 + zoomy*(-offsy));
	glVertex2f(but->x1 + zoomx*(-offsx), but->y1);
	glVertex2f(but->x1 + zoomx*(-offsx), but->y2);
	glEnd();
	
	/* cfra option */
	if(cumap->flag & CUMA_DRAW_CFRA) {
		glColor3ub(0x60, 0xc0, 0x40);
		glBegin(GL_LINES);
		glVertex2f(but->x1 + zoomx*(cumap->sample[0]-offsx), but->y1);
		glVertex2f(but->x1 + zoomx*(cumap->sample[0]-offsx), but->y2);
		glEnd();
	}
	/* sample option */
	if(cumap->flag & CUMA_DRAW_SAMPLE) {
		if(cumap->cur==3) {
			float lum= cumap->sample[0]*0.35f + cumap->sample[1]*0.45f + cumap->sample[2]*0.2f;
			glColor3ub(240, 240, 240);
			
			glBegin(GL_LINES);
			glVertex2f(but->x1 + zoomx*(lum-offsx), but->y1);
			glVertex2f(but->x1 + zoomx*(lum-offsx), but->y2);
			glEnd();
		}
		else {
			if(cumap->cur==0)
				glColor3ub(240, 100, 100);
			else if(cumap->cur==1)
				glColor3ub(100, 240, 100);
			else
				glColor3ub(100, 100, 240);
			
			glBegin(GL_LINES);
			glVertex2f(but->x1 + zoomx*(cumap->sample[cumap->cur]-offsx), but->y1);
			glVertex2f(but->x1 + zoomx*(cumap->sample[cumap->cur]-offsx), but->y2);
			glEnd();
		}
	}
	
	/* the curve */
	BIF_ThemeColorBlend(TH_TEXT, TH_BUT_NEUTRAL, 0.35);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBegin(GL_LINE_STRIP);
	
	if(cuma->table==NULL)
		curvemapping_changed(cumap, 0);	/* 0 = no remove doubles */
	cmp= cuma->table;
	
	/* first point */
	if((cuma->flag & CUMA_EXTEND_EXTRAPOLATE)==0)
		glVertex2f(but->x1, but->y1 + zoomy*(cmp[0].y-offsy));
	else {
		fx= but->x1 + zoomx*(cmp[0].x-offsx + cuma->ext_in[0]);
		fy= but->y1 + zoomy*(cmp[0].y-offsy + cuma->ext_in[1]);
		glVertex2f(fx, fy);
	}
	for(a=0; a<=CM_TABLE; a++) {
		fx= but->x1 + zoomx*(cmp[a].x-offsx);
		fy= but->y1 + zoomy*(cmp[a].y-offsy);
		glVertex2f(fx, fy);
	}
	/* last point */
	if((cuma->flag & CUMA_EXTEND_EXTRAPOLATE)==0)
		glVertex2f(but->x2, but->y1 + zoomy*(cmp[CM_TABLE].y-offsy));	
	else {
		fx= but->x1 + zoomx*(cmp[CM_TABLE].x-offsx - cuma->ext_out[0]);
		fy= but->y1 + zoomy*(cmp[CM_TABLE].y-offsy - cuma->ext_out[1]);
		glVertex2f(fx, fy);
	}
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);

	/* the points, use aspect to make them visible on edges */
	cmp= cuma->curve;
	glPointSize(3.0f);
	bglBegin(GL_POINTS);
	for(a=0; a<cuma->totpoint; a++) {
		if(cmp[a].flag & SELECT)
			BIF_ThemeColor(TH_TEXT_HI);
		else
			BIF_ThemeColor(TH_TEXT);
		fac[0]= but->x1 + zoomx*(cmp[a].x-offsx);
		fac[1]= but->y1 + zoomy*(cmp[a].y-offsy);
		bglVertex2fv(fac);
	}
	bglEnd();
	glPointSize(1.0f);
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	/* outline */
	BIF_ThemeColor(TH_BUT_OUTLINE);
	fdrawbox(but->x1, but->y1, but->x2, but->y2);

}

static void ui_draw_roundbox(uiBut *but)
{
	glEnable(GL_BLEND);
	
	//BIF_ThemeColorShadeAlpha(TH_PANEL, but->a2, but->a2);
	BIF_ThemeColorShadeAlpha(but->themecol, but->a2, but->a2);

	uiSetRoundBox(but->a1);
	gl_round_box(GL_POLYGON, but->x1, but->y1, but->x2, but->y2, but->min);

	glDisable(GL_BLEND);
}


/* nothing! */
static void ui_draw_nothing(int type, int colorid, float asp, float x1, float y1, float x2, float y2, int flag)
{
}


/* ************** EXTERN, called from interface.c ************* */
/* ************** MAIN CALLBACK FUNCTION          ************* */

void ui_set_embossfunc(uiBut *but, int drawtype)
{
	// this aded for evaluating textcolor for example
	but->dt= drawtype;
	
	// not really part of standard minimal themes, just make sure it is set
	but->sliderfunc= ui_draw_slider;

	// standard builtin first:
	if(but->type==LABEL || but->type==ROUNDBOX) but->embossfunc= ui_draw_nothing;
	else if(but->type==PULLDOWN) but->embossfunc= ui_draw_pulldown_round;
	else if(drawtype==UI_EMBOSSM) but->embossfunc= ui_draw_minimal;
	else if(drawtype==UI_EMBOSSN) but->embossfunc= ui_draw_nothing;
	else if(drawtype==UI_EMBOSSP) but->embossfunc= ui_draw_pulldown_item;
	else if(drawtype==UI_EMBOSSR) but->embossfunc= ui_draw_round;
	else {
		int theme= BIF_GetThemeValue(TH_BUT_DRAWTYPE);
		
		switch(theme) {
		
		case TH_ROUNDED:
			but->embossfunc= ui_draw_round;
			break;
		case TH_OLDSKOOL:
			but->embossfunc= ui_draw_oldskool;
			break;
		case TH_MINIMAL:
			but->embossfunc= ui_draw_minimal;
			break;
		case TH_SHADED:
		default:
			but->embossfunc= ui_draw_default;
			but->sliderfunc= ui_default_slider;
			break;
		}
	}
	
	// note: if you want aligning, adapt the call uiBlockEndAlign in interface.c 
}

void ui_draw_but(uiBut *but)
{
	double value;
	float x1, x2, y1, y2, fac;
	
	if(but==NULL) return;

	/* signal for frontbuf flush buttons and menus, not when normal drawing */
	if(but->block->in_use) ui_block_set_flush(but->block, but);
		
	switch (but->type) {

	case NUMSLI:
	case HSVSLI:
	
		but->embossfunc(but->type, but->themecol, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);
		ui_draw_text_icon(but);

		x1= (but->x1+but->x2)/2;
		x2= but->x2 - 5.0*but->aspect;
		y1= but->y1 + 2.0*but->aspect;
		y2= but->y2 - 2.0*but->aspect;
		
		value= ui_get_but_val(but);
		fac= (value-but->min)*(x2-x1)/(but->max - but->min);
		
		but->sliderfunc(but->themecol, fac, but->aspect, x1, y1, x2, y2, but->flag);
		break;
		
	case SEPR:
		//  only background
		break;
		
	case COL:
		ui_draw_but_COL(but);  // black box with color
		break;

	case HSVCUBE:
		ui_draw_but_HSVCUBE(but);  // box for colorpicker, three types
		break;

#ifdef INTERNATIONAL
	case CHARTAB:
		value= ui_get_but_val(but);
		ui_draw_but_CHARTAB(but);
		break;
#endif

	case LINK:
	case INLINK:
		ui_draw_icon(but, but->icon, 0);
		break;
		
	case ROUNDBOX:
		ui_draw_roundbox(but);
		break;
		
	case BUT_COLORBAND:
		ui_draw_but_COLORBAND(but);
		break;
	case BUT_NORMAL:
		ui_draw_but_NORMAL(but);
		break;
	case BUT_CURVE:
		ui_draw_but_CURVE(but);
		break;
		
	default:
		but->embossfunc(but->type, but->themecol, but->aspect, but->x1, but->y1, but->x2, but->y2, but->flag);
		ui_draw_text_icon(but);
	
	}
}

void ui_dropshadow(rctf *rct, float radius, float aspect, int select)
{
	float rad;
	float a;
	char alpha= 2;
	
	glEnable(GL_BLEND);
	
	if(radius > (rct->ymax-rct->ymin-10.0f)/2.0f)
		rad= (rct->ymax-rct->ymin-10.0f)/2.0f;
	else
		rad= radius;
	
	if(select) a= 12.0f*aspect; else a= 12.0f*aspect;
	for(; a>0.0f; a-=aspect) {
		/* alpha ranges from 2 to 20 or so */
		glColor4ub(0, 0, 0, alpha);
		alpha+= 2;
		
		gl_round_box(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax-10.0f + a, rad+a);
	}
	
	/* outline emphasis */
	glEnable( GL_LINE_SMOOTH );
	glColor4ub(0, 0, 0, 100);
	gl_round_box(GL_LINE_LOOP, rct->xmin-0.5f, rct->ymin-0.5f, rct->xmax+0.5f, rct->ymax+0.5f, radius);
	glDisable( GL_LINE_SMOOTH );
	
	glDisable(GL_BLEND);
}
