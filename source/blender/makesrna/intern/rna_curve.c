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
 * Contributor(s): Blender Foundation (2008), Juho Veps�l�inen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_curve.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_font.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifndef RNA_RUNTIME
static EnumPropertyItem beztriple_handle_type_items[] = {
	{HD_FREE, "FREE", 0, "Free", ""},
	{HD_VECT, "VECTOR", 0, "Vector", ""},
	{HD_ALIGN, "ALIGNED", 0, "Aligned", ""},
	{HD_AUTO, "AUTO", 0, "Auto", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif
		
EnumPropertyItem rna_enum_keyframe_handle_type_items[] = {
	{HD_FREE, "FREE", 0, "Free", ""},
	{HD_VECT, "VECTOR", 0, "Vector", ""},
	{HD_ALIGN, "ALIGNED", 0, "Aligned", ""},
	{HD_AUTO, "AUTO", 0, "Automatic", ""},
	{HD_AUTO_ANIM, "AUTO_CLAMPED", 0, "Auto Clamped", "Auto handles clamped to not overshoot"},
	{0, NULL, 0, NULL, NULL}
};

EnumPropertyItem rna_enum_beztriple_interpolation_mode_items[] = {
	/* interpolation */
	{0, "", 0, N_("Interpolation"), "Standard transitions between keyframes"},
	{BEZT_IPO_CONST, "CONSTANT", ICON_IPO_CONSTANT, "Constant", "No interpolation, value of A gets held until B is encountered"},
	{BEZT_IPO_LIN, "LINEAR", ICON_IPO_LINEAR, "Linear", "Straight-line interpolation between A and B (i.e. no ease in/out)"},
	{BEZT_IPO_BEZ, "BEZIER", ICON_IPO_BEZIER, "Bezier", "Smooth interpolation between A and B, with some control over curve shape"},
	
	/* easing */
	{0, "", 0, N_("Easing (by strength)"), "Predefined inertial transitions, useful for motion graphics (from least to most ''dramatic'')"},
	{BEZT_IPO_SINE, "SINE", ICON_IPO_SINE, "Sinusoidal", "Sinusoidal easing (weakest, almost linear but with a slight curvature)"},
	{BEZT_IPO_QUAD, "QUAD", ICON_IPO_QUAD, "Quadratic", "Quadratic easing"},
	{BEZT_IPO_CUBIC, "CUBIC", ICON_IPO_CUBIC, "Cubic", "Cubic easing"},
	{BEZT_IPO_QUART, "QUART", ICON_IPO_QUART, "Quartic", "Quartic easing"},
	{BEZT_IPO_QUINT, "QUINT", ICON_IPO_QUINT, "Quintic", "Quintic easing"},
	{BEZT_IPO_EXPO, "EXPO", ICON_IPO_EXPO, "Exponential", "Exponential easing (dramatic)"},
	{BEZT_IPO_CIRC, "CIRC", ICON_IPO_CIRC, "Circular", "Circular easing (strongest and most dynamic)"},
	
	{0, "", 0, N_("Dynamic Effects"), "Simple physics-inspired easing effects"},
	{BEZT_IPO_BACK, "BACK", ICON_IPO_BACK, "Back", "Cubic easing with overshoot and settle"},
	{BEZT_IPO_BOUNCE, "BOUNCE", ICON_IPO_BOUNCE, "Bounce", "Exponentially decaying parabolic bounce, like when objects collide"},
	{BEZT_IPO_ELASTIC, "ELASTIC", ICON_IPO_ELASTIC, "Elastic", "Exponentially decaying sine wave, like an elastic band"},
	
	{0, NULL, 0, NULL, NULL}
};

#ifndef RNA_RUNTIME
static EnumPropertyItem curve_type_items[] = {
	{CU_POLY, "POLY", 0, "Poly", ""},
	{CU_BEZIER, "BEZIER", 0, "Bezier", ""},
	{CU_BSPLINE, "BSPLINE", 0, "BSpline", ""},
	{CU_CARDINAL, "CARDINAL", 0, "Cardinal", ""},
	{CU_NURBS, "NURBS", 0, "Ease", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

static const EnumPropertyItem curve3d_fill_mode_items[] = {
	{0, "FULL", 0, "Full", ""},
	{CU_BACK, "BACK", 0, "Back", ""},
	{CU_FRONT, "FRONT", 0, "Front", ""},
	{CU_FRONT | CU_BACK, "HALF", 0, "Half", ""},
	{0, NULL, 0, NULL, NULL}
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem curve2d_fill_mode_items[] = {
	{0, "NONE", 0, "None", ""},
	{CU_BACK, "BACK", 0, "Back", ""},
	{CU_FRONT, "FRONT", 0, "Front", ""},
	{CU_FRONT | CU_BACK, "BOTH", 0, "Both", ""},
	{0, NULL, 0, NULL, NULL}
};
#endif

#ifdef RNA_RUNTIME

#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "WM_api.h"

#include "MEM_guardedalloc.h"

#include "ED_curve.h" /* for BKE_curve_nurbs_get */

/* highly irritating but from RNA we cant know this */
static Nurb *curve_nurb_from_point(Curve *cu, const void *point, int *nu_index, int *pt_index)
{
	ListBase *nurbs = BKE_curve_nurbs_get(cu);
	Nurb *nu;
	int i = 0;

	for (nu = nurbs->first; nu; nu = nu->next, i++) {
		if (nu->type == CU_BEZIER) {
			if (point >= (void *)nu->bezt && point < (void *)(nu->bezt + nu->pntsu)) {
				break;
			}
		}
		else {
			if (point >= (void *)nu->bp && point < (void *)(nu->bp + (nu->pntsu * nu->pntsv))) {
				break;
			}
		}
	}

	if (nu) {
		if (nu_index) {
			*nu_index = i;
		}
	
		if (pt_index) {
			if (nu->type == CU_BEZIER) *pt_index = (int)((BezTriple *)point - nu->bezt);
			else *pt_index = (int)((BPoint *)point - nu->bp);
		}
	}

	return nu;
}

static StructRNA *rna_Curve_refine(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->data;
	short obtype = BKE_curve_type_get(cu);
	
	if (obtype == OB_FONT) return &RNA_TextCurve;
	else if (obtype == OB_SURF) return &RNA_SurfaceCurve;
	else return &RNA_Curve;
}

static void rna_BezTriple_handle1_get(PointerRNA *ptr, float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	copy_v3_v3(values, bezt->vec[0]);
}

static void rna_BezTriple_handle1_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	copy_v3_v3(bezt->vec[0], values);
}

static void rna_BezTriple_handle2_get(PointerRNA *ptr, float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	copy_v3_v3(values, bezt->vec[2]);
}

static void rna_BezTriple_handle2_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	copy_v3_v3(bezt->vec[2], values);
}

static void rna_BezTriple_ctrlpoint_get(PointerRNA *ptr, float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	copy_v3_v3(values, bezt->vec[1]);
}

static void rna_BezTriple_ctrlpoint_set(PointerRNA *ptr, const float *values)
{
	BezTriple *bezt = (BezTriple *)ptr->data;
	copy_v3_v3(bezt->vec[1], values);
}

static void rna_Curve_texspace_set(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->data;
	
	if (cu->texflag & CU_AUTOSPACE)
		BKE_curve_texspace_calc(cu);
}

static int rna_Curve_texspace_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
	Curve *cu = (Curve *)ptr->data;
	return (cu->texflag & CU_AUTOSPACE) ? 0 : PROP_EDITABLE;
}

static void rna_Curve_texspace_loc_get(PointerRNA *ptr, float *values)
{
	Curve *cu = (Curve *)ptr->data;
	
	if (!cu->bb)
		BKE_curve_texspace_calc(cu);
	
	copy_v3_v3(values, cu->loc);
}

static void rna_Curve_texspace_loc_set(PointerRNA *ptr, const float *values)
{
	Curve *cu = (Curve *)ptr->data;
	
	copy_v3_v3(cu->loc, values);
}

static void rna_Curve_texspace_size_get(PointerRNA *ptr, float *values)
{
	Curve *cu = (Curve *)ptr->data;
	
	if (!cu->bb)
		BKE_curve_texspace_calc(cu);
	
	copy_v3_v3(values, cu->size);
}

static void rna_Curve_texspace_size_set(PointerRNA *ptr, const float *values)
{
	Curve *cu = (Curve *)ptr->data;
	
	copy_v3_v3(cu->size, values);
}

static void rna_Curve_material_index_range(PointerRNA *ptr, int *min, int *max,
                                           int *UNUSED(softmin), int *UNUSED(softmax))
{
	Curve *cu = (Curve *)ptr->id.data;
	*min = 0;
	*max = max_ii(0, cu->totcol - 1);
}

/* simply offset by don't expose -1 */
static int rna_ChariInfo_material_index_get(PointerRNA *ptr)
{
	CharInfo *info = ptr->data;
	return info->mat_nr ? info->mat_nr - 1 : 0;
}

static void rna_ChariInfo_material_index_set(PointerRNA *ptr, int value)
{
	CharInfo *info = ptr->data;
	info->mat_nr = value + 1;
}

static void rna_Curve_active_textbox_index_range(PointerRNA *ptr, int *min, int *max,
                                                 int *UNUSED(softmin), int *UNUSED(softmax))
{
	Curve *cu = (Curve *)ptr->id.data;
	*min = 0;
	*max = max_ii(0, cu->totbox - 1);
}


static void rna_Curve_dimension_set(PointerRNA *ptr, int value)
{
	Curve *cu = (Curve *)ptr->id.data;
	if (value == CU_3D) cu->flag |=  CU_3D;
	else cu->flag &= ~CU_3D;

	BKE_curve_curve_dimension_update(cu);
}

static EnumPropertyItem *rna_Curve_fill_mode_itemf(bContext *UNUSED(C), PointerRNA *ptr,
                                                   PropertyRNA *UNUSED(prop), bool *UNUSED(r_free))
{
	Curve *cu = (Curve *)ptr->id.data;

	/* cast to quiet warning it IS a const still */
	return (EnumPropertyItem *)((cu->flag & CU_3D) ? curve3d_fill_mode_items : curve2d_fill_mode_items);
}

static int rna_Nurb_length(PointerRNA *ptr)
{
	Nurb *nu = (Nurb *)ptr->data;
	if (nu->type == CU_BEZIER) return 0;
	return nu->pntsv > 0 ? nu->pntsu * nu->pntsv : nu->pntsu;
}

static void rna_Nurb_type_set(PointerRNA *ptr, int value)
{
	Curve *cu = (Curve *)ptr->id.data;
	Nurb *nu = (Nurb *)ptr->data;
	const int pntsu_prev = nu->pntsu;

	if (BKE_nurb_type_convert(nu, value, true)) {
		if (nu->pntsu != pntsu_prev) {
			cu->actvert = CU_ACT_NONE;
		}
	}
}

static void rna_BPoint_array_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Nurb *nu = (Nurb *)ptr->data;
	rna_iterator_array_begin(iter, (void *)nu->bp, sizeof(BPoint),
	                         nu->pntsv > 0 ? nu->pntsu * nu->pntsv : nu->pntsu, 0, NULL);
}

static void rna_Curve_update_data_id(Main *UNUSED(bmain), Scene *UNUSED(scene), ID *id)
{
	DAG_id_tag_update(id, 0);
	WM_main_add_notifier(NC_GEOM | ND_DATA, id);
}

static void rna_Curve_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_Curve_update_data_id(bmain, scene, ptr->id.data);
}

