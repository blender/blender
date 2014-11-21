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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_ruler.c
 *  \ingroup spview3d
 */

/* defines VIEW3D_OT_ruler modal operator */

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_unit.h"
#include "BKE_gpencil.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "BLF_api.h"
#include "BIF_glutil.h"

#include "UI_resources.h"
#include "UI_interface.h"

#include "view3d_intern.h"  /* own include */


/* -------------------------------------------------------------------- */
/* Snapping (could be own function) */
/* NOTE - this is not very nice use of transform snapping */
#include "ED_transform.h"

#define MVAL_MAX_PX_DIST 12.0f

/**
 * Convenience function for performing snapping.
 *
 * \param C  Context.
 * \param r_co  hit location.
 * \param r_no  hit normal (optional).
 * \param co_ss  Screenspace coordinate.
 * \param use_depth  Snap to the closest element, use when using more than one snap type.
 * \param use_obedit  Use editmode cage.
 * \param use_vert  Snap to verts.
 * \param use_edge  Snap to edges.
 * \param use_face  Snap to faces.
 * \return Snap success
 */
static bool ED_view3d_snap_co(bContext *C, float r_co[3], float r_no[3], const float co_ss[2],
                              bool use_depth, bool use_obedit,
                              bool use_vert, bool use_edge, bool use_face)
{
	float dist_px = MVAL_MAX_PX_DIST;  /* snap dist */
	float r_no_dummy[3];
	float ray_dist = TRANSFORM_DIST_MAX_RAY;
	bool ret = false;
	float *r_no_ptr = r_no ? r_no : r_no_dummy;

	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	struct Object *obedit = use_obedit ? CTX_data_edit_object(C) : NULL;

	BLI_assert(use_vert || use_edge || use_face);

	/* try snap edge, then face if it fails */
	if (use_vert) {
		ret |= snapObjectsEx(scene, NULL, v3d, ar, obedit, SCE_SNAP_MODE_VERTEX,
		                     co_ss, &dist_px, r_co, r_no_ptr, &ray_dist, SNAP_ALL);
	}
	if (use_edge && (ret == false || use_depth)) {
		if (use_depth == false) ray_dist = TRANSFORM_DIST_MAX_RAY;
		ret |= snapObjectsEx(scene, NULL, v3d, ar, obedit, SCE_SNAP_MODE_EDGE,
		                     co_ss, &dist_px, r_co, r_no_ptr, &ray_dist, SNAP_ALL);
	}
	if (use_face && (ret == false || use_depth)) {
		if (use_depth == false) ray_dist = TRANSFORM_DIST_MAX_RAY;
		ret |= snapObjectsEx(scene, NULL, v3d, ar, obedit, SCE_SNAP_MODE_FACE,
		                     co_ss, &dist_px, r_co, r_no_ptr, &ray_dist, SNAP_ALL);
	}

	return ret;
}

static bool ED_view3d_snap_ray(bContext *C, float r_co[3],
                               const float ray_start[3], const float ray_normal[3])
{
	float dist_px = MVAL_MAX_PX_DIST;  /* snap dist */
	float r_no_dummy[3];
	float ray_dist = TRANSFORM_DIST_MAX_RAY;
	bool ret;

	Scene *scene = CTX_data_scene(C);
	struct Object *obedit = CTX_data_edit_object(C);

	/* try snap edge, then face if it fails */
	ret = snapObjectsRayEx(scene, NULL, NULL, NULL, obedit, SCE_SNAP_MODE_FACE,
	                       NULL, NULL,
	                       ray_start, ray_normal, &ray_dist,
	                       NULL, &dist_px, r_co, r_no_dummy, SNAP_ALL);

	return ret;
}
/* done snapping */


/* -------------------------------------------------------------------- */
/* Ruler Item (we can have many) */
enum {
	RULERITEM_USE_ANGLE = (1 << 0),  /* use protractor */
	RULERITEM_USE_RAYCAST = (1 << 1)
};

enum {
	RULERITEM_DIRECTION_IN = 0,
	RULERITEM_DIRECTION_OUT
};

/* keep smaller then selection, since we may want click elsewhere without selecting a ruler */
#define RULER_PICK_DIST 12.0f
#define RULER_PICK_DIST_SQ (RULER_PICK_DIST * RULER_PICK_DIST)

