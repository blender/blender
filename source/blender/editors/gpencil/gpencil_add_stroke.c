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
 * The Original Code is Copyright (C) 2017 Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez, Matias Mendiola
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_add_stroke.c
 *  \ingroup edgpencil
 */

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"
#include "BKE_material.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_gpencil.h"

/* Definition of the most important info from a color */
typedef struct ColorTemplate {
	const char *name;
	float line[4];
	float fill[4];
} ColorTemplate;

/* Add color an ensure duplications (matched by name) */
static int gp_stroke_material(Main *bmain, Object *ob, const ColorTemplate *pct)
{
	short *totcol = give_totcolp(ob);
	Material *ma = NULL;
	for (short i = 0; i < *totcol; i++) {
		ma = give_current_material(ob, i + 1);
		if (STREQ(ma->id.name, pct->name)) {
			return i;
		}
	}

	/* create a new one */
	BKE_object_material_slot_add(bmain, ob);
	ma = BKE_material_add_gpencil(bmain, pct->name);
	assign_material(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

	copy_v4_v4(ma->gp_style->stroke_rgba, pct->line);
	copy_v4_v4(ma->gp_style->fill_rgba, pct->fill);

	return BKE_gpencil_get_material_index(ob, ma) - 1;
}

/* ***************************************************************** */
/* Stroke Geometry */

static const float data0[175 * GP_PRIM_DATABUF_SIZE] = {
	-1.281f, 0.0f, -0.315f, 0.038f, 1.0f, -1.269f, 0.0f, -0.302f, 0.069f, 1.0f,
	-1.261f, 0.0f, -0.293f, 0.089f, 1.0f, -1.251f, 0.0f, -0.282f, 0.112f, 1.0f,
	-1.241f, 0.0f, -0.271f, 0.134f, 1.0f, -1.23f, 0.0f, -0.259f, 0.155f, 1.0f,
	-1.219f, 0.0f, -0.247f, 0.175f, 1.0f, -1.208f, 0.0f, -0.234f, 0.194f, 1.0f,
	-1.196f, 0.0f, -0.221f, 0.211f, 1.0f, -1.184f, 0.0f, -0.208f, 0.227f, 1.0f,
	-1.172f, 0.0f, -0.194f, 0.242f, 1.0f, -1.159f, 0.0f, -0.18f, 0.256f, 1.0f,
	-1.147f, 0.0f, -0.165f, 0.268f, 1.0f, -1.134f, 0.0f, -0.151f, 0.28f, 1.0f,
	-1.121f, 0.0f, -0.136f, 0.29f, 1.0f, -1.108f, 0.0f, -0.121f, 0.299f, 1.0f,
	-1.094f, 0.0f, -0.106f, 0.307f, 1.0f, -1.08f, 0.0f, -0.091f, 0.315f, 1.0f,
	-1.066f, 0.0f, -0.076f, 0.322f, 1.0f, -1.052f, 0.0f, -0.061f, 0.329f, 1.0f,
	-1.037f, 0.0f, -0.047f, 0.335f, 1.0f, -1.022f, 0.0f, -0.032f, 0.341f, 1.0f,
	-1.007f, 0.0f, -0.017f, 0.346f, 1.0f, -0.991f, 0.0f, -0.003f, 0.351f, 1.0f,
	-0.975f, 0.0f, 0.012f, 0.355f, 1.0f, -0.959f, 0.0f, 0.027f, 0.36f, 1.0f,
	-0.942f, 0.0f, 0.041f, 0.364f, 1.0f, -0.926f, 0.0f, 0.056f, 0.368f, 1.0f,
	-0.909f, 0.0f, 0.071f, 0.371f, 1.0f, -0.893f, 0.0f, 0.086f, 0.373f, 1.0f,
	-0.876f, 0.0f, 0.1f, 0.376f, 1.0f, -0.859f, 0.0f, 0.115f, 0.377f, 1.0f,
	-0.842f, 0.0f, 0.129f, 0.378f, 1.0f, -0.824f, 0.0f, 0.144f, 0.379f, 1.0f,
	-0.807f, 0.0f, 0.158f, 0.379f, 1.0f, -0.79f, 0.0f, 0.172f, 0.379f, 1.0f,
	-0.773f, 0.0f, 0.186f, 0.38f, 1.0f, -0.755f, 0.0f, 0.199f, 0.38f, 1.0f,
	-0.738f, 0.0f, 0.212f, 0.381f, 1.0f, -0.721f, 0.0f, 0.224f, 0.382f, 1.0f,
	-0.703f, 0.0f, 0.236f, 0.384f, 1.0f, -0.686f, 0.0f, 0.248f, 0.386f, 1.0f,
	-0.67f, 0.0f, 0.26f, 0.388f, 1.0f, -0.653f, 0.0f, 0.27f, 0.39f, 1.0f,
	-0.637f, 0.0f, 0.28f, 0.393f, 1.0f, -0.621f, 0.0f, 0.29f, 0.396f, 1.0f,
	-0.605f, 0.0f, 0.298f, 0.399f, 1.0f, -0.589f, 0.0f, 0.306f, 0.403f, 1.0f,
	-0.574f, 0.0f, 0.313f, 0.407f, 1.0f, -0.559f, 0.0f, 0.319f, 0.411f, 1.0f,
	-0.544f, 0.0f, 0.325f, 0.415f, 1.0f, -0.53f, 0.0f, 0.331f, 0.42f, 1.0f,
	-0.516f, 0.0f, 0.336f, 0.425f, 1.0f, -0.503f, 0.0f, 0.34f, 0.431f, 1.0f,
	-0.489f, 0.0f, 0.344f, 0.437f, 1.0f, -0.477f, 0.0f, 0.347f, 0.443f, 1.0f,
	-0.464f, 0.0f, 0.35f, 0.45f, 1.0f, -0.452f, 0.0f, 0.352f, 0.457f, 1.0f,
	-0.44f, 0.0f, 0.354f, 0.464f, 1.0f, -0.429f, 0.0f, 0.355f, 0.471f, 1.0f,
	-0.418f, 0.0f, 0.355f, 0.479f, 1.0f, -0.407f, 0.0f, 0.355f, 0.487f, 1.0f,
	-0.397f, 0.0f, 0.354f, 0.495f, 1.0f, -0.387f, 0.0f, 0.353f, 0.503f, 1.0f,
	-0.378f, 0.0f, 0.351f, 0.512f, 1.0f, -0.368f, 0.0f, 0.348f, 0.52f, 1.0f,
	-0.36f, 0.0f, 0.344f, 0.528f, 1.0f, -0.351f, 0.0f, 0.34f, 0.537f, 1.0f,
	-0.344f, 0.0f, 0.336f, 0.545f, 1.0f, -0.336f, 0.0f, 0.33f, 0.553f, 1.0f,
	-0.329f, 0.0f, 0.324f, 0.562f, 1.0f, -0.322f, 0.0f, 0.318f, 0.57f, 1.0f,
	-0.316f, 0.0f, 0.31f, 0.579f, 1.0f, -0.311f, 0.0f, 0.303f, 0.588f, 1.0f,
	-0.306f, 0.0f, 0.294f, 0.597f, 1.0f, -0.301f, 0.0f, 0.285f, 0.606f, 1.0f,
	-0.297f, 0.0f, 0.275f, 0.615f, 1.0f, -0.293f, 0.0f, 0.264f, 0.625f, 1.0f,
	-0.29f, 0.0f, 0.253f, 0.635f, 1.0f, -0.288f, 0.0f, 0.241f, 0.644f, 1.0f,
	-0.286f, 0.0f, 0.229f, 0.654f, 1.0f, -0.285f, 0.0f, 0.216f, 0.664f, 1.0f,
	-0.284f, 0.0f, 0.202f, 0.675f, 1.0f, -0.283f, 0.0f, 0.188f, 0.685f, 1.0f,
	-0.283f, 0.0f, 0.173f, 0.696f, 1.0f, -0.284f, 0.0f, 0.158f, 0.707f, 1.0f,
	-0.285f, 0.0f, 0.142f, 0.718f, 1.0f, -0.286f, 0.0f, 0.125f, 0.729f, 1.0f,
	-0.288f, 0.0f, 0.108f, 0.74f, 1.0f, -0.29f, 0.0f, 0.091f, 0.751f, 1.0f,
	-0.293f, 0.0f, 0.073f, 0.761f, 1.0f, -0.295f, 0.0f, 0.054f, 0.772f, 1.0f,
	-0.298f, 0.0f, 0.035f, 0.782f, 1.0f, -0.302f, 0.0f, 0.016f, 0.793f, 1.0f,
	-0.305f, 0.0f, -0.004f, 0.804f, 1.0f, -0.309f, 0.0f, -0.024f, 0.815f, 1.0f,
	-0.313f, 0.0f, -0.044f, 0.828f, 1.0f, -0.317f, 0.0f, -0.065f, 0.843f, 1.0f,
	-0.321f, 0.0f, -0.085f, 0.86f, 1.0f, -0.326f, 0.0f, -0.106f, 0.879f, 1.0f,
	-0.33f, 0.0f, -0.127f, 0.897f, 1.0f, -0.335f, 0.0f, -0.148f, 0.915f, 1.0f,
	-0.339f, 0.0f, -0.168f, 0.932f, 1.0f, -0.344f, 0.0f, -0.189f, 0.947f, 1.0f,
	-0.348f, 0.0f, -0.21f, 0.962f, 1.0f, -0.353f, 0.0f, -0.23f, 0.974f, 1.0f,
	-0.357f, 0.0f, -0.25f, 0.985f, 1.0f, -0.361f, 0.0f, -0.27f, 0.995f, 1.0f,
	-0.365f, 0.0f, -0.29f, 1.004f, 1.0f, -0.369f, 0.0f, -0.309f, 1.011f, 1.0f,
	-0.372f, 0.0f, -0.328f, 1.018f, 1.0f, -0.375f, 0.0f, -0.347f, 1.024f, 1.0f,
	-0.377f, 0.0f, -0.365f, 1.029f, 1.0f, -0.379f, 0.0f, -0.383f, 1.033f, 1.0f,
	-0.38f, 0.0f, -0.4f, 1.036f, 1.0f, -0.38f, 0.0f, -0.417f, 1.037f, 1.0f,
	-0.38f, 0.0f, -0.434f, 1.037f, 1.0f, -0.379f, 0.0f, -0.449f, 1.035f, 1.0f,
	-0.377f, 0.0f, -0.464f, 1.032f, 1.0f, -0.374f, 0.0f, -0.478f, 1.029f, 1.0f,
	-0.371f, 0.0f, -0.491f, 1.026f, 1.0f, -0.366f, 0.0f, -0.503f, 1.023f, 1.0f,
	-0.361f, 0.0f, -0.513f, 1.021f, 1.0f, -0.354f, 0.0f, -0.523f, 1.019f, 1.0f,
	-0.347f, 0.0f, -0.531f, 1.017f, 1.0f, -0.339f, 0.0f, -0.538f, 1.016f, 1.0f,
	-0.33f, 0.0f, -0.543f, 1.016f, 1.0f, -0.32f, 0.0f, -0.547f, 1.016f, 1.0f,
	-0.31f, 0.0f, -0.549f, 1.016f, 1.0f, -0.298f, 0.0f, -0.55f, 1.017f, 1.0f,
	-0.286f, 0.0f, -0.55f, 1.017f, 1.0f, -0.274f, 0.0f, -0.548f, 1.018f, 1.0f,
	-0.261f, 0.0f, -0.544f, 1.017f, 1.0f, -0.247f, 0.0f, -0.539f, 1.017f, 1.0f,
	-0.232f, 0.0f, -0.533f, 1.016f, 1.0f, -0.218f, 0.0f, -0.525f, 1.015f, 1.0f,
	-0.202f, 0.0f, -0.515f, 1.013f, 1.0f, -0.186f, 0.0f, -0.503f, 1.009f, 1.0f,
	-0.169f, 0.0f, -0.49f, 1.005f, 1.0f, -0.151f, 0.0f, -0.475f, 0.998f, 1.0f,
	-0.132f, 0.0f, -0.458f, 0.99f, 1.0f, -0.112f, 0.0f, -0.44f, 0.98f, 1.0f,
	-0.091f, 0.0f, -0.42f, 0.968f, 1.0f, -0.069f, 0.0f, -0.398f, 0.955f, 1.0f,
	-0.045f, 0.0f, -0.375f, 0.939f, 1.0f, -0.021f, 0.0f, -0.35f, 0.923f, 1.0f,
	0.005f, 0.0f, -0.324f, 0.908f, 1.0f, 0.031f, 0.0f, -0.297f, 0.895f, 1.0f,
	0.06f, 0.0f, -0.268f, 0.882f, 1.0f, 0.089f, 0.0f, -0.238f, 0.87f, 1.0f,
	0.12f, 0.0f, -0.207f, 0.858f, 1.0f, 0.153f, 0.0f, -0.175f, 0.844f, 1.0f,
	0.187f, 0.0f, -0.14f, 0.828f, 1.0f, 0.224f, 0.0f, -0.104f, 0.81f, 1.0f,
	0.262f, 0.0f, -0.067f, 0.79f, 1.0f, 0.302f, 0.0f, -0.027f, 0.769f, 1.0f,
	0.344f, 0.0f, 0.014f, 0.747f, 1.0f, 0.388f, 0.0f, 0.056f, 0.724f, 1.0f,
	0.434f, 0.0f, 0.1f, 0.7f, 1.0f, 0.483f, 0.0f, 0.145f, 0.676f, 1.0f,
	0.533f, 0.0f, 0.191f, 0.651f, 1.0f, 0.585f, 0.0f, 0.238f, 0.625f, 1.0f,
	0.637f, 0.0f, 0.284f, 0.599f, 1.0f, 0.69f, 0.0f, 0.33f, 0.573f, 1.0f,
	0.746f, 0.0f, 0.376f, 0.546f, 1.0f, 0.802f, 0.0f, 0.421f, 0.516f, 1.0f,
	0.859f, 0.0f, 0.464f, 0.483f, 1.0f, 0.915f, 0.0f, 0.506f, 0.446f, 1.0f,
	0.97f, 0.0f, 0.545f, 0.407f, 1.0f, 1.023f, 0.0f, 0.581f, 0.365f, 1.0f,
	1.075f, 0.0f, 0.614f, 0.322f, 1.0f, 1.122f, 0.0f, 0.643f, 0.28f, 1.0f,
	1.169f, 0.0f, 0.671f, 0.236f, 1.0f, 1.207f, 0.0f, 0.693f, 0.202f, 1.0f,
	1.264f, 0.0f, 0.725f, 0.155f, 1.0f,
};

/* ***************************************************************** */
/* Color Data */

static const ColorTemplate gp_stroke_material_black = {
	"Black",
	{0.0f, 0.0f, 0.0f, 1.0f},
	{0.0f, 0.0f, 0.0f, 0.0f},
};

static const ColorTemplate gp_stroke_material_white = {
	"White",
	{1.0f, 1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f, 0.0f},
};

static const ColorTemplate gp_stroke_material_red = {
	"Red",
	{1.0f, 0.0f, 0.0f, 1.0f},
	{0.0f, 0.0f, 0.0f, 0.0f},
};

static const ColorTemplate gp_stroke_material_green = {
	"Green",
	{0.0f, 1.0f, 0.0f, 1.0f},
	{0.0f, 0.0f, 0.0f, 0.0f},
};

static const ColorTemplate gp_stroke_material_blue = {
	"Blue",
	{0.0f, 0.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f, 0.0f},
};

static const ColorTemplate gp_stroke_material_grey = {
	"Grey",
	{0.358f, 0.358f, 0.358f, 1.0f},
	{0.5f, 0.5f, 0.5f, 1.0f},
};

/* ***************************************************************** */
/* Stroke API */

/* add a Simple stroke with colors (original design created by Daniel M. Lara and Matias Mendiola) */
void ED_gpencil_create_stroke(bContext *C, float mat[4][4])
{
	Main *bmain = CTX_data_main(C);
	Object *ob = CTX_data_active_object(C);
	Depsgraph *depsgraph = CTX_data_depsgraph(C);
	int cfra_eval = (int)DEG_get_ctime(depsgraph);
	bGPdata *gpd = (bGPdata *)ob->data;
	bGPDstroke *gps;

	/* create colors */
	int color_black = gp_stroke_material(bmain, ob, &gp_stroke_material_black);
	gp_stroke_material(bmain, ob, &gp_stroke_material_white);
	gp_stroke_material(bmain, ob, &gp_stroke_material_red);
	gp_stroke_material(bmain, ob, &gp_stroke_material_green);
	gp_stroke_material(bmain, ob, &gp_stroke_material_blue);
	gp_stroke_material(bmain, ob, &gp_stroke_material_grey);

	/* layers */
	bGPDlayer *colors = BKE_gpencil_layer_addnew(gpd, "Colors", false);
	bGPDlayer *lines = BKE_gpencil_layer_addnew(gpd, "Lines", false);

	/* frames */
	bGPDframe *frame_color = BKE_gpencil_frame_addnew(colors, cfra_eval);
	bGPDframe *frame_lines = BKE_gpencil_frame_addnew(lines, cfra_eval);
	UNUSED_VARS(frame_color);

	/* generate stroke */
	gps = BKE_gpencil_add_stroke(frame_lines, color_black, 175, 3);
	BKE_gpencil_stroke_add_points(gps, data0, 175, mat);

	/* update depsgraph */
	DEG_id_tag_update(&gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
	gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
}
