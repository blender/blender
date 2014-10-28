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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/curve/editcurve_add.c
 *  \ingroup edcurve
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_library.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_view3d.h"
#include "ED_curve.h"

#include "curve_intern.h"

static const float nurbcircle[8][2] = {
	{0.0, -1.0}, {-1.0, -1.0}, {-1.0, 0.0}, {-1.0,  1.0},
	{0.0,  1.0}, { 1.0,  1.0}, { 1.0, 0.0}, { 1.0, -1.0}
};

/************ add primitive, used by object/ module ****************/

static const char *get_curve_defname(int type)
{
	int stype = type & CU_PRIMITIVE;

	if ((type & CU_TYPE) == CU_BEZIER) {
		switch (stype) {
			case CU_PRIM_CURVE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "BezierCurve");
			case CU_PRIM_CIRCLE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "BezierCircle");
			case CU_PRIM_PATH: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "CurvePath");
			default:
				return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "Curve");
		}
	}
	else {
		switch (stype) {
			case CU_PRIM_CURVE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "NurbsCurve");
			case CU_PRIM_CIRCLE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "NurbsCircle");
			case CU_PRIM_PATH: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "NurbsPath");
			default:
				return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "Curve");
		}
	}
}

static const char *get_surf_defname(int type)
{
	int stype = type & CU_PRIMITIVE;

	switch (stype) {
		case CU_PRIM_CURVE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "SurfCurve");
		case CU_PRIM_CIRCLE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "SurfCircle");
		case CU_PRIM_PATCH: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "SurfPatch");
		case CU_PRIM_SPHERE: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "SurfSphere");
		case CU_PRIM_DONUT: return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "SurfTorus");
		default:
			return CTX_DATA_(BLF_I18NCONTEXT_ID_CURVE, "Surface");
	}
}


Nurb *add_nurbs_primitive(bContext *C, Object *obedit, float mat[4][4], int type, int newob)
{
	static int xzproj = 0;   /* this function calls itself... */
	ListBase *editnurb = object_editcurve_get(obedit);
	RegionView3D *rv3d = ED_view3d_context_rv3d(C);
	Nurb *nu = NULL;
	BezTriple *bezt;
	BPoint *bp;
	Curve *cu = (Curve *)obedit->data;
	float vec[3], zvec[3] = {0.0f, 0.0f, 1.0f};
	float umat[4][4], viewmat[4][4];
	float fac;
	int a, b;
	const float grid = 1.0f;
	const int cutype = (type & CU_TYPE); // poly, bezier, nurbs, etc
	const int stype = (type & CU_PRIMITIVE);
	const int force_3d = ((Curve *)obedit->data)->flag & CU_3D; /* could be adding to an existing 3D curve */

	unit_m4(umat);
	unit_m4(viewmat);

	if (rv3d) {
		copy_m4_m4(viewmat, rv3d->viewmat);
		copy_v3_v3(zvec, rv3d->viewinv[2]);
	}

	BKE_nurbList_flag_set(editnurb, 0);

	/* these types call this function to return a Nurb */
	if (stype != CU_PRIM_TUBE && stype != CU_PRIM_DONUT) {
		nu = (Nurb *)MEM_callocN(sizeof(Nurb), "addNurbprim");
		nu->type = cutype;
		nu->resolu = cu->resolu;
		nu->resolv = cu->resolv;
	}

	switch (stype) {
		case CU_PRIM_CURVE: /* curve */
			nu->resolu = cu->resolu;
			if (cutype == CU_BEZIER) {
				if (!force_3d) nu->flag |= CU_2D;
				nu->pntsu = 2;
				nu->bezt = (BezTriple *)MEM_callocN(2 * sizeof(BezTriple), "addNurbprim1");
				bezt = nu->bezt;
				bezt->h1 = bezt->h2 = HD_ALIGN;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->radius = 1.0;

				bezt->vec[1][0] += -grid;
				bezt->vec[0][0] += -1.5f * grid;
				bezt->vec[0][1] += -0.5f * grid;
				bezt->vec[2][0] += -0.5f * grid;
				bezt->vec[2][1] +=  0.5f * grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);

				bezt++;
				bezt->h1 = bezt->h2 = HD_ALIGN;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->radius = bezt->weight = 1.0;

				bezt->vec[0][0] = 0;
				bezt->vec[0][1] = 0;
				bezt->vec[1][0] = grid;
				bezt->vec[1][1] = 0;
				bezt->vec[2][0] = grid * 2;
				bezt->vec[2][1] = 0;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);

				BKE_nurb_handles_calc(nu);
			}
			else {

				nu->pntsu = 4;
				nu->pntsv = 1;
				nu->orderu = 4;
				nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * 4, "addNurbprim3");

				bp = nu->bp;
				for (a = 0; a < 4; a++, bp++) {
					bp->vec[3] = 1.0;
					bp->f1 = SELECT;
					bp->radius = bp->weight = 1.0;
				}

				bp = nu->bp;
				bp->vec[0] += -1.5f * grid;
				bp++;
				bp->vec[0] += -grid;
				bp->vec[1] +=  grid;
				bp++;
				bp->vec[0] += grid;
				bp->vec[1] += grid;
				bp++;
				bp->vec[0] += 1.5f * grid;

				bp = nu->bp;
				for (a = 0; a < 4; a++, bp++) mul_m4_v3(mat, bp->vec);

				if (cutype == CU_NURBS) {
					nu->knotsu = NULL; /* nurbs_knot_calc_u allocates */
					BKE_nurb_knot_calc_u(nu);
				}

			}
			break;
		case CU_PRIM_PATH: /* 5 point path */
			nu->pntsu = 5;
			nu->pntsv = 1;
			nu->orderu = 5;
			nu->flagu = CU_NURB_ENDPOINT; /* endpoint */
			nu->resolu = cu->resolu;
			nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * 5, "addNurbprim3");

			bp = nu->bp;
			for (a = 0; a < 5; a++, bp++) {
				bp->vec[3] = 1.0;
				bp->f1 = SELECT;
				bp->radius = bp->weight = 1.0;
			}

			bp = nu->bp;
			bp->vec[0] += -2.0f * grid;
			bp++;
			bp->vec[0] += -grid;
			bp++; bp++;
			bp->vec[0] += grid;
			bp++;
			bp->vec[0] += 2.0f * grid;

			bp = nu->bp;
			for (a = 0; a < 5; a++, bp++) mul_m4_v3(mat, bp->vec);

			if (cutype == CU_NURBS) {
				nu->knotsu = NULL; /* nurbs_knot_calc_u allocates */
				BKE_nurb_knot_calc_u(nu);
			}

			break;
		case CU_PRIM_CIRCLE: /* circle */
			nu->resolu = cu->resolu;

			if (cutype == CU_BEZIER) {
				if (!force_3d) nu->flag |= CU_2D;
				nu->pntsu = 4;
				nu->bezt = (BezTriple *)MEM_callocN(sizeof(BezTriple) * 4, "addNurbprim1");
				nu->flagu = CU_NURB_CYCLIC;
				bezt = nu->bezt;

				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][0] += -grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				bezt++;
				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][1] += grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				bezt++;
				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][0] += grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				bezt++;
				bezt->h1 = bezt->h2 = HD_AUTO;
				bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
				bezt->vec[1][1] += -grid;
				for (a = 0; a < 3; a++) mul_m4_v3(mat, bezt->vec[a]);
				bezt->radius = bezt->weight = 1.0;

				BKE_nurb_handles_calc(nu);
			}
			else if (cutype == CU_NURBS) { /* nurb */
				nu->pntsu = 8;
				nu->pntsv = 1;
				nu->orderu = 4;
				nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * 8, "addNurbprim6");
				nu->flagu = CU_NURB_CYCLIC;
				bp = nu->bp;

				for (a = 0; a < 8; a++) {
					bp->f1 = SELECT;
					if (xzproj == 0) {
						bp->vec[0] += nurbcircle[a][0] * grid;
						bp->vec[1] += nurbcircle[a][1] * grid;
					}
					else {
						bp->vec[0] += 0.25f * nurbcircle[a][0] * grid - 0.75f * grid;
						bp->vec[2] += 0.25f * nurbcircle[a][1] * grid;
					}
					if (a & 1) bp->vec[3] = 0.25 * M_SQRT2;
					else bp->vec[3] = 1.0;
					mul_m4_v3(mat, bp->vec);
					bp->radius = bp->weight = 1.0;

					bp++;
				}

				BKE_nurb_knot_calc_u(nu);
			}
			break;
		case CU_PRIM_PATCH: /* 4x4 patch */
			if (cutype == CU_NURBS) { /* nurb */

				nu->pntsu = 4;
				nu->pntsv = 4;
				nu->orderu = 4;
				nu->orderv = 4;
				nu->flag = CU_SMOOTH;
				nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * (4 * 4), "addNurbprim6");
				nu->flagu = 0;
				nu->flagv = 0;
				bp = nu->bp;

				for (a = 0; a < 4; a++) {
					for (b = 0; b < 4; b++) {
						bp->f1 = SELECT;
						fac = (float)a - 1.5f;
						bp->vec[0] += fac * grid;
						fac = (float)b - 1.5f;
						bp->vec[1] += fac * grid;
						if ((a == 1 || a == 2) && (b == 1 || b == 2)) {
							bp->vec[2] += grid;
						}
						mul_m4_v3(mat, bp->vec);
						bp->vec[3] = 1.0;
						bp++;
					}
				}

				BKE_nurb_knot_calc_u(nu);
				BKE_nurb_knot_calc_v(nu);
			}
			break;
		case CU_PRIM_TUBE: /* Cylinder */
			if (cutype == CU_NURBS) {
				nu = add_nurbs_primitive(C, obedit, mat, CU_NURBS | CU_PRIM_CIRCLE, 0); /* circle */
				nu->resolu = cu->resolu;
				nu->flag = CU_SMOOTH;
				BLI_addtail(editnurb, nu); /* temporal for extrude and translate */
				vec[0] = vec[1] = 0.0;
				vec[2] = -grid;

				mul_mat3_m4_v3(mat, vec);

				ed_editnurb_translate_flag(editnurb, SELECT, vec);
				ed_editnurb_extrude_flag(cu->editnurb, SELECT);
				mul_v3_fl(vec, -2.0f);
				ed_editnurb_translate_flag(editnurb, SELECT, vec);

				BLI_remlink(editnurb, nu);

				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a-- > 0) {
					bp->f1 |= SELECT;
					bp++;
				}
			}
			break;
		case CU_PRIM_SPHERE: /* sphere */
			if (cutype == CU_NURBS) {
				float tmp_cent[3] = {0.f, 0.f, 0.f};
				float tmp_vec[3] = {0.f, 0.f, 1.f};

				nu->pntsu = 5;
				nu->pntsv = 1;
				nu->orderu = 3;
				nu->resolu = cu->resolu;
				nu->resolv = cu->resolv;
				nu->flag = CU_SMOOTH;
				nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * 5, "addNurbprim6");
				nu->flagu = 0;
				bp = nu->bp;

				for (a = 0; a < 5; a++) {
					bp->f1 = SELECT;
					bp->vec[0] += nurbcircle[a][0] * grid;
					bp->vec[2] += nurbcircle[a][1] * grid;
					if (a & 1) bp->vec[3] = 0.5 * M_SQRT2;
					else bp->vec[3] = 1.0;
					mul_m4_v3(mat, bp->vec);
					bp++;
				}
				nu->flagu = CU_NURB_BEZIER;
				BKE_nurb_knot_calc_u(nu);

				BLI_addtail(editnurb, nu); /* temporal for spin */

				if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0)
					ed_editnurb_spin(umat, obedit, tmp_vec, tmp_cent);
				else if ((U.flag & USER_ADD_VIEWALIGNED))
					ed_editnurb_spin(viewmat, obedit, zvec, mat[3]);
				else
					ed_editnurb_spin(umat, obedit, tmp_vec, mat[3]);

				BKE_nurb_knot_calc_v(nu);

				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a-- > 0) {
					bp->f1 |= SELECT;
					bp++;
				}
				BLI_remlink(editnurb, nu);
			}
			break;
		case CU_PRIM_DONUT: /* torus */
			if (cutype == CU_NURBS) {
				float tmp_cent[3] = {0.f, 0.f, 0.f};
				float tmp_vec[3] = {0.f, 0.f, 1.f};

				xzproj = 1;
				nu = add_nurbs_primitive(C, obedit, mat, CU_NURBS | CU_PRIM_CIRCLE, 0); /* circle */
				xzproj = 0;
				nu->resolu = cu->resolu;
				nu->resolv = cu->resolv;
				nu->flag = CU_SMOOTH;
				BLI_addtail(editnurb, nu); /* temporal for spin */

				/* same as above */
				if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0)
					ed_editnurb_spin(umat, obedit, tmp_vec, tmp_cent);
				else if ((U.flag & USER_ADD_VIEWALIGNED))
					ed_editnurb_spin(viewmat, obedit, zvec, mat[3]);
				else
					ed_editnurb_spin(umat, obedit, tmp_vec, mat[3]);


				BLI_remlink(editnurb, nu);

				a = nu->pntsu * nu->pntsv;
				bp = nu->bp;
				while (a-- > 0) {
					bp->f1 |= SELECT;
					bp++;
				}

			}
			break;

		default: /* should never happen */
			BLI_assert(!"invalid nurbs type");
			return NULL;
	}

	BLI_assert(nu != NULL);

	if (nu) { /* should always be set */
		nu->flag |= CU_SMOOTH;
		cu->actnu = BLI_countlist(editnurb);
		cu->actvert = CU_ACT_NONE;

		BKE_nurb_test2D(nu);
	}

	return nu;
}

