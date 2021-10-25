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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_paint.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_rand.h"
#include "BLI_math_geom.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_gpencil.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"
#include "BKE_screen.h"
#include "BKE_tracking.h"
#include "BKE_colortools.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_brush_types.h"
#include "DNA_windowmanager_types.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_clip.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "gpencil_intern.h"

/* ******************************************* */
/* 'Globals' and Defines */

/* values for tGPsdata->status */
typedef enum eGPencil_PaintStatus {
	GP_STATUS_IDLING = 0,   /* stroke isn't in progress yet */
	GP_STATUS_PAINTING,     /* a stroke is in progress */
	GP_STATUS_ERROR,        /* something wasn't correctly set up */
	GP_STATUS_DONE          /* painting done */
} eGPencil_PaintStatus;

/* Return flags for adding points to stroke buffer */
typedef enum eGP_StrokeAdd_Result {
	GP_STROKEADD_INVALID    = -2,       /* error occurred - insufficient info to do so */
	GP_STROKEADD_OVERFLOW   = -1,       /* error occurred - cannot fit any more points */
	GP_STROKEADD_NORMAL,                /* point was successfully added */
	GP_STROKEADD_FULL                   /* cannot add any more points to buffer */
} eGP_StrokeAdd_Result;

/* Runtime flags */
typedef enum eGPencil_PaintFlags {
	GP_PAINTFLAG_FIRSTRUN       = (1 << 0),    /* operator just started */
	GP_PAINTFLAG_STROKEADDED    = (1 << 1),
	GP_PAINTFLAG_V3D_ERASER_DEPTH = (1 << 2),
	GP_PAINTFLAG_SELECTMASK     = (1 << 3),
} eGPencil_PaintFlags;


/* Temporary 'Stroke' Operation data
 *   "p" = op->customdata
 */
typedef struct tGPsdata {
	Scene *scene;       /* current scene from context */
	
	wmWindow *win;      /* window where painting originated */
	ScrArea *sa;        /* area where painting originated */
	ARegion *ar;        /* region where painting originated */
	View2D *v2d;        /* needed for GP_STROKE_2DSPACE */
	rctf *subrect;      /* for using the camera rect within the 3d view */
	rctf subrect_data;
	
	GP_SpaceConversion gsc; /* settings to pass to gp_points_to_xy() */
	
	PointerRNA ownerPtr; /* pointer to owner of gp-datablock */
	bGPdata *gpd;       /* gp-datablock layer comes from */
	bGPDlayer *gpl;     /* layer we're working on */
	bGPDframe *gpf;     /* frame we're working on */
	
	char *align_flag;   /* projection-mode flags (toolsettings - eGPencil_Placement_Flags) */
	
	eGPencil_PaintStatus status;     /* current status of painting */
	eGPencil_PaintModes  paintmode;  /* mode for painting */
	eGPencil_PaintFlags  flags;      /* flags that can get set during runtime (eGPencil_PaintFlags) */
	
	short radius;       /* radius of influence for eraser */
	
	int mval[2];        /* current mouse-position */
	int mvalo[2];       /* previous recorded mouse-position */
	
	float pressure;     /* current stylus pressure */
	float opressure;    /* previous stylus pressure */
	
	/* These need to be doubles, as (at least under unix) they are in seconds since epoch,
	 * float (and its 7 digits precision) is definitively not enough here!
	 * double, with its 15 digits precision, ensures us millisecond precision for a few centuries at least.
	 */
	double inittime;    /* Used when converting to path */
	double curtime;     /* Used when converting to path */
	double ocurtime;    /* Used when converting to path */
	
	float imat[4][4];   /* inverted transformation matrix applying when converting coords from screen-space
	                     * to region space */
	float mat[4][4];
	
	float custom_color[4]; /* custom color - hack for enforcing a particular color for track/mask editing */
	
	void *erasercursor; /* radial cursor data for drawing eraser */

	bGPDpalettecolor *palettecolor; /* current palette color */
	bGPDbrush *brush; /* current drawing brush */
	short straight[2];   /* 1: line horizontal, 2: line vertical, other: not defined, second element position */
	int lock_axis;       /* lock drawing to one axis */

	short keymodifier;   /* key used for invoking the operator */
} tGPsdata;

/* ------ */

/* Macros for accessing sensitivity thresholds... */
/* minimum number of pixels mouse should move before new point created */
#define MIN_MANHATTEN_PX    (U.gp_manhattendist)
/* minimum length of new segment before new point can be added */
#define MIN_EUCLIDEAN_PX    (U.gp_euclideandist)

static bool gp_stroke_added_check(tGPsdata *p)
{
	return (p->gpf && p->gpf->strokes.last && p->flags & GP_PAINTFLAG_STROKEADDED);
}

static void gp_stroke_added_enable(tGPsdata *p)
{
	BLI_assert(p->gpf->strokes.last != NULL);
	p->flags |= GP_PAINTFLAG_STROKEADDED;
}

/* ------ */
/* Forward defines for some functions... */

static void gp_session_validatebuffer(tGPsdata *p);

/* ******************************************* */
/* Context Wrangling... */

/* check if context is suitable for drawing */
static int gpencil_draw_poll(bContext *C)
{
	if (ED_operator_regionactive(C)) {
		/* check if current context can support GPencil data */
		if (ED_gpencil_data_get_pointers(C, NULL) != NULL) {
			/* check if Grease Pencil isn't already running */
			if (ED_gpencil_session_active() == 0)
				return 1;
			else
				CTX_wm_operator_poll_msg_set(C, "Grease Pencil operator is already active");
		}
		else {
			CTX_wm_operator_poll_msg_set(C, "Failed to find Grease Pencil data to draw into");
		}
	}
	else {
		CTX_wm_operator_poll_msg_set(C, "Active region not set");
	}
	
	return 0;
}

/* check if projecting strokes into 3d-geometry in the 3D-View */
static bool gpencil_project_check(tGPsdata *p)
{
	bGPdata *gpd = p->gpd;
	return ((gpd->sbuffer_sflag & GP_STROKE_3DSPACE) && (*p->align_flag & (GP_PROJECT_DEPTH_VIEW | GP_PROJECT_DEPTH_STROKE)));
}

/* ******************************************* */
/* Calculations/Conversions */

/* Utilities --------------------------------- */

/* get the reference point for stroke-point conversions */
static void gp_get_3d_reference(tGPsdata *p, float vec[3])
{
	View3D *v3d = p->sa->spacedata.first;
	const float *fp = ED_view3d_cursor3d_get(p->scene, v3d);
	
	/* the reference point used depends on the owner... */
#if 0 /* XXX: disabled for now, since we can't draw relative to the owner yet */
	if (p->ownerPtr.type == &RNA_Object) {
		Object *ob = (Object *)p->ownerPtr.data;
		
		/* active Object
		 *  - use relative distance of 3D-cursor from object center
		 */
		sub_v3_v3v3(vec, fp, ob->loc);
	}
	else
#endif
	{
		/* use 3D-cursor */
		copy_v3_v3(vec, fp);
	}
}

/* Stroke Editing ---------------------------- */

/* check if the current mouse position is suitable for adding a new point */
static bool gp_stroke_filtermval(tGPsdata *p, const int mval[2], int pmval[2])
{
	int dx = abs(mval[0] - pmval[0]);
	int dy = abs(mval[1] - pmval[1]);
	
	/* if buffer is empty, just let this go through (i.e. so that dots will work) */
	if (p->gpd->sbuffer_size == 0)
		return true;
	
	/* check if mouse moved at least certain distance on both axes (best case)
	 *	- aims to eliminate some jitter-noise from input when trying to draw straight lines freehand
	 */
	else if ((dx > MIN_MANHATTEN_PX) && (dy > MIN_MANHATTEN_PX))
		return true;
	
	/* check if the distance since the last point is significant enough
	 *	- prevents points being added too densely
	 *	- distance here doesn't use sqrt to prevent slowness... we should still be safe from overflows though
	 */
	else if ((dx * dx + dy * dy) > MIN_EUCLIDEAN_PX * MIN_EUCLIDEAN_PX)
		return true;
	
	/* mouse 'didn't move' */
	else
		return false;
}

/* reproject the points of the stroke to a plane locked to axis to avoid stroke offset */
static void gp_project_points_to_plane(RegionView3D *rv3d, bGPDstroke *gps, const float origin[3], const int axis)
{
	float plane_normal[3];
	float vn[3];

	float ray[3];
	float rpoint[3];

	/* normal vector for a plane locked to axis */
	zero_v3(plane_normal);
	plane_normal[axis] = 1.0f;

	/* Reproject the points in the plane */
	for (int i = 0; i < gps->totpoints; i++) {
		bGPDspoint *pt = &gps->points[i];

		/* get a vector from the point with the current view direction of the viewport */
		ED_view3d_global_to_vector(rv3d, &pt->x, vn);

		/* calculate line extrem point to create a ray that cross the plane */
		mul_v3_fl(vn, -50.0f);
		add_v3_v3v3(ray, &pt->x, vn);

		/* if the line never intersect, the point is not changed */
		if (isect_line_plane_v3(rpoint, &pt->x, ray, origin, plane_normal)) {
			copy_v3_v3(&pt->x, rpoint);
		}
	}
}

/* reproject stroke to plane locked to axis in 3d cursor location */
static void gp_reproject_toplane(tGPsdata *p, bGPDstroke *gps)
{
	bGPdata *gpd = p->gpd;
	float origin[3];
	float cursor[3];
	RegionView3D *rv3d = p->ar->regiondata;

	/* verify the stroke mode is CURSOR 3d space mode */
	if ((gpd->sbuffer_sflag & GP_STROKE_3DSPACE) == 0) {
		return;
	}
	if ((*p->align_flag & GP_PROJECT_VIEWSPACE) == 0) {
		return;
	}
	if ((*p->align_flag & GP_PROJECT_DEPTH_VIEW) || (*p->align_flag & GP_PROJECT_DEPTH_STROKE)) {
		return;
	}

	/* get 3d cursor and set origin for locked axis only. Uses axis-1 because the enum for XYZ start with 1 */
	gp_get_3d_reference(p, cursor);
	zero_v3(origin);
	origin[p->lock_axis - 1] = cursor[p->lock_axis - 1];

	gp_project_points_to_plane(rv3d, gps, origin, p->lock_axis - 1);
}

