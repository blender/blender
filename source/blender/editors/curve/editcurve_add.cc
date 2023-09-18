/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurve
 */

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_layer.h"

#include "DEG_depsgraph.h"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curve.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"

#include "curve_intern.h"

static const float nurbcircle[8][2] = {
    {0.0, -1.0},
    {-1.0, -1.0},
    {-1.0, 0.0},
    {-1.0, 1.0},
    {0.0, 1.0},
    {1.0, 1.0},
    {1.0, 0.0},
    {1.0, -1.0},
};

/************ add primitive, used by object/ module ****************/

static const char *get_curve_defname(int type)
{
  int stype = type & CU_PRIMITIVE;

  if ((type & CU_TYPE) == CU_BEZIER) {
    switch (stype) {
      case CU_PRIM_CURVE:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "BezierCurve");
      case CU_PRIM_CIRCLE:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "BezierCircle");
      case CU_PRIM_PATH:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "CurvePath");
      default:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "Curve");
    }
  }
  else {
    switch (stype) {
      case CU_PRIM_CURVE:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "NurbsCurve");
      case CU_PRIM_CIRCLE:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "NurbsCircle");
      case CU_PRIM_PATH:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "NurbsPath");
      default:
        return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "Curve");
    }
  }
}

static const char *get_surf_defname(int type)
{
  int stype = type & CU_PRIMITIVE;

  switch (stype) {
    case CU_PRIM_CURVE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "SurfCurve");
    case CU_PRIM_CIRCLE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "SurfCircle");
    case CU_PRIM_PATCH:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "SurfPatch");
    case CU_PRIM_TUBE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "SurfCylinder");
    case CU_PRIM_SPHERE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "SurfSphere");
    case CU_PRIM_DONUT:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "SurfTorus");
    default:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_CURVE_LEGACY, "Surface");
  }
}