static int curvesurf_prim_add(bContext *C, wmOperator *op, int type, int isSurf)
{
	Object *obedit = CTX_data_edit_object(C);
	ListBase *editnurb;
	Nurb *nu;
	bool newob = false;
	bool enter_editmode;
	unsigned int layer;
	float dia;
	float loc[3], rot[3];
	float mat[4][4];

	WM_operator_view3d_unit_defaults(C, op);

	if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL))
		return OPERATOR_CANCELLED;

	if (!isSurf) { /* adding curve */
		if (obedit == NULL || obedit->type != OB_CURVE) {
			Curve *cu;

			obedit = ED_object_add_type(C, OB_CURVE, loc, rot, true, layer);
			newob = true;

			cu = (Curve *)obedit->data;
			cu->flag |= CU_DEFORM_FILL;

			if (type & CU_PRIM_PATH)
				cu->flag |= CU_PATH | CU_3D;
		}
		else {
			DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
		}
	}
	else { /* adding surface */
		if (obedit == NULL || obedit->type != OB_SURF) {
			obedit = ED_object_add_type(C, OB_SURF, loc, rot, true, layer);
			newob = true;
		}
		else {
			DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
		}
	}

	/* rename here, the undo stack checks name for valid undo pushes */
	if (newob) {
		if (obedit->type == OB_CURVE) {
			rename_id((ID *)obedit, get_curve_defname(type));
			rename_id((ID *)obedit->data, get_curve_defname(type));
		}
		else {
			rename_id((ID *)obedit, get_surf_defname(type));
			rename_id((ID *)obedit->data, get_surf_defname(type));
		}
	}

	/* ED_object_add_type doesnt do an undo, is needed for redo operator on primitive */
	if (newob && enter_editmode)
		ED_undo_push(C, "Enter Editmode");

	ED_object_new_primitive_matrix(C, obedit, loc, rot, mat, false);
	dia = RNA_float_get(op->ptr, "radius");
	mat[0][0] *= dia;
	mat[1][1] *= dia;
	mat[2][2] *= dia;

	nu = add_nurbs_primitive(C, obedit, mat, type, newob);
	editnurb = object_editcurve_get(obedit);
	BLI_addtail(editnurb, nu);

	/* userdef */
	if (newob && !enter_editmode) {
		ED_object_editmode_exit(C, EM_FREEDATA);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;
}