typedef struct RulerItem {
	struct RulerItem *next, *prev;

	/* worldspace coords, middle being optional */
	float co[3][3];

	/* selected coord */
	char  co_index; /* 0 -> 2*/

	int   flag;
	int   raycast_dir;  /* RULER_DIRECTION_* */
} RulerItem;


/* -------------------------------------------------------------------- */
/* Ruler Info (one per session) */

enum {
	RULER_STATE_NORMAL = 0,
	RULER_STATE_DRAG
};

enum {
	RULER_SNAP_OK = (1 << 0),
};

typedef struct RulerInfo {
	ListBase items;
	int      item_active;
	int flag;
	int snap_flag;
	int state;
	float drag_start_co[3];

	/* wm state */
	wmWindow *win;
	ARegion *ar;
	void *draw_handle_pixel;
} RulerInfo;

/* -------------------------------------------------------------------- */
/* local functions */
static RulerItem *ruler_item_add(RulerInfo *ruler_info)
{
	RulerItem *ruler_item = MEM_callocN(sizeof(RulerItem), "RulerItem");
	BLI_addtail(&ruler_info->items, ruler_item);
	return ruler_item;
}

static void ruler_item_remove(RulerInfo *ruler_info, RulerItem *ruler_item)
{
	BLI_remlink(&ruler_info->items, ruler_item);
	MEM_freeN(ruler_item);
}

static RulerItem *ruler_item_active_get(RulerInfo *ruler_info)
{
	return BLI_findlink(&ruler_info->items, ruler_info->item_active);
}

static void ruler_item_active_set(RulerInfo *ruler_info, RulerItem *ruler_item)
{
	ruler_info->item_active = BLI_findindex(&ruler_info->items, ruler_item);
}

static void ruler_item_as_string(RulerItem *ruler_item, UnitSettings *unit,
                                 char *numstr, size_t numstr_size, int prec)
{
	const bool do_split = (unit->flag & USER_UNIT_OPT_SPLIT) != 0;

	if (ruler_item->flag & RULERITEM_USE_ANGLE) {
		const float ruler_angle = angle_v3v3v3(ruler_item->co[0],
		                                       ruler_item->co[1],
		                                       ruler_item->co[2]);

		if (unit->system == USER_UNIT_NONE) {
			BLI_snprintf(numstr, numstr_size, "%.*f°", prec, RAD2DEGF(ruler_angle));
		}
		else {
			bUnit_AsString(numstr, numstr_size,
			               (double)ruler_angle,
			               prec, unit->system, B_UNIT_ROTATION, do_split, false);
		}
	}
	else {
		const float ruler_len = len_v3v3(ruler_item->co[0],
		                                 ruler_item->co[2]);

		if (unit->system == USER_UNIT_NONE) {
			BLI_snprintf(numstr, numstr_size, "%.*f", prec, ruler_len);
		}
		else {
			bUnit_AsString(numstr, numstr_size,
			               (double)(ruler_len * unit->scale_length),
			               prec, unit->system, B_UNIT_LENGTH, do_split, false);
		}
	}

}

