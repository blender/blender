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
#include "abc_writer_transform.h"
#include "intern/abc_axis_conversion.h"
#include "intern/abc_reader_curves.h"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::OInt16Property;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

namespace blender {
namespace io {
namespace alembic {

AbcCurveWriter::AbcCurveWriter(Object *ob,
                               AbcTransformWriter *parent,
                               uint32_t time_sampling,
                               ExportSettings &settings)
    : AbcObjectWriter(ob, time_sampling, settings, parent)
{
  OCurves curves(parent->alembicXform(), m_name, m_time_sampling);
  m_schema = curves.getSchema();

  Curve *cu = static_cast<Curve *>(m_object->data);
  OCompoundProperty user_props = m_schema.getUserProperties();
  OInt16Property user_prop_resolu(user_props, ABC_CURVE_RESOLUTION_U_PROPNAME);
  user_prop_resolu.set(cu->resolu);
}

void AbcCurveWriter::do_write()
{
  Curve *curve = static_cast<Curve *>(m_object->data);

  std::vector<Imath::V3f> verts;
  std::vector<int32_t> vert_counts;
  std::vector<float> widths;
  std::vector<float> weights;
  std::vector<float> knots;
  std::vector<uint8_t> orders;
  Imath::V3f temp_vert;

  Alembic::AbcGeom::BasisType curve_basis;
  Alembic::AbcGeom::CurveType curve_type;
  Alembic::AbcGeom::CurvePeriodicity periodicity;

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

  m_sample = OCurvesSchema::Sample(verts,
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

  m_sample.setSelfBounds(bounds());
  m_schema.set(m_sample);
}

AbcCurveMeshWriter::AbcCurveMeshWriter(Object *ob,
                                       AbcTransformWriter *parent,
                                       uint32_t time_sampling,
                                       ExportSettings &settings)
    : AbcGenericMeshWriter(ob, parent, time_sampling, settings)
{
}

Mesh *AbcCurveMeshWriter::getEvaluatedMesh(Scene * /*scene_eval*/,
                                           Object *ob_eval,
                                           bool &r_needsfree)
{
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  if (mesh_eval != NULL) {
    /* Mesh_eval only exists when generative modifiers are in use. */
    r_needsfree = false;
    return mesh_eval;
  }

  r_needsfree = true;
  return BKE_mesh_new_nomain_from_curve(ob_eval);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender
