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

#include "BLI_sys_types.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_timecode.h"

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
/* CURRENT FRAME DRAWING */

/* Draw current frame number in a little green box beside the current frame indicator */
static void draw_cfra_number(Scene *scene, View2D *v2d, const float cfra, const bool time)
{
	float xscale, yscale, x, y;
	char numstr[32] = "    t";  /* t is the character to start replacing from */
	short slen;
	
	/* because the frame number text is subject to the same scaling as the contents of the view */
	UI_view2d_scale_get(v2d, &xscale, &yscale);
	glScalef(1.0f / xscale, 1.0f, 1.0f);
	
	/* get timecode string 
	 *	- padding on str-buf passed so that it doesn't sit on the frame indicator
	 *	- power = 0, gives 'standard' behavior for time
	 *	  but power = 1 is required for frames (to get integer frames)
	 */
	if (time) {
		BLI_timecode_string_from_time(&numstr[4], sizeof(numstr) - 4, 0, FRA2TIME(cfra), FPS, U.timecode_style);
	}
	else {
		BLI_timecode_string_from_time_simple(&numstr[4], sizeof(numstr) - 4, 1, cfra);
	}
	slen = (short)UI_fontstyle_string_width(numstr) - 1;
	
	/* get starting coordinates for drawing */
	x = cfra * xscale;
	y = 0.9f * U.widget_unit;
	
	/* draw green box around/behind text */
	UI_ThemeColorShade(TH_CFRAME, 0);
	glRectf(x, y,  x + slen,  y + 0.75f * U.widget_unit);
	
	/* draw current frame number - black text */
	UI_ThemeColor(TH_TEXT);
	UI_draw_string(x - 0.25f * U.widget_unit, y + 0.15f * U.widget_unit, numstr);
	
	/* restore view transform */
	glScalef(xscale, 1.0, 1.0);
}

/* General call for drawing current frame indicator in animation editor */
void ANIM_draw_cfra(const bContext *C, View2D *v2d, short flag)
{
	Scene *scene = CTX_data_scene(C);
	float vec[2];
	
	/* Draw a light green line to indicate current frame */
	vec[0] = (float)(scene->r.cfra * scene->r.framelen);
	
	UI_ThemeColor(TH_CFRAME);
	if (flag & DRAWCFRA_WIDE)
		glLineWidth(3.0);
	else
		glLineWidth(2.0);
	
	glBegin(GL_LINE_STRIP);
	vec[1] = v2d->cur.ymin - 500.0f;    /* XXX arbitrary... want it go to bottom */
	glVertex2fv(vec);
		
	vec[1] = v2d->cur.ymax;
	glVertex2fv(vec);
	glEnd();
	
	glLineWidth(1.0);
	
	/* Draw current frame number in a little box */
	if (flag & DRAWCFRA_SHOW_NUMBOX) {
		UI_view2d_view_orthoSpecial(CTX_wm_region(C), v2d, 1);
		draw_cfra_number(scene, v2d, vec[0], (flag & DRAWCFRA_UNIT_SECONDS) != 0);
	}
}

/* *************************************************** */
/* PREVIEW RANGE 'CURTAINS' */
/* Note: 'Preview Range' tools are defined in anim_ops.c */

