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
 * The Original Code is Copyright (C) 2017, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_fill.c
 *  \ingroup edgpencil
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_stack.h"

#include "BLT_translation.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_main.h"
#include "BKE_brush.h"
#include "BKE_image.h"
#include "BKE_gpencil.h"
#include "BKE_material.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_paint.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GPU_immediate.h"
#include "GPU_draw.h"
#include "GPU_matrix.h"
#include "GPU_framebuffer.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "gpencil_intern.h"

#define LEAK_HORZ 0
#define LEAK_VERT 1


/* Temporary fill operation data (op->customdata) */
typedef struct tGPDfill {
	struct Main *bmain;
	struct Depsgraph *depsgraph;
	struct wmWindow *win;               /* window where painting originated */
	struct Scene *scene;                /* current scene from context */
	struct Object *ob;                  /* current active gp object */
	struct ScrArea *sa;                 /* area where painting originated */
	struct RegionView3D *rv3d;          /* region where painting originated */
	struct View3D *v3d;                 /* view3 where painting originated */
	struct ARegion *ar;                 /* region where painting originated */
	struct bGPdata *gpd;                /* current GP datablock */
	struct Material *mat;               /* current material */
	struct bGPDlayer *gpl;              /* layer */
	struct bGPDframe *gpf;              /* frame */

	short flag;                         /* flags */
	short oldkey;                       /* avoid too fast events */
	bool on_back;                       /* send to back stroke */

	int center[2];                      /* mouse fill center position */
	int sizex;                          /* windows width */
	int sizey;                          /* window height */
	int lock_axis;                      /* lock to viewport axis */

	short fill_leak;                    /* number of pixel to consider the leak is too small (x 2) */
	float fill_threshold;               /* factor for transparency */
	int fill_simplylvl;                 /* number of simplify steps */
	int fill_draw_mode;                 /* boundary limits drawing mode */

	short sbuffer_size;                 /* number of elements currently in cache */
	void *sbuffer;                      /* temporary points */
	float *depth_arr;                   /* depth array for reproject */

	Image *ima;                         /* temp image */
	BLI_Stack *stack;                   /* temp points data */
	void *draw_handle_3d;               /* handle for drawing strokes while operator is running 3d stuff */
} tGPDfill;


 /* draw a given stroke using same thickness and color for all points */
static void gp_draw_basic_stroke(
        tGPDfill *tgpf, bGPDstroke *gps, const float diff_mat[4][4],
        bool cyclic, float ink[4], int flag, float thershold)
{
	bGPDspoint *points = gps->points;

	Material *ma = tgpf->mat;
	MaterialGPencilStyle *gp_style = ma->gp_style;

	int totpoints = gps->totpoints;
	float fpt[3];
	float col[4];

	copy_v4_v4(col, ink);

	/* if cyclic needs more vertex */
	int cyclic_add = (cyclic) ? 1 : 0;

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

	/* draw stroke curve */
	glLineWidth(1.0f);
	immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints + cyclic_add);
	const bGPDspoint *pt = points;

	for (int i = 0; i < totpoints; i++, pt++) {

		if (flag & GP_BRUSH_FILL_HIDE) {
			float alpha = gp_style->stroke_rgba[3] * pt->strength;
			CLAMP(alpha, 0.0f, 1.0f);
			col[3] = alpha <= thershold ? 0.0f : 1.0f;
		}
		else {
			col[3] = 1.0f;
		}
		/* set point */
		immAttrib4fv(color, col);
		mul_v3_m4v3(fpt, diff_mat, &pt->x);
		immVertex3fv(pos, fpt);
	}

	if (cyclic && totpoints > 2) {
		/* draw line to first point to complete the cycle */
		immAttrib4fv(color, col);
		mul_v3_m4v3(fpt, diff_mat, &points->x);
		immVertex3fv(pos, fpt);
	}

	immEnd();
	immUnbindProgram();
}

