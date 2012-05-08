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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_draw.c
 *  \ingroup edanimation
 */

#include "BLO_sys_types.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_nla.h"
#include "BKE_object.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* *************************************************** */
/* TIME CODE FORMATTING */

/* Generate timecode/frame number string and store in the supplied string 
 * 	- buffer: must be at least 13 chars long 
 *	- power: special setting for View2D grid drawing, 
 *	  used to specify how detailed we need to be
 *	- timecodes: boolean specifying whether timecodes or
 *	  frame numbers get drawn
 *	- cfra: time in frames or seconds, consistent with the values shown by timecodes
 */
// TODO: have this in kernel instead under scene?
void ANIM_timecode_string_from_frame (char *str, Scene *scene, int power, short timecodes, float cfra)
{
	if (timecodes) {
		int hours=0, minutes=0, seconds=0, frames=0;
		float raw_seconds= cfra;
		char neg[2]= {'\0'};
		
		/* get cframes */
		if (cfra < 0) {
			/* correction for negative cfraues */
			neg[0]= '-';
			cfra = -cfra;
		}
		if (cfra >= 3600) {
			/* hours */
			/* XXX should we only display a single digit for hours since clips are 
			 * 	   VERY UNLIKELY to be more than 1-2 hours max? However, that would 
			 *	   go against conventions...
			 */
			hours= (int)cfra / 3600;
			cfra= (float)fmod(cfra, 3600);
		}
		if (cfra >= 60) {
			/* minutes */
			minutes= (int)cfra / 60;
			cfra= (float)fmod(cfra, 60);
		}
		if (power <= 0) {
			/* seconds + frames
			 *	Frames are derived from 'fraction' of second. We need to perform some additional rounding
			 *	to cope with 'half' frames, etc., which should be fine in most cases
			 */
			seconds= (int)cfra;
			frames= (int)floor( (((double)cfra - (double)seconds) * FPS) + 0.5 );
		}
		else {
			/* seconds (with pixel offset rounding) */
			seconds= (int)floor(cfra + 0.375f);
		}
		
		switch (U.timecode_style) {
			case USER_TIMECODE_MINIMAL: 
			{
				/*	- In general, minutes and seconds should be shown, as most clips will be
				 *	  within this length. Hours will only be included if relevant.
				 *	- Only show frames when zoomed in enough for them to be relevant 
				 *	  (using separator of '+' for frames).
				 *	  When showing frames, use slightly different display to avoid confusion with mm:ss format
				 */
				if (power <= 0) {
					/* include "frames" in display */
					if (hours) sprintf(str, "%s%02d:%02d:%02d+%02d", neg, hours, minutes, seconds, frames);
					else if (minutes) sprintf(str, "%s%02d:%02d+%02d", neg, minutes, seconds, frames);
					else sprintf(str, "%s%d+%02d", neg, seconds, frames);
				}
				else {
					/* don't include 'frames' in display */
					if (hours) sprintf(str, "%s%02d:%02d:%02d", neg, hours, minutes, seconds);
					else sprintf(str, "%s%02d:%02d", neg, minutes, seconds);
				}
			}
				break;
				
			case USER_TIMECODE_SMPTE_MSF:
			{
				/* reduced SMPTE format that always shows minutes, seconds, frames. Hours only shown as needed. */
				if (hours) sprintf(str, "%s%02d:%02d:%02d:%02d", neg, hours, minutes, seconds, frames);
				else sprintf(str, "%s%02d:%02d:%02d", neg, minutes, seconds, frames);
			}
				break;
			
			case USER_TIMECODE_MILLISECONDS:
			{
				/* reduced SMPTE. Instead of frames, milliseconds are shown */
				int ms_dp= (power <= 0) ? (1 - power) : 1; /* precision of decimal part */
				int s_pad= ms_dp+3;	/* to get 2 digit whole-number part for seconds display (i.e. 3 is for 2 digits + radix, on top of full length) */
				
				if (hours) sprintf(str, "%s%02d:%02d:%0*.*f", neg, hours, minutes, s_pad, ms_dp, cfra);
				else sprintf(str, "%s%02d:%0*.*f", neg, minutes, s_pad,  ms_dp, cfra);
			}
				break;
				
			case USER_TIMECODE_SECONDS_ONLY:
			{
				/* only show the original seconds display */
				/* round to whole numbers if power is >= 1 (i.e. scale is coarse) */
				if (power <= 0) sprintf(str, "%.*f", 1-power, raw_seconds);
				else sprintf(str, "%d", (int)floor(raw_seconds + 0.375f));
			}
				break;
			
			case USER_TIMECODE_SMPTE_FULL:
			default:
			{
				/* full SMPTE format */
				sprintf(str, "%s%02d:%02d:%02d:%02d", neg, hours, minutes, seconds, frames);
			}
				break;
		}
	}
	else {
		/* round to whole numbers if power is >= 1 (i.e. scale is coarse) */
		if (power <= 0) sprintf(str, "%.*f", 1-power, cfra);
		else sprintf(str, "%d", (int)floor(cfra + 0.375f));
	}
} 