/* convert screen-coordinates to buffer-coordinates */
/* XXX this method needs a total overhaul! */
static void gp_stroke_convertcoords(tGPsdata *p, const int mval[2], float out[3], float *depth)
{
	bGPdata *gpd = p->gpd;
	
	/* in 3d-space - pt->x/y/z are 3 side-by-side floats */
	if (gpd->sbuffer_sflag & GP_STROKE_3DSPACE) {
		if (gpencil_project_check(p) && (ED_view3d_autodist_simple(p->ar, mval, out, 0, depth))) {
			/* projecting onto 3D-Geometry
			 *	- nothing more needs to be done here, since view_autodist_simple() has already done it
			 */
		}
		else {
			float mval_prj[2];
			float rvec[3], dvec[3];
			float mval_f[2] = {UNPACK2(mval)};
			float zfac;
		
			/* Current method just converts each point in screen-coordinates to
			 * 3D-coordinates using the 3D-cursor as reference. In general, this
			 * works OK, but it could of course be improved.
			 *
			 * TODO:
			 *	- investigate using nearest point(s) on a previous stroke as
			 *	  reference point instead or as offset, for easier stroke matching
			 */
			
			gp_get_3d_reference(p, rvec);
			zfac = ED_view3d_calc_zfac(p->ar->regiondata, rvec, NULL);
			
			if (ED_view3d_project_float_global(p->ar, rvec, mval_prj, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
				sub_v2_v2v2(mval_f, mval_prj, mval_f);
				ED_view3d_win_to_delta(p->ar, mval_f, dvec, zfac);
				sub_v3_v3v3(out, rvec, dvec);
			}
			else {
				zero_v3(out);
			}
		}
	}
	
	/* 2d - on 'canvas' (assume that p->v2d is set) */
	else if ((gpd->sbuffer_sflag & GP_STROKE_2DSPACE) && (p->v2d)) {
		UI_view2d_region_to_view(p->v2d, mval[0], mval[1], &out[0], &out[1]);
		mul_v3_m4v3(out, p->imat, out);
	}
	
	/* 2d - relative to screen (viewport area) */
	else {
		if (p->subrect == NULL) { /* normal 3D view */
			out[0] = (float)(mval[0]) / (float)(p->ar->winx) * 100;
			out[1] = (float)(mval[1]) / (float)(p->ar->winy) * 100;
		}
		else { /* camera view, use subrect */
			out[0] = ((mval[0] - p->subrect->xmin) / BLI_rctf_size_x(p->subrect)) * 100;
			out[1] = ((mval[1] - p->subrect->ymin) / BLI_rctf_size_y(p->subrect)) * 100;
		}
	}
}

/* apply jitter to stroke */
static void gp_brush_jitter(bGPdata *gpd, bGPDbrush *brush, tGPspoint *pt, const int mval[2], int r_mval[2])
{
	float pressure = pt->pressure;
	float tmp_pressure = pt->pressure;
	if (brush->draw_jitter > 0.0f) {
		float curvef = curvemapping_evaluateF(brush->cur_jitter, 0, pressure);
		tmp_pressure = curvef * brush->draw_sensitivity;
	}
	const float exfactor = (brush->draw_jitter + 2.0f) * (brush->draw_jitter + 2.0f); /* exponential value */
	const float fac = BLI_frand() * exfactor * tmp_pressure;
	/* Jitter is applied perpendicular to the mouse movement vector (2D space) */
	float mvec[2], svec[2];
	/* mouse movement in ints -> floats */
	if (gpd->sbuffer_size > 1) {
		mvec[0] = (float)(mval[0] - (pt - 1)->x);
		mvec[1] = (float)(mval[1] - (pt - 1)->y);
		normalize_v2(mvec);
	}
	else {
		mvec[0] = 0.0f;
		mvec[1] = 0.0f;
	}
	/* rotate mvec by 90 degrees... */
	svec[0] = -mvec[1];
	svec[1] = mvec[0];
	/* scale the displacement by the random, and apply */
	if (BLI_frand() > 0.5f) {
		mul_v2_fl(svec, -fac);
	}
	else {
		mul_v2_fl(svec, fac);
	}

	r_mval[0] = mval[0] + svec[0];
	r_mval[1] = mval[1] + svec[1];

}

/* apply pressure change depending of the angle of the stroke to simulate a pen with shape */
static void gp_brush_angle(bGPdata *gpd, bGPDbrush *brush, tGPspoint *pt, const int mval[2])
{
	float mvec[2];
	float sen = brush->draw_angle_factor; /* sensitivity */;
	float fac;
	float mpressure;

	float angle = brush->draw_angle; /* default angle of brush in radians */;
	float v0[2] = { cos(angle), sin(angle) }; /* angle vector of the brush with full thickness */

	/* Apply to first point (only if there are 2 points because before no data to do it ) */
	if (gpd->sbuffer_size == 1) {
		mvec[0] = (float)(mval[0] - (pt - 1)->x);
		mvec[1] = (float)(mval[1] - (pt - 1)->y);
		normalize_v2(mvec);

		/* uses > 1.0f to get a smooth transition in first point */
		fac = 1.4f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
		(pt - 1)->pressure = (pt - 1)->pressure - (sen * fac);

		CLAMP((pt - 1)->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
	}

	/* apply from second point */
	if (gpd->sbuffer_size >= 1) {
		mvec[0] = (float)(mval[0] - (pt - 1)->x);
		mvec[1] = (float)(mval[1] - (pt - 1)->y);
		normalize_v2(mvec);

		fac = 1.0f - fabs(dot_v2v2(v0, mvec)); /* 0.0 to 1.0 */
		/* interpolate with previous point for smoother transitions */
		mpressure = interpf(pt->pressure - (sen * fac), (pt - 1)->pressure, 0.3f);
		pt->pressure = mpressure; 

		CLAMP(pt->pressure, GPENCIL_ALPHA_OPACITY_THRESH, 1.0f);
	}

}

/* add current stroke-point to buffer (returns whether point was successfully added) */
static short gp_stroke_addpoint(tGPsdata *p, const int mval[2], float pressure, double curtime)
{
	bGPdata *gpd = p->gpd;
	bGPDbrush *brush = p->brush;
	tGPspoint *pt;
	ToolSettings *ts = p->scene->toolsettings;

	/* check painting mode */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
		/* straight lines only - i.e. only store start and end point in buffer */
		if (gpd->sbuffer_size == 0) {
			/* first point in buffer (start point) */
			pt = (tGPspoint *)(gpd->sbuffer);
			
			/* store settings */
			copy_v2_v2_int(&pt->x, mval);
			pt->pressure = 1.0f; /* T44932 - Pressure vals are unreliable, so ignore for now */
			pt->strength = 1.0f;
			pt->time = (float)(curtime - p->inittime);
			
			/* increment buffer size */
			gpd->sbuffer_size++;
		}
		else {
			/* just reset the endpoint to the latest value
			 *	- assume that pointers for this are always valid...
			 */
			pt = ((tGPspoint *)(gpd->sbuffer) + 1);
			
			/* store settings */
			copy_v2_v2_int(&pt->x, mval);
			pt->pressure = 1.0f; /* T44932 - Pressure vals are unreliable, so ignore for now */
			pt->strength = 1.0f;
			pt->time = (float)(curtime - p->inittime);
			
			/* now the buffer has 2 points (and shouldn't be allowed to get any larger) */
			gpd->sbuffer_size = 2;
		}
		
		/* can keep carrying on this way :) */
		return GP_STROKEADD_NORMAL;
	}
	else if (p->paintmode == GP_PAINTMODE_DRAW) { /* normal drawing */
		/* check if still room in buffer */
		if (gpd->sbuffer_size >= GP_STROKE_BUFFER_MAX)
			return GP_STROKEADD_OVERFLOW;
		
		/* get pointer to destination point */
		pt = ((tGPspoint *)(gpd->sbuffer) + gpd->sbuffer_size);
		
		/* store settings */
		/* pressure */
		if (brush->flag & GP_BRUSH_USE_PRESSURE) {
			float curvef = curvemapping_evaluateF(brush->cur_sensitivity, 0, pressure);
			pt->pressure = curvef * brush->draw_sensitivity;
		}
		else {
			pt->pressure = 1.0f;
		}
		/* Apply jitter to position */
		if (brush->draw_jitter > 0.0f) {
			int r_mval[2];
			gp_brush_jitter(gpd, brush, pt, mval, r_mval);
			copy_v2_v2_int(&pt->x, r_mval);
		}
		else {
			copy_v2_v2_int(&pt->x, mval);
		}
		/* apply randomness to pressure */
		if ((brush->draw_random_press > 0.0f) && (brush->flag & GP_BRUSH_USE_RANDOM_PRESSURE)) {
			float curvef = curvemapping_evaluateF(brush->cur_sensitivity, 0, pressure);
			float tmp_pressure = curvef * brush->draw_sensitivity;
			if (BLI_frand() > 0.5f) {
				pt->pressure -= tmp_pressure * brush->draw_random_press * BLI_frand();
			}
			else {
				pt->pressure += tmp_pressure * brush->draw_random_press * BLI_frand();
			}
			CLAMP(pt->pressure, GPENCIL_STRENGTH_MIN, 1.0f);
		}

		/* apply angle of stroke to brush size */
		if (brush->draw_angle_factor > 0.0f) {
			gp_brush_angle(gpd, brush, pt, mval);
		}

		/* color strength */
		if (brush->flag & GP_BRUSH_USE_STENGTH_PRESSURE) {
			float curvef = curvemapping_evaluateF(brush->cur_strength, 0, pressure);
			float tmp_pressure = curvef * brush->draw_sensitivity;

			pt->strength = tmp_pressure * brush->draw_strength;
		}
		else {
			pt->strength = brush->draw_strength;
		}
		CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);

		/* apply randomness to color strength */
		if ((brush->draw_random_press > 0.0f) && (brush->flag & GP_BRUSH_USE_RANDOM_STRENGTH)) {
			if (BLI_frand() > 0.5f) {
				pt->strength -= pt->strength * brush->draw_random_press * BLI_frand();
			}
			else {
				pt->strength += pt->strength * brush->draw_random_press * BLI_frand();
			}
			CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
		}

		/* point time */
		pt->time = (float)(curtime - p->inittime);
		
		/* increment counters */
		gpd->sbuffer_size++;
		
		/* check if another operation can still occur */
		if (gpd->sbuffer_size == GP_STROKE_BUFFER_MAX)
			return GP_STROKEADD_FULL;
		else
			return GP_STROKEADD_NORMAL;
	}
	else if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
		
		bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
		/* get pointer to destination point */
		pt = (tGPspoint *)(gpd->sbuffer);
		
		/* store settings */
		copy_v2_v2_int(&pt->x, mval);
		pt->pressure = 1.0f; /* T44932 - Pressure vals are unreliable, so ignore for now */
		pt->strength = 1.0f;
		pt->time = (float)(curtime - p->inittime);
		
		/* if there's stroke for this poly line session add (or replace last) point
		 * to stroke. This allows to draw lines more interactively (see new segment
		 * during mouse slide, e.g.)
		 */
		if (gp_stroke_added_check(p)) {
			bGPDstroke *gps = p->gpf->strokes.last;
			bGPDspoint *pts;
			
			/* first time point is adding to temporary buffer -- need to allocate new point in stroke */
			if (gpd->sbuffer_size == 0) {
				gps->points = MEM_reallocN(gps->points, sizeof(bGPDspoint) * (gps->totpoints + 1));
				gps->totpoints++;
			}
			
			pts = &gps->points[gps->totpoints - 1];
			
			/* special case for poly lines: normally,
			 * depth is needed only when creating new stroke from buffer,
			 * but poly lines are converting to stroke instantly,
			 * so initialize depth buffer before converting coordinates
			 */
			if (gpencil_project_check(p)) {
				View3D *v3d = p->sa->spacedata.first;
				
				view3d_region_operator_needs_opengl(p->win, p->ar);
				ED_view3d_autodist_init(p->scene, p->ar, v3d, (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 1 : 0);
			}
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &pt->x, &pts->x, NULL);
			/* if axis locked, reproject to plane locked (only in 3d space) */
			if (p->lock_axis > GP_LOCKAXIS_NONE) {
				gp_reproject_toplane(p, gps);
			}
			/* if parented change position relative to parent object */
			if (gpl->parent != NULL) {
				gp_apply_parent_point(gpl, pts);
			}
			/* copy pressure and time */
			pts->pressure = pt->pressure;
			pts->strength = pt->strength;
			pts->time = pt->time;
			/* force fill recalc */
			gps->flag |= GP_STROKE_RECALC_CACHES;
		}
		
		/* increment counters */
		if (gpd->sbuffer_size == 0)
			gpd->sbuffer_size++;
		
		return GP_STROKEADD_NORMAL;
	}

	/* return invalid state for now... */
	return GP_STROKEADD_INVALID;
}

/* simplify a stroke (in buffer) before storing it
 *	- applies a reverse Chaikin filter
 *	- code adapted from etch-a-ton branch (editarmature_sketch.c)
 */