/* loop all layers */
static void gp_draw_datablock(tGPDfill *tgpf, float ink[4])
{
	/* duplicated: etempFlags */
	enum {
		GP_DRAWFILLS_NOSTATUS = (1 << 0),   /* don't draw status info */
		GP_DRAWFILLS_ONLY3D = (1 << 1),   /* only draw 3d-strokes */
	};

	Object *ob = tgpf->ob;
	bGPdata *gpd = tgpf->gpd;
	int cfra_eval = (int)DEG_get_ctime(tgpf->depsgraph);

	tGPDdraw tgpw;
	tgpw.rv3d = tgpf->rv3d;
	tgpw.depsgraph = tgpf->depsgraph;
	tgpw.ob = ob;
	tgpw.gpd = gpd;
	tgpw.offsx = 0;
	tgpw.offsy = 0;
	tgpw.winx = tgpf->ar->winx;
	tgpw.winy = tgpf->ar->winy;
	tgpw.dflag = 0;
	tgpw.disable_fill = 1;
	tgpw.dflag |= (GP_DRAWFILLS_ONLY3D | GP_DRAWFILLS_NOSTATUS);

	glEnable(GL_BLEND);

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* calculate parent position */
		ED_gpencil_parent_location(tgpw.depsgraph, ob, gpd, gpl, tgpw.diff_mat);

		/* do not draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE)
			continue;

		/* get frame to draw */
		bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra_eval, 0);
		if (gpf == NULL)
			continue;

		for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
			/* check if stroke can be drawn */
			if ((gps->points == NULL) || (gps->totpoints < 2)) {
				continue;
			}
			/* check if the color is visible */
			MaterialGPencilStyle *gp_style = BKE_material_gpencil_settings_get(ob, gps->mat_nr + 1);
			if ((gp_style == NULL) || (gp_style->flag & GP_STYLE_COLOR_HIDE)) {
				continue;
			}

			tgpw.gps = gps;
			tgpw.gpl = gpl;
			tgpw.gpf = gpf;
			tgpw.t_gpf = gpf;

			/* reduce thickness to avoid gaps */
			tgpw.lthick = gpl->line_change - 4;
			tgpw.opacity = 1.0;
			copy_v4_v4(tgpw.tintcolor, ink);
			tgpw.onion = true;
			tgpw.custonion = true;

			/* normal strokes */
			if ((tgpf->fill_draw_mode == GP_FILL_DMODE_STROKE) ||
			    (tgpf->fill_draw_mode == GP_FILL_DMODE_BOTH))
			{
				ED_gp_draw_fill(&tgpw);

			}

			/* 3D Lines with basic shapes and invisible lines */
			if ((tgpf->fill_draw_mode == GP_FILL_DMODE_CONTROL) ||
			    (tgpf->fill_draw_mode == GP_FILL_DMODE_BOTH))
			{
				gp_draw_basic_stroke(tgpf, gps, tgpw.diff_mat, gps->flag & GP_STROKE_CYCLIC, ink,
					tgpf->flag, tgpf->fill_threshold);
			}
		}
	}

	glDisable(GL_BLEND);
}

 /* draw strokes in offscreen buffer */
static void gp_render_offscreen(tGPDfill *tgpf)
{
	bool is_ortho = false;
	float winmat[4][4];

	if (!tgpf->gpd) {
		return;
	}

	char err_out[256] = "unknown";
	GPUOffScreen *offscreen = GPU_offscreen_create(tgpf->sizex, tgpf->sizey, 0, true, false, err_out);
	GPU_offscreen_bind(offscreen, true);
	uint flag = IB_rect | IB_rectfloat;
	ImBuf *ibuf = IMB_allocImBuf(tgpf->sizex, tgpf->sizey, 32, flag);

	rctf viewplane;
	float clipsta, clipend;

	is_ortho = ED_view3d_viewplane_get(tgpf->depsgraph, tgpf->v3d, tgpf->rv3d, tgpf->sizex, tgpf->sizey, &viewplane, &clipsta, &clipend, NULL);
	if (is_ortho) {
		orthographic_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, -clipend, clipend);
	}
	else {
		perspective_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
	}

	/* set temporary new size */
	int bwinx = tgpf->ar->winx;
	int bwiny = tgpf->ar->winy;
	rcti brect = tgpf->ar->winrct;

	tgpf->ar->winx = (short) tgpf->sizex;
	tgpf->ar->winy = (short) tgpf->sizey;
	tgpf->ar->winrct.xmin = 0;
	tgpf->ar->winrct.ymin = 0;
	tgpf->ar->winrct.xmax = tgpf->sizex;
	tgpf->ar->winrct.ymax = tgpf->sizey;

	GPU_matrix_push_projection();
	GPU_matrix_identity_set();
	GPU_matrix_push();
	GPU_matrix_identity_set();

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ED_view3d_update_viewmat(
	        tgpf->depsgraph, tgpf->scene, tgpf->v3d, tgpf->ar,
	        NULL, winmat, NULL);
	/* set for opengl */
	GPU_matrix_projection_set(tgpf->rv3d->winmat);
	GPU_matrix_set(tgpf->rv3d->viewmat);

	/* draw strokes */
	float ink[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	gp_draw_datablock(tgpf, ink);

	/* restore size */
	tgpf->ar->winx = (short)bwinx;
	tgpf->ar->winy = (short)bwiny;
	tgpf->ar->winrct = brect;

	GPU_matrix_pop_projection();
	GPU_matrix_pop();

	/* create a image to see result of template */
	if (ibuf->rect_float) {
		GPU_offscreen_read_pixels(offscreen, GL_FLOAT, ibuf->rect_float);
	}
	else if (ibuf->rect) {
		GPU_offscreen_read_pixels(offscreen, GL_UNSIGNED_BYTE, ibuf->rect);
	}
	if (ibuf->rect_float && ibuf->rect) {
		IMB_rect_from_float(ibuf);
	}

	tgpf->ima = BKE_image_add_from_imbuf(tgpf->bmain, ibuf, "GP_fill");
	tgpf->ima->id.tag |= LIB_TAG_DOIT;

	BKE_image_release_ibuf(tgpf->ima, ibuf, NULL);

	/* switch back to window-system-provided framebuffer */
	GPU_offscreen_unbind(offscreen, true);
	GPU_offscreen_free(offscreen);
}

