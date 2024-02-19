/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"

#include "BLI_utildefines.h"

#include "BKE_curve.hh"
#include "BKE_displist.h"
#include "BKE_lib_id.hh"
#include "BKE_modifier.hh"
#include "BKE_vfont.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

static Curve *curve_from_font_object(Object *object, Depsgraph *depsgraph)
{
  Curve *curve = (Curve *)object->data;
  Curve *new_curve = (Curve *)BKE_id_copy_ex(nullptr, &curve->id, nullptr, LIB_ID_COPY_LOCALIZE);

  Object *evaluated_object = DEG_get_evaluated_object(depsgraph, object);
  BKE_vfont_to_curve_nubase(evaluated_object, FO_EDIT, &new_curve->nurb);

  new_curve->type = OB_CURVES_LEGACY;

  new_curve->flag &= ~CU_3D;
  BKE_curve_dimension_update(new_curve);

  return new_curve;
}

static Curve *curve_from_curve_object(Object *object, Depsgraph *depsgraph, bool apply_modifiers)
{
  Object *evaluated_object = DEG_get_evaluated_object(depsgraph, object);
  Curve *curve = (Curve *)evaluated_object->data;
  Curve *new_curve = (Curve *)BKE_id_copy_ex(nullptr, &curve->id, nullptr, LIB_ID_COPY_LOCALIZE);

  if (apply_modifiers) {
    BKE_curve_calc_modifiers_pre(depsgraph,
                                 DEG_get_input_scene(depsgraph),
                                 evaluated_object,
                                 BKE_curve_nurbs_get(curve),
                                 &new_curve->nurb,
                                 DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  }

  return new_curve;
}

Curve *BKE_curve_new_from_object(Object *object, Depsgraph *depsgraph, bool apply_modifiers)
{
  if (!ELEM(object->type, OB_FONT, OB_CURVES_LEGACY)) {
    return nullptr;
  }

  if (object->type == OB_FONT) {
    return curve_from_font_object(object, depsgraph);
  }

  return curve_from_curve_object(object, depsgraph, apply_modifiers);
}
