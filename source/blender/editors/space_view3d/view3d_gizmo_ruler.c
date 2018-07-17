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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_gizmo_ruler.c
 *  \ingroup spview3d
 */

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_rect.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"

#include "BKE_object.h"
#include "BKE_unit.h"

#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_view3d_types.h"

#include "BIF_gl.h"

#include "ED_screen.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"

#include "UI_resources.h"
#include "UI_interface.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_toolsystem.h"

#include "view3d_intern.h"  /* own include */

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_select.h"
#include "GPU_state.h"

#include "BLF_api.h"


static const char *view3d_gzgt_ruler_id = "VIEW3D_GGT_ruler";


#define MVAL_MAX_PX_DIST 12.0f

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

/* not clicking on a point */
#define PART_LINE 0xff

/* -------------------------------------------------------------------- */
/* Ruler Info (wmGizmoGroup customdata) */

enum {
	RULER_STATE_NORMAL = 0,
	RULER_STATE_DRAG
};

enum {
	RULER_SNAP_OK = (1 << 0),
};

typedef struct RulerInfo {
	// ListBase items;
	int      item_active;
	int flag;
	int snap_flag;
	int state;

	struct SnapObjectContext *snap_context;

	/* wm state */
	wmWindow *win;
	ScrArea *sa;
	ARegion *ar;  /* re-assigned every modal update */
} RulerInfo;

/* -------------------------------------------------------------------- */
/* Ruler Item (two or three points) */

typedef struct RulerItem {
	wmGizmo gz;

	/* worldspace coords, middle being optional */
	float co[3][3];

	int   flag;
	int   raycast_dir;  /* RULER_DIRECTION_* */
} RulerItem;

typedef struct RulerInteraction {
	/* selected coord */
	char  co_index; /* 0 -> 2 */
	float drag_start_co[3];
	uint inside_region : 1;
} RulerInteraction;

/* -------------------------------------------------------------------- */
/** \name Internal Ruler Utilities
 * \{ */

static RulerItem *ruler_item_add(wmGizmoGroup *gzgroup)
{
	/* could pass this as an arg */
	const wmGizmoType *gzt_ruler = WM_gizmotype_find("VIEW3D_GT_ruler_item", true);
	RulerItem *ruler_item = (RulerItem *)WM_gizmo_new_ptr(gzt_ruler, gzgroup, NULL);
	WM_gizmo_set_flag(&ruler_item->gz, WM_GIZMO_DRAW_MODAL, true);
	return ruler_item;
}

static void ruler_item_remove(bContext *C, wmGizmoGroup *gzgroup, RulerItem *ruler_item)
{
	WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, &ruler_item->gz, C);
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

