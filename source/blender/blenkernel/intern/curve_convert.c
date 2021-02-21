/*
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
 */

/** \file
 * \ingroup bke
 */

#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"

#include "BLI_utildefines.h"

#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_font.h"
#include "BKE_lib_id.h"
#include "BKE_modifier.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

static Curve *curve_from_font_object(Object *object, Depsgraph *depsgraph)
{
  Curve *curve = (Curve *)object->data;
  Curve *new_curve = (Curve *)BKE_id_copy_ex(NULL, &curve->id, NULL, LIB_ID_COPY_LOCALIZE);

  Object *evaluated_object = DEG_get_evaluated_object(depsgraph, object);
  BKE_vfont_to_curve_nubase(evaluated_object, FO_EDIT, &new_curve->nurb);

  new_curve->type = OB_CURVE;

  new_curve->flag &= ~CU_3D;
  BKE_curve_curve_dimension_update(new_curve);

  return new_curve;
}

static Curve *curve_from_curve_object(Object *object, Depsgraph *depsgraph, bool apply_modifiers)
{
  Object *evaluated_object = DEG_get_evaluated_object(depsgraph, object);
  Curve *curve = (Curve *)evaluated_object->data;
  Curve *new_curve = (Curve *)BKE_id_copy_ex(NULL, &curve->id, NULL, LIB_ID_COPY_LOCALIZE);

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
  if (!ELEM(object->type, OB_FONT, OB_CURVE)) {
    return NULL;
  }

  if (object->type == OB_FONT) {
    return curve_from_font_object(object, depsgraph);
  }

  return curve_from_curve_object(object, depsgraph, apply_modifiers);
}