Nurb *ED_curve_add_nurbs_primitive(
    bContext *C, Object *obedit, float mat[4][4], int type, int newob)
{
  static int xzproj = 0; /* this function calls itself... */
  ListBase *editnurb = object_editcurve_get(obedit);
  RegionView3D *rv3d = ED_view3d_context_rv3d(C);
  Nurb *nu = nullptr;
  BezTriple *bezt;
  BPoint *bp;
  Curve *cu = (Curve *)obedit->data;
  float vec[3], zvec[3] = {0.0f, 0.0f, 1.0f};
  float umat[4][4], viewmat[4][4];
  float fac;
  int a, b;
  const float grid = 1.0f;
  const int cutype = (type & CU_TYPE); /* poly, bezier, nurbs, etc */
  const int stype = (type & CU_PRIMITIVE);

  unit_m4(umat);
  unit_m4(viewmat);

  if (rv3d) {
    copy_m4_m4(viewmat, rv3d->viewmat);
    copy_v3_v3(zvec, rv3d->viewinv[2]);
  }

  BKE_nurbList_flag_set(editnurb, SELECT, false);

  /* these types call this function to return a Nurb */
  if (!ELEM(stype, CU_PRIM_TUBE, CU_PRIM_DONUT)) {
    nu = (Nurb *)MEM_callocN(sizeof(Nurb), "addNurbprim");
    nu->type = cutype;
    nu->resolu = cu->resolu;
    nu->resolv = cu->resolv;
  }

  switch (stype) {
    case CU_PRIM_CURVE: /* curve */
      nu->resolu = cu->resolu;
      if (cutype == CU_BEZIER) {
        nu->pntsu = 2;
        nu->bezt = (BezTriple *)MEM_callocN(sizeof(BezTriple) * nu->pntsu, "addNurbprim1");
        bezt = nu->bezt;
        bezt->h1 = bezt->h2 = HD_ALIGN;
        bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
        bezt->radius = 1.0;

        bezt->vec[1][0] += -grid;
        bezt->vec[0][0] += -1.5f * grid;
        bezt->vec[0][1] += -0.5f * grid;
        bezt->vec[2][0] += -0.5f * grid;
        bezt->vec[2][1] += 0.5f * grid;
        for (a = 0; a < 3; a++) {
          mul_m4_v3(mat, bezt->vec[a]);
        }

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
        for (a = 0; a < 3; a++) {
          mul_m4_v3(mat, bezt->vec[a]);
        }

        BKE_nurb_handles_calc(nu);
      }
      else {

        nu->pntsu = 4;
        nu->pntsv = 1;
        nu->orderu = 4;
        nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * nu->pntsu, "addNurbprim3");

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
        bp->vec[1] += grid;
        bp++;
        bp->vec[0] += grid;
        bp->vec[1] += grid;
        bp++;
        bp->vec[0] += 1.5f * grid;

        bp = nu->bp;
        for (a = 0; a < 4; a++, bp++) {
          mul_m4_v3(mat, bp->vec);
        }

        if (cutype == CU_NURBS) {
          nu->knotsu = nullptr; /* nurbs_knot_calc_u allocates */
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
      nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * nu->pntsu, "addNurbprim3");

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
      bp++;
      bp++;
      bp->vec[0] += grid;
      bp++;
      bp->vec[0] += 2.0f * grid;

      bp = nu->bp;
      for (a = 0; a < 5; a++, bp++) {
        mul_m4_v3(mat, bp->vec);
      }

      if (cutype == CU_NURBS) {
        nu->knotsu = nullptr; /* nurbs_knot_calc_u allocates */
        BKE_nurb_knot_calc_u(nu);
      }

      break;
    case CU_PRIM_CIRCLE: /* circle */
      nu->resolu = cu->resolu;

      if (cutype == CU_BEZIER) {
        nu->pntsu = 4;
        nu->bezt = (BezTriple *)MEM_callocN(sizeof(BezTriple) * nu->pntsu, "addNurbprim1");
        nu->flagu = CU_NURB_CYCLIC;
        bezt = nu->bezt;

        bezt->h1 = bezt->h2 = HD_AUTO;
        bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
        bezt->vec[1][0] += -grid;
        for (a = 0; a < 3; a++) {
          mul_m4_v3(mat, bezt->vec[a]);
        }
        bezt->radius = bezt->weight = 1.0;

        bezt++;
        bezt->h1 = bezt->h2 = HD_AUTO;
        bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
        bezt->vec[1][1] += grid;
        for (a = 0; a < 3; a++) {
          mul_m4_v3(mat, bezt->vec[a]);
        }
        bezt->radius = bezt->weight = 1.0;

        bezt++;
        bezt->h1 = bezt->h2 = HD_AUTO;
        bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
        bezt->vec[1][0] += grid;
        for (a = 0; a < 3; a++) {
          mul_m4_v3(mat, bezt->vec[a]);
        }
        bezt->radius = bezt->weight = 1.0;

        bezt++;
        bezt->h1 = bezt->h2 = HD_AUTO;
        bezt->f1 = bezt->f2 = bezt->f3 = SELECT;
        bezt->vec[1][1] += -grid;
        for (a = 0; a < 3; a++) {
          mul_m4_v3(mat, bezt->vec[a]);
        }
        bezt->radius = bezt->weight = 1.0;

        BKE_nurb_handles_calc(nu);
      }
      else if (cutype == CU_NURBS) { /* nurb */
        nu->pntsu = 8;
        nu->pntsv = 1;
        nu->orderu = 3;
        nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * nu->pntsu, "addNurbprim6");
        nu->flagu = CU_NURB_CYCLIC | CU_NURB_BEZIER | CU_NURB_ENDPOINT;
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
          if (a & 1) {
            bp->vec[3] = 0.5 * M_SQRT2;
          }
          else {
            bp->vec[3] = 1.0;
          }
          mul_m4_v3(mat, bp->vec);
          bp->radius = bp->weight = 1.0;

          bp++;
        }

        BKE_nurb_knot_calc_u(nu);
      }
      break;
    case CU_PRIM_PATCH:         /* 4x4 patch */
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
            fac = float(a) - 1.5f;
            bp->vec[0] += fac * grid;
            fac = float(b) - 1.5f;
            bp->vec[1] += fac * grid;
            if (ELEM(a, 1, 2) && ELEM(b, 1, 2)) {
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
        nu = ED_curve_add_nurbs_primitive(C, obedit, mat, CU_NURBS | CU_PRIM_CIRCLE, 0);
        nu->resolu = cu->resolu;
        nu->flag = CU_SMOOTH;
        BLI_addtail(editnurb, nu); /* temporal for extrude and translate */
        vec[0] = vec[1] = 0.0;
        vec[2] = -grid;

        mul_mat3_m4_v3(mat, vec);

        ed_editnurb_translate_flag(editnurb, SELECT, vec, CU_IS_2D(cu));
        ed_editnurb_extrude_flag(cu->editnurb, SELECT);
        mul_v3_fl(vec, -2.0f);
        ed_editnurb_translate_flag(editnurb, SELECT, vec, CU_IS_2D(cu));

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
        const float tmp_cent[3] = {0.0f, 0.0f, 0.0f};
        const float tmp_vec[3] = {0.0f, 0.0f, 1.0f};

        nu->pntsu = 5;
        nu->pntsv = 1;
        nu->orderu = 3;
        nu->resolu = cu->resolu;
        nu->resolv = cu->resolv;
        nu->flag = CU_SMOOTH;
        nu->bp = (BPoint *)MEM_callocN(sizeof(BPoint) * nu->pntsu, "addNurbprim6");
        nu->flagu = 0;
        bp = nu->bp;

        for (a = 0; a < 5; a++) {
          bp->f1 = SELECT;
          bp->vec[0] += nurbcircle[a][0] * grid;
          bp->vec[2] += nurbcircle[a][1] * grid;
          if (a & 1) {
            bp->vec[3] = 0.5 * M_SQRT2;
          }
          else {
            bp->vec[3] = 1.0;
          }
          mul_m4_v3(mat, bp->vec);
          bp++;
        }
        nu->flagu = CU_NURB_BEZIER | CU_NURB_ENDPOINT;
        BKE_nurb_knot_calc_u(nu);

        BLI_addtail(editnurb, nu); /* temporal for spin */

        if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0) {
          ed_editnurb_spin(umat, nullptr, obedit, tmp_vec, tmp_cent);
        }
        else if (U.flag & USER_ADD_VIEWALIGNED) {
          ed_editnurb_spin(viewmat, nullptr, obedit, zvec, mat[3]);
        }
        else {
          ed_editnurb_spin(umat, nullptr, obedit, tmp_vec, mat[3]);
        }

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
        const float tmp_cent[3] = {0.0f, 0.0f, 0.0f};
        const float tmp_vec[3] = {0.0f, 0.0f, 1.0f};

        xzproj = 1;
        nu = ED_curve_add_nurbs_primitive(C, obedit, mat, CU_NURBS | CU_PRIM_CIRCLE, 0);
        xzproj = 0;
        nu->resolu = cu->resolu;
        nu->resolv = cu->resolv;
        nu->flag = CU_SMOOTH;
        BLI_addtail(editnurb, nu); /* temporal for spin */

        /* same as above */
        if (newob && (U.flag & USER_ADD_VIEWALIGNED) == 0) {
          ed_editnurb_spin(umat, nullptr, obedit, tmp_vec, tmp_cent);
        }
        else if (U.flag & USER_ADD_VIEWALIGNED) {
          ed_editnurb_spin(viewmat, nullptr, obedit, zvec, mat[3]);
        }
        else {
          ed_editnurb_spin(umat, nullptr, obedit, tmp_vec, mat[3]);
        }

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
      BLI_assert_msg(0, "invalid nurbs type");
      return nullptr;
  }

  BLI_assert(nu != nullptr);

  if (nu) { /* should always be set */
    nu->flag |= CU_SMOOTH;
    cu->actnu = BLI_listbase_count(editnurb);
    cu->actvert = CU_ACT_NONE;

    if (CU_IS_2D(cu)) {
      BKE_nurb_project_2d(nu);
    }
  }

  return nu;
}