/* return pixel data (rgba) at index */
static void get_pixel(ImBuf *ibuf, int idx, float r_col[4])
{
	if (ibuf->rect_float) {
		float *frgba = &ibuf->rect_float[idx * 4];
		copy_v4_v4(r_col, frgba);
	}
	else {
		/* XXX: This case probably doesn't happen, as we only write to the float buffer,
		 * but we get compiler warnings about uninitialised vars otherwise
		 */
		BLI_assert(!"gpencil_fill.c - get_pixel() non-float case is used!");
		zero_v4(r_col);
	}
}

/* set pixel data (rgba) at index */
static void set_pixel(ImBuf *ibuf, int idx, const float col[4])
{
	//BLI_assert(idx <= ibuf->x * ibuf->y);
	if (ibuf->rect) {
		uint *rrect = &ibuf->rect[idx];
		uchar ccol[4];

		rgba_float_to_uchar(ccol, col);
		*rrect = *((uint *)ccol);
	}

	if (ibuf->rect_float) {
		float *rrectf = &ibuf->rect_float[idx * 4];
		copy_v4_v4(rrectf, col);
	}
}

/* check if the size of the leak is narrow to determine if the stroke is closed
 * this is used for strokes with small gaps between them to get a full fill
 * and do not get a full screen fill.
 *
 * \param ibuf      Image pixel data
 * \param maxpixel  Maximum index
 * \param limit     Limit of pixels to analize
 * \param index     Index of current pixel
 * \param type      0-Horizontal 1-Vertical
 */
static bool is_leak_narrow(ImBuf *ibuf, const int maxpixel, int limit, int index, int type)
{
	float rgba[4];
	int i;
	int pt;
	bool t_a = false;
	bool t_b = false;

	/* Horizontal leak (check vertical pixels)
	 *     X
	 *	   X
	 *	   xB7
	 *	   X
	 *	   X
	 */
	if (type == LEAK_HORZ) {
		/* pixels on top */
		for (i = 1; i <= limit; i++) {
			pt = index + (ibuf->x * i);
			if (pt <= maxpixel) {
				get_pixel(ibuf, pt, rgba);
				if (rgba[0] == 1.0f) {
					t_a =  true;
					break;
				}
			}
			else {
				t_a = true; /* edge of image*/
				break;
			}
		}
		/* pixels on bottom */
		for (i = 1; i <= limit; i++) {
			pt = index - (ibuf->x * i);
			if (pt >= 0) {
				get_pixel(ibuf, pt, rgba);
				if (rgba[0] == 1.0f) {
					t_b = true;
					break;
				}
			}
			else {
				t_b = true; /* edge of image*/
				break;
			}
		}
	}

	/* Vertical leak (check horizontal pixels)
	 *
	 *  XXXxB7XX
	 *
	 */
	if (type == LEAK_VERT) {
		/* get pixel range of the row */
		int row = index / ibuf->x;
		int lowpix = row * ibuf->x;
		int higpix = lowpix + ibuf->x - 1;

		/* pixels to right */
		for (i = 0; i < limit; i++) {
			pt = index - (limit - i);
			if (pt >= lowpix) {
				get_pixel(ibuf, pt, rgba);
				if (rgba[0] == 1.0f) {
					t_a = true;
					break;
				}
			}
			else {
				t_a = true; /* edge of image*/
				break;
			}
		}
		/* pixels to left */
		for (i = 0; i < limit; i++) {
			pt = index + (limit - i);
			if (pt <= higpix) {
				get_pixel(ibuf, pt, rgba);
				if (rgba[0] == 1.0f) {
					t_b = true;
					break;
				}
			}
			else {
				t_b = true; /* edge of image */
				break;
			}
		}
	}
	return (bool)(t_a && t_b);
}

/* Boundary fill inside strokes
 * Fills the space created by a set of strokes using the stroke color as the boundary
 * of the shape to fill.
 *
 * \param tgpf       Temporary fill data
 */