static void rna_Curve_update_deps(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DAG_relations_tag_update(bmain);
	rna_Curve_update_data(bmain, scene, ptr);
}

static void rna_Curve_update_points(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	Nurb *nu = curve_nurb_from_point(cu, ptr->data, NULL, NULL);

	if (nu)
		BKE_nurb_handles_calc(nu);

	rna_Curve_update_data(bmain, scene, ptr);
}

static PointerRNA rna_Curve_bevelObject_get(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	Object *ob = cu->bevobj;

	if (ob)
		return rna_pointer_inherit_refine(ptr, &RNA_Object, ob);

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_Curve_bevelObject_set(PointerRNA *ptr, PointerRNA value)
{
	Curve *cu = (Curve *)ptr->id.data;
	Object *ob = (Object *)value.data;

	if (ob) {
		/* if bevel object has got the save curve, as object, for which it's */
		/* set as bevobj, there could be infinity loop in displist calculation */
		if (ob->type == OB_CURVE && ob->data != cu) {
			cu->bevobj = ob;
			id_lib_extern((ID *)ob);
		}
	}
	else {
		cu->bevobj = NULL;
	}
}

static int rna_Curve_otherObject_poll(PointerRNA *ptr, PointerRNA value)
{
	Curve *cu = (Curve *)ptr->id.data;
	Object *ob = (Object *)value.data;

	if (ob) {
		if (ob->type == OB_CURVE && ob->data != cu) {
			return 1;
		}
	}

	return 0;
}

static PointerRNA rna_Curve_taperObject_get(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	Object *ob = cu->taperobj;

	if (ob)
		return rna_pointer_inherit_refine(ptr, &RNA_Object, ob);

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_Curve_taperObject_set(PointerRNA *ptr, PointerRNA value)
{
	Curve *cu = (Curve *)ptr->id.data;
	Object *ob = (Object *)value.data;

	if (ob) {
		/* if taper object has got the save curve, as object, for which it's */
		/* set as bevobj, there could be infinity loop in displist calculation */
		if (ob->type == OB_CURVE && ob->data != cu) {
			cu->taperobj = ob;
			id_lib_extern((ID *)ob);
		}
	}
	else {
		cu->taperobj = NULL;
	}
}

static void rna_Curve_resolution_u_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	ListBase *nurbs = BKE_curve_nurbs_get(cu);
	Nurb *nu = nurbs->first;

	while (nu) {
		nu->resolu = cu->resolu;
		nu = nu->next;
	}
	
	rna_Curve_update_data(bmain, scene, ptr);
}

static void rna_Curve_resolution_v_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	ListBase *nurbs = BKE_curve_nurbs_get(cu);
	Nurb *nu = nurbs->first;


	while (nu) {
		nu->resolv = cu->resolv;
		nu = nu->next;
	}

	rna_Curve_update_data(bmain, scene, ptr);
}