static bool view3d_ruler_pick(RulerInfo *ruler_info, const float mval[2],
                              RulerItem **r_ruler_item, int *r_co_index)
{
	ARegion *ar = ruler_info->ar;
	RulerItem *ruler_item;

	float dist_best = RULER_PICK_DIST_SQ;
	RulerItem *ruler_item_best = NULL;
	int co_index_best = -1;

	for (ruler_item = ruler_info->items.first; ruler_item; ruler_item = ruler_item->next) {
		float co_ss[3][2];
		float dist;
		int j;

		/* should these be checked? - ok for now not to */
		for (j = 0; j < 3; j++) {
			ED_view3d_project_float_global(ar, ruler_item->co[j], co_ss[j], V3D_PROJ_TEST_NOP);
		}

		if (ruler_item->flag & RULERITEM_USE_ANGLE) {
			dist = min_ff(dist_squared_to_line_segment_v2(mval, co_ss[0], co_ss[1]),
			              dist_squared_to_line_segment_v2(mval, co_ss[1], co_ss[2]));
			if (dist < dist_best) {
				dist_best = dist;
				ruler_item_best = ruler_item;

				{
					const float dist_points[3] = {
					    len_squared_v2v2(co_ss[0], mval),
					    len_squared_v2v2(co_ss[1], mval),
					    len_squared_v2v2(co_ss[2], mval),
					};
					if (min_fff(UNPACK3(dist_points)) < RULER_PICK_DIST_SQ) {
						co_index_best = min_axis_v3(dist_points);
					}
					else {
						co_index_best = -1;
					}
				}
			}
		}
		else {
			dist = dist_squared_to_line_segment_v2(mval, co_ss[0], co_ss[2]);
			if (dist < dist_best) {
				dist_best = dist;
				ruler_item_best = ruler_item;

				{
					const float dist_points[2] = {
					    len_squared_v2v2(co_ss[0], mval),
					    len_squared_v2v2(co_ss[2], mval),
					};
					if (min_ff(UNPACK2(dist_points)) < RULER_PICK_DIST_SQ) {
						co_index_best = (dist_points[0] < dist_points[1]) ? 0 : 2;
					}
					else {
						co_index_best = -1;
					}
				}
			}
		}
	}

	if (ruler_item_best) {
		*r_ruler_item = ruler_item_best;
		*r_co_index = co_index_best;
		return true;
	}
	else {
		*r_ruler_item = NULL;
		*r_co_index = -1;
		return false;
	}
}

#define RULER_ID "RulerData3D"
static bool view3d_ruler_to_gpencil(bContext *C, RulerInfo *ruler_info)
{
	Scene *scene = CTX_data_scene(C);
	bGPDlayer *gpl;
	bGPDframe *gpf;
	bGPDstroke *gps;
	RulerItem *ruler_item;
	const char *ruler_name = RULER_ID;
	bool changed = false;

	if (scene->gpd == NULL) {
		scene->gpd = gpencil_data_addnew("GPencil");
	}

	gpl = BLI_findstring(&scene->gpd->layers, ruler_name, offsetof(bGPDlayer, info));
	if (gpl == NULL) {
		gpl = gpencil_layer_addnew(scene->gpd, ruler_name, false);
		gpl->thickness = 1;
		gpl->flag |= GP_LAYER_HIDE;
	}

	gpf = gpencil_layer_getframe(gpl, CFRA, true);
	free_gpencil_strokes(gpf);

	for (ruler_item = ruler_info->items.first; ruler_item; ruler_item = ruler_item->next) {
		bGPDspoint *pt;
		int j;

		/* allocate memory for a new stroke */
		gps = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
		if (ruler_item->flag & RULERITEM_USE_ANGLE) {
			gps->totpoints = 3;
			pt = gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
			for (j = 0; j < 3; j++) {
				copy_v3_v3(&pt->x, ruler_item->co[j]);
				pt->pressure = 1.0f;
				pt++;
			}
		}
		else {
			gps->totpoints = 2;
			pt = gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
			for (j = 0; j < 3; j += 2) {
				copy_v3_v3(&pt->x, ruler_item->co[j]);
				pt->pressure = 1.0f;
				pt++;
			}
		}
		gps->flag = GP_STROKE_3DSPACE;
		BLI_addtail(&gpf->strokes, gps);
		changed = true;
	}

	return changed;
}

static bool view3d_ruler_from_gpencil(bContext *C, RulerInfo *ruler_info)
{
	Scene *scene = CTX_data_scene(C);
	bool changed = false;

	if (scene->gpd) {
		bGPDlayer *gpl;
		const char *ruler_name = RULER_ID;
		gpl = BLI_findstring(&scene->gpd->layers, ruler_name, offsetof(bGPDlayer, info));
		if (gpl) {
			bGPDframe *gpf;
			gpf = gpencil_layer_getframe(gpl, CFRA, false);
			if (gpf) {
				bGPDstroke *gps;
				for (gps = gpf->strokes.first; gps; gps = gps->next) {
					bGPDspoint *pt = gps->points;
					int j;
					if (gps->totpoints == 3) {
						RulerItem *ruler_item = ruler_item_add(ruler_info);
						for (j = 0; j < 3; j++) {
							copy_v3_v3(ruler_item->co[j], &pt->x);
							pt++;
						}
						ruler_item->flag |= RULERITEM_USE_ANGLE;
						changed = true;
					}
					else if (gps->totpoints == 2) {
						RulerItem *ruler_item = ruler_item_add(ruler_info);
						for (j = 0; j < 3; j += 2) {
							copy_v3_v3(ruler_item->co[j], &pt->x);
							pt++;
						}
						changed = true;
					}
				}
			}
		}
	}

	return changed;
}