/* Draw preview range 'curtains' for highlighting where the animation data is */
void ANIM_draw_previewrange(const bContext *C, View2D *v2d, int end_frame_width)
{
	Scene *scene = CTX_data_scene(C);
	
	/* only draw this if preview range is set */
	if (PRVRANGEON) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
		
		/* only draw two separate 'curtains' if there's no overlap between them */
		if (PSFRA < PEFRA + end_frame_width) {
			glRectf(v2d->cur.xmin, v2d->cur.ymin, (float)PSFRA, v2d->cur.ymax);
			glRectf((float)(PEFRA + end_frame_width), v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
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
	if (G.is_rendering) return NULL;
	
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
	AnimData *adt = (AnimData *)ked->data;
	short only_keys = (short)ked->i1;
	
	/* adjust BezTriple handles only if allowed to */
	if (only_keys == 0) {
		bezt->vec[0][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_UNMAP);
		bezt->vec[2][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_UNMAP);
	}
	
	bezt->vec[1][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_UNMAP);
	
	return 0;
}

/* helper function for ANIM_nla_mapping_apply_fcurve() -> "apply", i.e. mapping points to NLA-mapped global time */
static short bezt_nlamapping_apply(KeyframeEditData *ked, BezTriple *bezt)
{
	/* AnimData block providing scaling is stored in 'data', only_keys option is stored in i1 */
	AnimData *adt = (AnimData *)ked->data;
	short only_keys = (short)ked->i1;
	
	/* adjust BezTriple handles only if allowed to */
	if (only_keys == 0) {
		bezt->vec[0][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[0][0], NLATIME_CONVERT_MAP);
		bezt->vec[2][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[2][0], NLATIME_CONVERT_MAP);
	}
	
	bezt->vec[1][0] = BKE_nla_tweakedit_remap(adt, bezt->vec[1][0], NLATIME_CONVERT_MAP);
	
	return 0;
}


/* Apply/Unapply NLA mapping to all keyframes in the nominated F-Curve 
 *	- restore = whether to map points back to non-mapped time 
 *  - only_keys = whether to only adjust the location of the center point of beztriples
 */
void ANIM_nla_mapping_apply_fcurve(AnimData *adt, FCurve *fcu, bool restore, bool only_keys)
{
	KeyframeEditData ked = {{NULL}};
	KeyframeEditFunc map_cb;
	
	/* init edit data 
	 *	- AnimData is stored in 'data'
	 *	- only_keys is stored in 'i1'
	 */
	ked.data = (void *)adt;
	ked.i1 = (int)only_keys;
	
	/* get editing callback */
	if (restore)
		map_cb = bezt_nlamapping_restore;
	else
		map_cb = bezt_nlamapping_apply;
	
	/* apply to F-Curve */
	ANIM_fcurve_keyframes_loop(&ked, fcu, NULL, map_cb, NULL);
}

/* *************************************************** */
/* UNITS CONVERSION MAPPING (required for drawing and editing keyframes) */

/* Get flags used for normalization in ANIM_unit_mapping_get_factor. */
short ANIM_get_normalization_flags(bAnimContext *ac)
{
	if (ac->sl->spacetype == SPACE_IPO) {
		SpaceIpo *sipo = (SpaceIpo *) ac->sl;
		bool use_normalization = (sipo->flag & SIPO_NORMALIZE) != 0;
		bool freeze_normalization = (sipo->flag & SIPO_NORMALIZE_FREEZE) != 0;
		return use_normalization
		    ? (ANIM_UNITCONV_NORMALIZE |  (freeze_normalization ? ANIM_UNITCONV_NORMALIZE_FREEZE : 0))
		    : 0;
	}

	return 0;
}

static float normalzation_factor_get(FCurve *fcu, short flag)
{
	float factor = 1.0f;

	if (flag & ANIM_UNITCONV_RESTORE) {
		return 1.0f / fcu->prev_norm_factor;
	}

	if (flag & ANIM_UNITCONV_NORMALIZE_FREEZE) {
		return fcu->prev_norm_factor;
	}

	if (G.moving & G_TRANSFORM_FCURVES) {
		return fcu->prev_norm_factor;
	}

	fcu->prev_norm_factor = 1.0f;
	if (fcu->bezt) {
		BezTriple *bezt;
		int i;
		float max_coord = -FLT_MAX;

		if (fcu->totvert < 1) {
			return 1.0f;
		}

		for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
			max_coord = max_ff(max_coord, fabsf(bezt->vec[0][1]));
			max_coord = max_ff(max_coord, fabsf(bezt->vec[1][1]));
			max_coord = max_ff(max_coord, fabsf(bezt->vec[2][1]));
		}

		if (max_coord > FLT_EPSILON) {
			factor = 1.0f / max_coord;
		}
	}
	fcu->prev_norm_factor = factor;
	return factor;
}

/* Get unit conversion factor for given ID + F-Curve */
float ANIM_unit_mapping_get_factor(Scene *scene, ID *id, FCurve *fcu, short flag)
{
	if (flag & ANIM_UNITCONV_NORMALIZE) {
		return normalzation_factor_get(fcu, flag);
	}

	/* sanity checks */
	if (id && fcu && fcu->rna_path) {
		PointerRNA ptr, id_ptr;
		PropertyRNA *prop;
		
		/* get RNA property that F-Curve affects */
		RNA_id_pointer_create(id, &id_ptr);
		if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
			/* rotations: radians <-> degrees? */
			if (RNA_SUBTYPE_UNIT(RNA_property_subtype(prop)) == PROP_UNIT_ROTATION) {
				/* if the radians flag is not set, default to using degrees which need conversions */
				if ((scene) && (scene->unit.system_rotation == USER_UNIT_ROT_RADIANS) == 0) {
					if (flag & ANIM_UNITCONV_RESTORE)
						return DEG2RADF(1.0f);  /* degrees to radians */
					else
						return RAD2DEGF(1.0f);  /* radians to degrees */
				}
			}
			
			/* TODO: other rotation types here as necessary */
		}
	}

	/* no mapping needs to occur... */
	return 1.0f;
}

/* *************************************************** */