/* *************************************************** */
/* CURRENT FRAME DRAWING */

/* Draw current frame number in a little green box beside the current frame indicator */
static void draw_cfra_number (Scene *scene, View2D *v2d, float cfra, short time)
{
	float xscale, yscale, x, y;
	char numstr[32] = "    t";	/* t is the character to start replacing from */
	short slen;
	
	/* because the frame number text is subject to the same scaling as the contents of the view */
	UI_view2d_getscale(v2d, &xscale, &yscale);
	glScalef(1.0f/xscale, 1.0f, 1.0f);
	
	/* get timecode string 
	 *	- padding on str-buf passed so that it doesn't sit on the frame indicator
	 *	- power = 0, gives 'standard' behavior for time
	 *	  but power = 1 is required for frames (to get integer frames)
	 */
	if (time)
		ANIM_timecode_string_from_frame(&numstr[4], scene, 0, time, FRA2TIME(cfra));
	else	
		ANIM_timecode_string_from_frame(&numstr[4], scene, 1, time, cfra);
	slen= (short)UI_GetStringWidth(numstr) - 1;
	
	/* get starting coordinates for drawing */
	x= cfra * xscale;
	y= 18;
	
	/* draw green box around/behind text */
	UI_ThemeColorShade(TH_CFRAME, 0);
	glRectf(x, y,  x+slen,  y+15);
	
	/* draw current frame number - black text */
	UI_ThemeColor(TH_TEXT);
	UI_DrawString(x-5, y+3, numstr);
	
	/* restore view transform */
	glScalef(xscale, 1.0, 1.0);
}

/* General call for drawing current frame indicator in animation editor */
void ANIM_draw_cfra (const bContext *C, View2D *v2d, short flag)
{
	Scene *scene= CTX_data_scene(C);
	float vec[2];
	
	/* Draw a light green line to indicate current frame */
	vec[0]= (float)(scene->r.cfra * scene->r.framelen);
	
	UI_ThemeColor(TH_CFRAME);
	if (flag & DRAWCFRA_WIDE)
		glLineWidth(3.0);
	else
		glLineWidth(2.0);
	
	glBegin(GL_LINE_STRIP);
		vec[1]= v2d->cur.ymin-500.0f;	/* XXX arbitrary... want it go to bottom */
		glVertex2fv(vec);
		
		vec[1]= v2d->cur.ymax;
		glVertex2fv(vec);
	glEnd();
	
	glLineWidth(1.0);
	
	/* Draw current frame number in a little box */
	if (flag & DRAWCFRA_SHOW_NUMBOX) {
		UI_view2d_view_orthoSpecial(CTX_wm_region(C), v2d, 1);
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
	if (PRVRANGEON) {
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
// TODO: do not supply return this if the animdata tells us that there is no mapping to perform
AnimData *ANIM_nla_mapping_get(bAnimContext *ac, bAnimListElem *ale)
{
	/* sanity checks */
	if (ac == NULL)
		return NULL;
	
	/* abort if rendering - we may get some race condition issues... */
	if (G.rendering) return NULL;
	
	/* handling depends on the type of animation-context we've got */
	if (ale)
		return ale->adt;
	else
		return NULL;
}

/* ------------------- */

/* helper function for ANIM_nla_mapping_apply_fcurve() -> "restore", i.e. mapping points back to action-time */
static short bezt_nlamapping_restore(KeyframeEditData *ked, BezTriple *bezt)
{
	/* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
	AnimData *adt= (AnimData *)ked->data;
	short only_keys= (short)ked->i1;
	
	/* adjust BezTriple handles only if allowed to */
	if (only_keys == 0) {
		bezt->vec[0][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_UNMAP);
		bezt->vec[2][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_UNMAP);
	}
	
	bezt->vec[1][0]= BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_UNMAP);
	
	return 0;
}

/* helper function for ANIM_nla_mapping_apply_fcurve() -> "apply", i.e. mapping points to NLA-mapped global time */
static short bezt_nlamapping_apply(KeyframeEditData *ked, BezTriple *bezt)
{
	/* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
	AnimData *adt= (AnimData*)ked->data;
	short only_keys= (short)ked->i1;
	
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
	KeyframeEditData ked= {{NULL}};
	KeyframeEditFunc map_cb;
	
	/* init edit data 
	 *	- AnimData is stored in 'data'
	 *	- only_keys is stored in 'i1'
	 */
	ked.data= (void *)adt;
	ked.i1= (int)only_keys;
	
	/* get editing callback */
	if (restore)
		map_cb= bezt_nlamapping_restore;
	else
		map_cb= bezt_nlamapping_apply;
	
	/* apply to F-Curve */
	ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, map_cb, NULL);
}

/* *************************************************** */
/* UNITS CONVERSION MAPPING (required for drawing and editing keyframes) */

/* Get unit conversion factor for given ID + F-Curve */
float ANIM_unit_mapping_get_factor (Scene *scene, ID *id, FCurve *fcu, short restore)
{
	/* sanity checks */
	if (id && fcu && fcu->rna_path) {
		PointerRNA ptr, id_ptr;
		PropertyRNA *prop;
		
		/* get RNA property that F-Curve affects */
		RNA_id_pointer_create(id, &id_ptr);
		if (RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop)) {
			/* rotations: radians <-> degrees? */
			if (RNA_SUBTYPE_UNIT(RNA_property_subtype(prop)) == PROP_UNIT_ROTATION) {
				/* if the radians flag is not set, default to using degrees which need conversions */
				if ((scene) && (scene->unit.system_rotation == USER_UNIT_ROT_RADIANS) == 0) {
					if (restore)
						return DEG2RADF(1.0f);	/* degrees to radians */
					else
						return RAD2DEGF(1.0f);	/* radians to degrees */
				}
			}
			
			// TODO: other rotation types here as necessary
		}
	}
	
	/* no mapping needs to occur... */
	return 1.0f;
}