static float rna_Curve_offset_get(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	return cu->width - 1.0f;
}

static void rna_Curve_offset_set(PointerRNA *ptr, float value)
{
	Curve *cu = (Curve *)ptr->id.data;
	cu->width = 1.0f + value;
}

static int rna_Curve_body_length(PointerRNA *ptr);
static void rna_Curve_body_get(PointerRNA *ptr, char *value)
{
	Curve *cu = (Curve *)ptr->id.data;
	BLI_strncpy(value, cu->str, rna_Curve_body_length(ptr) + 1);
}

static int rna_Curve_body_length(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	return cu->len;
}

/* TODO, how to handle editmode? */
static void rna_Curve_body_set(PointerRNA *ptr, const char *value)
{
	size_t len_bytes;
	size_t len_chars = BLI_strlen_utf8_ex(value, &len_bytes);

	Curve *cu = (Curve *)ptr->id.data;

	cu->len_wchar = len_chars;
	cu->len = len_bytes;
	cu->pos = len_chars;

	if (cu->str)
		MEM_freeN(cu->str);
	if (cu->strinfo)
		MEM_freeN(cu->strinfo);

	cu->str = MEM_mallocN(len_bytes + sizeof(wchar_t), "str");
	cu->strinfo = MEM_callocN((len_chars + 4) * sizeof(CharInfo), "strinfo");

	BLI_strncpy(cu->str, value, len_bytes + 1);
}

static void rna_Nurb_update_cyclic_u(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Nurb *nu = (Nurb *)ptr->data;

	if (nu->type == CU_BEZIER) {
		BKE_nurb_handles_calc(nu);
	}
	else {
		BKE_nurb_knot_calc_u(nu);
	}

	rna_Curve_update_data(bmain, scene, ptr);
}

static void rna_Nurb_update_cyclic_v(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Nurb *nu = (Nurb *)ptr->data;

	BKE_nurb_knot_calc_v(nu);

	rna_Curve_update_data(bmain, scene, ptr);
}

static void rna_Nurb_update_knot_u(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Nurb *nu = (Nurb *)ptr->data;

	BKE_nurb_order_clamp_u(nu);
	BKE_nurb_knot_calc_u(nu);

	rna_Curve_update_data(bmain, scene, ptr);
}

static void rna_Nurb_update_knot_v(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Nurb *nu = (Nurb *)ptr->data;

	BKE_nurb_order_clamp_v(nu);
	BKE_nurb_knot_calc_v(nu);

	rna_Curve_update_data(bmain, scene, ptr);
}

static void rna_Curve_spline_points_add(ID *id, Nurb *nu, ReportList *reports, int number)
{
	if (nu->type == CU_BEZIER) {
		BKE_report(reports, RPT_ERROR, "Bezier spline cannot have points added");
	}
	else if (number == 0) {
		/* do nothing */
	}
	else {

		BKE_nurb_points_add(nu, number);

		/* update */
		BKE_nurb_knot_calc_u(nu);

		rna_Curve_update_data_id(NULL, NULL, id);
	}
}

static void rna_Curve_spline_bezpoints_add(ID *id, Nurb *nu, ReportList *reports, int number)
{
	if (nu->type != CU_BEZIER) {
		BKE_report(reports, RPT_ERROR, "Only Bezier splines can be added");
	}
	else if (number == 0) {
		/* do nothing */
	}
	else {
		BKE_nurb_bezierPoints_add(nu, number);

		/* update */
		BKE_nurb_knot_calc_u(nu);

		rna_Curve_update_data_id(NULL, NULL, id);
	}
}

static Nurb *rna_Curve_spline_new(Curve *cu, int type)
{
	Nurb *nu = (Nurb *) MEM_callocN(sizeof(Nurb), "spline.new");

	if (type == CU_BEZIER) {
		BezTriple *bezt = (BezTriple *)MEM_callocN(sizeof(BezTriple), "spline.new.bezt");
		bezt->radius = 1.0;
		nu->bezt = bezt;
	}
	else {
		BPoint *bp = (BPoint *)MEM_callocN(sizeof(BPoint), "spline.new.bp");
		bp->radius = 1.0f;
		nu->bp = bp;
	}

	nu->type = type;
	nu->pntsu = 1;
	nu->pntsv = 1;

	nu->orderu = nu->orderv = 4;
	nu->resolu = nu->resolv = 12;
	nu->flag = CU_SMOOTH;

	if ((cu->flag & CU_3D) == 0) {
		nu->flag |= CU_2D;
	}

	BLI_addtail(BKE_curve_nurbs_get(cu), nu);

	return nu;
}

static void rna_Curve_spline_remove(Curve *cu, ReportList *reports, PointerRNA *nu_ptr)
{
	Nurb *nu = nu_ptr->data;
	ListBase *nurbs = BKE_curve_nurbs_get(cu);

	if (BLI_remlink_safe(nurbs, nu) == false) {
		BKE_reportf(reports, RPT_ERROR, "Curve '%s' does not contain spline given", cu->id.name + 2);
		return;
	}

	BKE_nurb_free(nu);
	RNA_POINTER_INVALIDATE(nu_ptr);

	DAG_id_tag_update(&cu->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_GEOM | ND_DATA, NULL);
}

static void rna_Curve_spline_clear(Curve *cu)
{
	ListBase *nurbs = BKE_curve_nurbs_get(cu);

	BKE_nurbList_free(nurbs);

	DAG_id_tag_update(&cu->id, OB_RECALC_DATA);
	WM_main_add_notifier(NC_GEOM | ND_DATA, NULL);
}

static PointerRNA rna_Curve_active_spline_get(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->data;
	Nurb *nu;
	ListBase *nurbs = BKE_curve_nurbs_get(cu);

	/* for curve outside editmode will set to -1,  should be changed to be allowed outside of editmode. */
	nu = BLI_findlink(nurbs, cu->actnu);

	if (nu)
		return rna_pointer_inherit_refine(ptr, &RNA_Spline, nu);

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_Curve_active_spline_set(PointerRNA *ptr, PointerRNA value)
{
	Curve *cu = (Curve *)ptr->data;
	Nurb *nu = value.data;
	ListBase *nubase = BKE_curve_nurbs_get(cu);

	/* -1 is ok for an unset index */
	if (nu == NULL)
		cu->actnu = -1;
	else
		cu->actnu = BLI_findindex(nubase, nu);
}

static char *rna_Curve_spline_path(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	ListBase *nubase = BKE_curve_nurbs_get(cu);
	Nurb *nu = ptr->data;
	int index = BLI_findindex(nubase, nu);

	if (index >= 0)
		return BLI_sprintfN("splines[%d]", index);
	else
		return BLI_strdup("");
}

/* use for both bezier and nurbs */
static char *rna_Curve_spline_point_path(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	Nurb *nu;
	void *point = ptr->data;
	int nu_index, pt_index;

	nu = curve_nurb_from_point(cu, point, &nu_index, &pt_index);

	if (nu) {
		if (nu->type == CU_BEZIER) {
			return BLI_sprintfN("splines[%d].bezier_points[%d]", nu_index, pt_index);
		}
		else {
			return BLI_sprintfN("splines[%d].points[%d]", nu_index, pt_index);
		}
	}
	else {
		return BLI_strdup("");
	}
}


static char *rna_TextBox_path(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	TextBox *tb = ptr->data;
	int index = (int)(tb - cu->tb);

	if (index >= 0 && index < cu->totbox)
		return BLI_sprintfN("text_boxes[%d]", index);
	else
		return BLI_strdup("");
}

static void rna_Curve_splines_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	rna_iterator_listbase_begin(iter, BKE_curve_nurbs_get(cu), NULL);
}