/* -------------------------------------------------------------------- */
/* local callbacks */

static void ruler_info_draw_pixel(const struct bContext *C, ARegion *ar, void *arg)
{
	Scene *scene = CTX_data_scene(C);
	UnitSettings *unit = &scene->unit;
	RulerItem *ruler_item;
	RulerInfo *ruler_info = arg;
	RegionView3D *rv3d = ruler_info->ar->regiondata;
//	ARegion *ar = ruler_info->ar;
	const float cap_size = 4.0f;
	const float bg_margin = 4.0f * U.pixelsize;
	const float bg_radius = 4.0f * U.pixelsize;
	const float arc_size = 64.0f * U.pixelsize;
#define ARC_STEPS 24
	const int arc_steps = ARC_STEPS;
	int i;
	//unsigned int color_act = 0x666600;
	unsigned int color_act = 0xffffff;
	unsigned int color_base = 0x0;
	unsigned char color_back[4] = {0xff, 0xff, 0xff, 0x80};
	unsigned char color_text[3];
	unsigned char color_wire[3];

	/* anti-aliased lines for more consistent appearance */
	glEnable(GL_LINE_SMOOTH);

	BLF_enable(blf_mono_font, BLF_ROTATION);
	BLF_size(blf_mono_font, 14 * U.pixelsize, U.dpi);
	BLF_rotation(blf_mono_font, 0.0f);

	UI_GetThemeColor3ubv(TH_TEXT, color_text);
	UI_GetThemeColor3ubv(TH_WIRE, color_wire);

	for (ruler_item = ruler_info->items.first, i = 0; ruler_item; ruler_item = ruler_item->next, i++) {
		const bool is_act = (i == ruler_info->item_active);
		float dir_ruler[2];
		float co_ss[3][2];
		int j;

		/* should these be checked? - ok for now not to */
		for (j = 0; j < 3; j++) {
			ED_view3d_project_float_global(ar, ruler_item->co[j], co_ss[j], V3D_PROJ_TEST_NOP);
		}

		glEnable(GL_BLEND);

		cpack(is_act ? color_act : color_base);

		if (ruler_item->flag & RULERITEM_USE_ANGLE) {
			glBegin(GL_LINE_STRIP);
			for (j = 0; j < 3; j++) {
				glVertex2fv(co_ss[j]);
			}
			glEnd();
			cpack(0xaaaaaa);
			setlinestyle(3);
			glBegin(GL_LINE_STRIP);
			for (j = 0; j < 3; j++) {
				glVertex2fv(co_ss[j]);
			}
			glEnd();
			setlinestyle(0);

			/* arc */
			{
				float dir_tmp[3];
				float co_tmp[3];
				float arc_ss_coords[ARC_STEPS + 1][2];

				float dir_a[3];
				float dir_b[3];
				float quat[4];
				float axis[3];
				float angle;
				const float px_scale = (ED_view3d_pixel_size(rv3d, ruler_item->co[1]) *
				                        min_fff(arc_size,
				                                len_v2v2(co_ss[0], co_ss[1]) / 2.0f,
				                                len_v2v2(co_ss[2], co_ss[1]) / 2.0f));

				sub_v3_v3v3(dir_a, ruler_item->co[0], ruler_item->co[1]);
				sub_v3_v3v3(dir_b, ruler_item->co[2], ruler_item->co[1]);
				normalize_v3(dir_a);
				normalize_v3(dir_b);

				cross_v3_v3v3(axis, dir_a, dir_b);
				angle = angle_normalized_v3v3(dir_a, dir_b);

				axis_angle_to_quat(quat, axis, angle / arc_steps);

				copy_v3_v3(dir_tmp, dir_a);

				glColor3ubv(color_wire);

				for (j = 0; j <= arc_steps; j++) {
					madd_v3_v3v3fl(co_tmp, ruler_item->co[1], dir_tmp, px_scale);
					ED_view3d_project_float_global(ar, co_tmp, arc_ss_coords[j], V3D_PROJ_TEST_NOP);
					mul_qt_v3(quat, dir_tmp);
				}

				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, arc_ss_coords);
				glDrawArrays(GL_LINE_STRIP, 0, arc_steps + 1);
				glDisableClientState(GL_VERTEX_ARRAY);
			}

			/* text */
			{
				char numstr[256];
				float numstr_size[2];
				float pos[2];
				const int prec = 2;  /* XXX, todo, make optional */

				ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);

				BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

				pos[0] = co_ss[1][0] + (cap_size * 2.0f);
				pos[1] = co_ss[1][1] - (numstr_size[1] / 2.0f);

				/* draw text (bg) */
				glColor4ubv(color_back);
				UI_draw_roundbox_corner_set(UI_CNR_ALL);
				UI_draw_roundbox(
				        pos[0] - bg_margin,                  pos[1] - bg_margin,
				        pos[0] + bg_margin + numstr_size[0], pos[1] + bg_margin + numstr_size[1],
				        bg_radius);
				/* draw text */
				glColor3ubv(color_text);
				BLF_position(blf_mono_font, pos[0], pos[1], 0.0f);
				BLF_rotation(blf_mono_font, 0.0f);
				BLF_draw(blf_mono_font, numstr, sizeof(numstr));
			}

			/* capping */
			{
				float rot_90_vec_a[2];
				float rot_90_vec_b[2];
				float cap[2];

				sub_v2_v2v2(dir_ruler, co_ss[0], co_ss[1]);
				rot_90_vec_a[0] = -dir_ruler[1];
				rot_90_vec_a[1] =  dir_ruler[0];
				normalize_v2(rot_90_vec_a);

				sub_v2_v2v2(dir_ruler, co_ss[1], co_ss[2]);
				rot_90_vec_b[0] = -dir_ruler[1];
				rot_90_vec_b[1] =  dir_ruler[0];
				normalize_v2(rot_90_vec_b);

				glEnable(GL_BLEND);

				glColor3ubv(color_wire);

				glBegin(GL_LINES);

				madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec_a, cap_size);
				glVertex2fv(cap);
				madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec_a, -cap_size);
				glVertex2fv(cap);

				madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec_b, cap_size);
				glVertex2fv(cap);
				madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec_b, -cap_size);
				glVertex2fv(cap);

				/* angle vertex */
				glVertex2f(co_ss[1][0] - cap_size, co_ss[1][1] - cap_size);
				glVertex2f(co_ss[1][0] + cap_size, co_ss[1][1] + cap_size);
				glVertex2f(co_ss[1][0] - cap_size, co_ss[1][1] + cap_size);
				glVertex2f(co_ss[1][0] + cap_size, co_ss[1][1] - cap_size);
				glEnd();

				glDisable(GL_BLEND);
			}
		}
		else {
			glBegin(GL_LINE_STRIP);
			for (j = 0; j < 3; j += 2) {
				glVertex2fv(co_ss[j]);
			}
			glEnd();
			cpack(0xaaaaaa);
			setlinestyle(3);
			glBegin(GL_LINE_STRIP);
			for (j = 0; j < 3; j += 2) {
				glVertex2fv(co_ss[j]);
			}
			glEnd();
			setlinestyle(0);

			sub_v2_v2v2(dir_ruler, co_ss[0], co_ss[2]);

			/* text */
			{
				char numstr[256];
				float numstr_size[2];
				const int prec = 6;  /* XXX, todo, make optional */
				float pos[2];

				ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);

				BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

				mid_v2_v2v2(pos, co_ss[0], co_ss[2]);

				/* center text */
				pos[0] -= numstr_size[0] / 2.0f;
				pos[1] -= numstr_size[1] / 2.0f;

				/* draw text (bg) */
				glColor4ubv(color_back);
				UI_draw_roundbox_corner_set(UI_CNR_ALL);
				UI_draw_roundbox(pos[0] - bg_margin,                  pos[1] - bg_margin,
				           pos[0] + bg_margin + numstr_size[0], pos[1] + bg_margin + numstr_size[1],
				           bg_radius);
				/* draw text */
				glColor3ubv(color_text);
				BLF_position(blf_mono_font, pos[0], pos[1], 0.0f);
				BLF_draw(blf_mono_font, numstr, sizeof(numstr));
			}

			/* capping */
			{
				float rot_90_vec[2] = {-dir_ruler[1], dir_ruler[0]};
				float cap[2];

				normalize_v2(rot_90_vec);

				glEnable(GL_BLEND);
				glColor3ubv(color_wire);

				glBegin(GL_LINES);
				madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec, cap_size);
				glVertex2fv(cap);
				madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec, -cap_size);
				glVertex2fv(cap);

				madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec, cap_size);
				glVertex2fv(cap);
				madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec, -cap_size);
				glVertex2fv(cap);
				glEnd();

				glDisable(GL_BLEND);
			}
		}
	}

	glDisable(GL_LINE_SMOOTH);

	BLF_disable(blf_mono_font, BLF_ROTATION);

