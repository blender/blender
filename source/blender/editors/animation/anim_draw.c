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
#include <math.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* XXX */
extern void ui_rasterpos_safe(float x, float y, float aspect);

/* *************************************************** */
/* CURRENT FRAME DRAWING */

/* Draw current frame number in a little green box beside the current frame indicator */
static void draw_cfra_number (Scene *scene, View2D *v2d, float cfra, short time)
{
	float xscale, yscale, x, y;
	char str[32];
	short slen;
	
	/* because the frame number text is subject to the same scaling as the contents of the view */
	UI_view2d_getscale(v2d, &xscale, &yscale);
	glScalef(1.0f/xscale, 1.0f, 1.0f);
	
	if (time) {
		/* Timecode:
		 *	- In general, minutes and seconds should be shown, as most clips will be
		 *	  within this length. Hours will only be included if relevant.
		 *	- Only show frames when zoomed in enough for them to be relevant 
		 *	  (using separator of '!' for frames).
		 *	  When showing frames, use slightly different display to avoid confusion with mm:ss format
		 * TODO: factor into reusable function.
		 * Meanwhile keep in sync:
		 *	  source/blender/editors/animation/anim_draw.c
		 *	  source/blender/editors/interface/view2d.c
		 */
		float val= FRA2TIME(CFRA);
		int hours=0, minutes=0, seconds=0, frames=0;
		char neg[2]= "";
		
		/* get values */
		if (val < 0) {
			/* correction for negative values */
			sprintf(neg, "-");
			val = -val;
		}
		if (val >= 3600) {
			/* hours */
			/* XXX should we only display a single digit for hours since clips are 
			 * 	   VERY UNLIKELY to be more than 1-2 hours max? However, that would 
			 *	   go against conventions...
			 */
			hours= (int)val / 3600;
			val= (float)fmod(val, 3600);
		}
		if (val >= 60) {
			/* minutes */
			minutes= (int)val / 60;
			val= (float)fmod(val, 60);
		}
		{
			/* seconds + frames
			 *	Frames are derived from 'fraction' of second. We need to perform some additional rounding
			 *	to cope with 'half' frames, etc., which should be fine in most cases
			 */
			seconds= (int)val;
			frames= (int)floor( ((val - seconds) * FPS) + 0.5f );
		}
		
		/* print timecode to temp string buffer */
		if (hours) sprintf(str, "   %s%02d:%02d:%02d!%02d", neg, hours, minutes, seconds, frames);
		else if (minutes) sprintf(str, "   %s%02d:%02d!%02d", neg, minutes, seconds, frames);
		else sprintf(str, "   %s%d!%02d", neg, seconds, frames);
	}
	else 
		sprintf(str, "   %d", CFRA);
	slen= (short)UI_GetStringWidth(str) - 1;
	
	/* get starting coordinates for drawing */
	x= cfra * xscale;
	y= 18;
	
	/* draw green box around/behind text */
	UI_ThemeColorShadeAlpha(TH_CFRAME, 0, -100);
	glRectf(x, y,  x+slen,  y+15);
	
	/* draw current frame number - black text */
	UI_ThemeColor(TH_TEXT);
	UI_DrawString(x-5, y+3, str); // XXX may need to be updated for font stuff
	
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
		vec[1]= v2d->cur.ymin-500.0f;	/* XXX arbitrary... want it go to bottom */
		glVertex2fv(vec);
		
		vec[1]= v2d->cur.ymax;
		glVertex2fv(vec);
	glEnd();
	
	/* Draw dark green line if slow-parenting/time-offset is enabled */
	if (flag & DRAWCFRA_SHOW_TIMEOFS) {
		Object *ob= (scene->basact) ? (scene->basact->object) : 0;
		
		// XXX ob->ipoflag is depreceated!
		if ((ob) && (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0f)) {
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
		draw_cfra_number(scene, v2d, vec[0], (flag & DRAWCFRA_UNIT_SECONDS));
	}
}

/* *************************************************** */
/* PREVIEW RANGE 'CURTAINS' */
/* Note: 'Preview Range' tools are defined in anim_ops.c */

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
			glRectf(v2d->cur.xmin, v2d->cur.ymin, (float)PSFRA, v2d->cur.ymax);
			glRectf((float)PEFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);	
		} 
		else {
			glRectf(v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
		}
		
		glDisable(GL_BLEND);
	}
}

/* *************************************************** */
/* NLA-MAPPING UTILITIES (required for drawing and also editing keyframes)  */

/* Obtain the AnimData block providing NLA-mapping for the given channel (if applicable) */
AnimData *ANIM_nla_mapping_get(bAnimContext *ac, bAnimListElem *ale)
{
	/* sanity checks */
	if (ac == NULL)
		return NULL;
	
	/* handling depends on the type of animation-context we've got */
	if (ale)
		return ale->adt;
	else
		return NULL;
}

/* ------------------- */

/* helper function for ANIM_nla_mapping_apply_fcurve() -> "restore", i.e. mapping points back to action-time */
static short bezt_nlamapping_restore(BeztEditData *bed, BezTriple *bezt)
{
	/* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
	AnimData *adt= (AnimData *)bed->data;
	short only_keys= (short)bed->i1;
	
	/* adjust BezTriple handles only if allowed to */
	if (only_keys == 0) {
		bezt->vec[0][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_UNMAP);
		bezt->vec[2][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_UNMAP);
	}
	
	bezt->vec[1][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_UNMAP);
	
	return 0;
}

/* helper function for ANIM_nla_mapping_apply_fcurve() -> "apply", i.e. mapping points to NLA-mapped global time */
static short bezt_nlamapping_apply(BeztEditData *bed, BezTriple *bezt)
{
	/* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
	AnimData *adt= (AnimData *)bed->data;
	short only_keys= (short)bed->i1;
	
	/* adjust BezTriple handles only if allowed to */
	if (only_keys == 0) {
		bezt->vec[0][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_MAP);
		bezt->vec[2][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_MAP);
	}
	
	bezt->vec[1][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_MAP);
	
	return 0;
}



/* Apply/Unapply NLA mapping to all keyframes in the nominated F-Curve 
 *	- restore = whether to map points back to non-mapped time 
 * 	- only_keys = whether to only adjust the location of the center point of beztriples
 */
void ANIM_nla_mapping_apply_fcurve (AnimData *adt, FCurve *fcu, short restore, short only_keys)
{
	BeztEditData bed;
	BeztEditFunc map_cb;
	
	/* init edit data 
	 *	- AnimData is stored in 'data'
	 *	- only_keys is stored in 'i1'
	 */
	memset(&bed, 0, sizeof(BeztEditData));
	bed.data= (void *)adt;
	bed.i1= (int)only_keys;
	
	/* get editing callback */
	if (restore)
		map_cb= bezt_nlamapping_restore;
	else
		map_cb= bezt_nlamapping_apply;
	
	/* apply to F-Curve */
	ANIM_fcurve_keys_bezier_loop(&bed, fcu, NULL, map_cb, NULL);
} 

/* *************************************************** */