static int rna_Curve_is_editmode_get(PointerRNA *ptr)
{
	Curve *cu = (Curve *)ptr->id.data;
	const short type = BKE_curve_type_get(cu);
	if (type == OB_FONT) {
		return (cu->editfont != NULL);
	}
	else {
		return (cu->editnurb != NULL);
	}
}

#else

static const float tilt_limit = DEG2RADF(21600.0f);

static void rna_def_bpoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "SplinePoint", NULL);
	RNA_def_struct_sdna(srna, "BPoint");
	RNA_def_struct_ui_text(srna, "SplinePoint", "Spline point without handles");

	/* Boolean values */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Select", "Selection status");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 0);
	RNA_def_property_ui_text(prop, "Hide", "Visibility status");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* Vector value */
	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_sdna(prop, NULL, "vec");
	RNA_def_property_ui_text(prop, "Point", "Point coordinates");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "vec[3]");
	RNA_def_property_ui_text(prop, "Weight", "NURBS weight");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* Number values */
	prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "alfa");
	RNA_def_property_range(prop, -tilt_limit, tilt_limit);
	RNA_def_property_ui_range(prop, -tilt_limit, tilt_limit, 10, 3);
	RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3D View");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "weight_softbody", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Bevel Radius", "Radius for beveling");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	RNA_def_struct_path_func(srna, "rna_Curve_spline_point_path");
}

static void rna_def_beztriple(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "BezierSplinePoint", NULL);
	RNA_def_struct_sdna(srna, "BezTriple");
	RNA_def_struct_ui_text(srna, "Bezier Curve Point", "Bezier curve point with two handles");

	/* Boolean values */
	prop = RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f1", 0);
	RNA_def_property_ui_text(prop, "Handle 1 selected", "Handle 1 selection status");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f3", 0);
	RNA_def_property_ui_text(prop, "Handle 2 selected", "Handle 2 selection status");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "select_control_point", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "f2", 0);
	RNA_def_property_ui_text(prop, "Control Point selected", "Control point selection status");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 0);
	RNA_def_property_ui_text(prop, "Hide", "Visibility status");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* Enums */
	prop = RNA_def_property(srna, "handle_left_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h1");
	RNA_def_property_enum_items(prop, beztriple_handle_type_items);
	RNA_def_property_ui_text(prop, "Handle 1 Type", "Handle types");
	RNA_def_property_update(prop, 0, "rna_Curve_update_points");

	prop = RNA_def_property(srna, "handle_right_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "h2");
	RNA_def_property_enum_items(prop, beztriple_handle_type_items);
	RNA_def_property_ui_text(prop, "Handle 2 Type", "Handle types");
	RNA_def_property_update(prop, 0, "rna_Curve_update_points");

	/* Vector values */
	prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_handle1_get", "rna_BezTriple_handle1_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 1", "Coordinates of the first handle");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_Curve_update_points");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_ctrlpoint_get", "rna_BezTriple_ctrlpoint_set", NULL);
	RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_Curve_update_points");

	prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_BezTriple_handle2_get", "rna_BezTriple_handle2_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 2", "Coordinates of the second handle");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_update(prop, 0, "rna_Curve_update_points");

	/* Number values */
	prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_ANGLE);
	RNA_def_property_float_sdna(prop, NULL, "alfa");
	RNA_def_property_range(prop, -tilt_limit, tilt_limit);
	RNA_def_property_ui_range(prop, -tilt_limit, tilt_limit, 10, 3);
	RNA_def_property_ui_text(prop, "Tilt", "Tilt in 3D View");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "weight_softbody", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "weight");
	RNA_def_property_range(prop, 0.01f, 100.0f);
	RNA_def_property_ui_text(prop, "Weight", "Softbody goal weight");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radius");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_text(prop, "Bevel Radius", "Radius for beveling");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	RNA_def_struct_path_func(srna, "rna_Curve_spline_point_path");
}

static void rna_def_path(BlenderRNA *UNUSED(brna), StructRNA *srna)
{
	PropertyRNA *prop;
	
	/* number values */
	prop = RNA_def_property(srna, "path_duration", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pathlen");
	RNA_def_property_range(prop, 1, MAXFRAME);
	RNA_def_property_ui_text(prop, "Path Length",
	                         "The number of frames that are needed to traverse the path, "
	                         "defining the maximum value for the 'Evaluation Time' setting");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	/* flags */
	prop = RNA_def_property(srna, "use_path", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_PATH);
	RNA_def_property_ui_text(prop, "Path", "Enable the curve to become a translation path");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "use_path_follow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_FOLLOW);
	RNA_def_property_ui_text(prop, "Follow", "Make curve path children to rotate along the path");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "use_stretch", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_STRETCH);
	RNA_def_property_ui_text(prop, "Stretch", "Option for curve-deform: "
	                         "make deformed child to stretch along entire path");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "use_deform_bounds", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", CU_DEFORM_BOUNDS_OFF);
	RNA_def_property_ui_text(prop, "Bounds Clamp", "Option for curve-deform: "
	                         "Use the mesh bounds to clamp the deformation");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "use_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_PATH_RADIUS);
	RNA_def_property_ui_text(prop, "Radius", "Option for paths and curve-deform: "
	                         "apply the curve radius with path following it and deforming");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
}