static bool view3d_ruler_pick(
        wmGizmoGroup *gzgroup, RulerItem *ruler_item, const float mval[2],
        int *r_co_index)
{
	RulerInfo *ruler_info = gzgroup->customdata;
	ARegion *ar = ruler_info->ar;
	bool found = false;

	float dist_best = RULER_PICK_DIST_SQ;
	int co_index_best = -1;

	{
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
				found = true;

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
				found = true;

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

	*r_co_index = co_index_best;
	return found;
}

/**
 * Ensure the 'snap_context' is only cached while dragging,
 * needed since the user may toggle modes between tool use.
 */
static void ruler_state_set(bContext *C, RulerInfo *ruler_info, int state)
{
	Main *bmain = CTX_data_main(C);
	if (state == ruler_info->state) {
		return;
	}

	/* always remove */
	if (ruler_info->snap_context) {
		ED_transform_snap_object_context_destroy(ruler_info->snap_context);
		ruler_info->snap_context = NULL;
	}

	if (state == RULER_STATE_NORMAL) {
		/* pass */
	}
	else if (state == RULER_STATE_DRAG) {
		ruler_info->snap_context = ED_transform_snap_object_context_create_view3d(
		        bmain, CTX_data_scene(C), CTX_data_depsgraph(C), 0,
		        ruler_info->ar, CTX_wm_view3d(C));
	}
	else {
		BLI_assert(0);
	}

	ruler_info->state = state;
}

static void view3d_ruler_item_project(
        RulerInfo *ruler_info, float r_co[3],
        const int xy[2])
{
	ED_view3d_win_to_3d_int(ruler_info->sa->spacedata.first, ruler_info->ar, r_co, xy, r_co);
}

/* use for mousemove events */
static bool view3d_ruler_item_mousemove(
        RulerInfo *ruler_info, RulerItem *ruler_item, const int mval[2],
        const bool do_thickness, const bool do_snap)
{
	RulerInteraction *inter = ruler_item->gz.interaction_data;
	const float eps_bias = 0.0002f;
	float dist_px = MVAL_MAX_PX_DIST * U.pixelsize;  /* snap dist */

	ruler_info->snap_flag &= ~RULER_SNAP_OK;

	if (ruler_item) {
		float *co = ruler_item->co[inter->co_index];
		/* restore the initial depth */
		copy_v3_v3(co, inter->drag_start_co);
		view3d_ruler_item_project(ruler_info, co, mval);
		if (do_thickness && inter->co_index != 1) {
			// Scene *scene = CTX_data_scene(C);
			// View3D *v3d = ruler_info->sa->spacedata.first;
			const float mval_fl[2] = {UNPACK2(mval)};
			float ray_normal[3];
			float ray_start[3];
			float *co_other;

			co_other = ruler_item->co[inter->co_index == 0 ? 2 : 0];

			if (ED_transform_snap_object_project_view3d(
			        ruler_info->snap_context,
			        SCE_SNAP_MODE_FACE,
			        &(const struct SnapObjectParams){
			            .snap_select = SNAP_ALL,
			            .use_object_edit_cage = true,
			        },
			        mval_fl, &dist_px,
			        co, ray_normal))
			{
				negate_v3(ray_normal);
				/* add some bias */
				madd_v3_v3v3fl(ray_start, co, ray_normal, eps_bias);
				ED_transform_snap_object_project_ray(
				        ruler_info->snap_context,
				        &(const struct SnapObjectParams){
				            .snap_select = SNAP_ALL,
				            .use_object_edit_cage = true,
				        },
				        ray_start, ray_normal, NULL,
				        co_other, NULL);
			}
		}
		else if (do_snap) {
			const float mval_fl[2] = {UNPACK2(mval)};

			if (ED_transform_snap_object_project_view3d(
			        ruler_info->snap_context,
			        (SCE_SNAP_MODE_VERTEX | SCE_SNAP_MODE_EDGE | SCE_SNAP_MODE_FACE),
			        &(const struct SnapObjectParams){
			            .snap_select = SNAP_ALL,
			            .use_object_edit_cage = true,
			            .use_occlusion_test = true,
			        },
			        mval_fl, &dist_px,
			        co, NULL))
			{
				ruler_info->snap_flag |= RULER_SNAP_OK;
			}
		}
		return true;
	}
	else {
		return false;
	}
}


/** \} */

/* -------------------------------------------------------------------- */
/** \name Ruler/Grease Pencil Conversion
 * \{ */

#define RULER_ID "RulerData3D"
static bool view3d_ruler_to_gpencil(bContext *C, wmGizmoGroup *gzgroup)
{
	// RulerInfo *ruler_info = gzgroup->customdata;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	bGPDlayer *gpl;
	bGPDframe *gpf;
	bGPDstroke *gps;
	bGPDpalette *palette;
	bGPDpalettecolor *palcolor;
	RulerItem *ruler_item;
	const char *ruler_name = RULER_ID;
	bool changed = false;

	if (scene->gpd == NULL) {
		scene->gpd = BKE_gpencil_data_addnew(bmain, "GPencil");
	}

	gpl = BLI_findstring(&scene->gpd->layers, ruler_name, offsetof(bGPDlayer, info));
	if (gpl == NULL) {
		gpl = BKE_gpencil_layer_addnew(scene->gpd, ruler_name, false);
		gpl->thickness = 1;
		gpl->flag |= GP_LAYER_HIDE;
	}

	/* try to get active palette or create a new one */
	palette = BKE_gpencil_palette_getactive(scene->gpd);
	if (palette == NULL) {
		palette = BKE_gpencil_palette_addnew(scene->gpd, DATA_("GP_Palette"), true);
	}
	/* try to get color with the ruler name or create a new one */
	palcolor = BKE_gpencil_palettecolor_getbyname(palette, (char *)ruler_name);
	if (palcolor == NULL) {
		palcolor = BKE_gpencil_palettecolor_addnew(palette, (char *)ruler_name, true);
	}

	gpf = BKE_gpencil_layer_getframe(gpl, CFRA, true);
	BKE_gpencil_free_strokes(gpf);

	for (ruler_item = gzgroup->gizmos.first; ruler_item; ruler_item = (RulerItem *)ruler_item->gz.next) {
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
				pt->strength = 1.0f;
				pt++;
			}
		}
		else {
			gps->totpoints = 2;
			pt = gps->points = MEM_callocN(sizeof(bGPDspoint) * gps->totpoints, "gp_stroke_points");
			for (j = 0; j < 3; j += 2) {
				copy_v3_v3(&pt->x, ruler_item->co[j]);
				pt->pressure = 1.0f;
				pt->strength = 1.0f;
				pt++;
			}
		}
		gps->flag = GP_STROKE_3DSPACE;
		gps->thickness = 3;
		/* assign color to stroke */
		BLI_strncpy(gps->colorname, palcolor->info, sizeof(gps->colorname));
		gps->palcolor = palcolor;
		BLI_addtail(&gpf->strokes, gps);
		changed = true;
	}

	return changed;
}