static void gpencil_boundaryfill_area(tGPDfill *tgpf)
{
	ImBuf *ibuf;
	float rgba[4];
	void *lock;
	const float fill_col[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
	const int maxpixel = (ibuf->x * ibuf->y) - 1;

	BLI_Stack *stack = BLI_stack_new(sizeof(int), __func__);

	/* calculate index of the seed point using the position of the mouse */
	int index = (tgpf->sizex * tgpf->center[1]) + tgpf->center[0];
	if ((index >= 0) && (index < maxpixel)) {
		BLI_stack_push(stack, &index);
	}

	/* the fill use a stack to save the pixel list instead of the common recursive
	* 4-contact point method.
	* The problem with recursive calls is that for big fill areas, we can get max limit
	* of recursive calls and STACK_OVERFLOW error.
	*
	* The 4-contact point analyze the pixels to the left, right, bottom and top
	*      -----------
	*      |    X    |
	*      |   XoX   |
	*      |    X    |
	*      -----------
	*/
	while (!BLI_stack_is_empty(stack)) {
		int v;
		BLI_stack_pop(stack, &v);

		get_pixel(ibuf, v, rgba);

		if (true) { /* Was: 'rgba' */
			/* check if no border(red) or already filled color(green) */
			if ((rgba[0] != 1.0f) && (rgba[1] != 1.0f)) {
				/* fill current pixel */
				set_pixel(ibuf, v, fill_col);

				/* add contact pixels */
				/* pixel left */
				if (v - 1 >= 0) {
					index = v - 1;
					if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_HORZ)) {
						BLI_stack_push(stack, &index);
					}
				}
				/* pixel right */
				if (v + 1 < maxpixel) {
					index = v + 1;
					if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_HORZ)) {
						BLI_stack_push(stack, &index);
					}
				}
				/* pixel top */
				if (v + tgpf->sizex < maxpixel) {
					index = v + tgpf->sizex;
					if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_VERT)) {
						BLI_stack_push(stack, &index);
					}
				}
				/* pixel bottom */
				if (v - tgpf->sizex >= 0) {
					index = v - tgpf->sizex;
					if (!is_leak_narrow(ibuf, maxpixel, tgpf->fill_leak, v, LEAK_VERT)) {
						BLI_stack_push(stack, &index);
					}
				}
			}
		}
	}

	/* release ibuf */
	if (ibuf) {
		BKE_image_release_ibuf(tgpf->ima, ibuf, lock);
	}

	tgpf->ima->id.tag |= LIB_TAG_DOIT;
	/* free temp stack data */
	BLI_stack_free(stack);
}