static void gp_stroke_simplify(tGPsdata *p)
{
	bGPdata *gpd = p->gpd;
	tGPspoint *old_points = (tGPspoint *)gpd->sbuffer;
	short num_points = gpd->sbuffer_size;
	short flag = gpd->sbuffer_sflag;
	short i, j;
	
	/* only simplify if simplification is enabled, and we're not doing a straight line */
	if (!(U.gp_settings & GP_PAINT_DOSIMPLIFY) || (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT))
		return;
	
	/* don't simplify if less than 4 points in buffer */
	if ((num_points <= 4) || (old_points == NULL))
		return;
	
	/* clear buffer (but don't free mem yet) so that we can write to it
	 *	- firstly set sbuffer to NULL, so a new one is allocated
	 *	- secondly, reset flag after, as it gets cleared auto
	 */
	gpd->sbuffer = NULL;
	gp_session_validatebuffer(p);
	gpd->sbuffer_sflag = flag;
	
/* macro used in loop to get position of new point
 *	- used due to the mixture of datatypes in use here
 */
#define GP_SIMPLIFY_AVPOINT(offs, sfac) \
	{ \
		co[0] += (float)(old_points[offs].x * sfac); \
		co[1] += (float)(old_points[offs].y * sfac); \
		pressure += old_points[offs].pressure * sfac; \
		time += old_points[offs].time * sfac; \
	} (void)0
	
	/* XXX Here too, do not lose start and end points! */
	gp_stroke_addpoint(p, &old_points->x, old_points->pressure, p->inittime + (double)old_points->time);
	for (i = 0, j = 0; i < num_points; i++) {
		if (i - j == 3) {
			float co[2], pressure, time;
			int mco[2];
			
			/* initialize values */
			co[0] = 0.0f;
			co[1] = 0.0f;
			pressure = 0.0f;
			time = 0.0f;
			
			/* using macro, calculate new point */
			GP_SIMPLIFY_AVPOINT(j, -0.25f);
			GP_SIMPLIFY_AVPOINT(j + 1, 0.75f);
			GP_SIMPLIFY_AVPOINT(j + 2, 0.75f);
			GP_SIMPLIFY_AVPOINT(j + 3, -0.25f);
			
			/* set values for adding */
			mco[0] = (int)co[0];
			mco[1] = (int)co[1];
			
			/* ignore return values on this... assume to be ok for now */
			gp_stroke_addpoint(p, mco, pressure, p->inittime + (double)time);
			
			j += 2;
		}
	}
	gp_stroke_addpoint(p, &old_points[num_points - 1].x, old_points[num_points - 1].pressure,
	                   p->inittime + (double)old_points[num_points - 1].time);
	
	/* free old buffer */
	MEM_freeN(old_points);
}

/* make a new stroke from the buffer data */
static void gp_stroke_newfrombuffer(tGPsdata *p)
{
	bGPdata *gpd = p->gpd;
	bGPDlayer *gpl = p->gpl;
	bGPDstroke *gps;
	bGPDspoint *pt;
	tGPspoint *ptc;
	bGPDbrush *brush = p->brush;
	ToolSettings *ts = p->scene->toolsettings;
	
	int i, totelem;
	/* since strokes are so fine, when using their depth we need a margin otherwise they might get missed */
	int depth_margin = (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 4 : 0;
	
	/* get total number of points to allocate space for
	 *	- drawing straight-lines only requires the endpoints
	 */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT)
		totelem = (gpd->sbuffer_size >= 2) ? 2 : gpd->sbuffer_size;
	else
		totelem = gpd->sbuffer_size;
	
	/* exit with error if no valid points from this stroke */
	if (totelem == 0) {
		if (G.debug & G_DEBUG)
			printf("Error: No valid points in stroke buffer to convert (tot=%d)\n", gpd->sbuffer_size);
		return;
	}
	
	/* special case for poly line -- for already added stroke during session
	 * coordinates are getting added to stroke immediately to allow more
	 * interactive behavior
	 */
	if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
		if (gp_stroke_added_check(p)) {
			return;
		}
	}
	
	/* allocate memory for a new stroke */
	gps = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
	
	/* copy appropriate settings for stroke */
	gps->totpoints = totelem;
	gps->thickness = brush->thickness;
	gps->flag = gpd->sbuffer_sflag;
	gps->inittime = p->inittime;
	
	/* enable recalculation flag by default (only used if hq fill) */
	gps->flag |= GP_STROKE_RECALC_CACHES;

	/* allocate enough memory for a continuous array for storage points */
	int sublevel = brush->sublevel;
	int new_totpoints = gps->totpoints;
	
	for (i = 0; i < sublevel; i++) {
		new_totpoints += new_totpoints - 1;
	}
	gps->points = MEM_callocN(sizeof(bGPDspoint) * new_totpoints, "gp_stroke_points");
	/* initialize triangle memory to dummy data */
	gps->triangles = MEM_callocN(sizeof(bGPDtriangle), "GP Stroke triangulation");
	gps->flag |= GP_STROKE_RECALC_CACHES;
	gps->tot_triangles = 0;
	/* set pointer to first non-initialized point */
	pt = gps->points + (gps->totpoints - totelem);
	
	/* copy points from the buffer to the stroke */
	if (p->paintmode == GP_PAINTMODE_DRAW_STRAIGHT) {
		/* straight lines only -> only endpoints */
		{
			/* first point */
			ptc = gpd->sbuffer;
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
			/* if axis locked, reproject to plane locked (only in 3d space) */
			if (p->lock_axis > GP_LOCKAXIS_NONE) {
				gp_reproject_toplane(p, gps);
			}
			/* if parented change position relative to parent object */
			if (gpl->parent != NULL) {
				gp_apply_parent_point(gpl, pt);
			}
			/* copy pressure and time */
			pt->pressure = ptc->pressure;
			pt->strength = ptc->strength;
			CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
			pt->time = ptc->time;
			
			pt++;
		}
		
		if (totelem == 2) {
			/* last point if applicable */
			ptc = ((tGPspoint *)gpd->sbuffer) + (gpd->sbuffer_size - 1);
			
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
			/* if axis locked, reproject to plane locked (only in 3d space) */
			if (p->lock_axis > GP_LOCKAXIS_NONE) {
				gp_reproject_toplane(p, gps);
			}
			/* if parented change position relative to parent object */
			if (gpl->parent != NULL) {
				gp_apply_parent_point(gpl, pt);
			}

			/* copy pressure and time */
			pt->pressure = ptc->pressure;
			pt->strength = ptc->strength;
			CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
			pt->time = ptc->time;
		}
	}
	else if (p->paintmode == GP_PAINTMODE_DRAW_POLY) {
		/* first point */
		ptc = gpd->sbuffer;
		
		/* convert screen-coordinates to appropriate coordinates (and store them) */
		gp_stroke_convertcoords(p, &ptc->x, &pt->x, NULL);
		/* if axis locked, reproject to plane locked (only in 3d space) */
		if (p->lock_axis > GP_LOCKAXIS_NONE) {
			gp_reproject_toplane(p, gps);
		}
		/* if parented change position relative to parent object */
		if (gpl->parent != NULL) {
			gp_apply_parent_point(gpl, pt);
		}
		/* copy pressure and time */
		pt->pressure = ptc->pressure;
		pt->strength = ptc->strength;
		CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
		pt->time = ptc->time;
	}
	else {
		float *depth_arr = NULL;

		/* get an array of depths, far depths are blended */
		if (gpencil_project_check(p)) {
			int mval[2], mval_prev[2] = { 0 };
			int interp_depth = 0;
			int found_depth = 0;

			depth_arr = MEM_mallocN(sizeof(float) * gpd->sbuffer_size, "depth_points");

			for (i = 0, ptc = gpd->sbuffer; i < gpd->sbuffer_size; i++, ptc++, pt++) {
				copy_v2_v2_int(mval, &ptc->x);

				if ((ED_view3d_autodist_depth(p->ar, mval, depth_margin, depth_arr + i) == 0) &&
				    (i && (ED_view3d_autodist_depth_seg(p->ar, mval, mval_prev, depth_margin + 1, depth_arr + i) == 0)))
				{
					interp_depth = true;
				}
				else {
					found_depth = true;
				}

				copy_v2_v2_int(mval_prev, mval);
			}

			if (found_depth == false) {
				/* eeh... not much we can do.. :/, ignore depth in this case, use the 3D cursor */
				for (i = gpd->sbuffer_size - 1; i >= 0; i--)
					depth_arr[i] = 0.9999f;
			}
			else {
				if (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE_ENDPOINTS) {
					/* remove all info between the valid endpoints */
					int first_valid = 0;
					int last_valid = 0;

					for (i = 0; i < gpd->sbuffer_size; i++) {
						if (depth_arr[i] != FLT_MAX)
							break;
					}
					first_valid = i;

					for (i = gpd->sbuffer_size - 1; i >= 0; i--) {
						if (depth_arr[i] != FLT_MAX)
							break;
					}
					last_valid = i;

					/* invalidate non-endpoints, so only blend between first and last */
					for (i = first_valid + 1; i < last_valid; i++)
						depth_arr[i] = FLT_MAX;

					interp_depth = true;
				}

				if (interp_depth) {
					interp_sparse_array(depth_arr, gpd->sbuffer_size, FLT_MAX);
				}
			}
		}


		pt = gps->points;

		/* convert all points (normal behavior) */
		for (i = 0, ptc = gpd->sbuffer; i < gpd->sbuffer_size && ptc; i++, ptc++, pt++) {
			/* convert screen-coordinates to appropriate coordinates (and store them) */
			gp_stroke_convertcoords(p, &ptc->x, &pt->x, depth_arr ? depth_arr + i : NULL);

			/* copy pressure and time */
			pt->pressure = ptc->pressure;
			pt->strength = ptc->strength;
			CLAMP(pt->strength, GPENCIL_STRENGTH_MIN, 1.0f);
			pt->time = ptc->time;
		}

		/* subdivide the stroke */
		if (sublevel > 0) {
			int totpoints = gps->totpoints;
			for (i = 0; i < sublevel; i++) {
				/* we're adding one new point between each pair of verts on each step */
				totpoints += totpoints - 1;

				gp_subdivide_stroke(gps, totpoints);
			}
		}
		/* apply randomness to stroke */
		if (brush->draw_random_sub > 0.0f) {
			gp_randomize_stroke(gps, brush);
		}

		/* smooth stroke after subdiv - only if there's something to do
		 * for each iteration, the factor is reduced to get a better smoothing without changing too much
		 * the original stroke
		 */
		if (brush->draw_smoothfac > 0.0f) {
			float reduce = 0.0f;
			for (int r = 0; r < brush->draw_smoothlvl; ++r) {
				for (i = 0; i < gps->totpoints; i++) {
					/* NOTE: No pressure smoothing, or else we get annoying thickness changes while drawing... */
					gp_smooth_stroke(gps, i, brush->draw_smoothfac - reduce, false);
				}
				reduce += 0.25f;  // reduce the factor
			}
		}

		/* if axis locked, reproject to plane locked (only in 3d space) */
		if (p->lock_axis > GP_LOCKAXIS_NONE) {
			gp_reproject_toplane(p, gps);
		}
		/* if parented change position relative to parent object */
		if (gpl->parent != NULL) {
			gp_apply_parent(gpl, gps);
		}

		if (depth_arr)
			MEM_freeN(depth_arr);
	}
	/* Save palette color */
	bGPDpalette *palette = BKE_gpencil_palette_getactive(p->gpd);
	bGPDpalettecolor *palcolor = BKE_gpencil_palettecolor_getactive(palette);
	gps->palcolor = palcolor;
	BLI_strncpy(gps->colorname, palcolor->info, sizeof(gps->colorname));

	/* add stroke to frame, usually on tail of the listbase, but if on back is enabled the stroke is added on listbase head 
	 * because the drawing order is inverse and the head stroke is the first to draw. This is very useful for artist
	 * when drawing the background
	 */
	if ((ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) && (p->paintmode != GP_PAINTMODE_DRAW_POLY)) {
		BLI_addhead(&p->gpf->strokes, gps);
	}
	else {
		BLI_addtail(&p->gpf->strokes, gps);
	}
	gp_stroke_added_enable(p);
}

/* --- 'Eraser' for 'Paint' Tool ------ */

/* which which point is infront (result should only be used for comparison) */
static float view3d_point_depth(const RegionView3D *rv3d, const float co[3])
{
	if (rv3d->is_persp) {
		return ED_view3d_calc_zfac(rv3d, co, NULL);
	}
	else {
		return -dot_v3v3(rv3d->viewinv[2], co);
	}
}