#undef ARC_STEPS

	/* draw snap */
	if ((ruler_info->snap_flag & RULER_SNAP_OK) && (ruler_info->state == RULER_STATE_DRAG)) {
		ruler_item = ruler_item_active_get(ruler_info);
		if (ruler_item) {
			/* size from drawSnapping */
			const float size = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);
			float co_ss[3];
			ED_view3d_project_float_global(ar, ruler_item->co[ruler_item->co_index], co_ss, V3D_PROJ_TEST_NOP);

			cpack(color_act);
			circ(co_ss[0], co_ss[1], size * U.pixelsize);
		}
	}

}

/* free, use for both cancel and finish */
static void view3d_ruler_end(const struct bContext *UNUSED(C), RulerInfo *ruler_info)
{
	ED_region_draw_cb_exit(ruler_info->ar->type, ruler_info->draw_handle_pixel);
}

static void view3d_ruler_free(RulerInfo *ruler_info)
{
	BLI_freelistN(&ruler_info->items);
	MEM_freeN(ruler_info);
}

static void view3d_ruler_item_project(RulerInfo *ruler_info, float r_co[3],
                                      const int xy[2])
{
	ED_view3d_win_to_3d_int(ruler_info->ar, r_co, xy, r_co);
}

/* use for mousemove events */
static bool view3d_ruler_item_mousemove(bContext *C, RulerInfo *ruler_info, const int mval[2],
                                        const bool do_thickness, const bool do_snap)
{
	const float eps_bias = 0.0002f;
	RulerItem *ruler_item = ruler_item_active_get(ruler_info);

	ruler_info->snap_flag &= ~RULER_SNAP_OK;

	if (ruler_item) {
		float *co = ruler_item->co[ruler_item->co_index];
		/* restore the initial depth */
		copy_v3_v3(co, ruler_info->drag_start_co);
		view3d_ruler_item_project(ruler_info, co, mval);
		if (do_thickness && ruler_item->co_index != 1) {
			const float mval_fl[2] = {UNPACK2(mval)};
			float ray_normal[3];
			float ray_start[3];
			float *co_other;

			co_other = ruler_item->co[ruler_item->co_index == 0 ? 2 : 0];

			if (ED_view3d_snap_co(C, co, ray_normal, mval_fl, true, false,
			                      false, false, true))
			{
				negate_v3(ray_normal);
				/* add some bias */
				madd_v3_v3v3fl(ray_start, co, ray_normal, eps_bias);
				ED_view3d_snap_ray(C, co_other,
				                   ray_start, ray_normal);
			}
		}
		else if (do_snap) {
			const float mval_fl[2] = {UNPACK2(mval)};
			View3D *v3d = CTX_wm_view3d(C);
			bool use_depth = (v3d->drawtype >= OB_SOLID);
			bool is_hit = ED_view3d_snap_co(C, co, NULL, mval_fl, use_depth, false,
			                                true, true, use_depth);

			if (is_hit) {
				ruler_info->snap_flag |= RULER_SNAP_OK;
			}
		}
		return true;
	}
	else {
		return false;
	}
}