/* ----------------------- */

/* helper function for ANIM_unit_mapping_apply_fcurve -> mapping callback for unit mapping */
static short bezt_unit_mapping_apply (KeyframeEditData *ked, BezTriple *bezt)
{
	/* mapping factor is stored in f1, flags are stored in i1 */
	short only_keys= (ked->i1 & ANIM_UNITCONV_ONLYKEYS);
	short sel_vs= (ked->i1 & ANIM_UNITCONV_SELVERTS);
	float fac= ked->f1;
	
	/* adjust BezTriple handles only if allowed to */
	if (only_keys == 0) {
		if ((sel_vs==0) || (bezt->f1 & SELECT)) 
			bezt->vec[0][1] *= fac;
		if ((sel_vs==0) || (bezt->f3 & SELECT)) 
			bezt->vec[2][1] *= fac;
	}
	
	if ((sel_vs == 0) || (bezt->f2 & SELECT))
		bezt->vec[1][1] *= fac;
	
	return 0;
}

/* Apply/Unapply units conversions to keyframes */
void ANIM_unit_mapping_apply_fcurve (Scene *scene, ID *id, FCurve *fcu, short flag)
{
	KeyframeEditData ked;
	KeyframeEditFunc sel_cb;
	float fac;
	
	/* abort if rendering - we may get some race condition issues... */
	if (G.rendering) return;
	
	/* calculate mapping factor, and abort if nothing to change */
	fac= ANIM_unit_mapping_get_factor(scene, id, fcu, (flag & ANIM_UNITCONV_RESTORE));
	if (fac == 1.0f)
		return;
	
	/* init edit data 
	 *	- mapping factor is stored in f1
	 *	- flags are stored in 'i1'
	 */
	memset(&ked, 0, sizeof(KeyframeEditData));
	ked.f1= (float)fac;
	ked.i1= (int)flag;
	
	/* only selected? */
	if (flag & ANIM_UNITCONV_ONLYSEL)
		sel_cb= ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	else
		sel_cb= NULL;
	
	/* apply to F-Curve */
	ANIM_fcurve_keyframes_loop(&ked, fcu, sel_cb, bezt_unit_mapping_apply, NULL);
	
	// FIXME: loop here for samples should be generalised
	// TODO: only sel?
	if (fcu->fpt) {
		FPoint *fpt;
		unsigned int i;
		
		for (i=0, fpt=fcu->fpt; i < fcu->totvert; i++, fpt++) {
			/* apply unit mapping */
			fpt->vec[1] *= fac;
		}
	}
}

/* *************************************************** */