/* only erase stroke points that are visible */
static bool gp_stroke_eraser_is_occluded(tGPsdata *p, const bGPDspoint *pt, const int x, const int y)
{
	if ((p->sa->spacetype == SPACE_VIEW3D) &&
	    (p->flags & GP_PAINTFLAG_V3D_ERASER_DEPTH))
	{
		RegionView3D *rv3d = p->ar->regiondata;
		bGPDlayer *gpl = p->gpl;

		const int mval[2] = {x, y};
		float mval_3d[3];
		float fpt[3];

		float diff_mat[4][4];
		/* calculate difference matrix if parent object */
		ED_gpencil_parent_location(gpl, diff_mat);

		if (ED_view3d_autodist_simple(p->ar, mval, mval_3d, 0, NULL)) {
			const float depth_mval = view3d_point_depth(rv3d, mval_3d);

			mul_v3_m4v3(fpt, diff_mat, &pt->x);
			const float depth_pt   = view3d_point_depth(rv3d, fpt);
			
			if (depth_pt > depth_mval) {
				return true;
			}
		}
	}
	return false;
}

/* apply a falloff effect to brush strength, based on distance */
static float gp_stroke_eraser_calc_influence(tGPsdata *p, const int mval[2], const int radius, const int co[2])
{
	/* Linear Falloff... */
	float distance = (float)len_v2v2_int(mval, co);
	float fac;
	
	CLAMP(distance, 0.0f, (float)radius);
	fac = 1.0f - (distance / (float)radius);
	
	/* Control this further using pen pressure */
	fac *= p->pressure;
	
	/* Return influence factor computed here */
	return fac;
}

/* eraser tool - evaluation per stroke */
/* TODO: this could really do with some optimization (KD-Tree/BVH?) */
static void gp_stroke_eraser_dostroke(tGPsdata *p,
                                      bGPDlayer *gpl, bGPDframe *gpf, bGPDstroke *gps,
                                      const int mval[2], const int mvalo[2],
                                      const int radius, const rcti *rect)
{
	bGPDspoint *pt1, *pt2;
	int pc1[2] = {0};
	int pc2[2] = {0};
	int i;
	float diff_mat[4][4];

	/* calculate difference matrix if parent object */
	if (gpl->parent != NULL) {
		ED_gpencil_parent_location(gpl, diff_mat);
	}

	if (gps->totpoints == 0) {
		/* just free stroke */
		if (gps->points)
			MEM_freeN(gps->points);
		if (gps->triangles)
			MEM_freeN(gps->triangles);
		BLI_freelinkN(&gpf->strokes, gps);
	}
	else if (gps->totpoints == 1) {
		/* only process if it hasn't been masked out... */
		if (!(p->flags & GP_PAINTFLAG_SELECTMASK) || (gps->points->flag & GP_SPOINT_SELECT)) {
			if (gpl->parent == NULL) {
				gp_point_to_xy(&p->gsc, gps, gps->points, &pc1[0], &pc1[1]);
			}
			else {
				bGPDspoint pt_temp;
				gp_point_to_parent_space(gps->points, diff_mat, &pt_temp);
				gp_point_to_xy(&p->gsc, gps, &pt_temp, &pc1[0], &pc1[1]);
			}
			/* do boundbox check first */
			if ((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) {
				/* only check if point is inside */
				if (len_v2v2_int(mval, pc1) <= radius) {
					/* free stroke */
					// XXX: pressure sensitive eraser should apply here too?
					MEM_freeN(gps->points);
					if (gps->triangles)
						MEM_freeN(gps->triangles);
					BLI_freelinkN(&gpf->strokes, gps);
				}
			}
		}
	}
	else {
		/* Pressure threshold at which stroke should be culled: Calculated as pressure value
		 * below which we would have invisible strokes
		 */
		const float cull_thresh = (gps->thickness) ?  1.0f / ((float)gps->thickness)  : 1.0f;
		
		/* Amount to decrease the pressure of each point with each stroke */
		// TODO: Fetch from toolsettings, or compute based on thickness instead?
		const float strength = 0.1f;
		
		/* Perform culling? */
		bool do_cull = false;
		
		
		/* Clear Tags
		 *
		 * Note: It's better this way, as we are sure that
		 * we don't miss anything, though things will be
		 * slightly slower as a result
		 */
		for (i = 0; i < gps->totpoints; i++) {
			bGPDspoint *pt = &gps->points[i];
			pt->flag &= ~GP_SPOINT_TAG;
		}
		
		/* First Pass: Loop over the points in the stroke
		 *   1) Thin out parts of the stroke under the brush
		 *   2) Tag "too thin" parts for removal (in second pass)
		 */
		for (i = 0; (i + 1) < gps->totpoints; i++) {
			/* get points to work with */
			pt1 = gps->points + i;
			pt2 = gps->points + i + 1;

			/* only process if it hasn't been masked out... */
			if ((p->flags & GP_PAINTFLAG_SELECTMASK) && !(gps->points->flag & GP_SPOINT_SELECT))
				continue;
			
			if (gpl->parent == NULL) {
				gp_point_to_xy(&p->gsc, gps, pt1, &pc1[0], &pc1[1]);
				gp_point_to_xy(&p->gsc, gps, pt2, &pc2[0], &pc2[1]);
			}
			else {
				bGPDspoint npt;
				gp_point_to_parent_space(pt1, diff_mat, &npt);
				gp_point_to_xy(&p->gsc, gps, &npt, &pc1[0], &pc1[1]);

				gp_point_to_parent_space(pt2, diff_mat, &npt);
				gp_point_to_xy(&p->gsc, gps, &npt, &pc2[0], &pc2[1]);
			}

			/* Check that point segment of the boundbox of the eraser stroke */
			if (((!ELEM(V2D_IS_CLIPPED, pc1[0], pc1[1])) && BLI_rcti_isect_pt(rect, pc1[0], pc1[1])) ||
			    ((!ELEM(V2D_IS_CLIPPED, pc2[0], pc2[1])) && BLI_rcti_isect_pt(rect, pc2[0], pc2[1])))
			{
				/* Check if point segment of stroke had anything to do with
				 * eraser region  (either within stroke painted, or on its lines)
				 *  - this assumes that linewidth is irrelevant
				 */
				if (gp_stroke_inside_circle(mval, mvalo, radius, pc1[0], pc1[1], pc2[0], pc2[1])) {
					if ((gp_stroke_eraser_is_occluded(p, pt1, pc1[0], pc1[1]) == false) ||
					    (gp_stroke_eraser_is_occluded(p, pt2, pc2[0], pc2[1]) == false))
					{
						/* Point is affected: */
						/* 1) Adjust thickness
						 *  - Influence of eraser falls off with distance from the middle of the eraser
						 *  - Second point gets less influence, as it might get hit again in the next segment
						 */
						pt1->pressure -= gp_stroke_eraser_calc_influence(p, mval, radius, pc1) * strength;
						pt2->pressure -= gp_stroke_eraser_calc_influence(p, mval, radius, pc2) * strength / 2.0f;
						
						/* 2) Tag any point with overly low influence for removal in the next pass */
						if (pt1->pressure < cull_thresh) {
							pt1->flag |= GP_SPOINT_TAG;
							do_cull = true;
						}
						if (pt2->pressure < cull_thresh) {
							pt2->flag |= GP_SPOINT_TAG;
							do_cull = true;
						}
					}
				}
			}
		}
		
		/* Second Pass: Remove any points that are tagged */
		if (do_cull) {
			gp_stroke_delete_tagged_points(gpf, gps, gps->next, GP_SPOINT_TAG);
		}
	}
}

/* erase strokes which fall under the eraser strokes */
static void gp_stroke_doeraser(tGPsdata *p)
{
	bGPDlayer *gpl;
	bGPDstroke *gps, *gpn;
	rcti rect;
	
	/* rect is rectangle of eraser */
	rect.xmin = p->mval[0] - p->radius;
	rect.ymin = p->mval[1] - p->radius;
	rect.xmax = p->mval[0] + p->radius;
	rect.ymax = p->mval[1] + p->radius;
	
	if (p->sa->spacetype == SPACE_VIEW3D) {
		if (p->flags & GP_PAINTFLAG_V3D_ERASER_DEPTH) {
			View3D *v3d = p->sa->spacedata.first;
			
			view3d_region_operator_needs_opengl(p->win, p->ar);
			ED_view3d_autodist_init(p->scene, p->ar, v3d, 0);
		}
	}
	
	/* loop over all layers too, since while it's easy to restrict editing to
	 * only a subset of layers, it is harder to perform the same erase operation
	 * on multiple layers...
	 */
	for (gpl = p->gpd->layers.first; gpl; gpl = gpl->next) {
		bGPDframe *gpf = gpl->actframe;
		
		/* only affect layer if it's editable (and visible) */
		if (gpencil_layer_is_editable(gpl) == false) {
			continue;
		}
		else if (gpf == NULL) {
			continue;
		}
		
		/* loop over strokes, checking segments for intersections */
		for (gps = gpf->strokes.first; gps; gps = gpn) {
			gpn = gps->next;
			/* check if the color is editable */
			if (ED_gpencil_stroke_color_use(gpl, gps) == false) {
				continue;
			}
			/* Not all strokes in the datablock may be valid in the current editor/context
			 * (e.g. 2D space strokes in the 3D view, if the same datablock is shared)
			 */
			if (ED_gpencil_stroke_can_use_direct(p->sa, gps)) {
				gp_stroke_eraser_dostroke(p, gpl, gpf, gps, p->mval, p->mvalo, p->radius, &rect);
			}
		}
	}
}

/* ******************************************* */
/* Sketching Operator */

/* clear the session buffers (call this before AND after a paint operation) */
static void gp_session_validatebuffer(tGPsdata *p)
{
	bGPdata *gpd = p->gpd;
	
	/* clear memory of buffer (or allocate it if starting a new session) */
	if (gpd->sbuffer) {
		/* printf("\t\tGP - reset sbuffer\n"); */
		memset(gpd->sbuffer, 0, sizeof(tGPspoint) * GP_STROKE_BUFFER_MAX);
	}
	else {
		/* printf("\t\tGP - allocate sbuffer\n"); */
		gpd->sbuffer = MEM_callocN(sizeof(tGPspoint) * GP_STROKE_BUFFER_MAX, "gp_session_strokebuffer");
	}
	
	/* reset indices */
	gpd->sbuffer_size = 0;
	
	/* reset flags */
	gpd->sbuffer_sflag = 0;
	
	/* reset inittime */
	p->inittime = 0.0;
}

/* create a new palette color */
static bGPDpalettecolor *gp_create_new_color(bGPDpalette *palette)
{
	bGPDpalettecolor *palcolor;

	palcolor = BKE_gpencil_palettecolor_addnew(palette, DATA_("Color"), true);
	
	return palcolor;
}

/* initialize a drawing brush */
static void gp_init_drawing_brush(ToolSettings *ts, tGPsdata *p)
{
	bGPDbrush *brush;

	/* if not exist, create a new one */
	if (BLI_listbase_is_empty(&ts->gp_brushes)) {
		/* create new brushes */
		BKE_gpencil_brush_init_presets(ts);
		brush = BKE_gpencil_brush_getactive(ts);
	}
	else {
		/* Use the current */
		brush = BKE_gpencil_brush_getactive(ts);
	}
	/* be sure curves are initializated */
	curvemapping_initialize(brush->cur_sensitivity);
	curvemapping_initialize(brush->cur_strength);
	curvemapping_initialize(brush->cur_jitter);

	/* asign to temp tGPsdata */
	p->brush = brush;
}


/* initialize a paint palette brush and a default color if not exist */
static void gp_init_palette(tGPsdata *p)
{
	bGPdata *gpd;
	bGPDpalette *palette;
	bGPDpalettecolor *palcolor;

	gpd = p->gpd;

	/* if not exist, create a new palette */
	if (BLI_listbase_is_empty(&gpd->palettes)) {
		/* create new palette */
		palette = BKE_gpencil_palette_addnew(gpd, DATA_("GP_Palette"), true);
		/* now create a default color */
		palcolor = gp_create_new_color(palette);
	}
	else {
		/* Use the current palette and color */
		palette = BKE_gpencil_palette_getactive(gpd);
		/* the palette needs one color */
		if (BLI_listbase_is_empty(&palette->colors)) {
			palcolor = gp_create_new_color(palette);
		}
		else {
			palcolor = BKE_gpencil_palettecolor_getactive(palette);
		}
		/* in some situations can be null, so use first */
		if (palcolor == NULL) {
			BKE_gpencil_palettecolor_setactive(palette, palette->colors.first);
			palcolor = palette->colors.first;
		}
	}

	/* asign to temp tGPsdata */
	p->palettecolor = palcolor;
}

/* (re)init new painting data */
static bool gp_session_initdata(bContext *C, tGPsdata *p)
{
	bGPdata **gpd_ptr = NULL;
	ScrArea *curarea = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	
	/* make sure the active view (at the starting time) is a 3d-view */
	if (curarea == NULL) {
		p->status = GP_STATUS_ERROR;
		if (G.debug & G_DEBUG)
			printf("Error: No active view for painting\n");
		return 0;
	}
	
	/* pass on current scene and window */
	p->scene = CTX_data_scene(C);
	p->win = CTX_wm_window(C);
	
	unit_m4(p->imat);
	unit_m4(p->mat);
	
	switch (curarea->spacetype) {
		/* supported views first */
		case SPACE_VIEW3D:
		{
			/* View3D *v3d = curarea->spacedata.first; */
			/* RegionView3D *rv3d = ar->regiondata; */
			
			/* set current area
			 *	- must verify that region data is 3D-view (and not something else)
			 */
			/* CAUTION: If this is the "toolbar", then this will change on the first stroke */
			p->sa = curarea;
			p->ar = ar;
			p->align_flag = &ts->gpencil_v3d_align;
			
			if (ar->regiondata == NULL) {
				p->status = GP_STATUS_ERROR;
				if (G.debug & G_DEBUG)
					printf("Error: 3D-View active region doesn't have any region data, so cannot be drawable\n");
				return 0;
			}
			break;
		}
		case SPACE_NODE:
		{
			/* SpaceNode *snode = curarea->spacedata.first; */
			
			/* set current area */
			p->sa = curarea;
			p->ar = ar;
			p->v2d = &ar->v2d;
			p->align_flag = &ts->gpencil_v2d_align;
			break;
		}
		case SPACE_SEQ:
		{
			SpaceSeq *sseq = curarea->spacedata.first;
			
			/* set current area */
			p->sa = curarea;
			p->ar = ar;
			p->v2d = &ar->v2d;
			p->align_flag = &ts->gpencil_seq_align;
			
			/* check that gpencil data is allowed to be drawn */
			if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
				p->status = GP_STATUS_ERROR;
				if (G.debug & G_DEBUG)
					printf("Error: In active view (sequencer), active mode doesn't support Grease Pencil\n");
				return 0;
			}
			break;
		}
		case SPACE_IMAGE:
		{
			/* SpaceImage *sima = curarea->spacedata.first; */
			
			/* set the current area */
			p->sa = curarea;
			p->ar = ar;
			p->v2d = &ar->v2d;
			p->align_flag = &ts->gpencil_ima_align;
			break;
		}
		case SPACE_CLIP:
		{
			SpaceClip *sc = curarea->spacedata.first;
			MovieClip *clip = ED_space_clip_get_clip(sc);
			
			if (clip == NULL) {
				p->status = GP_STATUS_ERROR;
				return false;
			}

			/* set the current area */
			p->sa = curarea;
			p->ar = ar;
			p->v2d = &ar->v2d;
			p->align_flag = &ts->gpencil_v2d_align;
			
			invert_m4_m4(p->imat, sc->unistabmat);
			
			/* custom color for new layer */
			p->custom_color[0] = 1.0f;
			p->custom_color[1] = 0.0f;
			p->custom_color[2] = 0.5f;
			p->custom_color[3] = 0.9f;
			
			if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
				int framenr = ED_space_clip_get_clip_frame_number(sc);
				MovieTrackingTrack *track = BKE_tracking_track_get_active(&clip->tracking);
				MovieTrackingMarker *marker = track ? BKE_tracking_marker_get(track, framenr) : NULL;

				if (marker) {
					p->imat[3][0] -= marker->pos[0];
					p->imat[3][1] -= marker->pos[1];
				}
				else {
					p->status = GP_STATUS_ERROR;
					return false;
				}
			}
			
			invert_m4_m4(p->mat, p->imat);
			copy_m4_m4(p->gsc.mat, p->mat);
			break;
		}
		/* unsupported views */
		default:
		{
			p->status = GP_STATUS_ERROR;
			if (G.debug & G_DEBUG)
				printf("Error: Active view not appropriate for Grease Pencil drawing\n");
			return 0;
		}
	}
	
	/* get gp-data */
	gpd_ptr = ED_gpencil_data_get_pointers(C, &p->ownerPtr);
	if (gpd_ptr == NULL) {
		p->status = GP_STATUS_ERROR;
		if (G.debug & G_DEBUG)
			printf("Error: Current context doesn't allow for any Grease Pencil data\n");
		return 0;
	}
	else {
		/* if no existing GPencil block exists, add one */
		if (*gpd_ptr == NULL)
			*gpd_ptr = BKE_gpencil_data_addnew("GPencil");
		p->gpd = *gpd_ptr;
	}
	
	if (ED_gpencil_session_active() == 0) {
		/* initialize undo stack,
		 * also, existing undo stack would make buffer drawn
		 */
		gpencil_undo_init(p->gpd);
	}
	
	/* clear out buffer (stored in gp-data), in case something contaminated it */
	gp_session_validatebuffer(p);
	/* set brush and create a new one if null */
	gp_init_drawing_brush(ts, p);
	/* set palette info and create a new one if null */
	gp_init_palette(p);
	/* set palette colors */
	bGPDpalettecolor *palcolor = p->palettecolor;
	bGPdata *pdata = p->gpd;
	copy_v4_v4(pdata->scolor, palcolor->color);
	pdata->sflag = palcolor->flag;
	/* lock axis */
	p->lock_axis = ts->gp_sculpt.lock_axis;

	return 1;
}