static void view3d_ruler_header_update(ScrArea *sa)
{
	const char *text = "Ctrl+LMB: Add, "
	                   "Del: Remove, "
	                   "Ctrl+Drag: Snap, "
	                   "Shift+Drag: Thickness, "
	                   "Ctrl+C: Copy Value, "
	                   "Enter: Store,  "
	                   "Esc: Cancel";

	ED_area_headerprint(sa, text);
}

/* -------------------------------------------------------------------- */
/* Operator callbacks */

static int view3d_ruler_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	RulerInfo *ruler_info;

	ruler_info = MEM_callocN(sizeof(RulerInfo), "RulerInfo");

	if (view3d_ruler_from_gpencil(C, ruler_info)) {
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
	}

	op->customdata = ruler_info;

	ruler_info->win = win;
	ruler_info->ar = ar;
	ruler_info->draw_handle_pixel = ED_region_draw_cb_activate(ar->type, ruler_info_draw_pixel,
	                                                           ruler_info, REGION_DRAW_POST_PIXEL);

	view3d_ruler_header_update(sa);

	WM_cursor_modal_set(win, BC_CROSSCURSOR);
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void view3d_ruler_cancel(bContext *C, wmOperator *op)
{
	RulerInfo *ruler_info = op->customdata;

	view3d_ruler_end(C, ruler_info);
	view3d_ruler_free(ruler_info);
	op->customdata = NULL;
}