static int curve_prim_add(bContext *C, wmOperator *op, int type)
{
	return curvesurf_prim_add(C, op, type, 0);
}

static int surf_prim_add(bContext *C, wmOperator *op, int type)
{
	return curvesurf_prim_add(C, op, type, 1);
}

/* ******************** Curves ******************* */

static int add_primitive_bezier_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_BEZIER | CU_PRIM_CURVE);
}

void CURVE_OT_primitive_bezier_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bezier";
	ot->description = "Construct a Bezier Curve";
	ot->idname = "CURVE_OT_primitive_bezier_curve_add";

	/* api callbacks */
	ot->exec = add_primitive_bezier_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_bezier_circle_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_BEZIER | CU_PRIM_CIRCLE);
}

void CURVE_OT_primitive_bezier_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bezier Circle";
	ot->description = "Construct a Bezier Circle";
	ot->idname = "CURVE_OT_primitive_bezier_circle_add";

	/* api callbacks */
	ot->exec = add_primitive_bezier_circle_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_curve_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_NURBS | CU_PRIM_CURVE);
}

void CURVE_OT_primitive_nurbs_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Nurbs Curve";
	ot->description = "Construct a Nurbs Curve";
	ot->idname = "CURVE_OT_primitive_nurbs_curve_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_curve_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_circle_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_NURBS | CU_PRIM_CIRCLE);
}