/* init new painting session */
static tGPsdata *gp_session_initpaint(bContext *C)
{
	tGPsdata *p = NULL;
	
	/* create new context data */
	p = MEM_callocN(sizeof(tGPsdata), "GPencil Drawing Data");
	
	gp_session_initdata(C, p);
	
	/* radius for eraser circle is defined in userprefs now */
	/* NOTE: we do this here, so that if we exit immediately,
	 *       erase size won't get lost
	 */
	p->radius = U.gp_eraser;

	/* return context data for running paint operator */
	return p;
}

/* cleanup after a painting session */
static void gp_session_cleanup(tGPsdata *p)
{
	bGPdata *gpd = (p) ? p->gpd : NULL;
	
	/* error checking */
	if (gpd == NULL)
		return;
	
	/* free stroke buffer */
	if (gpd->sbuffer) {
		/* printf("\t\tGP - free sbuffer\n"); */
		MEM_freeN(gpd->sbuffer);
		gpd->sbuffer = NULL;
	}
	
	/* clear flags */
	gpd->sbuffer_size = 0;
	gpd->sbuffer_sflag = 0;
	p->inittime = 0.0;
}

/* init new stroke */
static void gp_paint_initstroke(tGPsdata *p, eGPencil_PaintModes paintmode)
{
	Scene *scene = p->scene;
	ToolSettings *ts = scene->toolsettings;
	
	/* get active layer (or add a new one if non-existent) */
	p->gpl = BKE_gpencil_layer_getactive(p->gpd);
	if (p->gpl == NULL) {
		p->gpl = BKE_gpencil_layer_addnew(p->gpd, "GP_Layer", true);
		
		if (p->custom_color[3])
			copy_v3_v3(p->gpl->color, p->custom_color);
	}
	if (p->gpl->flag & GP_LAYER_LOCKED) {
		p->status = GP_STATUS_ERROR;
		if (G.debug & G_DEBUG)
			printf("Error: Cannot paint on locked layer\n");
		return;
	}
	
	/* get active frame (add a new one if not matching frame) */
	if (paintmode == GP_PAINTMODE_ERASER) {
		/* Eraser mode:
		 * 1) Add new frames to all frames that we might touch,
		 * 2) Ensure that p->gpf refers to the frame used for the active layer
		 *    (to avoid problems with other tools which expect it to exist)
		 */
		bool has_layer_to_erase = false;
		
		for (bGPDlayer *gpl = p->gpd->layers.first; gpl; gpl = gpl->next) {
			/* Skip if layer not editable */
			if (gpencil_layer_is_editable(gpl) == false)
				continue;
			
			/* Add a new frame if needed (and based off the active frame,
			 * as we need some existing strokes to erase)
			 *
			 * Note: We don't add a new frame if there's nothing there now, so
			 *       -> If there are no frames at all, don't add one
			 *       -> If there are no strokes in that frame, don't add a new empty frame
			 */
			if (gpl->actframe && gpl->actframe->strokes.first) {
				gpl->actframe = BKE_gpencil_layer_getframe(gpl, CFRA, GP_GETFRAME_ADD_COPY);
				has_layer_to_erase = true;
			}
			
			/* XXX: we omit GP_FRAME_PAINT here for now,
			 * as it is only really useful for doing
			 * paintbuffer drawing
			 */
		}
		
		/* Ensure this gets set... */
		p->gpf = p->gpl->actframe;
		
		/* Restrict eraser to only affecting selected strokes, if the "selection mask" is on
		 * (though this is only available in editmode)
		 */
		if (p->gpd->flag & GP_DATA_STROKE_EDITMODE) {
			if (ts->gp_sculpt.flag & GP_BRUSHEDIT_FLAG_SELECT_MASK) {
				p->flags |= GP_PAINTFLAG_SELECTMASK;
			}
		}
		
		if (has_layer_to_erase == false) {
			p->status = GP_STATUS_ERROR;
			//if (G.debug & G_DEBUG)
				printf("Error: Eraser will not be affecting anything (gpencil_paint_init)\n");
			return;
		}
	}
	else {
		/* Drawing Modes - Add a new frame if needed on the active layer */
		short add_frame_mode;
		
		if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST)
			add_frame_mode = GP_GETFRAME_ADD_COPY;
		else
			add_frame_mode = GP_GETFRAME_ADD_NEW;
			
		p->gpf = BKE_gpencil_layer_getframe(p->gpl, CFRA, add_frame_mode);
		
		if (p->gpf == NULL) {
			p->status = GP_STATUS_ERROR;
			if (G.debug & G_DEBUG)
				printf("Error: No frame created (gpencil_paint_init)\n");
			return;
		}
		else {
			p->gpf->flag |= GP_FRAME_PAINT;
		}
	}
	
	/* set 'eraser' for this stroke if using eraser */
	p->paintmode = paintmode;
	if (p->paintmode == GP_PAINTMODE_ERASER) {
		p->gpd->sbuffer_sflag |= GP_STROKE_ERASER;
		
		/* check if we should respect depth while erasing */
		if (p->sa->spacetype == SPACE_VIEW3D) {
			if (p->gpl->flag & GP_LAYER_NO_XRAY) {
				p->flags |= GP_PAINTFLAG_V3D_ERASER_DEPTH;
			}
		}
	}
	else {
		/* disable eraser flags - so that we can switch modes during a session */
		p->gpd->sbuffer_sflag &= ~GP_STROKE_ERASER;
		
		if (p->sa->spacetype == SPACE_VIEW3D) {
			if (p->gpl->flag & GP_LAYER_NO_XRAY) {
				p->flags &= ~GP_PAINTFLAG_V3D_ERASER_DEPTH;
			}
		}
	}
	
	/* set 'initial run' flag, which is only used to denote when a new stroke is starting */
	p->flags |= GP_PAINTFLAG_FIRSTRUN;
	
	
	/* when drawing in the camera view, in 2D space, set the subrect */
	p->subrect = NULL;
	if ((*p->align_flag & GP_PROJECT_VIEWSPACE) == 0) {
		if (p->sa->spacetype == SPACE_VIEW3D) {
			View3D *v3d = p->sa->spacedata.first;
			RegionView3D *rv3d = p->ar->regiondata;
			
			/* for camera view set the subrect */
			if (rv3d->persp == RV3D_CAMOB) {
				ED_view3d_calc_camera_border(p->scene, p->ar, v3d, rv3d, &p->subrect_data, true); /* no shift */
				p->subrect = &p->subrect_data;
			}
		}
	}
	
	/* init stroke point space-conversion settings... */
	p->gsc.gpd = p->gpd;
	p->gsc.gpl = p->gpl;
	
	p->gsc.sa = p->sa;
	p->gsc.ar = p->ar;
	p->gsc.v2d = p->v2d;
	
	p->gsc.subrect_data = p->subrect_data;
	p->gsc.subrect = p->subrect;
	
	copy_m4_m4(p->gsc.mat, p->mat);
	
	
	/* check if points will need to be made in view-aligned space */
	if (*p->align_flag & GP_PROJECT_VIEWSPACE) {
		switch (p->sa->spacetype) {
			case SPACE_VIEW3D:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_3DSPACE;
				break;
			}
			case SPACE_NODE:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
				break;
			}
			case SPACE_SEQ:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima = (SpaceImage *)p->sa->spacedata.first;
				
				/* only set these flags if the image editor doesn't have an image active,
				 * otherwise user will be confused by strokes not appearing after they're drawn
				 *
				 * Admittedly, this is a bit hacky, but it works much nicer from an ergonomic standpoint!
				 */
				if (ELEM(NULL, sima, sima->image)) {
					/* make strokes be drawn in screen space */
					p->gpd->sbuffer_sflag &= ~GP_STROKE_2DSPACE;
					*(p->align_flag) &= ~GP_PROJECT_VIEWSPACE;
				}
				else {
					p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
				}
				break;
			}
			case SPACE_CLIP:
			{
				p->gpd->sbuffer_sflag |= GP_STROKE_2DSPACE;
				break;
			}
		}
	}
}