static int view3d_ruler_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	bool do_draw = false;
	int exit_code = OPERATOR_RUNNING_MODAL;
	RulerInfo *ruler_info = op->customdata;
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = ruler_info->ar;
	RegionView3D *rv3d = ar->regiondata;

	/* its possible to change spaces while running the operator [#34894] */
	if (UNLIKELY(ar != CTX_wm_region(C))) {
		exit_code = OPERATOR_FINISHED;
		goto exit;
	}

	switch (event->type) {
		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				if (ruler_info->state == RULER_STATE_DRAG) {
					/* rubber-band angle removal */
					RulerItem *ruler_item = ruler_item_active_get(ruler_info);
					if (ruler_item && (ruler_item->co_index == 1) && (ruler_item->flag & RULERITEM_USE_ANGLE)) {
						if (!BLI_rcti_isect_pt_v(&ar->winrct, &event->x)) {
							ruler_item->flag &= ~RULERITEM_USE_ANGLE;
							do_draw = true;
						}
					}
					if (ruler_info->snap_flag & RULER_SNAP_OK) {
						ruler_info->snap_flag &= ~RULER_SNAP_OK;
						do_draw = true;
					}
					ruler_info->state = RULER_STATE_NORMAL;
				}
			}
			else {
				if (ruler_info->state == RULER_STATE_NORMAL) {

					if (event->ctrl ||
					    /* weak - but user friendly */
					    BLI_listbase_is_empty(&ruler_info->items))
					{
						View3D *v3d = CTX_wm_view3d(C);
						const bool use_depth = (v3d->drawtype >= OB_SOLID);

						/* Create new line */
						RulerItem *ruler_item_prev = ruler_item_active_get(ruler_info);
						RulerItem *ruler_item;
						/* check if we want to drag an existing point or add a new one */
						ruler_info->state = RULER_STATE_DRAG;

						ruler_item = ruler_item_add(ruler_info);
						ruler_item_active_set(ruler_info, ruler_item);

						if (use_depth) {
							/* snap the first point added, not essential but handy */
							ruler_item->co_index = 0;
							view3d_ruler_item_mousemove(C, ruler_info, event->mval, false, true);
							copy_v3_v3(ruler_info->drag_start_co, ruler_item->co[ruler_item->co_index]);
						}
						else {
							/* initial depth either previous ruler, view offset */
							if (ruler_item_prev) {
								copy_v3_v3(ruler_info->drag_start_co, ruler_item_prev->co[ruler_item_prev->co_index]);
							}
							else {
								negate_v3_v3(ruler_info->drag_start_co, rv3d->ofs);
							}

							copy_v3_v3(ruler_item->co[0], ruler_info->drag_start_co);
							view3d_ruler_item_project(ruler_info, ruler_item->co[0], event->mval);
						}

						copy_v3_v3(ruler_item->co[2], ruler_item->co[0]);
						ruler_item->co_index = 2;

						do_draw = true;
					}
					else {
						float mval_fl[2] = {UNPACK2(event->mval)};
						RulerItem *ruler_item_pick;
						int co_index;

						/* select and drag */
						if (view3d_ruler_pick(ruler_info, mval_fl, &ruler_item_pick, &co_index)) {
							if (co_index == -1) {
								if ((ruler_item_pick->flag & RULERITEM_USE_ANGLE) == 0) {
									/* Add Center Point */
									ruler_item_active_set(ruler_info, ruler_item_pick);
									ruler_item_pick->flag |= RULERITEM_USE_ANGLE;
									ruler_item_pick->co_index = 1;
									ruler_info->state = RULER_STATE_DRAG;

									/* find the factor */
									{
										float co_ss[2][2];
										float fac;

										ED_view3d_project_float_global(ar, ruler_item_pick->co[0], co_ss[0], V3D_PROJ_TEST_NOP);
										ED_view3d_project_float_global(ar, ruler_item_pick->co[2], co_ss[1], V3D_PROJ_TEST_NOP);

										fac = line_point_factor_v2(mval_fl, co_ss[0], co_ss[1]);
										CLAMP(fac, 0.0f, 1.0f);

										interp_v3_v3v3(ruler_item_pick->co[1],
										               ruler_item_pick->co[0],
										               ruler_item_pick->co[2], fac);
									}

									/* update the new location */
									view3d_ruler_item_mousemove(C, ruler_info, event->mval,
									                            event->shift != 0, event->ctrl != 0);
									do_draw = true;
								}
							}
							else {
								ruler_item_active_set(ruler_info, ruler_item_pick);
								ruler_item_pick->co_index = co_index;
								ruler_info->state = RULER_STATE_DRAG;

								/* store the initial depth */
								copy_v3_v3(ruler_info->drag_start_co, ruler_item_pick->co[ruler_item_pick->co_index]);

								do_draw = true;
							}
						}
						else {
							exit_code = OPERATOR_PASS_THROUGH;
						}

					}
				}
			}
			break;
		case CKEY:
		{
			if (event->ctrl) {
				RulerItem *ruler_item = ruler_item_active_get(ruler_info);
				if (ruler_item) {
					const int prec = 8;
					char numstr[256];
					Scene *scene = CTX_data_scene(C);
					UnitSettings *unit = &scene->unit;

					ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);
					WM_clipboard_text_set((void *) numstr, false);
				}
			}
			break;
		}
		case RIGHTCTRLKEY:
		case LEFTCTRLKEY:
		{
			WM_event_add_mousemove(C);
			break;
		}
		case MOUSEMOVE:
		{
			if (ruler_info->state == RULER_STATE_DRAG) {
				if (view3d_ruler_item_mousemove(C, ruler_info, event->mval,
				                                event->shift != 0, event->ctrl != 0))
				{
					do_draw = true;
				}
			}
			break;
		}
		case ESCKEY:
		{
			do_draw = true;
			exit_code = OPERATOR_CANCELLED;
			break;
		}
		case RETKEY:
		{
			view3d_ruler_to_gpencil(C, ruler_info);
			do_draw = true;
			exit_code = OPERATOR_FINISHED;
			break;
		}
		case DELKEY:
		{
			if (event->val == KM_PRESS) {
				if (ruler_info->state == RULER_STATE_NORMAL) {
					RulerItem *ruler_item = ruler_item_active_get(ruler_info);
					if (ruler_item) {
						RulerItem *ruler_item_other = ruler_item->prev ? ruler_item->prev : ruler_item->next;
						ruler_item_remove(ruler_info, ruler_item);
						ruler_item_active_set(ruler_info, ruler_item_other);
						do_draw = true;
					}
				}
			}
			break;
		}
		default:
			exit_code = OPERATOR_PASS_THROUGH;
			break;

	}

	if (do_draw) {
		view3d_ruler_header_update(sa);

		/* all 3d views draw rulers */
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, NULL);
	}

exit:
	if (ELEM(exit_code, OPERATOR_FINISHED, OPERATOR_CANCELLED)) {
		WM_cursor_modal_restore(ruler_info->win);

		view3d_ruler_end(C, ruler_info);
		view3d_ruler_free(ruler_info);
		op->customdata = NULL;

		ED_area_headerprint(sa, NULL);
	}

	return exit_code;
}

void VIEW3D_OT_ruler(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Ruler/Protractor";
	ot->description = "Interactive ruler";
	ot->idname = "VIEW3D_OT_ruler";

	/* api callbacks */
	ot->invoke = view3d_ruler_invoke;
	ot->cancel = view3d_ruler_cancel;
	ot->modal = view3d_ruler_modal;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = 0;
}