static void rna_def_nurbs(BlenderRNA *UNUSED(brna), StructRNA *srna)
{
	PropertyRNA *prop;
	
	/* flags */
	prop = RNA_def_property(srna, "use_uv_as_generated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_UV_ORCO);
	RNA_def_property_ui_text(prop, "Use UV for Mapping", "Uses the UV values as Generated textured coordinates");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
}

static void rna_def_font(BlenderRNA *UNUSED(brna), StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem prop_align_items[] = {
		{CU_ALIGN_X_LEFT, "LEFT", 0, "Left", "Align text to the left"},
		{CU_ALIGN_X_MIDDLE, "CENTER", 0, "Center", "Center text"},
		{CU_ALIGN_X_RIGHT, "RIGHT", 0, "Right", "Align text to the right"},
		{CU_ALIGN_X_JUSTIFY, "JUSTIFY", 0, "Justify", "Align to the left and the right"},
		{CU_ALIGN_X_FLUSH, "FLUSH", 0, "Flush", "Align to the left and the right, with equal character spacing"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem prop_align_y_items[] = {
		{CU_ALIGN_Y_TOP_BASELINE, "TOP_BASELINE", 0, "Top Base-Line", "Align to top but use the base-line of the text"},
		{CU_ALIGN_Y_TOP, "TOP", 0, "Top", "Align text to the top"},
		{CU_ALIGN_Y_CENTER, "CENTER", 0, "Center", "Align text to the middle"},
		{CU_ALIGN_Y_BOTTOM, "BOTTOM", 0, "Bottom", "Align text to the bottom"},
		{0, NULL, 0, NULL, NULL}
	};

	/* Enums */
	prop = RNA_def_property(srna, "align_x", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "spacemode");
	RNA_def_property_enum_items(prop, prop_align_items);
	RNA_def_property_ui_text(prop, "Text Horizontal Align", "Text horizontal align from the object center");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "align_y", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "align_y");
	RNA_def_property_enum_items(prop, prop_align_y_items);
	RNA_def_property_ui_text(prop, "Text Vertical Align", "Text vertical align from the object center");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* number values */
	prop = RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fsize");
	RNA_def_property_range(prop, 0.0001f, 10000.0f);
	RNA_def_property_ui_range(prop, 0.01, 10, 1, 3);
	RNA_def_property_ui_text(prop, "Font size", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "small_caps_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "smallcaps_scale");
	RNA_def_property_ui_range(prop, 0, 1.0, 1, 2);
	RNA_def_property_ui_text(prop, "Small Caps", "Scale of small capitals");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "space_line", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "linedist");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Distance between lines of text", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "space_word", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wordspace");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Spacing between words", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "space_character", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spacing");
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Global spacing between characters", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "shear", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shear");
	RNA_def_property_range(prop, -1.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Shear", "Italic angle of the characters");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "xof");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -50.0f, 50.0f, 10, 3);
	RNA_def_property_ui_text(prop, "X Offset", "Horizontal offset from the object origin");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "yof");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -50.0f, 50.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Y Offset", "Vertical offset from the object origin");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "underline_position", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ulpos");
	RNA_def_property_range(prop, -0.2f, 0.8f);
	RNA_def_property_ui_text(prop, "Underline Position", "Vertical position of underline");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "underline_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ulheight");
	RNA_def_property_range(prop, 0.0f, 0.8f);
	RNA_def_property_ui_text(prop, "Underline Thickness", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "text_boxes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "tb", "totbox");
	RNA_def_property_struct_type(prop, "TextBox");
	RNA_def_property_ui_text(prop, "Textboxes", "");

	prop = RNA_def_property(srna, "active_textbox", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "actbox");
	RNA_def_property_ui_text(prop, "The active text box", "");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_Curve_active_textbox_index_range");
	
	/* strings */
	prop = RNA_def_property(srna, "family", PROP_STRING, PROP_NONE);
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
	RNA_def_property_ui_text(prop, "Object Font",
	                         "Use Objects as font characters (give font objects a common name "
	                         "followed by the character they represent, eg. 'family-a', 'family-b', etc, "
	                         "set this setting to 'family-', and turn on Vertex Duplication)");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "body", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "str");
	RNA_def_property_ui_text(prop, "Body Text", "Content of this text object");
	RNA_def_property_string_funcs(prop, "rna_Curve_body_get", "rna_Curve_body_length", "rna_Curve_body_set");
	RNA_def_property_string_maxlength(prop, 8192); /* note that originally str did not have a limit! */
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "body_format", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "strinfo", "len_wchar");
	RNA_def_property_struct_type(prop, "TextCharacterFormat");
	RNA_def_property_ui_text(prop, "Character Info", "Stores the style of each character");
	
	/* pointers */
	prop = RNA_def_property(srna, "follow_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "textoncurve");
	RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Curve_otherObject_poll");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Text on Curve", "Curve deforming text object");
	RNA_def_property_update(prop, 0, "rna_Curve_update_deps");
	
	prop = RNA_def_property(srna, "font", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vfont");
	RNA_def_property_ui_text(prop, "Font", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "font_bold", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vfontb");
	RNA_def_property_ui_text(prop, "Font Bold", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "font_italic", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vfonti");
	RNA_def_property_ui_text(prop, "Font Italic", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "font_bold_italic", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "vfontbi");
	RNA_def_property_ui_text(prop, "Font Bold Italic", "");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "edit_format", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curinfo");
	RNA_def_property_ui_text(prop, "Edit Format", "Editing settings character formatting");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* flags */
	prop = RNA_def_property(srna, "use_fast_edit", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_FAST);
	RNA_def_property_ui_text(prop, "Fast Editing", "Don't fill polygons while editing");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
}

static void rna_def_textbox(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TextBox", NULL);
	RNA_def_struct_ui_text(srna, "Text Box", "Text bounding box for layout");
	
	/* number values */
	prop = RNA_def_property(srna, "x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -50.0f, 50.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Textbox X Offset", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "y");
	RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
	RNA_def_property_ui_range(prop, -50.0f, 50.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Textbox Y Offset", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "width", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "w");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 50.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Textbox Width", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "h");
	RNA_def_property_range(prop, 0.0f, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.0f, 50.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Textbox Height", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	RNA_def_struct_path_func(srna, "rna_TextBox_path");
}

static void rna_def_charinfo(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "TextCharacterFormat", NULL);
	RNA_def_struct_sdna(srna, "CharInfo");
	RNA_def_struct_ui_text(srna, "Text Character Format", "Text character formatting settings");
	
	/* flags */
	prop = RNA_def_property(srna, "use_bold", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_CHINFO_BOLD);
	RNA_def_property_ui_text(prop, "Bold", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "use_italic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_CHINFO_ITALIC);
	RNA_def_property_ui_text(prop, "Italic", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "use_underline", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_CHINFO_UNDERLINE);
	RNA_def_property_ui_text(prop, "Underline", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	/* probably there is no reason to expose this */
#if 0
	prop = RNA_def_property(srna, "wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_CHINFO_WRAP);
	RNA_def_property_ui_text(prop, "Wrap", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data"); */
#endif

	prop = RNA_def_property(srna, "use_small_caps", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_CHINFO_SMALLCAPS);
	RNA_def_property_ui_text(prop, "Small Caps", "");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
	// RNA_def_property_int_sdna(prop, NULL, "mat_nr");
	RNA_def_property_ui_text(prop, "Material Index", "");
	RNA_def_property_int_funcs(prop, "rna_ChariInfo_material_index_get", "rna_ChariInfo_material_index_set", "rna_Curve_material_index_range");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
}

static void rna_def_surface(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, "SurfaceCurve", "Curve");
	RNA_def_struct_sdna(srna, "Curve");
	RNA_def_struct_ui_text(srna, "Surface Curve", "Curve data-block used for storing surfaces");
	RNA_def_struct_ui_icon(srna, ICON_SURFACE_DATA);

	rna_def_nurbs(brna, srna);
}

static void rna_def_text(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna = RNA_def_struct(brna, "TextCurve", "Curve");
	RNA_def_struct_sdna(srna, "Curve");
	RNA_def_struct_ui_text(srna, "Text Curve", "Curve data-block used for storing text");
	RNA_def_struct_ui_icon(srna, ICON_FONT_DATA);

	rna_def_font(brna, srna);
	rna_def_nurbs(brna, srna);
}


/* curve.splines[0].points */
static void rna_def_curve_spline_points(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	/*PropertyRNA *prop; */

	FunctionRNA *func;
	/*PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "SplinePoints");
	srna = RNA_def_struct(brna, "SplinePoints", NULL);
	RNA_def_struct_sdna(srna, "Nurb");
	RNA_def_struct_ui_text(srna, "Spline Points", "Collection of spline points");

	func = RNA_def_function(srna, "add", "rna_Curve_spline_points_add");
	RNA_def_function_ui_description(func, "Add a number of points to this spline");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 1, 0, INT_MAX, "Number", "Number of points to add to the spline", 0, INT_MAX);

#if 0
	func = RNA_def_function(srna, "remove", "rna_Curve_spline_remove");
	RNA_def_function_ui_description(func, "Remove a spline from a curve");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "spline", "Spline", "", "The spline to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
#endif
}

static void rna_def_curve_spline_bezpoints(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	/*PropertyRNA *prop; */

	FunctionRNA *func;
	/*PropertyRNA *parm; */

	RNA_def_property_srna(cprop, "SplineBezierPoints");
	srna = RNA_def_struct(brna, "SplineBezierPoints", NULL);
	RNA_def_struct_sdna(srna, "Nurb");
	RNA_def_struct_ui_text(srna, "Spline Bezier Points", "Collection of spline Bezier points");

	func = RNA_def_function(srna, "add", "rna_Curve_spline_bezpoints_add");
	RNA_def_function_ui_description(func, "Add a number of points to this spline");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
	RNA_def_int(func, "count", 1, 0, INT_MAX, "Number", "Number of points to add to the spline", 0, INT_MAX);

#if 0
	func = RNA_def_function(srna, "remove", "rna_Curve_spline_remove");
	RNA_def_function_ui_description(func, "Remove a spline from a curve");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "spline", "Spline", "", "The spline to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
#endif
}

/* curve.splines */
static void rna_def_curve_splines(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "CurveSplines");
	srna = RNA_def_struct(brna, "CurveSplines", NULL);
	RNA_def_struct_sdna(srna, "Curve");
	RNA_def_struct_ui_text(srna, "Curve Splines", "Collection of curve splines");

	func = RNA_def_function(srna, "new", "rna_Curve_spline_new");
	RNA_def_function_ui_description(func, "Add a new spline to the curve");
	parm = RNA_def_enum(func, "type", curve_type_items, CU_POLY, "", "type for the new spline");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "spline", "Spline", "", "The newly created spline");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Curve_spline_remove");
	RNA_def_function_ui_description(func, "Remove a spline from a curve");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "spline", "Spline", "", "The spline to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	func = RNA_def_function(srna, "clear", "rna_Curve_spline_clear");
	RNA_def_function_ui_description(func, "Remove all splines from a curve");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Spline");
	RNA_def_property_pointer_funcs(prop, "rna_Curve_active_spline_get", "rna_Curve_active_spline_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Spline", "Active curve spline");
}


static void rna_def_curve(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem curve_twist_mode_items[] = {
		{CU_TWIST_Z_UP, "Z_UP", 0, "Z-Up", "Use Z-Up axis to calculate the curve twist at each point"},
		{CU_TWIST_MINIMUM, "MINIMUM", 0, "Minimum", "Use the least twist over the entire curve"},
		{CU_TWIST_TANGENT, "TANGENT", 0, "Tangent", "Use the tangent to calculate twist"},
		{0, NULL, 0, NULL, NULL}
	};

	static const EnumPropertyItem curve_axis_items[] = {
		{0, "2D", 0, "2D", "Clamp the Z axis of the curve"},
		{CU_3D, "3D", 0, "3D",
		 "Allow editing on the Z axis of this curve, also allows tilt and curve radius to be used"},
		{0, NULL, 0, NULL, NULL}
	};

	static EnumPropertyItem bevfac_mapping_items[] = {
		{CU_BEVFAC_MAP_RESOLU, "RESOLUTION", 0, "Resolution", "Map the bevel factor to the number of subdivisions of a spline (U resolution)"},
		{CU_BEVFAC_MAP_SEGMENT, "SEGMENTS", 0, "Segments", "Map the bevel factor to the length of a segment and to the number of subdivisions of a segment"},
		{CU_BEVFAC_MAP_SPLINE, "SPLINE", 0, "Spline", "Map the bevel factor to the length of a spline"},
		{0, NULL, 0, NULL, NULL}
	};

	srna = RNA_def_struct(brna, "Curve", "ID");
	RNA_def_struct_ui_text(srna, "Curve", "Curve data-block storing curves, splines and NURBS");
	RNA_def_struct_ui_icon(srna, ICON_CURVE_DATA);
	RNA_def_struct_refine_func(srna, "rna_Curve_refine");


	prop = RNA_def_property(srna, "shape_keys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "key");
	RNA_def_property_ui_text(prop, "Shape Keys", "");


	prop = RNA_def_property(srna, "splines", PROP_COLLECTION, PROP_NONE);
#if 0
	RNA_def_property_collection_sdna(prop, NULL, "nurb", NULL);
#else
	/* this way we get editmode nurbs too, keyframe in editmode */
	RNA_def_property_collection_funcs(prop, "rna_Curve_splines_begin", "rna_iterator_listbase_next",
	                                  "rna_iterator_listbase_end", "rna_iterator_listbase_get",
	                                  NULL, NULL, NULL, NULL);
#endif
	RNA_def_property_struct_type(prop, "Spline");
	RNA_def_property_ui_text(prop, "Splines", "Collection of splines in this curve data object");
	rna_def_curve_splines(brna, prop);

	prop = RNA_def_property(srna, "show_handles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "drawflag", CU_HIDE_HANDLES);
	RNA_def_property_ui_text(prop, "Draw Handles", "Display Bezier handles in editmode");
	RNA_def_property_update(prop, NC_GEOM | ND_DATA, NULL);

	prop = RNA_def_property(srna, "show_normal_face", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "drawflag", CU_HIDE_NORMALS);
	RNA_def_property_ui_text(prop, "Draw Normals", "Display 3D curve normals in editmode");
	RNA_def_property_update(prop, NC_GEOM | ND_DATA, NULL);

	rna_def_path(brna, srna);
	
	/* Number values */
	prop = RNA_def_property(srna, "bevel_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "bevresol");
	RNA_def_property_range(prop, 0, 32);
	RNA_def_property_ui_range(prop, 0, 32, 1.0, -1);
	RNA_def_property_ui_text(prop, "Bevel Resolution",
	                         "Bevel resolution when depth is non-zero and no specific bevel object has been defined");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE | PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "width");
	RNA_def_property_ui_range(prop, -1.0, 1.0, 0.1, 3);
	RNA_def_property_float_funcs(prop, "rna_Curve_offset_get", "rna_Curve_offset_set", NULL);
	RNA_def_property_ui_text(prop, "Offset", "Offset the curve to adjust the width of a text");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "extrude", PROP_FLOAT, PROP_NONE | PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "ext1");
	RNA_def_property_ui_range(prop, 0, 100.0, 0.1, 3);
	RNA_def_property_range(prop, 0.0, FLT_MAX);
	RNA_def_property_ui_text(prop, "Extrude", "Amount of curve extrusion when not using a bevel object");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "bevel_depth", PROP_FLOAT, PROP_NONE | PROP_UNIT_LENGTH);
	RNA_def_property_float_sdna(prop, NULL, "ext2");
	RNA_def_property_ui_range(prop, 0, 100.0, 0.1, 3);
	RNA_def_property_ui_text(prop, "Bevel Depth", "Bevel depth when not using a bevel object");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "resolution_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolu");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_range(prop, 1, 64, 1, -1);
	RNA_def_property_ui_text(prop, "Resolution U", "Surface resolution in U direction");
	RNA_def_property_update(prop, 0, "rna_Curve_resolution_u_update_data");
	
	prop = RNA_def_property(srna, "resolution_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv");
	RNA_def_property_ui_range(prop, 1, 64, 1, -1);
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_text(prop, "Resolution V", "Surface resolution in V direction");
	RNA_def_property_update(prop, 0, "rna_Curve_resolution_v_update_data");
	
	prop = RNA_def_property(srna, "render_resolution_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolu_ren");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_range(prop, 0, 64, 1, -1);
	RNA_def_property_ui_text(prop, "Render Resolution U",
	                         "Surface resolution in U direction used while rendering (zero uses preview resolution)");
	
	prop = RNA_def_property(srna, "render_resolution_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv_ren");
	RNA_def_property_ui_range(prop, 0, 64, 1, -1);
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Render Resolution V",
	                         "Surface resolution in V direction used while rendering (zero uses preview resolution)");
	
	
	prop = RNA_def_property(srna, "eval_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ctime");
	RNA_def_property_ui_text(prop, "Evaluation Time",
	                         "Parametric position along the length of the curve that Objects 'following' it should be "
	                         "at (position is evaluated by dividing by the 'Path Length' value)");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	/* pointers */
	prop = RNA_def_property(srna, "bevel_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "bevobj");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bevel Object", "Curve object name that defines the bevel shape");
	RNA_def_property_update(prop, 0, "rna_Curve_update_deps");
	RNA_def_property_pointer_funcs(prop, "rna_Curve_bevelObject_get", "rna_Curve_bevelObject_set", NULL,
	                               "rna_Curve_otherObject_poll");

	prop = RNA_def_property(srna, "taper_object", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_pointer_sdna(prop, NULL, "taperobj");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Taper Object", "Curve object name that defines the taper (width)");
	RNA_def_property_update(prop, 0, "rna_Curve_update_deps");
	RNA_def_property_pointer_funcs(prop, "rna_Curve_taperObject_get", "rna_Curve_taperObject_set", NULL,
	                               "rna_Curve_otherObject_poll");

	/* Flags */

	prop = RNA_def_property(srna, "dimensions", PROP_ENUM, PROP_NONE); /* as an enum */
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, curve_axis_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Curve_dimension_set", NULL);
	RNA_def_property_ui_text(prop, "Dimensions", "Select 2D or 3D curve type");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "fill_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, curve3d_fill_mode_items);
	RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Curve_fill_mode_itemf");
	RNA_def_property_ui_text(prop, "Fill Mode", "Mode of filling curve");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "twist_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "twist_mode");
	RNA_def_property_enum_items(prop, curve_twist_mode_items);
	RNA_def_property_ui_text(prop, "Twist Method", "The type of tilt calculation for 3D Curves");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "bevel_factor_mapping_start", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bevfac1_mapping");
	RNA_def_property_enum_items(prop, bevfac_mapping_items);
	RNA_def_property_ui_text(prop, "Start Mapping Type", "Determines how the start bevel factor is mapped to a spline");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "bevel_factor_mapping_end", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "bevfac2_mapping");
	RNA_def_property_enum_items(prop, bevfac_mapping_items);
	RNA_def_property_ui_text(prop, "End Mapping Type", "Determines how the end bevel factor is mapped to a spline");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* XXX - would be nice to have a better way to do this, only add for testing. */
	prop = RNA_def_property(srna, "twist_smooth", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "twist_smooth");
	RNA_def_property_ui_range(prop, 0, 100.0, 1, 2);
	RNA_def_property_ui_text(prop, "Twist Smooth", "Smoothing iteration for tangents");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "use_fill_deform", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_DEFORM_FILL);
	RNA_def_property_ui_text(prop, "Fill Deformed", "Fill curve after applying shape keys and all modifiers");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "use_fill_caps", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_FILL_CAPS);
	RNA_def_property_ui_text(prop, "Fill Caps", "Fill caps for beveled curves");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "use_map_taper", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_MAP_TAPER);
	RNA_def_property_ui_text(prop, "Map Taper", "Map effect of taper object on actually beveled curve");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	/* texture space */
	prop = RNA_def_property(srna, "use_auto_texspace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "texflag", CU_AUTOSPACE);
	RNA_def_property_ui_text(prop, "Auto Texture Space",
	                         "Adjust active object's texture space automatically when transforming object");
	RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Curve_texspace_set");

	prop = RNA_def_property(srna, "texspace_location", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Texture Space Location", "Texture space location");
	RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
	RNA_def_property_editable_func(prop, "rna_Curve_texspace_editable");
	RNA_def_property_float_funcs(prop, "rna_Curve_texspace_loc_get", "rna_Curve_texspace_loc_set", NULL);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "texspace_size", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_array(prop, 3);
	RNA_def_property_flag(prop, PROP_PROPORTIONAL);
	RNA_def_property_ui_text(prop, "Texture Space Size", "Texture space size");
	RNA_def_property_editable_func(prop, "rna_Curve_texspace_editable");
	RNA_def_property_float_funcs(prop, "rna_Curve_texspace_size_get", "rna_Curve_texspace_size_set", NULL);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	/* not supported yet */
#if 0
	prop = RNA_def_property(srna, "texspace_rot", PROP_FLOAT, PROP_EULER);
	RNA_def_property_float(prop, NULL, "rot");
	RNA_def_property_ui_text(prop, "Texture Space Rotation", "Texture space rotation");
	RNA_def_property_editable_func(prop, texspace_editable);
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
#endif
	
	prop = RNA_def_property(srna, "use_uv_as_generated", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_UV_ORCO);
	RNA_def_property_ui_text(prop, "Use UV for mapping", "Uses the UV values as Generated textured coordinates");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	/* materials */
	prop = RNA_def_property(srna, "materials", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
	RNA_def_property_struct_type(prop, "Material");
	RNA_def_property_ui_text(prop, "Materials", "");
	RNA_def_property_srna(prop, "IDMaterials"); /* see rna_ID.c */
	RNA_def_property_collection_funcs(prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "rna_IDMaterials_assign_int");

	prop = RNA_def_property(srna, "bevel_factor_start", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bevfac1");
	RNA_def_property_range(prop, 0, 1.0);
	RNA_def_property_ui_text(prop, "Start Bevel Factor", "Factor that defines from where beveling of spline happens (0=from the very beginning, 1=from the very end)");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "bevel_factor_end", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "bevfac2");
	RNA_def_property_range(prop, 0, 1.0);
	RNA_def_property_ui_text(prop, "End Bevel Factor", "Factor that defines to where beveling of spline happens (0=to the very beginning, 1=to the very end)");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "is_editmode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_Curve_is_editmode_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Editmode", "True when used in editmode");

	rna_def_animdata_common(srna);

	RNA_api_curve(srna);
}

static void rna_def_curve_nurb(BlenderRNA *brna)
{
	static EnumPropertyItem spline_interpolation_items[] = {
		{KEY_LINEAR, "LINEAR", 0, "Linear", ""},
		{KEY_CARDINAL, "CARDINAL", 0, "Cardinal", ""},
		{KEY_BSPLINE, "BSPLINE", 0, "BSpline", ""},
		{KEY_CU_EASE, "EASE", 0, "Ease", ""}, /* todo, define somewhere, not one of BEZT_IPO_* */
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Spline", NULL);
	RNA_def_struct_sdna(srna, "Nurb");
	RNA_def_struct_ui_text(srna, "Spline",
	                       "Element of a curve, either NURBS, Bezier or Polyline or a character with text objects");

	prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "bp", NULL);
	RNA_def_property_struct_type(prop, "SplinePoint");
	RNA_def_property_collection_funcs(prop, "rna_BPoint_array_begin", "rna_iterator_array_next",
	                                  "rna_iterator_array_end", "rna_iterator_array_get", "rna_Nurb_length",
	                                  NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Points", "Collection of points that make up this poly or nurbs spline");
	rna_def_curve_spline_points(brna, prop);

	prop = RNA_def_property(srna, "bezier_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "BezierSplinePoint");
	RNA_def_property_collection_sdna(prop, NULL, "bezt", "pntsu");
	RNA_def_property_ui_text(prop, "Bezier Points", "Collection of points for Bezier curves only");
	rna_def_curve_spline_bezpoints(brna, prop);

	
	prop = RNA_def_property(srna, "tilt_interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "tilt_interp");
	RNA_def_property_enum_items(prop, spline_interpolation_items);
	RNA_def_property_ui_text(prop, "Tilt Interpolation", "The type of tilt interpolation for 3D, Bezier curves");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "radius_interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "radius_interp");
	RNA_def_property_enum_items(prop, spline_interpolation_items);
	RNA_def_property_ui_text(prop, "Radius Interpolation", "The type of radius interpolation for Bezier curves");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, curve_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_Nurb_type_set", NULL);
	RNA_def_property_ui_text(prop, "Type", "The interpolation type for this curve element");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "point_count_u", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* editing this needs knot recalc*/
	RNA_def_property_int_sdna(prop, NULL, "pntsu");
	RNA_def_property_ui_text(prop, "Points U", "Total number points for the curve or surface in the U direction");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "point_count_v", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* editing this needs knot recalc*/
	RNA_def_property_int_sdna(prop, NULL, "pntsv");
	RNA_def_property_ui_text(prop, "Points V", "Total number points for the surface on the V direction");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");


	prop = RNA_def_property(srna, "order_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "orderu");
	RNA_def_property_range(prop, 2, 6);
	RNA_def_property_ui_text(prop, "Order U",
	                         "NURBS order in the U direction (for splines and surfaces, higher values "
	                         "let points influence a greater area)");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_knot_u");

	prop = RNA_def_property(srna, "order_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "orderv");
	RNA_def_property_range(prop, 2, 6);
	RNA_def_property_ui_text(prop, "Order V",
	                         "NURBS order in the V direction (for surfaces only, higher values "
	                         "let points influence a greater area)");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_knot_v");


	prop = RNA_def_property(srna, "resolution_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolu");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_range(prop, 1, 64, 1, -1);
	RNA_def_property_ui_text(prop, "Resolution U", "Curve or Surface subdivisions per segment");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "resolution_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "resolv");
	RNA_def_property_range(prop, 1, 1024);
	RNA_def_property_ui_range(prop, 1, 64, 1, -1);
	RNA_def_property_ui_text(prop, "Resolution V", "Surface subdivisions per segment");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "use_cyclic_u", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flagu", CU_NURB_CYCLIC);
	RNA_def_property_ui_text(prop, "Cyclic U", "Make this curve or surface a closed loop in the U direction");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_cyclic_u");

	prop = RNA_def_property(srna, "use_cyclic_v", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flagv", CU_NURB_CYCLIC);
	RNA_def_property_ui_text(prop, "Cyclic V", "Make this surface a closed loop in the V direction");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_cyclic_v");


	/* Note, endpoint and bezier flags should never be on at the same time! */
	prop = RNA_def_property(srna, "use_endpoint_u", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flagu", CU_NURB_ENDPOINT);
	RNA_def_property_ui_text(prop, "Endpoint U",
	                         "Make this nurbs curve or surface meet the endpoints in the U direction "
	                         "(Cyclic U must be disabled)");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_knot_u");

	prop = RNA_def_property(srna, "use_endpoint_v", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flagv", CU_NURB_ENDPOINT);
	RNA_def_property_ui_text(prop, "Endpoint V",
	                         "Make this nurbs surface meet the endpoints in the V direction "
	                         "(Cyclic V must be disabled)");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_knot_v");

	prop = RNA_def_property(srna, "use_bezier_u", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flagu", CU_NURB_BEZIER);
	RNA_def_property_ui_text(prop, "Bezier U",
	                         "Make this nurbs curve or surface act like a Bezier spline in the U direction "
	                         "(Order U must be 3 or 4, Cyclic U must be disabled)");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_knot_u");

	prop = RNA_def_property(srna, "use_bezier_v", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flagv", CU_NURB_BEZIER);
	RNA_def_property_ui_text(prop, "Bezier V",
	                         "Make this nurbs surface act like a Bezier spline in the V direction "
	                         "(Order V must be 3 or 4, Cyclic V must be disabled)");
	RNA_def_property_update(prop, 0, "rna_Nurb_update_knot_v");


	prop = RNA_def_property(srna, "use_smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CU_SMOOTH);
	RNA_def_property_ui_text(prop, "Smooth", "Smooth the normals of the surface or beveled curve");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "hide", 1);
	RNA_def_property_ui_text(prop, "Hide", "Hide this curve in Edit mode");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	prop = RNA_def_property(srna, "material_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "mat_nr");
	RNA_def_property_ui_text(prop, "Material Index", "");
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_Curve_material_index_range");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");
	
	prop = RNA_def_property(srna, "character_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "charidx");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* editing this needs knot recalc*/
	RNA_def_property_ui_text(prop, "Character Index",
	                         "Location of this character in the text data (only for text curves)");
	RNA_def_property_update(prop, 0, "rna_Curve_update_data");

	RNA_def_struct_path_func(srna, "rna_Curve_spline_path");
}

void RNA_def_curve(BlenderRNA *brna)
{
	rna_def_curve(brna);
	rna_def_surface(brna);
	rna_def_text(brna);
	rna_def_textbox(brna);
	rna_def_charinfo(brna);
	rna_def_bpoint(brna);
	rna_def_beztriple(brna);
	rna_def_curve_nurb(brna);
}

#endif