/* finish off a stroke (clears buffer, but doesn't finish the paint operation) */
static void gp_paint_strokeend(tGPsdata *p)
{
	ToolSettings *ts = p->scene->toolsettings;
	/* for surface sketching, need to set the right OpenGL context stuff so that
	 * the conversions will project the values correctly...
	 */
	if (gpencil_project_check(p)) {
		View3D *v3d = p->sa->spacedata.first;
		
		/* need to restore the original projection settings before packing up */
		view3d_region_operator_needs_opengl(p->win, p->ar);
		ED_view3d_autodist_init(p->scene, p->ar, v3d, (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_STROKE) ? 1 : 0);
	}
	
	/* check if doing eraser or not */
	if ((p->gpd->sbuffer_sflag & GP_STROKE_ERASER) == 0) {
		/* simplify stroke before transferring? */
		gp_stroke_simplify(p);
		
		/* transfer stroke to frame */
		gp_stroke_newfrombuffer(p);
	}
	
	/* clean up buffer now */
	gp_session_validatebuffer(p);
}

/* finish off stroke painting operation */
static void gp_paint_cleanup(tGPsdata *p)
{
	/* p->gpd==NULL happens when stroke failed to initialize,
	 * for example when GP is hidden in current space (sergey)
	 */
	if (p->gpd) {
		/* finish off a stroke */
		gp_paint_strokeend(p);
	}
	
	/* "unlock" frame */
	if (p->gpf)
		p->gpf->flag &= ~GP_FRAME_PAINT;
}

/* ------------------------------- */

/* Helper callback for drawing the cursor itself */
static void gpencil_draw_eraser(bContext *UNUSED(C), int x, int y, void *p_ptr)
{
	tGPsdata *p = (tGPsdata *)p_ptr;
	
	if (p->paintmode == GP_PAINTMODE_ERASER) {
		glPushMatrix();
		
		glTranslatef((float)x, (float)y, 0.0f);
		
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		
		glColor4ub(255, 100, 100, 20);
		glutil_draw_filled_arc(0.0, M_PI * 2.0, p->radius, 40);
		
		setlinestyle(6);
		
		glColor4ub(255, 100, 100, 200);
		glutil_draw_lined_arc(0.0, M_PI * 2.0, p->radius, 40);
		
		setlinestyle(0);
		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
		
		glPopMatrix();
	}
}

/* Turn brush cursor in 3D view on/off */
static void gpencil_draw_toggle_eraser_cursor(bContext *C, tGPsdata *p, short enable)
{
	if (p->erasercursor && !enable) {
		/* clear cursor */
		WM_paint_cursor_end(CTX_wm_manager(C), p->erasercursor);
		p->erasercursor = NULL;
	}
	else if (enable && !p->erasercursor) {
		/* enable cursor */
		p->erasercursor = WM_paint_cursor_activate(CTX_wm_manager(C),
		                                           NULL, /* XXX */
		                                           gpencil_draw_eraser, p);
	}
}

/* Check if tablet eraser is being used (when processing events) */
static bool gpencil_is_tablet_eraser_active(const wmEvent *event)
{
	if (event->tablet_data) {
		const wmTabletData *wmtab = event->tablet_data;
		return (wmtab->Active == EVT_TABLET_ERASER);
	}
	
	return false;
}

/* ------------------------------- */

static void gpencil_draw_exit(bContext *C, wmOperator *op)
{
	tGPsdata *p = op->customdata;
	
	/* clear undo stack */
	gpencil_undo_finish();
	
	/* restore cursor to indicate end of drawing */
	WM_cursor_modal_restore(CTX_wm_window(C));
	
	/* don't assume that operator data exists at all */
	if (p) {
		/* check size of buffer before cleanup, to determine if anything happened here */
		if (p->paintmode == GP_PAINTMODE_ERASER) {
			/* turn off radial brush cursor */
			gpencil_draw_toggle_eraser_cursor(C, p, false);
		}

		/* always store the new eraser size to be used again next time
		 * NOTE: Do this even when not in eraser mode, as eraser may
		 *       have been toggled at some point.
		 */
		U.gp_eraser = p->radius;
		
		/* cleanup */
		gp_paint_cleanup(p);
		gp_session_cleanup(p);
		
		/* finally, free the temp data */
		MEM_freeN(p);
	}
	
	op->customdata = NULL;
}

static void gpencil_draw_cancel(bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	gpencil_draw_exit(C, op);
}

/* ------------------------------- */


static int gpencil_draw_init(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGPsdata *p;
	eGPencil_PaintModes paintmode = RNA_enum_get(op->ptr, "mode");
	
	/* check context */
	p = op->customdata = gp_session_initpaint(C);
	if ((p == NULL) || (p->status == GP_STATUS_ERROR)) {
		/* something wasn't set correctly in context */
		gpencil_draw_exit(C, op);
		return 0;
	}
	
	/* init painting data */
	gp_paint_initstroke(p, paintmode);
	if (p->status == GP_STATUS_ERROR) {
		gpencil_draw_exit(C, op);
		return 0;
	}

	if (event != NULL) {
		p->keymodifier = event->keymodifier;
	}
	else {
		p->keymodifier = -1;
	}
	
	/* everything is now setup ok */
	return 1;
}


/* ------------------------------- */

/* ensure that the correct cursor icon is set */
static void gpencil_draw_cursor_set(tGPsdata *p)
{
	if (p->paintmode == GP_PAINTMODE_ERASER)
		WM_cursor_modal_set(p->win, BC_CROSSCURSOR);  /* XXX need a better cursor */
	else
		WM_cursor_modal_set(p->win, BC_PAINTBRUSHCURSOR);
}

/* update UI indicators of status, including cursor and header prints */
static void gpencil_draw_status_indicators(tGPsdata *p)
{
	/* header prints */
	switch (p->status) {
		case GP_STATUS_PAINTING:
			/* only print this for paint-sessions, otherwise it gets annoying */
			if (GPENCIL_SKETCH_SESSIONS_ON(p->scene))
				ED_area_headerprint(p->sa, IFACE_("Grease Pencil: Drawing/erasing stroke... Release to end stroke"));
			break;
		
		case GP_STATUS_IDLING:
			/* print status info */
			switch (p->paintmode) {
				case GP_PAINTMODE_ERASER:
					ED_area_headerprint(p->sa, IFACE_("Grease Pencil Erase Session: Hold and drag LMB or RMB to erase | "
					                                  "ESC/Enter to end  (or click outside this area)"));
					break;
				case GP_PAINTMODE_DRAW_STRAIGHT:
					ED_area_headerprint(p->sa, IFACE_("Grease Pencil Line Session: Hold and drag LMB to draw | "
					                                  "ESC/Enter to end  (or click outside this area)"));
					break;
				case GP_PAINTMODE_DRAW:
					ED_area_headerprint(p->sa, IFACE_("Grease Pencil Freehand Session: Hold and drag LMB to draw | "
					                                  "E/ESC/Enter to end  (or click outside this area)"));
					break;
				case GP_PAINTMODE_DRAW_POLY:
					ED_area_headerprint(p->sa, IFACE_("Grease Pencil Poly Session: LMB click to place next stroke vertex | "
					                                  "ESC/Enter to end  (or click outside this area)"));
					break;
				
				default: /* unhandled future cases */
					ED_area_headerprint(p->sa, IFACE_("Grease Pencil Session: ESC/Enter to end   (or click outside this area)"));
					break;
			}
			break;
		
		case GP_STATUS_ERROR:
		case GP_STATUS_DONE:
			/* clear status string */
			ED_area_headerprint(p->sa, NULL);
			break;
	}
}

/* ------------------------------- */

/* create a new stroke point at the point indicated by the painting context */
static void gpencil_draw_apply(wmOperator *op, tGPsdata *p)
{
	/* handle drawing/erasing -> test for erasing first */
	if (p->paintmode == GP_PAINTMODE_ERASER) {
		/* do 'live' erasing now */
		gp_stroke_doeraser(p);
		
		/* store used values */
		p->mvalo[0] = p->mval[0];
		p->mvalo[1] = p->mval[1];
		p->opressure = p->pressure;
	}
	/* only add current point to buffer if mouse moved (even though we got an event, it might be just noise) */
	else if (gp_stroke_filtermval(p, p->mval, p->mvalo)) {
		/* try to add point */
		short ok = gp_stroke_addpoint(p, p->mval, p->pressure, p->curtime);
		
		/* handle errors while adding point */
		if ((ok == GP_STROKEADD_FULL) || (ok == GP_STROKEADD_OVERFLOW)) {
			/* finish off old stroke */
			gp_paint_strokeend(p);
			/* And start a new one!!! Else, projection errors! */
			gp_paint_initstroke(p, p->paintmode);
			
			/* start a new stroke, starting from previous point */
			/* XXX Must manually reset inittime... */
			/* XXX We only need to reuse previous point if overflow! */
			if (ok == GP_STROKEADD_OVERFLOW) {
				p->inittime = p->ocurtime;
				gp_stroke_addpoint(p, p->mvalo, p->opressure, p->ocurtime);
			}
			else {
				p->inittime = p->curtime;
			}
			gp_stroke_addpoint(p, p->mval, p->pressure, p->curtime);
		}
		else if (ok == GP_STROKEADD_INVALID) {
			/* the painting operation cannot continue... */
			BKE_report(op->reports, RPT_ERROR, "Cannot paint stroke");
			p->status = GP_STATUS_ERROR;
			
			if (G.debug & G_DEBUG)
				printf("Error: Grease-Pencil Paint - Add Point Invalid\n");
			return;
		}
		
		/* store used values */
		p->mvalo[0] = p->mval[0];
		p->mvalo[1] = p->mval[1];
		p->opressure = p->pressure;
		p->ocurtime = p->curtime;
	}
}