static bool view3d_ruler_from_gpencil(const bContext *C, wmGizmoGroup *gzgroup)
{
	Scene *scene = CTX_data_scene(C);
	bool changed = false;

	if (scene->gpd) {
		bGPDlayer *gpl;
		const char *ruler_name = RULER_ID;
		gpl = BLI_findstring(&scene->gpd->layers, ruler_name, offsetof(bGPDlayer, info));
		if (gpl) {
			bGPDframe *gpf;
			gpf = BKE_gpencil_layer_getframe(gpl, CFRA, false);
			if (gpf) {
				bGPDstroke *gps;
				for (gps = gpf->strokes.first; gps; gps = gps->next) {
					bGPDspoint *pt = gps->points;
					int j;
					RulerItem *ruler_item = NULL;
					if (gps->totpoints == 3) {
						ruler_item = ruler_item_add(gzgroup);
						for (j = 0; j < 3; j++) {
							copy_v3_v3(ruler_item->co[j], &pt->x);
							pt++;
						}
						ruler_item->flag |= RULERITEM_USE_ANGLE;
						changed = true;
					}
					else if (gps->totpoints == 2) {
						ruler_item = ruler_item_add(gzgroup);
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ruler Item Gizmo Type
 * \{ */

static void gizmo_ruler_draw(const bContext *C, wmGizmo *gz)
{
	Scene *scene = CTX_data_scene(C);
	UnitSettings *unit = &scene->unit;
	RulerInfo *ruler_info = gz->parent_gzgroup->customdata;
	RulerItem *ruler_item = (RulerItem *)gz;
	ARegion *ar = ruler_info->ar;
	RegionView3D *rv3d = ar->regiondata;
	const float cap_size = 4.0f;
	const float bg_margin = 4.0f * U.pixelsize;
	const float bg_radius = 4.0f * U.pixelsize;
	const float arc_size = 64.0f * U.pixelsize;
#define ARC_STEPS 24
	const int arc_steps = ARC_STEPS;
	const float color_act[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float color_base[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	unsigned char color_text[3];
	unsigned char color_wire[3];
	float color_back[4] = {1.0f, 1.0f, 1.0f, 0.5f};

	/* anti-aliased lines for more consistent appearance */
	GPU_line_smooth(true);

	BLF_enable(blf_mono_font, BLF_ROTATION);
	BLF_size(blf_mono_font, 14 * U.pixelsize, U.dpi);
	BLF_rotation(blf_mono_font, 0.0f);

	UI_GetThemeColor3ubv(TH_TEXT, color_text);
	UI_GetThemeColor3ubv(TH_WIRE, color_wire);

	const bool is_act = (gz->flag & WM_GIZMO_DRAW_HOVER);
	float dir_ruler[2];
	float co_ss[3][2];
	int j;

	/* should these be checked? - ok for now not to */
	for (j = 0; j < 3; j++) {
		ED_view3d_project_float_global(ar, ruler_item->co[j], co_ss[j], V3D_PROJ_TEST_NOP);
	}

	GPU_blend(true);

	const uint shdr_pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	if (ruler_item->flag & RULERITEM_USE_ANGLE) {
		immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

		float viewport_size[4];
		GPU_viewport_size_get_f(viewport_size);
		immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

		immUniform1i("colors_len", 2);  /* "advanced" mode */
		const float *col = is_act ? color_act : color_base;
		immUniformArray4fv("colors", (float *)(float[][4]){{0.67f, 0.67f, 0.67f, 1.0f}, {col[0], col[1], col[2], col[3]}}, 2);
		immUniform1f("dash_width", 6.0f);

		immBegin(GPU_PRIM_LINE_STRIP, 3);

		immVertex2fv(shdr_pos, co_ss[0]);
		immVertex2fv(shdr_pos, co_ss[1]);
		immVertex2fv(shdr_pos, co_ss[2]);

		immEnd();

		immUnbindProgram();

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		/* arc */
		{
			float dir_tmp[3];
			float co_tmp[3];
			float arc_ss_coord[2];

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

			immUniformColor3ubv(color_wire);

			immBegin(GPU_PRIM_LINE_STRIP, arc_steps + 1);

			for (j = 0; j <= arc_steps; j++) {
				madd_v3_v3v3fl(co_tmp, ruler_item->co[1], dir_tmp, px_scale);
				ED_view3d_project_float_global(ar, co_tmp, arc_ss_coord, V3D_PROJ_TEST_NOP);
				mul_qt_v3(quat, dir_tmp);

				immVertex2fv(shdr_pos, arc_ss_coord);
			}

			immEnd();
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

			GPU_blend(true);

			immUniformColor3ubv(color_wire);

			immBegin(GPU_PRIM_LINES, 8);

			madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec_a, cap_size);
			immVertex2fv(shdr_pos, cap);
			madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec_a, -cap_size);
			immVertex2fv(shdr_pos, cap);

			madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec_b, cap_size);
			immVertex2fv(shdr_pos, cap);
			madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec_b, -cap_size);
			immVertex2fv(shdr_pos, cap);

			/* angle vertex */
			immVertex2f(shdr_pos, co_ss[1][0] - cap_size, co_ss[1][1] - cap_size);
			immVertex2f(shdr_pos, co_ss[1][0] + cap_size, co_ss[1][1] + cap_size);
			immVertex2f(shdr_pos, co_ss[1][0] - cap_size, co_ss[1][1] + cap_size);
			immVertex2f(shdr_pos, co_ss[1][0] + cap_size, co_ss[1][1] - cap_size);

			immEnd();

			GPU_blend(false);
		}

		immUnbindProgram();

		/* text */
		{
			char numstr[256];
			float numstr_size[2];
			float posit[2];
			const int prec = 2;  /* XXX, todo, make optional */

			ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);

			BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

			posit[0] = co_ss[1][0] + (cap_size * 2.0f);
			posit[1] = co_ss[1][1] - (numstr_size[1] / 2.0f);

			/* draw text (bg) */
			UI_draw_roundbox_corner_set(UI_CNR_ALL);
			UI_draw_roundbox_aa(
			        true,
			        posit[0] - bg_margin,                  posit[1] - bg_margin,
			        posit[0] + bg_margin + numstr_size[0], posit[1] + bg_margin + numstr_size[1],
			        bg_radius, color_back);
			/* draw text */
			BLF_color3ubv(blf_mono_font, color_text);
			BLF_position(blf_mono_font, posit[0], posit[1], 0.0f);
			BLF_rotation(blf_mono_font, 0.0f);
			BLF_draw(blf_mono_font, numstr, sizeof(numstr));
		}
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

		float viewport_size[4];
		GPU_viewport_size_get_f(viewport_size);
		immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

		immUniform1i("colors_len", 2);  /* "advanced" mode */
		const float *col = is_act ? color_act : color_base;
		immUniformArray4fv("colors", (float *)(float[][4]){{0.67f, 0.67f, 0.67f, 1.0f}, {col[0], col[1], col[2], col[3]}}, 2);
		immUniform1f("dash_width", 6.0f);

		immBegin(GPU_PRIM_LINES, 2);

		immVertex2fv(shdr_pos, co_ss[0]);
		immVertex2fv(shdr_pos, co_ss[2]);

		immEnd();

		immUnbindProgram();

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

		sub_v2_v2v2(dir_ruler, co_ss[0], co_ss[2]);

		/* capping */
		{
			float rot_90_vec[2] = {-dir_ruler[1], dir_ruler[0]};
			float cap[2];

			normalize_v2(rot_90_vec);

			GPU_blend(true);

			immUniformColor3ubv(color_wire);

			immBegin(GPU_PRIM_LINES, 4);

			madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec, cap_size);
			immVertex2fv(shdr_pos, cap);
			madd_v2_v2v2fl(cap, co_ss[0], rot_90_vec, -cap_size);
			immVertex2fv(shdr_pos, cap);

			madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec, cap_size);
			immVertex2fv(shdr_pos, cap);
			madd_v2_v2v2fl(cap, co_ss[2], rot_90_vec, -cap_size);
			immVertex2fv(shdr_pos, cap);

			immEnd();

			GPU_blend(false);
		}

		immUnbindProgram();

		/* text */
		{
			char numstr[256];
			float numstr_size[2];
			const int prec = 6;  /* XXX, todo, make optional */
			float posit[2];

			ruler_item_as_string(ruler_item, unit, numstr, sizeof(numstr), prec);

			BLF_width_and_height(blf_mono_font, numstr, sizeof(numstr), &numstr_size[0], &numstr_size[1]);

			mid_v2_v2v2(posit, co_ss[0], co_ss[2]);

			/* center text */
			posit[0] -= numstr_size[0] / 2.0f;
			posit[1] -= numstr_size[1] / 2.0f;

			/* draw text (bg) */
			UI_draw_roundbox_corner_set(UI_CNR_ALL);
			UI_draw_roundbox_aa(
			        true,
			        posit[0] - bg_margin,                  posit[1] - bg_margin,
			        posit[0] + bg_margin + numstr_size[0], posit[1] + bg_margin + numstr_size[1],
			        bg_radius, color_back);
			/* draw text */
			BLF_color3ubv(blf_mono_font, color_text);
			BLF_position(blf_mono_font, posit[0], posit[1], 0.0f);
			BLF_draw(blf_mono_font, numstr, sizeof(numstr));
		}
	}

	GPU_line_smooth(false);

	BLF_disable(blf_mono_font, BLF_ROTATION);

#undef ARC_STEPS

	/* draw snap */
	if ((ruler_info->snap_flag & RULER_SNAP_OK) &&
	    (ruler_info->state == RULER_STATE_DRAG) &&
	    (ruler_item->gz.interaction_data != NULL))
	{
		RulerInteraction *inter = ruler_item->gz.interaction_data;
		/* size from drawSnapping */
		const float size = 2.5f * UI_GetThemeValuef(TH_VERTEX_SIZE);
		float co_ss_snap[3];
		ED_view3d_project_float_global(ar, ruler_item->co[inter->co_index], co_ss_snap, V3D_PROJ_TEST_NOP);

		uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformColor4fv(color_act);

		imm_draw_circle_wire_2d(pos, co_ss_snap[0], co_ss_snap[1], size * U.pixelsize, 32);

		immUnbindProgram();
	}
}

static int gizmo_ruler_test_select(
        bContext *UNUSED(C), wmGizmo *gz, const wmEvent *event)
{
	RulerItem *ruler_item_pick = (RulerItem *)gz;
	float mval_fl[2] = {UNPACK2(event->mval)};
	int co_index;

	/* select and drag */
	if (view3d_ruler_pick(gz->parent_gzgroup, ruler_item_pick, mval_fl, &co_index)) {
		if (co_index == -1) {
			if ((ruler_item_pick->flag & RULERITEM_USE_ANGLE) == 0) {
				return PART_LINE;
			}
		}
		else {
			return co_index;
		}
	}
	return -1;
}

static int gizmo_ruler_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
	bool do_draw = false;
	int exit_code = OPERATOR_RUNNING_MODAL;
	RulerInfo *ruler_info = gz->parent_gzgroup->customdata;
	RulerItem *ruler_item = (RulerItem *)gz;
	RulerInteraction *inter = ruler_item->gz.interaction_data;
	ARegion *ar = CTX_wm_region(C);

	ruler_info->ar = ar;

	switch (event->type) {
		case MOUSEMOVE:
		{
			if (ruler_info->state == RULER_STATE_DRAG) {
				if (view3d_ruler_item_mousemove(
				        ruler_info, ruler_item, event->mval,
				        event->shift != 0, event->ctrl != 0))
				{
					do_draw = true;
				}
				inter->inside_region = BLI_rcti_isect_pt_v(&ar->winrct, &event->x);
			}
			break;
		}
	}
	if (do_draw) {
		ED_region_tag_redraw(ar);
	}
	return exit_code;
}

static int gizmo_ruler_invoke(
        bContext *C, wmGizmo *gz, const wmEvent *event)
{
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	RulerInfo *ruler_info = gzgroup->customdata;
	RulerItem *ruler_item_pick = (RulerItem *)gz;
	RulerInteraction *inter = MEM_callocN(sizeof(RulerInteraction), __func__);
	gz->interaction_data = inter;

	ARegion *ar = ruler_info->ar;

	const float mval_fl[2] = {UNPACK2(event->mval)};

	/* select and drag */
	if (gz->highlight_part == PART_LINE) {
		if ((ruler_item_pick->flag & RULERITEM_USE_ANGLE) == 0) {
			/* Add Center Point */
			ruler_item_pick->flag |= RULERITEM_USE_ANGLE;
			inter->co_index = 1;
			ruler_state_set(C, ruler_info, RULER_STATE_DRAG);

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
			view3d_ruler_item_mousemove(
			        ruler_info, ruler_item_pick, event->mval,
			        event->shift != 0, event->ctrl != 0);
		}
	}
	else {
		inter->co_index = gz->highlight_part;
		ruler_state_set(C, ruler_info, RULER_STATE_DRAG);

		/* store the initial depth */
		copy_v3_v3(inter->drag_start_co, ruler_item_pick->co[inter->co_index]);
	}

	return OPERATOR_RUNNING_MODAL;
}

static void gizmo_ruler_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
	wmGizmoGroup *gzgroup = gz->parent_gzgroup;
	RulerInfo *ruler_info = gzgroup->customdata;

	if (!cancel) {
		if (ruler_info->state == RULER_STATE_DRAG) {
			RulerItem *ruler_item = (RulerItem *)gz;
			RulerInteraction *inter = gz->interaction_data;
			/* rubber-band angle removal */
			if (!inter->inside_region) {
				if ((inter->co_index == 1) && (ruler_item->flag & RULERITEM_USE_ANGLE)) {
					ruler_item->flag &= ~RULERITEM_USE_ANGLE;
				}
				else {
					/* Not ideal, since the ruler isn't a mode and we don't want to override delete key
					 * use dragging out of the view for removal. */
					ruler_item_remove(C, gzgroup, ruler_item);
					ruler_item = NULL;
					gz = NULL;
					inter = NULL;
				}
			}
			if (ruler_info->snap_flag & RULER_SNAP_OK) {
				ruler_info->snap_flag &= ~RULER_SNAP_OK;
			}
			ruler_state_set(C, ruler_info, RULER_STATE_NORMAL);
		}
		/* We could convert only the current gizmo, for now just re-generate. */
		view3d_ruler_to_gpencil(C, gzgroup);
	}

	if (gz) {
		MEM_SAFE_FREE(gz->interaction_data);
	}

	ruler_state_set(C, ruler_info, RULER_STATE_NORMAL);
}

