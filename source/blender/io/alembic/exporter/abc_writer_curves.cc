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
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 */

/** \file
 * \ingroup balembic
 */

#include "abc_writer_curves.h"
#include "intern/abc_axis_conversion.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::OInt16Property;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

namespace blender {
namespace io {
namespace alembic {

const std::string ABC_CURVE_RESOLUTION_U_PROPNAME("blender:resolution");

ABCCurveWriter::ABCCurveWriter(const ABCWriterConstructorArgs &args) : ABCAbstractWriter(args)
{
}

void ABCCurveWriter::create_alembic_objects(const HierarchyContext *context)
{
  CLOG_INFO(&LOG, 2, "exporting %s", args_.abc_path.c_str());
  abc_curve_ = OCurves(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_curve_schema_ = abc_curve_.getSchema();

  Curve *cu = static_cast<Curve *>(context->object->data);
  OCompoundProperty user_props = abc_curve_schema_.getUserProperties();
  OInt16Property user_prop_resolu(user_props, ABC_CURVE_RESOLUTION_U_PROPNAME);
  user_prop_resolu.set(cu->resolu);
}

const Alembic::Abc::OObject ABCCurveWriter::get_alembic_object() const
{
  return abc_curve_;
}

void ABCCurveWriter::do_write(HierarchyContext &context)
{
  Curve *curve = static_cast<Curve *>(context.object->data);

  std::vector<Imath::V3f> verts;
  std::vector<int32_t> vert_counts;
  std::vector<float> widths;
  std::vector<float> weights;
  std::vector<float> knots;
  std::vector<uint8_t> orders;
  Imath::V3f temp_vert;

  Alembic::AbcGeom::BasisType curve_basis = Alembic::AbcGeom::kNoBasis;
  Alembic::AbcGeom::CurveType curve_type = Alembic::AbcGeom::kVariableOrder;
  Alembic::AbcGeom::CurvePeriodicity periodicity = Alembic::AbcGeom::kNonPeriodic;

  Nurb *nurbs = static_cast<Nurb *>(curve->nurb.first);
  for (; nurbs; nurbs = nurbs->next) {
    if (nurbs->bp) {
      curve_basis = Alembic::AbcGeom::kNoBasis;
      curve_type = Alembic::AbcGeom::kVariableOrder;

      const int totpoint = nurbs->pntsu * nurbs->pntsv;

      const BPoint *point = nurbs->bp;

      for (int i = 0; i < totpoint; i++, point++) {
        copy_yup_from_zup(temp_vert.getValue(), point->vec);
        verts.push_back(temp_vert);
        weights.push_back(point->vec[3]);
        widths.push_back(point->radius);
      }
    }
    else if (nurbs->bezt) {
      curve_basis = Alembic::AbcGeom::kBezierBasis;
      curve_type = Alembic::AbcGeom::kCubic;

      const int totpoint = nurbs->pntsu;

      const BezTriple *bezier = nurbs->bezt;

      /* TODO(kevin): store info about handles, Alembic doesn't have this. */
      for (int i = 0; i < totpoint; i++, bezier++) {
        copy_yup_from_zup(temp_vert.getValue(), bezier->vec[1]);
        verts.push_back(temp_vert);
        widths.push_back(bezier->radius);
      }
    }

    if ((nurbs->flagu & CU_NURB_ENDPOINT) != 0) {
      periodicity = Alembic::AbcGeom::kNonPeriodic;
    }
    else if ((nurbs->flagu & CU_NURB_CYCLIC) != 0) {
      periodicity = Alembic::AbcGeom::kPeriodic;

      /* Duplicate the start points to indicate that the curve is actually
       * cyclic since other software need those.
       */

      for (int i = 0; i < nurbs->orderu; i++) {
        verts.push_back(verts[i]);
      }
    }

    if (nurbs->knotsu != NULL) {
      const size_t num_knots = KNOTSU(nurbs);

      /* Add an extra knot at the beginning and end of the array since most apps
       * require/expect them. */
      knots.resize(num_knots + 2);

      for (int i = 0; i < num_knots; i++) {
        knots[i + 1] = nurbs->knotsu[i];
      }

      if ((nurbs->flagu & CU_NURB_CYCLIC) != 0) {
        knots[0] = nurbs->knotsu[0];
        knots[num_knots - 1] = nurbs->knotsu[num_knots - 1];
      }
      else {
        knots[0] = (2.0f * nurbs->knotsu[0] - nurbs->knotsu[1]);
        knots[num_knots - 1] = (2.0f * nurbs->knotsu[num_knots - 1] -
                                nurbs->knotsu[num_knots - 2]);
      }
    }

    orders.push_back(nurbs->orderu);
    vert_counts.push_back(verts.size());
  }

  Alembic::AbcGeom::OFloatGeomParam::Sample width_sample;
  width_sample.setVals(widths);

  OCurvesSchema::Sample sample(verts,
                               vert_counts,
                               curve_type,
                               periodicity,
                               width_sample,
                               OV2fGeomParam::Sample(), /* UVs */
                               ON3fGeomParam::Sample(), /* normals */
                               curve_basis,
                               weights,
                               orders,
                               knots);

  update_bounding_box(context.object);
  sample.setSelfBounds(bounding_box_);
  abc_curve_schema_.set(sample);
}

ABCCurveMeshWriter::ABCCurveMeshWriter(const ABCWriterConstructorArgs &args)
    : ABCGenericMeshWriter(args)
{
}

Mesh *ABCCurveMeshWriter::get_export_mesh(Object *object_eval, bool &r_needsfree)
{
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(object_eval);
  if (mesh_eval != NULL) {
    /* Mesh_eval only exists when generative modifiers are in use. */
    r_needsfree = false;
    return mesh_eval;
  }

  r_needsfree = true;
  return BKE_mesh_new_nomain_from_curve(object_eval);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