/* handle draw event */
static void gpencil_draw_apply_event(wmOperator *op, const wmEvent *event)
{
	tGPsdata *p = op->customdata;
	PointerRNA itemptr;
	float mousef[2];
	int tablet = 0;
	
	/* convert from window-space to area-space mouse coordinates
	 * NOTE: float to ints conversions, +1 factor is probably used to ensure a bit more accurate rounding...
	 */
	p->mval[0] = event->mval[0] + 1;
	p->mval[1] = event->mval[1] + 1;
	
	/* verify key status for straight lines */
	if ((event->ctrl > 0) || (event->alt > 0)) {
		if (p->straight[0] == 0) {
			int dx = abs(p->mval[0] - p->mvalo[0]);
			int dy = abs(p->mval[1] - p->mvalo[1]);
			if ((dx > 0) || (dy > 0)) {
				/* check mouse direction to replace the other coordinate with previous values */
				if (dx >= dy) {
					/* horizontal */
					p->straight[0] = 1;
					p->straight[1] = p->mval[1]; /* save y */
				}
				else {
					/* vertical */
					p->straight[0] = 2;
					p->straight[1] = p->mval[0]; /* save x */
				}
			}
		}
	}
	else {
		p->straight[0] = 0;
	}

	p->curtime = PIL_check_seconds_timer();
	
	/* handle pressure sensitivity (which is supplied by tablets) */
	if (event->tablet_data) {
		const wmTabletData *wmtab = event->tablet_data;
		
		tablet = (wmtab->Active != EVT_TABLET_NONE);
		p->pressure = wmtab->Pressure;
		
		/* Hack for pressure sensitive eraser on D+RMB when using a tablet:
		 *  The pen has to float over the tablet surface, resulting in
		 *  zero pressure (T47101). Ignore pressure values if floating
		 *  (i.e. "effectively zero" pressure), and only when the "active"
		 *  end is the stylus (i.e. the default when not eraser)
		 */
		if (p->paintmode == GP_PAINTMODE_ERASER) {
			if ((wmtab->Active != EVT_TABLET_ERASER) && (p->pressure < 0.001f)) {
				p->pressure = 1.0f;
			}
		}
	}
	else {
		/* No tablet data -> No pressure info is available */
		p->pressure = 1.0f;
	}
	
	/* special exception for start of strokes (i.e. maybe for just a dot) */
	if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
		p->flags &= ~GP_PAINTFLAG_FIRSTRUN;
		
		p->mvalo[0] = p->mval[0];
		p->mvalo[1] = p->mval[1];
		p->opressure = p->pressure;
		p->inittime = p->ocurtime = p->curtime;
		p->straight[0] = 0;
		p->straight[1] = 0;

		/* special exception here for too high pressure values on first touch in
		 *  windows for some tablets, then we just skip first touch...
		 */
		if (tablet && (p->pressure >= 0.99f))
			return;
	}
	
	/* check if alt key is pressed and limit to straight lines */
	if (p->straight[0] != 0) {
		if (p->straight[0] == 1) {
			/* horizontal */
			p->mval[1] = p->straight[1]; /* replace y */
		}
		else {
			/* vertical */
			p->mval[0] = p->straight[1]; /* replace x */
		}
	}

	/* fill in stroke data (not actually used directly by gpencil_draw_apply) */
	RNA_collection_add(op->ptr, "stroke", &itemptr);
	
	mousef[0] = p->mval[0];
	mousef[1] = p->mval[1];
	RNA_float_set_array(&itemptr, "mouse", mousef);
	RNA_float_set(&itemptr, "pressure", p->pressure);
	RNA_boolean_set(&itemptr, "is_start", (p->flags & GP_PAINTFLAG_FIRSTRUN) != 0);
	
	RNA_float_set(&itemptr, "time", p->curtime - p->inittime);
	
	/* apply the current latest drawing point */
	gpencil_draw_apply(op, p);
	
	/* force refresh */
	ED_region_tag_redraw(p->ar); /* just active area for now, since doing whole screen is too slow */
}

/* ------------------------------- */

/* operator 'redo' (i.e. after changing some properties, but also for repeat last) */
static int gpencil_draw_exec(bContext *C, wmOperator *op)
{
	tGPsdata *p = NULL;
	
	/* printf("GPencil - Starting Re-Drawing\n"); */
	
	/* try to initialize context data needed while drawing */
	if (!gpencil_draw_init(C, op, NULL)) {
		if (op->customdata) MEM_freeN(op->customdata);
		/* printf("\tGP - no valid data\n"); */
		return OPERATOR_CANCELLED;
	}
	else
		p = op->customdata;
	
	/* printf("\tGP - Start redrawing stroke\n"); */
	
	/* loop over the stroke RNA elements recorded (i.e. progress of mouse movement),
	 * setting the relevant values in context at each step, then applying
	 */
	RNA_BEGIN (op->ptr, itemptr, "stroke")
	{
		float mousef[2];
		
		/* printf("\t\tGP - stroke elem\n"); */
		
		/* get relevant data for this point from stroke */
		RNA_float_get_array(&itemptr, "mouse", mousef);
		p->mval[0] = (int)mousef[0];
		p->mval[1] = (int)mousef[1];
		p->pressure = RNA_float_get(&itemptr, "pressure");
		p->curtime = (double)RNA_float_get(&itemptr, "time") + p->inittime;
		
		if (RNA_boolean_get(&itemptr, "is_start")) {
			/* if first-run flag isn't set already (i.e. not true first stroke),
			 * then we must terminate the previous one first before continuing
			 */
			if ((p->flags & GP_PAINTFLAG_FIRSTRUN) == 0) {
				/* TODO: both of these ops can set error-status, but we probably don't need to worry */
				gp_paint_strokeend(p);
				gp_paint_initstroke(p, p->paintmode);
			}
		}
		
		/* if first run, set previous data too */
		if (p->flags & GP_PAINTFLAG_FIRSTRUN) {
			p->flags &= ~GP_PAINTFLAG_FIRSTRUN;
			
			p->mvalo[0] = p->mval[0];
			p->mvalo[1] = p->mval[1];
			p->opressure = p->pressure;
			p->ocurtime = p->curtime;
		}
		
		/* apply this data as necessary now (as per usual) */
		gpencil_draw_apply(op, p);
	}
	RNA_END;
		
	/* printf("\tGP - done\n"); */
	
	/* cleanup */
	gpencil_draw_exit(C, op);
	
	/* refreshes */
	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
	
	/* done */
	return OPERATOR_FINISHED;
}

/* ------------------------------- */

/* start of interactive drawing part of operator */
static int gpencil_draw_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGPsdata *p = NULL;
	
	if (G.debug & G_DEBUG)
		printf("GPencil - Starting Drawing\n");
	
	/* try to initialize context data needed while drawing */
	if (!gpencil_draw_init(C, op, event)) {
		if (op->customdata)
			MEM_freeN(op->customdata);
		if (G.debug & G_DEBUG)
			printf("\tGP - no valid data\n");
		return OPERATOR_CANCELLED;
	}
	else
		p = op->customdata;
	
	/* TODO: set any additional settings that we can take from the events?
	 * TODO? if tablet is erasing, force eraser to be on? */
	
	/* TODO: move cursor setting stuff to stroke-start so that paintmode can be changed midway... */
	
	/* if eraser is on, draw radial aid */
	if (p->paintmode == GP_PAINTMODE_ERASER) {
		gpencil_draw_toggle_eraser_cursor(C, p, true);
	}
	/* set cursor 
	 * NOTE: This may change later (i.e. intentionally via brush toggle,
	 *       or unintentionally if the user scrolls outside the area)...
	 */
	gpencil_draw_cursor_set(p);

	/* only start drawing immediately if we're allowed to do so... */
	if (RNA_boolean_get(op->ptr, "wait_for_input") == false) {
		/* hotkey invoked - start drawing */
		/* printf("\tGP - set first spot\n"); */
		p->status = GP_STATUS_PAINTING;

		/* handle the initial drawing - i.e. for just doing a simple dot */
		gpencil_draw_apply_event(op, event);
		op->flag |= OP_IS_MODAL_CURSOR_REGION;
	}
	else {
		/* toolbar invoked - don't start drawing yet... */
		/* printf("\tGP - hotkey invoked... waiting for click-drag\n"); */
		op->flag |= OP_IS_MODAL_CURSOR_REGION;
	}
	
	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
	/* add a modal handler for this operator, so that we can then draw continuous strokes */
	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

/* gpencil modal operator stores area, which can be removed while using it (like fullscreen) */
static bool gpencil_area_exists(bContext *C, ScrArea *sa_test)
{
	bScreen *sc = CTX_wm_screen(C);
	return (BLI_findindex(&sc->areabase, sa_test) != -1);
}

static tGPsdata *gpencil_stroke_begin(bContext *C, wmOperator *op)
{
	tGPsdata *p = op->customdata;
	
	/* we must check that we're still within the area that we're set up to work from
	 * otherwise we could crash (see bug #20586)
	 */
	if (CTX_wm_area(C) != p->sa) {
		printf("\t\t\tGP - wrong area execution abort!\n");
		p->status = GP_STATUS_ERROR;
	}
	
	/* printf("\t\tGP - start stroke\n"); */
	
	/* we may need to set up paint env again if we're resuming */
	/* XXX: watch it with the paintmode! in future,
	 *      it'd be nice to allow changing paint-mode when in sketching-sessions */
	
	if (gp_session_initdata(C, p))
		gp_paint_initstroke(p, p->paintmode);
	
	if (p->status != GP_STATUS_ERROR) {
		p->status = GP_STATUS_PAINTING;
		op->flag &= ~OP_IS_MODAL_CURSOR_REGION;
	}
	
	return op->customdata;
}

static void gpencil_stroke_end(wmOperator *op)
{
	tGPsdata *p = op->customdata;
	
	gp_paint_cleanup(p);
	
	gpencil_undo_push(p->gpd);
	
	gp_session_cleanup(p);
	
	p->status = GP_STATUS_IDLING;
	op->flag |= OP_IS_MODAL_CURSOR_REGION;
	
	p->gpd = NULL;
	p->gpl = NULL;
	p->gpf = NULL;
}

/* Move last stroke in the listbase to the head to be drawn below all previous strokes in the layer */
static void gpencil_move_last_stroke_to_back(bContext *C)
{
	/* move last stroke (the polygon) to head of the listbase stroke to draw on back of all previous strokes */
	bGPdata *gpd = ED_gpencil_data_get_active(C);
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);

	/* sanity checks */
	if (ELEM(NULL, gpd, gpl, gpl->actframe)) {
		return;
	}

	bGPDframe *gpf = gpl->actframe;
	bGPDstroke *gps = gpf->strokes.last;
	if (ELEM(NULL, gps)) {
		return;
	}

	BLI_remlink(&gpf->strokes, gps);
	BLI_insertlinkbefore(&gpf->strokes, gpf->strokes.first, gps);
}

