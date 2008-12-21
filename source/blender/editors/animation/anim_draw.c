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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <string.h>
#include <stdio.h>

#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

/* XXX */
extern void ui_rasterpos_safe(float x, float y, float aspect);

/* *************************************************** */
/* CURRENT FRAME DRAWING */

/* Draw current frame number in a little green box beside the current frame indicator */
static void draw_cfra_number (View2D *v2d, float cfra, short time)
{
	float xscale, yscale, x, y;
	char str[32];
	short slen;
	
	/* because the frame number text is subject to the same scaling as the contents of the view */
	UI_view2d_getscale(v2d, &xscale, &yscale);
	glScalef(1.0/xscale, 1.0, 1.0);
	
	if (time) 
		sprintf(str, "   %.2f", FRA2TIME(CFRA));
	else 
		sprintf(str, "   %d", CFRA);
	slen= UI_GetStringWidth(G.font, str, 0) - 1;
	
	/* get starting coordinates for drawing */
	x= cfra * xscale;
	y= 18;
	
	/* draw green box around/behind text */
	UI_ThemeColorShadeAlpha(TH_CFRAME, 0, -100);
	glRectf(x, y,  x+slen,  y+15);
	
	/* draw current frame number - black text */
	UI_ThemeColor(TH_TEXT);
	ui_rasterpos_safe(x-5, y+3, 1.0);
	UI_DrawString(G.fonts, str, 0); // XXX may need to be updated for font stuff
	
	/* restore view transform */
	glScalef(xscale, 1.0, 1.0);
}

/* General call for drawing current frame indicator in a */
void ANIM_draw_cfra (const bContext *C, View2D *v2d, short flag)
{
	Scene *scene= CTX_data_scene(C);
	float vec[2];
	
	/* Draw a light green line to indicate current frame */
	vec[0]= (float)(scene->r.cfra * scene->r.framelen);
	
	UI_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);
	
	glBegin(GL_LINE_STRIP);
		vec[1]= v2d->cur.ymin;
		glVertex2fv(vec);
		
		vec[1]= v2d->cur.ymax;
		glVertex2fv(vec);
	glEnd();
	
	/* Draw dark green line if slow-parenting/time-offset is enabled */
	if (flag & DRAWCFRA_SHOW_TIMEOFS) {
		Object *ob= (scene->basact) ? (scene->basact->object) : 0;
		if ((ob) && (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0)) {
			vec[0]-= give_timeoffset(ob); /* could avoid calling twice */
			
			UI_ThemeColorShade(TH_CFRAME, -30);
			
			glBegin(GL_LINE_STRIP);
				/*vec[1]= v2d->cur.ymax;*/ // this is set already. this line is only included
				glVertex2fv(vec);
				
				vec[1]= v2d->cur.ymin;
				glVertex2fv(vec);
			glEnd();
		}
	}
	
	glLineWidth(1.0);
	
	/* Draw current frame number in a little box */
	if (flag & DRAWCFRA_SHOW_NUMBOX) {
		UI_view2d_view_orthoSpecial(C, v2d, 1);
		draw_cfra_number(v2d, vec[0], (flag & DRAWCFRA_UNIT_SECONDS));
	}
}

/* *************************************************** */
/* PREVIEW RANGE 'CURTAINS' */

/* Draw preview range 'curtains' for highlighting where the animation data is */
void ANIM_draw_previewrange (const bContext *C, View2D *v2d)
{
	Scene *scene= CTX_data_scene(C);
	
	/* only draw this if preview range is set */
	if (scene->r.psfra) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
		
		/* only draw two separate 'curtains' if there's no overlap between them */
		if (PSFRA < PEFRA) {
			glRectf(v2d->cur.xmin, v2d->cur.ymin, PSFRA, v2d->cur.ymax);
			glRectf(PEFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);	
		} 
		else {
			glRectf(v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
		}
		
		glDisable(GL_BLEND);
	}
}

/* *************************************************** */