void CURVE_OT_primitive_nurbs_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Nurbs Circle";
	ot->description = "Construct a Nurbs Circle";
	ot->idname = "CURVE_OT_primitive_nurbs_circle_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_circle_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_curve_path_exec(bContext *C, wmOperator *op)
{
	return curve_prim_add(C, op, CU_NURBS | CU_PRIM_PATH);
}

void CURVE_OT_primitive_nurbs_path_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Path";
	ot->description = "Construct a Path";
	ot->idname = "CURVE_OT_primitive_nurbs_path_add";

	/* api callbacks */
	ot->exec = add_primitive_curve_path_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

/* **************** NURBS surfaces ********************** */
static int add_primitive_nurbs_surface_curve_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_CURVE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_curve_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Curve";
	ot->description = "Construct a Nurbs surface Curve";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_curve_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_surface_curve_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_surface_circle_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_CIRCLE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_circle_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Circle";
	ot->description = "Construct a Nurbs surface Circle";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_circle_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_surface_circle_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_surface_surface_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_PATCH | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_surface_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Patch";
	ot->description = "Construct a Nurbs surface Patch";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_surface_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_surface_surface_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_surface_cylinder_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_TUBE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_cylinder_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Cylinder";
	ot->description = "Construct a Nurbs surface Cylinder";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_cylinder_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_surface_cylinder_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_surface_sphere_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_SPHERE | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_sphere_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Sphere";
	ot->description = "Construct a Nurbs surface Sphere";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_sphere_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_surface_sphere_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}

static int add_primitive_nurbs_surface_torus_exec(bContext *C, wmOperator *op)
{
	return surf_prim_add(C, op, CU_PRIM_DONUT | CU_NURBS);
}

void SURFACE_OT_primitive_nurbs_surface_torus_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Surface Torus";
	ot->description = "Construct a Nurbs surface Torus";
	ot->idname = "SURFACE_OT_primitive_nurbs_surface_torus_add";

	/* api callbacks */
	ot->exec = add_primitive_nurbs_surface_torus_exec;
	ot->poll = ED_operator_scene_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ED_object_add_unit_props(ot);
	ED_object_add_generic_props(ot, true);
}