/* events handling during interactive drawing part of operator */
static int gpencil_draw_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGPsdata *p = op->customdata;
	ToolSettings *ts = CTX_data_tool_settings(C);
	int estate = OPERATOR_PASS_THROUGH; /* default exit state - pass through to support MMB view nav, etc. */
	
	/* if (event->type == NDOF_MOTION)
	 *    return OPERATOR_PASS_THROUGH;
	 * -------------------------------
	 * [mce] Not quite what I was looking
	 * for, but a good start! GP continues to
	 * draw on the screen while the 3D mouse
	 * moves the viewpoint. Problem is that
	 * the stroke is converted to 3D only after
	 * it is finished. This approach should work
	 * better in tools that immediately apply
	 * in 3D space.
	 */
	
	if (p->status == GP_STATUS_IDLING) {
		ARegion *ar = CTX_wm_region(C);
		p->ar = ar;
	}

	/* we don't pass on key events, GP is used with key-modifiers - prevents Dkey to insert drivers */
	if (ISKEYBOARD(event->type)) {
		if (ELEM(event->type, LEFTARROWKEY, DOWNARROWKEY, RIGHTARROWKEY, UPARROWKEY, ZKEY)) {
			/* allow some keys:
			 *   - for frame changing [#33412]
			 *   - for undo (during sketching sessions)
			 */
		}
		else if (ELEM(event->type, PAD0, PAD1, PAD2, PAD3, PAD4, PAD5, PAD6, PAD7, PAD8, PAD9)) {
			/* allow numpad keys so that camera/view manipulations can still take place
			 * - PAD0 in particular is really important for Grease Pencil drawing,
			 *   as animators may be working "to camera", so having this working
			 *   is essential for ensuring that they can quickly return to that view
			 */
		}
		else if ((ELEM(event->type, p->keymodifier)) && (event->val == KM_RELEASE)) {
			/* enable continuous if release D key in mid drawing */
			p->scene->toolsettings->gpencil_flags |= GP_TOOL_FLAG_PAINTSESSIONS_ON;
		}
		else if ((event->type == BKEY) && (event->val == KM_RELEASE)) {
			/* Add Blank Frame
			 * - Since this operator is non-modal, we can just call it here, and keep going...
	         * - This operator is especially useful when animating
			 */
			WM_operator_name_call(C, "GPENCIL_OT_blank_frame_add", WM_OP_EXEC_DEFAULT, NULL);
			estate = OPERATOR_RUNNING_MODAL;
		}
		else {
			estate = OPERATOR_RUNNING_MODAL;
		}
	}
	
	//printf("\tGP - handle modal event...\n");
	
	/* exit painting mode (and/or end current stroke)
	 * NOTE: cannot do RIGHTMOUSE (as is standard for canceling) as that would break polyline [#32647]
	 */
	if (ELEM(event->type, RETKEY, PADENTER, ESCKEY, SPACEKEY, EKEY)) {
		/* exit() ends the current stroke before cleaning up */
		/* printf("\t\tGP - end of paint op + end of stroke\n"); */
		/* if drawing polygon and enable on back, must move stroke */
		if (ts) {
			if ((ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) && (p->paintmode == GP_PAINTMODE_DRAW_POLY)) {
				if (p->flags & GP_PAINTFLAG_STROKEADDED) {
					gpencil_move_last_stroke_to_back(C);
				}
			}
		}
		p->status = GP_STATUS_DONE;
		estate = OPERATOR_FINISHED;
	}
	
	/* toggle painting mode upon mouse-button movement
	 *  - LEFTMOUSE  = standard drawing (all) / straight line drawing (all) / polyline (toolbox only)
	 *  - RIGHTMOUSE = polyline (hotkey) / eraser (all)
	 *    (Disabling RIGHTMOUSE case here results in bugs like [#32647])
	 * also making sure we have a valid event value, to not exit too early
	 */
	if (ELEM(event->type, LEFTMOUSE, RIGHTMOUSE) && (event->val != KM_NOTHING)) {
		/* if painting, end stroke */
		if (p->status == GP_STATUS_PAINTING) {
			int sketch = 0;
			
			/* basically, this should be mouse-button up = end stroke
			 * BUT what happens next depends on whether we 'painting sessions' is enabled
			 */
			sketch |= GPENCIL_SKETCH_SESSIONS_ON(p->scene);
			/* polyline drawing is also 'sketching' -- all knots should be added during one session */
			sketch |= (p->paintmode == GP_PAINTMODE_DRAW_POLY);
			
			if (sketch) {
				/* end stroke only, and then wait to resume painting soon */
				/* printf("\t\tGP - end stroke only\n"); */
				gpencil_stroke_end(op);
				
				/* If eraser mode is on, turn it off after the stroke finishes
				 * NOTE: This just makes it nicer to work with drawing sessions
				 */
				if (p->paintmode == GP_PAINTMODE_ERASER) {
					p->paintmode = RNA_enum_get(op->ptr, "mode");
					
					/* if the original mode was *still* eraser,
					 * we'll let it say for now, since this gives
					 * users an opportunity to have visual feedback
					 * when adjusting eraser size
					 */
					if (p->paintmode != GP_PAINTMODE_ERASER) {	
						/* turn off cursor...
						 * NOTE: this should be enough for now
						 *       Just hiding this makes it seem like
						 *       you can paint again...
						 */
						gpencil_draw_toggle_eraser_cursor(C, p, false);
					}
				}
				
				/* we've just entered idling state, so this event was processed (but no others yet) */
				estate = OPERATOR_RUNNING_MODAL;
				
				/* stroke could be smoothed, send notifier to refresh screen */
				WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
			}
			else {
				/* printf("\t\tGP - end of stroke + op\n"); */
				/* if drawing polygon and enable on back, must move stroke */
				if (ts) {
					if ((ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) && (p->paintmode == GP_PAINTMODE_DRAW_POLY)) {
						if (p->flags & GP_PAINTFLAG_STROKEADDED) {
							gpencil_move_last_stroke_to_back(C);
						}
					}
				}
				p->status = GP_STATUS_DONE;
				estate = OPERATOR_FINISHED;
			}
		}
		else if (event->val == KM_PRESS) {
			bool in_bounds = false;
			
			/* Check if we're outside the bounds of the active region
			 * NOTE: An exception here is that if launched from the toolbar,
			 *       whatever region we're now in should become the new region
			 */
			if ((p->ar) && (p->ar->regiontype == RGN_TYPE_TOOLS)) {
				/* Change to whatever region is now under the mouse */
				ARegion *current_region = BKE_area_find_region_xy(p->sa, RGN_TYPE_ANY, event->x, event->y);
				
				if (G.debug & G_DEBUG) {
					printf("found alternative region %p (old was %p) - at %d %d (sa: %d %d -> %d %d)\n",
						current_region, p->ar, event->x, event->y,
						p->sa->totrct.xmin, p->sa->totrct.ymin, p->sa->totrct.xmax, p->sa->totrct.ymax);
				}
				
				if (current_region) {
					/* Assume that since we found the cursor in here, it is in bounds
					 * and that this should be the region that we begin drawing in
					 */
					p->ar = current_region;
					in_bounds = true;
				}
				else {
					/* Out of bounds, or invalid in some other way */
					p->status = GP_STATUS_ERROR;
					estate = OPERATOR_CANCELLED;
					
					if (G.debug & G_DEBUG)
						printf("%s: Region under cursor is out of bounds, so cannot be drawn on\n", __func__);
				}
			}
			else if (p->ar) {
				rcti region_rect;
				
				/* Perform bounds check using  */
				ED_region_visible_rect(p->ar, &region_rect);
				in_bounds = BLI_rcti_isect_pt_v(&region_rect, event->mval);
			}
			else {
				/* No region */
				p->status = GP_STATUS_ERROR;
				estate = OPERATOR_CANCELLED;
				
				if (G.debug & G_DEBUG)
					printf("%s: No active region found in GP Paint session data\n", __func__);
			}
			
			if (in_bounds) {
				/* Switch paintmode (temporarily if need be) based on which button was used
				 * NOTE: This is to make it more convenient to erase strokes when using drawing sessions
				 */
				if ((event->type == RIGHTMOUSE) || gpencil_is_tablet_eraser_active(event)) {
					/* turn on eraser */
					p->paintmode = GP_PAINTMODE_ERASER;
				}
				else if (event->type == LEFTMOUSE) {
					/* restore drawmode to default */
					p->paintmode = RNA_enum_get(op->ptr, "mode");
				}
				
				gpencil_draw_toggle_eraser_cursor(C, p, p->paintmode == GP_PAINTMODE_ERASER);
				
				/* not painting, so start stroke (this should be mouse-button down) */
				p = gpencil_stroke_begin(C, op);
				
				if (p->status == GP_STATUS_ERROR) {
					estate = OPERATOR_CANCELLED;
				}
			}
			else if (p->status != GP_STATUS_ERROR) {
				/* User clicked outside bounds of window while idling, so exit paintmode 
				 * NOTE: Don't enter this case if an error occurred while finding the
				 *       region (as above)
				 */
				/* if drawing polygon and enable on back, must move stroke */
				if (ts) {
					if ((ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) && (p->paintmode == GP_PAINTMODE_DRAW_POLY)) {
						if (p->flags & GP_PAINTFLAG_STROKEADDED) {
							gpencil_move_last_stroke_to_back(C);
						}
					}
				}
				p->status = GP_STATUS_DONE;
				estate = OPERATOR_FINISHED;
			}
		}
		else {
			p->status = GP_STATUS_IDLING;
			op->flag |= OP_IS_MODAL_CURSOR_REGION;
		}
	}
	
	/* handle mode-specific events */
	if (p->status == GP_STATUS_PAINTING) {
		/* handle painting mouse-movements? */
		if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE) || (p->flags & GP_PAINTFLAG_FIRSTRUN)) {
			/* handle drawing event */
			/* printf("\t\tGP - add point\n"); */
			gpencil_draw_apply_event(op, event);
			
			/* finish painting operation if anything went wrong just now */
			if (p->status == GP_STATUS_ERROR) {
				printf("\t\t\t\tGP - add error done!\n");
				estate = OPERATOR_CANCELLED;
			}
			else {
				/* event handled, so just tag as running modal */
				/* printf("\t\t\t\tGP - add point handled!\n"); */
				estate = OPERATOR_RUNNING_MODAL;
			}
		}
		/* eraser size */
		else if ((p->paintmode == GP_PAINTMODE_ERASER) &&
		         ELEM(event->type, WHEELUPMOUSE, WHEELDOWNMOUSE, PADPLUSKEY, PADMINUS))
		{
			/* just resize the brush (local version)
			 * TODO: fix the hardcoded size jumps (set to make a visible difference) and hardcoded keys
			 */
			/* printf("\t\tGP - resize eraser\n"); */
			switch (event->type) {
				case WHEELDOWNMOUSE: /* larger */
				case PADPLUSKEY:
					p->radius += 5;
					break;
				
				case WHEELUPMOUSE: /* smaller */
				case PADMINUS:
					p->radius -= 5;
					
					if (p->radius <= 0)
						p->radius = 1;
					break;
			}
			
			/* force refresh */
			ED_region_tag_redraw(p->ar); /* just active area for now, since doing whole screen is too slow */
			
			/* event handled, so just tag as running modal */
			estate = OPERATOR_RUNNING_MODAL;
		}
		/* there shouldn't be any other events, but just in case there are, let's swallow them
		 * (i.e. to prevent problems with undo)
		 */
		else {
			/* swallow event to save ourselves trouble */
			estate = OPERATOR_RUNNING_MODAL;
		}
	}
	
	/* gpencil modal operator stores area, which can be removed while using it (like fullscreen) */
	if (0 == gpencil_area_exists(C, p->sa))
		estate = OPERATOR_CANCELLED;
	else {
		/* update status indicators - cursor, header, etc. */
		gpencil_draw_status_indicators(p);
		gpencil_draw_cursor_set(p); /* cursor may have changed outside our control - T44084 */
	}
	
	/* process last operations before exiting */
	switch (estate) {
		case OPERATOR_FINISHED:
			/* one last flush before we're done */
			gpencil_draw_exit(C, op);
			WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
			break;
		
		case OPERATOR_CANCELLED:
			gpencil_draw_exit(C, op);
			break;
		
		case OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH:
			/* event doesn't need to be handled */
#if 0
			printf("unhandled event -> %d (mmb? = %d | mmv? = %d)\n",
			       event->type, event->type == MIDDLEMOUSE, event->type==MOUSEMOVE);
#endif
			break;
	}
	
	/* return status code */
	return estate;
}

/* ------------------------------- */

static EnumPropertyItem prop_gpencil_drawmodes[] = {
	{GP_PAINTMODE_DRAW, "DRAW", 0, "Draw Freehand", "Draw freehand stroke(s)"},
	{GP_PAINTMODE_DRAW_STRAIGHT, "DRAW_STRAIGHT", 0, "Draw Straight Lines", "Draw straight line segment(s)"},
	{GP_PAINTMODE_DRAW_POLY, "DRAW_POLY", 0, "Draw Poly Line", "Click to place endpoints of straight line segments (connected)"},
	{GP_PAINTMODE_ERASER, "ERASER", 0, "Eraser", "Erase Grease Pencil strokes"},
	{0, NULL, 0, NULL, NULL}
};

void GPENCIL_OT_draw(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Grease Pencil Draw";
	ot->idname = "GPENCIL_OT_draw";
	ot->description = "Make annotations on the active data";
	
	/* api callbacks */
	ot->exec = gpencil_draw_exec;
	ot->invoke = gpencil_draw_invoke;
	ot->modal = gpencil_draw_modal;
	ot->cancel = gpencil_draw_cancel;
	ot->poll = gpencil_draw_poll;
	
	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;
	
	/* settings for drawing */
	ot->prop = RNA_def_enum(ot->srna, "mode", prop_gpencil_drawmodes, 0, "Mode", "Way to interpret mouse movements");
	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
	
	/* NOTE: wait for input is enabled by default, so that all UI code can work properly without needing users to know about this */
	RNA_def_boolean(ot->srna, "wait_for_input", true, "Wait for Input", "Wait for first click instead of painting immediately");
}