static int gizmo_ruler_cursor_get(wmGizmo *gz)
{
	if (gz->highlight_part == PART_LINE) {
		return BC_CROSSCURSOR;
	}
	return BC_NSEW_SCROLLCURSOR;
}

void VIEW3D_GT_ruler_item(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "VIEW3D_GT_ruler_item";

	/* api callbacks */
	gzt->draw = gizmo_ruler_draw;
	gzt->test_select = gizmo_ruler_test_select;
	gzt->modal = gizmo_ruler_modal;
	gzt->invoke = gizmo_ruler_invoke;
	gzt->exit = gizmo_ruler_exit;
	gzt->cursor_get = gizmo_ruler_cursor_get;

	gzt->struct_size = sizeof(RulerItem);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ruler Gizmo Group
 * \{ */

static bool WIDGETGROUP_ruler_poll(const bContext *C, wmGizmoGroupType *gzgt)
{
	bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
	if ((tref_rt == NULL) ||
	    !STREQ(gzgt->idname, tref_rt->gizmo_group))
	{
		WM_gizmo_group_type_unlink_delayed_ptr(gzgt);
		return false;
	}
	return true;
}

static void WIDGETGROUP_ruler_setup(const bContext *C, wmGizmoGroup *gzgroup)
{
	RulerInfo *ruler_info = MEM_callocN(sizeof(RulerInfo), __func__);

	if (view3d_ruler_from_gpencil(C, gzgroup)) {
		/* nop */
	}

	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	ruler_info->win = win;
	ruler_info->sa = sa;
	ruler_info->ar = ar;

	gzgroup->customdata = ruler_info;
}

void VIEW3D_GGT_ruler(wmGizmoGroupType *gzgt)
{
	gzgt->name = "Ruler Widgets";
	gzgt->idname = view3d_gzgt_ruler_id;

	gzgt->flag |= WM_GIZMOGROUPTYPE_SCALE | WM_GIZMOGROUPTYPE_DRAW_MODAL_ALL;

	gzgt->gzmap_params.spaceid = SPACE_VIEW3D;
	gzgt->gzmap_params.regionid = RGN_TYPE_WINDOW;

	gzgt->poll = WIDGETGROUP_ruler_poll;
	gzgt->setup = WIDGETGROUP_ruler_setup;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Ruler Operator
 * \{ */

static bool view3d_ruler_poll(bContext *C)
{
	bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
	if ((tref_rt == NULL) ||
	    !STREQ(view3d_gzgt_ruler_id, tref_rt->gizmo_group) ||
	    CTX_wm_region_view3d(C) == NULL)
	{
		return false;
	}
	return true;
}

static int view3d_ruler_add_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;

	wmGizmoMap *gzmap = ar->gizmo_map;
	wmGizmoGroup *gzgroup = WM_gizmomap_group_find(gzmap, view3d_gzgt_ruler_id);
	const bool use_depth = (v3d->shading.type >= OB_SOLID);

	/* Create new line */
	RulerItem *ruler_item;
	ruler_item = ruler_item_add(gzgroup);

	/* This is a little weak, but there is no real good way to tweak directly. */
	WM_gizmo_highlight_set(gzmap, &ruler_item->gz);
	if (WM_operator_name_call(
	        C, "GIZMOGROUP_OT_gizmo_tweak",
	        WM_OP_INVOKE_REGION_WIN, NULL) == OPERATOR_RUNNING_MODAL)
	{
		RulerInfo *ruler_info = gzgroup->customdata;
		RulerInteraction *inter = ruler_item->gz.interaction_data;
		if (use_depth) {
			/* snap the first point added, not essential but handy */
			inter->co_index = 0;
			view3d_ruler_item_mousemove(ruler_info, ruler_item, event->mval, false, true);
			copy_v3_v3(inter->drag_start_co, ruler_item->co[inter->co_index]);
		}
		else {
			negate_v3_v3(inter->drag_start_co, rv3d->ofs);
			copy_v3_v3(ruler_item->co[0], inter->drag_start_co);
			view3d_ruler_item_project(ruler_info, ruler_item->co[0], event->mval);
		}

		copy_v3_v3(ruler_item->co[2], ruler_item->co[0]);
		ruler_item->gz.highlight_part = inter->co_index = 2;
	}
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_ruler_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Ruler Add";
	ot->idname = "VIEW3D_OT_ruler_add";
	ot->description = "";

	ot->invoke = view3d_ruler_add_invoke;
	ot->poll = view3d_ruler_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/** \} */