/* clean external border of image to avoid infinite loops */
static void gpencil_clean_borders(tGPDfill *tgpf)
{
	ImBuf *ibuf;
	void *lock;
	const float fill_col[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
	int idx;
	int pixel = 0;

	/* horizontal lines */
	for (idx = 0; idx < ibuf->x - 1; idx++) {
		/* bottom line */
		set_pixel(ibuf, idx, fill_col);
		/* top line */
		pixel = idx + (ibuf->x * (ibuf->y - 1));
		set_pixel(ibuf, pixel, fill_col);
	}
	/* vertical lines */
	for (idx = 0; idx < ibuf->y; idx++) {
		/* left line */
		set_pixel(ibuf, ibuf->x * idx, fill_col);
		/* right line */
		pixel = ibuf->x * idx + (ibuf->x - 1);
		set_pixel(ibuf, pixel, fill_col);
	}

	/* release ibuf */
	if (ibuf) {
		BKE_image_release_ibuf(tgpf->ima, ibuf, lock);
	}

	tgpf->ima->id.tag |= LIB_TAG_DOIT;
}

/* Get the outline points of a shape using Moore Neighborhood algorithm
 *
 * This is a Blender customized version of the general algorithm described
 * in https://en.wikipedia.org/wiki/Moore_neighborhood
 */
static  void gpencil_get_outline_points(tGPDfill *tgpf)
{
	ImBuf *ibuf;
	float rgba[4];
	void *lock;
	int v[2];
	int boundary_co[2];
	int start_co[2];
	int backtracked_co[2];
	int current_check_co[2];
	int prev_check_co[2];
	int backtracked_offset[1][2] = {{0, 0}};
	// bool boundary_found = false;
	bool start_found = false;
	const int NEIGHBOR_COUNT = 8;

	int offset[8][2] = {
		{ -1, -1 },
		{ 0, -1 },
		{ 1, -1 },
		{ 1, 0 },
		{ 1, 1 },
		{ 0, 1 },
		{ -1, 1 },
		{ -1, 0 }
	};

	tgpf->stack = BLI_stack_new(sizeof(int[2]), __func__);

	ibuf = BKE_image_acquire_ibuf(tgpf->ima, NULL, &lock);
	int imagesize = ibuf->x * ibuf->y;

	/* find the initial point to start outline analysis */
	for (int idx = imagesize; idx >= 0; idx--) {
		get_pixel(ibuf, idx, rgba);
		if (rgba[1] == 1.0f) {
			boundary_co[0] = idx % ibuf->x;
			boundary_co[1] = idx / ibuf->x;
			copy_v2_v2_int(start_co, boundary_co);
			backtracked_co[0] = (idx - 1) % ibuf->x;
			backtracked_co[1] = (idx - 1) / ibuf->x;
			backtracked_offset[0][0] = backtracked_co[0] - boundary_co[0];
			backtracked_offset[0][1] = backtracked_co[1] - boundary_co[1];
			copy_v2_v2_int(prev_check_co, start_co);

			BLI_stack_push(tgpf->stack, &boundary_co);
			start_found = true;
			break;
		}
	}

	while (start_found) {
		int cur_back_offset = -1;
		for (int i = 0; i < NEIGHBOR_COUNT; i++) {
			if (backtracked_offset[0][0] == offset[i][0] &&
			    backtracked_offset[0][1] == offset[i][1])
			{
				/* Finding the bracktracked pixel offset index */
				cur_back_offset = i;
				break;
			}
		}

		int loop = 0;
		while (loop < (NEIGHBOR_COUNT - 1) && cur_back_offset != -1) {
			int offset_idx = (cur_back_offset + 1) % NEIGHBOR_COUNT;
			current_check_co[0] = boundary_co[0] + offset[offset_idx][0];
			current_check_co[1] = boundary_co[1] + offset[offset_idx][1];

			int image_idx = ibuf->x * current_check_co[1] + current_check_co[0];
			get_pixel(ibuf, image_idx, rgba);

			/* find next boundary pixel */
			if (rgba[1] == 1.0f) {
				copy_v2_v2_int(boundary_co, current_check_co);
				copy_v2_v2_int(backtracked_co, prev_check_co);
				backtracked_offset[0][0] = backtracked_co[0] - boundary_co[0];
				backtracked_offset[0][1] = backtracked_co[1] - boundary_co[1];

				BLI_stack_push(tgpf->stack, &boundary_co);

				break;
			}
			copy_v2_v2_int(prev_check_co, current_check_co);
			cur_back_offset++;
			loop++;
		}
		/* current pixel is equal to starting pixel */
		if (boundary_co[0] == start_co[0] &&
		    boundary_co[1] == start_co[1])
		{
			BLI_stack_pop(tgpf->stack, &v);
			// boundary_found = true;
			break;
		}
	}

	/* release ibuf */
	if (ibuf) {
		BKE_image_release_ibuf(tgpf->ima, ibuf, lock);
	}
}

/* get z-depth array to reproject on surface */
static void gpencil_get_depth_array(tGPDfill *tgpf)
{
	tGPspoint *ptc;
	ToolSettings *ts = tgpf->scene->toolsettings;
	int totpoints = tgpf->sbuffer_size;
	int i = 0;

	if (totpoints == 0) {
		return;
	}

	/* for surface sketching, need to set the right OpenGL context stuff so that
	* the conversions will project the values correctly...
	*/
	if (ts->gpencil_v3d_align & GP_PROJECT_DEPTH_VIEW) {
		/* need to restore the original projection settings before packing up */
		view3d_region_operator_needs_opengl(tgpf->win, tgpf->ar);
		ED_view3d_autodist_init(tgpf->depsgraph, tgpf->ar, tgpf->v3d, 0);

		/* since strokes are so fine, when using their depth we need a margin otherwise they might get missed */
		int depth_margin = 0;

		/* get an array of depths, far depths are blended */
		int mval[2], mval_prev[2] = { 0 };
		int interp_depth = 0;
		int found_depth = 0;

		tgpf->depth_arr = MEM_mallocN(sizeof(float) * totpoints, "depth_points");

		for (i = 0, ptc = tgpf->sbuffer; i < totpoints; i++, ptc++) {
			copy_v2_v2_int(mval, &ptc->x);

			if ((ED_view3d_autodist_depth(
			             tgpf->ar, mval, depth_margin, tgpf->depth_arr + i) == 0) &&
			    (i && (ED_view3d_autodist_depth_seg(
			                   tgpf->ar, mval, mval_prev, depth_margin + 1, tgpf->depth_arr + i) == 0)))
			{
				interp_depth = true;
			}
			else {
				found_depth = true;
			}

			copy_v2_v2_int(mval_prev, mval);
		}

		if (found_depth == false) {
			/* eeh... not much we can do.. :/, ignore depth in this case */
			for (i = totpoints - 1; i >= 0; i--)
				tgpf->depth_arr[i] = 0.9999f;
		}
		else {
			if (interp_depth) {
				interp_sparse_array(tgpf->depth_arr, totpoints, FLT_MAX);
			}
		}
	}
}

/* create array of points using stack as source */
static void gpencil_points_from_stack(tGPDfill *tgpf)
{
	tGPspoint *point2D;
	int totpoints = BLI_stack_count(tgpf->stack);
	if (totpoints == 0) {
		return;
	}

	tgpf->sbuffer_size = (short)totpoints;
	tgpf->sbuffer = MEM_callocN(sizeof(tGPspoint) * totpoints, __func__);

	point2D = tgpf->sbuffer;
	while (!BLI_stack_is_empty(tgpf->stack)) {
		int v[2];
		BLI_stack_pop(tgpf->stack, &v);
		point2D->x = v[0];
		point2D->y = v[1];

		point2D->pressure = 1.0f;
		point2D->strength = 1.0f;;
		point2D->time = 0.0f;
		point2D++;
	}
}

/* create a grease pencil stroke using points in buffer */
static void gpencil_stroke_from_buffer(tGPDfill *tgpf)
{
	ToolSettings *ts = tgpf->scene->toolsettings;
	int cfra_eval = (int)DEG_get_ctime(tgpf->depsgraph);

	Brush *brush;
	brush = BKE_brush_getactive_gpencil(ts);
	if (brush == NULL) {
		return;
	}

	bGPDspoint *pt;
	MDeformVert *dvert;
	tGPspoint *point2D;

	if (tgpf->sbuffer_size == 0) {
		return;
	}

	/* get frame or create a new one */
	tgpf->gpf = BKE_gpencil_layer_getframe(tgpf->gpl, cfra_eval, GP_GETFRAME_ADD_NEW);

	/* create new stroke */
	bGPDstroke *gps = MEM_callocN(sizeof(bGPDstroke), "bGPDstroke");
	gps->thickness = brush->size;
	gps->inittime = 0.0f;

	/* the polygon must be closed, so enabled cyclic */
	gps->flag |= GP_STROKE_CYCLIC;
	gps->flag |= GP_STROKE_3DSPACE;

	gps->mat_nr = BKE_object_material_slot_find_index(tgpf->ob, tgpf->mat) - 1;

	/* allocate memory for storage points */
	gps->totpoints = tgpf->sbuffer_size;
	gps->points = MEM_callocN(sizeof(bGPDspoint) * tgpf->sbuffer_size, "gp_stroke_points");
	gps->dvert = MEM_callocN(sizeof(MDeformVert) * tgpf->sbuffer_size, "gp_stroke_weights");

	/* initialize triangle memory to dummy data */
	gps->tot_triangles = 0;
	gps->triangles = NULL;
	gps->flag |= GP_STROKE_RECALC_CACHES;

	/* add stroke to frame */
	if ((ts->gpencil_flags & GP_TOOL_FLAG_PAINT_ONBACK) || (tgpf->on_back == true)) {
		BLI_addhead(&tgpf->gpf->strokes, gps);
	}
	else {
		BLI_addtail(&tgpf->gpf->strokes, gps);
	}

	/* add points */
	pt = gps->points;
	dvert = gps->dvert;
	point2D = (tGPspoint *)tgpf->sbuffer;
	for (int i = 0; i < tgpf->sbuffer_size && point2D; i++, point2D++, pt++, dvert++) {
		/* convert screen-coordinates to 3D coordinates */
		gp_stroke_convertcoords_tpoint(
		        tgpf->scene, tgpf->ar, tgpf->v3d, tgpf->ob,
		        tgpf->gpl, point2D,
		        tgpf->depth_arr ? tgpf->depth_arr + i : NULL,
		        &pt->x);

		pt->pressure = 1.0f;
		pt->strength = 1.0f;;
		pt->time = 0.0f;

		dvert->totweight = 0;
		dvert->dw = NULL;
	}

	/* smooth stroke */
	float reduce = 0.0f;
	float smoothfac = 1.0f;
	for (int r = 0; r < 1; r++) {
		for (int i = 0; i < gps->totpoints; i++) {
			BKE_gpencil_smooth_stroke(gps, i, smoothfac - reduce);
		}
		reduce += 0.25f;  // reduce the factor
	}

	/* if axis locked, reproject to plane locked */
	if ((tgpf->lock_axis > GP_LOCKAXIS_NONE) && ((ts->gpencil_v3d_align & GP_PROJECT_DEPTH_VIEW) == 0)) {
		float origin[3];
		ED_gp_get_drawing_reference(tgpf->v3d, tgpf->scene, tgpf->ob, tgpf->gpl,
			ts->gpencil_v3d_align, origin);
		ED_gp_project_stroke_to_plane(tgpf->ob, tgpf->rv3d, gps, origin,
			tgpf->lock_axis - 1);
	}

	/* if parented change position relative to parent object */
	for (int a = 0; a < tgpf->sbuffer_size; a++) {
		pt = &gps->points[a];
		gp_apply_parent_point(tgpf->depsgraph, tgpf->ob, tgpf->gpd, tgpf->gpl, pt);
	}

	/* simplify stroke */
	for (int b = 0; b < tgpf->fill_simplylvl; b++) {
		BKE_gpencil_simplify_fixed(gps);
	}
}

/* ----------------------- */
/* Drawing                 */
/* Helper: Draw status message while the user is running the operator */
static void gpencil_fill_status_indicators(bContext *C, tGPDfill *UNUSED(tgpf))
{
	const char *status_str = IFACE_("Fill: ESC/RMB cancel, LMB Fill, Shift Draw on Back");
	ED_workspace_status_text(C, status_str);
}

/* draw boundary lines to see fill limits */
static void gpencil_draw_boundary_lines(const bContext *UNUSED(C), tGPDfill *tgpf)
{
	if (!tgpf->gpd) {
		return;
	}
	float ink[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	gp_draw_datablock(tgpf, ink);
}

/* Drawing callback for modal operator in 3d mode */
static void gpencil_fill_draw_3d(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	tGPDfill *tgpf = (tGPDfill *)arg;
	/* draw only in the region that originated operator. This is required for multiwindow */
	ARegion *ar = CTX_wm_region(C);
	if (ar != tgpf->ar) {
		return;
	}

	gpencil_draw_boundary_lines(C, tgpf);
}

/* check if context is suitable for filling */
static bool gpencil_fill_poll(bContext *C)
{
	if (ED_operator_regionactive(C)) {
		ScrArea *sa = CTX_wm_area(C);
		if (sa->spacetype == SPACE_VIEW3D) {
			return 1;
		}
		else {
			CTX_wm_operator_poll_msg_set(C, "Active region not valid for filling operator");
			return 0;
		}
	}
	else {
		CTX_wm_operator_poll_msg_set(C, "Active region not set");
		return 0;
	}
}

/* Allocate memory and initialize values */
static tGPDfill *gp_session_init_fill(bContext *C, wmOperator *UNUSED(op))
{
	tGPDfill *tgpf = MEM_callocN(sizeof(tGPDfill), "GPencil Fill Data");

	/* define initial values */
	ToolSettings *ts = CTX_data_tool_settings(C);
	bGPdata *gpd = CTX_data_gpencil_data(C);
	Main *bmain = CTX_data_main(C);

	/* set current scene and window info */
	tgpf->bmain = CTX_data_main(C);
	tgpf->scene = CTX_data_scene(C);
	tgpf->ob = CTX_data_active_object(C);
	tgpf->sa = CTX_wm_area(C);
	tgpf->ar = CTX_wm_region(C);
	tgpf->rv3d = tgpf->ar->regiondata;
	tgpf->v3d = tgpf->sa->spacedata.first;
	tgpf->depsgraph = CTX_data_depsgraph(C);
	tgpf->win = CTX_wm_window(C);

	/* set GP datablock */
	tgpf->gpd = gpd;
	tgpf->gpl = BKE_gpencil_layer_getactive(gpd);

	tgpf->lock_axis = ts->gp_sculpt.lock_axis;

	tgpf->oldkey = -1;
	tgpf->sbuffer_size = 0;
	tgpf->sbuffer = NULL;
	tgpf->depth_arr = NULL;

	/* save filling parameters */
	Brush *brush = BKE_brush_getactive_gpencil(ts);
	tgpf->flag = brush->gpencil_settings->flag;
	tgpf->fill_leak = brush->gpencil_settings->fill_leak;
	tgpf->fill_threshold = brush->gpencil_settings->fill_threshold;
	tgpf->fill_simplylvl = brush->gpencil_settings->fill_simplylvl;
	tgpf->fill_draw_mode = brush->gpencil_settings->fill_draw_mode;

	/* get color info */
	Material *ma = BKE_gpencil_get_material_from_brush(brush);
	/* if no brush defaults, get material and color info */
	if ((ma == NULL) || (ma->gp_style == NULL)) {
		ma = BKE_gpencil_material_ensure(bmain, tgpf->ob);
		/* assign always the first material to the brush */
		brush->gpencil_settings->material = give_current_material(tgpf->ob, 1);
	}

	tgpf->mat = ma;

	/* init undo */
	gpencil_undo_init(tgpf->gpd);

	/* return context data for running operator */
	return tgpf;
}

/* end operator */
static void gpencil_fill_exit(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = CTX_data_active_object(C);

	/* clear undo stack */
	gpencil_undo_finish();

	/* restore cursor to indicate end of fill */
	WM_cursor_modal_restore(CTX_wm_window(C));

	tGPDfill *tgpf = op->customdata;

	/* don't assume that operator data exists at all */
	if (tgpf) {
		/* clear status message area */
		ED_workspace_status_text(C, NULL);

		MEM_SAFE_FREE(tgpf->sbuffer);
		MEM_SAFE_FREE(tgpf->depth_arr);

		/* remove drawing handler */
		if (tgpf->draw_handle_3d) {
			ED_region_draw_cb_exit(tgpf->ar->type, tgpf->draw_handle_3d);
		}

		/* delete temp image */
		if (tgpf->ima) {
			for (Image *ima = bmain->image.first; ima; ima = ima->id.next) {
				if (ima == tgpf->ima) {
					BLI_remlink(&bmain->image, ima);
					BKE_image_free(tgpf->ima);
					MEM_SAFE_FREE(tgpf->ima);
					break;
				}
			}
		}

		/* finally, free memory used by temp data */
		MEM_freeN(tgpf);
	}

	/* clear pointer */
	op->customdata = NULL;

	/* drawing batch cache is dirty now */
	if ((ob) && (ob->type == OB_GPENCIL) && (ob->data)) {
		bGPdata *gpd2 = ob->data;
		DEG_id_tag_update(&gpd2->id, OB_RECALC_OB | OB_RECALC_DATA);
		gpd2->flag |= GP_DATA_CACHE_IS_DIRTY;
	}

	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
}

static void gpencil_fill_cancel(bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	gpencil_fill_exit(C, op);
}

/* Init: Allocate memory and set init values */
static int gpencil_fill_init(bContext *C, wmOperator *op)
{
	tGPDfill *tgpf;

	/* check context */
	tgpf = op->customdata = gp_session_init_fill(C, op);
	if (tgpf == NULL) {
		/* something wasn't set correctly in context */
		gpencil_fill_exit(C, op);
		return 0;
	}

	/* everything is now setup ok */
	return 1;
}

/* start of interactive part of operator */
static int gpencil_fill_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	tGPDfill *tgpf = NULL;

	/* try to initialize context data needed */
	if (!gpencil_fill_init(C, op)) {
		gpencil_fill_exit(C, op);
		if (op->customdata)
			MEM_freeN(op->customdata);
		return OPERATOR_CANCELLED;
	}
	else {
		tgpf = op->customdata;
	}

	/* Enable custom drawing handlers to show help lines */
	if (tgpf->flag & GP_BRUSH_FILL_SHOW_HELPLINES) {
		tgpf->draw_handle_3d = ED_region_draw_cb_activate(tgpf->ar->type, gpencil_fill_draw_3d, tgpf, REGION_DRAW_POST_VIEW);
	}

	WM_cursor_modal_set(CTX_wm_window(C), BC_PAINTBRUSHCURSOR);

	gpencil_fill_status_indicators(C, tgpf);

	DEG_id_tag_update(&tgpf->gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

	/* add a modal handler for this operator*/
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* events handling during interactive part of operator */
static int gpencil_fill_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	tGPDfill *tgpf = op->customdata;

	int estate = OPERATOR_PASS_THROUGH; /* default exit state - pass through */

	switch (event->type) {
		case ESCKEY:
		case RIGHTMOUSE:
			estate = OPERATOR_CANCELLED;
			break;
		case LEFTMOUSE:
			tgpf->on_back = RNA_boolean_get(op->ptr, "on_back");
			/* first time the event is not enabled to show help lines */
			if ((tgpf->oldkey != -1) || ((tgpf->flag & GP_BRUSH_FILL_SHOW_HELPLINES) == 0)) {
				ARegion *ar = BKE_area_find_region_xy(CTX_wm_area(C), RGN_TYPE_ANY, event->x, event->y);
				if (ar) {
					bool in_bounds = false;

					/* Perform bounds check */
					in_bounds = BLI_rcti_isect_pt(&ar->winrct, event->x, event->y);

					if ((in_bounds) && (ar->regiontype == RGN_TYPE_WINDOW)) {
						/* TODO GPXX: Verify the mouse click is right for any window size */
						tgpf->center[0] = event->mval[0];
						tgpf->center[1] = event->mval[1];

						/* save size */
						tgpf->sizex = ar->winx;
						tgpf->sizey = ar->winy;

						/* render screen to temp image */
						gp_render_offscreen(tgpf);

						/* apply boundary fill */
						gpencil_boundaryfill_area(tgpf);

						/* clean borders to avoid infinite loops */
						gpencil_clean_borders(tgpf);

						/* analyze outline */
						gpencil_get_outline_points(tgpf);

						/* create array of points from stack */
						gpencil_points_from_stack(tgpf);

						/* create z-depth array for reproject */
						gpencil_get_depth_array(tgpf);

						/* create stroke and reproject */
						gpencil_stroke_from_buffer(tgpf);

						/* free temp stack data */
						if (tgpf->stack) {
							BLI_stack_free(tgpf->stack);
						}

						/* push undo data */
						gpencil_undo_push(tgpf->gpd);

						estate = OPERATOR_FINISHED;
					}
					else {
						estate = OPERATOR_CANCELLED;
					}
				}
				else {
					estate = OPERATOR_CANCELLED;
				}
			}
			tgpf->oldkey = event->type;
			break;
	}
	/* process last operations before exiting */
	switch (estate) {
		case OPERATOR_FINISHED:
			gpencil_fill_exit(C, op);
			WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
			break;

		case OPERATOR_CANCELLED:
			gpencil_fill_exit(C, op);
			break;

		case OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH:
			break;
	}

	/* return status code */
	return estate;
}

void GPENCIL_OT_fill(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Grease Pencil Fill";
	ot->idname = "GPENCIL_OT_fill";
	ot->description = "Fill with color the shape formed by strokes";

	/* api callbacks */
	ot->invoke = gpencil_fill_invoke;
	ot->modal = gpencil_fill_modal;
	ot->poll = gpencil_fill_poll;
	ot->cancel = gpencil_fill_cancel;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

	prop = RNA_def_boolean(ot->srna, "on_back", false, "Draw On Back", "Send new stroke to Back");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