static int curvesurf_prim_add(bContext *C, wmOperator *op, int type, int isSurf)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  ListBase *editnurb;
  Nurb *nu;
  bool newob = false;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];
  float mat[4][4];

  WM_operator_view3d_unit_defaults(C, op);

  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr))
  {
    return OPERATOR_CANCELLED;
  }

  if (!isSurf) { /* adding curve */
    if (obedit == nullptr || obedit->type != OB_CURVES_LEGACY) {
      const char *name = get_curve_defname(type);
      Curve *cu;

      obedit = ED_object_add_type(C, OB_CURVES_LEGACY, name, loc, rot, true, local_view_bits);
      newob = true;

      cu = (Curve *)obedit->data;

      if (type & CU_PRIM_PATH) {
        cu->flag |= CU_PATH | CU_3D;
      }
    }
    else {
      DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
    }
  }
  else { /* adding surface */
    if (obedit == nullptr || obedit->type != OB_SURF) {
      const char *name = get_surf_defname(type);
      obedit = ED_object_add_type(C, OB_SURF, name, loc, rot, true, local_view_bits);
      newob = true;
    }
    else {
      DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
    }
  }

  float radius = RNA_float_get(op->ptr, "radius");
  float scale[3];
  copy_v3_fl(scale, radius);
  ED_object_new_primitive_matrix(C, obedit, loc, rot, scale, mat);

  nu = ED_curve_add_nurbs_primitive(C, obedit, mat, type, newob);
  editnurb = object_editcurve_get(obedit);
  BLI_addtail(editnurb, nu);

  /* userdef */
  if (newob && !enter_editmode) {
    ED_object_editmode_exit_ex(bmain, scene, obedit, EM_FREEDATA);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
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

  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, true);
}
